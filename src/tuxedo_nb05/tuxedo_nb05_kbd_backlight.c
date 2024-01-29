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
#include <linux/module.h>
#include <linux/platform_device.h>
#include <linux/leds.h>
#include <linux/version.h>
#include "tuxedo_nb05_ec.h"

#define NB05_KBD_BRIGHTNESS_MAX_WHITE		0x02
#define NB05_KBD_BRIGHTNESS_DEFAULT_WHITE	0x00

static u8 white_brightness_to_level_map[] = {
	0x00,
	0x5c,
	0xb8,
};

struct driver_data_t {
	struct led_classdev nb05_kbd_led_cdev;
};

static void nb05_leds_set_brightness(struct led_classdev *led_cdev __always_unused,
				     enum led_brightness brightness)
{
	if (brightness < 0 || brightness > 2)
		return;

	nb05_write_ec_ram(0x0409, white_brightness_to_level_map[brightness]);
}

static int init_leds(struct platform_device *pdev)
{
	struct driver_data_t *driver_data = dev_get_drvdata(&pdev->dev);
	int retval;

	driver_data->nb05_kbd_led_cdev.name = "white:" LED_FUNCTION_KBD_BACKLIGHT;
	driver_data->nb05_kbd_led_cdev.max_brightness = NB05_KBD_BRIGHTNESS_MAX_WHITE;
	driver_data->nb05_kbd_led_cdev.brightness_set = &nb05_leds_set_brightness;
	driver_data->nb05_kbd_led_cdev.brightness = NB05_KBD_BRIGHTNESS_DEFAULT_WHITE;

	retval = led_classdev_register(&pdev->dev, &driver_data->nb05_kbd_led_cdev);
	if (retval)
		return retval;

	return 0;
}

static int __init tuxedo_nb05_kbd_backlight_probe(struct platform_device *pdev)
{
	int result;
	struct driver_data_t *driver_data;

	pr_debug("driver probe\n");

	driver_data = devm_kzalloc(&pdev->dev, sizeof(struct driver_data_t), GFP_KERNEL);
	if (!driver_data)
		return -ENOMEM;

	dev_set_drvdata(&pdev->dev, driver_data);

	if (result) {
		pr_err("Failed init write %d\n", result);
		return result;
	}

	result = init_leds(pdev);
	if (result)
		return result;

	return 0;
}

static int tuxedo_nb05_kbd_backlight_remove(struct platform_device *pdev)
{
	struct driver_data_t *driver_data = dev_get_drvdata(&pdev->dev);
	led_classdev_unregister(&driver_data->nb05_kbd_led_cdev);
	pr_debug("driver remove\n");
	return 0;
}

static struct platform_device *tuxedo_nb05_kbd_backlight_device;
static struct platform_driver tuxedo_nb05_kbd_backlight_driver = {
	.driver.name = "tuxedo_nb05_kbd_backlight",
	.remove = tuxedo_nb05_kbd_backlight_remove
};

static int __init tuxedo_nb05_kbd_backlight_init(void)
{
	tuxedo_nb05_kbd_backlight_device =
		platform_create_bundle(&tuxedo_nb05_kbd_backlight_driver,
				       tuxedo_nb05_kbd_backlight_probe, NULL, 0, NULL, 0);

	if (IS_ERR(tuxedo_nb05_kbd_backlight_device))
		return PTR_ERR(tuxedo_nb05_kbd_backlight_device);

	return 0;
}

static void __exit tuxedo_nb05_kbd_backlight_exit(void)
{
	platform_device_unregister(tuxedo_nb05_kbd_backlight_device);
	platform_driver_unregister(&tuxedo_nb05_kbd_backlight_driver);
}

module_init(tuxedo_nb05_kbd_backlight_init);
module_exit(tuxedo_nb05_kbd_backlight_exit);

MODULE_AUTHOR("TUXEDO Computers GmbH <tux@tuxedocomputers.com>");
MODULE_DESCRIPTION("Driver for NB05 keyboard backlight");
MODULE_VERSION("0.0.1");
MODULE_LICENSE("GPL");
