/*!
 * Copyright (c) 2020-2022 TUXEDO Computers GmbH <tux@tuxedocomputers.com>
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
 * All numbers in hex format
 * 
 * Turn off (same as "set params" described below). This is called with all other bytes set to zero when turned-off.
 * Control: 08 01 00 00 00 00 00 00
 * 
 * Set brightness only
 * Control: 09 02 [brightness] 00 00 00 00 00
 * 
 * Announce 64 byte data chunks (for "user mode" 0x33). To be followed by specified number on chunks on interrupt endpoint.
 * Control: 12 00 00 [nr of chunks] 00 00 00 00
 * 
 * [nr of chunks]
 * 	1 -> 8
 * 
 * Announce row data chunk (for "user mode" 0x33). To be followed by 64 bytes of row data on interrupt endpoint.
 * Control: 16 00 [row number] 00 00 00 00 00
 * 
 * [row number]
 * 	0 -> 5
 * 
 * 
 * Most "special modes" uses seven colors that can be defined individually
 * Set color define
 * Control: 14 00 [color define number] [red] [green] [blue] 00 00
 * 
 * [color define number]
 * 	1 -> 7
 * 
 * 
 * Set params
 * Control: 08 [power state] [anim mode] [speed] [brightness] 08 [behaviour] 00
 * 
 * [power state]
 * 	01 off
 * 	02 on
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
 * 	33 per key control, needs additional "data announcing", then data on interrupt endpoint
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

#define ITE8291_MAX_BRIGHTNESS		0x32
#define ITE8291_DEFAULT_BRIGHTNESS	((u8)(ITE8291_MAX_BRIGHTNESS * 0.7))

#define ITE8291_LEDS_PER_ROW_MAX	21
// Data length needs one byte (0x00) initial padding for the sending function
// and one byte (also seemingly 0x00) before the color data starts
#define ITE8291_ROW_DATA_PADDING	(1 + 1)
#define ITE8291_ROW_DATA_LENGTH		(ITE8291_ROW_DATA_PADDING + (ITE8291_LEDS_PER_ROW_MAX * 3))
#define ITE8291_NR_ROWS			6

#define ITE8291_PARAM_MODE_USER		0x33

typedef u8 row_data_t[ITE8291_NR_ROWS][ITE8291_ROW_DATA_LENGTH];

struct ite8291_driver_data_t {
	struct led_classdev cdev_brightness;
	struct hid_device *hid_dev;
	row_data_t row_data;
};

#if 0
/**
 * Set color for specified [row, column] in row based data structure
 * 
 * @param row_data Data structure to fill
 * @param row Row number 1 - 6
 * @param column Column number 1 - 21
 * @param red Red brightness 0x00 - 0xff
 * @param green Green brightness 0x00 - 0xff
 * @param blue Blue brightness 0x00 - 0xff
 * 
 * @returns 0 on success, otherwise error
 */
static int row_data_set(row_data_t row_data, int row, int column, u8 red, u8 green, u8 blue)
{
	int row_index, column_index_red, column_index_green, column_index_blue;

	if (row < 1 || row > ITE8291_NR_ROWS)
		return -EINVAL;
	
	if (column < 1 || column > ITE8291_LEDS_PER_ROW_MAX)
		return -EINVAL;

	row_index = row - 1;

	column_index_red = ITE8291_ROW_DATA_PADDING + (2 * ITE8291_LEDS_PER_ROW_MAX) + (column - 1);
	column_index_green = ITE8291_ROW_DATA_PADDING + (1 * ITE8291_LEDS_PER_ROW_MAX) + (column - 1);
	column_index_blue = ITE8291_ROW_DATA_PADDING + (0 * ITE8291_LEDS_PER_ROW_MAX) + (column - 1);
	
	row_data[row_index][column_index_red] = red;
	row_data[row_index][column_index_green] = green;
	row_data[row_index][column_index_blue] = blue;

	return 0;
}
#endif

/**
 * Set brightness only
 * 09 02 [brightness] 00 00 00 00 00
 */
static int ite8291_write_brightness(struct hid_device *hdev, u8 brightness)
{
	int result = 0;
	u8 *buf;
	if (hdev == NULL)
		return -ENODEV;

	buf = kzalloc(HID_DATA_SIZE, GFP_KERNEL);
	buf[0] = 0x09;
	buf[1] = 0x02;
	buf[2] = brightness;
	buf[3] = 0x00;
	buf[4] = 0x00;
	buf[5] = 0x00;
	buf[6] = 0x00;
	buf[7] = 0x00;

	result = hid_hw_raw_request(hdev, buf[0], buf, HID_DATA_SIZE,
				    HID_FEATURE_REPORT, HID_REQ_SET_REPORT);
	kfree(buf);

	return result;
}

/**
 * Write control data
 */
static int ite8291_write_control(struct hid_device *hdev, u8 *ctrl_data)
{
	int result = 0;
	u8 *buf;
	if (hdev == NULL)
		return -ENODEV;

	buf = kzalloc(HID_DATA_SIZE, GFP_KERNEL);

	memcpy(buf, ctrl_data, (size_t) 8);

	result = hid_hw_raw_request(hdev, buf[0], buf, HID_DATA_SIZE,
				    HID_FEATURE_REPORT, HID_REQ_SET_REPORT);
	kfree(buf);

	return result;
}

static int ite8291_write_off(struct hid_device *hdev)
{
	u8 ctrl_params_off[] = {0x08, 0x01, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00};
	return ite8291_write_control(hdev, ctrl_params_off);
}

/**
 * *experimental*
 * 
 * Write color (and brightness) to the whole keyboard (chunk-wise)
 */
static int ite8291_write_color_full(struct hid_device *hdev, u8 red, u8 green, u8 blue, u8 brightness)
{
	int result = 0, i, j;
	int nr_data_packets = 0x08;
	u8 ctrl_params[] = { 0x08,
			     0x02,
			     ITE8291_PARAM_MODE_USER,
			     0x00,
			     brightness % (ITE8291_MAX_BRIGHTNESS + 1),
			     0x00,
			     0x00,
			     0x00 };
	u8 ctrl_announce_data[] = { 0x12, 0x00, 0x00, (u8)nr_data_packets,
				    0x00, 0x00, 0x00, 0x00 };
	int data_packet_length = 65;
	u8 *data_buf;
	if (hdev == NULL)
		return -ENODEV;

	ite8291_write_control(hdev, ctrl_params);
	ite8291_write_control(hdev, ctrl_announce_data);

	data_buf = kzalloc(nr_data_packets * data_packet_length, GFP_KERNEL);
	if (!data_buf)
		return -ENOMEM;

	for (j = 0; j < nr_data_packets; ++j) {
		for (i = 1; i < data_packet_length; ++i) {
			if (((i + 2) % 4) == 0)
				data_buf[(j * data_packet_length) + i] = red;
			if (((i + 1) % 4) == 0)
				data_buf[(j * data_packet_length) + i] = green;
			if (((i) % 4) == 0)
				data_buf[(j * data_packet_length) + i] = blue;
		}
	}

	for (j = 0; j < nr_data_packets; ++j) {
		result = hdev->ll_driver->output_report(
			hdev, &data_buf[j * data_packet_length],
			data_packet_length);
		if (result < 0)
			return result;
	}

	kfree(data_buf);
	return result;
}

#if 0
/**
 * Write color (and brightness) to the whole keyboard from row data
 */
static int ite8291_write_rows(struct hid_device *hdev, row_data_t row_data, u8 brightness)
{
	int result = 0, row_index;
	u8 ctrl_params[] = { 0x08,
			     0x02,
			     ITE8291_PARAM_MODE_USER,
			     0x00,
			     brightness % (ITE8291_MAX_BRIGHTNESS + 1),
			     0x00,
			     0x00,
			     0x00 };
	u8 ctrl_announce_row_data[] = { 0x16, 0x00, 0x00, 0x00,
									0x00, 0x00, 0x00, 0x00 };
	if (hdev == NULL)
		return -ENODEV;

	ite8291_write_control(hdev, ctrl_params);

	for (row_index = 0; row_index < ITE8291_NR_ROWS; ++row_index) {
		ctrl_announce_row_data[2] = row_index;
		ite8291_write_control(hdev, ctrl_announce_row_data);
		result = hdev->ll_driver->output_report(
			hdev, row_data[row_index], ITE8291_ROW_DATA_LENGTH);
		if (result < 0)
			return result;
	}

	return result;
}
#endif

static int ite8291_write_state(struct ite8291_driver_data_t *ite8291_driver_data)
{
	ite8291_write_color_full(ite8291_driver_data->hid_dev, 0xff, 0x7e, 0x78, ite8291_driver_data->cdev_brightness.brightness);
	return 0;
}

static int ledcdev_set_blocking(struct led_classdev *led_cdev, enum led_brightness brightness)
{
	int status;
	struct ite8291_driver_data_t *ite8291_driver_data = container_of(led_cdev, struct ite8291_driver_data_t, cdev_brightness);
	status = ite8291_write_brightness(ite8291_driver_data->hid_dev, brightness);
	if (status == 0) {
		led_cdev->brightness = brightness;
	}
	return status;
}

static enum led_brightness ledcdev_get(struct led_classdev *led_cdev)
{
	return led_cdev->brightness;
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
	
	ite8291_driver_data->cdev_brightness.name = KBUILD_MODNAME "::kbd_backlight";
	ite8291_driver_data->cdev_brightness.max_brightness = ITE8291_MAX_BRIGHTNESS;
	ite8291_driver_data->cdev_brightness.brightness_set_blocking = &ledcdev_set_blocking;
	ite8291_driver_data->cdev_brightness.brightness_get = &ledcdev_get;
	ite8291_driver_data->cdev_brightness.brightness = ITE8291_DEFAULT_BRIGHTNESS;
	led_classdev_register(&hdev->dev, &ite8291_driver_data->cdev_brightness);

	ite8291_driver_data->hid_dev = hdev;

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
		led_classdev_unregister(&ite8291_driver_data->cdev_brightness);
	} else {
		pr_debug("driver data not found\n");
	}
	stop_hw(hdev);
	pr_debug("driver remove\n");
}

#ifdef CONFIG_PM
static int driver_suspend_callb(struct hid_device *hdev, pm_message_t message)
{
	ite8291_write_off(hdev);
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
	{ HID_USB_DEVICE(0x048d, 0x6004) },
	{ HID_USB_DEVICE(0x048d, 0x600a) },
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
MODULE_VERSION("0.0.3");
MODULE_LICENSE("GPL");
