/* SPDX-License-Identifier: GPL-2.0+ */
/*!
 * Copyright (c) 2024 TUXEDO Computers GmbH <tux@tuxedocomputers.com>
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

#ifndef TUXI_ACPI_H
#define TUXI_ACPI_H

#define TUXI_ACPI_RESOURCE_HID "TUXI0000"

MODULE_ALIAS("acpi*:" TUXI_ACPI_RESOURCE_HID ":*");

enum tuxi_fan_type {
	GENERAL = 0,
	CPU = 1,
	GPU = 2,
};

enum tuxi_fan_mode {
	AUTO = 0,
	MANUAL = 1,
};

int tuxi_set_fan_speed(u8 fan_index, u8 fan_speed);
int tuxi_get_fan_speed(u8 fan_index, u8 *fan_speed);
int tuxi_get_nr_fans(u8 *nr_fans);
int tuxi_set_fan_mode(enum tuxi_fan_mode mode);
int tuxi_get_fan_mode(enum tuxi_fan_mode *mode);
int tuxi_get_fan_type(u8 fan_index, enum tuxi_fan_type *type);
int tuxi_get_fan_temp(u8 index, u16 *temp);
int tuxi_get_fan_rpm(u8 index, u16 *rpm);

#endif
