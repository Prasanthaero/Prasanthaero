/**
 * @file pmic_i2c.c
 * @brief I2C driver implementation for PMIC register access.
 */

#include "pmic_i2c.h"
#include "pmic_platform.h"

/* ---------------------------------------------------------------- */

pmic_i2c_status_t pmic_i2c_init(pmic_i2c_t *dev, void *hw_handle)
{
    if (!dev || !hw_handle)
        return PMIC_I2C_ERR_INVALID_PARAM;

#ifdef STM32_HAL
    dev->hi2c = (I2C_HandleTypeDef *)hw_handle;
#else
    dev->hw_handle = hw_handle;
#endif
    dev->initialized = true;
    return PMIC_I2C_OK;
}

/* ---------------------------------------------------------------- */

pmic_i2c_status_t pmic_i2c_write_reg(pmic_i2c_t *dev, uint8_t slave_addr,
                                      uint8_t reg, uint8_t value)
{
    if (!dev || !dev->initialized)
        return PMIC_I2C_ERR_NOT_INIT;

#ifdef STM32_HAL
    uint16_t addr16 = (uint16_t)(slave_addr << 1);
    HAL_StatusTypeDef ret;

    for (uint8_t retry = 0; retry < PMIC_I2C_MAX_RETRIES; retry++) {
        ret = HAL_I2C_Mem_Write(dev->hi2c, addr16, reg,
                                I2C_MEMADD_SIZE_8BIT,
                                &value, 1, PMIC_I2C_TIMEOUT_MS);
        if (ret == HAL_OK)
            return PMIC_I2C_OK;

        platform_delay_ms(5);
    }

    if (ret == HAL_TIMEOUT)
        return PMIC_I2C_ERR_TIMEOUT;
    return PMIC_I2C_ERR_NACK;
#else
    /* Host/stub build — log the operation */
    (void)slave_addr; (void)reg; (void)value;
    platform_printf("[I2C] WRITE 0x%02X reg 0x%02X = 0x%02X\r\n",
                    slave_addr, reg, value);
    return PMIC_I2C_OK;
#endif
}

/* ---------------------------------------------------------------- */

pmic_i2c_status_t pmic_i2c_read_reg(pmic_i2c_t *dev, uint8_t slave_addr,
                                     uint8_t reg, uint8_t *out_value)
{
    if (!dev || !dev->initialized)
        return PMIC_I2C_ERR_NOT_INIT;
    if (!out_value)
        return PMIC_I2C_ERR_INVALID_PARAM;

#ifdef STM32_HAL
    uint16_t addr16 = (uint16_t)(slave_addr << 1);
    HAL_StatusTypeDef ret;

    for (uint8_t retry = 0; retry < PMIC_I2C_MAX_RETRIES; retry++) {
        ret = HAL_I2C_Mem_Read(dev->hi2c, addr16, reg,
                               I2C_MEMADD_SIZE_8BIT,
                               out_value, 1, PMIC_I2C_TIMEOUT_MS);
        if (ret == HAL_OK)
            return PMIC_I2C_OK;

        platform_delay_ms(5);
    }

    if (ret == HAL_TIMEOUT)
        return PMIC_I2C_ERR_TIMEOUT;
    return PMIC_I2C_ERR_NACK;
#else
    (void)slave_addr; (void)reg;
    *out_value = 0x00;
    return PMIC_I2C_OK;
#endif
}

/* ---------------------------------------------------------------- */

pmic_i2c_status_t pmic_i2c_write_block(pmic_i2c_t *dev, uint8_t slave_addr,
                                        uint8_t start_reg, const uint8_t *data,
                                        uint16_t len)
{
    if (!dev || !dev->initialized)
        return PMIC_I2C_ERR_NOT_INIT;
    if (!data || len == 0)
        return PMIC_I2C_ERR_INVALID_PARAM;

#ifdef STM32_HAL
    uint16_t addr16 = (uint16_t)(slave_addr << 1);
    HAL_StatusTypeDef ret;

    for (uint8_t retry = 0; retry < PMIC_I2C_MAX_RETRIES; retry++) {
        ret = HAL_I2C_Mem_Write(dev->hi2c, addr16, start_reg,
                                I2C_MEMADD_SIZE_8BIT,
                                (uint8_t *)data, len,
                                PMIC_I2C_TIMEOUT_MS);
        if (ret == HAL_OK)
            return PMIC_I2C_OK;

        platform_delay_ms(5);
    }

    if (ret == HAL_TIMEOUT)
        return PMIC_I2C_ERR_TIMEOUT;
    return PMIC_I2C_ERR_NACK;
#else
    (void)slave_addr; (void)start_reg; (void)data; (void)len;
    return PMIC_I2C_OK;
#endif
}

/* ---------------------------------------------------------------- */

pmic_i2c_status_t pmic_i2c_read_block(pmic_i2c_t *dev, uint8_t slave_addr,
                                       uint8_t start_reg, uint8_t *data,
                                       uint16_t len)
{
    if (!dev || !dev->initialized)
        return PMIC_I2C_ERR_NOT_INIT;
    if (!data || len == 0)
        return PMIC_I2C_ERR_INVALID_PARAM;

#ifdef STM32_HAL
    uint16_t addr16 = (uint16_t)(slave_addr << 1);
    HAL_StatusTypeDef ret;

    for (uint8_t retry = 0; retry < PMIC_I2C_MAX_RETRIES; retry++) {
        ret = HAL_I2C_Mem_Read(dev->hi2c, addr16, start_reg,
                               I2C_MEMADD_SIZE_8BIT,
                               data, len, PMIC_I2C_TIMEOUT_MS);
        if (ret == HAL_OK)
            return PMIC_I2C_OK;

        platform_delay_ms(5);
    }

    if (ret == HAL_TIMEOUT)
        return PMIC_I2C_ERR_TIMEOUT;
    return PMIC_I2C_ERR_NACK;
#else
    (void)slave_addr; (void)start_reg;
    memset(data, 0, len);
    return PMIC_I2C_OK;
#endif
}

/* ---------------------------------------------------------------- */

pmic_i2c_status_t pmic_i2c_scan(pmic_i2c_t *dev, uint8_t *addrs,
                                 uint8_t max_addrs, uint8_t *found)
{
    if (!dev || !dev->initialized)
        return PMIC_I2C_ERR_NOT_INIT;
    if (!addrs || !found)
        return PMIC_I2C_ERR_INVALID_PARAM;

    *found = 0;

#ifdef STM32_HAL
    for (uint16_t addr = 0x03; addr <= 0x77; addr++) {
        HAL_StatusTypeDef ret;
        ret = HAL_I2C_IsDeviceReady(dev->hi2c, (uint16_t)(addr << 1),
                                     1, PMIC_I2C_TIMEOUT_MS);
        if (ret == HAL_OK) {
            if (*found < max_addrs) {
                addrs[*found] = (uint8_t)addr;
                (*found)++;
            }
        }
    }
#else
    /* Stub: simulate finding one device at 0x48 */
    if (max_addrs >= 1) {
        addrs[0] = 0x48;
        *found = 1;
    }
#endif

    return PMIC_I2C_OK;
}

/* ---------------------------------------------------------------- */

const char *pmic_i2c_status_str(pmic_i2c_status_t status)
{
    switch (status) {
    case PMIC_I2C_OK:                return "OK";
    case PMIC_I2C_ERR_NACK:         return "NACK";
    case PMIC_I2C_ERR_BUS:          return "Bus error";
    case PMIC_I2C_ERR_TIMEOUT:      return "Timeout";
    case PMIC_I2C_ERR_INVALID_PARAM:return "Invalid parameter";
    case PMIC_I2C_ERR_NOT_INIT:     return "Not initialized";
    default:                         return "Unknown";
    }
}
