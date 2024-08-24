/*!
 * Copyright (c) 2018-2020 TUXEDO Computers GmbH <tux@tuxedocomputers.com>
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
#ifndef CLEVO_KEYBOARD_H
#define CLEVO_KEYBOARD_H

#include <linux/power_supply.h>
#include <acpi/battery.h>
#include <linux/version.h>

#include "tuxedo_keyboard_common.h"
#include "clevo_interfaces.h"
#include "clevo_leds.h"

// Clevo event codes
#define CLEVO_EVENT_KB_LEDS_DECREASE		0x81
#define CLEVO_EVENT_KB_LEDS_INCREASE		0x82
#define CLEVO_EVENT_KB_LEDS_CYCLE_MODE		0x83
#define CLEVO_EVENT_KB_LEDS_CYCLE_BRIGHTNESS	0x8A
#define CLEVO_EVENT_KB_LEDS_TOGGLE		0x9F

#define CLEVO_EVENT_KB_LEDS_DECREASE2		0x20
#define CLEVO_EVENT_KB_LEDS_INCREASE2		0x21
#define CLEVO_EVENT_KB_LEDS_TOGGLE2		0x3f

#define CLEVO_EVENT_TOUCHPAD_TOGGLE		0x5D
#define CLEVO_EVENT_TOUCHPAD_OFF		0xFC
#define CLEVO_EVENT_TOUCHPAD_ON			0xFD

#define CLEVO_EVENT_RFKILL1			0x85
#define CLEVO_EVENT_RFKILL2			0x86

#define CLEVO_EVENT_GAUGE_KEY			0x59

#define CLEVO_EVENT_FN_LOCK_TOGGLE		0x25

#define CLEVO_KB_MODE_DEFAULT			0 // "CUSTOM"/Static Color

static struct clevo_interfaces_t {
	struct clevo_interface_t *wmi;
	struct clevo_interface_t *acpi;
} clevo_interfaces;

static struct clevo_interface_t *active_clevo_interface;

static struct tuxedo_keyboard_driver clevo_keyboard_driver;

static DEFINE_MUTEX(clevo_keyboard_interface_modification_lock);

static struct key_entry clevo_keymap[] = {
	// Keyboard backlight (RGB versions)
	{ KE_KEY, CLEVO_EVENT_KB_LEDS_DECREASE, { KEY_KBDILLUMDOWN } },
	{ KE_KEY, CLEVO_EVENT_KB_LEDS_INCREASE, { KEY_KBDILLUMUP } },
	{ KE_KEY, CLEVO_EVENT_KB_LEDS_TOGGLE, { KEY_KBDILLUMTOGGLE } },
	{ KE_KEY, CLEVO_EVENT_KB_LEDS_CYCLE_MODE, { KEY_LIGHTS_TOGGLE } },
	// Single cycle key (white only versions) (currently handled in driver)
	// { KE_KEY, CLEVO_EVENT_KB_LEDS_CYCLE_BRIGHTNESS, { KEY_KBDILLUMTOGGLE } },

	// Alternative events (ex. 6 step white kbd)
	{ KE_KEY, CLEVO_EVENT_KB_LEDS_DECREASE2, { KEY_KBDILLUMDOWN } },
	{ KE_KEY, CLEVO_EVENT_KB_LEDS_INCREASE2, { KEY_KBDILLUMUP } },
	{ KE_KEY, CLEVO_EVENT_KB_LEDS_TOGGLE2, { KEY_KBDILLUMTOGGLE } },

	// Touchpad
	// The weirdly named touchpad toggle key that is implemented as KEY_F21 "everywhere"
	// (instead of KEY_TOUCHPAD_TOGGLE or on/off)
	// Most "new" devices just provide one toggle event
	{ KE_KEY, CLEVO_EVENT_TOUCHPAD_TOGGLE, { KEY_F21 } },
	// Some "old" devices produces on/off events
	{ KE_KEY, CLEVO_EVENT_TOUCHPAD_OFF, { KEY_F21 } },
	{ KE_KEY, CLEVO_EVENT_TOUCHPAD_ON, { KEY_F21 } },
	// The alternative key events (currently not used)
	// { KE_KEY, CLEVO_EVENT_TOUCHPAD_OFF, { KEY_TOUCHPAD_OFF } },
	// { KE_KEY, CLEVO_EVENT_TOUCHPAD_ON, { KEY_TOUCHPAD_ON } },
	// { KE_KEY, CLEVO_EVENT_TOUCHPAD_TOGGLE, { KEY_TOUCHPAD_TOGGLE } },

	// Rfkill still needed by some devices
	{ KE_KEY, CLEVO_EVENT_RFKILL1, { KEY_RFKILL } },
	{ KE_IGNORE, CLEVO_EVENT_RFKILL2, { KEY_RFKILL } }, // Older rfkill event
	// Note: Volume events need to be ignored as to not interfere with built-in functionality
	{ KE_IGNORE, 0xfa, { KEY_UNKNOWN } }, // Appears by volume up/down
	{ KE_IGNORE, 0xfb, { KEY_UNKNOWN } }, // Appears by mute toggle

	// Only used to put ev bits
	{ KE_KEY,	0xffff,				{ KEY_F6 } },
	{ KE_KEY,	0xffff,				{ KEY_LEFTALT } },
	{ KE_KEY,	0xffff,				{ KEY_LEFTMETA } },

	{ KE_END, 0 }
};

// Keyboard struct
static struct kbd_led_state_t {
	u8 has_mode;
	u8 mode;
	u8 whole_kbd_color;
} kbd_led_state = {
	.has_mode = 1,
	.mode = CLEVO_KB_MODE_DEFAULT,
	.whole_kbd_color = 7,
};

static struct kbd_backlight_mode_t {
	u8 key;
	u32 value;
	const char *const name;
} kbd_backlight_modes[] = {
        { .key = 0, .value = 0x00000000, .name = "CUSTOM"},
        { .key = 1, .value = 0x1002a000, .name = "BREATHE"},
        { .key = 2, .value = 0x33010000, .name = "CYCLE"},
        { .key = 3, .value = 0x80000000, .name = "DANCE"},
        { .key = 4, .value = 0xA0000000, .name = "FLASH"},
        { .key = 5, .value = 0x70000000, .name = "RANDOM_COLOR"},
        { .key = 6, .value = 0x90000000, .name = "TEMPO"},
        { .key = 7, .value = 0xB0000000, .name = "WAVE"}
};

// forward declaration
static int clevo_acpi_fn_get(u8 *on, u8 *kbstatus1, u8 *kbstatus2);
static int clevo_acpi_fn_lock_set(int on);

int clevo_evaluate_method2(u8 cmd, u32 arg, union acpi_object **result)
{
	if (IS_ERR_OR_NULL(active_clevo_interface)) {
		pr_err("clevo_keyboard: no active interface while attempting cmd %02x arg %08x\n", cmd, arg);
		return -ENODEV;
	}
	return active_clevo_interface->method_call(cmd, arg, result);
}
EXPORT_SYMBOL(clevo_evaluate_method2);

int clevo_evaluate_method_pkgbuf(u8 cmd, u8 *arg, u32 length, union acpi_object **result)
{
	if (IS_ERR_OR_NULL(active_clevo_interface)) {
		pr_err("clevo_keyboard: no active interface while attempting cmd %02x\n", cmd);
		return -ENODEV;
	}
	return active_clevo_interface->method_call_pkgbuf(cmd, arg, length, result);
}
EXPORT_SYMBOL(clevo_evaluate_method_pkgbuf);

int clevo_evaluate_method(u8 cmd, u32 arg, u32 *result)
{
	int status = 0;
	union acpi_object *out_obj;

	status = clevo_evaluate_method2(cmd, arg, &out_obj);
	if (status) {
		return status;
	}
	else {
		if (out_obj->type == ACPI_TYPE_INTEGER) {
			if (!IS_ERR_OR_NULL(result))
				*result = (u32) out_obj->integer.value;
		} else {
			pr_err("return type not integer, use clevo_evaluate_method2\n");
			status = -ENODATA;
		}
		ACPI_FREE(out_obj);
	}

	return status;
}
EXPORT_SYMBOL(clevo_evaluate_method);

int clevo_get_active_interface_id(char **id_str)
{
	if (IS_ERR_OR_NULL(active_clevo_interface))
		return -ENODEV;

	if (!IS_ERR_OR_NULL(id_str))
		*id_str = active_clevo_interface->string_id;

	return 0;
}
EXPORT_SYMBOL(clevo_get_active_interface_id);

static void set_next_color_whole_kb(void)
{
	/* "Calculate" new to-be color */
	u32 new_color_id;
	u32 new_color_code;

	new_color_id = kbd_led_state.whole_kbd_color + 1;
	if (new_color_id >= color_list.size) {
		new_color_id = 1; // Skip black
	}
	new_color_code = color_list.colors[new_color_id].code;

	TUXEDO_INFO("set_next_color_whole_kb(): new_color_id: %i, new_color_code %X", 
		    new_color_id, new_color_code);

	/* Set color on all four regions*/
	clevo_leds_set_color_extern(new_color_code);
	kbd_led_state.whole_kbd_color = new_color_id;
}

static void set_kbd_backlight_mode(u8 kbd_backlight_mode)
{
	TUXEDO_INFO("Set keyboard backlight mode on %s", kbd_backlight_modes[kbd_backlight_mode].name);

	if (!clevo_evaluate_method(CLEVO_CMD_SET_KB_RGB_LEDS, kbd_backlight_modes[kbd_backlight_mode].value, NULL)) {
		// method was succesfull so update ur internal state struct
		kbd_led_state.mode = kbd_backlight_mode;
	}
}

// Sysfs Interface for the keyboard backlight mode
static ssize_t list_kbd_backlight_modes_fs(struct device *child, struct device_attribute *attr,
                                         char *buffer)
{
	return sprintf(buffer, "%d\n", kbd_led_state.mode);
}

static ssize_t set_kbd_backlight_mode_fs(struct device *child,
                                       struct device_attribute *attr,
                                       const char *buffer, size_t size)
{
	unsigned int kbd_backlight_mode;

	int err = kstrtouint(buffer, 0, &kbd_backlight_mode);
	if (err) {
		return err;
	}

	kbd_backlight_mode = clamp_t(u8, kbd_backlight_mode, 0, ARRAY_SIZE(kbd_backlight_modes) - 1);
	set_kbd_backlight_mode(kbd_backlight_mode);

	return size;
}

// Sysfs attribute file permissions and method linking
static DEVICE_ATTR(kbd_backlight_mode, 0644, list_kbd_backlight_modes_fs, set_kbd_backlight_mode_fs);

static int kbd_backlight_mode_id_validator(const char *value,
					   const struct kernel_param *kbd_backlight_mode_param)
{
	int kbd_backlight_mode = 0;

	if (kstrtoint(value, 10, &kbd_backlight_mode) != 0
	    || kbd_backlight_mode < 0
	    || kbd_backlight_mode > (ARRAY_SIZE(kbd_backlight_modes) - 1)) {
		return -EINVAL;
	}

	return param_set_int(value, kbd_backlight_mode_param);
}

static const struct kernel_param_ops param_ops_mode_ops = {
	.set = kbd_backlight_mode_id_validator,
	.get = param_get_int,
};

static u8 param_kbd_backlight_mode = CLEVO_KB_MODE_DEFAULT;
module_param_cb(kbd_backlight_mode, &param_ops_mode_ops, &param_kbd_backlight_mode, S_IRUSR);
MODULE_PARM_DESC(kbd_backlight_mode, "Set the keyboard backlight mode");

static void clevo_send_cc_combo(void)
{
	// Special key combination. Opens TCC by default when installed.
	input_report_key(clevo_keyboard_driver.input_device, KEY_LEFTMETA, 1);
	input_report_key(clevo_keyboard_driver.input_device, KEY_LEFTALT, 1);
	input_report_key(clevo_keyboard_driver.input_device, KEY_F6, 1);
	input_sync(clevo_keyboard_driver.input_device);
	input_report_key(clevo_keyboard_driver.input_device, KEY_F6, 0);
	input_report_key(clevo_keyboard_driver.input_device, KEY_LEFTALT, 0);
	input_report_key(clevo_keyboard_driver.input_device, KEY_LEFTMETA, 0);
	input_sync(clevo_keyboard_driver.input_device);
}

static void clevo_keyboard_event_callb(u32 event)
{
	int err;
	u8 on, kbstatus1, kbstatus2;
	u32 key_event = event;

	TUXEDO_DEBUG("Clevo event: %0#6x\n", event);

	switch (key_event) {
		case CLEVO_EVENT_GAUGE_KEY:
			clevo_send_cc_combo();
			break;
		case CLEVO_EVENT_KB_LEDS_CYCLE_MODE:
			if (clevo_leds_get_backlight_type() == CLEVO_KB_BACKLIGHT_TYPE_FIXED_COLOR) {
				clevo_send_cc_combo();
			} else {
				set_next_color_whole_kb();
			}
			break;
		case CLEVO_EVENT_KB_LEDS_CYCLE_BRIGHTNESS:
			clevo_leds_notify_brightness_change_extern();
			break;
		case CLEVO_EVENT_FN_LOCK_TOGGLE:
			err = clevo_acpi_fn_get(&on, &kbstatus1, &kbstatus2);
			if (err) {
				TUXEDO_ERROR("Error while reading ACPI fn lock; ignoring toggle request");
			}
			else {
				if (on == 1)
					clevo_acpi_fn_lock_set(0);
				else
					clevo_acpi_fn_lock_set(1);
			}
			break;
		default:
			break;
	}

	if (current_driver != NULL && current_driver->input_device != NULL) {
		if (!sparse_keymap_report_known_event(current_driver->input_device, key_event, 1, true)) {
			TUXEDO_DEBUG("Unknown key - %d (%0#6x)\n", key_event, key_event);
		}
	}
}

static void clevo_keyboard_init_device_interface(struct platform_device *dev)
{
	// Setup sysfs
	if (clevo_leds_get_backlight_type() == CLEVO_KB_BACKLIGHT_TYPE_3_ZONE_RGB) {
		if (device_create_file(&dev->dev, &dev_attr_kbd_backlight_mode) != 0) {
			TUXEDO_ERROR("Sysfs attribute file creation failed for blinking pattern\n");
		}
		else {
			kbd_led_state.has_mode = 1;
		}
	}
}

/**
 * strstr version of dmi_match
 */
static bool dmi_string_in(enum dmi_field f, const char *str)
{
	const char *info = dmi_get_system_info(f);

	if (info == NULL || str == NULL)
		return info == str;

	return strstr(info, str) != NULL;
}

// Fn lock

static int clevo_acpi_fn_get(u8 *on, u8 *kbstatus1, u8 *kbstatus2)
{
	int status = 0;
	union acpi_object *out_obj;
	u8 fnlock;

	status = clevo_evaluate_method2(0x4, 0x18, &out_obj);
	if (status) {
		return status;
	}

	if (out_obj->type == ACPI_TYPE_BUFFER ) {
		if (out_obj->buffer.length >= 3) {
			// use buffer
			fnlock = out_obj->buffer.pointer[0];
			if (fnlock == 1) {
				pr_debug("FnLock is set: kbstatus[1]: %08x  kbstatus[2]: %08x\n", (u32) out_obj->buffer.pointer[1], (u32)out_obj->buffer.pointer[2]);
				*on = 1;
			}
			if (fnlock == 2) {
				pr_debug("FnLock is not set: kbstatus[1]: %08x  kbstatus[2]: %08x\n", (u32) out_obj->buffer.pointer[1], (u32)out_obj->buffer.pointer[2]);
				*on = 0;
			}
			if (fnlock != 1 && fnlock != 2) {
				pr_err("FnLock undefined: 0x%x\n", fnlock);
				status = -ENODATA;
			}
			*kbstatus1 = out_obj->buffer.pointer[1];
			*kbstatus2 = out_obj->buffer.pointer[2];
		}
		else {
			pr_err("unexpected buffer size %d\n", out_obj->buffer.length);
		}
		ACPI_FREE(out_obj);
	}

	return status;
}

static int clevo_acpi_fn_lock_set(int on)
{
	int status = 0;
	union acpi_object *out_obj;
	u8 fnlock_on, kbstatus1, kbstatus2;

	// cmd 0x19; fnlock_off = 0x02, fnlock_on = 0x01
	u8 args[] = {0x19, 0x02, 0x00, 0x00};

	if (on)
		args[1] = 0x01;

	// read keyboard status first
	status = clevo_acpi_fn_get(&fnlock_on, &kbstatus1, &kbstatus2);
	if (status)
		return status;

	args[2] = kbstatus1;
	args[3] = kbstatus2;

	// set Fn lock
	status = clevo_evaluate_method_pkgbuf(0x4, args, 4, &out_obj);

	if (status)
		pr_err("set/unset FnLock failed\n");
	else
		ACPI_FREE(out_obj);

	return status;
}

static ssize_t clevo_fn_lock_show(struct device *dev,
		struct device_attribute *attr,
		char *buf)
{
	int err;
	u8 on, kbstatus1, kbstatus2;

	err = clevo_acpi_fn_get(&on, &kbstatus1, &kbstatus2);
	if (err)
		return err;

	return sprintf(buf, "%d\n", on);
}

static ssize_t clevo_fn_lock_store(struct device *dev,
		struct device_attribute *attr,
		const char *buf, size_t size)
{
	int on, err;

	if (kstrtoint(buf, 10, &on) ||
			on < 0 || on > 1)
		return -EINVAL;

	err = clevo_acpi_fn_lock_set(on);
	if (err)
		return err;

	return size;
}

bool clevo_fn_lock_available(void){
	int err;
	union acpi_object *out_obj;
	u8 fnlock;

	// no sysfs device for Aura Gen3 due to Fn Lock interference (via keyboard)
	// but Aura Gen3 refresh (NL45AU2 und NL57AU) has working Fn Lock
	if ((dmi_match(DMI_PRODUCT_SKU, "AURA14GEN3") ||
	     dmi_match(DMI_PRODUCT_SKU, "AURA15GEN3")) &&
	    (dmi_match(DMI_BOARD_NAME, "NL57PU") ||
	     dmi_match(DMI_BOARD_NAME, "NL45PU2")))
			return 0;

	// check Fn lock for WMI
	if( strcmp(active_clevo_interface->string_id, CLEVO_INTERFACE_WMI_STRID) == 0) {
		pr_debug("FnLock test: WMI is not supported\n");
		return 0;
	}

	// check Fn Lock for ACPI
	if( strcmp(active_clevo_interface->string_id, CLEVO_INTERFACE_ACPI_STRID) == 0) {
		err = clevo_evaluate_method2(0x4, 0x18, &out_obj);
		if (err) {
			pr_debug("FnLock test: ACPI evaluate returned an error\n");
			return 0;
		}

		if (out_obj->type != ACPI_TYPE_BUFFER ) {
			pr_debug("FnLock test: no ACPI buffer\n");
			return 0;
		}
		if (out_obj->buffer.length < 3) {
			pr_debug("FnLock test: ACPI buffer too small\n");
			ACPI_FREE(out_obj);
			return 0;
		}
		fnlock = out_obj->buffer.pointer[0];
		if (fnlock != 1 && fnlock != 2) {
			pr_debug("FnLock test: unexpected FnLock value 0x%02x\n", fnlock);
			ACPI_FREE(out_obj);
			return 0;
		}
		ACPI_FREE(out_obj);
		return 1;
	}

	return 0;
}

static u8 clevo_legacy_flexicharger_start_values[] = {40, 50, 60, 70, 80, 95};
static u8 clevo_legacy_flexicharger_end_values[] = {60, 70, 80, 90, 100};

static int array_find_closest_u8(u8 value, u8 *haystack, size_t length)
{
	int i;
	u8 closest;

	if (length == 0)
		return -EINVAL;

	closest = haystack[0];
	for (i = 0; i < length; ++i) {
		if (abs(value - haystack[i]) < abs(value - closest))
			closest = haystack[i];
	}

	return closest;
}

static int clevo_has_legacy_flexicharger(bool *status)
{
	u32 read_data = 0;
	u32 write_data = 0x06000000;
	int result;

	// Known exclude list
	bool excluded_device = false
		|| dmi_string_in(DMI_BOARD_NAME, "N24_25BU")
		|| dmi_string_in(DMI_BOARD_NAME, "L14xMU")
		|| dmi_string_in(DMI_BOARD_NAME, "N141CU")
		|| dmi_string_in(DMI_BOARD_NAME, "NH5xAx")
		|| dmi_string_in(DMI_BOARD_NAME, "NL5xNU")
		|| dmi_string_in(DMI_BOARD_NAME, "P95xER")
		|| dmi_string_in(DMI_BOARD_NAME, "PCX0DX")
		|| dmi_string_in(DMI_BOARD_NAME, "PD5x_7xPNP_PNR_PNN_PNT")
		|| dmi_string_in(DMI_BOARD_NAME, "X170SM")
		|| dmi_string_in(DMI_BOARD_NAME, "NS50MU")
		|| dmi_string_in(DMI_BOARD_NAME, "NS50_70MU")
		;

	if (excluded_device) {
		*status = false;
		return 0;
	}

	// Use a combination of read and write read values to identify legacy flexicharger
	// Using set command should read the command back as result if existing
	result = clevo_evaluate_method(0x77, 0, &read_data);
	if (result != 0)
		return result;

	write_data |= ((read_data >> 8) & 0xffff);
	result = clevo_evaluate_method(0x76, write_data, &read_data);

	if (read_data == 0x76)
		*status = true;
	else
		*status = false;
	
	return result;
}

/**
 * Read legacy flexicharger data. If successful parameter contains result.
 * 
 * @param start Start threshold
 * @param end End threshold
 * @param status On or Off (1 or 0)
 */
static int clevo_legacy_flexicharger_read(u8 *start, u8 *end, u8 *status)
{
	/*
	 * Data bytes
	 *            end      start    on/off
	 * |--------|--------|--------|--------|
	 */
	u32 data;
	int result;

	result = clevo_evaluate_method(0x77, 0, &data);
	if (end != NULL)
		*end = (data >> 0x10) & 0xff;
	if (start != NULL)
		*start = (data >> 0x08) & 0xff;
	if (status != NULL)
		*status = data & 0x01;

	return result;
}

/**
 * Write flexicharger data.
 * 
 * @param start Start threshold
 * @param end End threshold
 * @param status On or Off (1 or 0)
 */
static int clevo_legacy_flexicharger_write(const u8 *param_start, const u8 *param_end, const u8 *param_status)
{
	// Two different subcommands for writing
	u32 write_data_thresholds = (0x06 << 0x18);
	u32 write_data_status = (0x05 << 0x18);

	u8 previous_start, previous_end, previous_status;
	u8 set_start, set_end, set_status;

	if (clevo_legacy_flexicharger_read(&previous_start, &previous_end, &previous_status) != 0)
		return -EIO;

	// Set choosen parameters, leave nulled ones with previous value
	set_start = param_start != NULL ? *param_start : previous_start;
	set_end = param_end != NULL ? *param_end : previous_end;
	set_status = param_status != NULL ? *param_status : previous_status;

	write_data_thresholds |= set_start;
	write_data_thresholds |= (set_end << 0x08);
	write_data_status |= set_status & 0x01;

	// Write to EC, note that status go last as it also triggers save
	if (clevo_evaluate_method(0x76, write_data_thresholds, NULL) != 0)
		return -EIO;

	if (clevo_evaluate_method(0x76, write_data_status, NULL) != 0)
		return -EIO;
	
	return 0;
}

static int clevo_cc4_flexicharger_read(u8 *start, u8 *end, u8 *status)
{
	int result;
	union acpi_object *out_obj;

	result = clevo_evaluate_method2(0x04, 0x1e, &out_obj);
	if (result)
		return -EIO;

	if (out_obj->type != ACPI_TYPE_BUFFER ||
	    out_obj->buffer.length < 3) {
		ACPI_FREE(out_obj);
		return -EIO;
	}

	if (out_obj->buffer.pointer[2] == 0 &&
	    out_obj->buffer.pointer[1] == 0) {
		ACPI_FREE(out_obj);
		return -ENODEV;
	}

	if (end != NULL)
		*end = out_obj->buffer.pointer[2];
	if (start != NULL)
		*start = out_obj->buffer.pointer[1];
	if (status != NULL)
		*status = out_obj->buffer.pointer[0];

	ACPI_FREE(out_obj);

	return result;
}

static int clevo_has_cc4_flexicharger(bool *status)
{
	if (clevo_cc4_flexicharger_read(NULL, NULL, NULL))
		*status = false;
	else
		*status = true;

	return 0;
}

static int clevo_cc4_flexicharger_write(const u8 *param_start,
					const u8 *param_end,
					const u8 *param_status)
{
	union acpi_object *dummy_out;
	u8 previous_start, previous_end, previous_status;
	u8 set_start, set_end, set_status;
	u8 buffer_set[0xff] = { 0x1f };

	if (clevo_cc4_flexicharger_read(&previous_start, &previous_end, &previous_status))
		return -EIO;

	// Set choosen parameters, leave nulled ones with previous value
	set_start = param_start != NULL ? *param_start : previous_start;
	set_end = param_end != NULL ? *param_end : previous_end;
	set_status = param_status != NULL ? *param_status : previous_status;

	buffer_set[1] = set_status;
	buffer_set[2] = set_start;
	buffer_set[3] = set_end;

	if (clevo_evaluate_method_pkgbuf(0x04, buffer_set, ARRAY_SIZE(buffer_set), &dummy_out))
		return -EIO;

	ACPI_FREE(dummy_out);

	return 0;
}

static int clevo_flexicharger_read(u8 *start, u8 *end, u8 *status)
{
	bool has_cc4_flexicharger;
	clevo_has_cc4_flexicharger(&has_cc4_flexicharger);
	if (has_cc4_flexicharger)
		return clevo_cc4_flexicharger_read(start, end, status);
	else
		return clevo_legacy_flexicharger_read(start, end, status);
}

static int clevo_flexicharger_write(const u8 *param_start,
				    const u8 *param_end,
				    const u8 *param_status)
{
	bool has_cc4_flexicharger;
	clevo_has_cc4_flexicharger(&has_cc4_flexicharger);
	if (has_cc4_flexicharger)
		return clevo_cc4_flexicharger_write(param_start, param_end, param_status);
	else
		return clevo_legacy_flexicharger_write(param_start, param_end, param_status);
}

static ssize_t charge_type_show(struct device *device,
				struct device_attribute *attr,
				char *buf)
{
	int result;
	u8 status;

	result = clevo_flexicharger_read(NULL, NULL, &status);

	if (result != 0)
		return result;

	if (status == 1)
		return sprintf(buf, "%s\n", "Custom");
	else
		return sprintf(buf, "%s\n", "Standard");

}

static ssize_t charge_type_store(struct device *dev,
				 struct device_attribute *attr,
				 const char *buf,
				 size_t count)
{
	u8 write_status;
	int result;

	if (sysfs_streq(buf, "Standard"))
		write_status = 0;
	else if (sysfs_streq(buf, "Custom"))
		write_status = 1;
	else
		return -EINVAL;

	result = clevo_flexicharger_write(NULL, NULL, &write_status);

	if (result < 0)
		return result;
	else
		return count;
}

static ssize_t charge_control_start_threshold_show(struct device *device,
						   struct device_attribute *attr,
						   char *buf)
{
	int result;
	u8 start_threshold;

	result = clevo_flexicharger_read(&start_threshold, NULL, NULL);

	if (result != 0)
		return result;

	return sprintf(buf, "%d\n", start_threshold);
}

static ssize_t charge_control_start_threshold_store(struct device *dev,
						    struct device_attribute *attr,
						    const char *buf,
						    size_t count)
{
	u8 value, write_value;
	int result;

	result = kstrtou8(buf, 10, &value);
	if (result)
		return result;

	if (value < 1 || value > 100)
		return -EINVAL;
	
	write_value = array_find_closest_u8(value,
					    clevo_legacy_flexicharger_start_values,
					    ARRAY_SIZE(clevo_legacy_flexicharger_start_values));
	result = clevo_flexicharger_write(&write_value, NULL, NULL);

	if (result < 0)
		return result;
	else
		return count;
}

static ssize_t charge_control_end_threshold_show(struct device *device,
						 struct device_attribute *attr,
						 char *buf)
{
	int result;
	u8 end_threshold;

	result = clevo_flexicharger_read(NULL, &end_threshold, NULL);

	if (result != 0)
		return result;

	return sprintf(buf, "%d\n", end_threshold);
}

static ssize_t charge_control_end_threshold_store(struct device *dev,
						  struct device_attribute *attr,
						  const char *buf,
						  size_t count)
{
	u8 value, write_value;
	int result;

	result = kstrtou8(buf, 10, &value);
	if (result)
		return result;

	if (value < 1 || value > 100)
		return -EINVAL;

	write_value = array_find_closest_u8(value,
					    clevo_legacy_flexicharger_end_values,
					    ARRAY_SIZE(clevo_legacy_flexicharger_end_values));
	result = clevo_flexicharger_write(NULL, &write_value, NULL);


	if (result < 0)
		return result;
	else
		return count;
}

static ssize_t charge_control_start_available_thresholds_show(struct device *device,
							    struct device_attribute *attr,
							    char *buf)
{
	int i;
	ssize_t length = ARRAY_SIZE(clevo_legacy_flexicharger_start_values);

	for (i = 0; i < length; ++i) {
		sprintf(buf + strlen(buf), "%d",
			clevo_legacy_flexicharger_start_values[i]);
		if (i < length - 1)
			sprintf(buf + strlen(buf), " ");
		else
			sprintf(buf + strlen(buf), "\n");
	}

	return strlen(buf);
}

static ssize_t charge_control_end_available_thresholds_show(struct device *device,
							    struct device_attribute *attr,
							    char *buf)
{
	int i;
	ssize_t length = ARRAY_SIZE(clevo_legacy_flexicharger_end_values);

	for (i = 0; i < length; ++i) {
		sprintf(buf + strlen(buf), "%d",
			clevo_legacy_flexicharger_end_values[i]);
		if (i < length - 1)
			sprintf(buf + strlen(buf), " ");
		else
			sprintf(buf + strlen(buf), "\n");
	}

	return strlen(buf);
}

// Official attributes
static DEVICE_ATTR_RW(charge_type);
static DEVICE_ATTR_RW(charge_control_start_threshold);
static DEVICE_ATTR_RW(charge_control_end_threshold);

// Unofficial attributes
static DEVICE_ATTR_RO(charge_control_start_available_thresholds);
static DEVICE_ATTR_RO(charge_control_end_available_thresholds);

static bool charge_control_registered = false;

static struct attribute *clevo_battery_attrs[] = {
	&dev_attr_charge_type.attr,
	&dev_attr_charge_control_start_threshold.attr,
	&dev_attr_charge_control_end_threshold.attr,
	&dev_attr_charge_control_start_available_thresholds.attr,
	&dev_attr_charge_control_end_available_thresholds.attr,
	NULL,
};

ATTRIBUTE_GROUPS(clevo_battery);

#if LINUX_VERSION_CODE < KERNEL_VERSION(6, 2, 0)
static int clevo_battery_add(struct power_supply *battery)
#else
static int clevo_battery_add(struct power_supply *battery, struct acpi_battery_hook *hook)
#endif
{
	bool has_legacy_flexicharger;
	bool has_cc4_flexicharger;

	clevo_has_legacy_flexicharger(&has_legacy_flexicharger);
	clevo_has_cc4_flexicharger(&has_cc4_flexicharger);

	if (has_cc4_flexicharger) {
		pr_debug("cc4 flexicharger identified\n");
	}

	if (has_legacy_flexicharger) {
		pr_debug("legacy flexicharger identified\n");
	}

	// Check support and type
	if (!has_legacy_flexicharger && !has_cc4_flexicharger)
		return -ENODEV;

	if (device_add_groups(&battery->dev, clevo_battery_groups))
		return -ENODEV;

	charge_control_registered = true;

	return 0;
}

#if LINUX_VERSION_CODE < KERNEL_VERSION(6, 2, 0)
static int clevo_battery_remove(struct power_supply *battery)
#else
static int clevo_battery_remove(struct power_supply *battery, struct acpi_battery_hook *hook)
#endif
{
	device_remove_groups(&battery->dev, clevo_battery_groups);
	return 0;
}

static struct acpi_battery_hook battery_hook = {
	.add_battery = clevo_battery_add,
	.remove_battery = clevo_battery_remove,
	.name = "TUXEDO Flexicharger Extension",
};

static void clevo_flexicharger_init(void)
{
	battery_hook_register(&battery_hook);
}

static void clevo_flexicharger_remove(void)
{
	if (charge_control_registered)
		battery_hook_unregister(&battery_hook);
}

static void clevo_keyboard_init(void)
{
	bool performance_profile_set_workaround;

	kbd_led_state.mode = param_kbd_backlight_mode;
	set_kbd_backlight_mode(kbd_led_state.mode);

	clevo_evaluate_method(CLEVO_CMD_SET_EVENTS_ENABLED, 0, NULL);

	// Workaround for firmware issue not setting selected performance profile.
	// Explicitly set "performance" perf. profile on init regardless of what is chosen
	// for these devices (Aura, XP14, IBS14v5)
	performance_profile_set_workaround = false
		|| dmi_string_in(DMI_BOARD_NAME, "AURA1501")
		|| dmi_string_in(DMI_BOARD_NAME, "EDUBOOK1502")
		|| dmi_string_in(DMI_BOARD_NAME, "NL5xRU")
		|| dmi_string_in(DMI_BOARD_NAME, "NV4XMB,ME,MZ")
		|| dmi_string_in(DMI_BOARD_NAME, "L140CU")
		|| dmi_string_in(DMI_BOARD_NAME, "NS50MU")
		|| dmi_string_in(DMI_BOARD_NAME, "NS50_70MU")
		|| dmi_string_in(DMI_BOARD_NAME, "PCX0DX")
		|| dmi_string_in(DMI_BOARD_NAME, "PCx0Dx_GN20")
		|| dmi_string_in(DMI_BOARD_NAME, "L14xMU")
		;
	if (performance_profile_set_workaround) {
		TUXEDO_INFO("Performance profile 'performance' set workaround applied\n");
		clevo_evaluate_method(CLEVO_CMD_OPT, 0x19000002, NULL);
	}

	clevo_flexicharger_init();
}

static int clevo_keyboard_probe(struct platform_device *dev)
{
	clevo_leds_init(dev);
	// clevo_keyboard_init_device_interface() must come after clevo_leds_init()
	// to know keyboard backlight type
	clevo_keyboard_init_device_interface(dev);
	clevo_keyboard_init();

	return 0;
}

static void clevo_keyboard_remove_device_interface(struct platform_device *dev)
{
	if (kbd_led_state.has_mode == 1) {
		device_remove_file(&dev->dev, &dev_attr_kbd_backlight_mode);
	}
}

#if LINUX_VERSION_CODE < KERNEL_VERSION(6, 11, 0)
static int clevo_keyboard_remove(struct platform_device *dev)
#else
static void clevo_keyboard_remove(struct platform_device *dev)
#endif
{
	clevo_flexicharger_remove();
	clevo_keyboard_remove_device_interface(dev);
	clevo_leds_remove(dev);
#if LINUX_VERSION_CODE < KERNEL_VERSION(6, 11, 0)
	return 0;
#endif
}

static int clevo_keyboard_suspend(struct platform_device *dev, pm_message_t state)
{
	clevo_leds_suspend(dev);
	return 0;
}

static int clevo_keyboard_resume(struct platform_device *dev)
{
	clevo_evaluate_method(CLEVO_CMD_SET_EVENTS_ENABLED, 0, NULL);
	clevo_leds_restore_state_extern(); // Sometimes clevo devices forget their last state after
					   // suspend, so let the kernel ensure it.
	clevo_leds_resume(dev);
	return 0;
}

static struct platform_driver platform_driver_clevo = {
	.remove = clevo_keyboard_remove,
	.suspend = clevo_keyboard_suspend,
	.resume = clevo_keyboard_resume,
	.driver =
		{
			.name = DRIVER_NAME,
			.owner = THIS_MODULE,
		},
};

static struct tuxedo_keyboard_driver clevo_keyboard_driver = {
	.platform_driver = &platform_driver_clevo,
	.probe = clevo_keyboard_probe,
	.key_map = clevo_keymap,
	.fn_lock_available = clevo_fn_lock_available,
	.fn_lock_show = clevo_fn_lock_show,
	.fn_lock_store = clevo_fn_lock_store,
};

int clevo_keyboard_add_interface(struct clevo_interface_t *new_interface)
{
	mutex_lock(&clevo_keyboard_interface_modification_lock);

	if (strcmp(new_interface->string_id, CLEVO_INTERFACE_WMI_STRID) == 0) {
		clevo_interfaces.wmi = new_interface;
		clevo_interfaces.wmi->event_callb = clevo_keyboard_event_callb;

		// Only use wmi if there is no other current interface
		if (ZERO_OR_NULL_PTR(active_clevo_interface)) {
			pr_debug("enable wmi events\n");
			clevo_interfaces.wmi->method_call(CLEVO_CMD_SET_EVENTS_ENABLED, 0, NULL);

			active_clevo_interface = clevo_interfaces.wmi;
		}
	} else if (strcmp(new_interface->string_id, CLEVO_INTERFACE_ACPI_STRID) == 0) {
		clevo_interfaces.acpi = new_interface;
		clevo_interfaces.acpi->event_callb = clevo_keyboard_event_callb;

		pr_debug("enable acpi events (takes priority)\n");
		clevo_interfaces.acpi->method_call(CLEVO_CMD_SET_EVENTS_ENABLED, 0, NULL);
		active_clevo_interface = clevo_interfaces.acpi;
	} else {
		// Not recognized interface
		pr_err("unrecognized interface\n");
		mutex_unlock(&clevo_keyboard_interface_modification_lock);
		return -EINVAL;
	}

	mutex_unlock(&clevo_keyboard_interface_modification_lock);

	if (active_clevo_interface != NULL)
		tuxedo_keyboard_init_driver(&clevo_keyboard_driver);

	return 0;
}
EXPORT_SYMBOL(clevo_keyboard_add_interface);

int clevo_keyboard_remove_interface(struct clevo_interface_t *interface)
{
	mutex_lock(&clevo_keyboard_interface_modification_lock);

	if (strcmp(interface->string_id, CLEVO_INTERFACE_WMI_STRID) == 0) {
		clevo_interfaces.wmi = NULL;
	} else if (strcmp(interface->string_id, CLEVO_INTERFACE_ACPI_STRID) == 0) {
		clevo_interfaces.acpi = NULL;
	} else {
		mutex_unlock(&clevo_keyboard_interface_modification_lock);
		return -EINVAL;
	}

	if (active_clevo_interface == interface) {
		tuxedo_keyboard_remove_driver(&clevo_keyboard_driver);
		active_clevo_interface = NULL;
	}


	mutex_unlock(&clevo_keyboard_interface_modification_lock);

	return 0;
}
EXPORT_SYMBOL(clevo_keyboard_remove_interface);

#endif // CLEVO_KEYBOARD_H
