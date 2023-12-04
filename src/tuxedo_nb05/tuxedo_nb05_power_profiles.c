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
#include <linux/version.h>
#include <linux/delay.h>

#define NB05_WMI_METHOD_BA_GUID	"99D89064-8D50-42BB-BEA9-155B2E5D0FCD"

struct driver_data_t {
};

#if LINUX_VERSION_CODE < KERNEL_VERSION(5, 3, 0)
static int tuxedo_nb05_power_profiles_probe(struct wmi_device *wdev)
#else
static int tuxedo_nb05_power_profiles_probe(struct wmi_device *wdev, const void *dummy_context)
#endif
{
	struct driver_data_t *driver_data;

	pr_debug("driver probe\n");

	driver_data = devm_kzalloc(&wdev->dev, sizeof(*driver_data), GFP_KERNEL);
	if (!driver_data)
		return -ENOMEM;

	dev_set_drvdata(&wdev->dev, driver_data);

	return 0;
}

#if LINUX_VERSION_CODE < KERNEL_VERSION(5, 13, 0)
static int tuxedo_nb05_power_profiles_remove(struct wmi_device *wdev)
#else
static void tuxedo_nb05_power_profiles_remove(struct wmi_device *wdev)
#endif
{
	pr_debug("driver remove\n");
	//struct driver_data_t *driver_data = dev_get_drvdata(&wdev->dev);

#if LINUX_VERSION_CODE < KERNEL_VERSION(5, 13, 0)
	return 0;
#endif
}

static const struct wmi_device_id tuxedo_nb05_power_profiles_device_ids[] = {
	{ .guid_string = NB05_WMI_METHOD_BA_GUID },
	{ }
};

static struct wmi_driver tuxedo_nb05_power_profiles_driver = {
	.driver = {
		.name = "tuxedo_nb05_power_profiles",
		.owner = THIS_MODULE
	},
	.id_table = tuxedo_nb05_power_profiles_device_ids,
	.probe = tuxedo_nb05_power_profiles_probe,
	.remove = tuxedo_nb05_power_profiles_remove,
};

module_wmi_driver(tuxedo_nb05_power_profiles_driver);

MODULE_AUTHOR("TUXEDO Computers GmbH <tux@tuxedocomputers.com>");
MODULE_DESCRIPTION("Driver for NB05 keyboard events");
MODULE_VERSION("0.0.1");
MODULE_LICENSE("GPL");

MODULE_DEVICE_TABLE(wmi, tuxedo_nb05_power_profiles_device_ids);
MODULE_ALIAS("wmi:" NB05_WMI_METHOD_BA_GUID);