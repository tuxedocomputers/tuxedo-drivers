/*!
 * Copyright (c) 2019-2020 TUXEDO Computers GmbH <tux@tuxedocomputers.com>
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
#include <linux/module.h>
#include <linux/kernel.h>
#include <linux/init.h>
#include <linux/device.h>
#include <linux/ioctl.h>
#include <linux/fs.h>
#include <linux/cdev.h>
#include <linux/uaccess.h>
#include <linux/hid.h>
#include <linux/acpi.h>
#include <linux/delay.h>
#include <linux/keyboard.h>

MODULE_DESCRIPTION("TUXEDO Computers, ITE backlight driver");
MODULE_AUTHOR("TUXEDO Computers GmbH <tux@tuxedocomputers.com>");
MODULE_VERSION("0.0.5");
MODULE_LICENSE("GPL");

#define KEYBOARD_ROWS       6
#define KEYBOARD_COLUMNS    20

#define get_led_id(row, col)    (u8)( ((row & 0x07) << 5) | (col & 0x1f) )

#define HID_DATA_SIZE 6

// Keyboard events
#define INT_KEY_B_UP		KEY_KBDILLUMUP
#define INT_KEY_B_DOWN		KEY_KBDILLUMDOWN
#define INT_KEY_B_TOGGLE	KEY_KBDILLUMTOGGLE
#define INT_KEY_B_NEXT		KEY_LIGHTS_TOGGLE

static struct hid_device *kbdev = NULL;
static struct mutex dev_lock;
static struct mutex input_lock;

// Default brightness (0-10)
#define DEFAULT_BRIGHTNESS  3
// Default mode (index to mode_to_color array) or extra modes
#define DEFAULT_MODE        6

static struct tuxedo_keyboard_ite_data {
	int brightness;
	int on;
	int mode;
} ti_data = {
	.brightness = DEFAULT_BRIGHTNESS,
	.on = TRUE,
	.mode = DEFAULT_MODE
};

// Color mode definition
static int mode_to_color[] = { 0xff0000, 0x00ff00, 0x0000ff, 0xffff00, 0xff00ff, 0x00ffff, 0xffffff };
// Length of color mode array
static const int MODE_MAP_LENGTH = sizeof(mode_to_color)/sizeof(mode_to_color[0]);
// Amount of extra modes in addition to the color ones
static const int MODE_EXTRAS_LENGTH = 2;

// Other parameters
int p_sweep_delay = 0;
u8 p_color_red = 0;
u8 p_color_green = 0;
u8 p_color_blue = 0;
int p_set_color = 0;

static void sweep_delay(void)
{
	if (p_sweep_delay != 0) {
		udelay(200 * clamp(p_sweep_delay, 0, 10));
	}
}

static int keyb_send_data(struct hid_device *dev, u8 cmd, u8 d0, u8 d1, u8 d2, u8 d3)
{
	int result = 0;
	u8 *buf;
	if (dev == NULL) {
		return -ENODEV;
	}

	mutex_lock(&dev_lock);

	buf = kzalloc(HID_DATA_SIZE, GFP_KERNEL);
	buf[0] = 0xcc;
	buf[1] = cmd;
	buf[2] = d0;
	buf[3] = d1;
	buf[4] = d2;
	buf[5] = d3;

	result = hid_hw_raw_request(dev, buf[0], buf, HID_DATA_SIZE, HID_FEATURE_REPORT, HID_REQ_SET_REPORT);
	kfree(buf);

	mutex_unlock(&dev_lock);

	return result;
}

void keyb_set_all(struct hid_device *dev, u8 color_red, u8 color_green, u8 color_blue)
{
	int row, col;
	for (col = 0; col < KEYBOARD_COLUMNS; ++col) {
		for (row = 0; row < KEYBOARD_ROWS; ++row) {
			keyb_send_data(dev, 0x01, get_led_id(row, col), color_red, color_green, color_blue);
			sweep_delay();
		}
	}
}

static void send_mode(struct hid_device *dev, int mode)
{
	int row, col;
	u8 color_red, color_green, color_blue;
	
	if (dev == NULL) {
		return;
	}

	p_set_color = 0;

	if (mode < MODE_MAP_LENGTH) {
		// Color modes, mode is index to mode_to_color array map
		color_red = (mode_to_color[mode] >> 0x10) & 0xff;
		color_green = (mode_to_color[mode] >> 0x08) & 0xff;
		color_blue = (mode_to_color[mode] >> 0x00) & 0xff;
		keyb_set_all(dev, color_red, color_green, color_blue);
	} else if (mode == MODE_MAP_LENGTH) {
		// White background, TUXEDO letters in red
		for (col = 0; col < KEYBOARD_COLUMNS; ++col) {
			for (row = 0; row < KEYBOARD_ROWS; ++row) {
				if (
					(row == 2 && col == 6) ||   // T
					(row == 2 && col == 8) ||   // U
					(row == 4 && col == 4) ||   // X
					(row == 2 && col == 4) ||   // E
					(row == 3 && col == 4) ||   // D
					(row == 2 && col == 10)     // O
				) {
					keyb_send_data(dev, 0x01, get_led_id(row, col), 0xff, 0x00, 0x00);
				} else {
					keyb_send_data(dev, 0x01, get_led_id(row, col), 0xff, 0xff, 0xff);
				}
			}
		}
	} else if (mode == MODE_MAP_LENGTH + 1) {
		// Random color animating effect, special mode
		keyb_send_data(dev, 0x00, 0x09, 0x00, 0x00, 0x00);
	}
}

static void stop_hw(struct hid_device *dev)
{
	hid_hw_power(dev, PM_HINT_NORMAL);
	kbdev = NULL;
	hid_hw_close(dev);
	hid_hw_stop(dev);
}

static int start_hw(struct hid_device *dev)
{
	int result;
	result = hid_hw_start(dev, HID_CONNECT_DEFAULT);
	if (result) {
		pr_err("hid_hw_start failed\n");
		goto err_stop_hw;
	}

	hid_hw_power(dev, PM_HINT_FULLON);

	result = hid_hw_open(dev);
	if (result) {
		pr_err("hid_hw_open failed\n");
		goto err_stop_hw;
	}

	kbdev = dev;
	return 0;

err_stop_hw:
	stop_hw(dev);
	return result;
}

static void init_from_params(struct hid_device *dev)
{
	if (ti_data.on) {
		keyb_send_data(kbdev, 0x09, ti_data.brightness, 0x02, 0x00, 0x00);
	}
	else {
		keyb_send_data(kbdev, 0x09, 0x00, 0x02, 0x00, 0x00);
	}

	if (p_set_color == 1) {
		keyb_set_all(dev, p_color_red, p_color_green, p_color_blue);
	} else {
		send_mode(dev, ti_data.mode);
	}
}

static void key_actions(unsigned long key_code)
{
	mutex_lock(&input_lock);

	switch (key_code) {
	case INT_KEY_B_UP:
		// Brightness one step up
		ti_data.on = TRUE;
		ti_data.brightness += 1;

		if (ti_data.brightness > 10) {
			ti_data.brightness = 10;
		}

		keyb_send_data(kbdev, 0x09, ti_data.brightness, 0x02, 0x00,
			       0x00);
		break;
	case INT_KEY_B_DOWN:
		// Brightness one step down
		ti_data.on = TRUE;
		ti_data.brightness -= 1;

		if (ti_data.brightness < 0) {
			ti_data.brightness = 0;
		}

		keyb_send_data(kbdev, 0x09, ti_data.brightness, 0x02, 0x00,
			       0x00);

		break;
	case INT_KEY_B_TOGGLE:
		// Toggle on/off
		if (ti_data.on) {
			ti_data.on = FALSE;
		} else {
			ti_data.on = TRUE;
		}

		if (ti_data.on) {
			keyb_send_data(kbdev, 0x09, ti_data.brightness, 0x02,
				       0x00, 0x00);
		} else {
			keyb_send_data(kbdev, 0x09, 0x00, 0x02, 0x00, 0x00);
		}

		break;
	case INT_KEY_B_NEXT:
		// Next mode
		ti_data.on = TRUE;
		ti_data.mode += 1;

		if (ti_data.mode >= MODE_MAP_LENGTH + MODE_EXTRAS_LENGTH) {
			ti_data.mode = 0;
		}

		send_mode(kbdev, ti_data.mode);
		break;
	}

	mutex_unlock(&input_lock);
}

static volatile unsigned long last_key = 0;

void ite_829x_key_work_handler(struct work_struct *work)
{
	key_actions(last_key);
}

static DECLARE_WORK(ite_829x_key_work, ite_829x_key_work_handler);

static int keyboard_notifier_callb(struct notifier_block *nb, unsigned long code, void *_param)
{
	struct keyboard_notifier_param *param = _param;
	int ret = NOTIFY_OK;

	if (!param->down) {
		return ret;
	}

	if (mutex_is_locked(&input_lock)) {
		return ret;
	}

	if (code == KBD_KEYCODE) {
		last_key = param->value;
		schedule_work(&ite_829x_key_work);
	}

	return NOTIFY_OK;
}

static struct notifier_block keyboard_notifier_block = {
	.notifier_call = keyboard_notifier_callb
};

static int probe_callb(struct hid_device *dev, const struct hid_device_id *id)
{
	int result;

	result = hid_parse(dev);
	if (result) {
		pr_err("hid_parse failed\n");
		stop_hw(dev);
		return result;
	}

	mutex_init(&dev_lock);

	result = start_hw(dev);
	if (result != 0) {
		return result;
	}

	register_keyboard_notifier(&keyboard_notifier_block);

	init_from_params(dev);

	return 0;
}

static void remove_callb(struct hid_device *dev)
{
	unregister_keyboard_notifier(&keyboard_notifier_block);
	stop_hw(dev);
	pr_debug("driver remove\n");
}

static int driver_suspend_callb(struct device *dev)
{
	pr_debug("driver suspend\n");
	return 0;
}

static int driver_resume_callb(struct device *dev)
{
	pr_debug("driver resume\n");
	init_from_params(kbdev);
	return 0;
}

static const struct hid_device_id hd_table[] = {
	{ HID_USB_DEVICE(0x048d, 0x8910) },
	{ }
};
MODULE_DEVICE_TABLE(hid, hd_table);

static struct hid_driver hd = {
	.name = "tuxedo-keyboard-ite",
	.probe = probe_callb,
	.remove = remove_callb,
	.id_table = hd_table,
};

static const struct dev_pm_ops tuxedo_keyboard_ite_pm = {
	SET_SYSTEM_SLEEP_PM_OPS(driver_suspend_callb, driver_resume_callb)
};

static int __init tuxedo_keyboard_ite_init(void)
{
	pr_debug("module init\n");
	mutex_init(&input_lock);
	
	hd.driver.pm = &tuxedo_keyboard_ite_pm;

	return hid_register_driver(&hd);
}

static void __exit tuxedo_keyboard_ite_exit(void)
{
	hid_unregister_driver(&hd);
	pr_debug("module exit\n");
}

// ---
// Module parameters
// ---
/*static int param_set_sweep_delay(const char *val, const struct kernel_param *kp)
{
	int result, value;
	result = kstrtoint(val, 10, &value);
	if (result != 0 || value < 0 || value > 10) {
		return -EINVAL;
	}

	value = clamp(value, 0, 10);
	return param_set_int(val, kp);
}

static const struct kernel_param_ops p_ops_sweep_delay = {
	.set = param_set_sweep_delay,
	.get = param_get_int
};
 
module_param_cb(sweep_delay, &p_ops_sweep_delay, &p_sweep_delay, S_IWUSR | S_IRUSR | S_IRGRP | S_IROTH);
MODULE_PARM_DESC(sweep_delay, "Delay LED sweep time (0-10)");*/

module_param_cb(color_red, &param_ops_byte, &p_color_red, S_IWUSR | S_IRUSR | S_IRGRP | S_IROTH);
MODULE_PARM_DESC(color_red, "Red color (0-255)");
module_param_cb(color_green, &param_ops_byte, &p_color_green, S_IWUSR | S_IRUSR | S_IRGRP | S_IROTH);
MODULE_PARM_DESC(color_green, "Green color (0-255)");
module_param_cb(color_blue, &param_ops_byte, &p_color_blue, S_IWUSR | S_IRUSR | S_IRGRP | S_IROTH);
MODULE_PARM_DESC(color_blue, "Blue color (0-255)");

static int param_set_mode(const char *val, const struct kernel_param *kp)
{
	int result, value;
	result = kstrtoint(val, 10, &value);
	if (result != 0 || value < 0 || value >= MODE_MAP_LENGTH + MODE_EXTRAS_LENGTH) {
		return -EINVAL;
	}

	ti_data.on = TRUE;
	send_mode(kbdev, value);
	return param_set_int(val, kp);
}

static const struct kernel_param_ops p_ops_mode = {
	.set = param_set_mode,
	.get = param_get_int
};
module_param_cb(mode, &p_ops_mode, &(ti_data.mode), S_IWUSR | S_IRUSR | S_IRGRP | S_IROTH);
MODULE_PARM_DESC(mode, "Mode");


static int param_set_color(const char *val, const struct kernel_param *kp)
{
	int result, value;
	result = kstrtoint(val, 10, &value);
	if (result != 0 || value <= 0 || value > 1) {
		return -EINVAL;
	}
	
	if (value == 1) {
		ti_data.on = TRUE;
		keyb_set_all(kbdev, p_color_red, p_color_green, p_color_blue);
	}
	return param_set_int(val, kp);
}

static const struct kernel_param_ops p_ops_set_color = {
	.set = param_set_color,
	.get = param_get_int
};

module_param_cb(set_color, &p_ops_set_color, &p_set_color, S_IWUSR | S_IRUSR | S_IRGRP | S_IROTH);
MODULE_PARM_DESC(set_color, "Applies chosen color to all keys if set to 1");

// ---
// Module bootstrap
// ---
module_init(tuxedo_keyboard_ite_init);
module_exit(tuxedo_keyboard_ite_exit);
