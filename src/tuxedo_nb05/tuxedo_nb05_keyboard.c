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
#include "tuxedo_nb05_power_profiles.h"
#include "../tuxedo_compatibility_check/tuxedo_compatibility_check.h"

#define NB05_WMI_EVENT_GUID	"8FAFC061-22DA-46E2-91DB-1FE3D7E5FF3C"

#define EVENT_STATUS_COMBO(event, status) (event | (status << 8)) 

#define NB05_WMI_EVENT_TOUCHPAD_TOGGLE			0x02
#define NB05_WMI_EVENT_KBD_BRT_MAX			0x07
#define NB05_WMI_EVENT_KBD_BRT_MIDDLE			0x08
#define NB05_WMI_EVENT_KBD_BRT_OFF			0x09
#define NB05_WMI_EVENT_MODE_POWER_SAVE			0x11
#define NB05_WMI_EVENT_MODE_BALANCE			0x12
#define NB05_WMI_EVENT_MODE_HIGH_PERFORMANCE		0x13
#define NB05_WMI_EVENT_CAMERA_TOGGLE			0x30
#define NB05_WMI_EVENT_FNLOCK_TOGGLE			0x31

#define NB05_WMI_EVENT_TOUCHPAD_ON	EVENT_STATUS_COMBO(NB05_WMI_EVENT_TOUCHPAD_TOGGLE, 0x11)
#define NB05_WMI_EVENT_TOUCHPAD_OFF	EVENT_STATUS_COMBO(NB05_WMI_EVENT_TOUCHPAD_TOGGLE, 0x22)

#define NB05_WMI_EVENT_CAMERA_ON	EVENT_STATUS_COMBO(NB05_WMI_EVENT_CAMERA_TOGGLE, 0x11)
#define NB05_WMI_EVENT_CAMERA_OFF	EVENT_STATUS_COMBO(NB05_WMI_EVENT_CAMERA_TOGGLE, 0x22)

#define NB05_WMI_EVENT_FNLOCK_ON	EVENT_STATUS_COMBO(NB05_WMI_EVENT_FNLOCK_TOGGLE, 0x11)
#define NB05_WMI_EVENT_FNLOCK_OFF	EVENT_STATUS_COMBO(NB05_WMI_EVENT_FNLOCK_TOGGLE, 0x22)

struct driver_data_t {
	struct input_dev *input_dev;
};

static struct key_entry driver_keymap[] = {
	{ KE_KEY,	NB05_WMI_EVENT_TOUCHPAD_ON,	{ KEY_F22 } },
	{ KE_KEY,	NB05_WMI_EVENT_TOUCHPAD_OFF,	{ KEY_F23 } },
	// Only used to put ev bits
	{ KE_KEY,	0xffff,				{ KEY_F6 } },
	{ KE_KEY,	0xffff,				{ KEY_LEFTALT } },
	{ KE_KEY,	0xffff,				{ KEY_LEFTMETA } },
	{ KE_END,	0 }
};

static void report_gauge_key_combo(struct input_dev *idev)
{
	// Special key combination when mode change key is pressed
	input_report_key(idev, KEY_LEFTMETA, 1);
	input_report_key(idev, KEY_LEFTALT, 1);
	input_report_key(idev, KEY_F6, 1);
	input_sync(idev);
	input_report_key(idev, KEY_F6, 0);
	input_report_key(idev, KEY_LEFTALT, 0);
	input_report_key(idev, KEY_LEFTMETA, 0);
	input_sync(idev);
}

/**
 * Basically a copy of the existing report event but doesn't report unknown events
 */
static bool sparse_keymap_report_known_event(struct input_dev *dev,
					     unsigned int code,
					     unsigned int value,
					     bool autorelease)
{
	const struct key_entry *ke =
		sparse_keymap_entry_from_scancode(dev, code);

	if (ke) {
		sparse_keymap_report_entry(dev, ke, value, autorelease);
		return true;
	}

	return false;
}

static int input_device_init(struct input_dev **input_dev_pp, const struct key_entry key_map[])
{
	struct input_dev *input_dev;
	int err;

	input_dev = input_allocate_device();
	if (unlikely(!input_dev)) {
		pr_err("Error allocating input device\n");
		return -ENOMEM;
	}

	input_dev->name = "TUXEDO Keyboard Events";
	input_dev->phys = "tuxedo-keyboard" "/input0";
	input_dev->id.bustype = BUS_HOST;
	input_dev->dev.parent = NULL;

	if (key_map != NULL) {
		err = sparse_keymap_setup(input_dev, key_map, NULL);
		if (err) {
			pr_err("Failed to setup sparse keymap\n");
			goto err_free_input_device;
		}
	}

	err = input_register_device(input_dev);
	if (unlikely(err)) {
		pr_err("Error registering input device\n");
		goto err_free_input_device;
	}

	*input_dev_pp = input_dev;

	return 0;

err_free_input_device:
	input_free_device(input_dev);

	return err;
}

#if LINUX_VERSION_CODE < KERNEL_VERSION(5, 3, 0)
static int tuxedo_nb05_keyboard_probe(struct wmi_device *wdev)
#else
static int tuxedo_nb05_keyboard_probe(struct wmi_device *wdev, const void *dummy_context)
#endif
{
	struct driver_data_t *driver_data;
	int err;

	pr_debug("driver probe\n");

	if (!tuxedo_is_compatible())
		return -ENODEV;

	driver_data = devm_kzalloc(&wdev->dev, sizeof(*driver_data), GFP_KERNEL);
	if (!driver_data)
		return -ENOMEM;

	dev_set_drvdata(&wdev->dev, driver_data);

	err = input_device_init(&driver_data->input_dev, driver_keymap);
	if (err)
		return err;

	return 0;
}

#if LINUX_VERSION_CODE < KERNEL_VERSION(5, 13, 0)
static int tuxedo_nb05_keyboard_remove(struct wmi_device *wdev)
#else
static void tuxedo_nb05_keyboard_remove(struct wmi_device *wdev)
#endif
{
	pr_debug("driver remove\n");
	struct driver_data_t *driver_data = dev_get_drvdata(&wdev->dev);
	input_unregister_device(driver_data->input_dev);

#if LINUX_VERSION_CODE < KERNEL_VERSION(5, 13, 0)
	return 0;
#endif
}

static void tuxedo_nb05_keyboard_notify(struct wmi_device *wdev, union acpi_object *obj)
{
	u8 function_number;
	u8 event_code;
	u16 device_status;
	struct driver_data_t *driver_data = dev_get_drvdata(&wdev->dev);

	if (obj &&
	    obj->type == ACPI_TYPE_BUFFER &&
	    obj->buffer.length >= 4)
	{
		function_number = obj->buffer.pointer[0];
		event_code = obj->buffer.pointer[1];
		device_status = obj->buffer.pointer[2] |
				(obj->buffer.pointer[3] << 8);
		pr_debug("event value: %d (%0#4x), device status %d (%0#6x)\n",
			 event_code, event_code, device_status, device_status);

		switch (event_code) {
		case NB05_WMI_EVENT_MODE_POWER_SAVE:
		case NB05_WMI_EVENT_MODE_BALANCE:
		case NB05_WMI_EVENT_MODE_HIGH_PERFORMANCE:
			report_gauge_key_combo(driver_data->input_dev);
			rewrite_last_profile();
			break;
		default:
			break;
		}

		// Report event_code
		sparse_keymap_report_known_event(driver_data->input_dev,
						 event_code,
						 1,
						 true);
		// Report combined event code and device status
		sparse_keymap_report_known_event(driver_data->input_dev,
						 EVENT_STATUS_COMBO(event_code, device_status),
						 1,
						 true);
	} else {
		pr_debug("expected buffer not found\n");
	}
}

static const struct wmi_device_id tuxedo_nb05_keyboard_device_ids[] = {
	{ .guid_string = NB05_WMI_EVENT_GUID },
	{ }
};

static struct wmi_driver tuxedo_nb05_keyboard_driver = {
	.driver = {
		.name = "tuxedo_nb05_keyboard",
		.owner = THIS_MODULE
	},
	.id_table = tuxedo_nb05_keyboard_device_ids,
	.probe = tuxedo_nb05_keyboard_probe,
	.remove = tuxedo_nb05_keyboard_remove,
	.notify = tuxedo_nb05_keyboard_notify,
};

module_wmi_driver(tuxedo_nb05_keyboard_driver);

MODULE_AUTHOR("TUXEDO Computers GmbH <tux@tuxedocomputers.com>");
MODULE_DESCRIPTION("Driver for NB05 keyboard events");
MODULE_VERSION("0.0.1");
MODULE_LICENSE("GPL");

MODULE_DEVICE_TABLE(wmi, tuxedo_nb05_keyboard_device_ids);
MODULE_ALIAS("wmi:" NB05_WMI_EVENT_GUID);
