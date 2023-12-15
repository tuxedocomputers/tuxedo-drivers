/*!
 * Copyright (c) 2023 TUXEDO Computers GmbH <tux@tuxedocomputers.com>
 *
 * This file is part of tuxedo-keyboard.
 *
 * tuxedo-keyboard is free software: you can redistribute it and/or modify
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
#include <linux/acpi.h>
#include <linux/module.h>
#include <linux/wmi.h>
#include <linux/keyboard.h>
#include <linux/input.h>
#include <linux/input/sparse-keymap.h>
#include <linux/version.h>
#include <linux/delay.h>
#include <linux/platform_device.h>
#include "tuxedo_nb05_power_profiles.h"
#include "../tuxedo_compatibility_check/tuxedo_compatibility_check.h"

#define dev_to_wdev(__dev)	container_of(__dev, struct wmi_device, dev)

#define NB05_WMI_METHOD_BA_GUID	"99D89064-8D50-42BB-BEA9-155B2E5D0FCD"

struct driver_data_t {
	struct platform_device *pdev;
	u64 last_chosen_profile;
};

static struct wmi_device *__wmi_dev;

/**
 * Method interface: int in, int out
 */
static int __nb05_wmi_aa_method(struct wmi_device *wdev, u32 wmi_method_id,
				u64 *in, u64 *out)
{
	struct acpi_buffer acpi_buffer_in = { (acpi_size) sizeof(*in), in };
	struct acpi_buffer return_buffer = { ACPI_ALLOCATE_BUFFER, NULL };
	union acpi_object *acpi_object_out;
	acpi_status status;

	pr_debug("evaluate: %u\n", wmi_method_id);
	status = wmidev_evaluate_method(wdev, 0, wmi_method_id,
					&acpi_buffer_in, &return_buffer);

	if (ACPI_FAILURE(status)) {
		pr_err("failed to evaluate wmi method %u\n", wmi_method_id);
		return -EIO;
	}

	acpi_object_out = (union acpi_object *) return_buffer.pointer;
	if (!acpi_object_out)
		return -ENODATA;

	if (acpi_object_out->type != ACPI_TYPE_INTEGER) {
		pr_err("No integer for method (%u) call\n", wmi_method_id);
		kfree(return_buffer.pointer);
		return -EIO;
	}

	*out = acpi_object_out->integer.value;
	kfree(return_buffer.pointer);

	return 0;
}

int nb05_wmi_aa_method(u32 wmi_method_id, u64 *in, u64 *out)
{
	if (__wmi_dev)
		return __nb05_wmi_aa_method(__wmi_dev, wmi_method_id, in, out);
	else
		return -ENODEV;
}
EXPORT_SYMBOL(nb05_wmi_aa_method);

static ssize_t platform_profile_choices_show(struct device *dev,
					     struct device_attribute *attr,
					     char *buffer);

static ssize_t platform_profile_show(struct device *dev,
				     struct device_attribute *attr, char *buffer);

static ssize_t platform_profile_store(struct device *dev,
				      struct device_attribute *attr,
				      const char *buffer, size_t size);

struct platform_profile_attrs_t {
	struct device_attribute platform_profile_choices;
	struct device_attribute platform_profile;
};

struct platform_profile_attrs_t platform_profile_attrs = {
	.platform_profile_choices = __ATTR(platform_profile_choices, 0444,
					   platform_profile_choices_show, NULL),
	.platform_profile = __ATTR(platform_profile, 0644,
				   platform_profile_show, platform_profile_store)
};

static struct attribute *platform_profile_attrs_list[] = {
	&platform_profile_attrs.platform_profile_choices.attr,
	&platform_profile_attrs.platform_profile.attr,
	NULL
};

static struct attribute_group platform_profile_attr_group = {
	.attrs = platform_profile_attrs_list
};

struct char_to_value_t {
	char* descriptor;
	u64 value;
};

static struct char_to_value_t platform_profile_options[] = {
	{ .descriptor = "low-power",		.value = 2 },
	{ .descriptor = "balanced",		.value = 0 },
	{ .descriptor = "performance",		.value = 1 }
};

static ssize_t platform_profile_choices_show(struct device *dev,
					     struct device_attribute *attr,
					     char *buffer)
{
	int i, n;
	n = ARRAY_SIZE(platform_profile_options);
	for (i = 0; i < n; ++i) {
		sprintf(buffer + strlen(buffer), "%s",
			platform_profile_options[i].descriptor);
		if (i < n - 1)
			sprintf(buffer + strlen(buffer), " ");
		else
			sprintf(buffer + strlen(buffer), "\n");
	}

	return strlen(buffer);
}

static ssize_t platform_profile_show(struct device *dev,
				     struct device_attribute *attr, char *buffer)
{
	u64 platform_profile_value;
	int i, err;
	struct platform_device *pdev = to_platform_device(dev);
	struct wmi_device *wdev = dev_to_wdev(pdev->dev.parent);

	err = __nb05_wmi_aa_method(wdev, 2, &platform_profile_value, &platform_profile_value);
	if (err) {
		pr_err("Error reading power profile");
		return -EIO;
	}

	for (i = 0; i < ARRAY_SIZE(platform_profile_options); ++i)
		if (platform_profile_options[i].value == platform_profile_value) {
			sprintf(buffer, "%s\n", platform_profile_options[i].descriptor);
			return strlen(buffer);
		}

	pr_err("Read platform profile value not matched to a descriptor\n");

	return -EIO;
}

int rewrite_last_profile(void)
{
	struct driver_data_t *driver_data = dev_get_drvdata(&__wmi_dev->dev);
	u64 out;
	int err = nb05_wmi_aa_method(1, &driver_data->last_chosen_profile, &out);
	if (err)
		return err;
	else if (out)
		return -EINVAL;
	else
		return 0;
}
EXPORT_SYMBOL(rewrite_last_profile);

static ssize_t platform_profile_store(struct device *dev,
				      struct device_attribute *attr,
				      const char *buffer, size_t size)
{
	struct platform_device *pdev = to_platform_device(dev);
	struct wmi_device *wdev = dev_to_wdev(pdev->dev.parent);
	struct driver_data_t *driver_data = dev_get_drvdata(&wdev->dev);
	u64 platform_profile_value, out;
	int i, err;
	char *buffer_copy;
	char *platform_profile_descriptor;

	buffer_copy = kmalloc(size + 1, GFP_KERNEL);
	strcpy(buffer_copy, buffer);
	platform_profile_descriptor = strstrip(buffer_copy);

	for (i = 0; i < ARRAY_SIZE(platform_profile_options); ++i)
		if (strcmp(platform_profile_options[i].descriptor, platform_profile_descriptor) == 0) {
			platform_profile_value = platform_profile_options[i].value;
			break;
		}

	kfree(buffer_copy);

	if (i < ARRAY_SIZE(platform_profile_options)) {
		// Option found try to set
		err = __nb05_wmi_aa_method(wdev, 1, &platform_profile_value, &out);
		if (err)
			return err;
		else if (out)
			return -EINVAL;
		
		driver_data->last_chosen_profile = platform_profile_value;
		return size;
	} else {
		// Invalid input, not matched to an option
		return -EINVAL;
	}
}

#if LINUX_VERSION_CODE < KERNEL_VERSION(5, 3, 0)
static int tuxedo_nb05_power_profiles_probe(struct wmi_device *wdev)
#else
static int tuxedo_nb05_power_profiles_probe(struct wmi_device *wdev, const void *dummy_context)
#endif
{
	int err;
	struct driver_data_t *driver_data;

	pr_debug("driver probe\n");

	__wmi_dev = wdev;

	if (!tuxedo_is_compatible())
		return -ENODEV;

	driver_data = devm_kzalloc(&wdev->dev, sizeof(*driver_data), GFP_KERNEL);
	if (!driver_data)
		return -ENOMEM;

	dev_set_drvdata(&wdev->dev, driver_data);

	// Initialize last chosen profile
	err = __nb05_wmi_aa_method(wdev, 2, &driver_data->last_chosen_profile, &driver_data->last_chosen_profile);
	if (err) {
		pr_err("Error reading power profile");
		return -EIO;
	}

	const struct platform_device_info pinfo = {
		.name = "tuxedo_platform_profile",
		.id = PLATFORM_DEVID_NONE,
		.parent = &wdev->dev
	};

	driver_data->pdev = platform_device_register_full(&pinfo);
	if (PTR_ERR_OR_ZERO(driver_data->pdev)) {
		pr_err("platform device creation failed\n");
		return -ENOMEM;
	}

	err = sysfs_create_group(&driver_data->pdev->dev.kobj, &platform_profile_attr_group);
	if (err) {
		pr_err("create group failed\n");
		platform_device_unregister(driver_data->pdev);
		return err;
	}

	return 0;
}

#if LINUX_VERSION_CODE < KERNEL_VERSION(5, 13, 0)
static int tuxedo_nb05_power_profiles_remove(struct wmi_device *wdev)
#else
static void tuxedo_nb05_power_profiles_remove(struct wmi_device *wdev)
#endif
{
	pr_debug("driver remove\n");
	struct driver_data_t *driver_data = dev_get_drvdata(&wdev->dev);
	sysfs_remove_group(&driver_data->pdev->dev.kobj, &platform_profile_attr_group);
	platform_device_unregister(driver_data->pdev);

#if LINUX_VERSION_CODE < KERNEL_VERSION(5, 13, 0)
	return 0;
#endif
}

static const struct wmi_device_id tuxedo_nb05_power_profiles_device_ids[] = {
	{ .guid_string = NB05_WMI_METHOD_BA_GUID },
	{ }
};

static struct wmi_driver tuxedo_nb05_power_profiles_driver = {
	.driver = {
		.name = "tuxedo_nb05_power_profiles",
		.owner = THIS_MODULE
	},
	.id_table = tuxedo_nb05_power_profiles_device_ids,
	.probe = tuxedo_nb05_power_profiles_probe,
	.remove = tuxedo_nb05_power_profiles_remove,
};

module_wmi_driver(tuxedo_nb05_power_profiles_driver);

MODULE_AUTHOR("TUXEDO Computers GmbH <tux@tuxedocomputers.com>");
MODULE_DESCRIPTION("Driver for NB05 power profiles");
MODULE_VERSION("0.0.1");
MODULE_LICENSE("GPL");

MODULE_DEVICE_TABLE(wmi, tuxedo_nb05_power_profiles_device_ids);
MODULE_ALIAS("wmi:" NB05_WMI_METHOD_BA_GUID);