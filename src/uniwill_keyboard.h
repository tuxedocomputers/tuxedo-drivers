/* SPDX-License-Identifier: GPL-2.0+ */
/*!
 * Copyright (c) 2020-2024 TUXEDO Computers GmbH <tux@tuxedocomputers.com>
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

#ifndef UNIWILL_KEYBOARD_H
#define UNIWILL_KEYBOARD_H

#include "tuxedo_keyboard_common.h"
#include <linux/acpi.h>
#include <linux/wmi.h>
#include <linux/workqueue.h>
#include <linux/keyboard.h>
#include <linux/timer.h>
#include <linux/delay.h>
#include <linux/leds.h>
#include <linux/led-class-multicolor.h>
#include <linux/string.h>
#include <linux/version.h>
#include <linux/i8042.h>
#include <linux/serio.h>
#include "uniwill_interfaces.h"
#include "uniwill_leds.h"

#define UNIWILL_OSD_RADIOON			0x01A
#define UNIWILL_OSD_RADIOOFF			0x01B
#define UNIWILL_OSD_KB_LED_LEVEL0		0x03B
#define UNIWILL_OSD_KB_LED_LEVEL1		0x03C
#define UNIWILL_OSD_KB_LED_LEVEL2		0x03D
#define UNIWILL_OSD_KB_LED_LEVEL3		0x03E
#define UNIWILL_OSD_KB_LED_LEVEL4		0x03F
#define UNIWILL_OSD_DC_ADAPTER_CHANGE		0x0AB
#define UNIWILL_OSD_MODE_CHANGE_KEY_EVENT	0x0B0

#define UNIWILL_KEY_RFKILL			0x0A4
#define UNIWILL_KEY_KBDILLUMDOWN		0x0B1
#define UNIWILL_KEY_KBDILLUMUP			0x0B2
#define UNIWILL_KEY_FN_LOCK			0x0B8
#define UNIWILL_KEY_KBDILLUMTOGGLE		0x0B9

#define UNIWILL_OSD_TOUCHPADWORKAROUND		0xFFF

#define UNIWILL_FN_LOCK_MASK			0x10

static void uw_charging_priority_write_state(void);
static void uw_charging_profile_write_state(void);

struct tuxedo_keyboard_driver uniwill_keyboard_driver;

struct uniwill_device_features_t uniwill_device_features;

static bool uw_feats_loaded = false;

static u8 uniwill_kbd_bl_enable_state_on_start = 0xff;

static struct key_entry uniwill_wmi_keymap[] = {
	// { KE_KEY,	UNIWILL_OSD_RADIOON,		{ KEY_RFKILL } },
	// { KE_KEY,	UNIWILL_OSD_RADIOOFF,		{ KEY_RFKILL } },
	// { KE_KEY,	0xb0,				{ KEY_F13 } },
	// Manual mode rfkill
	{ KE_KEY,	UNIWILL_KEY_RFKILL,		{ KEY_RFKILL }},
	{ KE_KEY,	UNIWILL_OSD_TOUCHPADWORKAROUND,	{ KEY_F21 } },
	// Keyboard brightness
	{ KE_KEY,	UNIWILL_KEY_KBDILLUMDOWN,	{ KEY_KBDILLUMDOWN } },
	{ KE_KEY,	UNIWILL_KEY_KBDILLUMUP,		{ KEY_KBDILLUMUP } },
	{ KE_KEY,	UNIWILL_KEY_KBDILLUMTOGGLE,	{ KEY_KBDILLUMTOGGLE } },
	{ KE_KEY,	UNIWILL_OSD_KB_LED_LEVEL0,	{ KEY_KBDILLUMTOGGLE } },
	{ KE_KEY,	UNIWILL_OSD_KB_LED_LEVEL1,	{ KEY_KBDILLUMTOGGLE } },
	{ KE_KEY,	UNIWILL_OSD_KB_LED_LEVEL2,	{ KEY_KBDILLUMTOGGLE } },
	{ KE_KEY,	UNIWILL_OSD_KB_LED_LEVEL3,	{ KEY_KBDILLUMTOGGLE } },
	{ KE_KEY,	UNIWILL_OSD_KB_LED_LEVEL4,	{ KEY_KBDILLUMTOGGLE } },
	// Send FN_ESC to user space as input-event-codes.h does not define Fn-Lock
	{ KE_KEY,	UNIWILL_KEY_FN_LOCK,		{ KEY_FN_ESC } },
	// Only used to put ev bits
	{ KE_KEY,	0xffff,				{ KEY_F6 } },
	{ KE_KEY,	0xffff,				{ KEY_LEFTALT } },
	{ KE_KEY,	0xffff,				{ KEY_LEFTMETA } },
	{ KE_END,	0 }
};

static struct uniwill_interfaces_t {
	struct uniwill_interface_t *wmi;
} uniwill_interfaces = { .wmi = NULL };

uniwill_event_callb_t uniwill_event_callb;

int uniwill_read_ec_ram(u16 address, u8 *data)
{
	int status;

	if (!IS_ERR_OR_NULL(uniwill_interfaces.wmi))
		status = uniwill_interfaces.wmi->read_ec_ram(address, data);
	else {
		pr_err("no active interface while read addr 0x%04x\n", address);
		status = -EIO;
	}

	return status;
}
EXPORT_SYMBOL(uniwill_read_ec_ram);

int uniwill_read_ec_ram_with_retry(u16 address, u8 *data, int retries)
{
	int status, i;

	for (i = 0; i < retries; ++i) {
		status = uniwill_read_ec_ram(address, data);
		if (status != 0)
			pr_debug("uniwill_read_ec_ram(...) failed.\n");
		else
			break;
	}

	return status;
}
EXPORT_SYMBOL(uniwill_read_ec_ram_with_retry);

int uniwill_write_ec_ram(u16 address, u8 data)
{
	int status;

	if (!IS_ERR_OR_NULL(uniwill_interfaces.wmi))
		status = uniwill_interfaces.wmi->write_ec_ram(address, data);
	else {
		pr_err("no active interface while write addr 0x%04x data 0x%02x\n", address, data);
		status = -EIO;
	}

	return status;
}
EXPORT_SYMBOL(uniwill_write_ec_ram);

int uniwill_write_ec_ram_with_retry(u16 address, u8 data, int retries)
{
	int status, i;
	u8 control_data;

	for (i = 0; i < retries; ++i) {
		status = uniwill_write_ec_ram(address, data);
		if (status != 0) {
			msleep(50);
			continue;
		}
		else {
			status = uniwill_read_ec_ram(address, &control_data);
			if (status != 0 || data != control_data) {
				msleep(50);
				continue;
			}
			break;
		}
	}

	return status;
}
EXPORT_SYMBOL(uniwill_write_ec_ram_with_retry);

static DEFINE_MUTEX(uniwill_interface_modification_lock);

int uniwill_add_interface(struct uniwill_interface_t *interface)
{
	mutex_lock(&uniwill_interface_modification_lock);

	if (strcmp(interface->string_id, UNIWILL_INTERFACE_WMI_STRID) == 0)
		uniwill_interfaces.wmi = interface;
	else {
		TUXEDO_DEBUG("trying to add unknown interface\n");
		mutex_unlock(&uniwill_interface_modification_lock);
		return -EINVAL;
	}
	interface->event_callb = uniwill_event_callb;

	mutex_unlock(&uniwill_interface_modification_lock);

	// Initialize driver if not already present
	tuxedo_keyboard_init_driver(&uniwill_keyboard_driver);

	return 0;
}
EXPORT_SYMBOL(uniwill_add_interface);

int uniwill_remove_interface(struct uniwill_interface_t *interface)
{
	mutex_lock(&uniwill_interface_modification_lock);

	if (strcmp(interface->string_id, UNIWILL_INTERFACE_WMI_STRID) == 0) {
		// Remove driver if last interface is removed
		tuxedo_keyboard_remove_driver(&uniwill_keyboard_driver);

		uniwill_interfaces.wmi = NULL;
	} else {
		mutex_unlock(&uniwill_interface_modification_lock);
		return -EINVAL;
	}

	mutex_unlock(&uniwill_interface_modification_lock);

	return 0;
}
EXPORT_SYMBOL(uniwill_remove_interface);

int uniwill_get_active_interface_id(char **id_str)
{
	if (IS_ERR_OR_NULL(uniwill_interfaces.wmi))
		return -ENODEV;

	if (!IS_ERR_OR_NULL(id_str))
		*id_str = uniwill_interfaces.wmi->string_id;

	return 0;
}
EXPORT_SYMBOL(uniwill_get_active_interface_id);

static void key_event_work(struct work_struct *work)
{
	// Delay sometimes needed to make userspace reliably separate
	// the touchpadtoggle key events from the custom key events
	// coming from firmware
	msleep(50);
	sparse_keymap_report_known_event(
		uniwill_keyboard_driver.input_device,
		UNIWILL_OSD_TOUCHPADWORKAROUND,
		1,
		true
	);
}
static DECLARE_WORK(uniwill_key_event_work, key_event_work);

static void uniwill_write_kbd_bl_enable(u8 enable)
{
	u8 backlight_data;
	enable = enable & 0x01;

	uniwill_read_ec_ram(UW_EC_REG_KBD_BL_STATUS, &backlight_data);
	backlight_data = backlight_data & ~(1 << 1);
	backlight_data |= (!enable << 1);
	uniwill_write_ec_ram(UW_EC_REG_KBD_BL_STATUS, backlight_data);
}

void uniwill_event_callb(u32 code)
{
	switch (code) {
		case UNIWILL_OSD_MODE_CHANGE_KEY_EVENT:
			// Special key combination when mode change key is pressed (the one next to
			// the power key). Opens TCC by default when installed.
			input_report_key(uniwill_keyboard_driver.input_device, KEY_LEFTMETA, 1);
			input_report_key(uniwill_keyboard_driver.input_device, KEY_LEFTALT, 1);
			input_report_key(uniwill_keyboard_driver.input_device, KEY_F6, 1);
			input_sync(uniwill_keyboard_driver.input_device);
			input_report_key(uniwill_keyboard_driver.input_device, KEY_F6, 0);
			input_report_key(uniwill_keyboard_driver.input_device, KEY_LEFTALT, 0);
			input_report_key(uniwill_keyboard_driver.input_device, KEY_LEFTMETA, 0);
			input_sync(uniwill_keyboard_driver.input_device);
			break;
		case UNIWILL_OSD_DC_ADAPTER_CHANGE:
			// Refresh keyboard state and charging prio on cable switch event
			uniwill_leds_restore_state_extern();
			msleep(50);
			uw_charging_priority_write_state();
			break;
		case UNIWILL_KEY_KBDILLUMTOGGLE:
		case UNIWILL_OSD_KB_LED_LEVEL0:
		case UNIWILL_OSD_KB_LED_LEVEL1:
		case UNIWILL_OSD_KB_LED_LEVEL2:
		case UNIWILL_OSD_KB_LED_LEVEL3:
		case UNIWILL_OSD_KB_LED_LEVEL4:
			// Notify userspace/UPower that the firmware changed the keyboard backlight
			// brightness on white only keyboards. Fallthrough on other keyboards to
			// emit KEY_KBDILLUMTOGGLE.
			if (uniwill_leds_notify_brightness_change_extern())
				return;
			fallthrough;
		default:
			if (uniwill_keyboard_driver.input_device != NULL)
				if (!sparse_keymap_report_known_event(uniwill_keyboard_driver.input_device, code, 1, true))
					TUXEDO_DEBUG("Unknown code - %d (%0#6x)\n", code, code);
	}
}

#define UNIWILL_LIGHTBAR_LED_MAX_BRIGHTNESS	0x24
#define UNIWILL_LIGHTBAR_LED_NAME_RGB_RED	"lightbar_rgb:1:status"
#define UNIWILL_LIGHTBAR_LED_NAME_RGB_GREEN	"lightbar_rgb:2:status"
#define UNIWILL_LIGHTBAR_LED_NAME_RGB_BLUE	"lightbar_rgb:3:status"
#define UNIWILL_LIGHTBAR_LED_NAME_ANIMATION	"lightbar_animation::status"

static void uniwill_write_lightbar_rgb(u8 red, u8 green, u8 blue)
{
	if (red <= UNIWILL_LIGHTBAR_LED_MAX_BRIGHTNESS) {
		uniwill_write_ec_ram(0x0749, red);
	}
	if (green <= UNIWILL_LIGHTBAR_LED_MAX_BRIGHTNESS) {
		uniwill_write_ec_ram(0x074a, green);
	}
	if (blue <= UNIWILL_LIGHTBAR_LED_MAX_BRIGHTNESS) {
		uniwill_write_ec_ram(0x074b, blue);
	}
}

static void uniwill_read_lightbar_rgb(u8 *red, u8 *green, u8 *blue)
{
	uniwill_read_ec_ram(0x0749, red);
	uniwill_read_ec_ram(0x074a, green);
	uniwill_read_ec_ram(0x074b, blue);
}

static void uniwill_write_lightbar_animation(bool animation_status)
{
	u8 value;

	uniwill_read_ec_ram(0x0748, &value);
	if (animation_status) {
		value |= 0x80;
	} else {
		value &= ~0x80;
	}
	uniwill_write_ec_ram(0x0748, value);
}

static void uniwill_read_lightbar_animation(bool *animation_status)
{
	u8 lightbar_animation_data;
	uniwill_read_ec_ram(0x0748, &lightbar_animation_data);
	*animation_status = (lightbar_animation_data & 0x80) > 0;
}

static int lightbar_set_blocking(struct led_classdev *led_cdev, enum led_brightness brightness)
{
	u8 red = 0xff, green = 0xff, blue = 0xff;
	bool led_red = strstr(led_cdev->name, UNIWILL_LIGHTBAR_LED_NAME_RGB_RED) != NULL;
	bool led_green = strstr(led_cdev->name, UNIWILL_LIGHTBAR_LED_NAME_RGB_GREEN) != NULL;
	bool led_blue = strstr(led_cdev->name, UNIWILL_LIGHTBAR_LED_NAME_RGB_BLUE) != NULL;
	bool led_animation = strstr(led_cdev->name, UNIWILL_LIGHTBAR_LED_NAME_ANIMATION) != NULL;

	if (led_red || led_green || led_blue) {
		if (led_red) {
			red = brightness;
		} else if (led_green) {
			green = brightness;
		} else if (led_blue) {
			blue = brightness;
		}
		uniwill_write_lightbar_rgb(red, green, blue);
		// Also make sure the animation is off
		uniwill_write_lightbar_animation(false);
	} else if (led_animation) {
		if (brightness == 1) {
			uniwill_write_lightbar_animation(true);
		} else {
			uniwill_write_lightbar_animation(false);
		}
	}
	return 0;
}

static enum led_brightness lightbar_get(struct led_classdev *led_cdev)
{
	u8 red, green, blue;
	bool animation_status;
	bool led_red = strstr(led_cdev->name, UNIWILL_LIGHTBAR_LED_NAME_RGB_RED) != NULL;
	bool led_green = strstr(led_cdev->name, UNIWILL_LIGHTBAR_LED_NAME_RGB_GREEN) != NULL;
	bool led_blue = strstr(led_cdev->name, UNIWILL_LIGHTBAR_LED_NAME_RGB_BLUE) != NULL;
	bool led_animation = strstr(led_cdev->name, UNIWILL_LIGHTBAR_LED_NAME_ANIMATION) != NULL;

	if (led_red || led_green || led_blue) {
		uniwill_read_lightbar_rgb(&red, &green, &blue);
		if (led_red) {
			return red;
		} else if (led_green) {
			return green;
		} else if (led_blue) {
			return blue;
		}
	} else if (led_animation) {
		uniwill_read_lightbar_animation(&animation_status);
		return animation_status ? 1 : 0;
	}

	return 0;
}

static bool uw_lightbar_loaded;
static struct led_classdev lightbar_led_classdevs[] = {
	{
		.name = UNIWILL_LIGHTBAR_LED_NAME_RGB_RED,
		.max_brightness = UNIWILL_LIGHTBAR_LED_MAX_BRIGHTNESS,
		.brightness_set_blocking = &lightbar_set_blocking,
		.brightness_get = &lightbar_get
	},
	{
		.name = UNIWILL_LIGHTBAR_LED_NAME_RGB_GREEN,
		.max_brightness = UNIWILL_LIGHTBAR_LED_MAX_BRIGHTNESS,
		.brightness_set_blocking = &lightbar_set_blocking,
		.brightness_get = &lightbar_get
	},
	{
		.name = UNIWILL_LIGHTBAR_LED_NAME_RGB_BLUE,
		.max_brightness = UNIWILL_LIGHTBAR_LED_MAX_BRIGHTNESS,
		.brightness_set_blocking = &lightbar_set_blocking,
		.brightness_get = &lightbar_get
	},
	{
		.name = UNIWILL_LIGHTBAR_LED_NAME_ANIMATION,
		.max_brightness = 1,
		.brightness_set_blocking = &lightbar_set_blocking,
		.brightness_get = &lightbar_get
	}
};

static int uw_lightbar_init(struct platform_device *dev)
{
	int i, j, status;

	bool lightbar_supported = false
		|| dmi_match(DMI_BOARD_NAME, "LAPQC71A")
		|| dmi_match(DMI_BOARD_NAME, "LAPQC71B")
		|| dmi_match(DMI_BOARD_NAME, "TRINITY1501I")
		|| dmi_match(DMI_BOARD_NAME, "TRINITY1701I")
		|| dmi_match(DMI_PRODUCT_NAME, "A60 MUV")
#if LINUX_VERSION_CODE >= KERNEL_VERSION(4, 18, 0)
		|| dmi_match(DMI_PRODUCT_SKU, "STELLARIS1XI03")
		|| dmi_match(DMI_PRODUCT_SKU, "STELLARIS1XA03")
		|| dmi_match(DMI_PRODUCT_SKU, "STELLARIS1XI04")
		|| dmi_match(DMI_PRODUCT_SKU, "STEPOL1XA04")

#endif
		;

#if LINUX_VERSION_CODE < KERNEL_VERSION(4, 18, 0)
	TUXEDO_ERROR(
		"Warning: Kernel version less that 4.18, lightbar might not be properly recognized.");
#endif

	if (!lightbar_supported)
		return -ENODEV;

	for (i = 0; i < ARRAY_SIZE(lightbar_led_classdevs); ++i) {
		status = led_classdev_register(&dev->dev, &lightbar_led_classdevs[i]);
		if (status < 0) {
			for (j = 0; j < i; j++)
				led_classdev_unregister(&lightbar_led_classdevs[j]);
			return status;
		}
	}

	// Init default state
	uniwill_write_lightbar_animation(false);
	uniwill_write_lightbar_rgb(0, 0, 0);

	return 0;
}

static int uw_lightbar_remove(struct platform_device *dev)
{
	int i;
	for (i = 0; i < ARRAY_SIZE(lightbar_led_classdevs); ++i) {
		led_classdev_unregister(&lightbar_led_classdevs[i]);
	}
	return 0;
}

static bool uw_charging_prio_loaded = false;
static bool uw_charging_prio_last_written_value;

static ssize_t uw_charging_prios_available_show(struct device *child,
						struct device_attribute *attr,
						char *buffer);
static ssize_t uw_charging_prio_show(struct device *child,
				     struct device_attribute *attr, char *buffer);
static ssize_t uw_charging_prio_store(struct device *child,
				      struct device_attribute *attr,
				      const char *buffer, size_t size);

struct uw_charging_prio_attrs_t {
	struct device_attribute charging_prios_available;
	struct device_attribute charging_prio;
} uw_charging_prio_attrs = {
	.charging_prios_available = __ATTR(charging_prios_available, 0444, uw_charging_prios_available_show, NULL),
	.charging_prio = __ATTR(charging_prio, 0644, uw_charging_prio_show, uw_charging_prio_store)
};

static struct attribute *uw_charging_prio_attrs_list[] = {
	&uw_charging_prio_attrs.charging_prios_available.attr,
	&uw_charging_prio_attrs.charging_prio.attr,
	NULL
};

static struct attribute_group uw_charging_prio_attr_group = {
	.name = "charging_priority",
	.attrs = uw_charging_prio_attrs_list
};

/*
 * charging_prio values
 *     0 => charging priority
 *     1 => performance priority
 */
static int uw_set_charging_priority(u8 charging_priority)
{
	u8 previous_data, next_data;
	int result;

	charging_priority = (charging_priority & 0x01) << 7;

	result = uniwill_read_ec_ram(0x07cc, &previous_data);
	if (result != 0)
		return result;

	next_data = (previous_data & ~(1 << 7)) | charging_priority;
	result = uniwill_write_ec_ram(0x07cc, next_data);
	if (result == 0)
		uw_charging_prio_last_written_value = charging_priority;

	return result;
}

static int uw_get_charging_priority(u8 *charging_priority)
{
	int result = uniwill_read_ec_ram(0x07cc, charging_priority);
	*charging_priority = (*charging_priority >> 7) & 0x01;
	return result;
}

static int uw_has_charging_priority(bool *status)
{
	u8 data;
	int result;

	bool not_supported_device = false
		|| dmi_match(DMI_BOARD_NAME, "PF5PU1G")
		|| dmi_match(DMI_BOARD_NAME, "LAPQC71A")
		|| dmi_match(DMI_BOARD_NAME, "LAPQC71B")
		|| dmi_match(DMI_PRODUCT_NAME, "A60 MUV")
		|| dmi_match(DMI_PRODUCT_SKU, "STELLARIS16I07")
		// TODO: Add Stellaris Gen6 and IBP Gen9 as well (or find cleaner solution)
		;

	if (not_supported_device) {
		*status = false;
		return 0;
	}

	result = uniwill_read_ec_ram(0x0742, &data);
	if (result != 0)
		return -EIO;

	if (data & (1 << 5))
		*status = true;
	else
		*status = false;

	return 0;
}

static void uw_charging_priority_write_state(void)
{
	if (uw_charging_prio_loaded)
		uw_set_charging_priority(uw_charging_prio_last_written_value);
}

static void uw_charging_priority_init(struct platform_device *dev)
{
	u8 value;
	struct uniwill_device_features_t *uw_feats = &uniwill_device_features;

	if (uw_feats->uniwill_has_charging_prio)
		uw_charging_prio_loaded = sysfs_create_group(&dev->dev.kobj, &uw_charging_prio_attr_group) == 0;

	// Read for state init
	if (uw_charging_prio_loaded) {
		uw_get_charging_priority(&value);
		uw_charging_prio_last_written_value = value;
	}
}

static bool uw_charging_profile_loaded = false;
static bool uw_charging_profile_last_written_value;

static ssize_t uw_charging_profiles_available_show(struct device *child,
						   struct device_attribute *attr,
						   char *buffer);
static ssize_t uw_charging_profile_show(struct device *child,
					struct device_attribute *attr, char *buffer);
static ssize_t uw_charging_profile_store(struct device *child,
					 struct device_attribute *attr,
					 const char *buffer, size_t size);

struct uw_charging_profile_attrs_t {
	struct device_attribute charging_profiles_available;
	struct device_attribute charging_profile;
} uw_charging_profile_attrs = {
	.charging_profiles_available = __ATTR(charging_profiles_available, 0444, uw_charging_profiles_available_show, NULL),
	.charging_profile = __ATTR(charging_profile, 0644, uw_charging_profile_show, uw_charging_profile_store)
};

static struct attribute *uw_charging_profile_attrs_list[] = {
	&uw_charging_profile_attrs.charging_profiles_available.attr,
	&uw_charging_profile_attrs.charging_profile.attr,
	NULL
};

static struct attribute_group uw_charging_profile_attr_group = {
	.name = "charging_profile",
	.attrs = uw_charging_profile_attrs_list
};

/*
 * charging_profile values
 *     0 => high capacity
 *     1 => balanced
 *     2 => stationary
 */
static int uw_set_charging_profile(u8 charging_profile)
{
	u8 previous_data, next_data;
	int result;

	charging_profile = (charging_profile & 0x03) << 4;

	result = uniwill_read_ec_ram(0x07a6, &previous_data);
	if (result != 0)
		return result;

	next_data = (previous_data & ~(0x03 << 4)) | charging_profile;
	result = uniwill_write_ec_ram(0x07a6, next_data);

	if (result == 0)
		uw_charging_profile_last_written_value = charging_profile;

	return result;
}

static int uw_get_charging_profile(u8 *charging_profile)
{
	int result = uniwill_read_ec_ram(0x07a6, charging_profile);
	if (result == 0)
		*charging_profile = (*charging_profile >> 4) & 0x03;
	return result;
}

static int uw_has_charging_profile(bool *status)
{
	u8 data;
	int result;

	bool not_supported_device = false
		|| dmi_match(DMI_BOARD_NAME, "PF5PU1G")
		|| dmi_match(DMI_BOARD_NAME, "LAPQC71A")
		|| dmi_match(DMI_BOARD_NAME, "LAPQC71B")
		|| dmi_match(DMI_PRODUCT_NAME, "A60 MUV")
	;

	if (not_supported_device) {
		*status = false;
		return 0;
	}

	result = uniwill_read_ec_ram(0x078e, &data);
	if (result != 0)
		return -EIO;

	if (data & (1 << 3))
		*status = true;
	else
		*status = false;

	return 0;
}

static void __attribute__ ((unused)) uw_charging_profile_write_state(void)
{
	if (uw_charging_profile_loaded)
		uw_set_charging_profile(uw_charging_profile_last_written_value);
}

static void uw_charging_profile_init(struct platform_device *dev)
{
	u8 value;
	struct uniwill_device_features_t *uw_feats = &uniwill_device_features;

	if (uw_feats->uniwill_has_charging_profile)
		uw_charging_profile_loaded = sysfs_create_group(&dev->dev.kobj, &uw_charging_profile_attr_group) == 0;

	// Read for state init
	if (uw_charging_profile_loaded) {
		uw_get_charging_profile(&value);
		uw_charging_profile_last_written_value = value;
	}
}

struct char_to_u8_t {
	char* descriptor;
	u8 value;
};

static struct char_to_u8_t charging_profile_options[] = {
	{ .descriptor = "high_capacity", .value = 0x00 },
	{ .descriptor = "balanced",	 .value = 0x01 },
	{ .descriptor = "stationary",	 .value = 0x02 }
};

static ssize_t uw_charging_profiles_available_show(struct device *child,
						   struct device_attribute *attr,
						   char *buffer)
{
	int i, n;
	n = ARRAY_SIZE(charging_profile_options);
	for (i = 0; i < n; ++i) {
		sprintf(buffer + strlen(buffer), "%s",
			charging_profile_options[i].descriptor);
		if (i < n - 1)
			sprintf(buffer + strlen(buffer), " ");
		else
			sprintf(buffer + strlen(buffer), "\n");
	}

	return strlen(buffer);
}

static ssize_t uw_charging_profile_show(struct device *child,
					struct device_attribute *attr, char *buffer)
{
	u8 charging_profile_value;
	int i, result;

	result = uw_get_charging_profile(&charging_profile_value);
	if (result != 0)
		return result;

	for (i = 0; i < ARRAY_SIZE(charging_profile_options); ++i)
		if (charging_profile_options[i].value == charging_profile_value) {
			sprintf(buffer, "%s\n", charging_profile_options[i].descriptor);
			return strlen(buffer);
		}

	pr_err("Read charging profile value not matched to a descriptor\n");

	return -EIO;
}

static ssize_t uw_charging_profile_store(struct device *child,
					 struct device_attribute *attr,
					 const char *buffer, size_t size)
{
	u8 charging_profile_value;
	int i, result;
	char *buffer_copy;
	char *charging_profile_descriptor;
	buffer_copy = kmalloc(size + 1, GFP_KERNEL);
	strcpy(buffer_copy, buffer);
	charging_profile_descriptor = strstrip(buffer_copy);

	for (i = 0; i < ARRAY_SIZE(charging_profile_options); ++i)
		if (strcmp(charging_profile_options[i].descriptor, charging_profile_descriptor) == 0) {
			charging_profile_value = charging_profile_options[i].value;
			break;
		}

	kfree(buffer_copy);

	if (i < ARRAY_SIZE(charging_profile_options)) {
		// Option found try to set
		result = uw_set_charging_profile(charging_profile_value);
		if (result == 0)
			return size;
		else
			return -EIO;
	} else
		// Invalid input, not matched to an option
		return -EINVAL;
}

static struct char_to_u8_t charging_prio_options[] = {
	{ .descriptor = "charge_battery", .value = 0x00 },
	{ .descriptor = "performance",    .value = 0x01 }
};

static ssize_t uw_charging_prios_available_show(struct device *child,
						struct device_attribute *attr,
						char *buffer)
{
	int i, n;
	n = ARRAY_SIZE(charging_prio_options);
	for (i = 0; i < n; ++i) {
		sprintf(buffer + strlen(buffer), "%s",
			charging_prio_options[i].descriptor);
		if (i < n - 1)
			sprintf(buffer + strlen(buffer), " ");
		else
			sprintf(buffer + strlen(buffer), "\n");
	}

	return strlen(buffer);
}

static ssize_t uw_charging_prio_show(struct device *child,
				     struct device_attribute *attr, char *buffer)
{
	u8 charging_prio_value;
	int i, result;

	result = uw_get_charging_priority(&charging_prio_value);
	if (result != 0)
		return result;

	for (i = 0; i < ARRAY_SIZE(charging_prio_options); ++i)
		if (charging_prio_options[i].value == charging_prio_value) {
			sprintf(buffer, "%s\n", charging_prio_options[i].descriptor);
			return strlen(buffer);
		}

	pr_err("Read charging prio value not matched to a descriptor\n");

	return -EIO;
}

static ssize_t uw_charging_prio_store(struct device *child,
				      struct device_attribute *attr,
				      const char *buffer, size_t size)
{
	u8 charging_prio_value;
	int i, result;
	char *buffer_copy;
	char *charging_prio_descriptor;
	buffer_copy = kmalloc(size + 1, GFP_KERNEL);
	strcpy(buffer_copy, buffer);
	charging_prio_descriptor = strstrip(buffer_copy);

	for (i = 0; i < ARRAY_SIZE(charging_prio_options); ++i)
		if (strcmp(charging_prio_options[i].descriptor, charging_prio_descriptor) == 0) {
			charging_prio_value = charging_prio_options[i].value;
			break;
		}

	kfree(buffer_copy);

	if (i < ARRAY_SIZE(charging_prio_options)) {
		// Option found try to set
		result = uw_set_charging_priority(charging_prio_value);
		if (result == 0)
			return size;
		else
			return -EIO;
	} else
		// Invalid input, not matched to an option
		return -EINVAL;
}

/*
 * We didn't find any identification bits to retrieve the information if the
 * device supports the features usb_powershare and ac_auto_boot. The nb02
 * control center reads the support flag either from the registry or from the
 * UniWillVariable efivar. The efivar most likely stores this information at an
 * offset of 92 bytes. However, tests have shown that the control center most
 * likely sets the bit in this variable itself dynamically, so we have to check
 * DMI strings for now.
 */
static int is_auto_boot_and_powershare_supported(bool *status)
{
  *status = false ||
            // IBP Gen6
            dmi_match(DMI_BOARD_NAME, "PHxTxX1") ||
            dmi_match(DMI_BOARD_NAME, "PHxTQx1") ||

            // IBP Gen9
            dmi_match(DMI_BOARD_NAME, "GXxMRXx") ||
            dmi_match(DMI_BOARD_NAME, "GXxHRXx") ||
            // IBP Gen10
            dmi_match(DMI_BOARD_NAME, "XxSP4NAx") ||
            dmi_match(DMI_BOARD_NAME, "XxKK4NAx") ||

            // Stellaris Gen6
            dmi_match(DMI_BOARD_NAME, "GM6IXxB_MB1") ||
            dmi_match(DMI_BOARD_NAME, "GM6IXxB_MB2") ||
            dmi_match(DMI_BOARD_NAME, "GM7IXxN") ||
            // Stellaris Gen7
            dmi_match(DMI_BOARD_NAME, "X6AR5xxY") ||
            dmi_match(DMI_BOARD_NAME, "X6AR5xxY_mLED") ||
            dmi_match(DMI_BOARD_NAME, "X6FR5xxY") ||

            // Stellaris Slim Gen6
            dmi_match(DMI_BOARD_NAME, "GMxHGxx") ||
            dmi_match(DMI_BOARD_NAME, "GM5IXxA");

  return 0;
}

static bool uw_ac_auto_boot_loaded = false;
static bool uw_ac_auto_boot_last_written_value;

static ssize_t uw_ac_auto_boot_show(struct device *child,
                                    struct device_attribute *attr,
                                    char *buffer);
static ssize_t uw_ac_auto_boot_store(struct device *child,
                                     struct device_attribute *attr,
                                     const char *buffer, size_t size);

struct uw_ac_auto_boot_attrs_t
{
    struct device_attribute ac_auto_boot;
} uw_ac_auto_boot_attrs = {
    .ac_auto_boot = __ATTR(ac_auto_boot, 0644, uw_ac_auto_boot_show, uw_ac_auto_boot_store)
};

static struct attribute *uw_ac_auto_boot_attrs_list[] = {
    &uw_ac_auto_boot_attrs.ac_auto_boot.attr,
    NULL};

static struct attribute_group uw_ac_auto_boot_attr_group = {
    .name = "ac_auto_boot",
    .attrs = uw_ac_auto_boot_attrs_list};

static int uw_set_ac_auto_boot(u8 ac_auto_boot)
{
    u8 previous_data, next_data;
    int result;

    ac_auto_boot = (ac_auto_boot & 0x01) << 3;

    result = uniwill_read_ec_ram(UW_EC_REG_AC_AUTO_BOOT_STATUS, &previous_data);
    if (result != 0)
        return result;

    next_data = (previous_data & ~(1 << 3)) | ac_auto_boot;
    result = uniwill_write_ec_ram(UW_EC_REG_AC_AUTO_BOOT_STATUS, next_data);
    if (result == 0)
        uw_ac_auto_boot_last_written_value = ac_auto_boot;

    return result;
}

static int uw_get_ac_auto_boot(u8 *ac_auto_boot)
{
    int result; 
    result = uniwill_read_ec_ram(UW_EC_REG_AC_AUTO_BOOT_STATUS, ac_auto_boot);
    if (result == 0)
        *ac_auto_boot = (*ac_auto_boot >> 3) & 0x01;
    return result;
}

static int uw_has_ac_auto_boot(bool *status)
{
    return is_auto_boot_and_powershare_supported(status);
}

static void uw_ac_auto_boot_init(struct platform_device *dev)
{
    u8 value;
    struct uniwill_device_features_t *uw_feats = &uniwill_device_features;

    if (uw_feats->uniwill_has_ac_auto_boot)
        uw_ac_auto_boot_loaded = sysfs_create_group(&dev->dev.kobj, &uw_ac_auto_boot_attr_group) == 0;

    // Read for state init
    if (uw_ac_auto_boot_loaded)
    {
        uw_get_ac_auto_boot(&value);
        uw_ac_auto_boot_last_written_value = value;
    }
}

static ssize_t uw_ac_auto_boot_show(struct device *child,
                                    struct device_attribute *attr,
                                    char *buffer) 
{
    u8 ac_auto_boot_value;
    int result;

    result = uw_get_ac_auto_boot(&ac_auto_boot_value);
    if (result == 0)
        return sprintf(buffer, "%d\n", ac_auto_boot_value);

    return -EIO;
}

static ssize_t uw_ac_auto_boot_store(struct device *child,
                                     struct device_attribute *attr,
                                     const char *buffer, size_t size) 
{
    u8 ac_auto_boot_value;
    int result;

    if (kstrtou8(buffer, 10, &ac_auto_boot_value) || ac_auto_boot_value < 0 ||
        ac_auto_boot_value > 1)
        return -EINVAL;

    result = uw_set_ac_auto_boot(ac_auto_boot_value);
    if (result == 0)
        return size;
    else
        return -EIO;
}

static bool uw_usb_powershare_loaded = false;
static bool uw_usb_powershare_last_written_value;

static ssize_t uw_usb_powershare_show(struct device *child,
                                      struct device_attribute *attr,
                                      char *buffer);
static ssize_t uw_usb_powershare_store(struct device *child,
                                       struct device_attribute *attr,
                                       const char *buffer, size_t size);

struct uw_usb_powershare_attrs_t
{
    struct device_attribute usb_powershare;
} uw_usb_powershare_attrs = {
    .usb_powershare = __ATTR(usb_powershare, 0644, uw_usb_powershare_show, uw_usb_powershare_store)
};

static struct attribute *uw_usb_powershare_attrs_list[] = {
    &uw_usb_powershare_attrs.usb_powershare.attr,
    NULL};

static struct attribute_group uw_usb_powershare_attr_group = {
    .name = "usb_powershare",
    .attrs = uw_usb_powershare_attrs_list};

static int uw_set_usb_powershare(u8 usb_powershare)
{
    u8 previous_data, next_data;
    int result;
    usb_powershare = (usb_powershare & 0x01) << 4;

    result = uniwill_read_ec_ram(UW_EC_REG_USB_POWERSHARE_STATUS, &previous_data);
    if (result != 0)
        return result;

    next_data = (previous_data & ~(1 << 4)) | usb_powershare;
    // This bit is set to 0 after a cold boot regardless of its original value for some reason.
    result = uniwill_write_ec_ram(UW_EC_REG_USB_POWERSHARE_STATUS, next_data);
    if (result == 0)
        uw_usb_powershare_last_written_value = usb_powershare;

    return result;
}

static int uw_get_usb_powershare(u8 *usb_powershare)
{
    int result; 
    result = uniwill_read_ec_ram(UW_EC_REG_USB_POWERSHARE_STATUS, usb_powershare);
    if (result == 0)
        *usb_powershare = (*usb_powershare >> 4) & 0x01;
    return result;
}

static int uw_has_usb_powershare(bool *status)
{
    return is_auto_boot_and_powershare_supported(status);
}

static void uw_usb_powershare_init(struct platform_device *dev)
{
    u8 value;
    struct uniwill_device_features_t *uw_feats = &uniwill_device_features;

    if (uw_feats->uniwill_has_usb_powershare)
        uw_usb_powershare_loaded = sysfs_create_group(&dev->dev.kobj, &uw_usb_powershare_attr_group) == 0;

    // Read for state init
    if (uw_usb_powershare_loaded)
    {
        uw_get_usb_powershare(&value);
        uw_usb_powershare_last_written_value = value;
    }
}

static ssize_t uw_usb_powershare_show(struct device *child,
                                      struct device_attribute *attr,
                                      char *buffer) 
{
    u8 usb_powershare_value;
    int result;

    result = uw_get_usb_powershare(&usb_powershare_value);
    if (result == 0)
        return sprintf(buffer, "%d\n", usb_powershare_value);

    return -EIO;
}

static ssize_t uw_usb_powershare_store(struct device *child,
                                       struct device_attribute *attr,
                                       const char *buffer, size_t size) 
{
    u8 usb_powershare_value;
    int result;

    if (kstrtou8(buffer, 10, &usb_powershare_value) || usb_powershare_value < 0 ||
        usb_powershare_value > 1)
        return -EINVAL;

    result = uw_set_usb_powershare(usb_powershare_value);
    if (result == 0)
        return size;
    else
        return -EIO;
}

static const u8 uw_romid_PH4PxX[14] = {0x0C, 0x00, 0x01, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF};
static const u8 uw_romid_PH6PxX[14] = {0x0C, 0x01, 0x01, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF};

static const struct dmi_system_id uw_sku_romid_table[] = {
	// IBPG8 mk1
	// Logic: If product serial matches 16inch use that, else default to 14inch
	{
		.matches = {
			DMI_MATCH(DMI_PRODUCT_SKU, "IBP1XI08MK1"),
			DMI_MATCH(DMI_PRODUCT_SERIAL, "PH6PRX"),
		},
		.driver_data = (void *)&uw_romid_PH6PxX
	},
	{
		.matches = {
			DMI_MATCH(DMI_PRODUCT_SKU, "IBP1XI08MK1"),
		},
		.driver_data = (void *)&uw_romid_PH4PxX
	},
	// IBP16G8 mk2
	{
		.matches = {
			DMI_MATCH(DMI_PRODUCT_SKU, "IBP1XI08MK2"),
			DMI_MATCH(DMI_PRODUCT_SERIAL, "PH6"),
		},
		.driver_data = (void *)&uw_romid_PH6PxX
	},
	{}
};

static int set_rom_id(void) {
	int i, ret;
	const struct dmi_system_id *uw_sku_romid;
	const u8 *romid;
	u8 data;
	bool romid_false = false;

	uw_sku_romid = dmi_first_match(uw_sku_romid_table);
	if (!uw_sku_romid)
		return 0;

	romid = (const u8 *)uw_sku_romid->driver_data;
	pr_debug("ROMID 0x%02X 0x%02X 0x%02X 0x%02X 0x%02X 0x%02X 0x%02X 0x%02X 0x%02X 0x%02X 0x%02X 0x%02X 0x%02X 0x%02X\n",
		 romid[0], romid[1], romid[2], romid[3], romid[4], romid[5], romid[6], romid[7],
		 romid[8], romid[9], romid[10], romid[11], romid[12], romid[13]);

	for (i = 0; i < 14; ++i) {
		ret = uniwill_read_ec_ram_with_retry(UW_EC_REG_ROMID_START + i, &data, 3);
		if (ret) {
			pr_debug("uniwill_read_ec_ram_with_retry(...) failed.\n");
			return ret;
		}
		pr_debug("ROMID index: %d, expected value: 0x%02X, actual value: 0x%02X\n", i, romid[i], data);
		if (data != romid[i]) {
			pr_debug("ROMID is false. Correcting...\n");
			romid_false = true;
			break;
		}
	}

	if (romid_false) {
		ret = uniwill_write_ec_ram_with_retry(UW_EC_REG_ROMID_SPECIAL_1, 0xA5, 3);
		if (ret) {
			pr_debug("uniwill_write_ec_ram_with_retry(...) failed.\n");
			return ret;
		}
		ret = uniwill_write_ec_ram_with_retry(UW_EC_REG_ROMID_SPECIAL_2, 0x78, 3);
		if (ret) {
			pr_debug("uniwill_write_ec_ram_with_retry(...) failed.\n");
			return ret;
		}
		for (i = 0; i < 14; ++i) {
			ret = uniwill_write_ec_ram_with_retry(UW_EC_REG_ROMID_START + i, romid[i], 3);
			if (ret) {
				pr_debug("uniwill_write_ec_ram_with_retry(...) failed.\n");
				return ret;
			}
		}
	}
	else
		pr_debug("ROMID is correct.\n");

	return 0;
}

static int has_universal_ec_fan_control(void) {
	int ret;
	u8 data;

	struct uniwill_device_features_t *uw_feats = &uniwill_device_features;

	if (uw_feats->model == UW_MODEL_PH4TRX) {
		// For some reason, on this particular device, the 2nd fan is not controlled via the
		// "GPU" fan curve when the bit to separate both fancurves is set, but the old fan
		// control works just fine.
		return 0;
	}

	ret = uniwill_read_ec_ram(0x078e, &data);
	if (ret < 0) {
		return ret;
	}
	return (data >> 6) & 1;
}

static int has_double_pl4(bool *status)
{
	u8 data;
	int result;

	result = uniwill_read_ec_ram(0x0727, &data);
	if (result)
		return result;

	if (data & (1 << 7))
		*status = true;
	else
		*status = false;

	return 0;
}

struct uniwill_device_features_t *uniwill_get_device_features(void)
{
	struct uniwill_device_features_t *uw_feats = &uniwill_device_features;
	u32 status;
	int result;
	bool feats_loaded;

	if (uw_feats_loaded)
		return uw_feats;

	feats_loaded = true;

	status = uniwill_read_ec_ram(0x0740, &uw_feats->model);
	if (status != 0) {
		uw_feats->model = 0;
		feats_loaded = false;
	}

	uw_feats->uniwill_profile_v1_two_profs = false
		|| dmi_match(DMI_BOARD_NAME, "PF5PU1G")
		|| dmi_match(DMI_BOARD_NAME, "PULSE1401")
		|| dmi_match(DMI_BOARD_NAME, "PULSE1501")
	;

	uw_feats->uniwill_profile_v1_three_profs = false
	// Devices with "classic" profile support
		|| dmi_match(DMI_BOARD_NAME, "POLARIS1501A1650TI")
		|| dmi_match(DMI_BOARD_NAME, "POLARIS1501A2060")
		|| dmi_match(DMI_BOARD_NAME, "POLARIS1501I1650TI")
		|| dmi_match(DMI_BOARD_NAME, "POLARIS1501I2060")
		|| dmi_match(DMI_BOARD_NAME, "POLARIS1701A1650TI")
		|| dmi_match(DMI_BOARD_NAME, "POLARIS1701A2060")
		|| dmi_match(DMI_BOARD_NAME, "POLARIS1701I1650TI")
		|| dmi_match(DMI_BOARD_NAME, "POLARIS1701I2060")
		|| dmi_match(DMI_BOARD_NAME, "GXxMRXx")
		|| dmi_match(DMI_BOARD_NAME, "GXxHRXx")
		// Note: XMG Fusion removed for now, seem to have
		// neither same power profile control nor TDP set
		//|| dmi_match(DMI_BOARD_NAME, "LAPQC71A")
		//|| dmi_match(DMI_BOARD_NAME, "LAPQC71B")
		//|| dmi_match(DMI_PRODUCT_NAME, "A60 MUV")
	;

	uw_feats->uniwill_profile_v1_three_profs_leds_only = false
	// Devices where profile mainly controls power profile LED status
#if LINUX_VERSION_CODE >= KERNEL_VERSION(4, 18, 0)
		|| dmi_match(DMI_PRODUCT_SKU, "POLARIS1XA02")
		|| dmi_match(DMI_PRODUCT_SKU, "POLARIS1XI02")
		|| dmi_match(DMI_PRODUCT_SKU, "POLARIS1XA03")
		|| dmi_match(DMI_PRODUCT_SKU, "POLARIS1XI03")
		|| dmi_match(DMI_PRODUCT_SKU, "STELLARIS1XI03")
		|| dmi_match(DMI_PRODUCT_SKU, "STELLARIS1XA03")
		|| dmi_match(DMI_PRODUCT_SKU, "STELLARIS1XI04")
		|| dmi_match(DMI_PRODUCT_SKU, "STEPOL1XA04")
#endif
	;

	uw_feats->uniwill_custom_profile_mode_needed = false
#if LINUX_VERSION_CODE >= KERNEL_VERSION(4, 18, 0)
		|| dmi_match(DMI_PRODUCT_SKU, "STELLARIS16I06")
		|| dmi_match(DMI_PRODUCT_SKU, "STELLARIS17I06")
		|| dmi_match(DMI_PRODUCT_SKU, "STELLSL15I06")
		|| dmi_match(DMI_PRODUCT_SKU, "STELLSL15A06")
		|| dmi_match(DMI_BOARD_NAME, "GXxMRXx")
		|| dmi_match(DMI_BOARD_NAME, "GXxHRXx")
#endif
	;


	if (has_double_pl4(&uw_feats->uniwill_has_double_pl4) != 0)
		feats_loaded = false;

	uw_feats->uniwill_profile_v1 =
		uw_feats->uniwill_profile_v1_two_profs ||
		uw_feats->uniwill_profile_v1_three_profs;

	if (uw_has_charging_priority(&uw_feats->uniwill_has_charging_prio) != 0)
		feats_loaded = false;
	if (uw_has_charging_profile(&uw_feats->uniwill_has_charging_profile) != 0)
		feats_loaded = false;
	if (uw_has_ac_auto_boot(&uw_feats->uniwill_has_ac_auto_boot) != 0)
		feats_loaded = false;
	if (uw_has_usb_powershare(&uw_feats->uniwill_has_usb_powershare) != 0)
		feats_loaded = false;

	result = has_universal_ec_fan_control();
	if (result < 0) {
		feats_loaded = false;
	} else {
		uw_feats->uniwill_has_universal_ec_fan_control = (result == 1);
	}


	if (feats_loaded)
		pr_debug("feats loaded\n");
	else
		pr_debug("feats not yet loaded\n");

	uw_feats_loaded = feats_loaded;

	return uw_feats;
}
EXPORT_SYMBOL(uniwill_get_device_features);

// Fn lock

static int uniwill_wmi_fn_lock_get(int *on)
{
	u8 data;
	int err;

	err = uniwill_read_ec_ram(UW_EC_REG_KBD_FN_LOCK_STATUS_BIT, &data);
	if (err)
		return err;

	if (on)
		*on = (data & UNIWILL_FN_LOCK_MASK) >> 4;

	return 0;
}

static int uniwill_wmi_fn_lock_set(int on)
{
	u8 data;
	int err;

	// possible race condition
	err = uniwill_read_ec_ram(UW_EC_REG_KBD_FN_LOCK_STATUS_BIT, &data);
	if (err)
		return err;

	if (on)
		data = data | UNIWILL_FN_LOCK_MASK;
	else
		data = data & ~UNIWILL_FN_LOCK_MASK;

	err = uniwill_write_ec_ram(UW_EC_REG_KBD_FN_LOCK_STATUS_BIT, data);
	if (err)
		return err;

	return 0;
}

static ssize_t uniwill_fn_lock_show(struct device *dev,
		struct device_attribute *attr,
		char *buf)
{
	int err, on;

	err = uniwill_wmi_fn_lock_get(&on);
	if (err)
		return err;

	return sprintf(buf, "%d\n", on);
}

static ssize_t uniwill_fn_lock_store(struct device *dev,
		struct device_attribute *attr,
		const char *buf, size_t size)
{
	int on, err;

	if (kstrtoint(buf, 10, &on) ||
			on < 0 || on > 1)
		return -EINVAL;

	err = uniwill_wmi_fn_lock_set(on);
	if (err)
		return err;

	return size;
}

static bool uniwill_fn_lock_available(void){
	int err, on;

	// Fn lock does not work for XMG Fusion
	// exclude all versions
	if (dmi_match(DMI_BOARD_NAME, "LAPQC71A")
	    || dmi_match(DMI_BOARD_NAME, "LAPQC71B")
	    || dmi_match(DMI_PRODUCT_NAME, "A60 MUV")) {
		return 0;
	}

	// do a read for test (this may not produce an error)
	err = uniwill_wmi_fn_lock_get(&on);
	if (err)
		return 0;
	else
		return 1;
}

static u8 uniwill_touchp_toggle_seq[] = {
	0xe0, 0x5b, // Super down
	0x1d,       // Control down
	0x76,       // Zenkaku/Hankaku down
	0xf6,       // Zenkaku/Hankaku up
	0x9d,       // Control up
	0xe0, 0xdb  // Super up
};

#if LINUX_VERSION_CODE < KERNEL_VERSION(6, 14, 0)
static bool uniwill_i8042_filter(unsigned char data, unsigned char str,
				 struct serio *port __always_unused)
#else
static bool uniwill_i8042_filter(unsigned char data, unsigned char str,
				 struct serio *port __always_unused,
				 void *context __always_unused)
#endif
{
	static u8 seq_pos;

	if (unlikely(str & I8042_STR_AUXDATA))
		return false;

	if (unlikely(data == uniwill_touchp_toggle_seq[seq_pos])) {
		++seq_pos;
		if (unlikely(data == 0x76 || data == 0xf6))
			return true;
		else if (unlikely(seq_pos == ARRAY_SIZE(uniwill_touchp_toggle_seq))) {
			schedule_work(&uniwill_key_event_work);
			seq_pos = 0;
		}
		return false;
	}

	seq_pos = 0;
	return false;
}

static int uniwill_keyboard_probe(struct platform_device *dev)
{
	u32 i;
	u8 data;
	int status;
	struct uniwill_device_features_t *uw_feats;

	set_rom_id();

	uw_feats = uniwill_get_device_features();

	// FIXME Hard set balanced profile until we have implemented a way to
	// switch it while tuxedo_io is loaded
	// uw_ec_write_addr(0x51, 0x07, 0x00, 0x00, &reg_write_return);
	uniwill_write_ec_ram(0x0751, 0x00);

	if (uw_feats->uniwill_profile_v1) {
		// Set manual-mode fan-curve in 0x0743 - 0x0747
		// Some kind of default fan-curve is stored in 0x0786 - 0x078a: Using it to initialize manual-mode fan-curve
		for (i = 0; i < 5; ++i) {
			uniwill_read_ec_ram(0x0786 + i, &data);
			uniwill_write_ec_ram(0x0743 + i, data);
		}
	}

	// Make sure custom TDP/custom fan curve mode is set. Using the
	// custom profile mode flag to ID this set of devices.
	if (uw_feats->uniwill_custom_profile_mode_needed) {
		// Certain devices seem to need this first reset to
		// zero on boot to have it properly applied
		uniwill_read_ec_ram(0x0727, &data);
		data &= ~(1 << 6);
		uniwill_write_ec_ram(0x0727, data);
		msleep(50);
		data |= (1 << 6);
		uniwill_write_ec_ram(0x0727, data);
	}

	// Enable manual mode
	uniwill_write_ec_ram(0x0741, 0x01);

	// Zero second fan temp for detection
	uniwill_write_ec_ram(0x044f, 0x00);

#if LINUX_VERSION_CODE < KERNEL_VERSION(5, 9, 0)
	TUXEDO_ERROR("Warning: Kernel version less that 5.9, keyboard backlight might not be properly recognized.");
#endif
	uniwill_read_ec_ram(UW_EC_REG_KBD_BL_STATUS, &data);
	uniwill_kbd_bl_enable_state_on_start = (data >> 1) & 0x01;
	uniwill_leds_init(dev);
	uniwill_write_kbd_bl_enable(1);

	status = uw_lightbar_init(dev);
	uw_lightbar_loaded = (status >= 0);

	uw_charging_priority_init(dev);
	uw_charging_profile_init(dev);
	uw_ac_auto_boot_init(dev);
	uw_usb_powershare_init(dev);

	// Ignore return value, it just means there is already a filter active
	// which is fine, because it is probably just the upstream patch of this
	// filter.
#if LINUX_VERSION_CODE < KERNEL_VERSION(6, 14, 0)
	if (i8042_install_filter(uniwill_i8042_filter))
#else
	if (i8042_install_filter(uniwill_i8042_filter, NULL))
#endif
		pr_info("Could not install i8042 filter.\n");

	return 0;
}

#if LINUX_VERSION_CODE < KERNEL_VERSION(6, 11, 0)
static int uniwill_keyboard_remove(struct platform_device *dev)
#else
static void uniwill_keyboard_remove(struct platform_device *dev)
#endif
{
	if (uw_charging_prio_loaded)
		sysfs_remove_group(&dev->dev.kobj, &uw_charging_prio_attr_group);

	if (uw_charging_profile_loaded)
		sysfs_remove_group(&dev->dev.kobj, &uw_charging_profile_attr_group);

	uniwill_leds_remove(dev);

	// Restore previous backlight enable state
	if (uniwill_kbd_bl_enable_state_on_start != 0xff) {
		uniwill_write_kbd_bl_enable(uniwill_kbd_bl_enable_state_on_start);
	}

	if (uw_lightbar_loaded)
		uw_lightbar_remove(dev);

	// Disable manual mode
	uniwill_write_ec_ram(0x0741, 0x00);

	// Ignore return value, it just means this filter was not active atm.
	if (i8042_remove_filter(uniwill_i8042_filter))
		pr_info("Could not remove i8042 filter.\n");

#if LINUX_VERSION_CODE < KERNEL_VERSION(6, 11, 0)
	return 0;
#endif
}

static int uniwill_keyboard_suspend(struct platform_device *dev, pm_message_t state)
{
	struct uniwill_device_features_t *uw_feats = &uniwill_device_features;
	u8 data;
	if (uw_feats->uniwill_custom_profile_mode_needed) {
		// Unset "customer mode light" before suspend. Otherwise at
		// least one device is known to immediately wake up.
		uniwill_read_ec_ram(0x0727, &data);
		data &= ~(1 << 6);
		uniwill_write_ec_ram(0x0727, data);
	}
	uniwill_write_kbd_bl_enable(0);
	return 0;
}

static int uniwill_keyboard_resume(struct platform_device *dev)
{
	struct uniwill_device_features_t *uw_feats = &uniwill_device_features;
	u8 data;
	if (uw_feats->uniwill_custom_profile_mode_needed) {
		// Re-set "customer mode light" on resume
		uniwill_read_ec_ram(0x0727, &data);
		data |= (1 << 6);
		uniwill_write_ec_ram(0x0727, data);
	}
	uniwill_leds_restore_state_extern();
	uniwill_write_kbd_bl_enable(1);
	return 0;
}

static struct platform_driver platform_driver_uniwill = {
	.remove = uniwill_keyboard_remove,
	.suspend = uniwill_keyboard_suspend,
	.resume = uniwill_keyboard_resume,
	.driver =
		{
			.name = DRIVER_NAME,
			.owner = THIS_MODULE,
		},
};

struct tuxedo_keyboard_driver uniwill_keyboard_driver = {
	.platform_driver = &platform_driver_uniwill,
	.probe = uniwill_keyboard_probe,
	.key_map = uniwill_wmi_keymap,
	.fn_lock_available = uniwill_fn_lock_available,
	.fn_lock_show = uniwill_fn_lock_show,
	.fn_lock_store = uniwill_fn_lock_store,
};

#endif // UNIWILL_KEYBOARD_H
