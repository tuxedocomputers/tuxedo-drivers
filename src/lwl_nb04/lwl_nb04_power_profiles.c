/*!
 * Copyright (c) 2023 LWL Computers GmbH <tux@tuxedocomputers.com>
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
#define pr_fmt(fmt) KBUILD_MODNAME ": " fmt
#include <linux/kernel.h>
#include <linux/module.h>
#include <linux/platform_device.h>
#include <linux/slab.h>
#include <linux/dmi.h>
#include <linux/version.h>
#include "lwl_nb04_wmi_bs.h"

#define DEFAULT_PROFILE		WMI_SYSTEM_MODE_BEAST

struct driver_data_t {
	struct platform_device *pdev;
	u8 current_profile_value;
};

static int set_system_mode(u8 mode_input)
{
	int err, wmi_return;
	u8 in[BS_INPUT_BUFFER_LENGTH] = { 0 };
	u8 out[BS_OUTPUT_BUFFER_LENGTH] = { 0 };

	if (mode_input >= WMI_SYSTEM_MODE_END)
		return -EINVAL;

	in[0] = mode_input;

	err = nb04_wmi_bs_method(0x07, in, out);
	if (err)
		return err;

	wmi_return = (out[1] << 8) | out[0];
	if (wmi_return != WMI_RETURN_STATUS_SUCCESS)
		return -EIO;

	return 0;
}

static int write_platform_profile_state(struct platform_device *pdev)
{
	struct driver_data_t *driver_data = dev_get_drvdata(&pdev->dev);
	return set_system_mode(driver_data->current_profile_value);
}

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
	{ .descriptor = "low-power",	.value = WMI_SYSTEM_MODE_BATTERY },
	{ .descriptor = "balanced",	.value = WMI_SYSTEM_MODE_HUMAN },
	{ .descriptor = "performance",	.value = WMI_SYSTEM_MODE_BEAST },
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
	struct driver_data_t *driver_data = dev_get_drvdata(&pdev->dev);

	err = 0;
	platform_profile_value = driver_data->current_profile_value;

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

static ssize_t platform_profile_store(struct device *dev,
				      struct device_attribute *attr,
				      const char *buffer, size_t size)
{
	struct platform_device *pdev = to_platform_device(dev);
	struct driver_data_t *driver_data = dev_get_drvdata(&pdev->dev);
	u64 platform_profile_value;
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
		driver_data->current_profile_value = platform_profile_value;
		err = write_platform_profile_state(pdev);
		if (err)
			return err;
		return size;
	} else {
		// Invalid input, not matched to an option
		return -EINVAL;
	}
}

static int __init lwl_nb04_power_profiles_probe(struct platform_device *pdev)
{
	int err;
	struct driver_data_t *driver_data = devm_kzalloc(&pdev->dev, sizeof(*driver_data), GFP_KERNEL);
	if (!driver_data)
		return -ENOMEM;

	pr_debug("driver probe\n");

	// Sirius uses other platform control interface
	if (dmi_match(DMI_SYS_VENDOR, "LWL") &&
	    dmi_match(DMI_PRODUCT_SKU, "SIRIUS1601"))
		return -ENODEV;

	dev_set_drvdata(&pdev->dev, driver_data);

	driver_data->pdev = pdev;
	driver_data->current_profile_value = DEFAULT_PROFILE;
	write_platform_profile_state(pdev);

	err = sysfs_create_group(&driver_data->pdev->dev.kobj, &platform_profile_attr_group);
	if (err) {
		pr_err("create group failed\n");
		platform_device_unregister(driver_data->pdev);
		return err;
	}

	return 0;
}

#if LINUX_VERSION_CODE < KERNEL_VERSION(6, 11, 0)
static int lwl_nb04_power_profiles_remove(struct platform_device *pdev)
#else
static void lwl_nb04_power_profiles_remove(struct platform_device *pdev)
#endif
{
	pr_debug("driver remove\n");
	struct driver_data_t *driver_data = dev_get_drvdata(&pdev->dev);
	sysfs_remove_group(&driver_data->pdev->dev.kobj, &platform_profile_attr_group);
#if LINUX_VERSION_CODE < KERNEL_VERSION(6, 11, 0)
	return 0;
#endif
}

static struct platform_device *lwl_nb04_power_profiles_device;
static struct platform_driver lwl_nb04_power_profiles_driver = {
	.driver.name = "lwl_platform_profile",
	.remove = lwl_nb04_power_profiles_remove,
};

static int __init lwl_nb04_power_profiles_init(void)
{
	if (!nb04_wmi_bs_available())
		return -ENODEV;

	lwl_nb04_power_profiles_device =
		platform_create_bundle(&lwl_nb04_power_profiles_driver,
				       lwl_nb04_power_profiles_probe, NULL, 0, NULL, 0);

	if (IS_ERR(lwl_nb04_power_profiles_device))
		return PTR_ERR(lwl_nb04_power_profiles_device);

	return 0;
}

static void __exit lwl_nb04_power_profiles_exit(void)
{
	platform_device_unregister(lwl_nb04_power_profiles_device);
	platform_driver_unregister(&lwl_nb04_power_profiles_driver);
}

module_init(lwl_nb04_power_profiles_init);
module_exit(lwl_nb04_power_profiles_exit);

MODULE_AUTHOR("LWL Computers GmbH <tux@tuxedocomputers.com>");
MODULE_DESCRIPTION("LWL Computers NB04 platform profile driver");
MODULE_LICENSE("GPL");
