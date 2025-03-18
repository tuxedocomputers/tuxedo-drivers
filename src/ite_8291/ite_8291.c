// SPDX-License-Identifier: GPL-2.0+
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
#include <linux/version.h>
#include <linux/module.h>
#include <linux/device.h>
#include <linux/usb.h>
#include <linux/hid.h>
#include <linux/dmi.h>
#include <linux/led-class-multicolor.h>
#include <linux/of.h>

// USB HID control data write size
#define HID_DATA_SIZE 8

// led_classdev names, default and max brightness
//#define ITE8291_KBD_BRIGHTNESS_MAX	0xff
#define ITE8291_KBD_BRIGHTNESS_MAX	0x32
#define ITE8291_KBD_BRIGHTNESS_DEFAULT	0x00

#define ITE8291_KB_COLOR_DEFAULT_RED	0xff
#define ITE8291_KB_COLOR_DEFAULT_GREEN	0xff
#define ITE8291_KB_COLOR_DEFAULT_BLUE	0xff
#define ITE8291_KB_COLOR_DEFAULT	((UNIWILL_KB_COLOR_DEFAULT_RED << 16) + (UNIWILL_KB_COLOR_DEFAULT_GREEN << 8) + UNIWILL_KB_COLOR_DEFAULT_BLUE)

#define ITE8291_LEDS_PER_ROW_MAX	21
// Data length needs one byte (0x00) initial padding for the sending function
// and one byte (also seemingly 0x00) before the color data starts
#define ITE8291_ROW_DATA_PADDING	(1 + 1)
#define ITE8291_ROW_DATA_LENGTH		(ITE8291_ROW_DATA_PADDING + (ITE8291_LEDS_PER_ROW_MAX * 3))
#define ITE8291_NR_ROWS			6

#define ITE8291_PARAM_MODE_USER		0x33

struct ite8291_driver_data_t {
	u16 bcd_device;
	struct hid_device *hid_dev;
	void *device_data;
	bool device_has_buffer_input_control;
	bool device_buffer_input;
	int (*device_add)(struct hid_device *);
	int (*device_remove)(struct hid_device *);
	int (*device_write_on)(struct hid_device *);
	int (*device_write_off)(struct hid_device *);
	int (*device_write_state)(struct hid_device *);
};

// Per key device specific defines
typedef u8 row_data_t[ITE8291_NR_ROWS][ITE8291_ROW_DATA_LENGTH];
struct ite8291_driver_data_perkey_t {
	row_data_t row_data;
	u8 brightness;
	struct led_classdev_mc mcled_cdevs[ITE8291_NR_ROWS][ITE8291_LEDS_PER_ROW_MAX];
	struct mc_subled mcled_cdevs_subleds[ITE8291_NR_ROWS][ITE8291_LEDS_PER_ROW_MAX][3];
};

static int ite8291_perkey_add(struct hid_device *);
static int ite8291_perkey_remove(struct hid_device *);
static int ite8291_perkey_write_on(struct hid_device *);
static int ite8291_perkey_write_off(struct hid_device *);
static int ite8291_perkey_write_state(struct hid_device *);

// Zones device specific defines
#define ITE8291_NR_ZONES 			4
#define ITE8291_KBD_ZONES_BRIGHTNESS_MAX	0x32
#define ITE8291_KBD_ZONES_BRIGHTNESS_DEFAULT	0x00
struct ite8291_driver_data_zones_t {
	u8 brightness;
	struct led_classdev_mc mcled_cdevs[ITE8291_NR_ZONES];
	struct mc_subled mcled_cdevs_subleds[ITE8291_NR_ZONES][3];
};

static int ite8291_zones_add(struct hid_device *);
static int ite8291_zones_remove(struct hid_device *);
static int ite8291_zones_write_on(struct hid_device *);
static int ite8291_zones_write_off(struct hid_device *);
static int ite8291_zones_write_state(struct hid_device *);

/**
 * Color scaling quirk list
 */
static void color_scaling(struct hid_device *hdev, u8 *red, u8 *green, u8 *blue, bool row_col_set, u8 row, u8 col)
{
	struct ite8291_driver_data_t *driver_data = hid_get_drvdata(hdev);
#if LINUX_VERSION_CODE >= KERNEL_VERSION(4, 18, 0)
	if (dmi_match(DMI_PRODUCT_SKU, "STEPOL1XA04") && hdev->product == 0x600a) {
		*red = (126 * *red) / 255;
	} else if (dmi_match(DMI_PRODUCT_SKU, "STELLARIS1XI05") && hdev->product == 0x600a) {
		*red = (200 * *red) / 255;
		*blue = (220 * *blue) / 255;

		// top row: reduce some additional violett
		if(row_col_set && row == 5) {
			*red = ( 230 * *red) / 255;
			*blue = ( 200 * *blue) / 255;
		}
	} else if (dmi_match(DMI_PRODUCT_SKU, "STELLARIS1XA05")) {
		*red = (128 * *red) / 255;
	} else if (dmi_match(DMI_PRODUCT_SKU, "STELLARIS1XI05") &&
		   hdev->product == 0xce00 && driver_data->bcd_device == 0x0002) {
		*red = (255 * *red) / 255;
		*green = (220 * *green) / 255;
		*blue = (200 * *blue) / 255;
	} else if (dmi_match(DMI_PRODUCT_SKU, "STELLARIS17I06") &&
		   hdev->product == 0xce00 && driver_data->bcd_device == 0x0002) {
		*green = (180 * *green) / 255;
		*blue = (180 * *blue) / 255;
	} else if (dmi_match(DMI_PRODUCT_SKU, "STELLARIS17I06") &&
		   hdev->product == 0x600a) {
		*red = (200 * *red) / 255;
		*blue = (220 * *blue) / 255;
	} else if ((dmi_match(DMI_PRODUCT_SKU, "STELLSL15I06") || dmi_match(DMI_PRODUCT_SKU, "STELLARIS16I06"))
		   && hdev->product == 0x600b) {
		// all keys: reduce pink
		*red = (155 * *red) / 255;
		*blue = (140 * *blue) / 255;

		// bottom row: reduce green (adding red and blue)
		if (row_col_set && row == 0) {
			*red = (279 * *red) / 255;
			*blue = (282 * *blue) / 255;
		}

		// top row: reduce violett
		if(row_col_set && row == 5) {
			*red = (148 * *red) / 255;
			*blue = (137 * *blue) / 255;
		}
	} else if (dmi_match(DMI_PRODUCT_SKU, "STELLARIS16I07")
			&& hdev->product == 0x600b) {
		// all keys: reduce pink
		*red = (155 * *red) / 255;
		*blue = (180 * *blue) / 255;
		*green = (220 * *green) / 255;
	} else {
		*green = (126 * *green) / 255;
		*blue = (120 * *blue) / 255;
	}
#endif
}

/**
 * Set color for specified [row, column] in row based data structure
 * 
 * @param row_data Data structure to fill
 * @param row Row number 0 - 5
 * @param column Column number 0 - 20
 * @param red Red brightness 0x00 - 0xff
 * @param green Green brightness 0x00 - 0xff
 * @param blue Blue brightness 0x00 - 0xff
 * 
 * @returns 0 on success, otherwise error
 */
static int row_data_set(struct hid_device *hdev, row_data_t row_data, int row, int column, u8 red, u8 green, u8 blue)
{
	int column_index_red, column_index_green, column_index_blue;

	color_scaling(hdev, &red, &green, &blue, true, row, column);

	if (row < 0 || row >= ITE8291_NR_ROWS)
		return -EINVAL;

	if (column < 0 || column >= ITE8291_LEDS_PER_ROW_MAX)
		return -EINVAL;

	column_index_red = ITE8291_ROW_DATA_PADDING + (2 * ITE8291_LEDS_PER_ROW_MAX) + column;
	column_index_green = ITE8291_ROW_DATA_PADDING + (1 * ITE8291_LEDS_PER_ROW_MAX) + column;
	column_index_blue = ITE8291_ROW_DATA_PADDING + (0 * ITE8291_LEDS_PER_ROW_MAX) + column;

	row_data[row][column_index_red] = red;
	row_data[row][column_index_green] = green;
	row_data[row][column_index_blue] = blue;

	return 0;
}

/**
 * Set brightness only
 * 09 02 [brightness] 00 00 00 00 00
 */
/*
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
*/

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

#if 0
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
			     brightness % (ITE8291_KBD_BRIGHTNESS_MAX + 1),
			     0x00,
			     0x00,
			     0x00 };
	u8 ctrl_announce_data[] = { 0x12, 0x00, 0x00, (u8)nr_data_packets,
				    0x00, 0x00, 0x00, 0x00 };
	int data_packet_length = 65;
	u8 *data_buf;
	if (hdev == NULL)
		return -ENODEV;

	color_scaling(hdev, &red, &green, &blue);

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
#endif

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
			     brightness > ITE8291_KBD_BRIGHTNESS_MAX ? ITE8291_KBD_BRIGHTNESS_MAX : brightness,
			     0x00,
			     0x00,
			     0x00 };
	u8 ctrl_announce_row_data[] = { 0x16, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00 };
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
	if (result > 0)
		result = 0;

	return result;
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

/*
static void leds_set_brightness_mc (struct led_classdev *led_cdev, enum led_brightness brightness) {
	struct led_classdev_mc *mcled_cdev = lcdev_to_mccdev(led_cdev);
	struct device *dev = led_cdev->dev->parent;
	struct hid_device *hdev = to_hid_device(dev);
	struct ite8291_driver_data_t *ite8291_driver_data = hid_get_drvdata(hdev);

	led_mc_calc_color_components(mcled_cdev, brightness);
	row_data_set(hdev, ite8291_driver_data->row_data, mcled_cdev->subled_info[0].channel / ITE8291_LEDS_PER_ROW_MAX,
		     mcled_cdev->subled_info[0].channel % ITE8291_LEDS_PER_ROW_MAX,
		     mcled_cdev->subled_info[0].brightness, mcled_cdev->subled_info[1].brightness,
		     mcled_cdev->subled_info[2].brightness);

	ite8291_write_state(ite8291_driver_data);
}
*/

static void leds_set_brightness_mc (struct led_classdev *led_cdev, enum led_brightness brightness) {
	int i, j;
	struct led_classdev_mc *mcled_cdev = lcdev_to_mccdev(led_cdev);
	struct device *dev = led_cdev->dev->parent;
	struct hid_device *hdev = to_hid_device(dev);
	struct ite8291_driver_data_t *ite8291_driver_data = hid_get_drvdata(hdev);
	struct ite8291_driver_data_perkey_t *device_data = ite8291_driver_data->device_data;

	pr_debug("leds_set_brightness_mc: channel: %d, brightness: %d, saved brightness: %d, red: %d, green: %d, blue: %d\n",
		 mcled_cdev->subled_info[0].channel, brightness, device_data->brightness, mcled_cdev->subled_info[0].intensity,
		 mcled_cdev->subled_info[1].intensity, mcled_cdev->subled_info[2].intensity);

	device_data->brightness = brightness;

	for (i = 0; i < ITE8291_NR_ROWS; ++i) {
		for (j = 0; j < ITE8291_LEDS_PER_ROW_MAX; ++j) {
			device_data->mcled_cdevs[i][j].led_cdev.brightness = brightness;
		}
	}

	row_data_set(hdev, device_data->row_data, mcled_cdev->subled_info[0].channel / ITE8291_LEDS_PER_ROW_MAX,
		     mcled_cdev->subled_info[0].channel % ITE8291_LEDS_PER_ROW_MAX,
		     mcled_cdev->subled_info[0].intensity, mcled_cdev->subled_info[1].intensity,
		     mcled_cdev->subled_info[2].intensity);

	if (!ite8291_driver_data->device_buffer_input)
		ite8291_perkey_write_state(hdev);
}

static int register_leds(struct hid_device *hdev)
{
	int res, i, j, k, l;
	struct ite8291_driver_data_t *ite8291_driver_data = hid_get_drvdata(hdev);
	struct ite8291_driver_data_perkey_t *device_data = ite8291_driver_data->device_data;

	for (i = 0; i < ITE8291_NR_ROWS; ++i) {
		for (j = 0; j < ITE8291_LEDS_PER_ROW_MAX; ++j) {
			device_data->mcled_cdevs[i][j].led_cdev.name = "rgb:" LED_FUNCTION_KBD_BACKLIGHT;
			device_data->mcled_cdevs[i][j].led_cdev.max_brightness = ITE8291_KBD_BRIGHTNESS_MAX;
			device_data->mcled_cdevs[i][j].led_cdev.brightness_set = &leds_set_brightness_mc;
			device_data->mcled_cdevs[i][j].led_cdev.brightness = ITE8291_KBD_BRIGHTNESS_DEFAULT;
			device_data->mcled_cdevs[i][j].num_colors = 3;
			device_data->mcled_cdevs[i][j].subled_info = device_data->mcled_cdevs_subleds[i][j];
			device_data->mcled_cdevs[i][j].subled_info[0].color_index = LED_COLOR_ID_RED;
			device_data->mcled_cdevs[i][j].subled_info[0].intensity = ITE8291_KB_COLOR_DEFAULT_RED;
			device_data->mcled_cdevs[i][j].subled_info[0].channel = ITE8291_LEDS_PER_ROW_MAX * i + j;
			device_data->mcled_cdevs[i][j].subled_info[1].color_index = LED_COLOR_ID_GREEN;
			device_data->mcled_cdevs[i][j].subled_info[1].intensity = ITE8291_KB_COLOR_DEFAULT_GREEN;
			device_data->mcled_cdevs[i][j].subled_info[1].channel = ITE8291_LEDS_PER_ROW_MAX * i + j;
			device_data->mcled_cdevs[i][j].subled_info[2].color_index = LED_COLOR_ID_BLUE;
			device_data->mcled_cdevs[i][j].subled_info[2].intensity = ITE8291_KB_COLOR_DEFAULT_BLUE;
			device_data->mcled_cdevs[i][j].subled_info[2].channel = ITE8291_LEDS_PER_ROW_MAX * i + j;

			res = devm_led_classdev_multicolor_register(&hdev->dev, &device_data->mcled_cdevs[i][j]);
			if (res) {
				for (k = 0; k <= i; ++k) {
					for (l = 0; l <= j; ++l) {
						devm_led_classdev_multicolor_unregister(&hdev->dev, &device_data->mcled_cdevs[i][j]);
					}
				}
				return res;
			}
		}
	}
	return 0;
}

static void unregister_leds(struct hid_device *hdev) {
	int i, j;
	struct ite8291_driver_data_t *ite8291_driver_data = hid_get_drvdata(hdev);
	struct ite8291_driver_data_perkey_t *device_data = ite8291_driver_data->device_data;

	for (i = 0; i < ITE8291_NR_ROWS; ++i) {
		for (j = 0; j < ITE8291_LEDS_PER_ROW_MAX; ++j) {
			devm_led_classdev_multicolor_unregister(&hdev->dev, &device_data->mcled_cdevs[i][j]);
		}
	}
}

static int ite8291_perkey_add(struct hid_device *hdev)
{
	struct ite8291_driver_data_t *driver_data;
	struct ite8291_driver_data_perkey_t *perkey_data;
	int result, i, j;

	driver_data = hid_get_drvdata(hdev);

	perkey_data = devm_kzalloc(&hdev->dev, sizeof(*perkey_data), GFP_KERNEL);
	if (!perkey_data)
		return -ENOMEM;

	driver_data->device_data = perkey_data;

	perkey_data->brightness = ITE8291_KBD_BRIGHTNESS_DEFAULT;
	for (i = 0; i < ITE8291_NR_ROWS; ++i) {
		for (j = 0; j < ITE8291_LEDS_PER_ROW_MAX; ++j) {
			row_data_set(hdev, perkey_data->row_data, i, j,
				     ITE8291_KB_COLOR_DEFAULT_RED,
				     ITE8291_KB_COLOR_DEFAULT_GREEN,
				     ITE8291_KB_COLOR_DEFAULT_BLUE);
		}
	}

	/*
	for (i = 0; i < ITE8291_NR_ROWS; ++i) {
		for (j = 0; j < ITE8291_LEDS_PER_ROW_MAX; ++j) {
			row_data_set(hdev, ite8291_driver_data->row_data, i, j,
				     ITE8291_KB_COLOR_DEFAULT_RED * ITE8291_KBD_BRIGHTNESS_DEFAULT / ITE8291_KBD_BRIGHTNESS_MAX,
				     ITE8291_KB_COLOR_DEFAULT_GREEN * ITE8291_KBD_BRIGHTNESS_DEFAULT / ITE8291_KBD_BRIGHTNESS_MAX,
				     ITE8291_KB_COLOR_DEFAULT_BLUE * ITE8291_KBD_BRIGHTNESS_DEFAULT / ITE8291_KBD_BRIGHTNESS_MAX);
		}
	}
	*/

	result = register_leds(hdev);
	if (result)
		return result;

	return 0;
}

static int ite8291_perkey_remove(struct hid_device *hdev)
{
	unregister_leds(hdev);
	return 0;
}

static int ite8291_perkey_write_on(struct hid_device *hdev)
{
	return 0;
}

static int ite8291_perkey_write_off(struct hid_device *hdev)
{
	u8 ctrl_params_off[] = {0x08, 0x01, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00};
	return ite8291_write_control(hdev, ctrl_params_off);
}

static int ite8291_perkey_write_state(struct hid_device *hdev)
{
	struct ite8291_driver_data_t *driver_data = hid_get_drvdata(hdev);
	struct ite8291_driver_data_perkey_t *device_data = driver_data->device_data;
	return ite8291_write_rows(hdev, device_data->row_data, device_data->brightness);
}

static void leds_zones_set_brightness_mc(struct led_classdev *led_cdev, enum led_brightness brightness) {
	int i;
	struct led_classdev_mc *mcled_cdev = lcdev_to_mccdev(led_cdev);
	struct device *dev = led_cdev->dev->parent;
	struct hid_device *hdev = to_hid_device(dev);
	struct ite8291_driver_data_t *driver_data = hid_get_drvdata(hdev);
	struct ite8291_driver_data_zones_t *device_data = driver_data->device_data;

	pr_debug("leds_set_brightness_mc: channel: %d, brightness: %d, saved brightness: %d, red: %d, green: %d, blue: %d\n",
		 mcled_cdev->subled_info[0].channel, brightness, device_data->brightness, mcled_cdev->subled_info[0].intensity,
		 mcled_cdev->subled_info[1].intensity, mcled_cdev->subled_info[2].intensity);

	device_data->brightness = brightness;

	for (i = 0; i < ITE8291_NR_ZONES; ++i) {
		device_data->mcled_cdevs[i].led_cdev.brightness = brightness;
	}

	ite8291_zones_write_state(hdev);
}

static int ite8291_zones_add(struct hid_device *hdev)
{
	struct ite8291_driver_data_t *driver_data;
	struct ite8291_driver_data_zones_t *zones_data;
	int result, i, k;

	driver_data = hid_get_drvdata(hdev);

	zones_data = devm_kzalloc(&hdev->dev, sizeof(*zones_data), GFP_KERNEL);
	if (!zones_data)
		return -ENOMEM;

	driver_data->device_data = zones_data;

	for (i = 0; i < ITE8291_NR_ZONES; ++i) {
		zones_data->mcled_cdevs[i].led_cdev.name = "rgb:" LED_FUNCTION_KBD_BACKLIGHT;
		zones_data->mcled_cdevs[i].led_cdev.max_brightness = ITE8291_KBD_ZONES_BRIGHTNESS_MAX;
		zones_data->mcled_cdevs[i].led_cdev.brightness_set = &leds_zones_set_brightness_mc;
		zones_data->mcled_cdevs[i].led_cdev.brightness = ITE8291_KBD_ZONES_BRIGHTNESS_DEFAULT;
		zones_data->mcled_cdevs[i].num_colors = 3;
		zones_data->mcled_cdevs[i].subled_info = zones_data->mcled_cdevs_subleds[i];
		zones_data->mcled_cdevs[i].subled_info[0].color_index = LED_COLOR_ID_RED;
		zones_data->mcled_cdevs[i].subled_info[0].intensity = 255;
		zones_data->mcled_cdevs[i].subled_info[0].channel = i;
		zones_data->mcled_cdevs[i].subled_info[1].color_index = LED_COLOR_ID_GREEN;
		zones_data->mcled_cdevs[i].subled_info[1].intensity = 255;
		zones_data->mcled_cdevs[i].subled_info[1].channel = i;
		zones_data->mcled_cdevs[i].subled_info[2].color_index = LED_COLOR_ID_BLUE;
		zones_data->mcled_cdevs[i].subled_info[2].intensity = 255;
		zones_data->mcled_cdevs[i].subled_info[2].channel = i;

		result = devm_led_classdev_multicolor_register(&hdev->dev, &zones_data->mcled_cdevs[i]);
		if (result) {
			for (k = 0; k < i; ++k)
				devm_led_classdev_multicolor_unregister(&hdev->dev, &zones_data->mcled_cdevs[k]);
			return result;
		}
	}

	return 0;
}

static int ite8291_zones_remove(struct hid_device *hdev)
{
	int i;
	struct ite8291_driver_data_t *ite8291_driver_data = hid_get_drvdata(hdev);
	struct ite8291_driver_data_zones_t *zones_data = ite8291_driver_data->device_data;

	for (i = 0; i < ITE8291_NR_ZONES; ++i)
		devm_led_classdev_multicolor_unregister(&hdev->dev, &zones_data->mcled_cdevs[i]);

	return 0;
}

static int ite8291_zones_write_on(struct hid_device *hdev)
{
	ite8291_write_control(hdev, (u8[]){ 0x1a, 0x00, 0x01, 0x04, 0x00, 0x00, 0x00, 0x01 });
	return 0;
}

static int ite8291_zones_write_off(struct hid_device *hdev)
{
	ite8291_write_control(hdev, (u8[]){ 0x09, 0x02, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00 });
	ite8291_write_control(hdev, (u8[]){ 0x12, 0x00, 0x03, 0x00, 0x00, 0x00, 0x00, 0x00 });
	ite8291_write_control(hdev, (u8[]){ 0x08, 0x05, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00 });
	ite8291_write_control(hdev, (u8[]){ 0x08, 0x01, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00 });
	ite8291_write_control(hdev, (u8[]){ 0x1a, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x01 });
	return 0;
}

static int ite8291_zones_write_state(struct hid_device *hdev)
{
	struct ite8291_driver_data_t *ite8291_driver_data = hid_get_drvdata(hdev);
	struct ite8291_driver_data_zones_t *zones_data = ite8291_driver_data->device_data;
	struct led_classdev_mc *mcled_cdev;
	int i;
	u8 red, green, blue;

	u8 brightness  = zones_data->mcled_cdevs[0].led_cdev.brightness;

	for (i = 0; i < ITE8291_NR_ZONES; ++i) {
		mcled_cdev = &zones_data->mcled_cdevs[i];
		red = mcled_cdev->subled_info[0].intensity;
		green = mcled_cdev->subled_info[1].intensity;
		blue = mcled_cdev->subled_info[2].intensity;
		color_scaling(hdev, &red, &green, &blue, false, 0, 0);
		ite8291_write_control(hdev, (u8[]){ 0x14, 0x00, i + 1, red, green, blue, 0x00, 0x00 });
	}

	ite8291_write_control(hdev, (u8[]){ 0x08, 0x02, 0x01, 0x03, brightness, 0x08, 0x00, 0x00 });

	return 0;
}

static int ite8291_driver_data_setup(struct hid_device *hdev)
{
	struct ite8291_driver_data_t *driver_data;

	struct usb_device *usb_dev;
	struct usb_device_descriptor *usb_desc;

	driver_data = hid_get_drvdata(hdev);

	usb_dev = to_usb_device(hdev->dev.parent->parent);
	usb_desc = &(usb_dev->descriptor);

	driver_data->hid_dev = hdev;
	driver_data->bcd_device = le16_to_cpu(usb_desc->bcdDevice);

	// Initialize device specific data
	if (hdev->product == 0xce00 && driver_data->bcd_device == 0x0002) {
		driver_data->device_has_buffer_input_control = false;
		driver_data->device_add = ite8291_zones_add;
		driver_data->device_remove = ite8291_zones_remove;
		driver_data->device_write_on = ite8291_zones_write_on;
		driver_data->device_write_off = ite8291_zones_write_off;
		driver_data->device_write_state = ite8291_zones_write_state;
	} else {
		driver_data->device_has_buffer_input_control = true;
		driver_data->device_add = ite8291_perkey_add;
		driver_data->device_remove = ite8291_perkey_remove;
		driver_data->device_write_on = ite8291_perkey_write_on;
		driver_data->device_write_off = ite8291_perkey_write_off;
		driver_data->device_write_state = ite8291_perkey_write_state;
	}

	return 0;
}

static ssize_t buffer_input_show(struct device *device,
				 struct device_attribute *attr,
				 char *buf)
{
	struct hid_device *hdev;
	struct ite8291_driver_data_t *driver_data;
	hdev = container_of(device, struct hid_device, dev);
	driver_data = hid_get_drvdata(hdev);
	return sysfs_emit(buf, "%d\n", (bool) driver_data->device_buffer_input);

}

static ssize_t buffer_input_store(struct device *device,
				  struct device_attribute *attr,
				  const char *buf,
				  size_t size)
{
	struct hid_device *hdev;
	struct ite8291_driver_data_t *driver_data;
	hdev = container_of(device, struct hid_device, dev);
	driver_data = hid_get_drvdata(hdev);

	if (kstrtobool(buf, &driver_data->device_buffer_input) < 0)
		return -EINVAL;

	if (!driver_data->device_buffer_input)
		driver_data->device_write_state(hdev);

	return size;
}

DEVICE_ATTR_RW(buffer_input);

static struct attribute *control_group_attrs[] = {
	&dev_attr_buffer_input.attr,
	NULL
};

static struct attribute_group control_group = {
	.name = "controls",
	.attrs = control_group_attrs
};

static int driver_probe_callb(struct hid_device *hdev, const struct hid_device_id *id)
{
	int result;
	struct ite8291_driver_data_t *ite8291_driver_data;

#if LINUX_VERSION_CODE >= KERNEL_VERSION(4, 18, 0)
	// Unused device on Stellaris Intel Gen5 (membrane), avoid binding to it
	if (dmi_match(DMI_PRODUCT_SKU, "STELLARIS1XI05") && hdev->product == 0x5000)
		return -ENODEV;
#endif

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

	hid_set_drvdata(hdev, ite8291_driver_data);

	result = ite8291_driver_data_setup(hdev);
	if (result != 0)
		return result;

	result = ite8291_driver_data->device_add(hdev);
	if (result != 0)
		return result;

	ite8291_driver_data->device_write_on(hdev);
	ite8291_driver_data->device_write_state(hdev);

	if (ite8291_driver_data->device_has_buffer_input_control) {
		result = sysfs_create_group(&hdev->dev.kobj, &control_group);
		if (result != 0) {
			stop_hw(hdev);
			return result;
		}
	}

	return 0;
}

static void driver_remove_callb(struct hid_device *hdev)
{
	struct ite8291_driver_data_t *driver_data;
	pr_debug("driver remove\n");
	driver_data = hid_get_drvdata(hdev);
	driver_data->device_write_off(hdev);
	driver_data->device_remove(hdev);
	if (driver_data->device_has_buffer_input_control)
		sysfs_remove_group(&hdev->dev.kobj, &control_group);

	stop_hw(hdev);
}

#ifdef CONFIG_PM
static int driver_suspend_callb(struct hid_device *hdev, pm_message_t message)
{
	struct ite8291_driver_data_t *driver_data;
	pr_debug("driver suspend\n");
	driver_data = hid_get_drvdata(hdev);
	driver_data->device_write_off(hdev);
	return 0;
}

static int driver_resume_callb(struct hid_device *hdev)
{
	struct ite8291_driver_data_t *driver_data;
	pr_debug("driver resume\n");
	driver_data = hid_get_drvdata(hdev);
	driver_data->device_write_on(hdev);
	return driver_data->device_write_state(hdev);
}

static int driver_reset_resume_callb(struct hid_device *hdev)
{
	struct ite8291_driver_data_t *driver_data;
	pr_debug("driver reset resume\n");
	driver_data = hid_get_drvdata(hdev);
	driver_data->device_write_on(hdev);
	return driver_data->device_write_state(hdev);
}
#endif

static const struct hid_device_id ite8291_device_table[] = {
	{ HID_USB_DEVICE(0x048d, 0xce00) },
	{ HID_USB_DEVICE(0x048d, 0x6004) },
	{ HID_USB_DEVICE(0x048d, 0x600a) },
	{ HID_USB_DEVICE(0x048d, 0x600b) },
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
MODULE_DESCRIPTION("Driver for ITE Device(8291) RGB LED keyboard backlight.");
MODULE_LICENSE("GPL");
