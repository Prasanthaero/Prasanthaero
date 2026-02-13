/**
 * @file pmic_clone.c
 * @brief Clone engine — reads, stores, writes, and compares PMIC registers.
 */

#include "pmic_clone.h"
#include "pmic_platform.h"
#include <string.h>

/* ================================================================ */

clone_status_t clone_init(clone_ctx_t *ctx, pmic_i2c_t *i2c,
                           const pmic_profile_t *profile)
{
    if (!ctx || !i2c || !profile)
        return CLONE_ERR_PROFILE;

    memset(ctx, 0, sizeof(*ctx));
    ctx->i2c     = i2c;
    ctx->profile = profile;
    ctx->snapshot_valid = false;
    ctx->progress_cb    = NULL;

    return CLONE_OK;
}

/* ================================================================ */

clone_status_t clone_read_source(clone_ctx_t *ctx, uint8_t i2c_addr)
{
    if (!ctx || !ctx->profile)
        return CLONE_ERR_PROFILE;

    uint8_t addr = i2c_addr ? i2c_addr : ctx->profile->i2c_addr;
    const pmic_profile_t *prof = ctx->profile;
    pmic_snapshot_t *snap = &ctx->snapshot;

    memset(snap, 0, sizeof(*snap));
    strncpy(snap->profile_name, prof->name, PMIC_MAX_NAME_LEN - 1);
    snap->i2c_addr    = addr;
    snap->entry_count = prof->reg_count;

    platform_printf("Reading %u registers from 0x%02X (%s)...\r\n",
                    prof->reg_count, addr, prof->name);

    uint16_t errors = 0;

    for (uint16_t i = 0; i < prof->reg_count; i++) {
        const pmic_reg_desc_t *rd = &prof->regs[i];

        if (rd->flags & PMIC_REG_SKIP) {
            snap->entries[i].addr  = rd->addr;
            snap->entries[i].value = 0;
            snap->entries[i].valid = false;
            continue;
        }

        if (rd->flags & PMIC_REG_WO) {
            /* Write-only registers can't be read — use default */
            snap->entries[i].addr  = rd->addr;
            snap->entries[i].value = rd->default_val;
            snap->entries[i].valid = true;
            continue;
        }

        uint8_t val = 0;
        pmic_i2c_status_t rc = pmic_i2c_read_reg(ctx->i2c, addr,
                                                   rd->addr, &val);

        snap->entries[i].addr  = rd->addr;
        snap->entries[i].value = val;
        snap->entries[i].valid = (rc == PMIC_I2C_OK);

        if (rc != PMIC_I2C_OK) {
            platform_printf("  WARN: reg 0x%02X (%s) read failed: %s\r\n",
                            rd->addr, rd->name, pmic_i2c_status_str(rc));
            errors++;
        }

        if (ctx->progress_cb)
            ctx->progress_cb(i + 1, prof->reg_count, rd->addr);
    }

    snap->checksum = pmic_snapshot_checksum(snap);
    ctx->snapshot_valid = true;

    platform_printf("Read complete. %u/%u registers OK, %u errors.\r\n",
                    prof->reg_count - errors, prof->reg_count, errors);

    return (errors > 0) ? CLONE_ERR_I2C : CLONE_OK;
}

/* ================================================================ */

clone_status_t clone_write_target(clone_ctx_t *ctx, uint8_t i2c_addr)
{
    if (!ctx || !ctx->profile)
        return CLONE_ERR_PROFILE;
    if (!ctx->snapshot_valid)
        return CLONE_ERR_NO_SNAPSHOT;

    uint8_t addr = i2c_addr ? i2c_addr : ctx->profile->i2c_addr;
    const pmic_profile_t *prof = ctx->profile;
    const pmic_snapshot_t *snap = &ctx->snapshot;

    platform_printf("Writing %u registers to 0x%02X (%s)...\r\n",
                    prof->reg_count, addr, prof->name);

    uint16_t written = 0, skipped = 0, errors = 0;

    for (uint16_t i = 0; i < prof->reg_count; i++) {
        const pmic_reg_desc_t *rd = &prof->regs[i];
        const pmic_reg_entry_t *entry = &snap->entries[i];

        /* Only write RW registers that have valid data */
        if (!(rd->flags & PMIC_REG_RW) || !entry->valid) {
            skipped++;
            continue;
        }

        /* Apply mask: read current value, merge, then write */
        uint8_t write_val = entry->value & rd->mask;

        if (rd->mask != 0xFF) {
            /* Partial register write — read-modify-write */
            uint8_t current = 0;
            pmic_i2c_status_t rc = pmic_i2c_read_reg(ctx->i2c, addr,
                                                       rd->addr, &current);
            if (rc == PMIC_I2C_OK) {
                write_val = (current & ~rd->mask) | (entry->value & rd->mask);
            }
        }

        pmic_i2c_status_t rc = pmic_i2c_write_reg(ctx->i2c, addr,
                                                    rd->addr, write_val);
        if (rc != PMIC_I2C_OK) {
            platform_printf("  ERR: reg 0x%02X (%s) write failed: %s\r\n",
                            rd->addr, rd->name, pmic_i2c_status_str(rc));
            errors++;
        } else {
            written++;
        }

        /* Small delay between writes for PMIC stability */
        platform_delay_ms(2);

        if (ctx->progress_cb)
            ctx->progress_cb(i + 1, prof->reg_count, rd->addr);
    }

    platform_printf("Write complete. %u written, %u skipped, %u errors.\r\n",
                    written, skipped, errors);

    return (errors > 0) ? CLONE_ERR_I2C : CLONE_OK;
}

/* ================================================================ */

clone_status_t clone_verify(clone_ctx_t *ctx, uint8_t i2c_addr,
                             uint16_t *mismatches)
{
    if (!ctx || !ctx->profile)
        return CLONE_ERR_PROFILE;
    if (!ctx->snapshot_valid)
        return CLONE_ERR_NO_SNAPSHOT;
    if (!mismatches)
        return CLONE_ERR_PROFILE;

    uint8_t addr = i2c_addr ? i2c_addr : ctx->profile->i2c_addr;
    const pmic_profile_t *prof = ctx->profile;
    const pmic_snapshot_t *snap = &ctx->snapshot;

    *mismatches = 0;

    platform_printf("Verifying %u registers on 0x%02X...\r\n",
                    prof->reg_count, addr);

    for (uint16_t i = 0; i < prof->reg_count; i++) {
        const pmic_reg_desc_t *rd = &prof->regs[i];
        const pmic_reg_entry_t *entry = &snap->entries[i];

        /* Only verify RW registers that we wrote */
        if (!(rd->flags & PMIC_REG_RW) || !entry->valid)
            continue;

        uint8_t readback = 0;
        pmic_i2c_status_t rc = pmic_i2c_read_reg(ctx->i2c, addr,
                                                   rd->addr, &readback);
        if (rc != PMIC_I2C_OK) {
            platform_printf("  ERR: reg 0x%02X (%s) verify read failed\r\n",
                            rd->addr, rd->name);
            (*mismatches)++;
            continue;
        }

        uint8_t expected = entry->value & rd->mask;
        uint8_t actual   = readback & rd->mask;

        if (expected != actual) {
            platform_printf("  MISMATCH: reg 0x%02X (%s) "
                            "expected 0x%02X, got 0x%02X\r\n",
                            rd->addr, rd->name, expected, actual);
            (*mismatches)++;
        }

        if (ctx->progress_cb)
            ctx->progress_cb(i + 1, prof->reg_count, rd->addr);
    }

    platform_printf("Verify complete. %u mismatches.\r\n", *mismatches);

    return (*mismatches > 0) ? CLONE_ERR_VERIFY : CLONE_OK;
}

/* ================================================================ */

clone_status_t clone_diff(const pmic_snapshot_t *a, const pmic_snapshot_t *b,
                           uint16_t *diff_count)
{
    if (!a || !b || !diff_count)
        return CLONE_ERR_NO_SNAPSHOT;

    *diff_count = 0;

    uint16_t count = (a->entry_count < b->entry_count)
                     ? a->entry_count : b->entry_count;

    for (uint16_t i = 0; i < count; i++) {
        if (!a->entries[i].valid || !b->entries[i].valid)
            continue;

        if (a->entries[i].value != b->entries[i].value) {
            platform_printf("  DIFF reg 0x%02X: 0x%02X vs 0x%02X\r\n",
                            a->entries[i].addr,
                            a->entries[i].value,
                            b->entries[i].value);
            (*diff_count)++;
        }
    }

    return CLONE_OK;
}

/* ================================================================ */

clone_status_t clone_read_single(clone_ctx_t *ctx, uint8_t i2c_addr,
                                  uint8_t reg_addr, uint8_t *value)
{
    if (!ctx || !value)
        return CLONE_ERR_PROFILE;

    uint8_t addr = i2c_addr ? i2c_addr : ctx->profile->i2c_addr;
    pmic_i2c_status_t rc = pmic_i2c_read_reg(ctx->i2c, addr, reg_addr, value);

    if (rc != PMIC_I2C_OK)
        return CLONE_ERR_I2C;

    /* Update snapshot if this register is in the profile */
    if (ctx->snapshot_valid) {
        for (uint16_t i = 0; i < ctx->snapshot.entry_count; i++) {
            if (ctx->snapshot.entries[i].addr == reg_addr) {
                ctx->snapshot.entries[i].value = *value;
                ctx->snapshot.entries[i].valid = true;
                break;
            }
        }
    }

    return CLONE_OK;
}

/* ================================================================ */

clone_status_t clone_write_single(clone_ctx_t *ctx, uint8_t i2c_addr,
                                   uint8_t reg_addr, uint8_t value)
{
    if (!ctx)
        return CLONE_ERR_PROFILE;

    uint8_t addr = i2c_addr ? i2c_addr : ctx->profile->i2c_addr;

    /* Find register in profile for mask */
    uint8_t mask = 0xFF;
    for (uint16_t i = 0; i < ctx->profile->reg_count; i++) {
        if (ctx->profile->regs[i].addr == reg_addr) {
            mask = ctx->profile->regs[i].mask;
            break;
        }
    }

    uint8_t write_val = value;
    if (mask != 0xFF) {
        uint8_t current = 0;
        pmic_i2c_read_reg(ctx->i2c, addr, reg_addr, &current);
        write_val = (current & ~mask) | (value & mask);
    }

    pmic_i2c_status_t rc = pmic_i2c_write_reg(ctx->i2c, addr,
                                                reg_addr, write_val);
    if (rc != PMIC_I2C_OK)
        return CLONE_ERR_I2C;

    /* Update snapshot */
    if (ctx->snapshot_valid) {
        for (uint16_t i = 0; i < ctx->snapshot.entry_count; i++) {
            if (ctx->snapshot.entries[i].addr == reg_addr) {
                ctx->snapshot.entries[i].value = write_val;
                ctx->snapshot.entries[i].valid = true;
                break;
            }
        }
    }

    return CLONE_OK;
}

/* ================================================================ */

const pmic_snapshot_t *clone_get_snapshot(const clone_ctx_t *ctx)
{
    if (!ctx || !ctx->snapshot_valid)
        return NULL;
    return &ctx->snapshot;
}

clone_status_t clone_load_snapshot(clone_ctx_t *ctx,
                                    const pmic_snapshot_t *snap)
{
    if (!ctx || !snap)
        return CLONE_ERR_NO_SNAPSHOT;

    /* Verify checksum */
    uint32_t calc = pmic_snapshot_checksum(snap);
    if (calc != snap->checksum)
        return CLONE_ERR_CHECKSUM;

    memcpy(&ctx->snapshot, snap, sizeof(pmic_snapshot_t));
    ctx->snapshot_valid = true;
    return CLONE_OK;
}

/* ================================================================ */

const char *clone_status_str(clone_status_t status)
{
    switch (status) {
    case CLONE_OK:              return "OK";
    case CLONE_ERR_I2C:         return "I2C error";
    case CLONE_ERR_VERIFY:      return "Verification failed";
    case CLONE_ERR_PROFILE:     return "Profile error";
    case CLONE_ERR_STORAGE:     return "Storage error";
    case CLONE_ERR_NO_SNAPSHOT: return "No snapshot loaded";
    case CLONE_ERR_CHECKSUM:    return "Checksum mismatch";
    default:                    return "Unknown";
    }
}
