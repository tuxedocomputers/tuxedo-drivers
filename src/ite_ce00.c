/*!
 * Copyright (c) 2020 TUXEDO Computers GmbH <tux@tuxedocomputers.com>
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

/*
 * Turn backlight off
 * 08 01 00 00 00 00 00 00
 * 
 * Set brightness only
 * 09 02 [brightness] 00 00 00 00 00
 * 
 * 
 * Most "special modes" uses seven colors that can be defined individually
 * Set color define
 * 14 00 [color define number] [red] [green] [blue] 00 00
 * 
 * [color define number]
 * 	1 -> 7
 * 
 * 
 * Set params
 * 08 02 [anim mode] [speed] [brightness] 08 [behaviour] 00
 * 
 * [anim mode]
 * 	02 breath
 * 	03 wave ([behaviour]: direction)
 * 	04 reactive ([behaviour]: key mode)
 * 	05 rainbow (no color set)
 * 	06 ripple ([behaviour]: key mode)
 * 	09 marquee
 * 	0a raindrop
 * 	0e aurora ([behaviour]: key mode)
 * 	11 spark ([behaviour]: key mode)
 * 
 * 	33 per key control, data in interrupt request
 * 
 * [speed]
 * 	0a -> 01
 * 
 * [brightness]
 * 	00 -> 32
 * 
 * [behaviour]
 * 	00 when not used, otherwise dependent on [anim mode]
 * 
 * [behaviour]: direction
 * 	01 left to right
 * 	02 right to left
 * 	03 bottom to top
 * 	04 top to bottom
 * 
 * [behaviour]: key mode
 * 	00 key press
 * 	01 auto
 */