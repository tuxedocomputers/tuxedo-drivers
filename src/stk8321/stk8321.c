/*!
 * Copyright (c) 2024 TUXEDO Computers GmbH <tux@tuxedocomputers.com>
 *
 * This file is part of tuxedo-drivers.
 *
 * tuxedo-drivers is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 *
 * This software is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this software.  If not, see <https://www.gnu.org/licenses/>.
 */
#define pr_fmt(fmt) KBUILD_MODNAME ": " fmt
#include <linux/i2c.h>
#include <linux/kernel.h>
#include <linux/module.h>
#include <linux/delay.h>
#include <linux/iio/buffer.h>
#include <linux/iio/iio.h>
#include <linux/iio/sysfs.h>

#define STK8321_DRIVER_NAME "stk8321"

#define STK8321_REG_CHIP_ID		0x00
#define STK8321_REG_XOUT1		0x02
#define STK8321_REG_XOUT2		0x03
#define STK8321_REG_YOUT1		0x04
#define STK8321_REG_YOUT2		0x05
#define STK8321_REG_ZOUT1		0x06
#define STK8321_REG_ZOUT2		0x07
#define STK8321_REG_RANGESEL		0x0f
#define STK8321_REG_BWSEL		0x10
#define STK8321_REG_POWMODE		0x11
#define STK8321_REG_DATASETUP		0x13
#define STK8321_REG_SWRST		0x14

#define STK8321_DATA_FILTER_MASK	0x80
#define STK8321_DATA_PROTECT_MASK	0x40

enum rangesel {
	STK8321_RANGESEL_2G = 0x03,
	STK8321_RANGESEL_4G = 0x05,
	STK8321_RANGESEL_8G = 0x08,
};

enum powmode {
	STK8321_POWMODE_NORMAL		= 0x00,
	STK8321_POWMODE_SUSPEND		= 0x80,
	STK8321_POWMODE_LOWPOWER	= 0x40,
};

enum bandwidth {
	STK8321_BW_HZ_7_81	= 0x08,
	STK8321_BW_HZ_15_63 	= 0x09,
	STK8321_BW_HZ_31_25 	= 0x0a,
	STK8321_BW_HZ_62_5 	= 0x0b,
	STK8321_BW_HZ_125 	= 0x0c,
	STK8321_BW_HZ_250 	= 0x0d,
	STK8321_BW_HZ_500 	= 0x0e,
	STK8321_BW_HZ_1000 	= 0x0f,
};

static const struct {
	int val;
	int val2;
	u8 reg_bits;
} stk8321_samp_freq_table[] = {
	{ 7, 810000, STK8321_BW_HZ_7_81 },
	{ 15, 630000, STK8321_BW_HZ_15_63 },
	{ 31, 250000, STK8321_BW_HZ_31_25 },
	{ 62, 500000, STK8321_BW_HZ_62_5 },
	{ 125, 0, STK8321_BW_HZ_125 },
	{ 250, 0, STK8321_BW_HZ_250 },
	{ 500, 0, STK8321_BW_HZ_500 },
	{ 1000, 0, STK8321_BW_HZ_1000 },
};

static IIO_CONST_ATTR_SAMP_FREQ_AVAIL(
	"7.810000 15.630000 31.250000 62.500000 125 250 500 1000");

static struct attribute *stk8321_accel_attributes[] = {
	&iio_const_attr_sampling_frequency_available.dev_attr.attr,
	NULL,
};

static const struct attribute_group stk8321_accel_attrs_group = {
	.attrs = stk8321_accel_attributes,
};

struct stk8321_data {
	struct i2c_client *client;
	struct i2c_client *second_client;
	struct iio_mount_matrix orientation;
	struct mutex lock;
	int samp_freq;
};

static const struct iio_mount_matrix iio_mount_zeromatrix = {
	.rotation = {
		"0", "0", "0",
		"0", "0", "0",
		"0", "0", "0"
	}
};

static int stk8321_read_axis(struct i2c_client *client, u8 reg1, u8 reg2)
{
	int low_byte, high_byte, result;
	low_byte = i2c_smbus_read_byte_data(client, reg1);
	if (low_byte < 0)
		return low_byte;
	high_byte = i2c_smbus_read_byte_data(client, reg2);
	if (high_byte < 0)
		return high_byte;

	result = (high_byte << 4) | (low_byte >> 4);
	return result;
}

static int stk8321_read_x(struct i2c_client *client)
{
	return stk8321_read_axis(client, STK8321_REG_XOUT1, STK8321_REG_XOUT2);
}

static int stk8321_read_y(struct i2c_client *client)
{
	return stk8321_read_axis(client, STK8321_REG_YOUT1, STK8321_REG_YOUT2);
}

static int stk8321_read_z(struct i2c_client *client)
{
	return stk8321_read_axis(client, STK8321_REG_ZOUT1, STK8321_REG_ZOUT2);
}

static int stk8321_set_range(struct i2c_client *client, enum rangesel range)
{
	return i2c_smbus_write_byte_data(client, STK8321_REG_RANGESEL, range);
}

static int stk8321_set_power_mode(struct i2c_client *client, enum powmode mode)
{
	return i2c_smbus_write_byte_data(client, STK8321_REG_POWMODE, mode);
}

static int stk8321_set_bandwidth(struct i2c_client *client, enum bandwidth bw)
{
	return i2c_smbus_write_byte_data(client, STK8321_REG_BWSEL, bw);
}

static const struct iio_mount_matrix *
stk8321_accel_get_mount_matrix(const struct iio_dev *indio_dev,
			       const struct iio_chan_spec *chan)
{
	struct stk8321_data *data = iio_priv(indio_dev);
	return &data->orientation;
}

static const struct iio_chan_spec_ext_info stk8321_ext_info[] = {
	IIO_MOUNT_MATRIX(IIO_SHARED_BY_DIR, stk8321_accel_get_mount_matrix),
	{ }
};

#define STK8321_ACCEL_CHANNEL(index, axis) {				\
	.type = IIO_ACCEL,						\
	.address = index,						\
	.modified = 1,							\
	.channel2 = IIO_MOD_##axis,					\
	.info_mask_separate = BIT(IIO_CHAN_INFO_RAW),			\
	.info_mask_shared_by_type = BIT(IIO_CHAN_INFO_SAMP_FREQ),	\
	.scan_type = {							\
		.realbits = 12,						\
	},								\
	.ext_info = stk8321_ext_info,					\
}

static const struct iio_chan_spec stk8321_channels[] = {
	STK8321_ACCEL_CHANNEL(0, X),
	STK8321_ACCEL_CHANNEL(1, Y),
	STK8321_ACCEL_CHANNEL(2, Z),
};

static int stk8321_read_raw(struct iio_dev *indio_dev,
			    struct iio_chan_spec const *chan,
			    int *val, int *val2, long mask)
{
	struct stk8321_data *data = iio_priv(indio_dev);
	int ret;

	switch (mask) {
	case IIO_CHAN_INFO_RAW:
		switch (chan->address) {
		case 0:
			mutex_lock(&data->lock);
			ret = stk8321_read_x(data->client);
			mutex_unlock(&data->lock);
			break;
		case 1:
			mutex_lock(&data->lock);
			ret = stk8321_read_y(data->client);
			mutex_unlock(&data->lock);
			break;
		case 2:
			mutex_lock(&data->lock);
			ret = stk8321_read_z(data->client);
			mutex_unlock(&data->lock);
			break;
		default:
			return -EINVAL;
		}

		if (ret < 0)
			return ret;

		*val = sign_extend32(ret, chan->scan_type.realbits - 1);
		return IIO_VAL_INT;
	case IIO_CHAN_INFO_SAMP_FREQ:
		*val = stk8321_samp_freq_table[data->samp_freq].val;
		*val2 = stk8321_samp_freq_table[data->samp_freq].val2;
		return IIO_VAL_INT_PLUS_MICRO;
	default:
		return -EINVAL;
	}
}

static int stk8321_write_raw(struct iio_dev *indio_dev,
			    struct iio_chan_spec const *chan,
			    int val, int val2, long mask)
{
	struct stk8321_data *data = iio_priv(indio_dev);
	int i, ret, matching_index;

	switch (mask) {
	case IIO_CHAN_INFO_SAMP_FREQ:
		matching_index = -1;
		for (i = 0; i < ARRAY_SIZE(stk8321_samp_freq_table); ++i) {
			if (val == stk8321_samp_freq_table[i].val &&
			    val2 == stk8321_samp_freq_table[i].val2) {
				matching_index = i;
			}
		}
		if (matching_index < 0)
			return -EINVAL;

		mutex_lock(&data->lock);
		data->samp_freq = matching_index;
		stk8321_set_power_mode(data->client, STK8321_POWMODE_SUSPEND);
		ret = stk8321_set_bandwidth(
			data->client,
			stk8321_samp_freq_table[data->samp_freq].reg_bits);
		stk8321_set_power_mode(data->client, STK8321_POWMODE_NORMAL);
		mutex_unlock(&data->lock);
		return ret;
	default:
		return -EINVAL;
	}
}

static const struct iio_info stk8321_info = {
	.attrs		= &stk8321_accel_attrs_group,
	.read_raw	= stk8321_read_raw,
	.write_raw	= stk8321_write_raw,
};

#ifdef CONFIG_ACPI
static int stk8321_apply_acpi_orientation(struct device *dev,
					  char *method_name,
					  struct iio_mount_matrix *orientation)
{
	struct acpi_buffer buffer = { ACPI_ALLOCATE_BUFFER, NULL };
	union acpi_object *obj;
	acpi_status status;
	struct acpi_device *adev = ACPI_COMPANION(dev);
	int i, axis;
	char *value;

	if (!adev) {
		pr_debug("no acpi object\n");
		return -ENODEV;
	}

	if (!acpi_has_method(adev->handle, method_name)) {
		pr_debug("acpi object does not have method\n");
		return -ENODEV;
	}

	status = acpi_evaluate_object(adev->handle, method_name, NULL, &buffer);
	if (ACPI_FAILURE(status))
		return -EIO;

	obj = (union acpi_object *)buffer.pointer;
	if (!obj || obj->type != ACPI_TYPE_BUFFER || obj->buffer.length != 3) {
		pr_err("unexpected object\n");
		kfree(buffer.pointer);
		return -EIO;
	}

	*orientation = iio_mount_zeromatrix;

	// Format is: three values, one for each axis, where each value
	// map to selected axis (bits 0-1) and possibly flips the sign
	// (bit 4)
	for (i = 0; i < 3; ++i) {
		axis = (obj->buffer.pointer[i] & 0x3) * 3;
		if (obj->buffer.pointer[i] & (1 << 4))
			value = "-1";
		else
			value = "1";

		orientation->rotation[i + axis] = value;
	}

	kfree(buffer.pointer);

	return 0;
}
#else
static int stk8321_apply_acpi_orientation(struct device *dev,
					  const char *method_name,
					  struct iio_mount_matrix *orientation)
{
	return -ENODEV;
}
#endif

static int stk8321_apply_orientation(struct iio_dev *indio_dev)
{
	struct stk8321_data *data = iio_priv(indio_dev);
	struct i2c_client *client = data->client;
	struct device *dev = &client->dev;
	int ret = 1;
	struct iio_mount_matrix *ori = &data->orientation;

	if (strstr(dev_name(&client->dev), ":00")) {
		indio_dev->label = "accel-display";
		ret = stk8321_apply_acpi_orientation(dev, "GETO",
						     &data->orientation);
	} else if (strstr(dev_name(&client->dev), ":01")) {
		// Note: There is a GETR ACPI method that doesn't comply with
		// the Linux defined orientation for base accelerometers, this
		// mount matrix need to be configured in 60-sensors.hwdb
		indio_dev->label = "accel-base";
	}

	// If no ACPI info is available, default to reading mount matrix from kernel
	if (ret) {
		pr_debug("[%02x] no acpi method found\n", client->addr);
		ret = iio_read_mount_matrix(&client->dev, &data->orientation);
	}

	return ret;
}

static void stk8321_dual_probe(struct i2c_client *client)
{
	struct stk8321_data *data = iio_priv(i2c_get_clientdata(client));
	struct acpi_device *adev = ACPI_COMPANION(&client->dev);
	char dev_name[16];
	struct i2c_board_info board_info = {
		.type = "stkh8321",
		.dev_name = dev_name,
		.fwnode = client->dev.fwnode,
	};

	snprintf(dev_name, sizeof(dev_name), "%s:01", acpi_device_hid(adev));

	data->second_client = i2c_acpi_new_device(&client->dev, 1, &board_info);
}

static void stk8321_dual_remove(struct i2c_client *client)
{
	struct stk8321_data *data = iio_priv(i2c_get_clientdata(client));
	if (!IS_ERR_OR_NULL(data->second_client))
		i2c_unregister_device(data->second_client);
}

static int stk8321_probe(struct i2c_client *client)
{
	int ret;
	struct iio_dev *indio_dev;
	struct stk8321_data *data;
	const struct i2c_device_id *id = i2c_client_get_device_id(client);

	// Initialize iio device
	indio_dev = devm_iio_device_alloc(&client->dev, sizeof(*data));
	if (!indio_dev) {
		dev_err(&client->dev, "iio device allocation failed!\n");
		return -ENOMEM;
	}

	data = iio_priv(indio_dev);
	dev_set_drvdata(&client->dev, indio_dev);

	ret = i2c_smbus_write_byte_data(client, STK8321_REG_SWRST, 0x00);
	if (ret < 0) {
		pr_err("[%02x] failed to reset sensor\n", client->addr);
		return ret;
	}

	mutex_init(&data->lock);
	data->client = client;
	data->samp_freq = 0;

	// Setup sensor in suspend mode
	stk8321_set_power_mode(client, STK8321_POWMODE_SUSPEND);
	stk8321_set_range(client, STK8321_RANGESEL_2G);
	stk8321_set_bandwidth(
		client, stk8321_samp_freq_table[data->samp_freq].reg_bits);
	i2c_smbus_write_byte_data(client, STK8321_REG_DATASETUP, 0);
	stk8321_set_power_mode(client, STK8321_POWMODE_NORMAL);

	indio_dev->name = "stk8321";
	indio_dev->info = &stk8321_info;
	indio_dev->modes = INDIO_DIRECT_MODE;
	indio_dev->channels = stk8321_channels;
	indio_dev->num_channels = ARRAY_SIZE(stk8321_channels);

	ret = stk8321_apply_orientation(indio_dev);
	if (ret)
		return ret;

	if (!id && has_acpi_companion(&client->dev))
		stk8321_dual_probe(client);

	return devm_iio_device_register(&client->dev, indio_dev);
}

static void stk8321_remove(struct i2c_client *client)
{
	stk8321_dual_remove(client);
	stk8321_set_power_mode(client, STK8321_POWMODE_SUSPEND);
}

static int stk8321_suspend(struct device *dev)
{
	struct stk8321_data *data = iio_priv(dev_get_drvdata(dev));
	struct i2c_client *client = data->client;
	mutex_lock(&data->lock);
	stk8321_set_power_mode(client, STK8321_POWMODE_SUSPEND);
	mutex_unlock(&data->lock);
	return 0;
}

static int stk8321_resume(struct device *dev)
{
	struct stk8321_data *data = iio_priv(dev_get_drvdata(dev));
	struct i2c_client *client = data->client;
	mutex_lock(&data->lock);
	stk8321_set_power_mode(client, STK8321_POWMODE_NORMAL);
	mutex_unlock(&data->lock);
	return 0;
}

static DEFINE_SIMPLE_DEV_PM_OPS(stk8321_pm_ops, stk8321_suspend,
				stk8321_resume);

static const struct i2c_device_id stk8321_i2c_id[] = {
	{ "stk8321", 0 },
	{ "stkh8321", 0 },
	{}
};
MODULE_DEVICE_TABLE(i2c, stk8321_i2c_id);

static const struct acpi_device_id stk8321_acpi_match[] = {
	{ "STKH8321" },
	{ }
};
MODULE_DEVICE_TABLE(acpi, stk8321_acpi_match);

static struct i2c_driver stk8321_driver = {
	.driver = {
		.name = STK8321_DRIVER_NAME,
		.pm = pm_sleep_ptr(&stk8321_pm_ops),
		.acpi_match_table = stk8321_acpi_match,
	},
	.probe = stk8321_probe,
	.remove = stk8321_remove,
	.id_table = stk8321_i2c_id,
};

module_i2c_driver(stk8321_driver);

MODULE_AUTHOR("TUXEDO Computers GmbH <tux@tuxedocomputers.com>");
MODULE_DESCRIPTION("STK8321 3-Axis Accelerometer driver");
MODULE_LICENSE("GPL v2");
