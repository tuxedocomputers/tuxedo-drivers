#
# Copyright (c) 2018-2020 TUXEDO Computers GmbH <tux@tuxedocomputers.com>
#
# This file is part of tuxedo-drivers.
#
# tuxedo-drivers. is free software: you can redistribute it and/or modify
# it under the terms of the GNU General Public License as published by
# the Free Software Foundation, either version 3 of the License, or
# (at your option) any later version.
#
# This software is distributed in the hope that it will be useful,
# but WITHOUT ANY WARRANTY; without even the implied warranty of
# MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
# GNU General Public License for more details.
#
# You should have received a copy of the GNU General Public License
# along with this software.  If not, see <https://www.gnu.org/licenses/>.
#

.PHONY: all clean install dkmsinstall dkmsremove package package-deb package-rpm

PWD := $(shell pwd)
KDIR := /lib/modules/$(shell uname -r)/build

PACKAGE_NAME := $(shell grep -Pom1 '.*(?= \(.*\) .*; urgency=.*)' debian/changelog)
PACKAGE_VERSION := $(shell grep -Pom1 '.* \(\K.*(?=\) .*; urgency=.*)' debian/changelog)

all:
	make -C $(KDIR) M=$(PWD) $(MAKEFLAGS) modules

clean:
	rm -f src/dkms.conf
	rm -f tuxedo-drivers.spec
	rm -f $(PACKAGE_NAME)-*.tar.gz
	rm -rf debian/.debhelper
	rm -f debian/*.debhelper
	rm -f debian/*.debhelper.log
	rm -f debian/*.substvars
	rm -f debian/debhelper-build-stamp
	rm -f debian/files
	rm -rf debian/tuxedo-drivers
	rm -rf debian/tuxedo-cc-wmi
	rm -rf debian/tuxedo-keyboard
	rm -rf debian/tuxedo-keyboard-dkms
	rm -rf debian/tuxedo-keyboard-ite
	rm -rf debian/tuxedo-touchpad-fix
	rm -rf debian/tuxedo-wmi-dkms
	rm -rf debian/tuxedo-xp-xc-airplane-mode-fix
	rm -rf debian/tuxedo-xp-xc-touchpad-key-fix
	make -C $(KDIR) M=$(PWD) $(MAKEFLAGS) clean

install:
	make -C $(KDIR) M=$(PWD) $(MAKEFLAGS) modules_install

dkmsinstall:
	sed 's/#MODULE_VERSION#/$(PACKAGE_VERSION)/' debian/tuxedo-drivers.dkms > src/dkms.conf
	if ! [ "$(shell dkms status -m tuxedo-drivers -v $(PACKAGE_VERSION))" = "" ]; then dkms remove $(PACKAGE_NAME)/$(PACKAGE_VERSION); fi
	rm -rf /usr/src/$(PACKAGE_NAME)-$(PACKAGE_VERSION)
	rsync --recursive --exclude=*.cmd --exclude=*.d --exclude=*.ko --exclude=*.mod --exclude=*.mod.c --exclude=*.o --exclude=modules.order src/ /usr/src/$(PACKAGE_NAME)-$(PACKAGE_VERSION)
	dkms install $(PACKAGE_NAME)/$(PACKAGE_VERSION)

dkmsremove:
	dkms remove $(PACKAGE_NAME)/$(PACKAGE_VERSION) --all
	rm -rf /usr/src/$(PACKAGE_NAME)-$(PACKAGE_VERSION)

package: package-deb package-rpm

package-deb:
	debuild --no-sign

package-rpm:
	sed 's/#MODULE_VERSION#/$(PACKAGE_VERSION)/' debian/tuxedo-drivers.dkms > src/dkms.conf
	sed 's/#MODULE_VERSION#/$(PACKAGE_VERSION)/' tuxedo-drivers.spec.in > tuxedo-drivers.spec
	mkdir -p $(shell rpm --eval "%{_sourcedir}")
	tar --create --file $(shell rpm --eval "%{_sourcedir}")/$(PACKAGE_NAME)-$(PACKAGE_VERSION).tar.xz\
		--transform="s/src/$(PACKAGE_NAME)-$(PACKAGE_VERSION)\/usr\/src\/$(PACKAGE_NAME)-$(PACKAGE_VERSION)/"\
		--transform="s/tuxedo_keyboard.conf/$(PACKAGE_NAME)-$(PACKAGE_VERSION)\/etc\/modprobe.d\/tuxedo_keyboard.conf/"\
		--transform="s/debian\/copyright/$(PACKAGE_NAME)-$(PACKAGE_VERSION)\/LICENSE/"\
		--transform="s/99-z-tuxedo-systemd-fix.rules/$(PACKAGE_NAME)-$(PACKAGE_VERSION)\/usr\/lib\/udev\/rules.d\/99-z-tuxedo-systemd-fix.rules/"\
		--exclude=*.cmd\
		--exclude=*.d\
		--exclude=*.ko\
		--exclude=*.mod\
		--exclude=*.mod.c\
		--exclude=*.o\
		--exclude=modules.order\
		src tuxedo_keyboard.conf debian/copyright 99-z-tuxedo-systemd-fix.rules
	rpmbuild -ba tuxedo-drivers.spec
