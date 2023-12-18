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
#ifndef TUXEDO_NB04_WMI_BS_H
#define TUXEDO_NB04_WMI_BS_H

#define NB04_WMI_BS_GUID	"1F174999-3A4E-4311-900D-7BE7166D5055"
MODULE_ALIAS("wmi:" NB04_WMI_BS_GUID);

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

#define BS_INPUT_BUFFER_LENGTH		8
#define BS_OUTPUT_BUFFER_LENGTH		80
int nb04_wmi_bs_method(u32 wmi_method_id, u8 *in, u8 *out);

#endif