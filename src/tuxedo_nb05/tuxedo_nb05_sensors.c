/*!
 * Copyright (c) 2023 TUXEDO Computers GmbH <tux@tuxedocomputers.com>
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
#include <linux/module.h>
#include <linux/hwmon.h>
#include <linux/platform_device.h>
#include "tuxedo_nb05_ec.h"

static int read_cpu_temp(void)
{
	u8 cpu_temp;
	nb05_read_ec_ram(0x470, &cpu_temp);
	return cpu_temp;
}

static int read_fan1_rpm(void)
{
	u8 rpm_low, rpm_high;
	nb05_read_ec_ram(0x298, &rpm_high);
	nb05_read_ec_ram(0x298, &rpm_low);
	return (rpm_high << 8) | rpm_low;
}

static int read_fan2_rpm(void)
{
	u8 rpm_low, rpm_high;
	nb05_read_ec_ram(0x218, &rpm_high);
	nb05_read_ec_ram(0x219, &rpm_low);
	return (rpm_high << 8) | rpm_low;
}

static const char * const temp_labels[] = {
	"cpu0"
};

static const char * const fan_labels[] = {
	"cpu0",
	"cpu1"
};

struct driver_data_t {
	int fan_cpu_max;
	int fan_cpu_min;
};

struct driver_data_t driver_data;

static umode_t
tuxedo_nb05_hwmon_is_visible(const void *drvdata, enum hwmon_sensor_types type,
			     u32 attr, int channel)
{
	return 0444;
}

static int
tuxedo_nb05_hwmon_read(struct device *dev, enum hwmon_sensor_types type,
		       u32 attr, int channel, long *val)
{
	struct driver_data_t *driver_data = dev_get_drvdata(dev);

	switch (type) {
	case hwmon_temp:
		*val = read_cpu_temp() * 1000;
		return 0;
	case hwmon_fan:
		switch (attr) {
		case hwmon_fan_min:
			if (channel == 0) {
				*val = driver_data->fan_cpu_min;
				return 0;
			} else if (channel == 1) {
				*val = driver_data->fan_cpu_min;
				return 0;
			}
			break;
		case hwmon_fan_max:
			if (channel == 0) {
				*val = driver_data->fan_cpu_max;
				return 0;
			} else if (channel == 1) {
				*val = driver_data->fan_cpu_max;
				return 0;
			}
			break;
		case hwmon_fan_input:
			if (channel == 0) {
				*val = read_fan1_rpm();
				return 0;
			} else if (channel == 1) {
				*val = read_fan2_rpm();
				return 0;
			}
		default:
			break;
		}
	default:
		break;
	}

	return -EOPNOTSUPP;
}

static int
tuxedo_nb05_hwmon_read_string(struct device *dev, enum hwmon_sensor_types type,
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

static const struct hwmon_ops tuxedo_nb05_hwmon_ops = {
	.is_visible = tuxedo_nb05_hwmon_is_visible,
	.read = tuxedo_nb05_hwmon_read,
	.read_string = tuxedo_nb05_hwmon_read_string
};

static const struct hwmon_channel_info *const tuxedo_nb05_hwmon_info[] = {
	HWMON_CHANNEL_INFO(temp,
			   HWMON_T_INPUT | HWMON_T_LABEL),
	HWMON_CHANNEL_INFO(fan,
			   HWMON_F_INPUT | HWMON_F_LABEL | HWMON_F_MIN | HWMON_F_MAX,
			   HWMON_F_INPUT | HWMON_F_LABEL | HWMON_F_MIN | HWMON_F_MAX),
	NULL
};

static const struct hwmon_chip_info tuxedo_nb05_hwmon_chip_info = {
	.ops = &tuxedo_nb05_hwmon_ops,
	.info = tuxedo_nb05_hwmon_info
};

static int __init tuxedo_nb05_sensors_probe(struct platform_device *pdev) {
	struct device *hwmon_dev;

	pr_debug("driver_probe\n");

	driver_data.fan_cpu_max=5000;
	driver_data.fan_cpu_min=0;

	hwmon_dev = devm_hwmon_device_register_with_info(&pdev->dev,
							 "tuxedo",
							 &driver_data,
							 &tuxedo_nb05_hwmon_chip_info,
							 NULL);

	return PTR_ERR_OR_ZERO(hwmon_dev);
}

static struct platform_device *tuxedo_nb05_sensors_device;
static struct platform_driver tuxedo_nb05_sensors_driver = {
	.driver.name = "tuxedo_nb05_sensors",
};

static int __init tuxedo_nb05_sensors_init(void)
{
	tuxedo_nb05_sensors_device =
		platform_create_bundle(&tuxedo_nb05_sensors_driver,
				       tuxedo_nb05_sensors_probe, NULL, 0, NULL, 0);

	if (IS_ERR(tuxedo_nb05_sensors_device))
		return PTR_ERR(tuxedo_nb05_sensors_device);

	return 0;
}

static void __exit tuxedo_nb05_sensors_exit(void)
{
	platform_device_unregister(tuxedo_nb05_sensors_device);
	platform_driver_unregister(&tuxedo_nb05_sensors_driver);
}

module_init(tuxedo_nb05_sensors_init);
module_exit(tuxedo_nb05_sensors_exit);

MODULE_AUTHOR("TUXEDO Computers GmbH <tux@tuxedocomputers.com>");
MODULE_DESCRIPTION("TUXEDO Computers NB05 sensors driver");
MODULE_LICENSE("GPL");
MODULE_VERSION("0.0.1");
