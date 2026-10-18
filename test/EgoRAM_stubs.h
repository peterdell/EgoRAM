#pragma once

#include <stdint.h>

// Stubs for the parser services that src/EgoRAM.c provides on the target,
// shared by all device tests (EgoRAM_stubs.c). stream_in and stream_out only
// record the request; the test feeds or reads the data through these
// variables.

extern uint8_t *in_ptr;
extern uint32_t in_len;
extern void (*in_done)(void);
extern const uint8_t *out_ptr;
extern uint32_t out_len;
