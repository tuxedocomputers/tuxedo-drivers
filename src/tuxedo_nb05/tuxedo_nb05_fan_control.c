// SPDX-License-Identifier: GPL-3.0+
/*!
 * Copyright (c) 2024 TUXEDO Computers GmbH <tux@tuxedocomputers.com>
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
#define pr_fmt(fmt) KBUILD_MODNAME ": " fmt
#include <linux/kernel.h>
#include <linux/module.h>
#include <linux/platform_device.h>
#include <linux/slab.h>
#include <linux/dmi.h>
#include <linux/version.h>
#include "tuxedo_nb05_ec.h"

#define FAN_SET_RPM_MAX 54
#define FAN_SET_DUTY_MAX 0xb8
#define FAN_SET_RPM_HIGHTEMP 15
#define FAN_SET_DUTY_HIGHTEMP ((FAN_SET_RPM_HIGHTEMP * FAN_SET_DUTY_MAX) / FAN_SET_RPM_MAX)

#define PWM_TO_RPM(pwm_data) ((pwm_data * FAN_SET_RPM_MAX) / 0xff)
#define PWM_TO_DUTY(pwm_data) ((pwm_data * FAN_SET_DUTY_MAX) / 0xff)
#define RPM_TO_PWM(rpm_data) ((rpm_data * 0xff) / FAN_SET_RPM_MAX)
#define DUTY_TO_PWM(duty_data) ((duty_data * 0xff) / FAN_SET_DUTY_MAX)

struct driver_data_t {
	struct platform_device *pdev;
	struct nb05_ec_data_t *ec_data;
	bool write_rpm;
};

static ssize_t fan1_pwm_show(struct device *dev,
			     struct device_attribute *attr, char *buffer);

static ssize_t fan1_pwm_store(struct device *dev,
			      struct device_attribute *attr,
			      const char *buffer, size_t size);

static ssize_t fan1_pwm_enable_show(struct device *dev,
				    struct device_attribute *attr, char *buffer);

static ssize_t fan1_pwm_enable_store(struct device *dev,
				     struct device_attribute *attr,
				     const char *buffer, size_t size);

static ssize_t fan2_pwm_show(struct device *dev,
			     struct device_attribute *attr, char *buffer);

static ssize_t fan2_pwm_store(struct device *dev,
			      struct device_attribute *attr,
			      const char *buffer, size_t size);

static ssize_t fan2_pwm_enable_show(struct device *dev,
				    struct device_attribute *attr, char *buffer);

static ssize_t fan2_pwm_enable_store(struct device *dev,
				     struct device_attribute *attr,
				     const char *buffer, size_t size);

static int write_fan1_rpm(u8 rpm_data);
static u8 read_fan1_duty_ranges(void);
static u8 read_fan1_duty_onereg(void);
static int write_fan1_duty_ranges(u8 rpm_data);
static int write_fan1_duty_onereg(u8 rpm_data);
static bool read_fan1_enable_ranges(void);
static bool read_fan1_enable_onereg(void);
static int write_fan1_enable_ranges(bool enable_data);
static int write_fan1_enable_onereg(bool enable_data);
static int write_fan2_rpm(u8 rpm_data);
static u8 read_fan2_duty(void);
static int write_fan2_duty(u8 rpm_data);
static bool read_fan2_enable(void);
static int write_fan2_enable(bool enable_data);

static int write_fan1_rpm(u8 rpm_data)
{
	int reg;

	if (rpm_data > FAN_SET_RPM_MAX)
		return -EINVAL;

	for (reg = 0x02d0; reg <= 0x02d6; ++reg)
		nb05_write_ec_ram(reg, rpm_data);

	if (rpm_data < FAN_SET_RPM_HIGHTEMP)
		rpm_data = FAN_SET_RPM_HIGHTEMP;

	nb05_write_ec_ram(0x02d7, rpm_data);
	nb05_write_ec_ram(0x02d8, rpm_data);

	return 0;
}

static u8 read_fan1_duty_ranges(void)
{
	u8 duty_data;
	nb05_read_ec_ram(0x2c1, &duty_data);
	return duty_data;
}

static u8 read_fan1_duty_onereg(void)
{
	u8 duty_data;
	nb05_read_ec_ram(0x1809, &duty_data);
	return duty_data;
}

static int write_fan1_duty_ranges(u8 duty_data)
{
	int reg;

	if (duty_data > FAN_SET_DUTY_MAX)
		return -EINVAL;

	for (reg = 0x02c1; reg <= 0x02c7; ++reg)
		nb05_write_ec_ram(reg, duty_data);

	if (duty_data < FAN_SET_DUTY_HIGHTEMP)
		duty_data = FAN_SET_DUTY_HIGHTEMP;

	nb05_write_ec_ram(0x02c8, duty_data);
	nb05_write_ec_ram(0x02c9, duty_data);

	return 0;
}

static int write_fan1_duty_onereg(u8 duty_data)
{
	if (duty_data > FAN_SET_DUTY_MAX)
		return -EINVAL;

	nb05_write_ec_ram(0x1809, duty_data);

	return 0;
}

static bool read_fan1_enable_ranges(void)
{
	u8 enable_data;
	nb05_read_ec_ram(0x2c0, &enable_data);
	return (enable_data & 0x01);
}

static bool read_fan1_enable_onereg(void)
{
	u8 enable_data;
	nb05_read_ec_ram(0x2f1, &enable_data);
	return (enable_data == 0xaa);
}

static int write_fan1_enable_ranges(bool enable)
{
	u8 enable_data = enable ? 1 : 0;
	nb05_write_ec_ram(0x2c0, enable_data);
	return 0;
}

static int write_fan1_enable_onereg(bool enable)
{
	u8 enable_data = enable ? 0xaa : 0x00;
	nb05_write_ec_ram(0x2f1, enable_data);
	return 0;
}

static int write_fan2_rpm(u8 rpm_data)
{
	int reg;

	if (rpm_data > FAN_SET_RPM_MAX)
		return -EINVAL;

	for (reg = 0x0250; reg <= 0x0256; ++reg)
		nb05_write_ec_ram(reg, rpm_data);

	if (rpm_data < FAN_SET_RPM_HIGHTEMP)
		rpm_data = FAN_SET_RPM_HIGHTEMP;

	nb05_write_ec_ram(0x0257, rpm_data);
	nb05_write_ec_ram(0x0258, rpm_data);

	return 0;
}

static u8 read_fan2_duty(void)
{
	u8 rpm_data;
	nb05_read_ec_ram(0x0241, &rpm_data);
	return rpm_data;
}

static int write_fan2_duty(u8 duty_data)
{
	int reg;

	if (duty_data > FAN_SET_DUTY_MAX)
		return -EINVAL;

	for (reg = 0x0241; reg <= 0x0247; ++reg)
		nb05_write_ec_ram(reg, duty_data);

	if (duty_data < FAN_SET_DUTY_HIGHTEMP)
		duty_data = FAN_SET_DUTY_HIGHTEMP;

	nb05_write_ec_ram(0x0248, duty_data);
	nb05_write_ec_ram(0x0249, duty_data);

	return 0;
}

static bool read_fan2_enable(void)
{
	u8 enable_data;
	nb05_read_ec_ram(0x240, &enable_data);
	return (enable_data & 0x01);
}

static int write_fan2_enable(bool enable)
{
	u8 enable_data = enable ? 1 : 0;
	nb05_write_ec_ram(0x240, enable_data);
	return 0;
}

struct fan_control_attrs_t {
	struct device_attribute fan1_pwm;
	struct device_attribute fan1_pwm_enable;
	struct device_attribute fan2_pwm;
	struct device_attribute fan2_pwm_enable;
};

struct fan_control_attrs_t fan_control_attrs = {
	.fan1_pwm = __ATTR(fan1_pwm, 0644, fan1_pwm_show, fan1_pwm_store),
	.fan1_pwm_enable = __ATTR(fan1_pwm_enable, 0644, fan1_pwm_enable_show, fan1_pwm_enable_store),
	.fan2_pwm = __ATTR(fan2_pwm, 0644, fan2_pwm_show, fan2_pwm_store),
	.fan2_pwm_enable = __ATTR(fan2_pwm_enable, 0644, fan2_pwm_enable_show, fan2_pwm_enable_store),
};

static struct attribute *fan_control_attrs_list[] = {
	&fan_control_attrs.fan1_pwm.attr,
	&fan_control_attrs.fan1_pwm_enable.attr,
	&fan_control_attrs.fan2_pwm.attr,
	&fan_control_attrs.fan2_pwm_enable.attr,
	NULL
};

static umode_t fan_control_attr_is_visible(struct kobject *kobj,
					   struct attribute *a, int n)
{
	struct device *dev = kobj_to_dev(kobj);
	struct driver_data_t *driver_data = dev_get_drvdata(dev);

	// Two attributes per fan, show interface for device specific number of fans
	if (n < (driver_data->ec_data->dev_data->number_fans * 2))
		return 0644;
	else
		return 0;
}

static struct attribute_group fan_control_attr_group = {
	.attrs = fan_control_attrs_list,
	.is_visible = fan_control_attr_is_visible,
};

static ssize_t fan1_pwm_show(struct device *dev,
			     struct device_attribute *attr, char *buffer)
{
	struct driver_data_t *driver_data = dev_get_drvdata(dev);
	u8 pwm_data, duty_data;

	if (driver_data->ec_data->dev_data->fanctl_onereg)
		duty_data = read_fan1_duty_onereg();
	else
		duty_data = read_fan1_duty_ranges();

	pwm_data = DUTY_TO_PWM(duty_data);
	sysfs_emit(buffer, "%d\n", pwm_data);

	return strlen(buffer);
}

static ssize_t fan1_pwm_store(struct device *dev,
			      struct device_attribute *attr,
			      const char *buffer, size_t size)
{
	u8 pwm_data, rpm_data, duty_data;
	int err;
	struct driver_data_t *driver_data = dev_get_drvdata(dev);

	if (kstrtou8(buffer, 0, &pwm_data))
		return -EINVAL;

	duty_data = PWM_TO_DUTY(pwm_data);
	rpm_data = PWM_TO_RPM(pwm_data);

	if (driver_data->ec_data->dev_data->fanctl_onereg)
		err = write_fan1_duty_onereg(duty_data);
	else
		err = write_fan1_duty_ranges(duty_data);

	if (err)
		return err;

	if (driver_data->write_rpm) {
		err = write_fan1_rpm(rpm_data);
		if (err)
			return err;
	}

	return size;
}

static ssize_t fan1_pwm_enable_show(struct device *dev,
				    struct device_attribute *attr, char *buffer)
{
	struct driver_data_t *driver_data = dev_get_drvdata(dev);
	bool enable_data;
	u8 enable_hwmon;

	if (driver_data->ec_data->dev_data->fanctl_onereg)
		enable_data = read_fan1_enable_onereg();
	else
		enable_data = read_fan1_enable_ranges();

	if (enable_data)
		enable_hwmon = 1;
	else
		enable_hwmon = 2;

	sysfs_emit(buffer, "%d\n", enable_hwmon);

	return strlen(buffer);
}

static ssize_t fan1_pwm_enable_store(struct device *dev,
				     struct device_attribute *attr,
				     const char *buffer, size_t size)
{
	struct driver_data_t *driver_data = dev_get_drvdata(dev);
	bool enable_data;
	u8 enable_hwmon;
	int err;

	if (kstrtou8(buffer, 0, &enable_hwmon))
		return -EINVAL;

	if (enable_hwmon == 2)
		enable_data = false;
	else
		enable_data = true;

	if (driver_data->ec_data->dev_data->fanctl_onereg)
		err = write_fan1_enable_onereg(enable_data);
	else
		err = write_fan1_enable_ranges(enable_data);

	if (err)
		return err;

	return size;
}

static ssize_t fan2_pwm_show(struct device *dev,
			     struct device_attribute *attr, char *buffer)
{
	u8 pwm_data, duty_data;
	duty_data = read_fan2_duty();
	pwm_data = DUTY_TO_PWM(duty_data);
	sysfs_emit(buffer, "%d\n", pwm_data);

	return strlen(buffer);
}

static ssize_t fan2_pwm_store(struct device *dev,
			      struct device_attribute *attr,
			      const char *buffer, size_t size)
{	
	u8 pwm_data, rpm_data, duty_data;
	int err;
	struct driver_data_t *driver_data = dev_get_drvdata(dev);

	if (kstrtou8(buffer, 0, &pwm_data))
		return -EINVAL;

	duty_data = PWM_TO_DUTY(pwm_data);
	rpm_data = PWM_TO_RPM(pwm_data);

	err = write_fan2_duty(duty_data);
	if (err)
		return err;

	if (driver_data->write_rpm) {
		err = write_fan2_rpm(rpm_data);
		if (err)
			return err;
	}

	return size;
}

static ssize_t fan2_pwm_enable_show(struct device *dev,
				    struct device_attribute *attr, char *buffer)
{
	bool enable_data;
	u8 enable_hwmon;
	enable_data = read_fan2_enable();
	if (enable_data)
		enable_hwmon = 1;
	else
		enable_hwmon = 2;

	sysfs_emit(buffer, "%d\n", enable_hwmon);

	return strlen(buffer);
}

static ssize_t fan2_pwm_enable_store(struct device *dev,
				     struct device_attribute *attr,
				     const char *buffer, size_t size)
{
	bool enable_data;
	u8 enable_hwmon;
	int err;

	if (kstrtou8(buffer, 0, &enable_hwmon))
		return -EINVAL;

	if (enable_hwmon == 2)
		enable_data = false;
	else
		enable_data = true;

	err = write_fan2_enable(enable_data);
	if (err)
		return err;

	return size;
}

static int __init tuxedo_nb05_fan_control_probe(struct platform_device *pdev)
{
	int err;

	struct driver_data_t *driver_data = devm_kzalloc(&pdev->dev, sizeof(*driver_data), GFP_KERNEL);
	if (!driver_data)
		return -ENOMEM;

	pr_debug("driver probe\n");

	dev_set_drvdata(&pdev->dev, driver_data);

	driver_data->pdev = pdev;
	nb05_get_ec_data(&driver_data->ec_data);

	driver_data->write_rpm = false;
	if (!driver_data->ec_data->dev_data->fanctl_onereg) {
		if (driver_data->ec_data->ver_major >= 9 &&
		    driver_data->ec_data->ver_minor >= 10)
			driver_data->write_rpm = false;
		else
			driver_data->write_rpm = true;
	}

	err = sysfs_create_group(&driver_data->pdev->dev.kobj, &fan_control_attr_group);
	if (err) {
		pr_err("create group failed\n");
		platform_device_unregister(driver_data->pdev);
		return err;
	}

	return 0;
}

#if LINUX_VERSION_CODE < KERNEL_VERSION(6, 11, 0)
static int tuxedo_nb05_fan_control_remove(struct platform_device *pdev)
#else
static void tuxedo_nb05_fan_control_remove(struct platform_device *pdev)
#endif
{
	pr_debug("driver remove\n");
	struct driver_data_t *driver_data = dev_get_drvdata(&pdev->dev);
	sysfs_remove_group(&driver_data->pdev->dev.kobj, &fan_control_attr_group);
#if LINUX_VERSION_CODE < KERNEL_VERSION(6, 11, 0)
	return 0;
#endif
}

static struct platform_device *tuxedo_nb05_fan_control_device;
static struct platform_driver tuxedo_nb05_fan_control_driver = {
	.driver.name = "tuxedo_fan_control",
	.remove = tuxedo_nb05_fan_control_remove,
};

static int __init tuxedo_nb05_fan_control_init(void)
{
	tuxedo_nb05_fan_control_device =
		platform_create_bundle(&tuxedo_nb05_fan_control_driver,
				       tuxedo_nb05_fan_control_probe, NULL, 0, NULL, 0);

	if (IS_ERR(tuxedo_nb05_fan_control_device))
		return PTR_ERR(tuxedo_nb05_fan_control_device);

	return 0;
}

static void __exit tuxedo_nb05_fan_control_exit(void)
{
	platform_device_unregister(tuxedo_nb05_fan_control_device);
	platform_driver_unregister(&tuxedo_nb05_fan_control_driver);
}

module_init(tuxedo_nb05_fan_control_init);
module_exit(tuxedo_nb05_fan_control_exit);

MODULE_AUTHOR("TUXEDO Computers GmbH <tux@tuxedocomputers.com>");
MODULE_DESCRIPTION("TUXEDO Computers NB05 fan control");
MODULE_LICENSE("GPL v3");
