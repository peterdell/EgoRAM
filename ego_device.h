#pragma once

#include "EgoRAM.h"

// Definitions for implementing a device inside the cartridge. Each device is
// its own compile unit that exports an ego_device_desc_t and registers it in
// ego_devices[] in EgoRAM.c; see plans/05_DEVICES.md.

// Every device's command 0 is Init: it resets the device to its power-on
// state. The core sends it to all devices at startup. The value is part of
// the wire protocol and must match asm/EgoRAM-Equates.asm.
#define EGO_CMD_INIT 0

// Maximum fixed parameter bytes of a command; devices assert that their
// longest parameter block fits.
#define EGO_PARAMS_MAX 16

// For the wire-format parameter structs that overlay the parser's parameter
// buffer in the command handlers.
#define EGO_PACKED __attribute__((packed))

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
