// Stubs for the parser services that src/EgoRAM.c provides on the target,
// shared by all device tests.

#include "pico/stdlib.h" // the shim in pico_shim/

#include "EgoRAM.h"
#include "ego_device.h"
#include "EgoRAM_stubs.h"

uint8_t *in_ptr;
uint32_t in_len;
void (*in_done)(void);
const uint8_t *out_ptr;
uint32_t out_len;

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
