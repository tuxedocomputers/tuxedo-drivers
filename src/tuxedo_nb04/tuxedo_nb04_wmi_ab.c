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
#include <linux/led-class-multicolor.h>
#include <linux/version.h>
#include <linux/delay.h>

#define dev_to_wdev(__dev)	container_of_const(__dev, struct wmi_device, dev)

#define NB04_WMI_AB_GUID	"80C9BAA6-AC48-4538-9234-9F81A55E7C85"

static DEFINE_MUTEX(nb04_wmi_ab_lock);

#define AB_INPUT_BUFFER_LENGTH_NORMAL	8
#define AB_INPUT_BUFFER_LENGTH_EXTENDED	496
#define AB_OUTPUT_BUFFER_LENGTH		80
#define AB_OUTPUT_BUFFER_LENGTH_REDUCED	10

enum wmi_return_status {
	WMI_RETURN_STATUS_SUCCESS = 0,
	WMI_RETURN_STATUS_UNSUPPORTED = 1,
	WMI_RETURN_STATUS_INVALID_PARAMETER = 2,
	WMI_RETURN_STATUS_UNDEFINED_DEVICE = 3,
	WMI_RETURN_STATUS_DEVICE_ERROR = 4,
	WMI_RETURN_STATUS_UNEXPECTED_ERROR = 5,
	WMI_RETURN_STATUS_TIMEOUT = 6,
	WMI_RETURN_STATUS_EC_BUSY = 7,
};

enum wmi_device_type_id {
	WMI_DEVICE_TYPE_ID_TOUCHPAD = 1,
	WMI_DEVICE_TYPE_ID_KEYBOARD = 2,
	WMI_DEVICE_TYPE_ID_APPDISPLAYPAGES = 3
};

enum wmi_keyboard_type {
	WMI_KEYBOARD_TYPE_NORMAL = 0,
	WMI_KEYBOARD_TYPE_PERKEY = 1,
	WMI_KEYBOARD_TYPE_4ZONE = 2,
	WMI_KEYBOARD_TYPE_WHITE = 3
};

enum wmi_keyboard_matrix {
	WMI_KEYBOARD_MATRIX_US = 0,
	WMI_KEYBOARD_MATRIX_UK = 1
};

enum wmi_color_preset {
	WMI_COLOR_PRESET_RED = 1,
	WMI_COLOR_PRESET_GREEN = 2,
	WMI_COLOR_PRESET_YELLOW = 3,
	WMI_COLOR_PRESET_BLUE = 4,
	WMI_COLOR_PRESET_PURPLE = 5,
	WMI_COLOR_PRESET_INDIGO = 6,
	WMI_COLOR_PRESET_WHITE = 7
};

#define KEYBOARD_MAX_BRIGHTNESS		0x0a
#define KEYBOARD_DEFAULT_BRIGHTNESS	0x00
#define KEYBOARD_DEFAULT_COLOR_RED	0xff
#define KEYBOARD_DEFAULT_COLOR_GREEN	0xff
#define KEYBOARD_DEFAULT_COLOR_BLUE	0xff

struct device_status_t {
	bool keyboard_state_enabled;
	enum wmi_keyboard_type keyboard_type;
	bool keyboard_sidebar_support;
	enum wmi_keyboard_matrix keyboard_matrix;
};

struct driver_data_t {
	struct led_classdev_mc mcled_cdev_keyboard;
	struct mc_subled mcled_cdev_subleds_keyboard[3];
	struct device_status_t device_status;
};

/**
 * Method interface 8 bytes in 80 bytes out
 */
static int nb04_wmi_ab_method_buffer(struct wmi_device *wdev, u32 wmi_method_id,
				     u8 *in, u8 *out)
{
	struct acpi_buffer acpi_buffer_in = { (acpi_size)AB_INPUT_BUFFER_LENGTH_NORMAL, in };
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

	if (acpi_object_out->type != ACPI_TYPE_BUFFER) {
		pr_err("No buffer for method (%u) call\n", wmi_method_id);
		kfree(return_buffer.pointer);
		return -EIO;
	}

	if (acpi_object_out->buffer.length != AB_OUTPUT_BUFFER_LENGTH) {
		pr_err("Unexpected buffer length: %u for method (%u) call\n", 
		       acpi_object_out->buffer.length, wmi_method_id);
		kfree(return_buffer.pointer);
		return -EIO;
	}

	memcpy(out, acpi_object_out->buffer.pointer, AB_OUTPUT_BUFFER_LENGTH);
	kfree(return_buffer.pointer);

	return 0;
}

/**
 * Method interface 8 bytes in 10 bytes out
 */
static int nb04_wmi_ab_method_buffer_reduced_output(struct wmi_device *wdev, u32 wmi_method_id,
				     u8 *in, u8 *out)
{
	struct acpi_buffer acpi_buffer_in = { (acpi_size)AB_INPUT_BUFFER_LENGTH_NORMAL, in };
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

	if (acpi_object_out->type != ACPI_TYPE_BUFFER) {
		pr_err("No buffer for method (%u) call\n", wmi_method_id);
		kfree(return_buffer.pointer);
		return -EIO;
	}

	if (acpi_object_out->buffer.length != AB_OUTPUT_BUFFER_LENGTH_REDUCED) {
		pr_err("Unexpected buffer length: %u for method (%u) call\n", 
		       acpi_object_out->buffer.length, wmi_method_id);
		kfree(return_buffer.pointer);
		return -EIO;
	}

	memcpy(out, acpi_object_out->buffer.pointer, AB_OUTPUT_BUFFER_LENGTH_REDUCED);
	kfree(return_buffer.pointer);

	return 0;
}

/**
 * Method interface 496 bytes in 80 bytes out
 */
static int nb04_wmi_ab_method_extended_input(struct wmi_device *wdev, u32 wmi_method_id,
					     u8 *in, u8 *out)
{
	struct acpi_buffer acpi_buffer_in = { (acpi_size)AB_INPUT_BUFFER_LENGTH_EXTENDED, in };
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

	if (acpi_object_out->type != ACPI_TYPE_BUFFER) {
		pr_err("No buffer for method (%u) call\n", wmi_method_id);
		kfree(return_buffer.pointer);
		return -EIO;
	}

	if (acpi_object_out->buffer.length != AB_OUTPUT_BUFFER_LENGTH) {
		pr_err("Unexpected buffer length: %u for method (%u) call\n", 
		       acpi_object_out->buffer.length, wmi_method_id);
		kfree(return_buffer.pointer);
		return -EIO;
	}

	memcpy(out, acpi_object_out->buffer.pointer, AB_OUTPUT_BUFFER_LENGTH);
	kfree(return_buffer.pointer);

	return 0;
}

/**
 * Method interface 8 bytes in integer out
 */
static int nb04_wmi_ab_method_int_out(struct wmi_device *wdev, u32 wmi_method_id,
				      u8 *in, u64 *out)
{
	struct acpi_buffer acpi_buffer_in = { (acpi_size)AB_INPUT_BUFFER_LENGTH_NORMAL, in };
	struct acpi_buffer return_buffer = { ACPI_ALLOCATE_BUFFER, NULL };
	union acpi_object *acpi_object_out;
	acpi_status status;

	pr_debug("evaluate: %u\n", wmi_method_id);
	status = wmidev_evaluate_method(wdev, 0, wmi_method_id,
					&acpi_buffer_in, &return_buffer);

	if (ACPI_FAILURE(status)) {
		pr_err("failed to evaluate wmi method\n");
		return -EIO;
	}

	acpi_object_out = (union acpi_object *) return_buffer.pointer;
	if (!acpi_object_out)
		return -ENODATA;

	if (acpi_object_out->type != ACPI_TYPE_INTEGER) {
		pr_err("No integer\n");
		kfree(return_buffer.pointer);
		return -EIO;
	}

	*out = acpi_object_out->integer.value;
	kfree(return_buffer.pointer);

	return 0;
}

int wmi_update_device_status_keyboard(struct wmi_device *wdev)
{
	u8 arg[AB_INPUT_BUFFER_LENGTH_NORMAL] = {0};
	u8 out[AB_OUTPUT_BUFFER_LENGTH] = {0};
	u16 wmi_return;
	int result;
	struct driver_data_t *driver_data = dev_get_drvdata(&wdev->dev);

	arg[0] = WMI_DEVICE_TYPE_ID_KEYBOARD;

	result = nb04_wmi_ab_method_buffer(wdev, 2, arg, out);
	if (result)
		return result;

	wmi_return = (out[1] << 8) | out[0];
	if (wmi_return != WMI_RETURN_STATUS_SUCCESS)
		return -EIO;

	driver_data->device_status.keyboard_state_enabled = out[2] == 1;
	driver_data->device_status.keyboard_type = out[3];
	driver_data->device_status.keyboard_sidebar_support = out[4] == 1;
	driver_data->device_status.keyboard_matrix = out[5];

	return 0;
}

int wmi_set_whole_keyboard(struct wmi_device *wdev, u8 red, u8 green, u8 blue, int brightness)
{
	u8 arg[AB_INPUT_BUFFER_LENGTH_NORMAL] = {0};
	u64 out;
	u8 brightness_byte = 0xfe, enable_byte = 0x01;
	int result;

	if (brightness == 0)
		enable_byte = 0x00;
	else if (brightness > 0 && brightness <= 10)
		brightness_byte = brightness;

	arg[0] = 0x00;
	arg[1] = red;
	arg[2] = green;
	arg[3] = blue;
	arg[4] = brightness_byte;
	arg[5] = 0xfe;
	arg[6] = 0x00;
	arg[7] = enable_byte;

	result = nb04_wmi_ab_method_int_out(wdev, 3, arg, &out);
	if (result)
		return result;

	if (out != 0)
		return -EIO;

	return 0;
}

void leds_set_brightness_mc_keyboard(struct led_classdev *led_cdev, enum led_brightness brightness)
{
	struct wmi_device *wdev = dev_to_wdev(led_cdev->dev->parent);
	struct led_classdev_mc *mcled_cdev = lcdev_to_mccdev(led_cdev);
	u8 red = mcled_cdev->subled_info[0].intensity;
	u8 green = mcled_cdev->subled_info[1].intensity;
	u8 blue = mcled_cdev->subled_info[2].intensity;

	pr_debug("wmi_set_whole_keyboard(%u, %u, %u, %u)\n", red, green, blue, brightness);

	wmi_set_whole_keyboard(wdev, red, green, blue, brightness);
}

static int init_leds(struct wmi_device *wdev)
{
	struct driver_data_t *driver_data = dev_get_drvdata(&wdev->dev);
	int retval;

	driver_data->mcled_cdev_keyboard.led_cdev.name = "rgb:" LED_FUNCTION_KBD_BACKLIGHT;
	driver_data->mcled_cdev_keyboard.led_cdev.max_brightness = KEYBOARD_MAX_BRIGHTNESS;
	driver_data->mcled_cdev_keyboard.led_cdev.brightness_set = &leds_set_brightness_mc_keyboard;
	driver_data->mcled_cdev_keyboard.led_cdev.brightness = KEYBOARD_DEFAULT_BRIGHTNESS;
	driver_data->mcled_cdev_keyboard.num_colors = 3;
	driver_data->mcled_cdev_keyboard.subled_info = driver_data->mcled_cdev_subleds_keyboard;
	driver_data->mcled_cdev_keyboard.subled_info[0].color_index = LED_COLOR_ID_RED;
	driver_data->mcled_cdev_keyboard.subled_info[0].intensity = KEYBOARD_DEFAULT_COLOR_RED;
	driver_data->mcled_cdev_keyboard.subled_info[0].channel = 0;
	driver_data->mcled_cdev_keyboard.subled_info[1].color_index = LED_COLOR_ID_GREEN;
	driver_data->mcled_cdev_keyboard.subled_info[1].intensity = KEYBOARD_DEFAULT_COLOR_GREEN;
	driver_data->mcled_cdev_keyboard.subled_info[1].channel = 0;
	driver_data->mcled_cdev_keyboard.subled_info[2].color_index = LED_COLOR_ID_BLUE;
	driver_data->mcled_cdev_keyboard.subled_info[2].intensity = KEYBOARD_DEFAULT_COLOR_BLUE;
	driver_data->mcled_cdev_keyboard.subled_info[2].channel = 0;

	retval = devm_led_classdev_multicolor_register(&wdev->dev, &driver_data->mcled_cdev_keyboard);
	if (retval)
		return retval;

	return 0;
}

#if LINUX_VERSION_CODE < KERNEL_VERSION(5, 3, 0)
static int tuxedo_nb04_wmi_ab_probe(struct wmi_device *wdev)
#else
static int tuxedo_nb04_wmi_ab_probe(struct wmi_device *wdev, const void *dummy_context)
#endif
{
	int result;
	struct driver_data_t *driver_data;

	pr_debug("driver probe\n");

	driver_data = devm_kzalloc(&wdev->dev, sizeof(struct driver_data_t), GFP_KERNEL);
	if (!driver_data)
		return -ENOMEM;

	dev_set_drvdata(&wdev->dev, driver_data);

	result = init_leds(wdev);
	if (result)
		return result;

	// Note: One read of keyboard status needed for fw init
	//       before writing can be done
	wmi_update_device_status_keyboard(wdev);

	return 0;
}

#if LINUX_VERSION_CODE < KERNEL_VERSION(5, 13, 0)
static int tuxedo_nb04_wmi_ab_remove(struct wmi_device *wdev)
#else
static void tuxedo_nb04_wmi_ab_remove(struct wmi_device *wdev)
#endif
{
	struct driver_data_t *driver_data = dev_get_drvdata(&wdev->dev);
	devm_led_classdev_multicolor_unregister(&wdev->dev, &driver_data->mcled_cdev_keyboard);
	pr_debug("driver remove\n");

#if LINUX_VERSION_CODE < KERNEL_VERSION(5, 13, 0)
	return 0;
#endif
}

static const struct wmi_device_id tuxedo_nb04_wmi_ab_device_ids[] = {
	{ .guid_string = NB04_WMI_AB_GUID },
	{ }
};

static struct wmi_driver tuxedo_nb04_wmi_ab_driver = {
	.driver = {
		.name = "tuxedo_nb04_wmi_ab",
		.owner = THIS_MODULE
	},
	.id_table = tuxedo_nb04_wmi_ab_device_ids,
	.probe = tuxedo_nb04_wmi_ab_probe,
	.remove = tuxedo_nb04_wmi_ab_remove,
};

module_wmi_driver(tuxedo_nb04_wmi_ab_driver);

MODULE_AUTHOR("TUXEDO Computers GmbH <tux@tuxedocomputers.com>");
MODULE_DESCRIPTION("Driver for NB04 WMI AB methods");
MODULE_VERSION("0.0.1");
MODULE_LICENSE("GPL");

MODULE_DEVICE_TABLE(wmi, tuxedo_nb04_wmi_ab_device_ids);
MODULE_ALIAS("wmi:" NB04_WMI_AB_GUID);
