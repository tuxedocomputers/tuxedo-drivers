#
# Copyright (c) 2018-2025 TUXEDO Computers GmbH <tux@tuxedocomputers.com>
#
# This file is part of tuxedo-drivers.
#
# tuxedo-drivers is free software; you can redistribute it and/or
# modify it under the terms of the GNU General Public License
# as published by the Free Software Foundation; version 2
# of the License.
#
# This program is distributed in the hope that it will be useful,
# but WITHOUT ANY WARRANTY; without even the implied warranty of
# MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
# GNU General Public License for more details.
#
# You should have received a copy of the GNU General Public License
# along with this program; if not, write to the Free Software
# Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA  02110-1301, USA.
#

.PHONY: all install clean package package-deb package-rpm

KDIR := /lib/modules/$(shell uname -r)/build

all:
	make -C $(KDIR) M=$(PWD) $(MAKEFLAGS) modules

install: all
	make -C $(KDIR) M=$(PWD) $(MAKEFLAGS) modules_install
	cp -r usr /

clean:
	make -C $(KDIR) M=$(PWD) $(MAKEFLAGS) clean
	rm -rf build/

package:
	simple-package-creator
	simple-package-tools build deb -d build -n
	simple-package-tools build rpm -d build -n

package-deb:
	simple-package-creator
	simple-package-tools build deb -d build -n

package-rpm:
	simple-package-creator
	simple-package-tools build rpm -d build -n
