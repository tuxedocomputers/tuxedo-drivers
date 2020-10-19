/*!
 * Copyright (c) 2020 TUXEDO Computers GmbH <tux@tuxedocomputers.com>
 *
 * This file is part of tuxedo-keyboard-ite.
 *
 * tuxedo-keyboard-ite is free software: you can redistribute it and/or modify
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

/*
 * Turn backlight off
 * 08 01 00 00 00 00 00 00
 * 
 * Set brightness only
 * 09 02 [brightness] 00 00 00 00 00
 * 
 * 
 * Most "special modes" uses seven colors that can be defined individually
 * Set color define
 * 14 00 [color define number] [red] [green] [blue] 00 00
 * 
 * [color define number]
 * 	1 -> 7
 * 
 * 
 * Set params
 * 08 02 [anim mode] [speed] [brightness] 08 [behaviour] 00
 * 
 * [anim mode]
 * 	02 breath
 * 	03 wave ([behaviour]: direction)
 * 	04 reactive ([behaviour]: key mode)
 * 	05 rainbow (no color set)
 * 	06 ripple ([behaviour]: key mode)
 * 	09 marquee
 * 	0a raindrop
 * 	0e aurora ([behaviour]: key mode)
 * 	11 spark ([behaviour]: key mode)
 * 
 * 	33 per key control, data in interrupt request
 * 
 * [speed]
 * 	0a -> 01
 * 
 * [brightness]
 * 	00 -> 32
 * 
 * [behaviour]
 * 	00 when not used, otherwise dependent on [anim mode]
 * 
 * [behaviour]: direction
 * 	01 left to right
 * 	02 right to left
 * 	03 bottom to top
 * 	04 top to bottom
 * 
 * [behaviour]: key mode
 * 	00 key press
 * 	01 auto
 */

#define pr_fmt(fmt) KBUILD_MODNAME ": " fmt
#include <linux/kernel.h>
#include <linux/module.h>
#include <linux/device.h>
#include <linux/hid.h>

// USB HID control data write size
#define HID_DATA_SIZE 8

// led_classdev names, default and max brightness
#define LED_MAX_BRIGHTNESS		0x32
#define ITE_8291_DEFAULT_BRIGHTNESS	0x00

struct ite8291_driver_data_t {
	struct hid_device *hid_dev;
};

static int ite8291_write_state(struct ite8291_driver_data_t *ite8291_driver_data)
{
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
	struct ite8291_driver_data_t *ite8291_driver_data;

	result = hid_parse(hdev);
	if (result) {
		pr_err("hid_parse failed\n");
		stop_hw(hdev);
		return result;
	}

	result = start_hw(hdev);
	if (result != 0)
		return result;

	ite8291_driver_data = devm_kzalloc(&hdev->dev, sizeof(*ite8291_driver_data), GFP_KERNEL);
	if (!ite8291_driver_data)
		return -ENOMEM;
	
	// TODO: Init driver data

	hid_set_drvdata(hdev, ite8291_driver_data);

	result = ite8291_write_state(ite8291_driver_data);
	if (result < 0)
		return result;

	return 0;
}

static void driver_remove_callb(struct hid_device *hdev)
{
	struct ite8291_driver_data_t *ite8291_driver_data = hid_get_drvdata(hdev);
	if (!IS_ERR_OR_NULL(ite8291_driver_data)) {
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
	struct ite8291_driver_data_t *ite8291_driver_data = hid_get_drvdata(hdev);
	pr_debug("driver resume\n");
	return ite8291_write_state(ite8291_driver_data);
}

static int driver_reset_resume_callb(struct hid_device *hdev)
{
	struct ite8291_driver_data_t *ite8291_driver_data = hid_get_drvdata(hdev);
	pr_debug("driver reset resume\n");
	return ite8291_write_state(ite8291_driver_data);
}
#endif

static const struct hid_device_id ite8291_device_table[] = {
	{ HID_USB_DEVICE(0x048d, 0xce00) },
	{ }
};
MODULE_DEVICE_TABLE(hid, ite8291_device_table);

static struct hid_driver ite8291_driver = {
	.name = KBUILD_MODNAME,
	.probe = driver_probe_callb,
	.remove = driver_remove_callb,
	.id_table = ite8291_device_table,
#ifdef CONFIG_PM
	.suspend = driver_suspend_callb,
	.resume = driver_resume_callb,
	.reset_resume = driver_reset_resume_callb
#endif
};
module_hid_driver(ite8291_driver);

MODULE_AUTHOR("TUXEDO Computers GmbH <tux@tuxedocomputers.com>");
MODULE_DESCRIPTION("Driver for ITE Device(8291) per-key RGB LED keyboard backlight.");
MODULE_VERSION("0.0.1");
MODULE_LICENSE("GPL");
