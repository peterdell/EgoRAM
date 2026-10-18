#pragma once

// Places the function in RAM (section ".time_critical.EgoRAM") so it runs
// at full speed without flash wait states. Requires the pico SDK headers to
// be included first.
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
// and must match asm/EgoRAM-Equates.asm.
typedef enum : uint8_t {
    EGO_REG_CMD,
    EGO_REG_STATUS,
    EGO_REG_DATA,
    EGO_REG_DEVICE
} EgoReg;

_Static_assert(EGO_REG_DEVICE == 3, "EgoReg values must match asm/EgoRAM-Equates.asm");

// Devices inside the cartridge, selected by writing the device number to
// EGO_REG_DEVICE. Graphics is device 0 and active after reset. The values are
// part of the wire protocol and must match asm/EgoRAM-Equates.asm, so only
// append new devices at the end.
typedef enum : uint8_t {
    EGO_DEVICE_GRAPHICS
} EgoDevice;

_Static_assert(EGO_DEVICE_GRAPHICS == 0, "EgoDevice values must match asm/EgoRAM-Equates.asm");

// Phase of the EGO command parser. Each command's fixed parameter bytes are
// described by the active device's command table; bulk payloads are streamed
// in the DATA phases. See plans/03_COMMAND_TABLE.md.
typedef enum : uint8_t {
    EGO_ST_IDLE,    // no command in progress
    EGO_ST_HEADER,  // collecting the fixed parameter bytes of a command
    EGO_ST_DATA_IN, // streaming bulk data from the Atari
    EGO_ST_DATA_OUT // streaming bulk data to the Atari
} EgoState;

// Every device's command 0 is Init: it resets the device to its power-on
// state. The core sends it to all devices at startup. The value is part of
// the wire protocol and must match asm/EgoRAM-Equates.asm.
#define EGO_CMD_INIT 0

// Maximum fixed parameter bytes of a command; devices assert that their
// longest parameter block fits.
#define EGO_PARAMS_MAX 16

// One command of a device: the number of fixed parameter bytes following the
// command byte, and the handler that runs once they have all arrived.
typedef struct {
    uint8_t header_len;
    void (*execute)(const uint8_t *params);
} ego_cmd_desc_t;

// One device inside the cartridge: its command table, indexed by command byte.
// Commands without an entry or with a NULL handler are ignored.
typedef struct {
    const ego_cmd_desc_t *commands;
    uint8_t num_commands;
} ego_device_desc_t;

// Parser services for device command handlers: start a bulk data phase after
// the fixed parameter bytes. stream_out serves the first byte immediately in
// EGO_REG_DATA and the remaining bytes on the subsequent reads.
void stream_in(uint8_t *dst, uint32_t len, void (*done)(void));
void stream_out(const uint8_t *src, uint32_t len);

void write_d5xx(uint8_t addr, uint8_t data);
uint8_t read_d5xx(uint8_t addr);
void ego_log(const char *format, ...);
