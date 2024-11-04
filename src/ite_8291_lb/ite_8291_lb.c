// SPDX-License-Identifier: GPL-3.0+
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
#include <linux/kernel.h>
#include <linux/version.h>
#include <linux/module.h>
#include <linux/device.h>
#include <linux/hid.h>
#include <linux/dmi.h>
#include <linux/led-class-multicolor.h>
#include <linux/of.h>
#include <linux/delay.h>

// USB HID control data write size
#define HID_DATA_SIZE 8

#define LIGHTBAR_MAX_BRIGHTNESS		0x64
#define LIGHTBAR_DEFAULT_BRIGHTNESS	0x00
#define LIGHTBAR_DEFAULT_COLOR_RED	0xff
#define LIGHTBAR_DEFAULT_COLOR_GREEN	0xff
#define LIGHTBAR_DEFAULT_COLOR_BLUE	0xff

struct color_u8 {
	u8 red;
	u8 green;
	u8 blue;
};

struct ite8291_driver_data_t {
	struct hid_device *hid_dev;
	struct led_classdev_mc mcled_cdev_lightbar;
	struct mc_subled mcled_cdev_subleds_lightbar[3];
	struct color_u8 *color_list;
	int color_list_length;
};

/**
 * strstr version of dmi_match
 */
static bool __attribute__ ((unused)) dmi_string_in(enum dmi_field f, const char *str)
{
	const char *info = dmi_get_system_info(f);

	if (info == NULL || str == NULL)
		return info == str;

	return strstr(info, str) != NULL;
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

/**
 * Color scaling quirk list
 */
static void color_scaling(struct hid_device *hdev, u8 *red, u8 *green, u8 *blue)
{
#if LINUX_VERSION_CODE >= KERNEL_VERSION(4, 18, 0)
	if (dmi_match(DMI_PRODUCT_SKU, "STEPOL1XA04") && hdev->product == 0x6010) {
		*green = (100 * *green) / 255;
		*blue = (100 * *blue) / 255;
	} else if (dmi_match(DMI_PRODUCT_SKU, "STELLARIS1XI05") && hdev->product == 0x6010) {
		*green = (100 * *green) / 255;
		*blue = (100 * *blue) / 255;
	} else if (dmi_match(DMI_PRODUCT_SKU, "STELLARIS17I06") && hdev->product == 0x6010) {
		*green = (100 * *green) / 255;
		*blue = (100 * *blue) / 255;
	}
#endif
}

static int ite8291_set_color_list_entry(struct hid_device *hdev, int index, u8 red, u8 green, u8 blue)
{
	struct ite8291_driver_data_t *driver_data = hid_get_drvdata(hdev);

	if (index >= driver_data->color_list_length)
		return -EINVAL;

	driver_data->color_list[index].red = red;
	driver_data->color_list[index].green = green;
	driver_data->color_list[index].blue = blue;

	return 0;
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

	memcpy(buf, ctrl_data, (size_t) HID_DATA_SIZE);

	result = hid_hw_raw_request(hdev, buf[0], buf, HID_DATA_SIZE,
				    HID_FEATURE_REPORT, HID_REQ_SET_REPORT);
	kfree(buf);

	return result;
}

static int ite8291_write_color_list(struct hid_device *hdev)
{
	struct ite8291_driver_data_t *driver_data = hid_get_drvdata(hdev);
	int i;

	u8 color_ctrl[] = { 0x14, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00 };


	switch (hdev->product) {
	case 0x6010:
		break;

	case 0x7000:
		color_ctrl[1] = 0x01;
		break;

	default:
		return -ENOSYS;
	}

	for (i = 0; i < driver_data->color_list_length; ++i) {
		color_ctrl[2] = (u8)i + 1;
		color_ctrl[3] = driver_data->color_list[i].red;
		color_ctrl[4] = driver_data->color_list[i].green;
		color_ctrl[5] = driver_data->color_list[i].blue;
		color_scaling(hdev, &color_ctrl[3], &color_ctrl[4], &color_ctrl[5]);
		ite8291_write_control(hdev, color_ctrl);
	}

	return 0;
}

/**
 * Set mono color
 * 
 * Note: For now not using color list
 * 
 * @param red Range 0x00 - 0xff
 * @param green Range 0x00 - 0xff
 * @param blue Range 0x00 - 0xff
 * @param brightness Range 0x00 - 0x64
 */
static int ite8291_write_lightbar_mono(struct hid_device *hdev, u8 red, u8 green, u8 blue, u8 brightness)
{
	if (hdev == NULL)
		return -ENODEV;

	if (brightness > 0x64)
		return -EINVAL;

	color_scaling(hdev, &red, &green, &blue);

	switch (hdev->product) {
	case 0x6010:
		ite8291_write_control(hdev, (u8[]){ 0x14, 0x00, 0x01, red, green, blue, 0x00, 0x00 });
		ite8291_write_control(hdev, (u8[]){ 0x08, 0x02, 0x01, 0x01, brightness, 0x08, 0x00, 0x00 });
		break;

	case 0x7000:
		ite8291_write_control(hdev, (u8[]){ 0x14, 0x01, 0x01, red, green, blue, 0x00, 0x00 });
		ite8291_write_control(hdev, (u8[]){ 0x08, 0x21, 0x01, 0x01, brightness, 0x01, 0x00, 0x00 });
		break;

	case 0x7001:
		ite8291_write_control(hdev, (u8[]){ 0x14, 0x00, 0x01, red, green, blue, 0x00, 0x00 });
		ite8291_write_control(hdev, (u8[]){ 0x08, 0x22, 0x01, 0x01, brightness, 0x01, 0x00, 0x00 });
		break;

	default:
		return -ENOSYS;
	}

	return 0;
}

/**
 * Write breathe mode
 * 
 * @param brightness Range 0x00 - 0x64
 * @param speed Range slowest 0x0a to fastest 0x01
 */
static int __attribute__ ((unused)) ite8291_write_lightbar_breathe(struct hid_device *hdev, u8 brightness, u8 speed)
{
	if (hdev == NULL)
		return -ENODEV;
	
	if (brightness > 0x64)
		return -EINVAL;

	if (speed < 0x01 || speed > 0x0a)
		return -EINVAL;

	switch (hdev->product) {
	case 0x6010:
		ite8291_write_color_list(hdev);
		ite8291_write_control(hdev, (u8[]){ 0x08, 0x02, 0x02, speed, brightness, 0x08, 0x00, 0x00 });
		break;

	case 0x7000:
		ite8291_write_color_list(hdev);
		ite8291_write_control(hdev, (u8[]){ 0x08, 0x21, 0x02, speed, brightness, 0x08, 0x00, 0x00 });
		break;

	default:
		return -ENOSYS;
	}

	return 0;
}

/**
 * Write wave mode
 * 
 * @param brightness Range 0x00 - 0x64
 * @param speed Range slowest 0x0a to fastest 0x01
 */
static int __attribute__ ((unused)) ite8291_write_lightbar_wave(struct hid_device *hdev, u8 brightness, u8 speed)
{
	if (hdev == NULL)
		return -ENODEV;

	if (brightness > 0x64)
		return -EINVAL;

	if (speed < 0x01 || speed > 0x0a)
		return -EINVAL;

	switch (hdev->product) {

	case 0x7000:
		// Reference usage writes previous color but no color is available for mode => skipping
		ite8291_write_control(hdev, (u8[]){ 0x08, 0x21, 0x03, speed, brightness, 0x01, 0x00, 0x00 });
		break;

	default:
		return -ENOSYS;
	}

	return 0;
}

/**
 * Write clash mode
 * 
 * @param brightness Range 0x00 - 0x64
 * @param speed Range slowest 0x0a to fastest 0x01
 */
static int __attribute__ ((unused)) ite8291_write_lightbar_clash(struct hid_device *hdev, u8 brightness, u8 speed)
{
	if (hdev == NULL)
		return -ENODEV;

	if (brightness > 0x64)
		return -EINVAL;

	if (speed < 0x01 || speed > 0x0a)
		return -EINVAL;

	switch (hdev->product) {

	case 0x7000:
		ite8291_write_color_list(hdev);
		ite8291_write_control(hdev, (u8[]){ 0x08, 0x21, 0x04, speed, brightness, 0x08, 0x00, 0x00 });
		break;

	default:
		return -ENOSYS;
	}

	return 0;
}

/**
 * Write "catch up" mode
 * 
 * @param brightness Range 0x00 - 0x64
 * @param speed Range slowest 0x0a to fastest 0x01
 */
static int __attribute__ ((unused)) ite8291_write_lightbar_catchup(struct hid_device *hdev, u8 brightness, u8 speed)
{
	if (hdev == NULL)
		return -ENODEV;

	if (brightness > 0x64)
		return -EINVAL;

	if (speed < 0x01 || speed > 0x0a)
		return -EINVAL;

	switch (hdev->product) {

	case 0x7000:
		// Reference usage writes previous color but no color is available for mode => skipping
		ite8291_write_control(hdev, (u8[]){ 0x08, 0x21, 0x05, speed, brightness, 0x01, 0x00, 0x00 });
		break;

	default:
		return -ENOSYS;
	}

	return 0;
}

/**
 * Write flash mode
 * 
 * @param brightness Range 0x00 - 0x64
 * @param speed Range slowest 0x0a to fastest 0x01
 * @param direction 0x00: no, 0x01: right, 0x02 left
 */
static int __attribute__ ((unused)) ite8291_write_lightbar_flash(struct hid_device *hdev, u8 brightness, u8 speed, u8 direction)
{
	if (hdev == NULL)
		return -ENODEV;

	if (brightness > 0x64)
		return -EINVAL;

	if (speed < 0x01 || speed > 0x0a)
		return -EINVAL;

	if (direction > 0x02)
		return -EINVAL;

	switch (hdev->product) {
	case 0x6010:
		ite8291_write_color_list(hdev);
		ite8291_write_control(hdev, (u8[]){ 0x08, 0x02, 0x11, speed, brightness, 0x08, direction, 0x00 });
		break;

	default:
		return -ENOSYS;
	}

	return 0;
}

static int ite8291_write_on(struct hid_device *hdev)
{
	switch (hdev->product) {
	case 0x6010:
		ite8291_write_control(hdev, (u8[]){ 0x1a, 0x00, 0x01, 0x04, 0x00, 0x00, 0x00, 0x01 });
		break;

	case 0x7000:
		ite8291_write_control(hdev, (u8[]){ 0x1a, 0x01, 0x01, 0x04, 0x00, 0x00, 0x00, 0x01 });
		break;

	default:
		return -ENOSYS;
	}

	return 0;
}

static int ite8291_write_off(struct hid_device *hdev)
{
	switch (hdev->product) {
	case 0x6010:
		// Explicitly write mono color "off" due to issue with turning off reliably (especially for sleep)
		ite8291_write_lightbar_mono(hdev, 0, 0, 0, 0);
		msleep(50);

		ite8291_write_control(hdev, (u8[]){ 0x12, 0x00, 0x03, 0x00, 0x00, 0x00, 0x00, 0x00 });
		ite8291_write_control(hdev, (u8[]){ 0x08, 0x05, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00 });
		ite8291_write_control(hdev, (u8[]){ 0x08, 0x01, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00 });
		ite8291_write_control(hdev, (u8[]){ 0x1a, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x01 });
		break;

	case 0x7000:
		ite8291_write_control(hdev, (u8[]){ 0x12, 0x00, 0x03, 0x00, 0x00, 0x00, 0x00, 0x00 });
		ite8291_write_control(hdev, (u8[]){ 0x08, 0x05, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00 });
		ite8291_write_control(hdev, (u8[]){ 0x08, 0x01, 0x01, 0x00, 0x00, 0x00, 0x00, 0x00 });
		ite8291_write_control(hdev, (u8[]){ 0x1a, 0x01, 0x00, 0x00, 0x00, 0x00, 0x00, 0x01 });
		break;

	default:
		return -ENOSYS;
	}

	return 0;
}

static void __attribute__ ((unused)) ite8291_set_testcolors(struct hid_device *hdev)
{
	ite8291_set_color_list_entry(hdev, 0, 0xff, 0x00, 0x00);
	ite8291_set_color_list_entry(hdev, 1, 0xff, 0xff, 0x00);
	ite8291_set_color_list_entry(hdev, 2, 0x00, 0xff, 0x00);
	ite8291_set_color_list_entry(hdev, 3, 0xff, 0x00, 0xff);
	ite8291_set_color_list_entry(hdev, 4, 0xff, 0xff, 0x00);
	ite8291_set_color_list_entry(hdev, 5, 0x00, 0xff, 0x00);
	ite8291_set_color_list_entry(hdev, 6, 0x00, 0x00, 0xff);
}

static int ite8291_write_state(struct hid_device *hdev)
{
	struct ite8291_driver_data_t *ite8291_driver_data = hid_get_drvdata(hdev);
	struct led_classdev_mc *mcled_cdev = &ite8291_driver_data->mcled_cdev_lightbar;

	// For now only supports mono color state saved in sysfs structure
	return ite8291_write_lightbar_mono(hdev,
					   mcled_cdev->subled_info[0].intensity,
					   mcled_cdev->subled_info[1].intensity,
					   mcled_cdev->subled_info[2].intensity,
					   ite8291_driver_data->mcled_cdev_lightbar.led_cdev.brightness);
}

static void leds_set_brightness_mc_lightbar(struct led_classdev *led_cdev, enum led_brightness brightness) {
	struct device *dev = led_cdev->dev->parent;
	struct hid_device *hdev = to_hid_device(dev);

	ite8291_write_state(hdev);
}

static int ite8291_init_leds(struct hid_device *hdev)
{
	struct ite8291_driver_data_t *ite8291_driver_data = hid_get_drvdata(hdev);
	int retval;

	ite8291_driver_data->mcled_cdev_lightbar.led_cdev.name = "rgb:" "lightbar";
	ite8291_driver_data->mcled_cdev_lightbar.led_cdev.max_brightness = LIGHTBAR_MAX_BRIGHTNESS;
	ite8291_driver_data->mcled_cdev_lightbar.led_cdev.brightness_set = &leds_set_brightness_mc_lightbar;
	ite8291_driver_data->mcled_cdev_lightbar.led_cdev.brightness = LIGHTBAR_DEFAULT_BRIGHTNESS;
	ite8291_driver_data->mcled_cdev_lightbar.num_colors = 3;
	ite8291_driver_data->mcled_cdev_lightbar.subled_info = ite8291_driver_data->mcled_cdev_subleds_lightbar;
	ite8291_driver_data->mcled_cdev_lightbar.subled_info[0].color_index = LED_COLOR_ID_RED;
	ite8291_driver_data->mcled_cdev_lightbar.subled_info[0].intensity = LIGHTBAR_DEFAULT_COLOR_RED;
	ite8291_driver_data->mcled_cdev_lightbar.subled_info[0].channel = 0;
	ite8291_driver_data->mcled_cdev_lightbar.subled_info[1].color_index = LED_COLOR_ID_GREEN;
	ite8291_driver_data->mcled_cdev_lightbar.subled_info[1].intensity = LIGHTBAR_DEFAULT_COLOR_GREEN;
	ite8291_driver_data->mcled_cdev_lightbar.subled_info[1].channel = 0;
	ite8291_driver_data->mcled_cdev_lightbar.subled_info[2].color_index = LED_COLOR_ID_BLUE;
	ite8291_driver_data->mcled_cdev_lightbar.subled_info[2].intensity = LIGHTBAR_DEFAULT_COLOR_BLUE;
	ite8291_driver_data->mcled_cdev_lightbar.subled_info[2].channel = 0;

	retval = devm_led_classdev_multicolor_register(&hdev->dev, &ite8291_driver_data->mcled_cdev_lightbar);
	if (retval != 0)
		return retval;

	return 0;
}

static int ite8291_driver_data_setup(struct hid_device *hdev, struct ite8291_driver_data_t *driver_data)
{
	int i;

	driver_data->hid_dev = hdev;

	switch (hdev->product) {
	case 0x6010:
		// Reference usage writes 9 entries but only 7 seem to be in
		// effect. Therefore defining 7.
	case 0x7000:
		driver_data->color_list_length = 7;
		break;

	default:
		driver_data->color_list_length = 0;
	}

	if (driver_data->color_list_length != 0) {
		driver_data->color_list = devm_kzalloc(&hdev->dev,
						       sizeof(struct color_u8) * driver_data->color_list_length,
						       GFP_KERNEL);
		if (!driver_data->color_list)
			return -ENOMEM;

		// Initialize color list
		for (i = 0; i < driver_data->color_list_length; ++i) {
			driver_data->color_list[i].red = 0;
			driver_data->color_list[i].green = 0;
			driver_data->color_list[i].blue = 0;
		}
	}

	return 0;
}

static int driver_probe_callb(struct hid_device *hdev, const struct hid_device_id *id)
{
	int result;
	struct ite8291_driver_data_t *ite8291_driver_data;
	bool exclude_device = false;

	// Unused devices in Stellaris Gen5 models
#if LINUX_VERSION_CODE >= KERNEL_VERSION(4, 18, 0)
	if (dmi_match(DMI_PRODUCT_SKU, "STELLARIS1XI05") &&
	    !dmi_match(DMI_PRODUCT_FAMILY, "STELLARIS17I05") &&
	    hdev->product == 0x6010)
		exclude_device = true;
#endif

	if (exclude_device) {
		pr_info("Note: device excluded, not binding to device %0#6x\n", hdev->product);
		return -ENODEV;
	}

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

	result = ite8291_driver_data_setup(hdev, ite8291_driver_data);
	if (result != 0)
		return result;

	hid_set_drvdata(hdev, ite8291_driver_data);

	result = ite8291_init_leds(hdev);
	if (result != 0)
		return result;

	ite8291_write_on(hdev);
	ite8291_write_state(hdev);

	return result;
}

static void driver_remove_callb(struct hid_device *hdev)
{
	struct ite8291_driver_data_t *ite8291_driver_data = hid_get_drvdata(hdev);

	devm_led_classdev_multicolor_unregister(&hdev->dev, &ite8291_driver_data->mcled_cdev_lightbar);

	ite8291_write_off(hdev);

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
	pr_debug("driver resume\n");
	ite8291_write_on(hdev);
	return ite8291_write_state(hdev);
}

static int driver_reset_resume_callb(struct hid_device *hdev)
{
	pr_debug("driver reset resume\n");
	ite8291_write_on(hdev);
	return ite8291_write_state(hdev);
}
#endif

static const struct hid_device_id ite8291_device_table[] = {
	{ HID_USB_DEVICE(0x048d, 0x6010) },
	{ HID_USB_DEVICE(0x048d, 0x7000) },
	{ HID_USB_DEVICE(0x048d, 0x7001) },
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
MODULE_DESCRIPTION("Driver for ITE RGB lightbars");
MODULE_LICENSE("GPL v3");
