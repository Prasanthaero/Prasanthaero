/**
 * @file pmic_cli.c
 * @brief UART command-line interface for the PMIC clone tool.
 *
 * Commands:
 *   help                    — Show available commands
 *   scan                    — Scan I2C bus for devices
 *   profile list            — List built-in PMIC profiles
 *   profile select <name>   — Select a profile (or "generic <addr> <nregs>")
 *   read [addr]             — Read all registers from PMIC
 *   write [addr]            — Write snapshot to target PMIC
 *   verify [addr]           — Verify target matches snapshot
 *   dump                    — Print current snapshot
 *   reg read <reg>          — Read a single register
 *   reg write <reg> <val>   — Write a single register
 *   save <slot>             — Save snapshot to flash slot (0-3)
 *   load <slot>             — Load snapshot from flash slot
 *   slots                   — Show flash slot status
 *   erase <slot|all>        — Erase flash slot(s)
 *   diff <slotA> <slotB>    — Compare two saved snapshots
 *   clone [src] [dst]       — Full clone: read source → write target → verify
 *   status                  — Show current context state
 */

#include "pmic_cli.h"
#include "pmic_platform.h"
#include <string.h>
#include <stdlib.h>
#include <ctype.h>

/* ================================================================
 * Helpers
 * ================================================================ */

static pmic_profile_t s_generic_profile;  /* For "generic" profile */

static uint8_t parse_hex_or_dec(const char *s)
{
    if (!s) return 0;
    if (s[0] == '0' && (s[1] == 'x' || s[1] == 'X'))
        return (uint8_t)strtoul(s, NULL, 16);
    return (uint8_t)strtoul(s, NULL, 0);
}

static int tokenize(char *buf, char *argv[], int max_args)
{
    int argc = 0;
    char *p = buf;

    while (*p && argc < max_args) {
        while (*p == ' ' || *p == '\t') p++;
        if (*p == '\0') break;
        argv[argc++] = p;
        while (*p && *p != ' ' && *p != '\t') p++;
        if (*p) *p++ = '\0';
    }
    return argc;
}

/* ================================================================
 * Command handlers
 * ================================================================ */

static void cmd_help(void)
{
    platform_printf(
        "Commands:\r\n"
        "  help                      Show this help\r\n"
        "  scan                      Scan I2C bus for devices\r\n"
        "  profile list              List PMIC profiles\r\n"
        "  profile select <name>     Select a built-in profile\r\n"
        "  profile generic <addr> <n> Create generic profile\r\n"
        "  read [addr]               Read all regs from source PMIC\r\n"
        "  write [addr]              Write snapshot to target PMIC\r\n"
        "  verify [addr]             Verify target matches snapshot\r\n"
        "  dump                      Print current register snapshot\r\n"
        "  reg read <reg> [addr]     Read one register\r\n"
        "  reg write <reg> <val> [addr] Write one register\r\n"
        "  save <slot>               Save snapshot to flash (0-%d)\r\n"
        "  load <slot>               Load snapshot from flash\r\n"
        "  slots                     Show flash slot status\r\n"
        "  erase <slot|all>          Erase flash slot(s)\r\n"
        "  diff <slotA> <slotB>      Compare two snapshots\r\n"
        "  clone [src_addr] [dst_addr]  Read+Write+Verify in one step\r\n"
        "  status                    Show current state\r\n",
        STORAGE_MAX_SLOTS - 1
    );
}

static void cmd_scan(cli_ctx_t *ctx)
{
    uint8_t addrs[16];
    uint8_t found = 0;

    pmic_i2c_status_t rc = pmic_i2c_scan(ctx->i2c, addrs, 16, &found);
    if (rc != PMIC_I2C_OK) {
        platform_printf("Scan error: %s\r\n", pmic_i2c_status_str(rc));
        return;
    }

    platform_printf("Found %u device(s):\r\n", found);
    for (uint8_t i = 0; i < found; i++) {
        platform_printf("  0x%02X\r\n", addrs[i]);
    }
}

static void cmd_profile_list(void)
{
    uint8_t count = pmic_profile_count();
    platform_printf("Built-in profiles (%u):\r\n", count);
    for (uint8_t i = 0; i < count; i++) {
        const pmic_profile_t *p = pmic_profile_get(i);
        platform_printf("  [%u] %-16s addr=0x%02X regs=%u\r\n",
                        i, p->name, p->i2c_addr, p->reg_count);
    }
    platform_printf("  Use 'profile generic <addr> <nregs>' for custom.\r\n");
}

static void cmd_profile_select(cli_ctx_t *ctx, const char *name)
{
    const pmic_profile_t *prof = pmic_profile_find(name);
    if (!prof) {
        platform_printf("Profile '%s' not found. Use 'profile list'.\r\n", name);
        return;
    }

    clone_status_t rc = clone_init(ctx->clone_ctx, ctx->i2c, prof);
    if (rc != CLONE_OK) {
        platform_printf("Init failed: %s\r\n", clone_status_str(rc));
        return;
    }

    platform_printf("Profile set: %s (0x%02X, %u regs)\r\n",
                    prof->name, prof->i2c_addr, prof->reg_count);
}

static void cmd_profile_generic(cli_ctx_t *ctx, uint8_t addr, uint16_t nregs)
{
    pmic_profile_create_generic(&s_generic_profile, "CUSTOM",
                                 addr, nregs);

    clone_status_t rc = clone_init(ctx->clone_ctx, ctx->i2c,
                                    &s_generic_profile);
    if (rc != CLONE_OK) {
        platform_printf("Init failed: %s\r\n", clone_status_str(rc));
        return;
    }

    platform_printf("Generic profile: addr=0x%02X, %u regs\r\n",
                    addr, nregs);
}

static void cmd_read(cli_ctx_t *ctx, uint8_t addr)
{
    if (!ctx->clone_ctx->profile) {
        platform_printf("No profile selected. Use 'profile select'.\r\n");
        return;
    }
    clone_status_t rc = clone_read_source(ctx->clone_ctx, addr);
    if (rc != CLONE_OK && rc != CLONE_ERR_I2C) {
        platform_printf("Read error: %s\r\n", clone_status_str(rc));
    }
}

static void cmd_write(cli_ctx_t *ctx, uint8_t addr)
{
    if (!ctx->clone_ctx->snapshot_valid) {
        platform_printf("No snapshot loaded. Read a source first.\r\n");
        return;
    }
    clone_status_t rc = clone_write_target(ctx->clone_ctx, addr);
    if (rc != CLONE_OK) {
        platform_printf("Write error: %s\r\n", clone_status_str(rc));
    }
}

static void cmd_verify(cli_ctx_t *ctx, uint8_t addr)
{
    if (!ctx->clone_ctx->snapshot_valid) {
        platform_printf("No snapshot loaded.\r\n");
        return;
    }
    uint16_t mismatches = 0;
    clone_verify(ctx->clone_ctx, addr, &mismatches);
}

static void cmd_dump(cli_ctx_t *ctx)
{
    const pmic_snapshot_t *snap = clone_get_snapshot(ctx->clone_ctx);
    if (!snap) {
        platform_printf("No snapshot in memory.\r\n");
        return;
    }

    platform_printf("Snapshot: %s @ 0x%02X (%u regs, CRC 0x%08lX)\r\n",
                    snap->profile_name, snap->i2c_addr,
                    snap->entry_count, (unsigned long)snap->checksum);
    platform_printf("--------------------------------------\r\n");

    const pmic_profile_t *prof = ctx->clone_ctx->profile;

    for (uint16_t i = 0; i < snap->entry_count; i++) {
        const char *name = "???";
        uint8_t flags = 0;

        if (prof && i < prof->reg_count) {
            name  = prof->regs[i].name;
            flags = prof->regs[i].flags;
        }

        char flag_str[8] = "";
        if (flags & PMIC_REG_RO) strcat(flag_str, "RO");
        else if (flags & PMIC_REG_RW) strcat(flag_str, "RW");
        else if (flags & PMIC_REG_WO) strcat(flag_str, "WO");

        if (snap->entries[i].valid) {
            platform_printf("  0x%02X = 0x%02X  %-2s  %s\r\n",
                            snap->entries[i].addr,
                            snap->entries[i].value,
                            flag_str, name);
        } else {
            platform_printf("  0x%02X = ----  %-2s  %s (invalid)\r\n",
                            snap->entries[i].addr,
                            flag_str, name);
        }
    }
}

static void cmd_reg_read(cli_ctx_t *ctx, uint8_t reg, uint8_t addr)
{
    if (!ctx->clone_ctx->profile) {
        platform_printf("No profile selected.\r\n");
        return;
    }
    uint8_t val = 0;
    clone_status_t rc = clone_read_single(ctx->clone_ctx, addr, reg, &val);
    if (rc == CLONE_OK)
        platform_printf("Reg 0x%02X = 0x%02X\r\n", reg, val);
    else
        platform_printf("Read error: %s\r\n", clone_status_str(rc));
}

static void cmd_reg_write(cli_ctx_t *ctx, uint8_t reg, uint8_t val,
                           uint8_t addr)
{
    if (!ctx->clone_ctx->profile) {
        platform_printf("No profile selected.\r\n");
        return;
    }
    clone_status_t rc = clone_write_single(ctx->clone_ctx, addr, reg, val);
    if (rc == CLONE_OK)
        platform_printf("Reg 0x%02X <= 0x%02X OK\r\n", reg, val);
    else
        platform_printf("Write error: %s\r\n", clone_status_str(rc));
}

static void cmd_save(cli_ctx_t *ctx, uint8_t slot)
{
    const pmic_snapshot_t *snap = clone_get_snapshot(ctx->clone_ctx);
    if (!snap) {
        platform_printf("No snapshot to save.\r\n");
        return;
    }
    storage_status_t rc = storage_save(slot, snap);
    if (rc == STORAGE_OK)
        platform_printf("Saved to slot %u.\r\n", slot);
    else
        platform_printf("Save error: %s\r\n", storage_status_str(rc));
}

static void cmd_load(cli_ctx_t *ctx, uint8_t slot)
{
    pmic_snapshot_t snap;
    storage_status_t rc = storage_load(slot, &snap);
    if (rc != STORAGE_OK) {
        platform_printf("Load error: %s\r\n", storage_status_str(rc));
        return;
    }

    clone_status_t crc = clone_load_snapshot(ctx->clone_ctx, &snap);
    if (crc == CLONE_OK)
        platform_printf("Loaded slot %u: %s @ 0x%02X (%u regs)\r\n",
                        slot, snap.profile_name, snap.i2c_addr,
                        snap.entry_count);
    else
        platform_printf("Load error: %s\r\n", clone_status_str(crc));
}

static void cmd_slots(void)
{
    char buf[128];
    platform_printf("Flash storage slots:\r\n");
    for (uint8_t i = 0; i < STORAGE_MAX_SLOTS; i++) {
        storage_slot_info(i, buf, sizeof(buf));
        platform_printf("  %s\r\n", buf);
    }
}

static void cmd_erase(const char *arg)
{
    if (strcmp(arg, "all") == 0) {
        storage_status_t rc = storage_erase_all();
        platform_printf("Erase all: %s\r\n", storage_status_str(rc));
    } else {
        uint8_t slot = (uint8_t)atoi(arg);
        storage_status_t rc = storage_erase_slot(slot);
        platform_printf("Erase slot %u: %s\r\n", slot,
                        storage_status_str(rc));
    }
}

static void cmd_diff(uint8_t slotA, uint8_t slotB)
{
    pmic_snapshot_t a, b;

    storage_status_t ra = storage_load(slotA, &a);
    if (ra != STORAGE_OK) {
        platform_printf("Slot %u: %s\r\n", slotA, storage_status_str(ra));
        return;
    }

    storage_status_t rb = storage_load(slotB, &b);
    if (rb != STORAGE_OK) {
        platform_printf("Slot %u: %s\r\n", slotB, storage_status_str(rb));
        return;
    }

    platform_printf("Comparing slot %u (%s) vs slot %u (%s):\r\n",
                    slotA, a.profile_name, slotB, b.profile_name);

    uint16_t diffs = 0;
    clone_diff(&a, &b, &diffs);
    platform_printf("Total differences: %u\r\n", diffs);
}

static void cmd_clone(cli_ctx_t *ctx, uint8_t src_addr, uint8_t dst_addr)
{
    if (!ctx->clone_ctx->profile) {
        platform_printf("No profile selected. Use 'profile select' first.\r\n");
        return;
    }

    platform_printf("=== CLONE: Read source ===\r\n");
    clone_status_t rc = clone_read_source(ctx->clone_ctx, src_addr);
    if (rc != CLONE_OK) {
        platform_printf("Read failed: %s. Aborting.\r\n",
                        clone_status_str(rc));
        return;
    }

    platform_printf("\r\n=== CLONE: Write target ===\r\n");
    rc = clone_write_target(ctx->clone_ctx, dst_addr);
    if (rc != CLONE_OK) {
        platform_printf("Write failed: %s. Verify skipped.\r\n",
                        clone_status_str(rc));
        return;
    }

    platform_printf("\r\n=== CLONE: Verify target ===\r\n");
    uint16_t mismatches = 0;
    clone_verify(ctx->clone_ctx, dst_addr, &mismatches);

    if (mismatches == 0)
        platform_printf("\r\nCLONE COMPLETE — all registers verified OK.\r\n");
    else
        platform_printf("\r\nCLONE DONE with %u mismatches.\r\n", mismatches);
}

static void cmd_status(cli_ctx_t *ctx)
{
    platform_printf("PMIC Clone Tool Status\r\n");
    platform_printf("  Profile: ");
    if (ctx->clone_ctx->profile)
        platform_printf("%s (0x%02X, %u regs)\r\n",
                        ctx->clone_ctx->profile->name,
                        ctx->clone_ctx->profile->i2c_addr,
                        ctx->clone_ctx->profile->reg_count);
    else
        platform_printf("(none)\r\n");

    platform_printf("  Snapshot: %s\r\n",
                    ctx->clone_ctx->snapshot_valid ? "loaded" : "empty");

    if (ctx->clone_ctx->snapshot_valid) {
        platform_printf("  Snapshot CRC: 0x%08lX\r\n",
                        (unsigned long)ctx->clone_ctx->snapshot.checksum);
    }
}

/* ================================================================
 * Command dispatcher
 * ================================================================ */

static void dispatch(cli_ctx_t *ctx, char *line)
{
    char *argv[CLI_MAX_ARGS];
    int argc = tokenize(line, argv, CLI_MAX_ARGS);

    if (argc == 0) return;

    /* --- help --- */
    if (strcmp(argv[0], "help") == 0 || strcmp(argv[0], "?") == 0) {
        cmd_help();
    }
    /* --- scan --- */
    else if (strcmp(argv[0], "scan") == 0) {
        cmd_scan(ctx);
    }
    /* --- profile --- */
    else if (strcmp(argv[0], "profile") == 0 && argc >= 2) {
        if (strcmp(argv[1], "list") == 0) {
            cmd_profile_list();
        } else if (strcmp(argv[1], "select") == 0 && argc >= 3) {
            cmd_profile_select(ctx, argv[2]);
        } else if (strcmp(argv[1], "generic") == 0 && argc >= 4) {
            uint8_t addr = parse_hex_or_dec(argv[2]);
            uint16_t nregs = (uint16_t)strtoul(argv[3], NULL, 0);
            cmd_profile_generic(ctx, addr, nregs);
        } else {
            platform_printf("Usage: profile list|select <name>|"
                            "generic <addr> <nregs>\r\n");
        }
    }
    /* --- read --- */
    else if (strcmp(argv[0], "read") == 0) {
        uint8_t addr = (argc >= 2) ? parse_hex_or_dec(argv[1]) : 0;
        cmd_read(ctx, addr);
    }
    /* --- write --- */
    else if (strcmp(argv[0], "write") == 0) {
        uint8_t addr = (argc >= 2) ? parse_hex_or_dec(argv[1]) : 0;
        cmd_write(ctx, addr);
    }
    /* --- verify --- */
    else if (strcmp(argv[0], "verify") == 0) {
        uint8_t addr = (argc >= 2) ? parse_hex_or_dec(argv[1]) : 0;
        cmd_verify(ctx, addr);
    }
    /* --- dump --- */
    else if (strcmp(argv[0], "dump") == 0) {
        cmd_dump(ctx);
    }
    /* --- reg --- */
    else if (strcmp(argv[0], "reg") == 0 && argc >= 3) {
        if (strcmp(argv[1], "read") == 0) {
            uint8_t reg = parse_hex_or_dec(argv[2]);
            uint8_t addr = (argc >= 4) ? parse_hex_or_dec(argv[3]) : 0;
            cmd_reg_read(ctx, reg, addr);
        } else if (strcmp(argv[1], "write") == 0 && argc >= 4) {
            uint8_t reg = parse_hex_or_dec(argv[2]);
            uint8_t val = parse_hex_or_dec(argv[3]);
            uint8_t addr = (argc >= 5) ? parse_hex_or_dec(argv[4]) : 0;
            cmd_reg_write(ctx, reg, val, addr);
        } else {
            platform_printf("Usage: reg read <reg> [addr] | "
                            "reg write <reg> <val> [addr]\r\n");
        }
    }
    /* --- save --- */
    else if (strcmp(argv[0], "save") == 0 && argc >= 2) {
        uint8_t slot = (uint8_t)atoi(argv[1]);
        cmd_save(ctx, slot);
    }
    /* --- load --- */
    else if (strcmp(argv[0], "load") == 0 && argc >= 2) {
        uint8_t slot = (uint8_t)atoi(argv[1]);
        cmd_load(ctx, slot);
    }
    /* --- slots --- */
    else if (strcmp(argv[0], "slots") == 0) {
        cmd_slots();
    }
    /* --- erase --- */
    else if (strcmp(argv[0], "erase") == 0 && argc >= 2) {
        cmd_erase(argv[1]);
    }
    /* --- diff --- */
    else if (strcmp(argv[0], "diff") == 0 && argc >= 3) {
        uint8_t a = (uint8_t)atoi(argv[1]);
        uint8_t b = (uint8_t)atoi(argv[2]);
        cmd_diff(a, b);
    }
    /* --- clone --- */
    else if (strcmp(argv[0], "clone") == 0) {
        uint8_t src = (argc >= 2) ? parse_hex_or_dec(argv[1]) : 0;
        uint8_t dst = (argc >= 3) ? parse_hex_or_dec(argv[2]) : 0;
        cmd_clone(ctx, src, dst);
    }
    /* --- status --- */
    else if (strcmp(argv[0], "status") == 0) {
        cmd_status(ctx);
    }
    /* --- unknown --- */
    else {
        platform_printf("Unknown command: '%s'. Type 'help'.\r\n", argv[0]);
    }
}

/* ================================================================
 * Public API
 * ================================================================ */

void cli_init(cli_ctx_t *ctx, clone_ctx_t *clone_ctx, pmic_i2c_t *i2c)
{
    memset(ctx, 0, sizeof(*ctx));
    ctx->clone_ctx    = clone_ctx;
    ctx->i2c          = i2c;
    ctx->echo_enabled = true;
}

void cli_process_char(cli_ctx_t *ctx, char c)
{
    /* Handle backspace */
    if (c == '\b' || c == 0x7F) {
        if (ctx->cmd_pos > 0) {
            ctx->cmd_pos--;
            if (ctx->echo_enabled)
                platform_printf("\b \b");
        }
        return;
    }

    /* Handle enter */
    if (c == '\r' || c == '\n') {
        if (ctx->echo_enabled)
            platform_printf("\r\n");

        ctx->cmd_buf[ctx->cmd_pos] = '\0';

        if (ctx->cmd_pos > 0) {
            dispatch(ctx, ctx->cmd_buf);
        }

        ctx->cmd_pos = 0;
        cli_print_prompt(ctx);
        return;
    }

    /* Buffer printable characters */
    if (ctx->cmd_pos < CLI_MAX_CMD_LEN - 1 && c >= 0x20) {
        ctx->cmd_buf[ctx->cmd_pos++] = c;
        if (ctx->echo_enabled) {
            char echo[2] = { c, '\0' };
            platform_uart_send(echo);
        }
    }
}

void cli_print_prompt(cli_ctx_t *ctx)
{
    (void)ctx;
    platform_printf(CLI_PROMPT);
}

void cli_print_banner(void)
{
    platform_printf("\r\n");
    platform_printf("========================================\r\n");
    platform_printf("  PMIC Clone Tool for TCON Boards\r\n");
    platform_printf("  I2C / STM32 HAL\r\n");
    platform_printf("========================================\r\n");
    platform_printf("Type 'help' for available commands.\r\n\r\n");
}
