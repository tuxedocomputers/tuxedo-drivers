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
#include <linux/module.h>
#include <linux/platform_device.h>
#include <linux/led-class-multicolor.h>
#include <linux/version.h>
#include "tuxedo_nb04_wmi_ab.h"

#define KEYBOARD_MAX_BRIGHTNESS		0x0a
#define KEYBOARD_DEFAULT_BRIGHTNESS	0x00
#define KEYBOARD_DEFAULT_COLOR_RED	0xff
#define KEYBOARD_DEFAULT_COLOR_GREEN	0xff
#define KEYBOARD_DEFAULT_COLOR_BLUE	0xff

struct driver_data_t {
	struct led_classdev_mc mcled_cdev_keyboard;
	struct mc_subled mcled_cdev_subleds_keyboard[3];
	struct device_keyboard_status_t device_status;
};

void leds_set_brightness_mc_keyboard(struct led_classdev *led_cdev, enum led_brightness brightness)
{
	struct led_classdev_mc *mcled_cdev = lcdev_to_mccdev(led_cdev);
	u8 red = mcled_cdev->subled_info[0].intensity;
	u8 green = mcled_cdev->subled_info[1].intensity;
	u8 blue = mcled_cdev->subled_info[2].intensity;

	pr_debug("wmi_set_whole_keyboard(%u, %u, %u, %u)\n", red, green, blue, brightness);

	wmi_set_whole_keyboard(red, green, blue, brightness);
}

static int init_leds(struct platform_device *pdev)
{
	struct driver_data_t *driver_data = dev_get_drvdata(&pdev->dev);
	int retval;

	driver_data->mcled_cdev_keyboard.led_cdev.name = "rgb:" LED_FUNCTION_KBD_BACKLIGHT;
	driver_data->mcled_cdev_keyboard.led_cdev.max_brightness = KEYBOARD_MAX_BRIGHTNESS;
	driver_data->mcled_cdev_keyboard.led_cdev.brightness_set = &leds_set_brightness_mc_keyboard;
	driver_data->mcled_cdev_keyboard.led_cdev.brightness = KEYBOARD_DEFAULT_BRIGHTNESS;
	driver_data->mcled_cdev_keyboard.num_colors = 3;
	driver_data->mcled_cdev_keyboard.subled_info = driver_data->mcled_cdev_subleds_keyboard;
	driver_data->mcled_cdev_keyboard.subled_info[0].color_index = LED_COLOR_ID_RED;
	driver_data->mcled_cdev_keyboard.subled_info[0].intensity = KEYBOARD_DEFAULT_COLOR_RED;
	driver_data->mcled_cdev_keyboard.subled_info[0].channel = 0;
	driver_data->mcled_cdev_keyboard.subled_info[1].color_index = LED_COLOR_ID_GREEN;
	driver_data->mcled_cdev_keyboard.subled_info[1].intensity = KEYBOARD_DEFAULT_COLOR_GREEN;
	driver_data->mcled_cdev_keyboard.subled_info[1].channel = 0;
	driver_data->mcled_cdev_keyboard.subled_info[2].color_index = LED_COLOR_ID_BLUE;
	driver_data->mcled_cdev_keyboard.subled_info[2].intensity = KEYBOARD_DEFAULT_COLOR_BLUE;
	driver_data->mcled_cdev_keyboard.subled_info[2].channel = 0;

	retval = devm_led_classdev_multicolor_register(&pdev->dev, &driver_data->mcled_cdev_keyboard);
	if (retval)
		return retval;

	return 0;
}

static int __init tuxedo_nb04_kbd_backlight_probe(struct platform_device *pdev)
{
	int result;
	struct driver_data_t *driver_data;

	pr_debug("driver probe\n");

	driver_data = devm_kzalloc(&pdev->dev, sizeof(struct driver_data_t), GFP_KERNEL);
	if (!driver_data)
		return -ENOMEM;

	dev_set_drvdata(&pdev->dev, driver_data);

	// Note: Read of keyboard status needed for fw init
	//       before writing can be done
	result = wmi_update_device_status_keyboard(&driver_data->device_status);

	if (result) {
		if (result != -ENODEV)
			pr_err("Failed initial read %d\n", result);
		return result;
	}

	result = init_leds(pdev);
	if (result)
		return result;

	pr_debug("kbd enabled: %d\n", driver_data->device_status.keyboard_state_enabled);
	pr_debug("kbd type: %d\n", driver_data->device_status.keyboard_type);
	pr_debug("kbd sidebar support: %d\n", driver_data->device_status.keyboard_sidebar_support);
	pr_debug("kbd keyboard matrix: %d\n", driver_data->device_status.keyboard_matrix);

	return 0;
}

#if LINUX_VERSION_CODE < KERNEL_VERSION(6, 11, 0)
static int tuxedo_nb04_kbd_backlight_remove(struct platform_device *pdev)
#else
static void tuxedo_nb04_kbd_backlight_remove(struct platform_device *pdev)
#endif
{
	struct driver_data_t *driver_data = dev_get_drvdata(&pdev->dev);
	devm_led_classdev_multicolor_unregister(&pdev->dev, &driver_data->mcled_cdev_keyboard);
	pr_debug("driver remove\n");
#if LINUX_VERSION_CODE < KERNEL_VERSION(6, 11, 0)
	return 0;
#endif
}

static struct platform_device *tuxedo_nb04_kbd_backlight_device;
static struct platform_driver tuxedo_nb04_kbd_backlight_driver = {
	.driver.name = "tuxedo_nb04_kbd_backlight",
	.remove = tuxedo_nb04_kbd_backlight_remove
};

static int __init tuxedo_nb04_kbd_backlight_init(void)
{
	tuxedo_nb04_kbd_backlight_device =
		platform_create_bundle(&tuxedo_nb04_kbd_backlight_driver,
				       tuxedo_nb04_kbd_backlight_probe, NULL, 0, NULL, 0);

	if (IS_ERR(tuxedo_nb04_kbd_backlight_device))
		return PTR_ERR(tuxedo_nb04_kbd_backlight_device);

	return 0;
}

static void __exit tuxedo_nb04_kbd_backlight_exit(void)
{
	platform_device_unregister(tuxedo_nb04_kbd_backlight_device);
	platform_driver_unregister(&tuxedo_nb04_kbd_backlight_driver);
}

module_init(tuxedo_nb04_kbd_backlight_init);
module_exit(tuxedo_nb04_kbd_backlight_exit);

MODULE_AUTHOR("TUXEDO Computers GmbH <tux@tuxedocomputers.com>");
MODULE_DESCRIPTION("Driver for NB04 keyboard backlight");
MODULE_LICENSE("GPL");

