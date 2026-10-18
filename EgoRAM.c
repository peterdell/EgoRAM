/**
 * Copyright (c) 2020 Raspberry Pi (Trading) Ltd.
 *
 * SPDX-License-Identifier: BSD-3-Clause
 */

/*

A0-12  13
D0-7    8
CCTL    1   gpio 21
PHI2    1   gpio 22
RW      1   gpio 23
S4      1   gpio 24
S5      1   gpio 25
    ------
       26
    ======

#define ALL_GPIO_MASK 0x3FFFFFFF
#define ADDR_GPIO_MASK 0x00001FFF
#define DATA_GPIO_MASK 0x001FE000
#define CCTL_GPIO_MASK 0x00200000 // gpio 21
#define PHI2_GPIO_MASK 0x00400000 // gpio 22
#define RW_GPIO_MASK 0x00800000	  // gpio 23
#define S4_GPIO_MASK 0x01000000	  // gpio 24
#define S5_GPIO_MASK 0x02000000	  // gpio 25
*/

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdarg.h>
#include "pico/stdlib.h"
#include "pico/multicore.h"
#include "hardware/pio.h"
#include "hardware/clocks.h"
#include "hardware/pll.h"
#include "hardware/structs/systick.h"
#include "hardware/vreg.h"
#include "hardware/structs/bus_ctrl.h"
#include "hardware/regs/busctrl.h"

#include "atari_cart.h"
#include "EgoRAM.h"
#include "ego_gfx.h"
#include "cvideo.h"

char c;

static char strbuf[256];

static EgoMode mode;

static volatile uint8_t *cart_d5xx;
static volatile uint32_t ticks;
static bool uart_init_once;

static EgoDevice ego_device; // zero-initialized: EGO_DEVICE_GRAPHICS
static uint8_t ego_cmd;
static EgoState ego_state;
static uint8_t ego_params[EGO_PARAMS_MAX];
static uint8_t ego_param_idx;
static uint8_t *ego_in_ptr;
static uint32_t ego_in_len;
static void (*ego_in_done)(void);
static const uint8_t *ego_out_ptr;
static uint32_t ego_out_len;

// Devices and their command tables, indexed by EgoDevice. Placed in RAM so
// that command dispatch never stalls on flash XIP.
static const ego_device_desc_t *const __not_in_flash("ego_devices") ego_devices[] = {
    [EGO_DEVICE_GRAPHICS] = &gfx_device,
};

static PIO pio = pio0;

static void systick_init(void) {
    systick_hw->csr = 0x5;        // Enable + CPU-Clock als Quelle
    systick_hw->rvr = 0x00FFFFFF; // Maximaler Reload-Wert (24-Bit)
}

void RAM_FUNC stream_in(uint8_t *dst, uint32_t len, void (*done)(void)) {
    if (len == 0) {
        return;
    }
    ego_in_ptr = dst;
    ego_in_len = len;
    ego_in_done = done;
    ego_state = EGO_ST_DATA_IN;
}

void RAM_FUNC stream_out(const uint8_t *src, uint32_t len) {
    cart_d5xx[EGO_REG_DATA] = src[0];
    ego_out_ptr = &src[1];
    ego_out_len = len - 1;
    ego_state = EGO_ST_DATA_OUT;
}

void RAM_FUNC do_data(uint8_t data) {
    switch (ego_state) {
    case EGO_ST_HEADER:
        ego_params[ego_param_idx++] = data;
        if (ego_param_idx == ego_devices[ego_device]->commands[ego_cmd].header_len) {
            ego_state = EGO_ST_IDLE; // execute() may start a data phase
            ego_devices[ego_device]->commands[ego_cmd].execute(ego_params);
        }
        break;
    case EGO_ST_DATA_IN:
        *ego_in_ptr++ = data;
        if (--ego_in_len == 0) {
            ego_state = EGO_ST_IDLE;
            if (ego_in_done != NULL) {
                ego_in_done();
            }
        }
        break;
    default:
        break;
    }
}

void RAM_FUNC do_command(uint8_t data) {
    const ego_device_desc_t *device = ego_devices[ego_device];

    ego_cmd = data;
    ego_param_idx = 0;

    if (data >= device->num_commands || device->commands[data].execute == NULL) {
        ego_state = EGO_ST_IDLE;
        return;
    }

    if (device->commands[data].header_len == 0) {
        ego_state = EGO_ST_IDLE; // execute() may start a data phase
        device->commands[data].execute(NULL);
    }
    else {
        ego_state = EGO_ST_HEADER;
    }
}

void RAM_FUNC core1_main() {
    uint32_t msg;
    uint8_t data;
    uint16_t addr;

    systick_init();

    ego_log("core1 started...\n");

    while (true) {
        msg = multicore_fifo_pop_blocking();

        ticks = systick_hw->cvr;

        addr = msg & 0xff;
        data = msg >> 16;

        switch (addr) {
        case EGO_REG_CMD:
            do_command(data);
            /*
            if (data == EGO_CMD_RENDER_SPRITES) {
                ticks = (ticks - systick_hw->cvr) & 0xffffff;
                ego_log("addr: %04X, data: %02X, ticks: %d\n", msg & 0xffff, msg >> 16, ticks);
            }
            */
            break;
        case EGO_REG_DATA:
            do_data(data);
            break;
        case EGO_REG_DEVICE:
            // Unknown device numbers are ignored. Switching devices aborts
            // any command in progress. The register is readable so the Atari
            // can check the active device.
            if (data < sizeof(ego_devices) / sizeof(ego_devices[0]) && ego_devices[data] != NULL) {
                ego_device = data;
                ego_state = EGO_ST_IDLE;
                cart_d5xx[EGO_REG_DEVICE] = data;
            }
            break;
        default:
            break;
        }

        // Clear busy only once all queued writes are processed, so the
        // Atari can rely on it as a "command fully handled" barrier.
        if (!multicore_fifo_rvalid()) {
            cart_d5xx[EGO_REG_STATUS] &= ~0x80;
        }
    }
}

uint8_t read_d5xx(uint8_t addr) {

    switch (ego_state) {
    case EGO_ST_DATA_OUT:
        cart_d5xx[addr] = *ego_out_ptr++;
        if (--ego_out_len == 0) {
            ego_state = EGO_ST_IDLE;
        }
        break;
    default:
        break;
    }

    return cart_d5xx[addr];
}

void RAM_FUNC write_d5xx(uint8_t addr, uint8_t data) {
    cart_d5xx[EGO_REG_STATUS] |= 0x80;

    if (multicore_fifo_wready) {
        multicore_fifo_push_blocking(addr | (data << 16));
    }
    else {
        ego_log("write to FiFo failed\n");
    }
}

void RAM_FUNC ego_log(const char *format, ...) {

    va_list args;

    va_start(args, format);
    vsnprintf(strbuf, sizeof(strbuf), format, args);
    va_end(args);

    if (mode == EGO_MODE_USB) {
        while (!stdio_usb_connected()) {
            sleep_ms(100);
        }

        printf("%s", strbuf);
    }
    else {
        if (!uart_init_once) {
            uart_init_once = true;
            uart_init(EGO_UART_ID, EGO_BAUD_RATE);
            gpio_set_function(EGO_UART_TX_PIN, GPIO_FUNC_UART);
            gpio_set_function(EGO_UART_RX_PIN, GPIO_FUNC_UART);
        }

        uart_puts(EGO_UART_ID, strbuf);
    }
}

void set_core0_highest_bus_priority() {
    // Setzt die Bus-Priorität für Kern 0 auf hoch (1) und Kern 1 auf niedrig (0)
    bus_ctrl_hw->priority = BUSCTRL_BUS_PRIORITY_PROC0_BITS;
}

int main() {
    vreg_set_voltage(VREG_VOLTAGE_1_20);
    sleep_ms(10);
    set_sys_clock_khz(250000, true);
    sleep_ms(10);
    systick_init();

    stdio_init_all();

    // check to see if we are plugged into Atari 8-bit
    // by checking for high on PHI2 gpio for xxx ms
    gpio_init(ATARI_PHI2_PIN);
    gpio_set_dir(ATARI_PHI2_PIN, GPIO_IN);

    uart_init_once = false;
    mode = EGO_MODE_USB;

    while (to_ms_since_boot(get_absolute_time()) < 100) {
        if (!gpio_get(ATARI_PHI2_PIN)) {
            mode = EGO_MODE_ATARI;
        }
    }

    cart_d5xx = get_cart_d5xx();

    // Send every device its Init command (command 0 by convention) before
    // core 1 starts parsing.
    for (uint8_t device = 0; device < sizeof(ego_devices) / sizeof(ego_devices[0]); device++) {
        ego_device = device;
        do_command(EGO_CMD_INIT);
    }
    ego_device = EGO_DEVICE_GRAPHICS;

    memset((void *)(get_cart_ram() + 0x2000), 0xaa, 8 * 1024);

    ego_log("\033[2J\033[HEgoRAM starting...\n");
    if (mode == EGO_MODE_USB) {
        ego_log("mode: USB\n");
    }
    else {
        ego_log("mode: ATARI\n");
    }

    ego_log("sys_clock: %d\n", clock_get_hz(clk_sys));
    ego_log("video_ram at %p\n", get_cart_ram());
    ego_log("cart_d5xx at %p\n", cart_d5xx);

    multicore_launch_core1(core1_main);

    if (mode == EGO_MODE_ATARI) {
        // irq_set_enabled(USBCTRL_IRQ, false);
        uint32_t status = save_and_disable_interrupts();
        set_core0_highest_bus_priority();
        atari_cart_main();
    }

    initialise_cvideo(pio0);

    test();
}
