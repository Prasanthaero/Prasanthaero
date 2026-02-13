/**
 * @file pmic_regmap.h
 * @brief PMIC register map definitions and configuration structures.
 *
 * Supports multiple PMIC types through configurable register descriptors.
 * Each PMIC profile defines which registers to read/write during cloning.
 */

#ifndef PMIC_REGMAP_H
#define PMIC_REGMAP_H

#include <stdint.h>
#include <stdbool.h>

/* ---------- Limits ---------- */

#define PMIC_MAX_REGISTERS      256
#define PMIC_MAX_NAME_LEN       32
#define PMIC_MAX_PROFILES       8

/* ---------- Register access flags ---------- */

typedef enum {
    PMIC_REG_RO    = 0x01,  /* Read-only */
    PMIC_REG_RW    = 0x02,  /* Read-write */
    PMIC_REG_WO    = 0x04,  /* Write-only */
    PMIC_REG_VOL   = 0x08,  /* Volatile (re-read each time) */
    PMIC_REG_SKIP  = 0x10,  /* Skip during clone */
} pmic_reg_flags_t;

/* ---------- Single register descriptor ---------- */

typedef struct {
    uint8_t  addr;           /* Register address */
    uint8_t  default_val;    /* Factory default value */
    uint8_t  mask;           /* Writable bits mask (1 = writable) */
    uint8_t  flags;          /* pmic_reg_flags_t combination */
    char     name[PMIC_MAX_NAME_LEN];  /* Human-readable name */
} pmic_reg_desc_t;

/* ---------- PMIC profile ---------- */

typedef struct {
    char             name[PMIC_MAX_NAME_LEN];  /* e.g. "TPS65185" */
    uint8_t          i2c_addr;                  /* Default 7-bit address */
    uint16_t         reg_count;                 /* Number of registers */
    pmic_reg_desc_t  regs[PMIC_MAX_REGISTERS];  /* Register descriptors */
} pmic_profile_t;

/* ---------- Register snapshot (captured data) ---------- */

typedef struct {
    uint8_t  addr;
    uint8_t  value;
    bool     valid;     /* Was this register successfully read? */
} pmic_reg_entry_t;

typedef struct {
    char              profile_name[PMIC_MAX_NAME_LEN];
    uint8_t           i2c_addr;
    uint16_t          entry_count;
    pmic_reg_entry_t  entries[PMIC_MAX_REGISTERS];
    uint32_t          checksum;   /* CRC32 of entries for integrity */
} pmic_snapshot_t;

/* ---------- API ---------- */

/**
 * @brief Get built-in profile by index.
 */
const pmic_profile_t *pmic_profile_get(uint8_t index);

/**
 * @brief Get number of built-in profiles.
 */
uint8_t pmic_profile_count(void);

/**
 * @brief Find profile by name (case-insensitive).
 * @return Profile pointer or NULL if not found.
 */
const pmic_profile_t *pmic_profile_find(const char *name);

/**
 * @brief Create a generic profile for an unknown PMIC.
 *
 * Generates a profile with sequential registers 0x00..num_regs-1,
 * all marked as RW with mask 0xFF.
 *
 * @param out        Profile to fill.
 * @param name       Profile name.
 * @param i2c_addr   7-bit I2C address.
 * @param num_regs   Number of registers.
 */
void pmic_profile_create_generic(pmic_profile_t *out, const char *name,
                                  uint8_t i2c_addr, uint16_t num_regs);

/**
 * @brief Compute CRC32 checksum for a snapshot.
 */
uint32_t pmic_snapshot_checksum(const pmic_snapshot_t *snap);

#endif /* PMIC_REGMAP_H */
