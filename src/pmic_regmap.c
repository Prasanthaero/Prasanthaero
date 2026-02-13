/**
 * @file pmic_regmap.c
 * @brief PMIC register map profiles and snapshot utilities.
 */

#include "pmic_regmap.h"
#include "pmic_platform.h"
#include <string.h>
#include <ctype.h>

/* ================================================================
 * Built-in PMIC profiles
 *
 * Add your own PMIC profiles here. Each profile lists the registers
 * that should be read/written during a clone operation.
 * ================================================================ */

/* --- TPS65185 (TI e-paper PMIC) --- */
static const pmic_profile_t profile_tps65185 = {
    .name     = "TPS65185",
    .i2c_addr = 0x68,
    .reg_count = 16,
    .regs = {
        { 0x00, 0x00, 0xFF, PMIC_REG_RO,  "TMST_VALUE"  },
        { 0x01, 0x19, 0xFF, PMIC_REG_RW,  "ENABLE"      },
        { 0x02, 0x00, 0xFF, PMIC_REG_RW,  "VADJ"        },
        { 0x03, 0x78, 0xFF, PMIC_REG_RW,  "VCOM1"       },
        { 0x04, 0x00, 0x01, PMIC_REG_RW,  "VCOM2"       },
        { 0x05, 0x00, 0xFF, PMIC_REG_RW,  "INT_EN1"     },
        { 0x06, 0x00, 0xFF, PMIC_REG_RW,  "INT_EN2"     },
        { 0x07, 0x00, 0xFF, PMIC_REG_RO,  "INT1"        },
        { 0x08, 0x00, 0xFF, PMIC_REG_RO,  "INT2"        },
        { 0x09, 0x06, 0xFF, PMIC_REG_RW,  "UPSEQ0"      },
        { 0x0A, 0x00, 0xFF, PMIC_REG_RW,  "UPSEQ1"      },
        { 0x0B, 0x00, 0xFF, PMIC_REG_RW,  "DWNSEQ0"     },
        { 0x0C, 0x00, 0xFF, PMIC_REG_RW,  "DWNSEQ1"     },
        { 0x0D, 0x00, 0xFF, PMIC_REG_RW,  "TMST1"       },
        { 0x0E, 0x00, 0xFF, PMIC_REG_RW,  "TMST2"       },
        { 0x0F, 0x65, 0x00, PMIC_REG_RO,  "REVID"       },
    },
};

/* --- TPS65186 (variant) --- */
static const pmic_profile_t profile_tps65186 = {
    .name     = "TPS65186",
    .i2c_addr = 0x68,
    .reg_count = 16,
    .regs = {
        { 0x00, 0x00, 0xFF, PMIC_REG_RO,  "TMST_VALUE"  },
        { 0x01, 0x19, 0xFF, PMIC_REG_RW,  "ENABLE"      },
        { 0x02, 0x00, 0xFF, PMIC_REG_RW,  "VADJ"        },
        { 0x03, 0x78, 0xFF, PMIC_REG_RW,  "VCOM1"       },
        { 0x04, 0x00, 0x01, PMIC_REG_RW,  "VCOM2"       },
        { 0x05, 0x00, 0xFF, PMIC_REG_RW,  "INT_EN1"     },
        { 0x06, 0x00, 0xFF, PMIC_REG_RW,  "INT_EN2"     },
        { 0x07, 0x00, 0xFF, PMIC_REG_RO,  "INT1"        },
        { 0x08, 0x00, 0xFF, PMIC_REG_RO,  "INT2"        },
        { 0x09, 0x06, 0xFF, PMIC_REG_RW,  "UPSEQ0"      },
        { 0x0A, 0x00, 0xFF, PMIC_REG_RW,  "UPSEQ1"      },
        { 0x0B, 0x00, 0xFF, PMIC_REG_RW,  "DWNSEQ0"     },
        { 0x0C, 0x00, 0xFF, PMIC_REG_RW,  "DWNSEQ1"     },
        { 0x0D, 0x00, 0xFF, PMIC_REG_RW,  "TMST1"       },
        { 0x0E, 0x00, 0xFF, PMIC_REG_RW,  "TMST2"       },
        { 0x0F, 0x66, 0x00, PMIC_REG_RO,  "REVID"       },
    },
};

/* --- MAX20998 (Maxim display PMIC) --- */
static const pmic_profile_t profile_max20998 = {
    .name     = "MAX20998",
    .i2c_addr = 0x31,
    .reg_count = 12,
    .regs = {
        { 0x00, 0x00, 0x00, PMIC_REG_RO,  "CHIP_ID"     },
        { 0x01, 0x00, 0x00, PMIC_REG_RO,  "CHIP_REV"    },
        { 0x02, 0x00, 0xFF, PMIC_REG_RW,  "STATUS"      },
        { 0x03, 0x00, 0xFF, PMIC_REG_RW,  "INT_MASK"    },
        { 0x10, 0x00, 0xFF, PMIC_REG_RW,  "VPOS_CTRL"   },
        { 0x11, 0x00, 0xFF, PMIC_REG_RW,  "VNEG_CTRL"   },
        { 0x12, 0x00, 0xFF, PMIC_REG_RW,  "VCOM_CTRL"   },
        { 0x13, 0x00, 0xFF, PMIC_REG_RW,  "VCOM_DAC_L"  },
        { 0x14, 0x00, 0x03, PMIC_REG_RW,  "VCOM_DAC_H"  },
        { 0x20, 0x00, 0xFF, PMIC_REG_RW,  "SEQ_CTRL"    },
        { 0x21, 0x00, 0xFF, PMIC_REG_RW,  "TIMING1"     },
        { 0x22, 0x00, 0xFF, PMIC_REG_RW,  "TIMING2"     },
    },
};

/* --- Profile table --- */
static const pmic_profile_t *s_profiles[] = {
    &profile_tps65185,
    &profile_tps65186,
    &profile_max20998,
};

#define NUM_PROFILES (sizeof(s_profiles) / sizeof(s_profiles[0]))

/* ================================================================ */

const pmic_profile_t *pmic_profile_get(uint8_t index)
{
    if (index >= NUM_PROFILES)
        return NULL;
    return s_profiles[index];
}

uint8_t pmic_profile_count(void)
{
    return (uint8_t)NUM_PROFILES;
}

const pmic_profile_t *pmic_profile_find(const char *name)
{
    if (!name) return NULL;

    for (uint8_t i = 0; i < NUM_PROFILES; i++) {
        const char *a = s_profiles[i]->name;
        const char *b = name;
        bool match = true;

        while (*a && *b) {
            if (tolower((unsigned char)*a) != tolower((unsigned char)*b)) {
                match = false;
                break;
            }
            a++;
            b++;
        }
        if (match && *a == '\0' && *b == '\0')
            return s_profiles[i];
    }
    return NULL;
}

/* ================================================================ */

void pmic_profile_create_generic(pmic_profile_t *out, const char *name,
                                  uint8_t i2c_addr, uint16_t num_regs)
{
    if (!out) return;

    memset(out, 0, sizeof(*out));
    strncpy(out->name, name ? name : "GENERIC", PMIC_MAX_NAME_LEN - 1);
    out->i2c_addr = i2c_addr;

    if (num_regs > PMIC_MAX_REGISTERS)
        num_regs = PMIC_MAX_REGISTERS;

    out->reg_count = num_regs;
    for (uint16_t i = 0; i < num_regs; i++) {
        out->regs[i].addr        = (uint8_t)i;
        out->regs[i].default_val = 0x00;
        out->regs[i].mask        = 0xFF;
        out->regs[i].flags       = PMIC_REG_RW;
        snprintf(out->regs[i].name, PMIC_MAX_NAME_LEN, "REG_0x%02X",
                 (unsigned)i);
    }
}

/* ================================================================
 * CRC32 for snapshot integrity
 * ================================================================ */

static uint32_t crc32_byte(uint32_t crc, uint8_t byte)
{
    crc ^= byte;
    for (int i = 0; i < 8; i++) {
        if (crc & 1)
            crc = (crc >> 1) ^ 0xEDB88320;
        else
            crc >>= 1;
    }
    return crc;
}

uint32_t pmic_snapshot_checksum(const pmic_snapshot_t *snap)
{
    if (!snap) return 0;

    uint32_t crc = 0xFFFFFFFF;

    /* Hash the entries */
    for (uint16_t i = 0; i < snap->entry_count; i++) {
        crc = crc32_byte(crc, snap->entries[i].addr);
        crc = crc32_byte(crc, snap->entries[i].value);
        crc = crc32_byte(crc, snap->entries[i].valid ? 1 : 0);
    }

    return crc ^ 0xFFFFFFFF;
}
