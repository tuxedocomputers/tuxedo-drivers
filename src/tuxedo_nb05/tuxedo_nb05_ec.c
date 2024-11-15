// SPDX-License-Identifier: GPL-2.0+
/*!
 * Copyright (c) 2023-2024 TUXEDO Computers GmbH <tux@tuxedocomputers.com>
 *
 * This file is part of tuxedo-drivers.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License along
 * with this program; if not, see <https://www.gnu.org/licenses/>.
 */

#define pr_fmt(fmt) KBUILD_MODNAME ": " fmt

#include <linux/module.h>
#include <linux/kernel.h>
#include <linux/platform_device.h>
#include <linux/dmi.h>
#include <linux/acpi.h>
#include <linux/delay.h>
#include <asm/io.h>
#include "tuxedo_nb05_ec.h"
#include "../tuxedo_compatibility_check/tuxedo_compatibility_check.h"

static struct nb05_ec_data_t ec_data;

#define EC_PORT_ADDR	0x4e
#define EC_PORT_DATA	0x4f

#define I2EC_REG_ADDR	0x2e
#define I2EC_REG_DATA	0x2f

#define I2EC_ADDR_LOW	0x10
#define I2EC_ADDR_HIGH	0x11
#define I2EC_ADDR_DATA	0x12

static DEFINE_MUTEX(nb05_ec_access_lock);

static void io_write(u8 reg, u8 data)
{
	outb(reg, EC_PORT_ADDR);
	outb(data, EC_PORT_DATA);
}

static u8 io_read(u8 reg)
{
	outb(reg, EC_PORT_ADDR);
	return inb(EC_PORT_DATA);
}

void nb05_read_ec_ram(u16 addr, u8 *data)
{
	u8 addr_high = (addr >> 8) & 0xff;
	u8 addr_low = (addr & 0xff);

	mutex_lock(&nb05_ec_access_lock);

	io_write(I2EC_REG_ADDR, I2EC_ADDR_HIGH);
	io_write(I2EC_REG_DATA, addr_high);

	io_write(I2EC_REG_ADDR, I2EC_ADDR_LOW);
	io_write(I2EC_REG_DATA, addr_low);

	io_write(I2EC_REG_ADDR, I2EC_ADDR_DATA);
	*data = io_read(I2EC_REG_DATA);

	mutex_unlock(&nb05_ec_access_lock);
}
EXPORT_SYMBOL(nb05_read_ec_ram);

void nb05_write_ec_ram(u16 addr, u8 data)
{
	u8 addr_high = (addr >> 8) & 0xff;
	u8 addr_low = (addr & 0xff);

	mutex_lock(&nb05_ec_access_lock);

	io_write(I2EC_REG_ADDR, I2EC_ADDR_HIGH);
	io_write(I2EC_REG_DATA, addr_high);

	io_write(I2EC_REG_ADDR, I2EC_ADDR_LOW);
	io_write(I2EC_REG_DATA, addr_low);

	io_write(I2EC_REG_ADDR, I2EC_ADDR_DATA);
	io_write(I2EC_REG_DATA, data);

	mutex_unlock(&nb05_ec_access_lock);
}
EXPORT_SYMBOL(nb05_write_ec_ram);

void nb05_read_ec_fw_version(u8 *major, u8 *minor)
{
	nb05_read_ec_ram(0x0400, major);
	nb05_read_ec_ram(0x0401, minor);
}
EXPORT_SYMBOL(nb05_read_ec_fw_version);

static int tuxedo_nb05_ec_probe(struct platform_device *pdev)
{
	u8 minor, major;

	nb05_read_ec_fw_version(&major, &minor);
	pr_info("EC I/O driver loaded, firmware version %d.%d\n", major, minor);

	ec_data.ver_major = major;
	ec_data.ver_minor = minor;

	return 0;
}

static struct platform_driver tuxedo_nb05_ec_driver = {
	.driver		= {
		.name	= KBUILD_MODNAME,
	},
	.probe		= tuxedo_nb05_ec_probe,
};

static struct platform_device *tuxedo_nb05_ec_device;

static int dmi_check_callback(const struct dmi_system_id *id)
{
	printk(KERN_INFO KBUILD_MODNAME ": found model '%s'\n", id->ident);
	ec_data.dev_data = id->driver_data;
	return 1;
}

struct nb05_device_data_t data_pulse = {
	.number_fans = 2,
	.fanctl_onereg = false,
};

struct nb05_device_data_t data_infinityflex = {
	.number_fans = 1,
	.fanctl_onereg = true,
};

static const struct dmi_system_id tuxedo_nb05_id_table[] = {
	{
		.ident = PULSE1403,
		.matches = {
			DMI_MATCH(DMI_SYS_VENDOR, "TUXEDO"),
			DMI_MATCH(DMI_BOARD_VENDOR, "NB05"),
			DMI_MATCH(DMI_PRODUCT_SKU, "PULSE1403"),
		},
		.callback = dmi_check_callback,
		.driver_data = &data_pulse,
	},
	{
		.ident = PULSE1404,
		.matches = {
			DMI_MATCH(DMI_SYS_VENDOR, "TUXEDO"),
			DMI_MATCH(DMI_BOARD_VENDOR, "NB05"),
			DMI_MATCH(DMI_PRODUCT_SKU, "PULSE1404"),
		},
		.callback = dmi_check_callback,
		.driver_data = &data_pulse,
	},
	{
		.ident = IFLX14I01,
		.matches = {
			DMI_MATCH(DMI_SYS_VENDOR, "TUXEDO"),
			DMI_MATCH(DMI_BOARD_VENDOR, "NB05"),
			DMI_MATCH(DMI_PRODUCT_SKU, "IFLX14I01"),
		},
		.callback = dmi_check_callback,
		.driver_data = &data_infinityflex,
	},
	{ },
};

MODULE_DEVICE_TABLE(dmi, tuxedo_nb05_id_table);

const struct dmi_system_id *nb05_match_device(void)
{
	return dmi_first_match(tuxedo_nb05_id_table);
}
EXPORT_SYMBOL(nb05_match_device);

void nb05_get_ec_data(struct nb05_ec_data_t **ec_data_pp)
{
	*ec_data_pp = &ec_data;
}
EXPORT_SYMBOL(nb05_get_ec_data);

static int __init tuxedo_nb05_ec_init(void)
{
	if (!dmi_check_system(tuxedo_nb05_id_table))
		return -ENODEV;

	if (!tuxedo_is_compatible())
		return -ENODEV;

	tuxedo_nb05_ec_device =
		platform_create_bundle(&tuxedo_nb05_ec_driver,
				       tuxedo_nb05_ec_probe,
				       NULL, 0, NULL, 0);

	return PTR_ERR_OR_ZERO(tuxedo_nb05_ec_device);
}

static void __exit tuxedo_nb05_ec_exit(void)
{
	platform_device_unregister(tuxedo_nb05_ec_device);
	platform_driver_unregister(&tuxedo_nb05_ec_driver);
}

module_init(tuxedo_nb05_ec_init);
module_exit(tuxedo_nb05_ec_exit);

MODULE_AUTHOR("TUXEDO Computers GmbH <tux@tuxedocomputers.com>");
MODULE_DESCRIPTION("Driver for NB05 EC I/O");
MODULE_LICENSE("GPL");
