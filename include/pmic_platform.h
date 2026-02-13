/**
 * @file pmic_platform.h
 * @brief Platform-specific configuration and UART output hooks.
 *
 * Edit this file to match your STM32 family, pin assignments,
 * flash sector layout, and UART peripheral.
 */

#ifndef PMIC_PLATFORM_H
#define PMIC_PLATFORM_H

#include <stdint.h>
#include <stdarg.h>
#include <stdio.h>
#include <string.h>

/* ----------------------------------------------------------------
 * STM32 family selection — uncomment ONE line for your target
 * ---------------------------------------------------------------- */
/* #define STM32F1 */
/* #define STM32F4 */
/* #define STM32L4 */
/* #define STM32H7 */

#ifdef STM32_HAL
  #if defined(STM32F1)
    #include "stm32f1xx_hal.h"
  #elif defined(STM32F4)
    #include "stm32f4xx_hal.h"
  #elif defined(STM32L4)
    #include "stm32l4xx_hal.h"
  #elif defined(STM32H7)
    #include "stm32h7xx_hal.h"
  #endif
#endif

/* ----------------------------------------------------------------
 * I2C peripheral configuration
 * ---------------------------------------------------------------- */
/* Which I2C peripheral to use (e.g., &hi2c1) — set at runtime */
/* Default I2C clock speed: 100 kHz (standard) or 400 kHz (fast) */
#define PMIC_I2C_SPEED_HZ       100000

/* ----------------------------------------------------------------
 * UART peripheral for CLI
 * ---------------------------------------------------------------- */
#define CLI_UART_BAUD            115200

/* ----------------------------------------------------------------
 * Flash storage configuration
 * ----------------------------------------------------------------
 * Adjust these for your specific STM32 variant.
 * Example: STM32F4 uses sector-based erase (e.g., Sector 7).
 * STM32L4/F1 use page-based erase.
 */
#define STORAGE_FLASH_BASE_ADDR  0x080E0000  /* Last 128KB sector on F4 */
#define STORAGE_FLASH_SIZE       0x20000     /* 128 KB */

#ifdef STM32_HAL
  /* STM32F4 example — adjust sector number for your chip */
  #define STORAGE_FLASH_SECTOR   FLASH_SECTOR_7
  #define STORAGE_FLASH_VOLTAGE  FLASH_VOLTAGE_RANGE_3
#endif

/* ----------------------------------------------------------------
 * UART output function (platform-specific)
 * ---------------------------------------------------------------- */

/**
 * @brief Transmit a string over UART. Implement per platform.
 */
void platform_uart_send(const char *str);

/**
 * @brief printf-style output over UART.
 */
void platform_printf(const char *fmt, ...);

/**
 * @brief Millisecond delay.
 */
void platform_delay_ms(uint32_t ms);

/**
 * @brief Get system tick in milliseconds.
 */
uint32_t platform_get_tick_ms(void);

#endif /* PMIC_PLATFORM_H */
