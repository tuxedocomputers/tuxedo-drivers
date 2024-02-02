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
#ifndef TUXEDO_NB04_WMI_AB_H
#define TUXEDO_NB04_WMI_AB_H

#define NB04_WMI_AB_GUID	"80C9BAA6-AC48-4538-9234-9F81A55E7C85"
MODULE_ALIAS("wmi:" NB04_WMI_AB_GUID);

#define AB_INPUT_BUFFER_LENGTH_NORMAL	8
#define AB_INPUT_BUFFER_LENGTH_EXTENDED	496
#define AB_OUTPUT_BUFFER_LENGTH		80
#define AB_OUTPUT_BUFFER_LENGTH_REDUCED	10

enum wmi_return_status {
	WMI_RETURN_STATUS_SUCCESS = 0,
	WMI_RETURN_STATUS_UNSUPPORTED = 1,
	WMI_RETURN_STATUS_INVALID_PARAMETER = 2,
	WMI_RETURN_STATUS_UNDEFINED_DEVICE = 3,
	WMI_RETURN_STATUS_DEVICE_ERROR = 4,
	WMI_RETURN_STATUS_UNEXPECTED_ERROR = 5,
	WMI_RETURN_STATUS_TIMEOUT = 6,
	WMI_RETURN_STATUS_EC_BUSY = 7,
};

enum wmi_device_type_id {
	WMI_DEVICE_TYPE_ID_TOUCHPAD = 1,
	WMI_DEVICE_TYPE_ID_KEYBOARD = 2,
	WMI_DEVICE_TYPE_ID_APPDISPLAYPAGES = 3
};

enum wmi_keyboard_type {
	WMI_KEYBOARD_TYPE_NORMAL = 0,
	WMI_KEYBOARD_TYPE_PERKEY = 1,
	WMI_KEYBOARD_TYPE_4ZONE = 2,
	WMI_KEYBOARD_TYPE_WHITE = 3,
	WMI_KEYBOARD_TYPE_MAX,
};

enum wmi_keyboard_matrix {
	WMI_KEYBOARD_MATRIX_US = 0,
	WMI_KEYBOARD_MATRIX_UK = 1
};

enum wmi_color_preset {
	WMI_COLOR_PRESET_RED = 1,
	WMI_COLOR_PRESET_GREEN = 2,
	WMI_COLOR_PRESET_YELLOW = 3,
	WMI_COLOR_PRESET_BLUE = 4,
	WMI_COLOR_PRESET_PURPLE = 5,
	WMI_COLOR_PRESET_INDIGO = 6,
	WMI_COLOR_PRESET_WHITE = 7
};

struct device_keyboard_status_t {
	bool keyboard_state_enabled;
	enum wmi_keyboard_type keyboard_type;
	bool keyboard_sidebar_support;
	enum wmi_keyboard_matrix keyboard_matrix;
};

int nb04_wmi_ab_method_buffer(u32 wmi_method_id, u8 *in, u8 *out);
int nb04_wmi_ab_method_buffer_reduced_output(u32 wmi_method_id, u8 *in, u8 *out);
int nb04_wmi_ab_method_extended_input(u32 wmi_method_id, u8 *in, u8 *out);
int nb04_wmi_ab_method_int_out(u32 wmi_method_id, u8 *in, u64 *out);

int wmi_update_device_status_keyboard(struct device_keyboard_status_t *kbds);
int wmi_set_whole_keyboard(u8 red, u8 green, u8 blue, int brightness);

#endif