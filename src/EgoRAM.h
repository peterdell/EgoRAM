#pragma once

#include <stdint.h>

// Places the function in RAM (section ".time_critical.EgoRAM") so it runs
// at full speed without flash wait states. Compile units using it must
// include the pico SDK headers first, which define __not_in_flash.
#define RAM_FUNC __not_in_flash("EgoRAM")

// Operating mode of the cartridge, internal to the firmware.
typedef enum : uint8_t {
    EGO_MODE_ATARI,
    EGO_MODE_USB
} EgoMode;

#define EGO_UART_ID uart0
#define EGO_BAUD_RATE 115200
#define EGO_UART_TX_PIN 28
#define EGO_UART_RX_PIN 29

// Register offsets within $D5xx. The values are part of the wire protocol
// and must match the scoped enum ego_reg in asm/EgoRAM-Equates.asm, which
// holds the absolute $D5xx addresses.
typedef enum : uint8_t {
    EGO_REG_CMD,
    EGO_REG_STATUS,
    EGO_REG_DATA,
    EGO_REG_DEVICE
} EgoReg;

_Static_assert(EGO_REG_DEVICE == 3, "EgoReg values must match asm/EgoRAM-Equates.asm");

// Devices inside the cartridge, selected by writing the device number to
// EGO_REG_DEVICE. The debug device is device 0 and active after reset, so
// clients select their device first. The values are part of the wire protocol
// and must match the scoped enum ego_device in asm/EgoRAM-Equates.asm, so
// only append new devices at the end.
typedef enum : uint8_t {
    EGO_DEVICE_DEBUG,
    EGO_DEVICE_GRAPHICS,
    EGO_DEVICE_MOS6502
} EgoDevice;

_Static_assert(EGO_DEVICE_MOS6502 == 2, "EgoDevice values must match asm/EgoRAM-Equates.asm");

// Phase of the EGO command parser. Each command's fixed parameter bytes are
// described by the active device's command table; bulk payloads are streamed
// in the DATA phases. See plans/03_COMMAND_TABLE.md.
typedef enum : uint8_t {
    EGO_ST_IDLE,    // no command in progress
    EGO_ST_HEADER,  // collecting the fixed parameter bytes of a command
    EGO_ST_DATA_IN, // streaming bulk data from the Atari
    EGO_ST_DATA_OUT // streaming bulk data to the Atari
} EgoState;

void write_d5xx(uint8_t addr, uint8_t data);
uint8_t read_d5xx(uint8_t addr);
void ego_log(const char *format, ...);
