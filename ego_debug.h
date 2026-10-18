#pragma once

#include "ego_device.h"

// Debug device (EGO_DEVICE_DEBUG): prints firmware state to the log output.
// All commands and handlers are private to ego_debug.c; the core dispatches
// to the device through debug_device.

// The debug device as seen by the command parser.
extern const ego_device_desc_t debug_device;
