// SPDX-License-Identifier: GPL-3.0+
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
#include <linux/kernel.h>
#include <linux/module.h>
#include <linux/acpi.h>
#include <linux/version.h>

#define DRIVER_NAME "gxtp7380"

static int gxtp7380_add(struct acpi_device *device)
{
	kobject_uevent(&device->dev.kobj, KOBJ_ADD);
	return 0;
}

#if LINUX_VERSION_CODE < KERNEL_VERSION(6, 2, 0)
static int gxtp7380_remove(struct acpi_device *device)
#else
static void gxtp7380_remove(struct acpi_device *device)
#endif
{
	kobject_uevent(&device->dev.kobj, KOBJ_REMOVE);
#if LINUX_VERSION_CODE < KERNEL_VERSION(6, 2, 0)
	return 0;
#endif
}

static void gxtp7380_notify(struct acpi_device *device, u32 event)
{
	kobject_uevent(&device->dev.kobj, KOBJ_CHANGE);
}

static const struct acpi_device_id gxtp7380_device_ids[] = {
	{ "GXTP7380", 0 },
	{ "", 0 }
};

static struct acpi_driver gxtp7380_driver = {
	.name = DRIVER_NAME,
	.class = DRIVER_NAME,
	.ids = gxtp7380_device_ids,
	.flags = ACPI_DRIVER_ALL_NOTIFY_EVENTS,
	.ops = {
		.add = gxtp7380_add,
		.remove = gxtp7380_remove,
		.notify = gxtp7380_notify,
	},
};

module_acpi_driver(gxtp7380_driver);

MODULE_AUTHOR("TUXEDO Computers GmbH <tux@tuxedocomputers.com>");
MODULE_DESCRIPTION("Touch panel disable, notify driver");
MODULE_LICENSE("GPL v3");

MODULE_DEVICE_TABLE(acpi, gxtp7380_device_ids);
