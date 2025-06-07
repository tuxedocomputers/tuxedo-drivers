// SPDX-License-Identifier: GPL-2.0

#include <linux/device.h>
#include <linux/kernel.h>
#include <linux/moduleparam.h>
#include <linux/power_supply.h>
#include <acpi/battery.h>
#include <linux/sysfs.h>
#include <linux/types.h>
#include <linux/version.h>

#include "ec.c"

/* ========================================================================== */
static bool battery_hook_registered;
/* ========================================================================== */

static ssize_t charge_control_end_threshold_show(struct device *dev,
						 struct device_attribute *attr, char *buf)
{
	int status = ec_read_byte(BATT_CHARGE_CTRL_ADDR);

	if (status < 0)
		return status;

	status &= BATT_CHARGE_CTRL_VALUE_MASK;

	if (status == 0)
		status = 100;

	return sprintf(buf, "%d\n", status);
}

static ssize_t charge_control_end_threshold_store(struct device *dev, struct device_attribute *attr,
						  const char *buf, size_t count)
{
	int status, value;

	if (kstrtoint(buf, 10, &value) || !(1 <= value && value <= 100))
		return -EINVAL;

	status = ec_read_byte(BATT_CHARGE_CTRL_ADDR);
	if (status < 0)
		return status;

	if (value == 100)
		value = 0;

	status = (status & ~BATT_CHARGE_CTRL_VALUE_MASK) | value;

	status = ec_write_byte(BATT_CHARGE_CTRL_ADDR, status);

	if (status < 0)
		return status;

	return count;
}

static DEVICE_ATTR_RW(charge_control_end_threshold);
static struct attribute *tuxedo_laptop_batt_attrs[] = {
	&dev_attr_charge_control_end_threshold.attr,
	NULL
};
ATTRIBUTE_GROUPS(tuxedo_laptop_batt);

#if LINUX_VERSION_CODE >= KERNEL_VERSION(6, 2, 0)
static int tuxedo_laptop_batt_add(struct power_supply *battery, struct acpi_battery_hook *hook)
#else
static int tuxedo_laptop_batt_add(struct power_supply *battery)
#endif
{
	if (strcmp(battery->desc->name, "BAT0") != 0)
		return 0;

	return device_add_groups(&battery->dev, tuxedo_laptop_batt_groups);
}

#if LINUX_VERSION_CODE >= KERNEL_VERSION(6, 2, 0)
static int tuxedo_laptop_batt_remove(struct power_supply *battery, struct acpi_battery_hook *hook)
#else
static int tuxedo_laptop_batt_remove(struct power_supply *battery)
#endif
{
	if (strcmp(battery->desc->name, "BAT0") != 0)
		return 0;

	device_remove_groups(&battery->dev, tuxedo_laptop_batt_groups);
	return 0;
}

static struct acpi_battery_hook tuxedo_laptop_batt_hook = {
	.add_battery    = tuxedo_laptop_batt_add,
	.remove_battery = tuxedo_laptop_batt_remove,
	.name           = "tuxedo laptop battery extension",
};

int __init tuxedo_battery_setup(void)
{
	battery_hook_register(&tuxedo_laptop_batt_hook);
	battery_hook_registered = true;

	return 0;
}

void tuxedo_battery_cleanup(void)
{
	if (battery_hook_registered)
		battery_hook_unregister(&tuxedo_laptop_batt_hook);
}


module_init(tuxedo_battery_setup);
module_exit(tuxedo_battery_cleanup);

MODULE_LICENSE("GPL");

MODULE_AUTHOR("TUXEDO Computers GmbH <tux@tuxedocomputers.com>");
MODULE_DESCRIPTION("TUXEDO Computers battery driver");
