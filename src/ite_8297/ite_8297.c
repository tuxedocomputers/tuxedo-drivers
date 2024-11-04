// SPDX-License-Identifier: GPL-3.0+
/*!
 * Copyright (c) 2020 TUXEDO Computers GmbH <tux@tuxedocomputers.com>
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
#include <linux/device.h>
#include <linux/hid.h>

// USB HID feature data write size
#define HID_DATA_SIZE 64

// led_classdev names, default and max brightness
#define LED_MAX_BRIGHTNESS		0xff
#define ITE_8297_DEFAULT_BRIGHTNESS	0x00
#define LED_NAME_RGB_RED		KBUILD_MODNAME ":1"
#define LED_NAME_RGB_GREEN		KBUILD_MODNAME ":2"
#define LED_NAME_RGB_BLUE		KBUILD_MODNAME ":3"

struct color_t {
	u8 red;
	u8 green;
	u8 blue;
};

struct ite8297_driver_data_t {
	struct led_classdev cdev_red;
	struct led_classdev cdev_green;
	struct led_classdev cdev_blue;
	struct hid_device *hid_dev;
	struct color_t current_color;
};

static int ite8297_write_color(struct hid_device *hdev, u8 red, u8 green, u8 blue)
{
	int result = 0;
	u8 *buf;
	if (hdev == NULL)
		return -ENODEV;

	buf = kzalloc(HID_DATA_SIZE, GFP_KERNEL);
	buf[0] = 0xcc;
	buf[1] = 0xb0;
	buf[2] = 0x01;
	buf[3] = 0x01;
	buf[4] = red;
	buf[5] = green;
	buf[6] = blue;

	result = hid_hw_raw_request(hdev, buf[0], buf, HID_DATA_SIZE,
				    HID_FEATURE_REPORT, HID_REQ_SET_REPORT);
	kfree(buf);

	return result;
}

static int ite8297_write_state(struct ite8297_driver_data_t *ite8297_driver_data)
{
	return ite8297_write_color(ite8297_driver_data->hid_dev,
				   ite8297_driver_data->current_color.red,
				   ite8297_driver_data->current_color.green,
				   ite8297_driver_data->current_color.blue);
}

static int lightbar_set_blocking(struct led_classdev *led_cdev, enum led_brightness brightness)
{
	bool led_red = strstr(led_cdev->name, LED_NAME_RGB_RED) != NULL;
	bool led_green = strstr(led_cdev->name, LED_NAME_RGB_GREEN) != NULL;
	bool led_blue = strstr(led_cdev->name, LED_NAME_RGB_BLUE) != NULL;
	struct ite8297_driver_data_t *ite8297_driver_data;

	if (led_red) {
		ite8297_driver_data = container_of(led_cdev, struct ite8297_driver_data_t, cdev_red);
		ite8297_driver_data->current_color.red = brightness;
	} else if (led_green) {
		ite8297_driver_data = container_of(led_cdev, struct ite8297_driver_data_t, cdev_green);
		ite8297_driver_data->current_color.green = brightness;
	} else if (led_blue) {
		ite8297_driver_data = container_of(led_cdev, struct ite8297_driver_data_t, cdev_blue);
		ite8297_driver_data->current_color.blue = brightness;
	}
	ite8297_write_state(ite8297_driver_data);

	return 0;
}

static enum led_brightness lightbar_get(struct led_classdev *led_cdev)
{
	bool led_red = strstr(led_cdev->name, LED_NAME_RGB_RED) != NULL;
	bool led_green = strstr(led_cdev->name, LED_NAME_RGB_GREEN) != NULL;
	bool led_blue = strstr(led_cdev->name, LED_NAME_RGB_BLUE) != NULL;
	struct ite8297_driver_data_t *ite8297_driver_data;

	if (led_red) {
		ite8297_driver_data = container_of(led_cdev, struct ite8297_driver_data_t, cdev_red);
		return ite8297_driver_data->current_color.red;
	} else if (led_green) {
		ite8297_driver_data = container_of(led_cdev, struct ite8297_driver_data_t, cdev_green);
		return ite8297_driver_data->current_color.green;
	} else if (led_blue) {
		ite8297_driver_data = container_of(led_cdev, struct ite8297_driver_data_t, cdev_blue);
		return ite8297_driver_data->current_color.blue;
	}

	return 0;
}

static void stop_hw(struct hid_device *hdev)
{
	hid_hw_power(hdev, PM_HINT_NORMAL);
	hid_hw_close(hdev);
	hid_hw_stop(hdev);
}

static int start_hw(struct hid_device *hdev)
{
	int result;
	result = hid_hw_start(hdev, HID_CONNECT_DEFAULT);
	if (result) {
		pr_err("hid_hw_start failed\n");
		goto err_stop_hw;
	}

	hid_hw_power(hdev, PM_HINT_FULLON);

	result = hid_hw_open(hdev);
	if (result) {
		pr_err("hid_hw_open failed\n");
		goto err_stop_hw;
	}

	return 0;

err_stop_hw:
	stop_hw(hdev);
	return result;
}

static int driver_probe_callb(struct hid_device *hdev, const struct hid_device_id *id)
{
	int result;
	struct ite8297_driver_data_t *ite8297_driver_data;

	result = hid_parse(hdev);
	if (result) {
		pr_err("hid_parse failed\n");
		stop_hw(hdev);
		return result;
	}

	result = start_hw(hdev);
	if (result != 0)
		return result;

	ite8297_driver_data = devm_kzalloc(&hdev->dev, sizeof(*ite8297_driver_data), GFP_KERNEL);
	if (!ite8297_driver_data)
		return -ENOMEM;

	ite8297_driver_data->cdev_red.name = LED_NAME_RGB_RED;
	ite8297_driver_data->cdev_red.max_brightness = LED_MAX_BRIGHTNESS;
	ite8297_driver_data->cdev_red.brightness_set_blocking = &lightbar_set_blocking;
	ite8297_driver_data->cdev_red.brightness_get = &lightbar_get;

	ite8297_driver_data->cdev_green.name = LED_NAME_RGB_GREEN;
	ite8297_driver_data->cdev_green.max_brightness = LED_MAX_BRIGHTNESS;
	ite8297_driver_data->cdev_green.brightness_set_blocking = &lightbar_set_blocking;
	ite8297_driver_data->cdev_green.brightness_get = &lightbar_get;

	ite8297_driver_data->cdev_blue.name = LED_NAME_RGB_BLUE;
	ite8297_driver_data->cdev_blue.max_brightness = LED_MAX_BRIGHTNESS;
	ite8297_driver_data->cdev_blue.brightness_set_blocking = &lightbar_set_blocking;
	ite8297_driver_data->cdev_blue.brightness_get = &lightbar_get;

	ite8297_driver_data->hid_dev = hdev;
	ite8297_driver_data->current_color.red = ITE_8297_DEFAULT_BRIGHTNESS;
	ite8297_driver_data->current_color.green = ITE_8297_DEFAULT_BRIGHTNESS;
	ite8297_driver_data->current_color.blue = ITE_8297_DEFAULT_BRIGHTNESS;

	led_classdev_register(&hdev->dev, &ite8297_driver_data->cdev_red);
	led_classdev_register(&hdev->dev, &ite8297_driver_data->cdev_green);
	led_classdev_register(&hdev->dev, &ite8297_driver_data->cdev_blue);

	hid_set_drvdata(hdev, ite8297_driver_data);

	result = ite8297_write_state(ite8297_driver_data);
	if (result < 0)
		return result;

	return 0;
}

static void driver_remove_callb(struct hid_device *hdev)
{
	struct ite8297_driver_data_t *ite8297_driver_data = hid_get_drvdata(hdev);
	if (!IS_ERR_OR_NULL(ite8297_driver_data)) {
		led_classdev_unregister(&ite8297_driver_data->cdev_red);
		led_classdev_unregister(&ite8297_driver_data->cdev_green);
		led_classdev_unregister(&ite8297_driver_data->cdev_blue);
	} else {
		pr_debug("driver data not found\n");
	}
	stop_hw(hdev);
	pr_debug("driver remove\n");
}

#ifdef CONFIG_PM
static int driver_suspend_callb(struct hid_device *hdev, pm_message_t message)
{
	pr_debug("driver suspend\n");
	return 0;
}

static int driver_resume_callb(struct hid_device *hdev)
{
	struct ite8297_driver_data_t *ite8297_driver_data = hid_get_drvdata(hdev);
	pr_debug("driver resume\n");
	return ite8297_write_state(ite8297_driver_data);
}

static int driver_reset_resume_callb(struct hid_device *hdev)
{
	struct ite8297_driver_data_t *ite8297_driver_data = hid_get_drvdata(hdev);
	pr_debug("driver reset resume\n");
	return ite8297_write_state(ite8297_driver_data);
}
#endif

static const struct hid_device_id ite8297_device_table[] = {
	{ HID_USB_DEVICE(0x048d, 0x8297) },
	{ }
};
MODULE_DEVICE_TABLE(hid, ite8297_device_table);

static struct hid_driver ite8297_driver = {
	.name = KBUILD_MODNAME,
	.probe = driver_probe_callb,
	.remove = driver_remove_callb,
	.id_table = ite8297_device_table,
#ifdef CONFIG_PM
	.suspend = driver_suspend_callb,
	.resume = driver_resume_callb,
	.reset_resume = driver_reset_resume_callb
#endif
};
module_hid_driver(ite8297_driver);

MODULE_AUTHOR("TUXEDO Computers GmbH <tux@tuxedocomputers.com>");
MODULE_DESCRIPTION("Driver for IT8297 RGB LED Controller");
MODULE_LICENSE("GPL v3");
