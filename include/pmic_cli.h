/**
 * @file pmic_cli.h
 * @brief UART command-line interface for the PMIC clone tool.
 *
 * Provides an interactive serial console for reading, writing, cloning,
 * and managing PMIC configurations on TCON boards.
 */

#ifndef PMIC_CLI_H
#define PMIC_CLI_H

#include "pmic_clone.h"
#include "pmic_storage.h"

/* ---------- Configuration ---------- */

#define CLI_MAX_CMD_LEN     128
#define CLI_MAX_ARGS        8
#define CLI_PROMPT          "pmic> "

/* ---------- CLI context ---------- */

typedef struct {
    clone_ctx_t *clone_ctx;
    pmic_i2c_t  *i2c;
    char         cmd_buf[CLI_MAX_CMD_LEN];
    uint8_t      cmd_pos;
    bool         echo_enabled;
} cli_ctx_t;

/* ---------- API ---------- */

/**
 * @brief Initialize the CLI.
 */
void cli_init(cli_ctx_t *ctx, clone_ctx_t *clone_ctx, pmic_i2c_t *i2c);

/**
 * @brief Process one received character (call from UART RX interrupt/poll).
 *
 * When a full line is received (CR/LF), the command is parsed and executed.
 */
void cli_process_char(cli_ctx_t *ctx, char c);

/**
 * @brief Print the command prompt.
 */
void cli_print_prompt(cli_ctx_t *ctx);

/**
 * @brief Print welcome banner and help summary.
 */
void cli_print_banner(void);

#endif /* PMIC_CLI_H */
