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
#ifndef TUXEDO_NB05_EC_H
#define TUXEDO_NB05_EC_H

MODULE_ALIAS("platform:tuxedo_nb05_ec");

void nb05_read_ec_ram(u16 addr, u8 *data);
void nb05_write_ec_ram(u16 addr, u8 data);
void nb05_read_ec_fw_version(u8 *major, u8 *minor);
#endif