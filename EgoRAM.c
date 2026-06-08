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
#include "pico/rand.h"
#include "hardware/pio.h"
#include "hardware/clocks.h"
#include "hardware/pll.h"
#include "hardware/structs/systick.h"
#include "hardware/vreg.h"

#include "vt100_font_8x8.h"
#include "atari_cart.h"
#include "EgoRAM.h"
#include "cvideo.h"

char c;
int posx, posy;
char *vram;

sprite_t *sp0;
sprite_t *sp_array[EGO_MAX_SPRITES];
int dx_array[EGO_MAX_SPRITES];
int dy_array[EGO_MAX_SPRITES];
int sp_cnt;

static char strbuf[80];

static uint8_t *char_font = (uint8_t *)vt100_font_8x8;
static int blitwidth;
static int blitheight;
static uint32_t mode;

static volatile uint8_t *video_ram;
static volatile uint8_t *cart_d5xx;
static volatile uint32_t ticks;
static int uart_init_once;

static PIO pio = pio0;

static void
systick_init(void)
{
    systick_hw->csr = 0x5;        // Enable + CPU-Clock als Quelle
    systick_hw->rvr = 0x00FFFFFF; // Maximaler Reload-Wert (24-Bit)
}

int get_blitwidth()
{
    return blitwidth;
}

int get_blitheight()
{
    return blitheight;
}

void set_blitwidth(int w)
{
    blitwidth = w;
}

void set_blitheight(int h)
{
    blitheight = h;
}

sprite_t *sprite_new(uint8_t width, uint8_t height)
{
    sprite_t *sp;

    sp = malloc(sizeof(sprite_t));
    sp->width = width;
    sp->height = height;
    sp->data = malloc(width * height);
    memset(sp->data, 0xff, width * height);

    if (sp == NULL || sp->data == NULL)
    {
        ego_log("sprite_new failed!\n");
    }

    return sp;
}

void sprite_free(sprite_t *sp)
{
    if (sp)
    {
        free(sp->data);
        free(sp);
    }
}

void __not_in_flash("sprite_draw_all") sprite_draw_all()
{
    for (int i = 0; i < EGO_MAX_SPRITES; i++)
    {
        // ego_log("%d %p\n", i, sp_array[i]);
        sprite_draw(sp_array[i]);
    }
}

void __not_in_flash("sprite_draw") sprite_draw(sprite_t *sp)
{
    if (sp != NULL)
    {
        // ego_log("sprite_draw: %p x:%d y:%d \n", sp, (int)sp->xpos, (int)sp->ypos);
        sprite_draw_xy(sp, sp->xpos, sp->ypos);
    }
}

void __not_in_flash("sprite_draw_xy") sprite_draw_xy(sprite_t *sp, uint16_t xpos, uint16_t ypos)
{
    int bytex = (xpos >> 3) - sp->width;
    int pixx = xpos & 0x07;

    int ystart;
    int vstart;
    int ylen;
    int vdiff;
    int x, y;

    volatile uint8_t *vpos;
    uint8_t *sppos;
    char c;

    // ego_log("sprite_draw_xy: %p x:%d y:%d \n", sp, sp->xpos, sp->ypos);

    if (xpos < 0 || ypos < 0)
        return;

    sp->xpos = xpos;
    sp->ypos = ypos;

    ystart = sp->height - ypos;
    if (ystart < 0)
        ystart = 0;

    ylen = sp->height - ystart;

    vstart = ypos - sp->height;
    if (vstart < 0)
        vstart = 0;

    vdiff = blitheight + sp->height - ypos;
    if (vdiff < sp->height)
    {
        ylen = vdiff;
    }

    vpos = &video_ram[vstart * blitwidth];
    sppos = &sp->data[ystart * sp->width];

    // ego_log("sprite xpos:%d ypos:%d bytex: %d pixx:%d ystart:%d ylen:%d\n", xpos, ypos, bytex, pixx, ystart, ylen);

    for (y = 0; y < ylen; y++)
    {
        c = 0;
        for (x = 0; x < sp->width; x++)
        {
            if (bytex + x >= 0 && bytex + x < blitwidth)
                vpos[bytex + x] ^= (c | (sppos[x] >> pixx));
            c = (sppos[x]) << (8 - pixx);
        }
        if (bytex + x >= 0 && bytex + x < blitwidth)
            vpos[bytex + x] ^= c;

        vpos += blitwidth;
        sppos += sp->width;
    }
}

void __not_in_flash_func(renderer_callback)(void)
{
    int xpos, ypos;
    sprite_t *sp;

    // uint8_t d5xx_addr = get_d5xx_addr();
    // uint8_t d5xx_data = get_d5xx_data();

    for (int i = 0; i < EGO_MAX_SPRITES; i++)
    {
        sp = sp_array[i];

        sprite_draw(sp);

        xpos = sp->xpos;
        ypos = sp->ypos;

        xpos += dx_array[i];
        if (xpos >= blitwidth << 3 || xpos <= 0)
        {
            dx_array[i] *= -1;
            xpos += dx_array[i];
        }

        ypos += dy_array[i];
        if (ypos >= blitheight || ypos <= 0)
        {
            dy_array[i] *= -1;
            ypos += dy_array[i];
        }

        sprite_draw_xy(sp, xpos, ypos);
    }

    // ego_log("ticks: %d\n", (ticks - systick_hw->cvr) & 0xffffff);
}

void do_data(uint8_t data)
{
}

void do_command(uint8_t data)
{
    switch (data)
    {
    case EGO_CMD_RENDER_SPRITES:
        renderer_callback();
        break;
    case EGO_CMD_START_SPRITE_DATA:
        break;
    case EGO_CMD_SET_WRITEABLE:
        set_writeable(true);
        cart_d5xx[EGO_REG_STATUS] |= 0x01;
        break;
    case EGO_CMD_SET_READONLY:
        set_writeable(false);
        cart_d5xx[EGO_REG_STATUS] &= ~0x01;
        break;
    case EGO_CMD_ABORT:
        break;
    default:
        break;
    }
}

void __not_in_flash("core1_main") core1_main()
{
    uint32_t msg;
    uint32_t data;
    uint32_t addr;

    systick_init();

    ego_log("core1 started...\n");

    while (true)
    {
        msg = multicore_fifo_pop_blocking();

        ticks = systick_hw->cvr;

        addr = msg & 0xff;
        data = msg >> 16;

        switch (addr)
        {
        case EGO_REG_CMD:
            do_command(data);
            break;
        case EGO_REG_DATA:
            do_data(data);
            break;
        default:
            break;
        }

        ticks = (ticks - systick_hw->cvr) & 0xffffff;

        cart_d5xx[0] &= ~0x80;
        ego_log("addr: %04X, data: %02X, ticks: %d\n", msg & 0xffff, msg >> 16, ticks);
    }
}

void putChar(int xpos, int ypos, uint8_t c)
{
    uint8_t volatile *pscreen = &video_ram[(ypos * blitwidth * 8) + xpos];
    uint8_t *puint8_t = &char_font[c << 3];

    for (int i = 0; i < 8; i++)
    {
        *pscreen = *puint8_t;
        puint8_t++;
        pscreen += PAL_WIDTH;
    }
}

uint8_t read_d5xx(uint8_t addr)
{
    return cart_d5xx[addr];
}

void write_d5xx(uint8_t addr, uint8_t data)
{

    cart_d5xx[EGO_REG_STATUS] |= 0x80;

    uint32_t msg = data << 16 | addr;
    multicore_fifo_push_blocking(msg);
}

void ego_log(const char *format, ...)
{

    va_list args;

    va_start(args, format);
    vsnprintf(strbuf, sizeof(strbuf), format, args);
    va_end(args);

    if (mode == EGO_MODE_USB)
    {
        while (!stdio_usb_connected())
        {
            sleep_ms(100);
        }

        printf("%s", strbuf);
    }
    else
    {
        if (uart_init_once == 0)
        {
            uart_init_once = 1;
            uart_init(EGO_UART_ID, EGO_BAUD_RATE);
            gpio_set_function(EGO_UART_TX_PIN, GPIO_FUNC_UART);
            gpio_set_function(EGO_UART_RX_PIN, GPIO_FUNC_UART);
            // uart_puts(EGO_UART_ID, "UART0 initialised at GPIO 28!\n");
        }
        uart_puts(EGO_UART_ID, strbuf);
    }
}

int main()
{
    vreg_set_voltage(VREG_VOLTAGE_1_20);
    sleep_ms(100);
    set_sys_clock_khz(250000, true);
    sleep_ms(100);
    systick_init();

    stdio_init_all();

    // check to see if we are plugged into Atari 8-bit
    // by checking for high on PHI2 gpio for xxx ms
    gpio_init(ATARI_PHI2_PIN);
    gpio_set_dir(ATARI_PHI2_PIN, GPIO_IN);

    uart_init_once = 0;
    mode = EGO_MODE_USB;

    while (to_ms_since_boot(get_absolute_time()) < 300)
    {
        if (gpio_get(ATARI_PHI2_PIN))
        {
            mode = EGO_MODE_ATARI;
        }
    }

    video_ram = get_cart_ram();
    cart_d5xx = get_cart_d5xx();

    memset((void *)video_ram, 0xaa, 8 * 1024);

    ego_log("\033[2J\033[HEgoRAM starting...\n");
    if (mode == EGO_MODE_USB)
        ego_log("mode: USB\n");
    else
        ego_log("mode: ATARI\n");

    ego_log("sys_clock: %d\n", clock_get_hz(clk_sys));
    ego_log("video_ram at %p\n", video_ram);
    ego_log("cart_d5xx at %p\n", cart_d5xx);

    set_blitwidth(40);
    set_blitheight(200);

    for (int i = 0; i < EGO_MAX_SPRITES; i++)
    {
        sp_array[i] = sprite_new(3, 16);
        sp_array[i]->xpos = get_rand_32() % 639;
        sp_array[i]->ypos = get_rand_32() % 199;

        dx_array[i] = 1;
        if (get_rand_32() & 0x01)
            dx_array[i] *= -1;

        dy_array[i] = 1;
        if (get_rand_32() & 0x01)
            dy_array[i] *= -1;
    }
    sp_cnt = EGO_MAX_SPRITES;

    for (int i = 0; i < PAL_WIDTH * PAL_HEIGHT; i++)
    {
        putChar(i % PAL_WIDTH, i / PAL_WIDTH, i);
    }

    // sprite_draw_all();

    multicore_launch_core1(core1_main);

    if (mode == EGO_MODE_ATARI)
    {
        atari_cart_main();
    }

    initialise_cvideo(pio0);

    while (true)
    {
        c = getchar();

        write_d5xx(0x00, 0x00);

        while (read_d5xx(0) & 0x80)
            ;
    }
}
