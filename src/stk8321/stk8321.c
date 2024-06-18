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
#include <linux/interrupt.h>
#include <linux/kernel.h>
#include <linux/module.h>
#include <linux/delay.h>
#include <linux/iio/buffer.h>
#include <linux/iio/iio.h>
#include <linux/iio/sysfs.h>
#include <linux/iio/trigger.h>
#include <linux/iio/triggered_buffer.h>
#include <linux/iio/trigger_consumer.h>

#define STK8321_DRIVER_NAME "stk8321"

#define STK8321_REG_CHIP_ID		0x00
#define STK8321_REG_SWRST		0x14

struct stk8321_data {
	struct i2c_client *client;
	struct mutex lock;
};

static int stk8321_probe(struct i2c_client *client)
{
	int ret;
	pr_debug("probe (addr: %02x)\n", client->addr);

	ret = i2c_smbus_write_byte_data(client, STK8321_REG_SWRST, 0x00);
	if (ret < 0) {
		pr_err("failed to reset sensor\n");
		return ret;
	}

	ret = i2c_smbus_read_byte_data(client, STK8321_REG_CHIP_ID);
	if (ret < 0)
		return ret;

	pr_debug("chip id: 0x%02x\n", ret);

	return 0;
}

static void stk8321_remove(struct i2c_client *client)
{
	pr_debug("remove (%02x)\n", client->addr);
}

static int stk8321_suspend(struct device *dev)
{
	pr_debug("suspend\n");
	return 0;
}

static int stk8321_resume(struct device *dev)
{
	pr_debug("resume\n");
	return 0;
}

static DEFINE_SIMPLE_DEV_PM_OPS(stk8321_pm_ops, stk8321_suspend,
				stk8321_resume);

static const struct i2c_device_id stk8321_i2c_id[] = {
	{ "stk8321", 0 },
	{ "stk8321-display", 0 },
	{ "stk8321-base", 0 },
	{}
};
MODULE_DEVICE_TABLE(i2c, stk8321_i2c_id);

static struct i2c_driver stk8321_driver = {
	.driver = {
		.name = STK8321_DRIVER_NAME,
		.pm = pm_sleep_ptr(&stk8321_pm_ops),
	},
	.probe = stk8321_probe,
	.remove = stk8321_remove,
	.id_table = stk8321_i2c_id,
};

module_i2c_driver(stk8321_driver);

MODULE_AUTHOR("TUXEDO Computers GmbH <tux@tuxedocomputers.com>");
MODULE_DESCRIPTION("STK8321 3-Axis Accelerometer driver");
MODULE_LICENSE("GPL v2");
