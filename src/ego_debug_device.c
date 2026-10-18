// Debug device (EGO_DEVICE_DEBUG) of the EgoRAM cartridge: prints firmware
// state to the log output. Everything except the debug_device descriptor is
// private to this compile unit.

#include "pico/stdlib.h"

#include "EgoRAM.h"
#include "ego_gfx_device.h"
#include "ego_debug.h"
#include "ego_debug_device.h"

// Resets the device to its power-on state; sent by the core to every device
// at startup. The debug device has no state.
static void RAM_FUNC cmd_init(const uint8_t *p) {
}

static void RAM_FUNC cmd_debug(const uint8_t *p) {
    gfx_print_debug();
}

// Explicitly placed in RAM so that command dispatch never stalls on flash XIP.
static const ego_cmd_desc_t __not_in_flash("debug_commands") commands[] = {
    [EGO_CMD_INIT] = {0, cmd_init},
    [EGO_CMD_DEBUG] = {0, cmd_debug},
};

const ego_device_desc_t __not_in_flash("debug_device") debug_device = {
    .commands = commands,
    .num_commands = sizeof(commands) / sizeof(commands[0]),
};
