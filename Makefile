#
# Copyright (c) 2023 TUXEDO Computers GmbH <tux@tuxedocomputers.com>
#
# This file is part of tuxedo-yt6801.
#
# tuxedo-yt6801 is free software; you can redistribute it and/or
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

.PHONY: package package-deb package-rpm

PACKAGE_NAME := $(shell grep -Pom1 '.*(?= \(.*\) .*; urgency=.*)' debian/changelog)
PACKAGE_VERSION := $(shell grep -Pom1 '.* \(\K.*(?=\) .*; urgency=.*)' debian/changelog)

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
		--transform="s/src/$(PACKAGE_NAME)-$(PACKAGE_VERSION)\/usr\/src\/$(PACKAGE_NAME)-$(PACKAGE_VERSION)/" \
		--transform="s/debian\/copyright/$(PACKAGE_NAME)-$(PACKAGE_VERSION)\/LICENSE/" \
		--transform="s/61-keyboard-tuxedo.hwdb/$(PACKAGE_NAME)-$(PACKAGE_VERSION)\/usr\/lib\/udev\/hwdb.d\/61-keyboard-tuxedo.hwdb/" \
		--transform="s/61-sensor-tuxedo.hwdb/$(PACKAGE_NAME)-$(PACKAGE_VERSION)\/usr\/lib\/udev\/hwdb.d\/61-sensor-tuxedo.hwdb/" \
		--transform="s/99-tuxedo-fix-infinity-flex-touchpanel-toggle.rules/$(PACKAGE_NAME)-$(PACKAGE_VERSION)\/usr\/lib\/udev\/rules.d\/99-tuxedo-fix-infinity-flex-touchpanel-toggle.rules/" \
		--transform="s/99-tuxedo-fix-nb02-touchpad-mouse.rules/$(PACKAGE_NAME)-$(PACKAGE_VERSION)\/usr\/lib\/udev\/rules.d\/99-tuxedo-fix-nb02-touchpad-mouse.rules/" \
		--transform="s/99-tuxedo-fix-systemd-led-bootdelay.rules/$(PACKAGE_NAME)-$(PACKAGE_VERSION)\/usr\/lib\/udev\/rules.d\/99-tuxedo-fix-systemd-led-bootdelay.rules/" \
		--transform="s/tuxedo_keyboard.conf/$(PACKAGE_NAME)-$(PACKAGE_VERSION)\/etc\/modprobe.d\/tuxedo_keyboard.conf/" \
		--exclude=*.cmd \
		--exclude=*.d \
		--exclude=*.ko \
		--exclude=*.mod \
		--exclude=*.mod.c \
		--exclude=*.o \
		--exclude=modules.order \
		src debian/copyright \
		61-keyboard-tuxedo.hwdb 61-sensor-tuxedo.hwdb \
		99-tuxedo-fix-infinity-flex-touchpanel-toggle.rules 99-tuxedo-fix-nb02-touchpad-mouse.rules 99-tuxedo-fix-systemd-led-bootdelay.rules \
		tuxedo_keyboard.conf
	rpmbuild -ba tuxedo-drivers.spec
