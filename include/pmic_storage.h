/**
 * @file pmic_storage.h
 * @brief Non-volatile storage for PMIC snapshots (STM32 internal flash).
 *
 * Stores up to STORAGE_MAX_SLOTS snapshots in a dedicated flash sector.
 * Each slot holds one complete PMIC register snapshot with integrity checks.
 */

#ifndef PMIC_STORAGE_H
#define PMIC_STORAGE_H

#include "pmic_regmap.h"

/* ---------- Configuration ---------- */

#define STORAGE_MAX_SLOTS       4
#define STORAGE_MAGIC           0x504D4943  /* "PMIC" */

/* ---------- Status ---------- */

typedef enum {
    STORAGE_OK = 0,
    STORAGE_ERR_FLASH,
    STORAGE_ERR_FULL,
    STORAGE_ERR_INVALID_SLOT,
    STORAGE_ERR_EMPTY,
    STORAGE_ERR_CHECKSUM,
    STORAGE_ERR_ERASE,
} storage_status_t;

/* ---------- Slot header ---------- */

typedef struct {
    uint32_t magic;
    uint8_t  slot_index;
    uint8_t  reserved[3];
    uint32_t data_size;
    uint32_t checksum;
} storage_header_t;

/* ---------- API ---------- */

/**
 * @brief Initialize storage subsystem.
 *
 * Scans flash for existing slots and builds an index.
 */
storage_status_t storage_init(void);

/**
 * @brief Save a snapshot to a slot.
 * @param slot  Slot index (0 .. STORAGE_MAX_SLOTS-1).
 * @param snap  Snapshot to save.
 */
storage_status_t storage_save(uint8_t slot, const pmic_snapshot_t *snap);

/**
 * @brief Load a snapshot from a slot.
 * @param slot  Slot index.
 * @param snap  Output snapshot.
 */
storage_status_t storage_load(uint8_t slot, pmic_snapshot_t *snap);

/**
 * @brief Erase a single slot.
 */
storage_status_t storage_erase_slot(uint8_t slot);

/**
 * @brief Erase all slots.
 */
storage_status_t storage_erase_all(void);

/**
 * @brief Check if a slot contains valid data.
 */
bool storage_slot_valid(uint8_t slot);

/**
 * @brief Get a summary string for a slot (name, address, reg count).
 * @param slot  Slot index.
 * @param buf   Output buffer.
 * @param len   Buffer size.
 */
storage_status_t storage_slot_info(uint8_t slot, char *buf, uint16_t len);

/**
 * @brief Convert status code to string.
 */
const char *storage_status_str(storage_status_t status);

#endif /* PMIC_STORAGE_H */
