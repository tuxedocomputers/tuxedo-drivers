/*!
 * Copyright (c) 2024 LWL Computers GmbH <tux@lwlcomputers.com>
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
#include <linux/module.h>
#include <linux/platform_device.h>
#include <linux/leds.h>
#include <linux/version.h>
#include "lwl_nb05_kbd_backlight.h"
#include "lwl_nb05_ec.h"

#define NB05_KBD_BRIGHTNESS_MAX_WHITE		0x02
#define NB05_KBD_BRIGHTNESS_DEFAULT_WHITE	0x00

static struct led_classdev *__nb05_kbd_led_cdev = NULL;

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
	if (brightness < 0 || brightness > NB05_KBD_BRIGHTNESS_MAX_WHITE)
		return;

	nb05_write_ec_ram(0x0409, white_brightness_to_level_map[brightness]);
}

void nb05_leds_notify_brightness_change_extern(u8 step)
{
	if (step > NB05_KBD_BRIGHTNESS_MAX_WHITE)
		return;

	if (__nb05_kbd_led_cdev) {
		__nb05_kbd_led_cdev->brightness = step;
		led_classdev_notify_brightness_hw_changed(__nb05_kbd_led_cdev, step);
	}
}
EXPORT_SYMBOL(nb05_leds_notify_brightness_change_extern);

static int init_leds(struct platform_device *pdev)
{
	struct driver_data_t *driver_data = dev_get_drvdata(&pdev->dev);
	int retval;

	driver_data->nb05_kbd_led_cdev.name = "white:" LED_FUNCTION_KBD_BACKLIGHT;
	driver_data->nb05_kbd_led_cdev.max_brightness = NB05_KBD_BRIGHTNESS_MAX_WHITE;
	driver_data->nb05_kbd_led_cdev.brightness_set = &nb05_leds_set_brightness;
	driver_data->nb05_kbd_led_cdev.brightness = NB05_KBD_BRIGHTNESS_DEFAULT_WHITE;
	driver_data->nb05_kbd_led_cdev.flags = LED_BRIGHT_HW_CHANGED;

	retval = led_classdev_register(&pdev->dev, &driver_data->nb05_kbd_led_cdev);
	if (retval)
		return retval;

	__nb05_kbd_led_cdev = &driver_data->nb05_kbd_led_cdev;

	return 0;
}

static int __init lwl_nb05_kbd_backlight_probe(struct platform_device *pdev)
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

#if LINUX_VERSION_CODE < KERNEL_VERSION(6, 11, 0)
static int lwl_nb05_kbd_backlight_remove(struct platform_device *pdev)
#else
static void lwl_nb05_kbd_backlight_remove(struct platform_device *pdev)
#endif
{
	struct driver_data_t *driver_data = dev_get_drvdata(&pdev->dev);
	led_classdev_unregister(&driver_data->nb05_kbd_led_cdev);
	__nb05_kbd_led_cdev = NULL;
	pr_debug("driver remove\n");
#if LINUX_VERSION_CODE < KERNEL_VERSION(6, 11, 0)
	return 0;
#endif
}

static struct platform_device *lwl_nb05_kbd_backlight_device;
static struct platform_driver lwl_nb05_kbd_backlight_driver = {
	.driver.name = "lwl_nb05_kbd_backlight",
	.remove = lwl_nb05_kbd_backlight_remove
};

static int __init lwl_nb05_kbd_backlight_init(void)
{
	lwl_nb05_kbd_backlight_device =
		platform_create_bundle(&lwl_nb05_kbd_backlight_driver,
				       lwl_nb05_kbd_backlight_probe, NULL, 0, NULL, 0);

	if (IS_ERR(lwl_nb05_kbd_backlight_device))
		return PTR_ERR(lwl_nb05_kbd_backlight_device);

	return 0;
}

static void __exit lwl_nb05_kbd_backlight_exit(void)
{
	platform_device_unregister(lwl_nb05_kbd_backlight_device);
	platform_driver_unregister(&lwl_nb05_kbd_backlight_driver);
}

module_init(lwl_nb05_kbd_backlight_init);
module_exit(lwl_nb05_kbd_backlight_exit);

MODULE_AUTHOR("LWL Computers GmbH <tux@lwlcomputers.com>");
MODULE_DESCRIPTION("Driver for NB05 keyboard backlight");
MODULE_LICENSE("GPL");
