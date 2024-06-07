/*!
 * Copyright (c) 2018-2020 TUXEDO Computers GmbH <tux@tuxedocomputers.com>
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

#ifndef UNIWILL_LEDS_H
#define UNIWILL_LEDS_H

#include <linux/platform_device.h>

typedef enum {
	UNIWILL_KB_BACKLIGHT_TYPE_NONE,
	UNIWILL_KB_BACKLIGHT_TYPE_FIXED_COLOR,
	UNIWILL_KB_BACKLIGHT_TYPE_1_ZONE_RGB,
	UNIWILL_KB_BACKLIGHT_TYPE_PER_KEY_RGB
} uniwill_kb_backlight_type_t;

int uniwill_leds_init(struct platform_device *dev);
int uniwill_leds_remove(struct platform_device *dev);

uniwill_kb_backlight_type_t uniwill_leds_get_backlight_type_extern(void);
void uniwill_leds_restore_state_extern(void);
bool uniwill_leds_notify_brightness_change_extern(void);

// TODO The following should go into a seperate .c file, but for this to work more reworking is required in the tuxedo_keyboard structure.

//#include "uniwill_leds.h"

#include <linux/types.h>
#include <linux/leds.h>
#include <linux/led-class-multicolor.h>
#include "uniwill_interfaces.h"

#define UNIWILL_KBD_BRIGHTNESS_MAX_WHITE		0x02
#define UNIWILL_KBD_BRIGHTNESS_DEFAULT_WHITE		0x00

#define UNIWILL_KBD_BRIGHTNESS_MAX_1_ZONE_RGB		0x04
#define UNIWILL_KBD_BRIGHTNESS_DEFAULT_1_ZONE_RGB	0x00

#define UNIWILL_KBD_COLOR_DEFAULT_RED			0xff
#define UNIWILL_KBD_COLOR_DEFAULT_GREEN			0xff
#define UNIWILL_KBD_COLOR_DEFAULT_BLUE			0xff

static uniwill_kb_backlight_type_t uniwill_kb_backlight_type = UNIWILL_KB_BACKLIGHT_TYPE_NONE;
static u8 uniwill_barebone_id = 0;
static bool uniwill_kbl_brightness_ec_controlled = false;
static bool uw_leds_initialized = false;

static int uniwill_write_kbd_bl_brightness(u8 brightness)
{
	int result = 0;
	u8 data = 0;

	result = uniwill_read_ec_ram(UW_EC_REG_KBD_BL_STATUS, &data);
	if (result)
		return result;
	data &= 0x0f; // lower bits must be preserved
	data |= brightness << 5; // upper 2 to 3 bits encode brightness
	data |= UW_EC_REG_KBD_BL_STATUS_SUBCMD_RESET; // "apply bit"
	return uniwill_write_ec_ram(UW_EC_REG_KBD_BL_STATUS, data);
}

static int uniwill_write_kbd_bl_brightness_white_workaround(u8 brightness)
{
	int result = 0;
	u8 data = 0;

	// When keyboard backlight is off, new settings to 0x078c do not get applied automatically
	// on Pulse Gen1/2 until next keypress or manual change to the 0x1808 immediate brightness
	// value for some reason.
	// Sidenote: IBP Gen6/7 has immediate brightness value on 0x1802 and not on 0x1808, but does
	// not need this workaround. No model check required because this doesn't do anything on
	// these devices.

	result = uniwill_read_ec_ram(UW_EC_REG_KBD_BL_RGB_BLUE_BRIGHTNESS_IMMEDIATE, &data);
	if (result)
		pr_debug("uniwill_write_kbd_bl_white(): Get UW_EC_REG_KBD_BL_RGB_BLUE_BRIGHTNESS_IMMEDIATE failed.\n");

	if (!data && brightness) {
		result = uniwill_write_ec_ram(UW_EC_REG_KBD_BL_RGB_BLUE_BRIGHTNESS_IMMEDIATE, 0x01);
		if (result)
			pr_debug("uniwill_write_kbd_bl_white(): Set UW_EC_REG_KBD_BL_RGB_BLUE_BRIGHTNESS_IMMEDIATE failed.\n");
	}

	return uniwill_write_kbd_bl_brightness(brightness);
}

// Converts the range 0-255 to the range 0-50
static int tf_convert_rgb_range(u8 input) {
	return input*200/(255*4);
}

static int uniwill_write_kbd_bl_color(u8 red, u8 green, u8 blue)
{
	int result = 0;
	u8 data = 0;

	// If, after conversion, all three (red, green, and blue) values are zero at the same time,
	// a special case is triggered in the EC and (probably device dependent) default values are
	// written instead.

	result = uniwill_write_ec_ram(UW_EC_REG_KBD_BL_RGB_RED_BRIGHTNESS, tf_convert_rgb_range(red));
	if (result)
		return result;

	result = uniwill_write_ec_ram(UW_EC_REG_KBD_BL_RGB_GREEN_BRIGHTNESS, tf_convert_rgb_range(green));
	if (result)
		return result;

	result = uniwill_write_ec_ram(UW_EC_REG_KBD_BL_RGB_BLUE_BRIGHTNESS, tf_convert_rgb_range(blue));
	if (result)
		return result;

	result = uniwill_read_ec_ram(UW_EC_REG_KBD_BL_RGB_MODE, &data);
	if (result)
		return result;

	result = uniwill_write_ec_ram(UW_EC_REG_KBD_BL_RGB_MODE, data | UW_EC_REG_KBD_BL_RGB_MODE_BIT_APPLY_COLOR);
	if (result)
		return result;

	pr_debug("Wrote kbd color [%0#4x, %0#4x, %0#4x]\n", red, green, blue);

	return result;
}

static void uniwill_leds_set_brightness(struct led_classdev *led_cdev __always_unused, enum led_brightness brightness) {
	int result = 0;

	result = uniwill_write_kbd_bl_brightness_white_workaround(brightness);
	if (result) {
		pr_debug("uniwill_leds_set_brightness(): uniwill_write_kbd_bl_white() failed\n");
		return;
	}

	led_cdev->brightness = brightness;
}

static void uniwill_leds_set_brightness_mc(struct led_classdev *led_cdev, enum led_brightness brightness) {
	int result = 0;
	struct led_classdev_mc *mcled_cdev = lcdev_to_mccdev(led_cdev);

	if (mcled_cdev->subled_info[0].intensity == 0 &&
	    mcled_cdev->subled_info[1].intensity == 0 &&
	    mcled_cdev->subled_info[2].intensity == 0) {
		pr_debug("uniwill_leds_set_brightness_mc(): Trigger RGB 0x000000 special case\n");
		result = uniwill_write_kbd_bl_brightness(0);
		if (result) {
			pr_debug("uniwill_leds_set_brightness_mc(): uniwill_write_kbd_bl_brightness() failed\n");
			return;
		}
	}
	else {
		result = uniwill_write_kbd_bl_color(mcled_cdev->subled_info[0].intensity,
					       mcled_cdev->subled_info[1].intensity,
					       mcled_cdev->subled_info[2].intensity);
		if (result) {
			pr_debug("uniwill_leds_set_brightness_mc(): uniwill_write_kbd_bl_rgb() failed\n");
			return;
		}

		result = uniwill_write_kbd_bl_brightness(brightness);
		if (result) {
			pr_debug("uniwill_leds_set_brightness_mc(): uniwill_write_kbd_bl_brightness() failed\n");
			return;
		}
	}

	led_cdev->brightness = brightness;
}

static struct led_classdev uniwill_led_cdev = {
	.name = "white:" LED_FUNCTION_KBD_BACKLIGHT,
	.max_brightness = UNIWILL_KBD_BRIGHTNESS_MAX_WHITE,
	.brightness_set = &uniwill_leds_set_brightness,
	.brightness = UNIWILL_KBD_BRIGHTNESS_DEFAULT_WHITE,
};

static struct mc_subled uw_mcled_cdev_subleds[3] = {
	{
		.color_index = LED_COLOR_ID_RED,
		.intensity = UNIWILL_KBD_COLOR_DEFAULT_RED,
		.channel = 0
	},
	{
		.color_index = LED_COLOR_ID_GREEN,
		.intensity = UNIWILL_KBD_COLOR_DEFAULT_GREEN,
		.channel = 0
	},
	{
		.color_index = LED_COLOR_ID_BLUE,
		.intensity = UNIWILL_KBD_COLOR_DEFAULT_BLUE,
		.channel = 0
	}
};

static struct led_classdev_mc uniwill_mcled_cdev = {
	.led_cdev.name = "rgb:" LED_FUNCTION_KBD_BACKLIGHT,
	.led_cdev.max_brightness = UNIWILL_KBD_BRIGHTNESS_MAX_1_ZONE_RGB,
	.led_cdev.brightness_set = &uniwill_leds_set_brightness_mc,
	.led_cdev.brightness = UNIWILL_KBD_BRIGHTNESS_DEFAULT_1_ZONE_RGB,
	.num_colors = 3,
	.subled_info = uw_mcled_cdev_subleds
};

static const struct dmi_system_id force_no_ec_led_control[] = {
	{
		.matches = {
			DMI_MATCH(DMI_SYS_VENDOR, "TUXEDO"),
			DMI_MATCH(DMI_PRODUCT_SKU, "STELLARIS1XA05"),
		},
		.matches = {
			DMI_MATCH(DMI_SYS_VENDOR, "TUXEDO"),
			DMI_MATCH(DMI_PRODUCT_SKU, "STELLSL15I01"),
		},
	},
	{ }
};

int uniwill_leds_init(struct platform_device *dev)
{
	int result = 0, i = 0;
	u8 data = 0;

	for (i = 0; i < 3; ++i) {
		if (i) {
			pr_err("Reading barebone ID failed. Retrying ...\n");
		}

		result = uniwill_read_ec_ram(UW_EC_REG_BAREBONE_ID, &uniwill_barebone_id);
		if (!result && uniwill_barebone_id) {
			break;
		}
		msleep(200);
	}
	if (result || !uniwill_barebone_id) {
		pr_err("Reading barebone ID failed.\n");
		return result;
	}
	pr_debug("EC Barebone ID: %#04x\n", uniwill_barebone_id);

	if (dmi_check_system(force_no_ec_led_control)) {
		pr_debug("Skip uniwill_kb_backlight_type checks because of quirk.\n");
	}
	else if (uniwill_barebone_id == UW_EC_REG_BAREBONE_ID_VALUE_PFxxxxx ||
		 uniwill_barebone_id == UW_EC_REG_BAREBONE_ID_VALUE_PFxMxxx ||
		 uniwill_barebone_id == UW_EC_REG_BAREBONE_ID_VALUE_PH4TRX1 ||
		 uniwill_barebone_id == UW_EC_REG_BAREBONE_ID_VALUE_PH4TUX1 ||
		 uniwill_barebone_id == UW_EC_REG_BAREBONE_ID_VALUE_PH4TQx1 ||
		 uniwill_barebone_id == UW_EC_REG_BAREBONE_ID_VALUE_PH6TRX1 ||
		 uniwill_barebone_id == UW_EC_REG_BAREBONE_ID_VALUE_PH6TQxx ||
		 uniwill_barebone_id == UW_EC_REG_BAREBONE_ID_VALUE_PH4Axxx ||
		 uniwill_barebone_id == UW_EC_REG_BAREBONE_ID_VALUE_PH4Pxxx) {
		uniwill_kb_backlight_type = UNIWILL_KB_BACKLIGHT_TYPE_FIXED_COLOR;
		uniwill_kbl_brightness_ec_controlled = true;
	}
	else {
		result = uniwill_read_ec_ram(UW_EC_REG_FEATURES_1, &data);
		if (result) {
			pr_err("Reading features 1 failed.\n");
			return result;
		}
		pr_debug("UW_EC_REG_FEATURES_1: 0x%02x\n", data);

		if (data & UW_EC_REG_FEATURES_1_BIT_1_ZONE_RGB_KB) {
			uniwill_kb_backlight_type = UNIWILL_KB_BACKLIGHT_TYPE_1_ZONE_RGB;
			uniwill_kbl_brightness_ec_controlled = true;
		}
	}
	pr_debug("Keyboard backlight type: 0x%02x\n", uniwill_kb_backlight_type);

	if (uniwill_kb_backlight_type == UNIWILL_KB_BACKLIGHT_TYPE_FIXED_COLOR) {
		pr_debug("Registering fixed color leds interface\n");
		if (uniwill_kbl_brightness_ec_controlled)
			uniwill_led_cdev.flags = LED_BRIGHT_HW_CHANGED;
		result = led_classdev_register(&dev->dev, &uniwill_led_cdev);
		if (result) {
			pr_err("Registering fixed color leds interface failed\n");
			return result;
		}
	}
	else if (uniwill_kb_backlight_type == UNIWILL_KB_BACKLIGHT_TYPE_1_ZONE_RGB) {
		pr_debug("Registering single zone rgb leds interface\n");
		if (uniwill_kbl_brightness_ec_controlled)
			uniwill_mcled_cdev.led_cdev.flags = LED_BRIGHT_HW_CHANGED;
		result = devm_led_classdev_multicolor_register(&dev->dev, &uniwill_mcled_cdev);
		if (result) {
			pr_err("Registering single zone rgb leds interface failed\n");
			return result;
		}
	}

	uw_leds_initialized = true;

	return 0;
}
EXPORT_SYMBOL(uniwill_leds_init);

int uniwill_leds_remove(struct platform_device *dev)
{
	int result = 0;

	if (uw_leds_initialized) {
		uw_leds_initialized = false;

		if (uniwill_kb_backlight_type == UNIWILL_KB_BACKLIGHT_TYPE_FIXED_COLOR) {
			led_classdev_unregister(&uniwill_led_cdev);
		}
		else if (uniwill_kb_backlight_type == UNIWILL_KB_BACKLIGHT_TYPE_1_ZONE_RGB) {
			devm_led_classdev_multicolor_unregister(&dev->dev, &uniwill_mcled_cdev);
		}
	}

	return result;
}
EXPORT_SYMBOL(uniwill_leds_remove);

uniwill_kb_backlight_type_t uniwill_leds_get_backlight_type_extern(void) {
	return uniwill_kb_backlight_type;
}
EXPORT_SYMBOL(uniwill_leds_get_backlight_type_extern);

void uniwill_leds_restore_state_extern(void) {
	if (uniwill_kb_backlight_type == UNIWILL_KB_BACKLIGHT_TYPE_FIXED_COLOR) {
		if (uniwill_write_kbd_bl_brightness_white_workaround(uniwill_led_cdev.brightness)) {
			pr_debug("uniwill_leds_restore_state_extern(): uniwill_write_kbd_bl_white() failed\n");
		}
	}
	else if (uniwill_kb_backlight_type == UNIWILL_KB_BACKLIGHT_TYPE_1_ZONE_RGB) {
		if (uniwill_write_kbd_bl_color(uniwill_mcled_cdev.subled_info[0].intensity,
					     uniwill_mcled_cdev.subled_info[1].intensity,
					     uniwill_mcled_cdev.subled_info[2].intensity)) {
			pr_debug("uniwill_leds_restore_state_extern(): uniwill_write_kbd_bl_rgb() failed\n");
		}
		if (uniwill_write_kbd_bl_brightness(uniwill_mcled_cdev.led_cdev.brightness)) {
			pr_debug("uniwill_leds_restore_state_extern(): uniwill_write_kbd_bl_brightness() failed\n");
		}
	}
}
EXPORT_SYMBOL(uniwill_leds_restore_state_extern);

bool uniwill_leds_notify_brightness_change_extern(void) {
	int result = 0;
	u8 data = 0, brightness = 0;

	if (uw_leds_initialized) {
		if (uniwill_kbl_brightness_ec_controlled) {
			uniwill_read_ec_ram(UW_EC_REG_KBD_BL_STATUS, &data);
			brightness = (data >> 5) & 0x07;
			if (uniwill_kb_backlight_type == UNIWILL_KB_BACKLIGHT_TYPE_FIXED_COLOR) {
				uniwill_led_cdev.brightness = brightness;
				led_classdev_notify_brightness_hw_changed(&uniwill_led_cdev, uniwill_led_cdev.brightness);
				return true;
			}
			else if (uniwill_kb_backlight_type == UNIWILL_KB_BACKLIGHT_TYPE_1_ZONE_RGB) {
				result = 0;
				if (uniwill_mcled_cdev.led_cdev.brightness == brightness) {
					// Workaround for devices where EC does not react to FN+space in manual mode (know device: Polaris Gen2)
					result = uniwill_write_kbd_bl_brightness((brightness + 1) % 5);
					if (result) {
						pr_debug("uniwill_leds_set_brightness_mc(): uniwill_write_kbd_bl_brightness() failed\n");
					}
					else {
						brightness = (brightness + 1) % 5;
					}
				}
				if (!result) {
					uniwill_mcled_cdev.led_cdev.brightness = brightness;
					led_classdev_notify_brightness_hw_changed(&uniwill_mcled_cdev.led_cdev, uniwill_mcled_cdev.led_cdev.brightness);
				}
				return true;
			}
		}
	}
	return false;
}

MODULE_LICENSE("GPL");

#endif // UNIWILL_LEDS_H
