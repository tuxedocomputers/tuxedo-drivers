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
#include "tuxi_acpi.h"

#define DRIVER_NAME "tuxi_acpi"

struct tuxi_acpi_driver_data_t {
	struct acpi_device *tuxi_adev;
	acpi_handle tfan_handle;
};

static struct tuxi_acpi_driver_data_t *tuxi_driver_data = NULL;

static
int evaluate_intparams(acpi_handle handle,
		       acpi_string pathname,
		       unsigned long long *int_params,
		       u32 param_count,
		       unsigned long long *retval);

static
int evaluate_intparams(acpi_handle handle,
		       acpi_string pathname,
		       unsigned long long *int_params,
		       u32 param_count,
		       unsigned long long *retval)
{
	struct acpi_object_list input;
	union acpi_object *params;
	unsigned long long result;
	acpi_status status;
	int i;
	u32 param_buffer_size = sizeof(union acpi_object) * param_count;

	if (!handle)
		return -ENODEV;

	if (param_buffer_size > 0)
		params = kzalloc(param_buffer_size, GFP_KERNEL);

	for (i = 0; i < param_count; ++i) {
		params[i].type = ACPI_TYPE_INTEGER;
		params[i].integer.value = int_params[i];
	}

	input.count = param_count;
	input.pointer = params;

	if (param_buffer_size > 0) {
		status = acpi_evaluate_integer(handle, pathname, &input, &result);
		kfree(params);
	} else {
		status = acpi_evaluate_integer(handle, pathname, NULL, &result);
	}

	if (ACPI_FAILURE(status))
		return -EIO;

	if (retval)
		*retval = result;

	return 0;
}

int tuxi_set_fan_speed(u8 fan_index, u8 fan_speed)
{
	long long int retval;
	long long int args[] = { fan_index, fan_speed };
	int err;
	err = evaluate_intparams(tuxi_driver_data->tfan_handle,
				 "SSPD",
				 args, ARRAY_SIZE(args),
				 &retval);
	if (err)
		return err;

	if (retval)
		return -EINVAL;

	return 0;
}
EXPORT_SYMBOL(tuxi_set_fan_speed);

int tuxi_get_fan_speed(u8 fan_index, u8 *fan_speed)
{
	long long int retval;
	long long int args[] = { fan_index };
	int err;
	err = evaluate_intparams(tuxi_driver_data->tfan_handle,
				 "GSPD",
				 args, ARRAY_SIZE(args),
				 &retval);
	if (err)
		return err;

	if (retval < 0)
		return -EINVAL;

	*fan_speed = (u8) retval;

	return 0;
}
EXPORT_SYMBOL(tuxi_get_fan_speed);

int tuxi_get_nr_fans(u8 *nr_fans)
{
	long long int retval;
	int err;
	err = evaluate_intparams(tuxi_driver_data->tfan_handle,
				 "GCNT",
				 NULL, 0,
				 &retval);
	if (err)
		return err;

	*nr_fans = (u8) retval;

	return 0;
}
EXPORT_SYMBOL(tuxi_get_nr_fans);

// TODO: mode enum signature
int tuxi_set_fan_mode(u8 mode)
{
	long long int retval;
	long long int args[] = { mode };
	int err;
	err = evaluate_intparams(tuxi_driver_data->tfan_handle,
				 "SMOD",
				 args, ARRAY_SIZE(args),
				 &retval);
	if (err)
		return err;

	if (retval < 0)
		return -EINVAL;

	return 0;
}

int tuxi_get_fan_mode(u8 *mode)
{
	long long int retval;
	int err;
	err = evaluate_intparams(tuxi_driver_data->tfan_handle,
				 "GMOD",
				 NULL, 0,
				 &retval);
	if (err)
		return err;

	if (retval < 0)
		return -EINVAL;

	*mode = (u8) retval;

	return 0;
}
EXPORT_SYMBOL(tuxi_get_fan_mode);

int tuxi_get_fan_type(u8 fan_index, enum tuxi_fan_type *type)
{
	long long int retval;
	long long int args[] = { fan_index };
	int err;
	err = evaluate_intparams(tuxi_driver_data->tfan_handle,
				 "GTYP",
				 args, ARRAY_SIZE(args),
				 &retval);
	if (err)
		return err;

	if (retval < 0)
		return -EINVAL;

	*type = (u8) retval;

	return 0;
}
EXPORT_SYMBOL(tuxi_get_fan_type);

static int get_tfan(struct acpi_device *tuxi_dev, acpi_handle *tfan_handle)
{
	acpi_status status;
	status = acpi_get_handle(tuxi_dev->handle, "TFAN", tfan_handle);
	if (ACPI_FAILURE(status))
		return -ENODEV;
	return 0;
}

static int tuxi_acpi_add(struct acpi_device *device)
{
	struct tuxi_acpi_driver_data_t *driver_data;
	int err;

	driver_data = devm_kzalloc(&device->dev, sizeof(*driver_data), GFP_KERNEL);
	if (!driver_data)
		return -ENOMEM;

	driver_data->tuxi_adev = device;
	device->driver_data = driver_data;

	// Find subdevices
	err = get_tfan(device, &driver_data->tfan_handle);
	if (err)
		driver_data->tfan_handle = NULL;

	if (!driver_data->tfan_handle)
		pr_info("no interface found\n");

	tuxi_driver_data = driver_data;

	pr_info("interface initialized\n");

	return 0;
}

#if LINUX_VERSION_CODE < KERNEL_VERSION(6, 2, 0)
static int tuxi_acpi_remove(struct acpi_device *device)
#else
static void tuxi_acpi_remove(struct acpi_device *device)
#endif
{
	tuxi_driver_data = NULL;
	pr_debug("driver remove\n");
#if LINUX_VERSION_CODE < KERNEL_VERSION(6, 2, 0)
	return 0;
#endif
}

void tuxi_acpi_notify(struct acpi_device *device, u32 event)
{
	pr_debug("event: %d\n", event);
}

#ifdef CONFIG_PM
static int driver_suspend_callb(struct device *dev)
{
	pr_debug("driver suspend\n");
	return 0;
}

static int driver_resume_callb(struct device *dev)
{
	pr_debug("driver resume\n");
	return 0;
}

static SIMPLE_DEV_PM_OPS(tuxi_driver_pm_ops, driver_suspend_callb, driver_resume_callb);
#endif

static const struct acpi_device_id tuxi_acpi_device_ids[] = {
	{ TUXI_ACPI_RESOURCE_HID, 0 },
	{ "", 0 }
};

static struct acpi_driver tuxi_acpi_driver = {
	.name = DRIVER_NAME,
	.class = DRIVER_NAME,
	.owner = THIS_MODULE,
	.ids = tuxi_acpi_device_ids,
	.flags = ACPI_DRIVER_ALL_NOTIFY_EVENTS,
	.ops = {
		.add = tuxi_acpi_add,
		.remove = tuxi_acpi_remove,
		.notify = tuxi_acpi_notify,
	},
#ifdef CONFIG_PM
	.drv.pm = &tuxi_driver_pm_ops
#endif
};

module_acpi_driver(tuxi_acpi_driver);

MODULE_AUTHOR("TUXEDO Computers GmbH <tux@tuxedocomputers.com>");
MODULE_DESCRIPTION("Driver for TUXEDO ACPI interface");
MODULE_LICENSE("GPL");

MODULE_DEVICE_TABLE(acpi, tuxi_acpi_device_ids);
