#include <linux/platform_device.h>

#include "../uniwill_interfaces.h"

#define __unused __attribute__((unused))

static ssize_t ctgp_enable_show(struct device *__unused, struct device_attribute *__unused,
				char *buf)
{
	int result = 0;
	u8 data = 0;

	result = uniwill_read_ec_ram(UW_EC_REG_CTGP_DB_ENABLE, &data);
	if (result < 0)
		return result;

	return sysfs_emit(buf, "%u\n", data & UW_EC_REG_CTGP_DB_ENABLE_BIT_CTGP_ENABLE? 1 : 0);
}
static ssize_t ctgp_enable_store(struct device *__unused, struct device_attribute *__unused,
				 const char *buf, size_t count)
{
	int result = 0;
	u8 data = 0;
	bool enable = false;

	result = kstrtobool(buf, &enable);
	if (result < 0)
		return result;

	result = uniwill_read_ec_ram(UW_EC_REG_CTGP_DB_ENABLE, &data);
	if (result < 0)
		return result;

	if (enable) {
		result = uniwill_write_ec_ram(UW_EC_REG_CTGP_DB_ENABLE, data |
					      UW_EC_REG_CTGP_DB_ENABLE_BIT_GENERAL_ENABLE |
					      UW_EC_REG_CTGP_DB_ENABLE_BIT_CTGP_ENABLE);
		if (result < 0)
			return result;
	}
	else {
		if (data & UW_EC_REG_CTGP_DB_ENABLE_BIT_DB_ENABLE) {
			result = uniwill_write_ec_ram(UW_EC_REG_CTGP_DB_ENABLE, data &
						      ~UW_EC_REG_CTGP_DB_ENABLE_BIT_CTGP_ENABLE);
			if (result < 0)
				return result;
		}
		else {
			result = uniwill_write_ec_ram(UW_EC_REG_CTGP_DB_ENABLE, data &
						      ~(UW_EC_REG_CTGP_DB_ENABLE_BIT_CTGP_ENABLE |
						      UW_EC_REG_CTGP_DB_ENABLE_BIT_GENERAL_ENABLE));
			if (result < 0)
				return result;
		}
	}

	return count;
}
DEVICE_ATTR_RW(ctgp_enable);

static ssize_t ctgp_offset_show(struct device *__unused, struct device_attribute *__unused,
				char *buf)
{
	int result = 0;
	u8 data = 0;

	result = uniwill_read_ec_ram(UW_EC_REG_CTGP_DB_CTGP_OFFSET, &data);
	if (result < 0)
		return result;

	return sysfs_emit(buf, "%u\n", data);
}
static ssize_t ctgp_offset_store(struct device *__unused, struct device_attribute *__unused,
				 const char *buf, size_t count)
{
	int result = 0;
	u8 data = 0;

	result = kstrtou8(buf, 0, &data);
	if (result < 0)
		return result;

	result = uniwill_write_ec_ram(UW_EC_REG_CTGP_DB_CTGP_OFFSET, data);
	if (result < 0)
		return result;

	return count;
}
DEVICE_ATTR_RW(ctgp_offset);

static u8 max_ctgp_offset = 25;
static ssize_t max_ctgp_offset_show(struct device *__unused, struct device_attribute *__unused,
				    char *buf)
{
	return sysfs_emit(buf, "%u\n", max_ctgp_offset);
}
DEVICE_ATTR_RO(max_ctgp_offset);

static ssize_t db_enable_show(struct device *__unused, struct device_attribute *__unused,
				char *buf)
{
	int result = 0;
	u8 data = 0;

	result = uniwill_read_ec_ram(UW_EC_REG_CTGP_DB_ENABLE, &data);
	if (result < 0)
		return result;

	return sysfs_emit(buf, "%u\n", data & UW_EC_REG_CTGP_DB_ENABLE_BIT_DB_ENABLE? 1 : 0);
}
static ssize_t db_enable_store(struct device *__unused, struct device_attribute *__unused,
				 const char *buf, size_t count)
{
	int result = 0;
	u8 data = 0;
	bool enable = false;

	result = kstrtobool(buf, &enable);
	if (result < 0)
		return result;

	result = uniwill_read_ec_ram(UW_EC_REG_CTGP_DB_ENABLE, &data);
	if (result < 0)
		return result;

	if (enable) {
		result = uniwill_write_ec_ram(UW_EC_REG_CTGP_DB_ENABLE, data |
					      UW_EC_REG_CTGP_DB_ENABLE_BIT_GENERAL_ENABLE |
					      UW_EC_REG_CTGP_DB_ENABLE_BIT_DB_ENABLE);
		if (result < 0)
			return result;
	}
	else {
		if (data & UW_EC_REG_CTGP_DB_ENABLE_BIT_CTGP_ENABLE) {
			result = uniwill_write_ec_ram(UW_EC_REG_CTGP_DB_ENABLE, data &
						      ~UW_EC_REG_CTGP_DB_ENABLE_BIT_DB_ENABLE);
			if (result < 0)
				return result;
		}
		else {
			result = uniwill_write_ec_ram(UW_EC_REG_CTGP_DB_ENABLE, data &
						      ~(UW_EC_REG_CTGP_DB_ENABLE_BIT_DB_ENABLE |
						      UW_EC_REG_CTGP_DB_ENABLE_BIT_GENERAL_ENABLE));
			if (result < 0)
				return result;
		}
	}

	return count;
}
DEVICE_ATTR_RW(db_enable);

static ssize_t db_offset_show(struct device *__unused, struct device_attribute *__unused, char *buf)
{
	int result = 0;
	u8 data = 0;

	result = uniwill_read_ec_ram(UW_EC_REG_CTGP_DB_DB_OFFSET, &data);
	if (result < 0)
		return result;

	return sysfs_emit(buf, "%u\n", data);
}
static ssize_t db_offset_store(struct device *__unused, struct device_attribute *__unused,
			       const char *buf, size_t count)
{
	int result = 0;
	u8 data = 0;

	result = kstrtou8(buf, 0, &data);
	if (result < 0)
		return result;

	result = uniwill_write_ec_ram(UW_EC_REG_CTGP_DB_DB_OFFSET, data);
	if (result < 0)
		return result;

	return count;
}
DEVICE_ATTR_RW(db_offset);

static u8 max_db_offset = 25;
static ssize_t max_db_offset_show(struct device *__unused, struct device_attribute *__unused,
				  char *buf)
{
	return sysfs_emit(buf, "%u\n", max_db_offset);
}
DEVICE_ATTR_RO(max_db_offset);

static ssize_t db_target_offset_show(struct device *__unused, struct device_attribute *__unused, char *buf)
{
	int result = 0;
	u8 data = 0;

	result = uniwill_read_ec_ram(UW_EC_REG_CTGP_DB_DB_TARGET_OFFSET, &data);
	if (result < 0)
		return result;

	return sysfs_emit(buf, "%u\n", data);
}
static ssize_t db_target_offset_store(struct device *__unused, struct device_attribute *__unused,
			       const char *buf, size_t count)
{
	int result = 0;
	u8 data = 0;

	result = kstrtou8(buf, 0, &data);
	if (result < 0)
		return result;

	result = uniwill_write_ec_ram(UW_EC_REG_CTGP_DB_DB_TARGET_OFFSET, data);
	if (result < 0)
		return result;

	return count;
}
DEVICE_ATTR_RW(db_target_offset);

static u8 max_combined_offset = 50;
static ssize_t max_combined_offset_show(struct device *__unused, struct device_attribute *__unused,
				  char *buf)
{
	return sysfs_emit(buf, "%u\n", max_combined_offset);
}
DEVICE_ATTR_RO(max_combined_offset);

static int __init init_db_and_ctgp(void)
{
	int result = 0;

	// TODO Get max cTGP and DB offsets

	result = uniwill_write_ec_ram(UW_EC_REG_CTGP_DB_ENABLE,
				      UW_EC_REG_CTGP_DB_ENABLE_BIT_GENERAL_ENABLE |
				      UW_EC_REG_CTGP_DB_ENABLE_BIT_DB_ENABLE |
				      UW_EC_REG_CTGP_DB_ENABLE_BIT_CTGP_ENABLE);
	if (result < 0)
		return result;

	result = uniwill_write_ec_ram(UW_EC_REG_CTGP_DB_CTGP_OFFSET, 0);
	if (result < 0)
		return result;

	result = uniwill_write_ec_ram(UW_EC_REG_CTGP_DB_DB_TARGET_OFFSET, 35);
	if (result < 0)
		return result;

	result = uniwill_write_ec_ram(UW_EC_REG_CTGP_DB_DB_OFFSET, 5);
	if (result < 0)
		return result;

	return 0;
}

static int __init init_sysfs_attrs(struct platform_device *pdev)
{
	int result = 0;

	result = sysfs_create_file(&pdev->dev.kobj, &dev_attr_ctgp_enable.attr);
	if (result)
		return result;

	result = sysfs_create_file(&pdev->dev.kobj, &dev_attr_ctgp_offset.attr);
	if (result)
		return result;

	result = sysfs_create_file(&pdev->dev.kobj, &dev_attr_max_ctgp_offset.attr);
	if (result)
		return result;

	result = sysfs_create_file(&pdev->dev.kobj, &dev_attr_db_enable.attr);
	if (result)
		return result;

	result = sysfs_create_file(&pdev->dev.kobj, &dev_attr_db_offset.attr);
	if (result)
		return result;

	result = sysfs_create_file(&pdev->dev.kobj, &dev_attr_max_db_offset.attr);
	if (result)
		return result;

	result = sysfs_create_file(&pdev->dev.kobj, &dev_attr_db_target_offset.attr);
	if (result)
		return result;

	result = sysfs_create_file(&pdev->dev.kobj, &dev_attr_max_combined_offset.attr);
	if (result)
		return result;

	return 0;
}

static int __init tuxedo_nb02_nvidia_power_ctrl_probe(struct platform_device *pdev) {
	int result = 0;
	u8 data = 0;
	char **uniwill_active_interface = NULL;

	// TODO this function is actually implemented in tuxedo_keyboard (via uniwill_keyboard.h)
	// and not in uniwill_wmi. This results in this module not depending on uniwill_wmi when
	// loading with modprobe.
	result = uniwill_get_active_interface_id(uniwill_active_interface);
	if (result < 0)
		return result;

	// TODO Replace by check for NVIDIA 3000 series or newer
	result = uniwill_read_ec_ram(UW_EC_REG_FAN_CTRL_STATUS, &data);
	if (result < 0)
		return result;
	if (!(data & UW_EC_REG_FAN_CTRL_STATUS_BIT_HAS_UW_FAN_CTRL))
		return -ENODEV;

	result = init_db_and_ctgp();
	if (result < 0)
		return result;

	result = init_sysfs_attrs(pdev);
	if (result < 0)
		return result;

	return 0;
}



// Boilerplate

static struct platform_device *tuxedo_nb02_nvidia_power_ctrl_device;
static struct platform_driver tuxedo_nb02_nvidia_power_ctrl_driver = {
	.driver.name = "tuxedo_nb02_nvidia_power_ctrl",
};

static int __init tuxedo_nb02_nvidia_power_ctrl_init(void)
{
	tuxedo_nb02_nvidia_power_ctrl_device =
		platform_create_bundle(&tuxedo_nb02_nvidia_power_ctrl_driver,
				       tuxedo_nb02_nvidia_power_ctrl_probe, NULL, 0, NULL, 0);

	if (IS_ERR(tuxedo_nb02_nvidia_power_ctrl_device))
		return PTR_ERR(tuxedo_nb02_nvidia_power_ctrl_device);

	return 0;
}

static void __exit tuxedo_nb02_nvidia_power_ctrl_exit(void)
{
	platform_device_unregister(tuxedo_nb02_nvidia_power_ctrl_device);
	platform_driver_unregister(&tuxedo_nb02_nvidia_power_ctrl_driver);
}

module_init(tuxedo_nb02_nvidia_power_ctrl_init);
module_exit(tuxedo_nb02_nvidia_power_ctrl_exit);

MODULE_AUTHOR("TUXEDO Computers GmbH <tux@tuxedocomputers.com>");
MODULE_DESCRIPTION("TUXEDO Computers Dynamic Boost and cTGP control driver for NVIDIA silicon for devices marked with board_vendor NB02");
MODULE_LICENSE("GPL");
MODULE_VERSION("0.0.1");
MODULE_ALIAS("platform:tuxedo_keyboard");
