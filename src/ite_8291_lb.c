/*!
 * Copyright (c) 2023 TUXEDO Computers GmbH <tux@tuxedocomputers.com>
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
#define pr_fmt(fmt) KBUILD_MODNAME ": " fmt
#include <linux/kernel.h>
#include <linux/version.h>
#include <linux/module.h>
#include <linux/device.h>
#include <linux/hid.h>
#include <linux/dmi.h>
#include <linux/led-class-multicolor.h>
#include <linux/of.h>

// USB HID control data write size
#define HID_DATA_SIZE 8

struct ite8291_driver_data_t {
	struct hid_device *hid_dev;
	struct led_classdev_mc mcled_cdev_lightbar;
	struct mc_subled mcled_cdev_subleds_lightbar[3];
};

/**
 * Color scaling quirk list
 */
static void color_scaling(struct hid_device *hdev, u8 *red, u8 *green, u8 *blue)
{
#if LINUX_VERSION_CODE >= KERNEL_VERSION(4, 18, 0)
	if (dmi_match(DMI_PRODUCT_SKU, "STEPOL1XA04") && hdev->product == 0x6010) {
		*green = (100 * *green) / 255;
		*blue = (100 * *blue) / 255;
	}
#endif
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
		return -ENODEV;
	}

	return 0;
}

static int ite8291_write_off(struct hid_device *hdev)
{
	switch (hdev->product) {
	case 0x6010:
		// TODO: Fix for Stellaris 17 Gen 4
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
		return -ENODEV;
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

static int ite8291_write_lightbar(struct hid_device *hdev, u8 red, u8 green, u8 blue, u8 brightness)
{
	if (hdev == NULL)
		return -ENODEV;

	color_scaling(hdev, &red, &green, &blue);

	switch (hdev->product) {
	case 0x6010:
		ite8291_write_control(hdev, (u8[]){ 0x14, 0x00, 0x01, red, green, blue, 0x00, 0x01 });
		ite8291_write_control(hdev, (u8[]){ 0x08, 0x02, 0x01, 0x01, brightness, 0x08, 0x00, 0x00 });
		break;

	case 0x7000:
		ite8291_write_control(hdev, (u8[]){ 0x14, 0x01, 0x01, red, green, blue, 0x00, 0x00 });
		ite8291_write_control(hdev, (u8[]){ 0x08, 0x21, 0x01, 0x01, brightness, 0x01, 0x00, 0x00 });
		break;

	default:
		return -ENODEV;
	}

	return 0;
}

static int ite8291_write_state(struct hid_device *hdev)
{
	struct ite8291_driver_data_t *ite8291_driver_data = hid_get_drvdata(hdev);
	struct led_classdev_mc *mcled_cdev = &ite8291_driver_data->mcled_cdev_lightbar;

	return ite8291_write_lightbar(hdev, mcled_cdev->subled_info[0].intensity, mcled_cdev->subled_info[1].intensity, mcled_cdev->subled_info[2].intensity, ite8291_driver_data->mcled_cdev_lightbar.led_cdev.brightness);
}

void leds_set_brightness_mc_lightbar(struct led_classdev *led_cdev, enum led_brightness brightness) {
	struct device *dev = led_cdev->dev->parent;
	struct hid_device *hdev = to_hid_device(dev);

	ite8291_write_state(hdev);
}

static int ite8291_init_leds(struct hid_device *hdev)
{
	struct ite8291_driver_data_t *ite8291_driver_data = hid_get_drvdata(hdev);
	int retval;

	ite8291_driver_data->mcled_cdev_lightbar.led_cdev.name = "rgb:" "lightbar";
	ite8291_driver_data->mcled_cdev_lightbar.led_cdev.max_brightness = 0x64;
	ite8291_driver_data->mcled_cdev_lightbar.led_cdev.brightness_set = &leds_set_brightness_mc_lightbar;
	ite8291_driver_data->mcled_cdev_lightbar.led_cdev.brightness = 0x64;
	ite8291_driver_data->mcled_cdev_lightbar.num_colors = 3;
	ite8291_driver_data->mcled_cdev_lightbar.subled_info = ite8291_driver_data->mcled_cdev_subleds_lightbar;
	ite8291_driver_data->mcled_cdev_lightbar.subled_info[0].color_index = LED_COLOR_ID_RED;
	ite8291_driver_data->mcled_cdev_lightbar.subled_info[0].intensity = 255;
	ite8291_driver_data->mcled_cdev_lightbar.subled_info[0].channel = 0;
	ite8291_driver_data->mcled_cdev_lightbar.subled_info[1].color_index = LED_COLOR_ID_GREEN;
	ite8291_driver_data->mcled_cdev_lightbar.subled_info[1].intensity = 255;
	ite8291_driver_data->mcled_cdev_lightbar.subled_info[1].channel = 0;
	ite8291_driver_data->mcled_cdev_lightbar.subled_info[2].color_index = LED_COLOR_ID_BLUE;
	ite8291_driver_data->mcled_cdev_lightbar.subled_info[2].intensity = 255;
	ite8291_driver_data->mcled_cdev_lightbar.subled_info[2].channel = 0;

	retval = devm_led_classdev_multicolor_register(&hdev->dev, &ite8291_driver_data->mcled_cdev_lightbar);
	if (retval != 0)
		return retval;

	return 0;
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

	ite8291_driver_data->hid_dev = hdev;

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
MODULE_DESCRIPTION("Driver for ITE Device(8291) RGB lightbar");
MODULE_VERSION("0.0.1");
MODULE_LICENSE("GPL");
