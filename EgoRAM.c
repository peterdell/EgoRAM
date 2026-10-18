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
#include "hardware/structs/bus_ctrl.h"
#include "hardware/regs/busctrl.h"

#include "vt100_font_8x8.h"
#include "atari_cart.h"
#include "EgoRAM.h"
#include "cvideo.h"

char c;

int dx_array[EGO_MAX_SPRITES];
int dy_array[EGO_MAX_SPRITES];

static char strbuf[256];

static uint8_t *char_font = (uint8_t *)vt100_font_8x8;
static EgoMode mode;

static volatile uint8_t *video_ram;
// static volatile uint8_t *char_ram;
static volatile uint8_t *cart_d5xx;
static volatile uint32_t ticks;
static bool uart_init_once;

static EgoCmd ego_cmd;
static EgoState ego_state;
static uint8_t ego_params[16];
static uint8_t ego_param_idx;
static uint8_t *ego_in_ptr;
static uint32_t ego_in_len;
static void (*ego_in_done)(void);
static const uint8_t *ego_out_ptr;
static uint32_t ego_out_len;
static uint32_t ego_line_no;
static uint32_t ego_charset_no;
static uint16_t ego_blitwidth;
static uint16_t ego_blitheight;
static uint16_t ego_line_ptr[256];
static uint8_t ego_line_staging[512];
static uint16_t ego_src_ptr;
static uint16_t ego_dst_ptr;
static uint16_t ego_src_inc;
static uint16_t ego_dst_inc;
static uint8_t ego_chr2gfx_width;
static uint8_t ego_chr2gfx_height;
static uint8_t ego_chr2gfx_scroll;
static uint8_t ego_chr2gfx_col38;

static shape_t shape_array[EGO_MAX_SHAPES];
static sprite_t sprite_array[EGO_MAX_SPRITES];
static uint8_t *charset_array[256];

static PIO pio = pio0;

static uint8_t fill_front[8] = {
    0x00,
    0x00,
    0xc0,
    0xc0,
    0xf0,
    0xf0,
    0xfc,
    0xfc};

static uint8_t fill_back[8] = {
    0xff,
    0xff,
    0x3f,
    0x3f,
    0x0f,
    0x0f,
    0x03,
    0x03};

static uint8_t manta[] = {
    0x00, 0x00, 0x00, 0x0F, 0xFF, 0x00, 0x3D, 0x57, 0xFF, 0x37, 0xFD, 0xD5, 0xF7, 0xD5, 0x70, 0xDF, 0x7F, 0xFC, 0xDD, 0xFA, 0xBC, 0xDD, 0xEA, 0xEC, 0xD5, 0xFF, 0xAD, 0xFF, 0xEB, 0xAF, 0xD5, 0xEB, 0xFD, 0xFF, 0xEB, 0xAF, 0xD5, 0xFF, 0xAD, 0xDD, 0xEA, 0xEC, 0xDD, 0xFA, 0xBC, 0xDF, 0x7F, 0xFC, 0xF7, 0xD5, 0x70, 0x37, 0xFD, 0xFF, 0x3D, 0x57, 0xD5, 0x0F, 0xFF, 0x00, 0x00, 0x00, 0x00, 0x00};

static void systick_init(void)
{
    systick_hw->csr = 0x5;        // Enable + CPU-Clock als Quelle
    systick_hw->rvr = 0x00FFFFFF; // Maximaler Reload-Wert (24-Bit)
}

uint16_t RAM_FUNC get_blitwidth()
{
    return ego_blitwidth;
}

void RAM_FUNC set_blitwidth(uint16_t width)
{
    ego_blitwidth = width;
}

uint16_t RAM_FUNC get_blitheight()
{
    return ego_blitheight;
}

void RAM_FUNC set_blitheight(uint16_t height)
{
    ego_blitheight = height;
}

uint8_t RAM_FUNC read_vram(uint16_t addr)
{
    return video_ram[addr];
}

void RAM_FUNC write_vram(uint16_t addr, uint8_t data)
{
    if (addr >= 0x1fff)
    {
        ego_log("error vram addr: %d\n", addr);
    }
    else
    {
        video_ram[addr] = data;
    }
}

void RAM_FUNC sprite_draw(sprite_t *sp)
{

    int xpos = sp->xpos;
    int ypos = sp->ypos;
    int shape_no = sp->shape_no;

    int width = shape_array[shape_no].width;
    int height = shape_array[shape_no].height;
    int bpp = shape_array[shape_no].bitsperpix;

    int pixx = (xpos << (bpp - 1)) & 0x07;
    int bytex = (xpos >> (4 - bpp));

    int xstart, ystart;
    int xvstart, vstart;
    int xlen, ylen;
    int i, x, y;

    uint8_t mask;
    uint8_t shadow;
    uint16_t vpos;
    uint8_t *sppos, *mpos;
    uint8_t b, c, m, data;

    // ego_log("blitwidth: %d, blitheight %d\n", ego_blitwidth, ego_blitheight);
    // ego_log("sprite_draw: shape_no:%d, x:%d y:%d, width:%d, height: %d, bpp: %d\n", shape_no, xpos, ypos, width, height, bpp);

    if (bytex >= ego_blitwidth + width || ypos > ego_blitheight + height)
    {
        return;
    }

    //
    // compute once the "and" mask for shapes having bitsperpixel == 2
    //
    if (shape_array[shape_no].bitsperpix == 2 && shape_array[shape_no].mask == NULL)
    {
        shape_array[shape_no].mask = malloc(width * height);
        shape_array[shape_no].shadow = malloc(width * height);

        vpos = 0;
        for (y = 0; y < height; y++)
        {
            for (x = 0; x < width; x++)
            {
                mask = 0xc0;
                shadow = 0x80;

                data = shape_array[shape_no].data[vpos];
                shape_array[shape_no].mask[vpos] = 0;
                shape_array[shape_no].shadow[vpos] = 0;

                for (i = 0; i < 4; i++)
                {
                    if (!(data & mask))
                    {
                        shape_array[shape_no].mask[vpos] |= mask;
                    }
                    else
                    {
                        shape_array[shape_no].shadow[vpos] |= shadow;
                    }
                    mask >>= 2;
                    shadow >>= 2;
                }
                // ego_log("%02X ", shape_array[shape_no].mask[vpos]);
                vpos++;
            }
            // ego_log("\n");
        }
    }

    // vertical start in shape data (top bound)
    ystart = height - ypos;
    if (ystart < 0)
        ystart = 0;

    // vertical number of lines of shape data
    ylen = height - ystart;

    // check bottom bound
    if (ypos >= ego_blitheight)
    {
        ylen = ego_blitheight + height - ypos;
    }

    // line of screen RAM
    vstart = ypos - height;
    if (vstart < 0)
        vstart = 0;

    xstart = width - bytex;
    if (xstart < 0)
        xstart = 0;
    xlen = width - xstart;

    xvstart = bytex - width;
    if (xvstart < 0)
        xvstart = 0;

    if (bytex > ego_blitwidth)
        xlen = bytex - ego_blitwidth;

    if (sp->mode == EGO_MODE_SPECIAL)
    {
        sppos = &(shape_array[shape_no].shadow[ystart * width + xstart]);
    }
    else
    {
        sppos = &(shape_array[shape_no].data[ystart * width + xstart]);
    }
    mpos = &(shape_array[shape_no].mask[ystart * width + xstart]);

    // ego_log("sprite bytex:%d pixx:%d xstart:%d xlen:%d xvstart:%d ystart:%d ylen:%d vstart:%d mpos: %p sppos: %p\n", bytex, pixx, xstart, xlen, xvstart, ystart, ylen, vstart, mpos, sppos);

    for (y = 0; y < ylen; y++)
    {
        vpos = ego_line_ptr[vstart + y] + xvstart;

        c = 0;
        m = fill_front[pixx];
        if (xstart > 0)
        {
            c = (sppos[-1]) << (8 - pixx);
            m = (mpos[-1]) << (8 - pixx);
        }

        for (x = 0; x < xlen; x++)
        {
            b = read_vram(vpos + x);

            if (sp->mode == EGO_MODE_XOR)
            {
                b ^= (c | (sppos[x] >> pixx));
            }
            else
            {
                if (sp->mode == EGO_MODE_MASK)
                {
                    b &= (m | (mpos[x] >> pixx));
                    b |= (c | (sppos[x] >> pixx));
                }
                else
                {
                    mask = 0xc0;
                    c |= (sppos[x] >> pixx);
                    for (i = 0; i < 4; i++)
                    {
                        if (!(b & mask))
                        {
                            c &= ~mask;
                        }
                        mask >>= 2;
                    }
                    b &= (m | (mpos[x] >> pixx));
                    b |= c;
                }
            }

            write_vram(vpos + x, b);

            c = (sppos[x]) << (8 - pixx);
            m = (mpos[x]) << (8 - pixx);
        }

        if (pixx != 0 && bytex < ego_blitwidth)
        {
            b = read_vram(vpos + x);
            if (sp->mode == EGO_MODE_XOR)
            {
                b ^= c;
            }
            else
            {
                if (sp->mode == EGO_MODE_SPECIAL)
                {
                    mask = 0xc0;
                    for (i = 0; i < 4; i++)
                    {
                        if (!(b & mask))
                        {
                            c &= ~mask;
                        }
                        mask >>= 2;
                    }
                }
                b &= (m | fill_back[pixx]);
                b |= c;
            }
            write_vram(vpos + x, b);
        }

        sppos += width;
        mpos += width;
    }
}

void RAM_FUNC render_sprites(void)
{

    sprite_t *sp;

    for (int i = EGO_MAX_SPRITES - 1; i >= 0; i--)
    {
        sp = &sprite_array[i];

        if (sp->enabled)
        {
            // ego_log("calling sprite_draw sprite: %d\n", i);
            sprite_draw(sp);
        }
    }
}

void do_movement()
{
    sprite_t *sp;
    uint16_t xpos, ypos;

    for (int i = 0; i < EGO_MAX_SPRITES; i++)
    {
        sp = &sprite_array[i];

        if (sp->enabled)
        {
            sprite_draw(sp);

            xpos = sp->xpos;
            ypos = sp->ypos;

            xpos += dx_array[i];
            if (xpos >= (ego_blitwidth - shape_array[sp->shape_no].width) << 3 || xpos <= 0)
            {
                dx_array[i] *= -1;
                xpos += dx_array[i];
            }

            ypos += dy_array[i];
            if (ypos >= ego_blitheight || ypos - shape_array[sp->shape_no].height <= 0)
            {
                dy_array[i] *= -1;
                ypos += dy_array[i];
            }

            sp->xpos = xpos;
            sp->ypos = ypos;

            sprite_draw(sp);
        }
    }

    cart_d5xx[EGO_REG_STATUS] &= ~0x80;
}

void RAM_FUNC char_to_video()
{
    int i, x, y;
    int vypos;
    int line;
    uint16_t src_pos;
    uint16_t dst_pos;
    uint8_t b, c;

    src_pos = ego_src_ptr & 0x3fff;
    dst_pos = ego_dst_ptr & 0x3fff;

    // ego_log("char2gfx src:%04X, inc:%04X, dst:%04X, inc:%04X, width:%02X, heigth:%02X, charset:%02X, scroll:%02X\n", src_pos, ego_src_inc, dst_pos, ego_dst_inc, ego_chr2gfx_width, ego_chr2gfx_height, ego_charset_no, ego_chr2gfx_scroll);

    for (y = 0; y < ego_chr2gfx_height; y++)
    {
        for (line = 0; line < 8; line++)
        {
            b = 0;
            for (x = 0; x < ego_chr2gfx_width; x++)
            {
                c = charset_array[ego_charset_no][(video_ram[src_pos + x] << 3) + line];
                b |= (c >> ego_chr2gfx_scroll);
                video_ram[dst_pos + line * ego_dst_inc + x] = b;
                b = c << (8 - ego_chr2gfx_scroll);
            }
            if (ego_chr2gfx_col38)
            {
                video_ram[dst_pos + line * ego_dst_inc] = 0;
                video_ram[dst_pos + line * ego_dst_inc + 39] = 0;
            }
        }
        src_pos += ego_src_inc;
        dst_pos += (ego_dst_inc << 3);
    }
}

void print_debug()
{
    sprite_t *sp;
    int i;

    for (i = EGO_MAX_SPRITES - 1; i >= 0; i--)
    {
        sp = &sprite_array[i];

        if (sp->enabled)
        {
            ego_log("sprite:%d, mode:%d, shape:%d, xpos:%d, ypos:%d\n", i, sp->mode, sp->shape_no, sp->xpos, sp->ypos);
        }
    }
}

static void RAM_FUNC stream_in(uint8_t *dst, uint32_t len, void (*done)(void))
{
    if (len == 0)
        return;
    ego_in_ptr = dst;
    ego_in_len = len;
    ego_in_done = done;
    ego_state = EGO_ST_DATA_IN;
}

static void RAM_FUNC cmd_render_sprites(const uint8_t *p)
{
    render_sprites();
}

static void RAM_FUNC cmd_set_writeable(const uint8_t *p)
{
    set_writeable(true);
    cart_d5xx[EGO_REG_STATUS] |= 0x01;
}

static void RAM_FUNC cmd_set_readonly(const uint8_t *p)
{
    set_writeable(false);
    cart_d5xx[EGO_REG_STATUS] &= ~0x01;
}

static void RAM_FUNC cmd_abort(const uint8_t *p)
{
}

static void RAM_FUNC cmd_shape_data(const uint8_t *p)
{
    shape_t *shape = &shape_array[p[0]];

    shape->width = p[1];
    shape->height = p[2];
    shape->bitsperpix = p[3];
    if (shape->data != NULL)
    {
        free(shape->data);
    }
    if (shape->mask != NULL)
    {
        free(shape->mask);
        shape->mask = NULL;
    }
    shape->data = malloc(shape->width * shape->height);
    stream_in(shape->data, shape->width * shape->height, NULL);
}

static void RAM_FUNC cmd_sprite_data(const uint8_t *p)
{
    sprite_array[p[0]].shape_no = p[1];
}

static void RAM_FUNC cmd_enable_sprite(const uint8_t *p)
{
    sprite_array[p[0]].enabled = true;
}

static void RAM_FUNC cmd_disable_sprite(const uint8_t *p)
{
    sprite_array[p[0]].enabled = false;
}

static void RAM_FUNC cmd_set_sprite_x(const uint8_t *p)
{
    sprite_t *sprite = &sprite_array[p[0]];

    sprite->xold = sprite->xpos;
    sprite->xpos = p[1] | p[2] << 8;
}

static void RAM_FUNC cmd_set_sprite_y(const uint8_t *p)
{
    sprite_t *sprite = &sprite_array[p[0]];

    sprite->yold = sprite->ypos;
    sprite->ypos = p[1] | p[2] << 8;
}

static void RAM_FUNC cmd_set_sprite_xy(const uint8_t *p)
{
    sprite_t *sprite = &sprite_array[p[0]];

    sprite->xold = sprite->xpos;
    sprite->xpos = p[1] | p[2] << 8;
    sprite->yold = sprite->ypos;
    sprite->ypos = p[3] | p[4] << 8;
}

static void RAM_FUNC cmd_set_blit_width(const uint8_t *p)
{
    ego_blitwidth = p[0];
}

static void RAM_FUNC cmd_set_blit_height(const uint8_t *p)
{
    ego_blitheight = p[0];
}

static void RAM_FUNC line_ptr_done(void)
{
    for (uint32_t i = 0; i < ego_line_no; i++)
    {
        ego_line_ptr[i] = ego_line_staging[2 * i] | (ego_line_staging[2 * i + 1] & 0x1f) << 8;
    }
}

static void RAM_FUNC cmd_line_ptr(const uint8_t *p)
{
    ego_line_no = p[0];
    stream_in(ego_line_staging, ego_line_no * 2, line_ptr_done);
}

static void RAM_FUNC cmd_fill_mem(const uint8_t *p)
{
    uint16_t addr = p[0] | (p[1] & 0x3f) << 8;
    uint16_t len = p[2] | p[3] << 8;

    if (addr + len > 0x3fff)
    {
        len = 0x3fff - addr;
    }
    memset((void *)&video_ram[addr], p[4], len);
}

static void RAM_FUNC cmd_sprite_mode(const uint8_t *p)
{
    sprite_array[p[0]].mode = p[1];
}

static void RAM_FUNC cmd_charset(const uint8_t *p)
{
    ego_charset_no = p[0];
    if (charset_array[ego_charset_no] == NULL)
    {
        charset_array[ego_charset_no] = malloc(2048);
    }
    stream_in(charset_array[ego_charset_no], 2048, NULL);
}

static void RAM_FUNC cmd_char_to_video(const uint8_t *p)
{
    ego_src_ptr = p[0] | p[1] << 8;
    ego_src_inc = p[2] | p[3] << 8;
    ego_dst_ptr = p[4] | p[5] << 8;
    ego_dst_inc = p[6] | p[7] << 8;
    ego_chr2gfx_width = p[8];
    ego_chr2gfx_height = p[9];
    ego_charset_no = p[10];
    ego_chr2gfx_scroll = p[11] & 0x07;
    ego_chr2gfx_col38 = p[12];
    char_to_video();
}

static void RAM_FUNC cmd_set_char(const uint8_t *p)
{
    ego_charset_no = p[0];
    stream_in(&charset_array[ego_charset_no][p[1] << 3], 8, NULL);
}

static void RAM_FUNC cmd_get_char(const uint8_t *p)
{
    const uint8_t *char_data = &charset_array[p[0]][p[1] << 3];

    cart_d5xx[EGO_REG_DATA] = char_data[0];
    ego_out_ptr = &char_data[1];
    ego_out_len = 7;
    ego_state = EGO_ST_DATA_OUT;
}

static void RAM_FUNC cmd_debug(const uint8_t *p)
{
    print_debug();
}

typedef struct
{
    uint8_t header_len;
    void (*execute)(const uint8_t *params);
} ego_cmd_desc_t;

// Explicitly placed in RAM so that command dispatch never stalls on flash XIP.
static const ego_cmd_desc_t __not_in_flash("ego_commands") ego_commands[] = {
    [EGO_CMD_RENDER_SPRITES] = {0, cmd_render_sprites},
    [EGO_CMD_SET_WRITEABLE] = {0, cmd_set_writeable},
    [EGO_CMD_SET_READONLY] = {0, cmd_set_readonly},
    [EGO_CMD_ABORT] = {0, cmd_abort},
    [EGO_CMD_SHAPE_DATA] = {4, cmd_shape_data},
    [EGO_CMD_SPRITE_DATA] = {2, cmd_sprite_data},
    [EGO_CMD_ENA_SPRITE] = {1, cmd_enable_sprite},
    [EGO_CMD_DIS_SPRITE] = {1, cmd_disable_sprite},
    [EGO_CMD_SET_SPRITE_X] = {3, cmd_set_sprite_x},
    [EGO_CMD_SET_SPRITE_Y] = {3, cmd_set_sprite_y},
    [EGO_CMD_SET_SPRITE_XY] = {5, cmd_set_sprite_xy},
    [EGO_CMD_SET_BLIT_WIDTH] = {1, cmd_set_blit_width},
    [EGO_CMD_SET_BLIT_HEIGHT] = {1, cmd_set_blit_height},
    [EGO_CMD_LINE_PTR] = {1, cmd_line_ptr},
    [EGO_CMD_FILL_MEM] = {5, cmd_fill_mem},
    [EGO_CMD_SPRITE_MODE] = {2, cmd_sprite_mode},
    [EGO_CMD_CHARSET] = {1, cmd_charset},
    [EGO_CMD_CHAR_TO_VIDEO] = {13, cmd_char_to_video},
    [EGO_CMD_SET_CHAR] = {2, cmd_set_char},
    [EGO_CMD_GET_CHAR] = {2, cmd_get_char},
    [EGO_CMD_DEBUG] = {0, cmd_debug},
};

void RAM_FUNC do_data(uint8_t data)
{
    switch (ego_state)
    {
    case EGO_ST_HEADER:
        ego_params[ego_param_idx++] = data;
        if (ego_param_idx == ego_commands[ego_cmd].header_len)
        {
            ego_state = EGO_ST_IDLE; // execute() may start a data phase
            ego_commands[ego_cmd].execute(ego_params);
        }
        break;
    case EGO_ST_DATA_IN:
        *ego_in_ptr++ = data;
        if (--ego_in_len == 0)
        {
            ego_state = EGO_ST_IDLE;
            if (ego_in_done != NULL)
            {
                ego_in_done();
            }
        }
        break;
    default:
        break;
    }
}

void RAM_FUNC do_command(uint8_t data)
{
    ego_cmd = data;
    ego_param_idx = 0;

    if (data >= sizeof(ego_commands) / sizeof(ego_commands[0]) || ego_commands[data].execute == NULL)
    {
        ego_state = EGO_ST_IDLE;
        return;
    }

    if (ego_commands[data].header_len == 0)
    {
        ego_state = EGO_ST_IDLE; // execute() may start a data phase
        ego_commands[data].execute(NULL);
    }
    else
    {
        ego_state = EGO_ST_HEADER;
    }
}

void RAM_FUNC core1_main()
{
    uint32_t msg;
    uint8_t data;
    uint16_t addr;

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
            /*
            if (data == EGO_CMD_RENDER_SPRITES)
            {
                ticks = (ticks - systick_hw->cvr) & 0xffffff;
                ego_log("addr: %04X, data: %02X, ticks: %d\n", msg & 0xffff, msg >> 16, ticks);
            }
            */
            break;
        case EGO_REG_DATA:
            do_data(data);
            break;
        default:
            break;
        }

        cart_d5xx[EGO_REG_STATUS] &= ~0x80;
    }
}

void RAM_FUNC putChar(int xpos, int ypos, uint8_t c)
{
    // ego_log("%p %02X ", (ypos * ego_blitwidth * 8) + xpos, c);

    uint8_t volatile *pscreen = &video_ram[(ypos * ego_blitwidth * 8) + xpos];
    uint8_t *pchar = &char_font[c << 3];

    for (int i = 0; i < 8; i++)
    {
        *pscreen = *pchar;
        pchar++;
        pscreen += ego_blitwidth;
    }
}

uint8_t read_d5xx(uint8_t addr)
{

    switch (ego_state)
    {
    case EGO_ST_DATA_OUT:
        cart_d5xx[addr] = *ego_out_ptr++;
        if (--ego_out_len == 0)
            ego_state = EGO_ST_IDLE;
        break;
    default:
        break;
    }

    return cart_d5xx[addr];
}

void RAM_FUNC write_d5xx(uint8_t addr, uint8_t data)
{
    cart_d5xx[EGO_REG_STATUS] |= 0x80;

    if (multicore_fifo_wready)
    {
        multicore_fifo_push_blocking(addr | (data << 16));
    }
    else
    {
        ego_log("write to FiFo failed\n");
    }
}

void RAM_FUNC ego_log(const char *format, ...)
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

void test()
{
    int width = 40;
    int height = 200;
    uint16_t xpos = 20;
    uint16_t ypos = 20;

    int x, y, c;
    uint16_t ptr = 0;

    write_d5xx(EGO_REG_CMD, EGO_CMD_SET_BLIT_WIDTH);
    write_d5xx(EGO_REG_DATA, (uint8_t)width);
    write_d5xx(EGO_REG_CMD, EGO_CMD_SET_BLIT_HEIGHT);
    write_d5xx(EGO_REG_DATA, (uint8_t)height);

    write_d5xx(EGO_REG_CMD, EGO_CMD_LINE_PTR);
    write_d5xx(EGO_REG_DATA, 200);
    for (x = 0; x < 200; x++)
    {
        write_d5xx(EGO_REG_DATA, ptr & 0xff);
        write_d5xx(EGO_REG_DATA, ptr >> 8);
        ptr += width;
    }

    for (x = 0; x < EGO_MAX_SPRITES; x++)
    {
        dx_array[x] = 1;
        if (get_rand_32() & 0x01)
            dx_array[x] *= -1;

        dy_array[x] = 1;
        if (get_rand_32() & 0x01)
            dy_array[x] *= -1;
    }

    c = 0;
    for (y = 0; y < (height >> 3); y++)
    {
        for (x = 0; x < width; x++)
        {
            putChar(x, y, c);
            // c++;
        }
    }

    write_d5xx(EGO_REG_CMD, EGO_CMD_SPRITE_MODE);
    write_d5xx(EGO_REG_DATA, x); // sprite number
    write_d5xx(EGO_REG_DATA, 1); // sprite number

    write_d5xx(EGO_REG_CMD, EGO_CMD_SHAPE_DATA);
    write_d5xx(EGO_REG_DATA, 0x00); // shape number
    write_d5xx(EGO_REG_DATA, 3);    // shape width
    write_d5xx(EGO_REG_DATA, 21);   // shape height
    write_d5xx(EGO_REG_DATA, 2);    // shape bpp

    for (y = 0; y < 21; y++)
    {
        for (x = 0; x < 3; x++)
        {
            write_d5xx(EGO_REG_DATA, manta[y * 3 + x]);
        }
    }

    /*
        write_d5xx(EGO_REG_DATA, 0xff); // shape data
        write_d5xx(EGO_REG_DATA, 0xff); // shape data
        write_d5xx(EGO_REG_DATA, 0xff); // shape data
        write_d5xx(EGO_REG_DATA, 0xff); // shape data
        write_d5xx(EGO_REG_DATA, 0xff); // shape data
        write_d5xx(EGO_REG_DATA, 0xff); // shape data
        write_d5xx(EGO_REG_DATA, 0xff); // shape data
        write_d5xx(EGO_REG_DATA, 0xff); // shape data
        write_d5xx(EGO_REG_DATA, 0xff); // shape data
        write_d5xx(EGO_REG_DATA, 0xff); // shape data
        write_d5xx(EGO_REG_DATA, 0xff); // shape data
        write_d5xx(EGO_REG_DATA, 0xff); // shape data
        write_d5xx(EGO_REG_DATA, 0xff); // shape data
        write_d5xx(EGO_REG_DATA, 0xff); // shape data
        write_d5xx(EGO_REG_DATA, 0xff); // shape data
        write_d5xx(EGO_REG_DATA, 0xff); // shape data
    */

    for (x = 0; x < EGO_MAX_SPRITES; x++)
    {
        write_d5xx(EGO_REG_CMD, EGO_CMD_SPRITE_DATA);
        write_d5xx(EGO_REG_DATA, x);    // sprite number
        write_d5xx(EGO_REG_DATA, 0x00); // shape number

        write_d5xx(EGO_REG_CMD, EGO_CMD_SET_SPRITE_XY);
        write_d5xx(EGO_REG_DATA, x);                            // sprite number
        write_d5xx(EGO_REG_DATA, get_rand_32() % (width << 3)); // sprite X lo
        write_d5xx(EGO_REG_DATA, 0);                            // sprite X hi
        // write_d5xx(EGO_REG_DATA, xpos & 0xff);                  // sprite X lo
        // write_d5xx(EGO_REG_DATA, xpos >> 8);                    // sprite X hi
        write_d5xx(EGO_REG_DATA, get_rand_32() % height); // sprite Y lo
        write_d5xx(EGO_REG_DATA, 0);                      // sprite Y hi
        // write_d5xx(EGO_REG_DATA, ypos & 0xff);                  // sprite Y lo
        // write_d5xx(EGO_REG_DATA, ypos >> 8);                    // sprite Y hi

        write_d5xx(EGO_REG_CMD, EGO_CMD_ENA_SPRITE);
        write_d5xx(EGO_REG_DATA, x); // sprite number

        write_d5xx(EGO_REG_CMD, EGO_CMD_SPRITE_MODE);
        write_d5xx(EGO_REG_DATA, x); // sprite number
        write_d5xx(EGO_REG_DATA, 1); // sprite number
    }

    write_d5xx(EGO_REG_CMD, EGO_CMD_RENDER_SPRITES);

    while (true)
    {
        c = getchar();
        printf("%c", c);

        write_d5xx(EGO_REG_CMD, EGO_CMD_RENDER_SPRITES);
        while (read_d5xx(EGO_REG_STATUS & 0x80))
            ;

        switch (c)
        {
        case 's':
            ypos--;
            break;
        case 'x':
            ypos++;
            break;
        case 'a':
            xpos--;
            break;
        case 'd':
            xpos++;
            break;
        default:
            break;
        }

        write_d5xx(EGO_REG_CMD, EGO_CMD_SET_SPRITE_XY);
        write_d5xx(EGO_REG_DATA, 0);
        write_d5xx(EGO_REG_DATA, xpos & 0xff);
        write_d5xx(EGO_REG_DATA, xpos >> 8);
        write_d5xx(EGO_REG_DATA, ypos & 0xff);
        write_d5xx(EGO_REG_DATA, ypos >> 8);

        write_d5xx(EGO_REG_CMD, EGO_CMD_RENDER_SPRITES);
        while (read_d5xx(EGO_REG_STATUS & 0x80))
            ;
    }
}

void set_core0_highest_bus_priority()
{
    // Setzt die Bus-Priorität für Kern 0 auf hoch (1) und Kern 1 auf niedrig (0)
    bus_ctrl_hw->priority = BUSCTRL_BUS_PRIORITY_PROC0_BITS;
}

int main()
{
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

    while (to_ms_since_boot(get_absolute_time()) < 100)
    {
        if (!gpio_get(ATARI_PHI2_PIN))
        {
            mode = EGO_MODE_ATARI;
        }
    }

    video_ram = get_cart_ram();
    //   char_ram = video_ram + 0x3800;
    cart_d5xx = get_cart_d5xx();

    // memset((void *)video_ram, 0x01, 4 * 1024);
    // memset((void *)video_ram + 0x1000, 0x10, 4 * 1024);
    memset((void *)video_ram + 0x2000, 0xaa, 8 * 1024);

    memset((void *)sprite_array, 0x00, sizeof(sprite_array));
    memset((void *)shape_array, 0x00, sizeof(shape_array));

    ego_log("\033[2J\033[HEgoRAM starting...\n");
    if (mode == EGO_MODE_USB)
        ego_log("mode: USB\n");
    else
        ego_log("mode: ATARI\n");

    ego_log("sys_clock: %d\n", clock_get_hz(clk_sys));
    ego_log("video_ram at %p\n", video_ram);
    ego_log("cart_d5xx at %p\n", cart_d5xx);

    for (int x = 0; x < EGO_MAX_SPRITES; x++)
    {
        dx_array[x] = 1;
        if (get_rand_32() & 0x01)
            dx_array[x] *= -1;

        dy_array[x] = 1;
        if (get_rand_32() & 0x01)
            dy_array[x] *= -1;
    }

    multicore_launch_core1(core1_main);

    if (mode == EGO_MODE_ATARI)
    {
        // irq_set_enabled(USBCTRL_IRQ, false);
        uint32_t status = save_and_disable_interrupts();
        set_core0_highest_bus_priority();
        atari_cart_main();
    }

    initialise_cvideo(pio0);

    test();
}
