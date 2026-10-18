// MOS6502 device (EGO_DEVICE_MOS6502) of the EgoRAM cartridge: a virtual
// 6502 CPU executed by the Fake6502 core (fake6502.c/h, see ego_mos6502.md).
// The CPU runs against its own private 64 KB memory for now; sharing memory
// with the cartridge will be defined later. Everything except the
// mos6502_device descriptor is private to this compile unit.

#include <string.h>
#include "pico/stdlib.h"

#include "EgoRAM.h"
#include "fake6502.h"
#include "ego_mos6502.h"

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

// Private 64 KB address space of the virtual CPU.
static uint8_t memory[0x10000];

// Virtual bus of the Fake6502 core.
uint8_t read6502(uint16_t address) {
    return memory[address];
}

void write6502(uint16_t address, uint8_t value) {
    memory[address] = value;
}

#pragma region Command parameters

// Wire-format parameter blocks, see ego_gfx.c for the conventions.

typedef struct EGO_PACKED {
    uint16_t addr;
    uint16_t length;
    // followed by length memory bytes (DATA_IN stream)
} ego_cmd_set_memory_params_t;

typedef struct EGO_PACKED {
    uint16_t addr;
    uint16_t length;
    // answered with length memory bytes (DATA_OUT stream)
} ego_cmd_get_memory_params_t;

typedef struct EGO_PACKED {
    uint16_t cycles;
} ego_cmd_execute_params_t;

_Static_assert(sizeof(ego_cmd_set_memory_params_t) == 4, "wire layout must match asm client");
_Static_assert(sizeof(ego_cmd_get_memory_params_t) == 4, "wire layout must match asm client");
_Static_assert(sizeof(ego_cmd_execute_params_t) == 2, "wire layout must match asm client");

_Static_assert(sizeof(ego_cmd_set_memory_params_t) <= EGO_PARAMS_MAX, "EGO_PARAMS_MAX must hold the longest header");

#pragma endregion

#pragma region Command handlers

// Resets the device to its power-on state; sent by the core to every device
// at startup. Clears the memory and resets the CPU.
static void RAM_FUNC cmd_init(const uint8_t *p) {
    memset(memory, 0x00, sizeof(memory));
    reset6502();
}

static void RAM_FUNC cmd_set_memory(const uint8_t *p) {
    const ego_cmd_set_memory_params_t *params = (const void *)p;
    uint32_t length = params->length;

    // Clamp at the end of the address space; excess data bytes are ignored.
    if (length > sizeof(memory) - params->addr) {
        length = sizeof(memory) - params->addr;
    }
    stream_in(&memory[params->addr], length, NULL);
}

static void RAM_FUNC cmd_get_memory(const uint8_t *p) {
    const ego_cmd_get_memory_params_t *params = (const void *)p;
    uint32_t length = params->length;

    if (length == 0) {
        return;
    }
    if (length > sizeof(memory) - params->addr) {
        length = sizeof(memory) - params->addr;
    }
    stream_out(&memory[params->addr], length);
}

static void RAM_FUNC cmd_execute(const uint8_t *p) {
    const ego_cmd_execute_params_t *params = (const void *)p;
    uint32_t executed = 0;

    // Instruction granularity: runs at least the requested number of cycles.
    while (executed < params->cycles) {
        executed += step6502();
    }
}

#pragma endregion

// Explicitly placed in RAM so that command dispatch never stalls on flash XIP.
static const ego_cmd_desc_t __not_in_flash("mos6502_commands") commands[] = {
    [EGO_CMD_INIT] = {0, cmd_init},
    [EGO_CMD_SET_MEMORY] = {sizeof(ego_cmd_set_memory_params_t), cmd_set_memory},
    [EGO_CMD_GET_MEMORY] = {sizeof(ego_cmd_get_memory_params_t), cmd_get_memory},
    [EGO_CMD_EXECUTE] = {sizeof(ego_cmd_execute_params_t), cmd_execute},
};

const ego_device_desc_t __not_in_flash("mos6502_device") mos6502_device = {
    .commands = commands,
    .num_commands = sizeof(commands) / sizeof(commands[0]),
};
