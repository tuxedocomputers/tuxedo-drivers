// SPDX-License-Identifier: GPL-2.0+
/*!
 * Copyright (c) 2024-2026 TUXEDO Computers GmbH <tux@tuxedocomputers.com>
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

#include <linux/kernel.h>
#include <linux/platform_device.h>
#include <linux/module.h>
#include <linux/version.h>
#include <linux/acpi.h>

#define DRIVER_NAME "gxtp7380"

static void gxtp7380_notify(acpi_handle handle, u32 event, void *data)
{
	struct acpi_device *acpi = data;
	kobject_uevent(&acpi->dev.kobj, KOBJ_CHANGE);
}

static int gxtp7380_probe(struct platform_device *pdev)
{
	struct acpi_device *acpi;
	int error;

	acpi = ACPI_COMPANION(&pdev->dev);
	if (!acpi)
		return -ENODEV;

	error = acpi_dev_install_notify_handler(acpi, ACPI_ALL_NOTIFY, gxtp7380_notify, acpi);
	if (error)
		return error;

	kobject_uevent(&pdev->dev.kobj, KOBJ_ADD);
	return 0;
}

#if LINUX_VERSION_CODE < KERNEL_VERSION(6, 11, 0)
static int gxtp7380_remove(struct platform_device *pdev)
#else
static void gxtp7380_remove(struct platform_device *pdev)
#endif
{
	struct acpi_device *acpi;

	acpi = ACPI_COMPANION(&pdev->dev);
	acpi_dev_remove_notify_handler(acpi, ACPI_ALL_NOTIFY, gxtp7380_notify);

	kobject_uevent(&pdev->dev.kobj, KOBJ_REMOVE);

#if LINUX_VERSION_CODE < KERNEL_VERSION(6, 11, 0)
	return 0;
#endif
}

static const struct acpi_device_id gxtp7380_device_ids[] = {
	{ "GXTP7380", 0 },
	{ "", 0 }
};
MODULE_DEVICE_TABLE(acpi, gxtp7380_device_ids);

static struct platform_driver gxtp7380_driver = {
	.driver = {
		.name = DRIVER_NAME,
		.acpi_match_table = gxtp7380_device_ids,
	},
	.probe = gxtp7380_probe,
	.remove = gxtp7380_remove,
};

static int __init gxtp7380_driver_init(void)
{
	struct acpi_device *acpi;
	struct platform_device_info pdevinfo = {0};
	int ret;

	acpi = acpi_dev_get_first_match_dev("GXTP7380", NULL, -1);
	if (!acpi)
		return -ENODEV;

	pdevinfo.name   = DRIVER_NAME;
        pdevinfo.id     = PLATFORM_DEVID_NONE;
        pdevinfo.fwnode = acpi_fwnode_handle(acpi);

	pdev = platform_device_register_full(&pdevinfo);
	if (IS_ERR(pdev))
		return PTR_ERR(pdev);

	ret = platform_driver_register(&gxtp7380_driver);
	if (ret) {
		platform_device_unregister(pdev);
		pdev = NULL;
		return ret;
	}

	return 0;
}

static void __exit gxtp7380_driver_exit(void)
{
	platform_driver_unregister(&gxtp7380_driver);

	if (pdev)
		platform_device_unregister(pdev);

	pdev = NULL;
}

module_init(gxtp7380_driver_init);
module_exit(gxtp7380_driver_exit);

MODULE_AUTHOR("TUXEDO Computers GmbH <tux@tuxedocomputers.com>");
MODULE_DESCRIPTION("Touch panel disable, notify driver");
MODULE_LICENSE("GPL");
