#pragma once

#include <stdint.h>

// Wire protocol of the debug device (EGO_DEVICE_DEBUG): the command bytes.
// Included by ego_debug_device.c and its tests only — every device's
// wire-protocol header defines EgoCmd, so a compile unit can include at most
// one of them.

// Command bytes written to EGO_REG_CMD while the debug device is active.
// The values are part of the wire protocol and must match the enum cmd in
// the ego_debug namespace of asm/EgoRAM-Equates.asm, so only append new
// commands at the end.
typedef enum : uint8_t {
    EGO_CMD_DEBUG = 1 // 0 is EGO_CMD_INIT, common to all devices
} EgoCmd;

_Static_assert(EGO_CMD_DEBUG == 1, "EgoCmd values must match asm/EgoRAM-Equates.asm");
