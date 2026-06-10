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
int dx_array[EGO_MAX_SPRITES];
int dy_array[EGO_MAX_SPRITES];
int sp_cnt;

static char strbuf[80];

static uint8_t *char_font = (uint8_t *)vt100_font_8x8;
static uint32_t mode;

static volatile uint8_t *video_ram;
static volatile uint8_t *cart_d5xx;
static volatile uint32_t ticks;
static bool uart_init_once;

static uint8_t ego_cmd;
static uint32_t ego_state;
static uint32_t ego_shape_no;
static uint32_t ego_sprite_no;
static uint32_t ego_shape_width;
static uint32_t ego_shape_height;
static uint16_t ego_blitwidth;
static uint16_t ego_blitheight;

static shape_t shape_array[EGO_MAX_SHAPES];
static uint8_t *shape_ptr;
static sprite_t sprite_array[EGO_MAX_SPRITES];
static uint8_t *sprite_ptr;

static PIO pio = pio0;

static void systick_init(void)
{
    systick_hw->csr = 0x5;        // Enable + CPU-Clock als Quelle
    systick_hw->rvr = 0x00FFFFFF; // Maximaler Reload-Wert (24-Bit)
}

uint16_t __not_in_flash_func(get_blitwidth)()
{
    return ego_blitwidth;
}

void __not_in_flash_func(set_blitwidth)(uint16_t width)
{
    ego_blitwidth = width;
}

uint16_t __not_in_flash_func(get_blitheight)()
{
    return ego_blitheight;
}

void __not_in_flash_func(set_blitheight)(uint16_t height)
{
    ego_blitheight = height;
}


void __not_in_flash("sprite_draw_all") sprite_draw_all()
{
    for (int i = 0; i < EGO_MAX_SPRITES; i++)
    {
        sprite_draw(&sprite_array[i]);
    }
}


void __not_in_flash_func(sprite_draw)(sprite_t *sp)
{
    
    int xpos = sp->xpos;
    int ypos = sp->ypos;
    int width = shape_array[sp->shape_no].width;
    int height = shape_array[sp->shape_no].height;

    int pixx = xpos & 0x07;
    int bytex = xpos >> 3;

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

    ystart = height - ypos;
    if (ystart < 0)
        ystart = 0;

    ylen = height - ystart;

    vstart = ypos - height;
    if (vstart < 0)
        vstart = 0;
    if (vdiff < height)
    {
        ylen = vdiff;
    }

    vpos = &video_ram[vstart * ego_blitwidth];
    sppos = &(shape_array[sp->shape_no].data[ystart * width]);

    // ego_log("sprite xpos:%d ypos:%d bytex: %d pixx:%d ystart:%d ylen:%d\n", xpos, ypos, bytex, pixx, ystart, ylen);

    for (y = 0; y < ylen; y++)
    {
        c = 0;
        {
            if (bytex + x >= 0 && bytex + x < ego_blitwidth)
                vpos[bytex + x] ^= (c | (sppos[x] >> pixx));
            c = (sppos[x]) << (8 - pixx);
        }
        if (bytex + x >= 0 && bytex + x < ego_blitwidth)
            vpos[bytex + x] ^= c;

        vpos += ego_blitwidth;
        sppos += width;
    }

}

void __not_in_flash_func(render_sprites)(void)
{

    sprite_t *sp;

    for (int i = 0; i < EGO_MAX_SPRITES; i++)
    {
        sp = &sprite_array[i];

        if (sp->enabled) {
            sprite_draw(sp);
        }
    }
/*
        xpos = sp->xpos;
        ypos = sp->ypos;

        xpos += dx_array[i];
        if (xpos >= ego_blitwidth << 3 || xpos <= 0)
        {
            dx_array[i] *= -1;
            xpos += dx_array[i];
        }

        {
            dy_array[i] *= -1;
            ypos += dy_array[i];
        }

        sprite_draw_xy(sp, xpos, ypos);
    }

    // ego_log("ticks: %d\n", (ticks - systick_hw->cvr) & 0xffffff);
*/
}

void __not_in_flash_func(do_data)(uint8_t data)
{
    switch(ego_state)
    {
    case EGO_ST_SHAPE_NO:
        ego_shape_no = data;
        ego_state = EGO_ST_SHAPE_WIDTH;
        break;
    case EGO_ST_SHAPE_WIDTH:
        shape_array[ego_shape_no].width = data;
        ego_state = EGO_ST_SHAPE_HEIGHT;
        break;
    case EGO_ST_SHAPE_HEIGHT:
        shape_array[ego_shape_no].height = data;
        if (shape_array[ego_shape_no].data != NULL)  {
            free(shape_array[ego_shape_no].data);
        }
        shape_array[ego_shape_no].data = malloc(shape_array[ego_shape_no].width * shape_array[ego_shape_no].height);
        shape_array[ego_shape_no].count = 0;

        ego_state = EGO_ST_SHAPE_DATA;

        break;
    case EGO_ST_SHAPE_DATA:
        shape_array[ego_shape_no].data[shape_array[ego_shape_no].count] = data;
        shape_array[ego_shape_no].count++;

        if (shape_array[ego_shape_no].count == shape_array[ego_shape_no].width * shape_array[ego_shape_no].height) {
            ego_state = EGO_ST_IDLE;
        }
        break;
    case EGO_ST_SPRITE_NO:
        ego_sprite_no = data;

        if (ego_cmd == EGO_CMD_SET_SPRITE_X || ego_cmd == EGO_CMD_SET_SPRITE_XY)
            ego_state = EGO_ST_SPRITE_X_LO;
        else if (ego_cmd == EGO_CMD_SET_SPRITE_Y)
            ego_state = EGO_ST_SPRITE_Y_LO;
        else 
            ego_state = EGO_ST_SPRITE_SHAPE_NO;
        break;
    case EGO_ST_SPRITE_SHAPE_NO:
        sprite_array[ego_sprite_no].shape_no = data;
        ego_state = EGO_ST_IDLE;
        break;
    case EGO_ST_SPRITE_X_LO:
        sprite_array[ego_sprite_no].xold = sprite_array[ego_sprite_no].xpos;
        sprite_array[ego_sprite_no].xpos = data;        
        ego_state = EGO_ST_SPRITE_X_HI;
        break;
    case EGO_ST_SPRITE_X_HI:
        sprite_array[ego_sprite_no].xpos |= data << 8;
        if (ego_cmd == EGO_CMD_SET_SPRITE_XY)
            ego_state = EGO_ST_SPRITE_Y_LO;
        else
            ego_state = EGO_ST_IDLE;
        break;
    case EGO_ST_SPRITE_Y_LO:
        sprite_array[ego_sprite_no].yold = sprite_array[ego_sprite_no].ypos;
        sprite_array[ego_sprite_no].ypos = data;
        ego_state = EGO_ST_SPRITE_Y_HI;
        break;
    case EGO_ST_SPRITE_Y_HI:
        sprite_array[ego_sprite_no].ypos |= data << 8;
        ego_state = EGO_ST_IDLE;
        break;
    case EGO_ST_SPRITE_ENA:
        ego_sprite_no = data;
        sprite_array[ego_sprite_no].enabled = true;
        ego_state = EGO_ST_IDLE;
        break;
    case EGO_ST_SPRITE_DIS:
        ego_sprite_no = data;
        sprite_array[ego_sprite_no].enabled = false;
        ego_state = EGO_ST_IDLE;
        break;
    default:
        break;
    }
}

void __not_in_flash_func(do_command)(uint8_t data)
{
    ego_cmd = data;

    switch (ego_cmd)
    {
    case EGO_CMD_RENDER_SPRITES:
        render_sprites();
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
        ego_state = EGO_ST_IDLE;
        break;
    case EGO_CMD_SHAPE_DATA:
        ego_state = EGO_ST_SHAPE_NO;
        break;
    case EGO_CMD_DEL_SHAPE:
        break;
    case EGO_CMD_SPRITE_DATA:
        ego_state = EGO_ST_SPRITE_NO;
        break;
    case EGO_CMD_DEL_SPRITE:
        break;
    case EGO_CMD_ENA_SPRITE:
        ego_state = EGO_ST_SPRITE_ENA;
        break;
    case EGO_CMD_DIS_SPRITE:
        ego_state = EGO_ST_SPRITE_DIS;
        break;
    case EGO_CMD_SET_SPRITE_X:
        ego_state = EGO_ST_SPRITE_NO;
        break;
    case EGO_CMD_SET_SPRITE_Y:
        ego_state = EGO_ST_SPRITE_NO;
        break;
    case EGO_CMD_SET_SPRITE_XY:
        ego_state = EGO_ST_SPRITE_NO;
        break;
    default:
        break;
    }
}

void __not_in_flash_func(core1_main)()
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

void __not_in_flash_func(putChar)(int xpos, int ypos, uint8_t c)
{
    uint8_t volatile *pscreen = &video_ram[(ypos * ego_blitwidth * 8) + xpos];
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

void __not_in_flash_func(write_d5xx)(uint8_t addr, uint8_t data)
{

    cart_d5xx[EGO_REG_STATUS] |= 0x80;

    uint32_t msg = data << 16 | addr;
    multicore_fifo_push_blocking(msg);
}

void __not_in_flash_func(ego_log)(const char *format, ...)
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
        if (!uart_init_once)
        {
            uart_init_once = true;
            uart_init(EGO_UART_ID, EGO_BAUD_RATE);
            gpio_set_function(EGO_UART_TX_PIN, GPIO_FUNC_UART);
            gpio_set_function(EGO_UART_RX_PIN, GPIO_FUNC_UART);
        }

        uart_puts(EGO_UART_ID, strbuf);
    }
}

void test() {


    set_blitwidth(40);
    set_blitheight(192);

    for (int i = 0; i < EGO_MAX_SPRITES; i++)
    {

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
    while (true)
    {
        c = getchar();

        write_d5xx(0x00, 0x00);

        while (read_d5xx(0) & 0x80)
            ;
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

    uart_init_once = false;
    mode = EGO_MODE_USB;

    shape_ptr = NULL;
    sprite_ptr = NULL;
    
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


    multicore_launch_core1(core1_main);

    if (mode == EGO_MODE_ATARI)
    {
        atari_cart_main();
    }

    initialise_cvideo(pio0);

    test();
}
