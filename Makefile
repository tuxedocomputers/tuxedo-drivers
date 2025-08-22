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

PACKAGE_NAME := $(shell grep -Pom1 '.*(?= \(.*\) .*; urgency=.*)' debian/changelog)
PACKAGE_VERSION := $(shell grep -Pom1 '.* \(\K.*(?=\) .*; urgency=.*)' debian/changelog)

all:
	make -C $(KDIR) M=$(PWD) $(MAKEFLAGS) modules

install: all
	make -C $(KDIR) M=$(PWD) $(MAKEFLAGS) modules_install
	cp -r etc usr /

clean:
	make -C $(KDIR) M=$(PWD) $(MAKEFLAGS) clean
	rm -f debian/*.debhelper
	rm -f debian/*.debhelper.log
	rm -f debian/*.substvars
	rm -f debian/debhelper-build-stamp
	rm -f debian/files
	rm -rf debian/tuxedo-drivers
	rm -f src/dkms.conf
	rm -f tuxedo-drivers.spec

package: package-deb package-rpm

package-deb:
	debuild --no-tgz-check --no-sign

package-rpm:
	sed 's/#MODULE_VERSION#/$(PACKAGE_VERSION)/' debian/tuxedo-drivers.dkms > src/dkms.conf
	sed 's/#MODULE_VERSION#/$(PACKAGE_VERSION)/' tuxedo-drivers.spec.in > tuxedo-drivers.spec
	echo >> tuxedo-drivers.spec
	./debian-changelog-to-rpm-changelog.awk debian/changelog >> tuxedo-drivers.spec
	mkdir -p $(shell rpm --eval "%{_sourcedir}")
	tar --create --file $(shell rpm --eval "%{_sourcedir}")/$(PACKAGE_NAME)-$(PACKAGE_VERSION).tar.xz \
		--transform="s/debian\/copyright/$(PACKAGE_NAME)-$(PACKAGE_VERSION)\/LICENSE/" \
		--transform="s/etc/$(PACKAGE_NAME)-$(PACKAGE_VERSION)\/etc/" \
		--transform="s/usr/$(PACKAGE_NAME)-$(PACKAGE_VERSION)\/usr/" \
		--transform="s/src/$(PACKAGE_NAME)-$(PACKAGE_VERSION)\/usr\/src\/$(PACKAGE_NAME)-$(PACKAGE_VERSION)/" \
		--exclude=*.cmd \
		--exclude=*.ko \
		--exclude=*.mod \
		--exclude=*.mod.c \
		--exclude=*.o \
		--exclude=*.o.d \
		--exclude=modules.order \
		debian/copyright etc src usr
	rpmbuild -ba tuxedo-drivers.spec
