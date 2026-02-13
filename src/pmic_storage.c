/**
 * @file pmic_storage.c
 * @brief Non-volatile flash storage for PMIC snapshots.
 *
 * Implements slot-based storage in a dedicated flash sector.
 * On STM32, uses HAL flash driver. On host builds, uses a RAM buffer.
 */

#include "pmic_storage.h"
#include "pmic_platform.h"
#include <string.h>

/* ================================================================
 * Internal state
 * ================================================================ */

typedef struct {
    storage_header_t header;
    pmic_snapshot_t  snapshot;
} storage_slot_t;

#ifdef HOST_BUILD
/* RAM-based storage for host/test builds */
static storage_slot_t s_slots[STORAGE_MAX_SLOTS];
static bool s_slot_occupied[STORAGE_MAX_SLOTS];
#else
/* Flash-based storage: read from flash addresses at runtime */
#define SLOT_SIZE       (sizeof(storage_slot_t))
#define SLOT_ADDR(n)    (STORAGE_FLASH_BASE_ADDR + (SLOT_SIZE * (n)))
#endif

static bool s_initialized = false;

/* ================================================================
 * Flash helpers (STM32 HAL)
 * ================================================================ */

#ifdef STM32_HAL
static storage_status_t flash_erase_sector(void)
{
    HAL_FLASH_Unlock();

    FLASH_EraseInitTypeDef erase_cfg;
    uint32_t sector_error = 0;

    erase_cfg.TypeErase    = FLASH_TYPEERASE_SECTORS;
    erase_cfg.Sector       = STORAGE_FLASH_SECTOR;
    erase_cfg.NbSectors    = 1;
    erase_cfg.VoltageRange = STORAGE_FLASH_VOLTAGE;

    HAL_StatusTypeDef rc = HAL_FLASHEx_Erase(&erase_cfg, &sector_error);
    HAL_FLASH_Lock();

    return (rc == HAL_OK) ? STORAGE_OK : STORAGE_ERR_ERASE;
}

static storage_status_t flash_write_slot(uint8_t slot, const storage_slot_t *data)
{
    HAL_FLASH_Unlock();

    uint32_t dest = SLOT_ADDR(slot);
    const uint8_t *src = (const uint8_t *)data;
    storage_status_t status = STORAGE_OK;

    for (uint32_t i = 0; i < sizeof(storage_slot_t); i++) {
        if (HAL_FLASH_Program(FLASH_TYPEPROGRAM_BYTE, dest + i, src[i])
            != HAL_OK) {
            status = STORAGE_ERR_FLASH;
            break;
        }
    }

    HAL_FLASH_Lock();
    return status;
}

static void flash_read_slot(uint8_t slot, storage_slot_t *out)
{
    uint32_t src = SLOT_ADDR(slot);
    memcpy(out, (const void *)src, sizeof(storage_slot_t));
}
#endif /* STM32_HAL */

/* ================================================================
 * Public API
 * ================================================================ */

storage_status_t storage_init(void)
{
#ifdef HOST_BUILD
    memset(s_slots, 0, sizeof(s_slots));
    memset(s_slot_occupied, 0, sizeof(s_slot_occupied));
#endif
    s_initialized = true;
    return STORAGE_OK;
}

/* ---------------------------------------------------------------- */

storage_status_t storage_save(uint8_t slot, const pmic_snapshot_t *snap)
{
    if (!s_initialized) return STORAGE_ERR_FLASH;
    if (slot >= STORAGE_MAX_SLOTS) return STORAGE_ERR_INVALID_SLOT;
    if (!snap) return STORAGE_ERR_FLASH;

    storage_slot_t data;
    memset(&data, 0, sizeof(data));
    data.header.magic      = STORAGE_MAGIC;
    data.header.slot_index = slot;
    data.header.data_size  = sizeof(pmic_snapshot_t);
    memcpy(&data.snapshot, snap, sizeof(pmic_snapshot_t));
    data.header.checksum   = pmic_snapshot_checksum(&data.snapshot);

#ifdef HOST_BUILD
    memcpy(&s_slots[slot], &data, sizeof(data));
    s_slot_occupied[slot] = true;
    return STORAGE_OK;
#elif defined(STM32_HAL)
    /* For simplicity, erase entire sector and rewrite all slots.
     * A production version would use wear-leveling. */
    storage_slot_t backup[STORAGE_MAX_SLOTS];
    for (uint8_t i = 0; i < STORAGE_MAX_SLOTS; i++) {
        flash_read_slot(i, &backup[i]);
    }
    memcpy(&backup[slot], &data, sizeof(data));

    storage_status_t rc = flash_erase_sector();
    if (rc != STORAGE_OK) return rc;

    for (uint8_t i = 0; i < STORAGE_MAX_SLOTS; i++) {
        if (backup[i].header.magic == STORAGE_MAGIC) {
            rc = flash_write_slot(i, &backup[i]);
            if (rc != STORAGE_OK) return rc;
        }
    }
    return STORAGE_OK;
#else
    (void)data;
    return STORAGE_ERR_FLASH;
#endif
}

/* ---------------------------------------------------------------- */

storage_status_t storage_load(uint8_t slot, pmic_snapshot_t *snap)
{
    if (!s_initialized) return STORAGE_ERR_FLASH;
    if (slot >= STORAGE_MAX_SLOTS) return STORAGE_ERR_INVALID_SLOT;
    if (!snap) return STORAGE_ERR_FLASH;

#ifdef HOST_BUILD
    if (!s_slot_occupied[slot]) return STORAGE_ERR_EMPTY;
    memcpy(snap, &s_slots[slot].snapshot, sizeof(pmic_snapshot_t));
#elif defined(STM32_HAL)
    storage_slot_t data;
    flash_read_slot(slot, &data);
    if (data.header.magic != STORAGE_MAGIC) return STORAGE_ERR_EMPTY;

    uint32_t calc = pmic_snapshot_checksum(&data.snapshot);
    if (calc != data.header.checksum) return STORAGE_ERR_CHECKSUM;

    memcpy(snap, &data.snapshot, sizeof(pmic_snapshot_t));
#else
    return STORAGE_ERR_FLASH;
#endif

    return STORAGE_OK;
}

/* ---------------------------------------------------------------- */

storage_status_t storage_erase_slot(uint8_t slot)
{
    if (slot >= STORAGE_MAX_SLOTS) return STORAGE_ERR_INVALID_SLOT;

#ifdef HOST_BUILD
    memset(&s_slots[slot], 0, sizeof(s_slots[slot]));
    s_slot_occupied[slot] = false;
    return STORAGE_OK;
#elif defined(STM32_HAL)
    /* Read all, clear target, erase, rewrite remaining */
    storage_slot_t backup[STORAGE_MAX_SLOTS];
    for (uint8_t i = 0; i < STORAGE_MAX_SLOTS; i++)
        flash_read_slot(i, &backup[i]);

    memset(&backup[slot], 0xFF, sizeof(storage_slot_t));

    storage_status_t rc = flash_erase_sector();
    if (rc != STORAGE_OK) return rc;

    for (uint8_t i = 0; i < STORAGE_MAX_SLOTS; i++) {
        if (backup[i].header.magic == STORAGE_MAGIC) {
            rc = flash_write_slot(i, &backup[i]);
            if (rc != STORAGE_OK) return rc;
        }
    }
    return STORAGE_OK;
#else
    return STORAGE_ERR_FLASH;
#endif
}

/* ---------------------------------------------------------------- */

storage_status_t storage_erase_all(void)
{
#ifdef HOST_BUILD
    memset(s_slots, 0, sizeof(s_slots));
    memset(s_slot_occupied, 0, sizeof(s_slot_occupied));
    return STORAGE_OK;
#elif defined(STM32_HAL)
    return flash_erase_sector();
#else
    return STORAGE_ERR_FLASH;
#endif
}

/* ---------------------------------------------------------------- */

bool storage_slot_valid(uint8_t slot)
{
    if (slot >= STORAGE_MAX_SLOTS) return false;

#ifdef HOST_BUILD
    return s_slot_occupied[slot];
#elif defined(STM32_HAL)
    storage_slot_t data;
    flash_read_slot(slot, &data);
    return (data.header.magic == STORAGE_MAGIC);
#else
    return false;
#endif
}

/* ---------------------------------------------------------------- */

storage_status_t storage_slot_info(uint8_t slot, char *buf, uint16_t len)
{
    if (slot >= STORAGE_MAX_SLOTS) return STORAGE_ERR_INVALID_SLOT;
    if (!buf || len == 0) return STORAGE_ERR_FLASH;

    pmic_snapshot_t snap;
    storage_status_t rc = storage_load(slot, &snap);

    if (rc == STORAGE_ERR_EMPTY) {
        snprintf(buf, len, "Slot %u: [empty]", slot);
        return STORAGE_OK;
    }
    if (rc != STORAGE_OK) {
        snprintf(buf, len, "Slot %u: [error: %s]", slot,
                 storage_status_str(rc));
        return rc;
    }

    snprintf(buf, len, "Slot %u: %s @ 0x%02X (%u regs, CRC 0x%08lX)",
             slot, snap.profile_name, snap.i2c_addr,
             snap.entry_count, (unsigned long)snap.checksum);
    return STORAGE_OK;
}

/* ---------------------------------------------------------------- */

const char *storage_status_str(storage_status_t status)
{
    switch (status) {
    case STORAGE_OK:             return "OK";
    case STORAGE_ERR_FLASH:      return "Flash error";
    case STORAGE_ERR_FULL:       return "Storage full";
    case STORAGE_ERR_INVALID_SLOT: return "Invalid slot";
    case STORAGE_ERR_EMPTY:      return "Slot empty";
    case STORAGE_ERR_CHECKSUM:   return "Checksum error";
    case STORAGE_ERR_ERASE:      return "Erase failed";
    default:                     return "Unknown";
    }
}
