// Host-side unit test for the MOS6502 device (src/ego_mos6502_device.c): drives the
// device through its command table and stubs the parser services that
// src/EgoRAM.c provides on the target.

#include <assert.h>
#include <stdio.h>
#include <string.h>
#include "pico/stdlib.h" // the shim in pico_shim/

#include "EgoRAM.h"
#include "ego_device.h"
#include "ego_mos6502.h"
#include "ego_mos6502_device.h"

#pragma region Parser service stubs

static uint8_t *in_ptr;
static uint32_t in_len;
static void (*in_done)(void);
static const uint8_t *out_ptr;
static uint32_t out_len;

void stream_in(uint8_t *dst, uint32_t len, void (*done)(void)) {
    in_ptr = dst;
    in_len = len;
    in_done = done;
}

void stream_out(const uint8_t *src, uint32_t len) {
    out_ptr = src;
    out_len = len;
}

void ego_log(const char *format, ...) {
}

#pragma endregion

static void run(uint8_t cmd, const uint8_t *params) {
    assert(cmd < mos6502_device.num_commands);
    assert(mos6502_device.commands[cmd].execute != NULL);
    mos6502_device.commands[cmd].execute(params);
}

// Sends SET_MEMORY and feeds the data bytes into the requested stream, as the
// parser would.
static void set_memory(uint16_t addr, const uint8_t *data, uint16_t length) {
    uint8_t params[4] = {addr & 0xff, addr >> 8, length & 0xff, length >> 8};

    in_ptr = NULL;
    run(EGO_CMD_SET_MEMORY, params);
    assert(in_ptr != NULL);
    assert(in_len == length);
    memcpy(in_ptr, data, length);
    if (in_done != NULL) {
        in_done();
    }
}

int main(void) {
    // lda #$2a, sta $0200 (6 cycles). After INIT the memory is cleared, so
    // the reset vector is $0000 and execution starts at the program.
    const uint8_t program[] = {0xa9, 0x2a, 0x8d, 0x00, 0x02};
    const uint8_t execute_params[] = {6, 0};
    const uint8_t get_params[] = {0x00, 0x02, 0x01, 0x00};

    run(EGO_CMD_INIT, NULL);
    set_memory(0x0000, program, sizeof(program));
    run(EGO_CMD_EXECUTE, execute_params);

    out_ptr = NULL;
    run(EGO_CMD_GET_MEMORY, get_params);
    assert(out_ptr != NULL);
    assert(out_len == 1);
    assert(out_ptr[0] == 0x2a);

    printf("test_ego_mos6502: all tests passed\n");
    return 0;
}
