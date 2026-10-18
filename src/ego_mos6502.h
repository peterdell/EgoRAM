#pragma once

#include <stdint.h>

// Wire protocol of the MOS6502 device (EGO_DEVICE_MOS6502): the command
// bytes. Included by ego_mos6502_device.c and its tests only — every device's
// wire-protocol header defines EgoCmd, so a compile unit can include at most
// one of them.

// Command bytes written to EGO_REG_CMD while the MOS6502 device is active.
// The values are part of the wire protocol and must match the enum cmd in
// the ego_mos6502 namespace of asm/EgoRAM-Equates.asm, so only append new
// commands at the end.
typedef enum : uint8_t {
    EGO_CMD_SET_MEMORY = 1, // 0 is EGO_CMD_INIT, common to all devices
    EGO_CMD_GET_MEMORY,
    EGO_CMD_EXECUTE
} EgoCmd;

_Static_assert(EGO_CMD_EXECUTE == 3, "EgoCmd values must match asm/EgoRAM-Equates.asm");
