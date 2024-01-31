/*!
 * Copyright (c) 2023 TUXEDO Computers GmbH <tux@tuxedocomputers.com>
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
#include <linux/acpi.h>
#include <linux/module.h>
#include <linux/wmi.h>
#include <linux/keyboard.h>
#include <linux/input.h>
#include <linux/input/sparse-keymap.h>
#include <linux/version.h>
#include <linux/delay.h>
#include "../tuxedo_compatibility_check/tuxedo_compatibility_check.h"

#define NB04_WMI_EVENT_GUID	"96A786FA-690C-48FB-9EB3-FA9BC3D92300"

#define NB04_WMI_EVENT_MODE_BATTERY			0x01
#define NB04_WMI_EVENT_MODE_HUMAN			0x02
#define NB04_WMI_EVENT_MODE_BEAST			0x03
#define NB04_WMI_EVENT_FULL_FAN				0x04
#define NB04_WMI_EVENT_NEXT_POWER_MODE			0x05
#define NB04_WMI_EVENT_TOUCHPAD_TOGGLE			0x06
#define NB04_WMI_EVENT_MIC_MUTE				0x07
#define NB04_WMI_EVENT_KBD_BRT_UP			0x08
#define NB04_WMI_EVENT_KBD_BRT_DOWN			0x09
#define NB04_WMI_EVENT_KBD_EFFECT_ALWAYS		0x0A
#define NB04_WMI_EVENT_KBD_EFFECT_BREATHING		0x0B
#define NB04_WMI_EVENT_KBD_EFFECT_WAVE			0x0C
#define NB04_WMI_EVENT_KBD_EFFECT_TWINKLE		0x0D
#define NB04_WMI_EVENT_KBD_EFFECT_COLOR_CYCLE		0x0E
#define NB04_WMI_EVENT_KBD_EFFECT_REACTIVE		0x0F
#define NB04_WMI_EVENT_KBD_EFFECT_RIPPLE		0x10
#define NB04_WMI_EVENT_KBD_EFFECT_SPIRAL_RAINBOW	0x11
#define NB04_WMI_EVENT_KBD_EFFECT_RAINBOW_RIPPLE	0x12

struct driver_data_t {
	struct input_dev *input_dev;
};

static struct key_entry driver_keymap[] = {
	{ KE_KEY,	NB04_WMI_EVENT_MIC_MUTE,	{ KEY_F20 } },
	{ KE_KEY,	NB04_WMI_EVENT_TOUCHPAD_TOGGLE,	{ KEY_F21 } },
	{ KE_KEY,	NB04_WMI_EVENT_KBD_BRT_DOWN,	{ KEY_KBDILLUMDOWN } },
	{ KE_KEY,	NB04_WMI_EVENT_KBD_BRT_UP,	{ KEY_KBDILLUMUP } },
	{ KE_END,	0 }
};

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
static int tuxedo_nb04_keyboard_probe(struct wmi_device *wdev)
#else
static int tuxedo_nb04_keyboard_probe(struct wmi_device *wdev, const void *dummy_context)
#endif
{
	struct driver_data_t *driver_data;
	int err;

	pr_debug("driver probe\n");

	if (!tuxedo_is_compatible())
		return -ENODEV;

	if (!wmi_has_guid(NB04_WMI_EVENT_GUID))
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
static int tuxedo_nb04_keyboard_remove(struct wmi_device *wdev)
#else
static void tuxedo_nb04_keyboard_remove(struct wmi_device *wdev)
#endif
{
	pr_debug("driver remove\n");
	struct driver_data_t *driver_data = dev_get_drvdata(&wdev->dev);
	input_unregister_device(driver_data->input_dev);

#if LINUX_VERSION_CODE < KERNEL_VERSION(5, 13, 0)
	return 0;
#endif
}

static void tuxedo_nb04_keyboard_notify(struct wmi_device *wdev, union acpi_object *obj)
{
	u8 function_number;
	u8 event_code;
	struct driver_data_t *driver_data = dev_get_drvdata(&wdev->dev);

	if (obj &&
	    obj->type == ACPI_TYPE_BUFFER &&
	    obj->buffer.length >= 2)
	{
		function_number = obj->buffer.pointer[0];
		event_code = obj->buffer.pointer[1];
		pr_debug("event value: %d (%0#4x)\n",
			 event_code, event_code);
		sparse_keymap_report_known_event(driver_data->input_dev,
						 event_code,
						 1,
						 true);
	} else {
		pr_debug("expected buffer not found\n");
	}
}

static const struct wmi_device_id tuxedo_nb04_keyboard_device_ids[] = {
	{ .guid_string = NB04_WMI_EVENT_GUID },
	{ }
};

static struct wmi_driver tuxedo_nb04_keyboard_driver = {
	.driver = {
		.name = "tuxedo_nb04_keyboard",
		.owner = THIS_MODULE
	},
	.id_table = tuxedo_nb04_keyboard_device_ids,
	.probe = tuxedo_nb04_keyboard_probe,
	.remove = tuxedo_nb04_keyboard_remove,
	.notify = tuxedo_nb04_keyboard_notify,
};

module_wmi_driver(tuxedo_nb04_keyboard_driver);

MODULE_AUTHOR("TUXEDO Computers GmbH <tux@tuxedocomputers.com>");
MODULE_DESCRIPTION("Driver for NB04 WMI (keyboard) events");
MODULE_VERSION("0.0.1");
MODULE_LICENSE("GPL");

MODULE_DEVICE_TABLE(wmi, tuxedo_nb04_keyboard_device_ids);
MODULE_ALIAS("wmi:" NB04_WMI_EVENT_GUID);
