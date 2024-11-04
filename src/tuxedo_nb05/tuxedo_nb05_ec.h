/* SPDX-License-Identifier: GPL-3.0+ */
/*!
 * Copyright (c) 2023-2024 TUXEDO Computers GmbH <tux@tuxedocomputers.com>
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

#define PULSE1403 "TUXEDO Pulse 14 Gen3"
#define PULSE1404 "TUXEDO Pulse 14 Gen4"
#define IFLX14I01 "TUXEDO InfinityFlex 14 Gen1"

MODULE_ALIAS("platform:tuxedo_nb05_ec");

struct nb05_ec_data_t {
	u8 ver_major;
	u8 ver_minor;
	struct nb05_device_data_t *dev_data;
};

struct nb05_device_data_t {
	int number_fans;
	bool fanctl_onereg;
};

void nb05_read_ec_ram(u16 addr, u8 *data);
void nb05_write_ec_ram(u16 addr, u8 data);
void nb05_read_ec_fw_version(u8 *major, u8 *minor);
void nb05_get_ec_data(struct nb05_ec_data_t **ec_data);

const struct dmi_system_id *nb05_match_device(void);

#endif
