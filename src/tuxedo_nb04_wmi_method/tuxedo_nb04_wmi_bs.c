/*!
 * Copyright (c) 2023 TUXEDO Computers GmbH <tux@tuxedocomputers.com>
 *
 * This file is part of tuxedo-keyboard.
 *
 * tuxedo-keyboard is free software: you can redistribute it and/or modify
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
#include <linux/acpi.h>
#include <linux/module.h>
#include <linux/wmi.h>
#include <linux/keyboard.h>
#include <linux/input.h>
#include <linux/input/sparse-keymap.h>
#include <linux/hwmon.h>
#include <linux/version.h>
#include <linux/delay.h>

#define NB04_WMI_AB_GUID	"80C9BAA6-AC48-4538-9234-9F81A55E7C85"
#define NB04_WMI_BB_GUID	"B8BED7BB-3F3D-4C71-953D-6D4172F27A63"
#define NB04_WMI_BS_GUID	"1F174999-3A4E-4311-900D-7BE7166D5055"

static const char * const temp_labels[] = {
	"cpu-fan",
	"gpu-fan"
};

static const char * const fan_labels[] = {
	"cpu0",
	"gpu0"
};

struct driver_data_t {
	int fan_cpu_max;
	int fan_cpu_min;
	int fan_gpu_max;
	int fan_gpu_min;
};

static umode_t
nb04_wmi_temp_is_visible(const void *drvdata, enum hwmon_sensor_types type,
			 u32 attr, int channel)
{
	return 0444;
}

static int
nb04_wmi_temp_read(struct device *dev, enum hwmon_sensor_types type,
		   u32 attr, int channel, long *val)
{
	struct driver_data_t *driver_data = dev_get_drvdata(dev);

	switch (type) {
	case hwmon_temp:
		// TODO: Read from HW
		*val = 15000;
		return 0;
	case hwmon_fan:
		switch (attr) {
		case hwmon_fan_min:
			if (channel == 0) {
				*val = driver_data->fan_cpu_min;
				return 0;
			} else if (channel == 1) {
				*val = driver_data->fan_gpu_min;
				return 0;
			}
			break;
		case hwmon_fan_max:
			if (channel == 0) {
				*val = driver_data->fan_cpu_max;
				return 0;
			} else if (channel == 1) {
				*val = driver_data->fan_gpu_max;
				return 0;
			}
			break;
		case hwmon_fan_input:
			// TODO: Read from HW
			*val = 2400;
			return 0;
		default:
			break;
		}
	default:
		break;
	}

	return -EOPNOTSUPP;
}

static int
nb04_wmi_temp_read_string(struct device *dev, enum hwmon_sensor_types type,
			  u32 attr, int channel, const char **str)
{
	switch (type) {
	case hwmon_temp:
		*str = temp_labels[channel];
		return 0;
	case hwmon_fan:
		*str = fan_labels[channel];
		return 0;
	default:
		break;
	}

	return -EOPNOTSUPP;
}

static const struct hwmon_ops nb04_wmi_temp_ops = {
	.is_visible = nb04_wmi_temp_is_visible,
	.read = nb04_wmi_temp_read,
	.read_string = nb04_wmi_temp_read_string
};

static const struct hwmon_channel_info *const nb04_wmi_temp_info[] = {
	HWMON_CHANNEL_INFO(temp,
			   HWMON_T_INPUT | HWMON_T_LABEL,
			   HWMON_T_INPUT | HWMON_T_LABEL),
	HWMON_CHANNEL_INFO(fan,
			   HWMON_F_INPUT | HWMON_F_LABEL | HWMON_F_MIN | HWMON_F_MAX,
			   HWMON_F_INPUT | HWMON_F_LABEL | HWMON_F_MIN | HWMON_F_MAX),
	NULL
};

static const struct hwmon_chip_info nb04_wmi_chip_info = {
	.ops = &nb04_wmi_temp_ops,
	.info = nb04_wmi_temp_info
};

#if LINUX_VERSION_CODE < KERNEL_VERSION(5, 3, 0)
static int tuxedo_nb04_wmi_probe(struct wmi_device *wdev)
#else
static int tuxedo_nb04_wmi_probe(struct wmi_device *wdev, const void *dummy_context)
#endif
{
	struct driver_data_t *driver_data;
	struct device *hwmon_dev;

	pr_debug("driver probe\n");

	driver_data = devm_kzalloc(&wdev->dev, sizeof(struct driver_data_t), GFP_KERNEL);
	if (!driver_data)
		return -ENOMEM;

	dev_set_drvdata(&wdev->dev, driver_data);

	// TODO: Initial read from HW
	driver_data->fan_cpu_max=10000;
	driver_data->fan_cpu_min=0;
	driver_data->fan_gpu_max=10000;
	driver_data->fan_gpu_min=0;

	hwmon_dev = devm_hwmon_device_register_with_info(&wdev->dev,
							 "tuxedo",
							 driver_data,
							 &nb04_wmi_chip_info,
							 NULL);

	return 0;
}

#if LINUX_VERSION_CODE < KERNEL_VERSION(5, 13, 0)
static int tuxedo_nb04_wmi_remove(struct wmi_device *wdev)
#else
static void tuxedo_nb04_wmi_remove(struct wmi_device *wdev)
#endif
{
	pr_debug("driver remove\n");

#if LINUX_VERSION_CODE < KERNEL_VERSION(5, 13, 0)
	return 0;
#endif
}

static const struct wmi_device_id tuxedo_nb04_wmi_device_ids[] = {
	{ .guid_string = NB04_WMI_BS_GUID },
	{ }
};

static struct wmi_driver tuxedo_nb04_wmi_driver = {
	.driver = {
		.name = "tuxedo_nb04_wmi",
		.owner = THIS_MODULE
	},
	.id_table = tuxedo_nb04_wmi_device_ids,
	.probe = tuxedo_nb04_wmi_probe,
	.remove = tuxedo_nb04_wmi_remove,
};

module_wmi_driver(tuxedo_nb04_wmi_driver);

MODULE_AUTHOR("TUXEDO Computers GmbH <tux@tuxedocomputers.com>");
MODULE_DESCRIPTION("Driver for NB04 WMI BS methods");
MODULE_VERSION("0.0.1");
MODULE_LICENSE("GPL");

MODULE_DEVICE_TABLE(wmi, tuxedo_nb04_wmi_device_ids);
MODULE_ALIAS("wmi:" NB04_WMI_BS_GUID);
