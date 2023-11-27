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
#include <linux/led-class-multicolor.h>
#include <linux/version.h>
#include <linux/delay.h>

#define NB04_WMI_AB_GUID	"80C9BAA6-AC48-4538-9234-9F81A55E7C85"
#define NB04_WMI_BB_GUID	"B8BED7BB-3F3D-4C71-953D-6D4172F27A63"

#define KEYBOARD_MAX_BRIGHTNESS		0x09
#define KEYBOARD_DEFAULT_BRIGHTNESS	0x00
#define KEYBOARD_DEFAULT_COLOR_RED	0xff
#define KEYBOARD_DEFAULT_COLOR_GREEN	0xff
#define KEYBOARD_DEFAULT_COLOR_BLUE	0xff

struct driver_data_t {
	struct led_classdev_mc mcled_cdev_keyboard;
	struct mc_subled mcled_cdev_subleds_keyboard[3];
};

void leds_set_brightness_mc_keyboard(struct led_classdev *led_cdev, enum led_brightness brightness) {

}

static int init_leds(struct wmi_device *wdev)
{
	struct driver_data_t *driver_data = dev_get_drvdata(&wdev->dev);
	int retval;

	driver_data->mcled_cdev_keyboard.led_cdev.name = "rgb:" "kbd_backlight";
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

	retval = devm_led_classdev_multicolor_register(&wdev->dev, &driver_data->mcled_cdev_keyboard);
	if (retval)
		return retval;

	return 0;
}

#if LINUX_VERSION_CODE < KERNEL_VERSION(5, 3, 0)
static int tuxedo_nb04_wmi_ab_probe(struct wmi_device *wdev)
#else
static int tuxedo_nb04_wmi_ab_probe(struct wmi_device *wdev, const void *dummy_context)
#endif
{
	int result;
	struct driver_data_t *driver_data;

	pr_debug("driver probe\n");

	driver_data = devm_kzalloc(&wdev->dev, sizeof(struct driver_data_t), GFP_KERNEL);
	if (!driver_data)
		return -ENOMEM;

	dev_set_drvdata(&wdev->dev, driver_data);

	result = init_leds(wdev);
	if (result)
		return result;

	return 0;
}

#if LINUX_VERSION_CODE < KERNEL_VERSION(5, 13, 0)
static int tuxedo_nb04_wmi_ab_remove(struct wmi_device *wdev)
#else
static void tuxedo_nb04_wmi_ab_remove(struct wmi_device *wdev)
#endif
{
	struct driver_data_t *driver_data = dev_get_drvdata(&wdev->dev);
	devm_led_classdev_multicolor_unregister(&wdev->dev, &driver_data->mcled_cdev_keyboard);
	pr_debug("driver remove\n");

#if LINUX_VERSION_CODE < KERNEL_VERSION(5, 13, 0)
	return 0;
#endif
}

static const struct wmi_device_id tuxedo_nb04_wmi_ab_device_ids[] = {
	{ .guid_string = NB04_WMI_AB_GUID },
	{ }
};

static struct wmi_driver tuxedo_nb04_wmi_ab_driver = {
	.driver = {
		.name = "tuxedo_nb04_wmi_ab",
		.owner = THIS_MODULE
	},
	.id_table = tuxedo_nb04_wmi_ab_device_ids,
	.probe = tuxedo_nb04_wmi_ab_probe,
	.remove = tuxedo_nb04_wmi_ab_remove,
};

module_wmi_driver(tuxedo_nb04_wmi_ab_driver);

MODULE_AUTHOR("TUXEDO Computers GmbH <tux@tuxedocomputers.com>");
MODULE_DESCRIPTION("Driver for NB04 WMI AB methods");
MODULE_VERSION("0.0.1");
MODULE_LICENSE("GPL");

MODULE_DEVICE_TABLE(wmi, tuxedo_nb04_wmi_ab_device_ids);
MODULE_ALIAS("wmi:" NB04_WMI_AB_GUID);
