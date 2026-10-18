#pragma once

#include "ego_device.h"

// Graphics device (EGO_DEVICE_GRAPHICS): shapes, sprites, charsets and
// character-to-bitmap rendering. All state, commands and handlers are private
// to ego_gfx_device.c; the core dispatches to the device through gfx_device.

// The graphics device as seen by the command parser.
extern const ego_device_desc_t gfx_device;

// Prints the enabled sprites to the log output; used by the debug device.
void gfx_print_debug(void);

// Interactive graphics test for USB mode.
void test(void);
