/*!
 * Copyright (c) 2023 LWL Computers GmbH <tux@lwlcomputers.com>
 *
 * This file is part of lwl-drivers.
 *
 * lwl-drivers is free software: you can redistribute it and/or modify
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

#ifndef LWL_COMPATIBILITY_CHECK_H
#define LWL_COMPATIBILITY_CHECK_H

#include <linux/kernel.h>

bool lwl_is_compatible(void);

#endif // LWL_COMPATIBILITY_CHECK_H
