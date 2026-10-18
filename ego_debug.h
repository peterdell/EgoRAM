#pragma once

#include "EgoRAM.h"

// Debug device (EGO_DEVICE_DEBUG): prints firmware state to the log output.
// All command handlers are private to ego_debug.c; the core dispatches to the
// device through debug_device.

// Command bytes written to EGO_REG_CMD while the debug device is active.
// The values are part of the wire protocol and must match
// asm/EgoRAM-Equates.asm, so only append new commands at the end.
typedef enum : uint8_t {
    EGO_CMD_DEBUG = 1 // 0 is EGO_CMD_INIT, common to all devices
} EgoDebugCmd;

_Static_assert(EGO_CMD_DEBUG == 1, "EgoDebugCmd values must match asm/EgoRAM-Equates.asm");

// The debug device as seen by the command parser.
extern const ego_device_desc_t debug_device;
