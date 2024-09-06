/*!
 * Copyright (c) 2023 LWL Computers GmbH <tux@lwlcomputers.com>
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
#include <linux/delay.h>
#include "lwl_nb04_wmi_ab.h"
#include "../lwl_compatibility_check/lwl_compatibility_check.h"

#define dev_to_wdev(__dev)	container_of(__dev, struct wmi_device, dev)

static DEFINE_MUTEX(nb04_wmi_ab_lock);

static struct wmi_device *__wmi_dev;

#define KEYBOARD_MAX_BRIGHTNESS		0x0a
#define KEYBOARD_DEFAULT_BRIGHTNESS	0x00
#define KEYBOARD_DEFAULT_COLOR_RED	0xff
#define KEYBOARD_DEFAULT_COLOR_GREEN	0xff
#define KEYBOARD_DEFAULT_COLOR_BLUE	0xff

struct driver_data_t {};

static int
__nb04_wmi_ab_method_buffer(struct wmi_device *wdev, u32 wmi_method_id,
			    u8 *in, u8 *out)
{
	struct acpi_buffer acpi_buffer_in = { (acpi_size)AB_INPUT_BUFFER_LENGTH_NORMAL, in };
	struct acpi_buffer return_buffer = { ACPI_ALLOCATE_BUFFER, NULL };
	union acpi_object *acpi_object_out;
	acpi_status status;

	mutex_lock(&nb04_wmi_ab_lock);
	pr_debug("evaluate: %u\n", wmi_method_id);
	status = wmidev_evaluate_method(wdev, 0, wmi_method_id,
					&acpi_buffer_in, &return_buffer);
	mutex_unlock(&nb04_wmi_ab_lock);

	if (ACPI_FAILURE(status)) {
		pr_err("failed to evaluate wmi method %u\n", wmi_method_id);
		return -EIO;
	}

	acpi_object_out = (union acpi_object *) return_buffer.pointer;
	if (!acpi_object_out)
		return -ENODATA;

	if (acpi_object_out->type != ACPI_TYPE_BUFFER) {
		pr_err("No buffer for method (%u) call\n", wmi_method_id);
		kfree(return_buffer.pointer);
		return -EIO;
	}

	if (acpi_object_out->buffer.length != AB_OUTPUT_BUFFER_LENGTH) {
		pr_err("Unexpected buffer length: %u for method (%u) call\n", 
		       acpi_object_out->buffer.length, wmi_method_id);
		kfree(return_buffer.pointer);
		return -EIO;
	}

	memcpy(out, acpi_object_out->buffer.pointer, AB_OUTPUT_BUFFER_LENGTH);
	kfree(return_buffer.pointer);

	return 0;
}

/**
 * Method interface 8 bytes in 80 bytes out
 */
int nb04_wmi_ab_method_buffer(u32 wmi_method_id, u8 *in, u8 *out)
{
	if (__wmi_dev)
		return __nb04_wmi_ab_method_buffer(__wmi_dev, wmi_method_id, in, out);
	else
		return -ENODEV;
}
EXPORT_SYMBOL(nb04_wmi_ab_method_buffer);

static int
__nb04_wmi_ab_method_buffer_reduced_output(struct wmi_device *wdev, u32 wmi_method_id,
					   u8 *in, u8 *out)
{
	struct acpi_buffer acpi_buffer_in = { (acpi_size)AB_INPUT_BUFFER_LENGTH_NORMAL, in };
	struct acpi_buffer return_buffer = { ACPI_ALLOCATE_BUFFER, NULL };
	union acpi_object *acpi_object_out;
	acpi_status status;

	mutex_lock(&nb04_wmi_ab_lock);
	pr_debug("evaluate: %u\n", wmi_method_id);
	status = wmidev_evaluate_method(wdev, 0, wmi_method_id,
					&acpi_buffer_in, &return_buffer);
	mutex_unlock(&nb04_wmi_ab_lock);
	if (ACPI_FAILURE(status)) {
		pr_err("failed to evaluate wmi method %u\n", wmi_method_id);
		return -EIO;
	}

	acpi_object_out = (union acpi_object *) return_buffer.pointer;
	if (!acpi_object_out)
		return -ENODATA;

	if (acpi_object_out->type != ACPI_TYPE_BUFFER) {
		pr_err("No buffer for method (%u) call\n", wmi_method_id);
		kfree(return_buffer.pointer);
		return -EIO;
	}

	if (acpi_object_out->buffer.length != AB_OUTPUT_BUFFER_LENGTH_REDUCED) {
		pr_err("Unexpected buffer length: %u for method (%u) call\n", 
		       acpi_object_out->buffer.length, wmi_method_id);
		kfree(return_buffer.pointer);
		return -EIO;
	}

	memcpy(out, acpi_object_out->buffer.pointer, AB_OUTPUT_BUFFER_LENGTH_REDUCED);
	kfree(return_buffer.pointer);

	return 0;
}

/**
 * Method interface 8 bytes in 10 bytes out
 */
int nb04_wmi_ab_method_buffer_reduced_output(u32 wmi_method_id, u8 *in, u8 *out)
{
	if (__wmi_dev)
		return __nb04_wmi_ab_method_buffer_reduced_output(__wmi_dev, wmi_method_id, in, out);
	else
		return -ENODEV;
}
EXPORT_SYMBOL(nb04_wmi_ab_method_buffer_reduced_output);

static int
__nb04_wmi_ab_method_extended_input(struct wmi_device *wdev, u32 wmi_method_id,
				    u8 *in, u8 *out)
{
	struct acpi_buffer acpi_buffer_in = { (acpi_size)AB_INPUT_BUFFER_LENGTH_EXTENDED, in };
	struct acpi_buffer return_buffer = { ACPI_ALLOCATE_BUFFER, NULL };
	union acpi_object *acpi_object_out;
	acpi_status status;

	mutex_lock(&nb04_wmi_ab_lock);
	pr_debug("evaluate: %u\n", wmi_method_id);
	status = wmidev_evaluate_method(wdev, 0, wmi_method_id,
					&acpi_buffer_in, &return_buffer);
	mutex_unlock(&nb04_wmi_ab_lock);

	if (ACPI_FAILURE(status)) {
		pr_err("failed to evaluate wmi method %u\n", wmi_method_id);
		return -EIO;
	}

	acpi_object_out = (union acpi_object *) return_buffer.pointer;
	if (!acpi_object_out)
		return -ENODATA;

	if (acpi_object_out->type != ACPI_TYPE_BUFFER) {
		pr_err("No buffer for method (%u) call\n", wmi_method_id);
		kfree(return_buffer.pointer);
		return -EIO;
	}

	if (acpi_object_out->buffer.length != AB_OUTPUT_BUFFER_LENGTH) {
		pr_err("Unexpected buffer length: %u for method (%u) call\n", 
		       acpi_object_out->buffer.length, wmi_method_id);
		kfree(return_buffer.pointer);
		return -EIO;
	}

	memcpy(out, acpi_object_out->buffer.pointer, AB_OUTPUT_BUFFER_LENGTH);
	kfree(return_buffer.pointer);

	return 0;
}

/**
 * Method interface 496 bytes in 80 bytes out
 */
int nb04_wmi_ab_method_extended_input(u32 wmi_method_id, u8 *in, u8 *out)
{
	if (__wmi_dev)
		return __nb04_wmi_ab_method_extended_input(__wmi_dev, wmi_method_id, in, out);
	else
		return -ENODEV;
}
EXPORT_SYMBOL(nb04_wmi_ab_method_extended_input);

static int
__nb04_wmi_ab_method_int_out(struct wmi_device *wdev, u32 wmi_method_id,
			     u8 *in, u64 *out)
{
	struct acpi_buffer acpi_buffer_in = { (acpi_size)AB_INPUT_BUFFER_LENGTH_NORMAL, in };
	struct acpi_buffer return_buffer = { ACPI_ALLOCATE_BUFFER, NULL };
	union acpi_object *acpi_object_out;
	acpi_status status;

	mutex_lock(&nb04_wmi_ab_lock);
	pr_debug("evaluate: %u\n", wmi_method_id);
	status = wmidev_evaluate_method(wdev, 0, wmi_method_id,
					&acpi_buffer_in, &return_buffer);
	mutex_unlock(&nb04_wmi_ab_lock);

	if (ACPI_FAILURE(status)) {
		pr_err("failed to evaluate wmi method\n");
		return -EIO;
	}

	acpi_object_out = (union acpi_object *) return_buffer.pointer;
	if (!acpi_object_out)
		return -ENODATA;

	if (acpi_object_out->type != ACPI_TYPE_INTEGER) {
		pr_err("No integer\n");
		kfree(return_buffer.pointer);
		return -EIO;
	}

	*out = acpi_object_out->integer.value;
	kfree(return_buffer.pointer);

	return 0;
}

/**
 * Method interface 8 bytes in integer out
 */
int nb04_wmi_ab_method_int_out(u32 wmi_method_id, u8 *in, u64 *out)
{
	if (__wmi_dev)
		return __nb04_wmi_ab_method_int_out(__wmi_dev, wmi_method_id, in, out);
	else
		return -ENODEV;
}
EXPORT_SYMBOL(nb04_wmi_ab_method_int_out);

int wmi_update_device_status_keyboard(struct device_keyboard_status_t *kbds)
{
	u8 arg[AB_INPUT_BUFFER_LENGTH_NORMAL] = {0};
	u8 out[AB_OUTPUT_BUFFER_LENGTH] = {0};
	u16 wmi_return;
	int result, retry_count = 3;

	arg[0] = WMI_DEVICE_TYPE_ID_KEYBOARD;

	while (retry_count--) {
		result = nb04_wmi_ab_method_buffer(2, arg, out);
		if (result)
			return result;

		wmi_return = (out[1] << 8) | out[0];
		if (wmi_return != WMI_RETURN_STATUS_SUCCESS)
			return -EIO;

		if (out[3] < WMI_KEYBOARD_TYPE_MAX)
			break;

		// Sometimes initial read has proved to fail without having a fail
		// status. However the returned keyboard type in this case is invalid.
		pr_debug("Unexpected keyboard type (%d), retry read\n", out[3]);
		msleep(50);
	}

	// If, despite retries, not getting a valid keyboard type => give up
	if (out[3] >= WMI_KEYBOARD_TYPE_MAX) {
		pr_err("Failed to identifiy keyboard type\n");
		return -ENODEV;
	}

	kbds->keyboard_state_enabled = out[2] == 1;
	kbds->keyboard_type = out[3];
	kbds->keyboard_sidebar_support = out[4] == 1;
	kbds->keyboard_matrix = out[5];

	return 0;
}
EXPORT_SYMBOL(wmi_update_device_status_keyboard);

int wmi_set_whole_keyboard(u8 red, u8 green, u8 blue, int brightness)
{
	u8 arg[AB_INPUT_BUFFER_LENGTH_NORMAL] = {0};
	u64 out;
	u8 brightness_byte = 0xfe, enable_byte = 0x01;
	int result;

	if (brightness == 0)
		enable_byte = 0x00;
	else if (brightness > 0 && brightness <= 10)
		brightness_byte = brightness;

	arg[0] = 0x00;
	arg[1] = red;
	arg[2] = green;
	arg[3] = blue;
	arg[4] = brightness_byte;
	arg[5] = 0xfe;
	arg[6] = 0x00;
	arg[7] = enable_byte;

	result = nb04_wmi_ab_method_int_out(3, arg, &out);
	if (result)
		return result;

	if (out != 0)
		return -EIO;

	return 0;
}
EXPORT_SYMBOL(wmi_set_whole_keyboard);

#if LINUX_VERSION_CODE < KERNEL_VERSION(5, 3, 0)
static int lwl_nb04_wmi_ab_probe(struct wmi_device *wdev)
#else
static int lwl_nb04_wmi_ab_probe(struct wmi_device *wdev, const void *dummy_context)
#endif
{
	struct driver_data_t *driver_data;

	pr_debug("driver probe\n");

	if (!lwl_is_compatible())
		return -ENODEV;

	if (!wmi_has_guid(NB04_WMI_AB_GUID))
		return -ENODEV;

	driver_data = devm_kzalloc(&wdev->dev, sizeof(struct driver_data_t), GFP_KERNEL);
	if (!driver_data)
		return -ENOMEM;

	dev_set_drvdata(&wdev->dev, driver_data);

	__wmi_dev = wdev;

	return 0;
}

#if LINUX_VERSION_CODE < KERNEL_VERSION(5, 13, 0)
static int lwl_nb04_wmi_ab_remove(struct wmi_device *wdev)
#else
static void lwl_nb04_wmi_ab_remove(struct wmi_device *wdev)
#endif
{
	__wmi_dev = NULL;
	pr_debug("driver remove\n");

#if LINUX_VERSION_CODE < KERNEL_VERSION(5, 13, 0)
	return 0;
#endif
}

static const struct wmi_device_id lwl_nb04_wmi_ab_device_ids[] = {
	{ .guid_string = NB04_WMI_AB_GUID },
	{ }
};

static struct wmi_driver lwl_nb04_wmi_ab_driver = {
	.driver = {
		.name = "lwl_nb04_wmi_ab",
		.owner = THIS_MODULE
	},
	.id_table = lwl_nb04_wmi_ab_device_ids,
	.probe = lwl_nb04_wmi_ab_probe,
	.remove = lwl_nb04_wmi_ab_remove,
};

module_wmi_driver(lwl_nb04_wmi_ab_driver);

MODULE_AUTHOR("LWL Computers GmbH <tux@lwlcomputers.com>");
MODULE_DESCRIPTION("Driver for NB04 WMI AB methods");
MODULE_LICENSE("GPL");

MODULE_DEVICE_TABLE(wmi, lwl_nb04_wmi_ab_device_ids);
MODULE_ALIAS("wmi:" NB04_WMI_AB_GUID);
