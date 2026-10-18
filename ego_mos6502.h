#pragma once

#include "ego_device.h"

// MOS6502 device (EGO_DEVICE_MOS6502): a virtual 6502 CPU executed by the
// Fake6502 core (fake6502.c/h), see ego_mos6502.md. All state and command
// handlers are private to ego_mos6502.c; the core dispatches to the device
// through mos6502_device.

// Command bytes written to EGO_REG_CMD while the MOS6502 device is active.
// The values are part of the wire protocol and must match
// asm/EgoRAM-Equates.asm, so only append new commands at the end.
typedef enum : uint8_t {
    EGO_CMD_SET_MEMORY = 1, // 0 is EGO_CMD_INIT, common to all devices
    EGO_CMD_GET_MEMORY,
    EGO_CMD_EXECUTE
} EgoMos6502Cmd;

_Static_assert(EGO_CMD_EXECUTE == 3, "EgoMos6502Cmd values must match asm/EgoRAM-Equates.asm");

// The MOS6502 device as seen by the command parser.
extern const ego_device_desc_t mos6502_device;
