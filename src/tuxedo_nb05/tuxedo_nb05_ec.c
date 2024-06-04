/*!
 * Copyright (c) 2023-2024 TUXEDO Computers GmbH <tux@tuxedocomputers.com>
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
#include <linux/kernel.h>
#include <linux/platform_device.h>
#include <linux/dmi.h>
#include <linux/acpi.h>
#include <linux/delay.h>
#include <asm/io.h>
#include "tuxedo_nb05_ec.h"
#include "../tuxedo_compatibility_check/tuxedo_compatibility_check.h"

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

	return 0;
}

static struct platform_driver tuxedo_nb05_ec_driver = {
	.driver		= {
		.name	= KBUILD_MODNAME,
	},
	.probe		= tuxedo_nb05_ec_probe,
};

static struct platform_device *tuxedo_nb05_ec_device;

static int __init dmi_check_callback(const struct dmi_system_id *id)
{
	printk(KERN_INFO KBUILD_MODNAME ": found model '%s'\n", id->ident);
	return 1;
}

static const struct dmi_system_id tuxedo_nb05_ec_id_table[] __initconst = {
	{
		.ident = "TUXEDO Pulse 14 Gen3",
		.matches = {
			DMI_MATCH(DMI_SYS_VENDOR, "TUXEDO"),
			DMI_MATCH(DMI_BOARD_VENDOR, "NB05"),
			DMI_MATCH(DMI_PRODUCT_SKU, "PULSE1403"),
		},
		.callback = dmi_check_callback,
	},
{
		.ident = "TUXEDO Pulse 14 Gen4",
		.matches = {
			DMI_MATCH(DMI_SYS_VENDOR, "TUXEDO"),
			DMI_MATCH(DMI_BOARD_VENDOR, "NB05"),
			DMI_MATCH(DMI_PRODUCT_SKU, "PULSE1404"),
		},
		.callback = dmi_check_callback,
	},
	{ },
};
MODULE_DEVICE_TABLE(dmi, tuxedo_nb05_ec_id_table);

static int __init tuxedo_nb05_ec_init(void)
{
	if (!dmi_check_system(tuxedo_nb05_ec_id_table))
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
