// SPDX-License-Identifier: GPL-2.0+
/*!
 * Copyright (c) 2019-2023 TUXEDO Computers GmbH <tux@tuxedocomputers.com>
 * Copyright (c) 2026 Valentin Lobstein <valentin@chocapikk.com>
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
 *
 * ITE 8910 protocol reverse-engineered from the Uniwill Control Center
 * (perkey_api.dll + LedKeyboardSetting.exe .NET IL).
 * Protocol documentation: https://chocapikk.com/posts/2026/reverse-engineering-ite8910-keyboard-rgb/
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
#include <linux/dmi.h>
#include <linux/led-class-multicolor.h>

MODULE_DESCRIPTION("TUXEDO Computers, ITE 829x backlight driver with animation modes");
MODULE_AUTHOR("TUXEDO Computers GmbH <tux@tuxedocomputers.com>");
MODULE_AUTHOR("Valentin Lobstein <valentin@chocapikk.com>");
MODULE_LICENSE("GPL");

/* --- Hardware constants --- */

#define KEYBOARD_ROWS		6
#define KEYBOARD_COLUMNS	20

#define HID_DATA_SIZE		6
#define REPORT_ID		0xcc

#define BRIGHTNESS_MAX		0x0a
#define BRIGHTNESS_DEFAULT	0x00
#define SPEED_MAX		0x0a
#define SPEED_DEFAULT		0x00

#define COLOR_CUSTOM		0xaa
#define COLOR_SLOT_BASE		0xa1
#define PRESET_SLOT_BASE	0x71

#define get_led_id(row, col)	((u8)(((row) & 0x07) << 5 | ((col) & 0x1f)))

/* --- HID command bytes --- */

#define CMD_ANIMATION		0x00
#define CMD_SET_LED		0x01
#define CMD_BRIGHTNESS_SPEED	0x09
#define CMD_BREATHING		0x0a
#define CMD_FLASHING		0x0b
#define CMD_WAVE_COLOR		0x15
#define CMD_SNAKE_COLOR		0x16
#define CMD_SCAN_COLOR		0x17
#define CMD_RANDOM_COLOR	0x18

/* --- Animation mode IDs (for CMD_ANIMATION) --- */

#define ANIM_SPECTRUM_CYCLE	0x02
#define ANIM_RAINBOW_WAVE	0x04
#define ANIM_RANDOM		0x09
#define ANIM_SCAN		0x0a
#define ANIM_SNAKE		0x0b
#define ANIM_CLEAR		0x0c

/* --- Effect indices (sysfs) --- */

enum ite829x_effect {
	EFFECT_OFF = 0,
	EFFECT_DIRECT,
	EFFECT_SPECTRUM_CYCLE,
	EFFECT_RAINBOW_WAVE,
	EFFECT_BREATHING,
	EFFECT_BREATHING_COLOR,
	EFFECT_FLASHING,
	EFFECT_FLASHING_COLOR,
	EFFECT_RANDOM,
	EFFECT_RANDOM_COLOR,
	EFFECT_SCAN,
	EFFECT_SNAKE,
	EFFECT_WAVE,
	EFFECT_COUNT,
};

static const char * const effect_names[] = {
	[EFFECT_OFF]             = "off",
	[EFFECT_DIRECT]          = "direct",
	[EFFECT_SPECTRUM_CYCLE]  = "spectrum_cycle",
	[EFFECT_RAINBOW_WAVE]    = "rainbow_wave",
	[EFFECT_BREATHING]       = "breathing",
	[EFFECT_BREATHING_COLOR] = "breathing_color",
	[EFFECT_FLASHING]        = "flashing",
	[EFFECT_FLASHING_COLOR]  = "flashing_color",
	[EFFECT_RANDOM]          = "random",
	[EFFECT_RANDOM_COLOR]    = "random_color",
	[EFFECT_SCAN]            = "scan",
	[EFFECT_SNAKE]           = "snake",
	[EFFECT_WAVE]            = "wave",
};

/* Wave: 8 directions (preset 0x71-0x78, custom 0xA1-0xA8) */
static const char * const wave_direction_names[] = {
	"up_left", "up_right", "down_left", "down_right",
	"up", "down", "left", "right",
};
#define WAVE_DIR_COUNT	ARRAY_SIZE(wave_direction_names)

/* Snake: 4 diagonal directions (preset 0x71-0x74, custom 0xA1-0xA4) */
static const char * const snake_direction_names[] = {
	"up_left", "up_right", "down_left", "down_right",
};
#define SNAKE_DIR_COUNT	ARRAY_SIZE(snake_direction_names)

/* --- Keyboard events --- */

#define INT_KEY_B_NEXT		KEY_LIGHTS_TOGGLE

/* --- Driver state --- */

static struct hid_device *kbdev;
static struct mutex dev_lock;
static struct mutex input_lock;

static struct ite829x_state {
	int brightness;
	int speed;
	int effect;
	int direction;		/* index into direction table */
	u8 color1_r, color1_g, color1_b;
	u8 color2_r, color2_g, color2_b;
} state = {
	.brightness = BRIGHTNESS_DEFAULT,
	.speed = SPEED_DEFAULT,
	.effect = EFFECT_DIRECT,
	.direction = 0,
	.color1_r = 0xff, .color1_g = 0x00, .color1_b = 0x00,
	.color2_r = 0x00, .color2_g = 0x00, .color2_b = 0xff,
};

static struct led_classdev_mc clevo_mcled_cdevs[KEYBOARD_ROWS][KEYBOARD_COLUMNS];
static struct mc_subled clevo_mcled_cdevs_subleds[KEYBOARD_ROWS][KEYBOARD_COLUMNS][3];

/* --- Low-level HID --- */

/*
 * Persistent DMA-safe buffer for HID reports. Allocated once at probe,
 * reused for every send under dev_lock. Avoids per-call kzalloc/kfree
 * overhead (critical during send_clear which sends 121 reports).
 */
static u8 *hid_buf;

static int keyb_send_data(struct hid_device *dev, u8 cmd, u8 d0, u8 d1, u8 d2, u8 d3)
{
	int result;

	if (!dev)
		return -ENODEV;

	mutex_lock(&dev_lock);

	hid_buf[0] = REPORT_ID;
	hid_buf[1] = cmd;
	hid_buf[2] = d0;
	hid_buf[3] = d1;
	hid_buf[4] = d2;
	hid_buf[5] = d3;

	result = hid_hw_raw_request(dev, hid_buf[0], hid_buf, HID_DATA_SIZE,
				    HID_FEATURE_REPORT, HID_REQ_SET_REPORT);
	mutex_unlock(&dev_lock);

	return result;
}

/* --- Command helpers --- */

static int last_sent_brightness = -1;
static int last_sent_speed = -1;

static void send_brightness_speed(struct hid_device *dev, int brightness, int speed)
{
	int b = clamp_val(brightness, 0, BRIGHTNESS_MAX);
	int s = clamp_val(speed, 0, SPEED_MAX);

	if (b == last_sent_brightness && s == last_sent_speed)
		return;

	keyb_send_data(dev, CMD_BRIGHTNESS_SPEED, b, s, 0x00, 0x00);
	last_sent_brightness = b;
	last_sent_speed = s;
}

static void invalidate_brightness_speed_cache(void)
{
	last_sent_brightness = -1;
	last_sent_speed = -1;
}

static void send_animation_mode(struct hid_device *dev, u8 mode_id)
{
	keyb_send_data(dev, CMD_ANIMATION, mode_id, 0x00, 0x00, 0x00);
}

static void send_clear(struct hid_device *dev)
{
	int row, col;

	send_animation_mode(dev, ANIM_CLEAR);
	for (row = 0; row < KEYBOARD_ROWS; ++row)
		for (col = 0; col < KEYBOARD_COLUMNS; ++col)
			keyb_send_data(dev, CMD_SET_LED,
				       get_led_id(row, col), 0, 0, 0);
}

static void send_led_color(struct hid_device *dev, u8 led_id,
			    u8 r, u8 g, u8 b)
{
	keyb_send_data(dev, CMD_SET_LED, led_id, r, g, b);
}

/* --- Effect helpers --- */

static void send_random_or_custom(struct hid_device *dev, u8 cmd)
{
	keyb_send_data(dev, cmd, 0x00, 0x00, 0x00, 0x00);
}

static void send_custom_color(struct hid_device *dev, u8 cmd)
{
	keyb_send_data(dev, cmd, COLOR_CUSTOM,
		       state.color1_r, state.color1_g, state.color1_b);
}

static bool has_custom_color(void)
{
	return state.color1_r || state.color1_g || state.color1_b;
}

static void send_directional_slot(struct hid_device *dev, u8 cmd,
				   int dir, int max_dir)
{
	int clamped = clamp_val(dir, 0, max_dir - 1);
	u8 slot;

	if (has_custom_color()) {
		slot = COLOR_SLOT_BASE + clamped;
		keyb_send_data(dev, cmd, slot,
			       state.color1_r, state.color1_g,
			       state.color1_b);
	} else {
		slot = PRESET_SLOT_BASE + clamped;
		keyb_send_data(dev, cmd, slot, 0x00, 0x00, 0x00);
	}
}

/* --- Effect dispatch --- */

static void apply_effect(struct hid_device *dev)
{
	if (!dev)
		return;

	switch (state.effect) {
	case EFFECT_OFF:
		send_clear(dev);
		send_brightness_speed(dev, 0, state.speed);
		return;

	case EFFECT_DIRECT:
		send_clear(dev);
		send_brightness_speed(dev, state.brightness, state.speed);
		return;

	case EFFECT_SPECTRUM_CYCLE:
		send_animation_mode(dev, ANIM_SPECTRUM_CYCLE);
		break;

	case EFFECT_RAINBOW_WAVE:
		send_animation_mode(dev, ANIM_RAINBOW_WAVE);
		break;

	case EFFECT_BREATHING:
		send_random_or_custom(dev, CMD_BREATHING);
		break;

	case EFFECT_BREATHING_COLOR:
		send_custom_color(dev, CMD_BREATHING);
		break;

	case EFFECT_FLASHING:
		send_random_or_custom(dev, CMD_FLASHING);
		break;

	case EFFECT_FLASHING_COLOR:
		send_custom_color(dev, CMD_FLASHING);
		break;

	case EFFECT_RANDOM:
		send_animation_mode(dev, ANIM_RANDOM);
		break;

	case EFFECT_RANDOM_COLOR:
		send_animation_mode(dev, ANIM_RANDOM);
		keyb_send_data(dev, CMD_RANDOM_COLOR, COLOR_SLOT_BASE,
			       state.color1_r, state.color1_g, state.color1_b);
		break;

	case EFFECT_SCAN:
		send_animation_mode(dev, ANIM_SCAN);
		keyb_send_data(dev, CMD_SCAN_COLOR, COLOR_SLOT_BASE,
			       state.color1_r, state.color1_g, state.color1_b);
		keyb_send_data(dev, CMD_SCAN_COLOR, COLOR_SLOT_BASE + 1,
			       state.color2_r, state.color2_g, state.color2_b);
		break;

	case EFFECT_SNAKE:
		send_animation_mode(dev, ANIM_SNAKE);
		send_directional_slot(dev, CMD_SNAKE_COLOR,
				      state.direction, SNAKE_DIR_COUNT);
		break;

	case EFFECT_WAVE:
		send_animation_mode(dev, ANIM_RAINBOW_WAVE);
		send_directional_slot(dev, CMD_WAVE_COLOR,
				      state.direction, WAVE_DIR_COUNT);
		break;
	}

	send_brightness_speed(dev, state.brightness, state.speed);
}

/* --- sysfs attributes --- */

static ssize_t effect_show(struct device *dev, struct device_attribute *attr,
			    char *buf)
{
	int i, len = 0;

	for (i = 0; i < EFFECT_COUNT; i++) {
		if (i == state.effect)
			len += sysfs_emit_at(buf, len, "[%s] ", effect_names[i]);
		else
			len += sysfs_emit_at(buf, len, "%s ", effect_names[i]);
	}
	len += sysfs_emit_at(buf, len, "\n");
	return len;
}

static ssize_t effect_store(struct device *dev, struct device_attribute *attr,
			     const char *buf, size_t count)
{
	int i;

	for (i = 0; i < EFFECT_COUNT; i++) {
		if (sysfs_streq(buf, effect_names[i])) {
			state.effect = i;
			apply_effect(kbdev);
			return count;
		}
	}
	return -EINVAL;
}
static DEVICE_ATTR_RW(effect);

static ssize_t brightness_show(struct device *dev,
				struct device_attribute *attr, char *buf)
{
	return sysfs_emit(buf, "%d\n", state.brightness);
}

static ssize_t brightness_store(struct device *dev,
				 struct device_attribute *attr,
				 const char *buf, size_t count)
{
	int val;

	if (kstrtoint(buf, 10, &val))
		return -EINVAL;

	state.brightness = clamp_val(val, 0, BRIGHTNESS_MAX);
	send_brightness_speed(kbdev, state.brightness, state.speed);
	return count;
}
static DEVICE_ATTR_RW(brightness);

static ssize_t speed_show(struct device *dev, struct device_attribute *attr,
			   char *buf)
{
	return sysfs_emit(buf, "%d\n", state.speed);
}

static ssize_t speed_store(struct device *dev, struct device_attribute *attr,
			    const char *buf, size_t count)
{
	int val;

	if (kstrtoint(buf, 10, &val))
		return -EINVAL;

	state.speed = clamp_val(val, 0, SPEED_MAX);
	send_brightness_speed(kbdev, state.brightness, state.speed);
	return count;
}
static DEVICE_ATTR_RW(speed);

static void get_direction_table(const char * const **names, int *count)
{
	if (state.effect == EFFECT_WAVE) {
		*names = wave_direction_names;
		*count = WAVE_DIR_COUNT;
	} else if (state.effect == EFFECT_SNAKE) {
		*names = snake_direction_names;
		*count = SNAKE_DIR_COUNT;
	} else {
		*names = NULL;
		*count = 0;
	}
}

static ssize_t direction_show(struct device *dev,
			       struct device_attribute *attr, char *buf)
{
	const char * const *names;
	int dir_count, i, len = 0;

	get_direction_table(&names, &dir_count);
	if (!names)
		return sysfs_emit(buf, "none\n");

	for (i = 0; i < dir_count; i++) {
		if (i == state.direction)
			len += sysfs_emit_at(buf, len, "[%s] ", names[i]);
		else
			len += sysfs_emit_at(buf, len, "%s ", names[i]);
	}
	len += sysfs_emit_at(buf, len, "\n");
	return len;
}

static ssize_t direction_store(struct device *dev,
				struct device_attribute *attr,
				const char *buf, size_t count)
{
	const char * const *names;
	int dir_count, i;

	get_direction_table(&names, &dir_count);
	if (!names)
		return -EINVAL;

	for (i = 0; i < dir_count; i++) {
		if (sysfs_streq(buf, names[i])) {
			state.direction = i;
			apply_effect(kbdev);
			return count;
		}
	}
	return -EINVAL;
}
static DEVICE_ATTR_RW(direction);

static ssize_t show_color(char *buf, u8 r, u8 g, u8 b)
{
	return sysfs_emit(buf, "%02x%02x%02x\n", r, g, b);
}

static int parse_color(const char *buf, u8 *r, u8 *g, u8 *b)
{
	unsigned int rgb;

	if (kstrtouint(buf, 16, &rgb))
		return -EINVAL;

	*r = (rgb >> 16) & 0xff;
	*g = (rgb >> 8) & 0xff;
	*b = rgb & 0xff;
	return 0;
}

static ssize_t color1_show(struct device *dev, struct device_attribute *attr,
			    char *buf)
{
	return show_color(buf, state.color1_r, state.color1_g, state.color1_b);
}

static ssize_t color1_store(struct device *dev, struct device_attribute *attr,
			     const char *buf, size_t count)
{
	if (parse_color(buf, &state.color1_r, &state.color1_g, &state.color1_b))
		return -EINVAL;

	if (state.effect != EFFECT_DIRECT && state.effect != EFFECT_OFF)
		apply_effect(kbdev);

	return count;
}
static DEVICE_ATTR_RW(color1);

static ssize_t color2_show(struct device *dev, struct device_attribute *attr,
			    char *buf)
{
	return show_color(buf, state.color2_r, state.color2_g, state.color2_b);
}

static ssize_t color2_store(struct device *dev, struct device_attribute *attr,
			     const char *buf, size_t count)
{
	if (parse_color(buf, &state.color2_r, &state.color2_g, &state.color2_b))
		return -EINVAL;

	if (state.effect == EFFECT_SCAN)
		apply_effect(kbdev);

	return count;
}
static DEVICE_ATTR_RW(color2);

static struct attribute *ite829x_attrs[] = {
	&dev_attr_effect.attr,
	&dev_attr_brightness.attr,
	&dev_attr_speed.attr,
	&dev_attr_direction.attr,
	&dev_attr_color1.attr,
	&dev_attr_color2.attr,
	NULL,
};
ATTRIBUTE_GROUPS(ite829x);

/* --- Multicolor LED callback --- */

static void leds_set_brightness_mc(struct led_classdev *led_cdev,
				    enum led_brightness brightness)
{
	int i, j;
	struct led_classdev_mc *led_cdev_mc = lcdev_to_mccdev(led_cdev);

	state.brightness = brightness;

	for (i = 0; i < KEYBOARD_ROWS; ++i)
		for (j = 0; j < KEYBOARD_COLUMNS; ++j)
			clevo_mcled_cdevs[i][j].led_cdev.brightness = brightness;

	send_brightness_speed(kbdev, state.brightness, state.speed);

	send_led_color(kbdev, led_cdev_mc->subled_info[0].channel,
		       led_cdev_mc->subled_info[0].intensity,
		       led_cdev_mc->subled_info[1].intensity,
		       led_cdev_mc->subled_info[2].intensity);
}

/* --- Fn key handler --- */

static void key_actions(unsigned long key_code)
{
	mutex_lock(&input_lock);

	if (key_code == INT_KEY_B_NEXT) {
		state.effect = (state.effect + 1) % EFFECT_COUNT;
		apply_effect(kbdev);
	}

	mutex_unlock(&input_lock);
}

static volatile unsigned long last_key;

static void ite_829x_key_work_handler(struct work_struct *work)
{
	key_actions(last_key);
}

static DECLARE_WORK(ite_829x_key_work, ite_829x_key_work_handler);

static int keyboard_notifier_callb(struct notifier_block *nb,
				    unsigned long code, void *_param)
{
	const struct keyboard_notifier_param *param = _param;

	if (!param->down)
		return NOTIFY_OK;

	if (mutex_is_locked(&input_lock))
		return NOTIFY_OK;

	if (code == KBD_KEYCODE) {
		last_key = param->value;
		schedule_work(&ite_829x_key_work);
	}

	return NOTIFY_OK;
}

static struct notifier_block keyboard_notifier_block = {
	.notifier_call = keyboard_notifier_callb
};

/* --- HID driver callbacks --- */

static int probe_callb(struct hid_device *dev, const struct hid_device_id *id)
{
	int result, i, j;

	result = hid_parse(dev);
	if (result) {
		pr_err("hid_parse failed\n");
		return result;
	}

	mutex_init(&dev_lock);

	hid_buf = kzalloc(HID_DATA_SIZE, GFP_KERNEL);
	if (!hid_buf)
		return -ENOMEM;

	result = hid_hw_start(dev, HID_CONNECT_DEFAULT);
	if (result) {
		pr_err("hid_hw_start failed\n");
		return result;
	}

	hid_hw_power(dev, PM_HINT_FULLON);

	result = hid_hw_open(dev);
	if (result) {
		pr_err("hid_hw_open failed\n");
		hid_hw_stop(dev);
		return result;
	}

	kbdev = dev;

	/* Clear all LEDs and set default brightness */
	send_clear(dev);
	send_brightness_speed(dev, state.brightness, state.speed);

	/* Register per-key multicolor LEDs */
	for (i = 0; i < KEYBOARD_ROWS; ++i) {
		for (j = 0; j < KEYBOARD_COLUMNS; ++j) {
			clevo_mcled_cdevs[i][j].led_cdev.name =
				"rgb:" LED_FUNCTION_KBD_BACKLIGHT;
			clevo_mcled_cdevs[i][j].led_cdev.max_brightness =
				BRIGHTNESS_MAX;
			clevo_mcled_cdevs[i][j].led_cdev.brightness_set =
				&leds_set_brightness_mc;
			clevo_mcled_cdevs[i][j].led_cdev.brightness =
				BRIGHTNESS_DEFAULT;
			clevo_mcled_cdevs[i][j].num_colors = 3;
			clevo_mcled_cdevs[i][j].subled_info =
				clevo_mcled_cdevs_subleds[i][j];
			clevo_mcled_cdevs[i][j].subled_info[0].color_index =
				LED_COLOR_ID_RED;
			clevo_mcled_cdevs[i][j].subled_info[0].intensity = 0;
			clevo_mcled_cdevs[i][j].subled_info[0].channel =
				get_led_id(i, j);
			clevo_mcled_cdevs[i][j].subled_info[1].color_index =
				LED_COLOR_ID_GREEN;
			clevo_mcled_cdevs[i][j].subled_info[1].intensity = 0;
			clevo_mcled_cdevs[i][j].subled_info[1].channel =
				get_led_id(i, j);
			clevo_mcled_cdevs[i][j].subled_info[2].color_index =
				LED_COLOR_ID_BLUE;
			clevo_mcled_cdevs[i][j].subled_info[2].intensity = 0;
			clevo_mcled_cdevs[i][j].subled_info[2].channel =
				get_led_id(i, j);

			devm_led_classdev_multicolor_register(&dev->dev,
				&clevo_mcled_cdevs[i][j]);
		}
	}

	/* Create sysfs attributes for animation control */
	result = sysfs_create_groups(&dev->dev.kobj, ite829x_groups);
	if (result)
		pr_warn("failed to create sysfs attributes\n");

	register_keyboard_notifier(&keyboard_notifier_block);

	pr_info("ITE 829x keyboard backlight initialized (%d modes, %d LEDs)\n",
		EFFECT_COUNT, KEYBOARD_ROWS * KEYBOARD_COLUMNS);

	return 0;
}

static void remove_callb(struct hid_device *dev)
{
	int i, j;

	unregister_keyboard_notifier(&keyboard_notifier_block);
	sysfs_remove_groups(&dev->dev.kobj, ite829x_groups);

	for (i = 0; i < KEYBOARD_ROWS; ++i)
		for (j = 0; j < KEYBOARD_COLUMNS; ++j)
			devm_led_classdev_multicolor_unregister(&dev->dev,
				&clevo_mcled_cdevs[i][j]);

	hid_hw_power(dev, PM_HINT_NORMAL);
	kbdev = NULL;
	hid_hw_close(dev);
	hid_hw_stop(dev);

	kfree(hid_buf);
	hid_buf = NULL;

	pr_debug("driver removed\n");
}

static int driver_suspend_callb(struct device *dev)
{
	pr_debug("suspend\n");
	return 0;
}

static int driver_resume_callb(struct device *dev)
{
	pr_debug("resume\n");
	invalidate_brightness_speed_cache();
	apply_effect(kbdev);
	return 0;
}

/* --- HID driver registration --- */

static const struct hid_device_id ite829x_device_table[] = {
	{ HID_USB_DEVICE(0x048d, 0x8910) },
	{ }
};
MODULE_DEVICE_TABLE(hid, ite829x_device_table);

static struct hid_driver ite829x_driver = {
	.name = KBUILD_MODNAME,
	.probe = probe_callb,
	.remove = remove_callb,
	.id_table = ite829x_device_table,
};

static const struct dev_pm_ops ite829x_pm = {
	SET_SYSTEM_SLEEP_PM_OPS(driver_suspend_callb, driver_resume_callb)
};

static int __init ite829x_init(void)
{
	pr_debug("module init\n");

	if (dmi_match(DMI_SYS_VENDOR, "TUXEDO") &&
	    (dmi_match(DMI_PRODUCT_SKU, "SIRIUS1601") ||
	     dmi_match(DMI_PRODUCT_SKU, "SIRIUS1602")))
		return -ENODEV;

	mutex_init(&input_lock);

	ite829x_driver.driver.pm = &ite829x_pm;

	return hid_register_driver(&ite829x_driver);
}

static void __exit ite829x_exit(void)
{
	hid_unregister_driver(&ite829x_driver);
	pr_debug("module exit\n");
}

module_init(ite829x_init);
module_exit(ite829x_exit);
