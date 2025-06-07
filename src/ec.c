// SPDX-License-Identifier: GPL-2.0

#include <linux/acpi.h>
#include <linux/compiler_types.h>
#include <linux/error-injection.h>
#include <linux/rwsem.h>
#include <linux/printk.h>
#include <linux/wmi.h>
#include <linux/module.h>

#include "ec.h"
#include "uniwill_interfaces.h"

/* ========================================================================== */

static DECLARE_RWSEM(ec_lock);

/* ========================================================================== */

int __must_check tuxedo_ec_lock(void)
{
	return down_write_killable(&ec_lock);
}
EXPORT_SYMBOL(tuxedo_ec_lock);

void tuxedo_ec_unlock(void)
{
	up_write(&ec_lock);
}
EXPORT_SYMBOL(tuxedo_ec_unlock);

int __must_check tuxedo_ec_transaction(uint16_t addr, uint16_t data,
				     union tuxedo_ec_result *result, bool read)
{
	uint8_t buf[] = {
		addr & 0xFF,
		addr >> 8,
		data & 0xFF,
		data >> 8,
		0,
		read ? 1 : 0,
		0,
		0,
	};
	static_assert(ARRAY_SIZE(buf) == 8);

	/* the returned ACPI_TYPE_BUFFER is 40 bytes long for some reason ... */
	uint8_t output_buf[sizeof(union acpi_object) + 40];

	struct acpi_buffer input = { sizeof(buf), buf },
			   output = { sizeof(output_buf), output_buf };
	union acpi_object *obj;
	acpi_status status = AE_OK;
	int err;

	if (read) err = down_read_killable(&ec_lock);
	else      err = down_write_killable(&ec_lock);

	if (err)
		goto out;

	memset(output_buf, 0, sizeof(output_buf));

	status = wmi_evaluate_method(UNIWILL_WMI_MGMT_GUID_BC, 0,
				     4, &input, &output);

	if (read) up_read(&ec_lock);
	else      up_write(&ec_lock);

	if (ACPI_FAILURE(status)) {
		err = -EIO;
		goto out;
	}

	obj = output.pointer;

	if (result) {
		if (obj && obj->type == ACPI_TYPE_BUFFER && obj->buffer.length >= sizeof(*result)) {
			memcpy(result, obj->buffer.pointer, sizeof(*result));
		} else {
			err = -ENODATA;
			goto out;
		}
	}

out:
	pr_debug(
		"%s(addr=%#06x, data=%#06x, result=%c, read=%c)"
		": (%d) [%#010lx] %s"
		": [%*ph]\n",

		__func__, (unsigned int) addr, (unsigned int) data,
		result ? 'y' : 'n', read ? 'y' : 'n',
		err, (unsigned long) status, acpi_format_exception(status),
		(obj && obj->type == ACPI_TYPE_BUFFER) ?
			(int) min(sizeof(*result), (size_t) obj->buffer.length) : 0,
		(obj && obj->type == ACPI_TYPE_BUFFER) ?
			obj->buffer.pointer : NULL
	);

	return err;
}
ALLOW_ERROR_INJECTION(tuxedo_ec_transaction, ERRNO);
EXPORT_SYMBOL(tuxedo_ec_transaction);

