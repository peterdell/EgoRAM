#pragma once

#include "ego_device.h"

// MOS6502 device (EGO_DEVICE_MOS6502): a virtual 6502 CPU executed by the
// Fake6502 core (fake6502.c/h), see ego_mos6502.md. All state, commands and
// handlers are private to ego_mos6502.c; the core dispatches to the device
// through mos6502_device.

// The MOS6502 device as seen by the command parser.
extern const ego_device_desc_t mos6502_device;
