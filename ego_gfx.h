#pragma once

#include "EgoRAM.h"

// Graphics device (EGO_DEVICE_GRAPHICS): shapes, sprites, charsets and
// character-to-bitmap rendering. All state and command handlers are private
// to ego_gfx.c; the core dispatches to the device through gfx_device.

// Command bytes written to EGO_REG_CMD while the graphics device is active.
// The values are part of the wire protocol and must match
// asm/EgoRAM-Equates.asm, so only append new commands at the end.
typedef enum : uint8_t {
    EGO_CMD_RENDER_SPRITES = 1, // 0 is EGO_CMD_INIT, common to all devices
    EGO_CMD_SET_WRITEABLE,
    EGO_CMD_SET_READONLY,
    EGO_CMD_ABORT,
    EGO_CMD_SHAPE_DATA,
    EGO_CMD_DEL_SHAPE,
    EGO_CMD_SPRITE_DATA,
    EGO_CMD_DEL_SPRITE,
    EGO_CMD_ENA_SPRITE,
    EGO_CMD_DIS_SPRITE,
    EGO_CMD_SET_SPRITE_X,
    EGO_CMD_SET_SPRITE_Y,
    EGO_CMD_SET_SPRITE_XY,
    EGO_CMD_SET_BLIT_WIDTH,
    EGO_CMD_SET_BLIT_HEIGHT,
    EGO_CMD_LINE_PTR,
    EGO_CMD_FILL_MEM,
    EGO_CMD_SPRITE_MODE,
    EGO_CMD_CHARSET,
    EGO_CMD_CHAR_TO_VIDEO,
    EGO_CMD_SET_CHAR,
    EGO_CMD_GET_CHAR,

    EGO_CMD_DEBUG = 99
} EgoCmd;

_Static_assert(EGO_CMD_GET_CHAR == 22, "EgoCmd values must match asm/EgoRAM-Equates.asm");

// Sprite blit mode, set via EGO_CMD_SPRITE_MODE. The values are part of the
// wire protocol and must match asm/EgoRAM-Equates.asm.
typedef enum : uint8_t {
    EGO_MODE_XOR,
    EGO_MODE_MASK,
    EGO_MODE_SPECIAL
} EgoSpriteMode;

_Static_assert(EGO_MODE_SPECIAL == 2, "EgoSpriteMode values must match asm/EgoRAM-Equates.asm");

// The graphics device as seen by the command parser.
extern const ego_device_desc_t gfx_device;

// Interactive graphics test for USB mode.
void test(void);
