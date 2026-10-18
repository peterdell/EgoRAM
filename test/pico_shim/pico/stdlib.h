#pragma once

// Minimal host-side stand-in for the pico SDK, used by the unit tests in
// test/. Provides just enough for the device compile units: the standard
// integer types the sources expect and a no-op __not_in_flash (RAM_FUNC).

#include <stdint.h>
#include <stdbool.h>
#include <stddef.h>
#include <stdio.h>

#define __not_in_flash(group)
