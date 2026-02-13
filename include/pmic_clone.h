/**
 * @file pmic_clone.h
 * @brief Clone engine — reads, stores, writes, and compares PMIC configurations.
 *
 * Core logic for the PMIC clone tool. Orchestrates I2C operations against
 * a register map profile to capture and program TCON board PMICs.
 */

#ifndef PMIC_CLONE_H
#define PMIC_CLONE_H

#include "pmic_i2c.h"
#include "pmic_regmap.h"

/* ---------- Status ---------- */

typedef enum {
    CLONE_OK = 0,
    CLONE_ERR_I2C,
    CLONE_ERR_VERIFY,
    CLONE_ERR_PROFILE,
    CLONE_ERR_STORAGE,
    CLONE_ERR_NO_SNAPSHOT,
    CLONE_ERR_CHECKSUM,
} clone_status_t;

/* ---------- Progress callback ---------- */

/**
 * @brief Called during long operations to report progress.
 * @param current  Current register index being processed.
 * @param total    Total number of registers.
 * @param reg_addr Address of the register being processed.
 */
typedef void (*clone_progress_cb)(uint16_t current, uint16_t total,
                                   uint8_t reg_addr);

/* ---------- Clone context ---------- */

typedef struct {
    pmic_i2c_t          *i2c;
    const pmic_profile_t *profile;
    pmic_snapshot_t       snapshot;
    bool                  snapshot_valid;
    clone_progress_cb     progress_cb;
} clone_ctx_t;

/* ---------- API ---------- */

/**
 * @brief Initialize clone context.
 */
clone_status_t clone_init(clone_ctx_t *ctx, pmic_i2c_t *i2c,
                           const pmic_profile_t *profile);

/**
 * @brief Read all registers from the source PMIC into the snapshot.
 * @param ctx         Clone context.
 * @param i2c_addr    I2C address override (0 = use profile default).
 */
clone_status_t clone_read_source(clone_ctx_t *ctx, uint8_t i2c_addr);

/**
 * @brief Write the stored snapshot to a target PMIC.
 *
 * Only writes registers marked RW in the profile. Respects bit masks.
 *
 * @param ctx         Clone context.
 * @param i2c_addr    I2C address override (0 = use profile default).
 */
clone_status_t clone_write_target(clone_ctx_t *ctx, uint8_t i2c_addr);

/**
 * @brief Verify target PMIC registers match the snapshot.
 *
 * Reads back each register and compares against stored values
 * (masked to writable bits).
 *
 * @param ctx         Clone context.
 * @param i2c_addr    I2C address override (0 = use profile default).
 * @param mismatches  Outputs the number of mismatched registers.
 */
clone_status_t clone_verify(clone_ctx_t *ctx, uint8_t i2c_addr,
                             uint16_t *mismatches);

/**
 * @brief Compare two snapshots and report differences.
 *
 * @param a           First snapshot.
 * @param b           Second snapshot.
 * @param diff_count  Number of differing registers.
 */
clone_status_t clone_diff(const pmic_snapshot_t *a, const pmic_snapshot_t *b,
                           uint16_t *diff_count);

/**
 * @brief Read a single register and update the snapshot entry.
 */
clone_status_t clone_read_single(clone_ctx_t *ctx, uint8_t i2c_addr,
                                  uint8_t reg_addr, uint8_t *value);

/**
 * @brief Write a single register value (with mask) and update snapshot.
 */
clone_status_t clone_write_single(clone_ctx_t *ctx, uint8_t i2c_addr,
                                   uint8_t reg_addr, uint8_t value);

/**
 * @brief Get the current snapshot (read-only).
 */
const pmic_snapshot_t *clone_get_snapshot(const clone_ctx_t *ctx);

/**
 * @brief Load an external snapshot into the context.
 */
clone_status_t clone_load_snapshot(clone_ctx_t *ctx,
                                    const pmic_snapshot_t *snap);

/**
 * @brief Convert status code to string.
 */
const char *clone_status_str(clone_status_t status);

#endif /* PMIC_CLONE_H */
