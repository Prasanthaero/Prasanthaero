# PMIC Clone Tool for TCON Boards

An embedded C tool for STM32 that reads, stores, writes, and verifies PMIC (Power Management IC) register configurations on TCON (Timing Controller) boards over I2C.

## Features

- **Read** all PMIC registers from a source TCON board
- **Write** stored configuration to a target TCON board
- **Verify** programmed values match the source
- **Clone** in one step: read + write + verify
- **Save/Load** up to 4 snapshots in STM32 internal flash
- **Compare** two saved snapshots to see differences
- **I2C bus scan** to discover connected devices
- **Multiple PMIC profiles** (TPS65185, TPS65186, MAX20998) plus custom/generic profiles
- **Bit-mask aware** read-modify-write for partial registers
- **UART CLI** for interactive serial console control

## Project Structure

```
include/
  pmic_i2c.h        - I2C driver abstraction
  pmic_regmap.h      - Register map / profile definitions
  pmic_clone.h       - Clone engine API
  pmic_storage.h     - Flash storage API
  pmic_cli.h         - UART CLI API
  pmic_platform.h    - Platform configuration (STM32 family, pins, flash sector)
src/
  pmic_i2c.c         - I2C read/write/scan implementation (STM32 HAL)
  pmic_regmap.c      - Built-in PMIC profiles + CRC32 checksum
  pmic_clone.c       - Clone engine (read source, write target, verify, diff)
  pmic_storage.c     - Flash slot-based snapshot storage
  pmic_cli.c         - Command parser and handlers
  pmic_platform.c    - UART output, delay, tick (STM32 HAL + host stubs)
  main.c             - Application entry point
Makefile             - Build for STM32 (cross-compile) or host (native)
```

## Hardware Setup

```
STM32 MCU
  |-- I2C1 SDA/SCL --> TCON Board PMIC (7-bit address, e.g. 0x68)
  |-- USART2 TX/RX --> USB-Serial adapter --> PC terminal (115200 baud)
```

## Building

### STM32 Cross-Compilation

Requires `arm-none-eabi-gcc` and STM32 HAL headers:

```bash
# Edit include/pmic_platform.h to select your STM32 family
# Provide your linker script path in the Makefile

make STM32_FAMILY=STM32F4 CPU=cortex-m4
make flash    # Flash via ST-Link
```

### Host Build (PC testing)

Builds with native gcc for testing the CLI and logic without hardware:

```bash
make host
./build/pmic_clone_tool
```

## CLI Commands

Connect a serial terminal at **115200 baud** and use these commands:

| Command | Description |
|---|---|
| `help` | Show all commands |
| `scan` | Scan I2C bus for devices |
| `profile list` | List built-in PMIC profiles |
| `profile select <name>` | Select a profile (e.g. `TPS65185`) |
| `profile generic <addr> <nregs>` | Create a custom profile |
| `read [addr]` | Read all registers from source PMIC |
| `write [addr]` | Write snapshot to target PMIC |
| `verify [addr]` | Verify target matches snapshot |
| `clone [src_addr] [dst_addr]` | Full clone: read + write + verify |
| `dump` | Print current register snapshot |
| `reg read <reg> [addr]` | Read a single register |
| `reg write <reg> <val> [addr]` | Write a single register |
| `save <slot>` | Save snapshot to flash (0-3) |
| `load <slot>` | Load snapshot from flash |
| `slots` | Show flash slot status |
| `erase <slot\|all>` | Erase flash slot(s) |
| `diff <slotA> <slotB>` | Compare two saved snapshots |
| `status` | Show current tool state |

Addresses and register values accept hex (`0x68`) or decimal (`104`).

## Typical Workflow

```
pmic> scan                        # Find PMIC on the bus
pmic> profile select TPS65185     # Select matching profile
pmic> read                        # Read all regs from source board
pmic> dump                        # Inspect captured values
pmic> save 0                      # Save to flash slot 0

# Swap to target board
pmic> write                       # Program target PMIC
pmic> verify                      # Confirm all registers match

# Or do it all at once:
pmic> clone 0x68 0x68             # Read source, write target, verify
```

## Adding a New PMIC Profile

Edit `src/pmic_regmap.c` and add a new `pmic_profile_t` struct with:
- PMIC name and default I2C address
- Register list with address, default value, writable bit mask, and access flags

Then add it to the `s_profiles[]` table.

## Configuration

Edit `include/pmic_platform.h` to configure:
- **STM32 family** (F1, F4, L4, H7)
- **Flash sector** for snapshot storage
- **I2C speed** (100 kHz standard / 400 kHz fast mode)
- **UART baud rate**

Edit `src/pmic_platform.c` to change the UART/I2C peripheral handles if they differ from the defaults (`hi2c1`, `huart2`).
