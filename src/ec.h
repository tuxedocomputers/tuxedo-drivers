// SPDX-License-Identifier: GPL-2.0
#ifndef tuxedo_LAPTOP_EC_H
#define tuxedo_LAPTOP_EC_H

#include <linux/compiler_types.h>
#include <linux/types.h>

/* ========================================================================== */
/*
 * EC register addresses and bitmasks,
 * some of them are not used,
 * only for documentation
 */

#define ADDR(page, offset)       (((uint16_t)(page) << 8) | ((uint16_t)(offset)))

/* ========================================================================== */

#define AP_BIOS_BYTE_ADDR ADDR(0x07, 0xA4)
#define AP_BIOS_BYTE_FN_LOCK_SWITCH BIT(3)

/* ========================================================================== */

/* battery charger control register */
#define BATT_CHARGE_CTRL_ADDR ADDR(0x07, 0xB9)
#define BATT_CHARGE_CTRL_VALUE_MASK GENMASK(6, 0)
#define BATT_CHARGE_CTRL_REACHED BIT(7)

#define BATT_STATUS_ADDR        ADDR(0x04, 0x32)
#define BATT_STATUS_DISCHARGING BIT(0)

/* possibly temp (in C) = value / 10 + X */
#define BATT_TEMP_ADDR ADDR(0x04, 0xA2)

#define BATT_ALERT_ADDR ADDR(0x04, 0x94)

/* ========================================================================== */

union tuxedo_ec_result {
	uint32_t dword;
	struct {
		uint16_t w1;
		uint16_t w2;
	} words;
	struct {
		uint8_t b1;
		uint8_t b2;
		uint8_t b3;
		uint8_t b4;
	} bytes;
};

int __must_check tuxedo_ec_transaction(uint16_t addr, uint16_t data,
				     union tuxedo_ec_result *result, bool read);

static inline __must_check int tuxedo_ec_read(uint16_t addr, union tuxedo_ec_result *result)
{
	return tuxedo_ec_transaction(addr, 0, result, true);
}

static inline __must_check int tuxedo_ec_write(uint16_t addr, uint16_t data)
{
	return tuxedo_ec_transaction(addr, data, NULL, false);
}

static inline __must_check int ec_write_byte(uint16_t addr, uint8_t data)
{
	return tuxedo_ec_write(addr, data);
}

static inline __must_check int ec_read_byte(uint16_t addr)
{
	union tuxedo_ec_result result;
	int err;

	err = tuxedo_ec_read(addr, &result);

	if (err)
		return err;

	return result.bytes.b1;
}

#endif /* tuxedo_LAPTOP_EC_H */
