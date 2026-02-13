# ============================================================
# PMIC Clone Tool — Makefile
# ============================================================
# This Makefile is structured for cross-compilation with
# arm-none-eabi-gcc targeting STM32. Adjust paths and flags
# for your specific STM32 family and toolchain.
#
# For native (host) compilation/testing, run:
#   make HOST=1
# ============================================================

# --- Toolchain ---
ifdef HOST
  CC      = gcc
  CFLAGS  = -Wall -Wextra -std=c11 -g -O0
  CFLAGS += -DHOST_BUILD
  LDFLAGS =
else
  PREFIX  = arm-none-eabi-
  CC      = $(PREFIX)gcc
  OBJCOPY = $(PREFIX)objcopy
  SIZE    = $(PREFIX)size

  # --- STM32 family (choose one) ---
  STM32_FAMILY ?= STM32F4
  CPU          ?= cortex-m4
  FPU          ?= -mfpu=fpv4-sp-d16 -mfloat-abi=hard

  CFLAGS  = -Wall -Wextra -std=c11 -Os -g
  CFLAGS += -mcpu=$(CPU) -mthumb $(FPU)
  CFLAGS += -D$(STM32_FAMILY) -DSTM32_HAL -DUSE_HAL_DRIVER
  CFLAGS += -ffunction-sections -fdata-sections

  LDFLAGS  = -mcpu=$(CPU) -mthumb $(FPU)
  LDFLAGS += -Wl,--gc-sections
  LDFLAGS += -specs=nosys.specs -specs=nano.specs
  # LDFLAGS += -T path/to/your_linker_script.ld
endif

# --- Paths ---
SRC_DIR   = src
INC_DIR   = include
BUILD_DIR = build
CFG_DIR   = config

CFLAGS += -I$(INC_DIR) -I$(CFG_DIR)

# --- Sources ---
SRCS = $(wildcard $(SRC_DIR)/*.c)
OBJS = $(patsubst $(SRC_DIR)/%.c,$(BUILD_DIR)/%.o,$(SRCS))

# --- Target ---
ifdef HOST
  TARGET = $(BUILD_DIR)/pmic_clone_tool
else
  TARGET = $(BUILD_DIR)/pmic_clone_tool.elf
  HEX    = $(BUILD_DIR)/pmic_clone_tool.hex
  BIN    = $(BUILD_DIR)/pmic_clone_tool.bin
endif

# ============================================================
# Rules
# ============================================================

.PHONY: all clean flash size host

all: $(TARGET)
ifdef HOST
	@echo "Host build complete: $(TARGET)"
else
	$(OBJCOPY) -O ihex $(TARGET) $(HEX)
	$(OBJCOPY) -O binary $(TARGET) $(BIN)
	$(SIZE) $(TARGET)
endif

$(TARGET): $(OBJS)
	@mkdir -p $(BUILD_DIR)
	$(CC) $(LDFLAGS) -o $@ $^

$(BUILD_DIR)/%.o: $(SRC_DIR)/%.c
	@mkdir -p $(BUILD_DIR)
	$(CC) $(CFLAGS) -c -o $@ $<

clean:
	rm -rf $(BUILD_DIR)

# Flash via ST-Link (requires st-flash or STM32CubeProgrammer)
flash: all
ifndef HOST
	st-flash write $(BIN) 0x08000000
else
	@echo "Flash target not available for host builds"
endif

size:
ifndef HOST
	$(SIZE) $(TARGET)
else
	@ls -la $(TARGET)
endif

host:
	$(MAKE) HOST=1
