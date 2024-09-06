/*!
 * Copyright (c) 2023 LWL Computers GmbH <tux@tuxedocomputers.com>
 *
 * This file is part of lwl-drivers.
 *
 * lwl-drivers is free software: you can redistribute it and/or modify
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
#include <linux/version.h>
#include "../lwl_compatibility_check/lwl_compatibility_check.h"
#include "lwl_nb04_wmi_bs.h"

#define BS_INPUT_BUFFER_LENGTH	8
#define BS_OUTPUT_BUFFER_LENGTH	80

struct driver_data_t {};

static DEFINE_MUTEX(nb04_wmi_bs_access_lock);

static struct wmi_device *__wmi_dev;

bool nb04_wmi_bs_available(void)
{
	if (__wmi_dev)
		return true;
	else
		return false;
}
EXPORT_SYMBOL(nb04_wmi_bs_available);

static int __nb04_wmi_bs_method(struct wmi_device *wdev, u32 wmi_method_id,
				u8 *in, u8 *out)
{
	struct acpi_buffer acpi_buffer_in = { (acpi_size)BS_INPUT_BUFFER_LENGTH, in };
	struct acpi_buffer return_buffer = { ACPI_ALLOCATE_BUFFER, NULL };
	union acpi_object *acpi_object_out;
	acpi_status status;

	mutex_lock(&nb04_wmi_bs_access_lock);

	pr_debug("evaluate: %u\n", wmi_method_id);
	status = wmidev_evaluate_method(wdev, 0, wmi_method_id,
					&acpi_buffer_in, &return_buffer);

	mutex_unlock(&nb04_wmi_bs_access_lock);

	if (ACPI_FAILURE(status)) {
		pr_err("failed to evaluate wmi method %u\n", wmi_method_id);
		return -EIO;
	}

	acpi_object_out = (union acpi_object *) return_buffer.pointer;
	if (!acpi_object_out)
		return -ENODATA;

	if (acpi_object_out->type != ACPI_TYPE_BUFFER) {
		// Returns an int 0 when not finding a valid method number
		kfree(return_buffer.pointer);
		return -EINVAL;
	}

	if (acpi_object_out->buffer.length != BS_OUTPUT_BUFFER_LENGTH) {
		pr_err("Unexpected buffer length: %u for method (%u) call\n", 
		       acpi_object_out->buffer.length, wmi_method_id);
		kfree(return_buffer.pointer);
		return -EIO;
	}

	memcpy(out, acpi_object_out->buffer.pointer, BS_OUTPUT_BUFFER_LENGTH);
	kfree(return_buffer.pointer);

	return 0;
}

/**
 * Method interface 8 bytes in 80 bytes out
 */
int nb04_wmi_bs_method(u32 wmi_method_id, u8 *in, u8 *out)
{
	if (__wmi_dev)
		return __nb04_wmi_bs_method(__wmi_dev, wmi_method_id, in, out);
	else
		return -ENODEV;
}
EXPORT_SYMBOL(nb04_wmi_bs_method);

#if LINUX_VERSION_CODE < KERNEL_VERSION(5, 3, 0)
static int lwl_nb04_wmi_probe(struct wmi_device *wdev)
#else
static int lwl_nb04_wmi_probe(struct wmi_device *wdev, const void *dummy_context)
#endif
{
	struct driver_data_t *driver_data;

	pr_debug("driver probe\n");

	if (!lwl_is_compatible())
		return -ENODEV;

	if (!wmi_has_guid(NB04_WMI_BS_GUID))
		return -ENODEV;

	__wmi_dev = wdev;

	driver_data = devm_kzalloc(&wdev->dev, sizeof(struct driver_data_t), GFP_KERNEL);
	if (!driver_data)
		return -ENOMEM;

	dev_set_drvdata(&wdev->dev, driver_data);

	return 0;
}

#if LINUX_VERSION_CODE < KERNEL_VERSION(5, 13, 0)
static int lwl_nb04_wmi_remove(struct wmi_device *wdev)
#else
static void lwl_nb04_wmi_remove(struct wmi_device *wdev)
#endif
{
	pr_debug("driver remove\n");

#if LINUX_VERSION_CODE < KERNEL_VERSION(5, 13, 0)
	return 0;
#endif
}

static const struct wmi_device_id lwl_nb04_wmi_device_ids[] = {
	{ .guid_string = NB04_WMI_BS_GUID },
	{ }
};

static struct wmi_driver lwl_nb04_wmi_driver = {
	.driver = {
		.name = "lwl_nb04_wmi",
		.owner = THIS_MODULE
	},
	.id_table = lwl_nb04_wmi_device_ids,
	.probe = lwl_nb04_wmi_probe,
	.remove = lwl_nb04_wmi_remove,
};

module_wmi_driver(lwl_nb04_wmi_driver);

MODULE_AUTHOR("LWL Computers GmbH <tux@tuxedocomputers.com>");
MODULE_DESCRIPTION("Driver for NB04 WMI BS methods");
MODULE_LICENSE("GPL");

MODULE_DEVICE_TABLE(wmi, lwl_nb04_wmi_device_ids);
