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

#ifndef TUXEDO_NB05_KBD_BACKLIGHT_H
#define TUXEDO_NB05_KBD_BACKLIGHT_H
#include <linux/types.h>
void nb05_leds_notify_brightness_change_extern(u8);
#endif
