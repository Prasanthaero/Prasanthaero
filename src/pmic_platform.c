/**
 * @file pmic_platform.c
 * @brief Platform-specific implementations (UART output, delay, tick).
 *
 * This file provides the glue between the PMIC tool and the STM32 HAL.
 * Adjust UART handle and peripheral references for your board.
 */

#ifdef HOST_BUILD
#define _POSIX_C_SOURCE 199309L
#endif

#include "pmic_platform.h"
#include <string.h>

/* ================================================================
 * STM32 HAL implementation
 * ================================================================ */

#ifdef STM32_HAL

extern UART_HandleTypeDef huart2;  /* Adjust to your UART peripheral */

void platform_uart_send(const char *str)
{
    if (!str) return;
    uint16_t len = (uint16_t)strlen(str);
    HAL_UART_Transmit(&huart2, (uint8_t *)str, len, 1000);
}

void platform_printf(const char *fmt, ...)
{
    char buf[256];
    va_list args;
    va_start(args, fmt);
    vsnprintf(buf, sizeof(buf), fmt, args);
    va_end(args);
    platform_uart_send(buf);
}

void platform_delay_ms(uint32_t ms)
{
    HAL_Delay(ms);
}

uint32_t platform_get_tick_ms(void)
{
    return HAL_GetTick();
}

#endif /* STM32_HAL */

/* ================================================================
 * Host build (for PC testing / simulation)
 * ================================================================ */

#ifdef HOST_BUILD

#include <stdio.h>
#include <unistd.h>
#include <time.h>

void platform_uart_send(const char *str)
{
    if (str) fputs(str, stdout);
    fflush(stdout);
}

void platform_printf(const char *fmt, ...)
{
    va_list args;
    va_start(args, fmt);
    vprintf(fmt, args);
    va_end(args);
    fflush(stdout);
}

void platform_delay_ms(uint32_t ms)
{
    struct timespec ts_delay;
    ts_delay.tv_sec  = ms / 1000;
    ts_delay.tv_nsec = (ms % 1000) * 1000000L;
    nanosleep(&ts_delay, NULL);
}

uint32_t platform_get_tick_ms(void)
{
    struct timespec ts;
    clock_gettime(CLOCK_MONOTONIC, &ts);
    return (uint32_t)(ts.tv_sec * 1000 + ts.tv_nsec / 1000000);
}

#endif /* HOST_BUILD */
