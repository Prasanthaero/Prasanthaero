/**
 * @file pmic_i2c.h
 * @brief I2C driver abstraction for PMIC register access on STM32.
 *
 * Provides low-level I2C read/write operations for communicating with
 * PMIC devices on TCON boards.
 */

#ifndef PMIC_I2C_H
#define PMIC_I2C_H

#include <stdint.h>
#include <stdbool.h>

#ifdef STM32_HAL
#include "stm32f4xx_hal.h"  /* Adjust for your STM32 family */
#endif

/* ---------- Configuration ---------- */

#define PMIC_I2C_TIMEOUT_MS     100
#define PMIC_I2C_MAX_RETRIES    3

/* ---------- Status codes ---------- */

typedef enum {
    PMIC_I2C_OK = 0,
    PMIC_I2C_ERR_NACK,
    PMIC_I2C_ERR_BUS,
    PMIC_I2C_ERR_TIMEOUT,
    PMIC_I2C_ERR_INVALID_PARAM,
    PMIC_I2C_ERR_NOT_INIT,
} pmic_i2c_status_t;

/* ---------- Handle ---------- */

typedef struct {
#ifdef STM32_HAL
    I2C_HandleTypeDef *hi2c;
#else
    void *hw_handle;  /* Opaque pointer for portability */
#endif
    bool initialized;
} pmic_i2c_t;

/* ---------- API ---------- */

/**
 * @brief Initialize the I2C driver.
 * @param dev       Pointer to driver handle.
 * @param hw_handle Platform-specific I2C peripheral handle.
 * @return Status code.
 */
pmic_i2c_status_t pmic_i2c_init(pmic_i2c_t *dev, void *hw_handle);

/**
 * @brief Write a single register.
 * @param dev      Driver handle.
 * @param slave_addr 7-bit I2C address.
 * @param reg      Register address.
 * @param value    Value to write.
 */
pmic_i2c_status_t pmic_i2c_write_reg(pmic_i2c_t *dev, uint8_t slave_addr,
                                      uint8_t reg, uint8_t value);

/**
 * @brief Read a single register.
 * @param dev       Driver handle.
 * @param slave_addr 7-bit I2C address.
 * @param reg       Register address.
 * @param out_value Pointer to store read value.
 */
pmic_i2c_status_t pmic_i2c_read_reg(pmic_i2c_t *dev, uint8_t slave_addr,
                                     uint8_t reg, uint8_t *out_value);

/**
 * @brief Write a block of consecutive registers.
 */
pmic_i2c_status_t pmic_i2c_write_block(pmic_i2c_t *dev, uint8_t slave_addr,
                                        uint8_t start_reg, const uint8_t *data,
                                        uint16_t len);

/**
 * @brief Read a block of consecutive registers.
 */
pmic_i2c_status_t pmic_i2c_read_block(pmic_i2c_t *dev, uint8_t slave_addr,
                                       uint8_t start_reg, uint8_t *data,
                                       uint16_t len);

/**
 * @brief Scan the I2C bus and report devices found.
 * @param dev       Driver handle.
 * @param addrs     Array to store found addresses (caller allocates).
 * @param max_addrs Size of addrs array.
 * @param found     Number of devices found.
 */
pmic_i2c_status_t pmic_i2c_scan(pmic_i2c_t *dev, uint8_t *addrs,
                                 uint8_t max_addrs, uint8_t *found);

/**
 * @brief Convert status code to human-readable string.
 */
const char *pmic_i2c_status_str(pmic_i2c_status_t status);

#endif /* PMIC_I2C_H */
