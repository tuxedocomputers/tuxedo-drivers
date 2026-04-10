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

.PHONY: all install clean package package-deb package-rpm package-alpm

KDIR := /lib/modules/$(shell uname -r)/build

all:
	make -C $(KDIR) M=$(PWD) $(MAKEFLAGS) modules

install: all
	make -C $(KDIR) M=$(PWD) $(MAKEFLAGS) modules_install
	cp -r usr /

clean:
	make -C $(KDIR) M=$(PWD) $(MAKEFLAGS) clean
	rm -rf build/

package: package-deb package-rpm package-alpm

package-deb:
	simple-package-creator --output-dir build-deb --formats DEB DKMS --config deb.yml
	simple-package-tools build deb --dir build-deb --no-build-deps

package-rpm:
	simple-package-creator --output-dir build-rpm --formats RPM DKMS
	simple-package-tools build rpm --dir build-rpm --no-build-deps

package-alpm:
	simple-package-creator --output-dir build-alpm --formats ALPM DKMS
	simple-package-tools build alpm --dir build-alpm --no-build-deps
