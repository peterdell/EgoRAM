// Graphics device (EGO_DEVICE_GRAPHICS) of the EgoRAM cartridge: shapes,
// sprites, charsets and character-to-bitmap rendering. Everything except the
// gfx_device descriptor and the functions in ego_gfx.h is private to this
// compile unit.

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "pico/stdlib.h"
#include "pico/rand.h"

#include "vt100_font_8x8.h"
#include "atari_cart.h"
#include "EgoRAM.h"
#include "ego_gfx.h"

#define EGO_MAX_SPRITES 64
#define EGO_MAX_SHAPES 256

typedef struct {
    uint8_t width;
    uint8_t height;
    uint8_t bitsperpix;
    uint8_t *data;
    uint8_t *mask;
    uint8_t *shadow;
} shape_t;

typedef struct {
    uint8_t enabled;
    EgoSpriteMode mode;
    uint16_t xpos;
    uint16_t ypos;
    uint8_t shape_no;
    uint16_t xold;
    uint16_t yold;
} sprite_t;

// State of the graphics device.
typedef struct {
    uint16_t blitwidth;        // blit area width in bytes
    uint16_t blitheight;       // blit area height in lines
    uint32_t line_no;          // line count of a pending LINE_PTR stream
    uint16_t line_ptr[256];    // video RAM offset of each screen line
    uint8_t line_staging[512]; // LINE_PTR DATA_IN buffer (lo/hi pairs)
    shape_t shapes[EGO_MAX_SHAPES];
    sprite_t sprites[EGO_MAX_SPRITES];
    uint8_t *charsets[256];
} ego_gfx_t;

static ego_gfx_t ego_gfx;

static volatile uint8_t *video_ram;
static volatile uint8_t *cart_d5xx;

static uint8_t *char_font = (uint8_t *)vt100_font_8x8;

static int dx_array[EGO_MAX_SPRITES];
static int dy_array[EGO_MAX_SPRITES];

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

#pragma region Command parameters

// Wire-format parameter blocks, one per command with parameters. Each struct
// mirrors the exact byte sequence the Atari writes to EGO_REG_DATA after the
// command byte (16-bit values lo byte first, matching the little-endian
// RP2040), so the handlers overlay them on the parser's parameter buffer
// instead of unpacking indexed bytes. The command table takes header_len from
// sizeof(), which keeps the expected byte count and the declared layout in
// sync.
#define EGO_PACKED __attribute__((packed))

typedef struct EGO_PACKED {
    uint8_t shape_no;
    uint8_t width;  // in bytes
    uint8_t height; // in lines
    uint8_t bits_per_pixel;
    // followed by width * height data bytes (DATA_IN stream)
} ego_cmd_shape_data_params_t;

typedef struct EGO_PACKED {
    uint8_t sprite_no;
    uint8_t shape_no;
} ego_cmd_sprite_data_params_t;

// ENA_SPRITE and DIS_SPRITE
typedef struct EGO_PACKED {
    uint8_t sprite_no;
} ego_cmd_sprite_no_params_t;

typedef struct EGO_PACKED {
    uint8_t sprite_no;
    uint16_t x;
} ego_cmd_set_sprite_x_params_t;

typedef struct EGO_PACKED {
    uint8_t sprite_no;
    uint16_t y;
} ego_cmd_set_sprite_y_params_t;

typedef struct EGO_PACKED {
    uint8_t sprite_no;
    uint16_t x;
    uint16_t y;
} ego_cmd_set_sprite_xy_params_t;

typedef struct EGO_PACKED {
    uint8_t width; // in bytes
} ego_cmd_set_blit_width_params_t;

typedef struct EGO_PACKED {
    uint8_t height; // in lines
} ego_cmd_set_blit_height_params_t;

typedef struct EGO_PACKED {
    uint8_t line_count;
    // followed by line_count lo/hi line pointer pairs (DATA_IN stream),
    // high bytes masked with 0x1f
} ego_cmd_line_ptr_params_t;

typedef struct EGO_PACKED {
    uint16_t addr; // offset in video RAM, masked with 0x3fff by the handler
    uint16_t length;
    uint8_t value;
} ego_cmd_fill_mem_params_t;

typedef struct EGO_PACKED {
    uint8_t sprite_no;
    EgoSpriteMode mode;
} ego_cmd_sprite_mode_params_t;

typedef struct EGO_PACKED {
    uint8_t charset_no;
    // followed by 2048 charset data bytes (DATA_IN stream)
} ego_cmd_charset_params_t;

typedef struct EGO_PACKED {
    uint16_t src;     // offset in video RAM, masked with 0x3fff
    uint16_t src_inc; // source bytes per character row
    uint16_t dst;     // offset in video RAM, masked with 0x3fff
    uint16_t dst_inc; // destination bytes per scan line
    uint8_t width;    // in characters
    uint8_t height;   // in character rows
    uint8_t charset_no;
    uint8_t scroll;   // horizontal scroll in pixels, masked with 0x07
    uint8_t col38;    // nonzero blanks columns 0 and 39 (38-column mode)
} ego_cmd_char_to_video_params_t;

typedef struct EGO_PACKED {
    uint8_t charset_no;
    uint8_t char_no;
    // followed by 8 character data bytes (DATA_IN stream)
} ego_cmd_set_char_params_t;

typedef struct EGO_PACKED {
    uint8_t charset_no;
    uint8_t char_no;
    // answered with 8 character data bytes (DATA_OUT stream)
} ego_cmd_get_char_params_t;

// The struct sizes are the commands' wire header lengths; a mismatch means
// padding or an enum-size surprise broke the overlay.
_Static_assert(sizeof(ego_cmd_shape_data_params_t) == 4, "wire layout must match asm client");
_Static_assert(sizeof(ego_cmd_sprite_data_params_t) == 2, "wire layout must match asm client");
_Static_assert(sizeof(ego_cmd_sprite_no_params_t) == 1, "wire layout must match asm client");
_Static_assert(sizeof(ego_cmd_set_sprite_x_params_t) == 3, "wire layout must match asm client");
_Static_assert(sizeof(ego_cmd_set_sprite_y_params_t) == 3, "wire layout must match asm client");
_Static_assert(sizeof(ego_cmd_set_sprite_xy_params_t) == 5, "wire layout must match asm client");
_Static_assert(sizeof(ego_cmd_set_blit_width_params_t) == 1, "wire layout must match asm client");
_Static_assert(sizeof(ego_cmd_set_blit_height_params_t) == 1, "wire layout must match asm client");
_Static_assert(sizeof(ego_cmd_line_ptr_params_t) == 1, "wire layout must match asm client");
_Static_assert(sizeof(ego_cmd_fill_mem_params_t) == 5, "wire layout must match asm client");
_Static_assert(sizeof(ego_cmd_sprite_mode_params_t) == 2, "wire layout must match asm client");
_Static_assert(sizeof(ego_cmd_charset_params_t) == 1, "wire layout must match asm client");
_Static_assert(sizeof(ego_cmd_char_to_video_params_t) == 13, "wire layout must match asm client");
_Static_assert(sizeof(ego_cmd_set_char_params_t) == 2, "wire layout must match asm client");
_Static_assert(sizeof(ego_cmd_get_char_params_t) == 2, "wire layout must match asm client");

_Static_assert(sizeof(ego_cmd_char_to_video_params_t) <= EGO_PARAMS_MAX, "EGO_PARAMS_MAX must hold the longest header");

#pragma endregion

static uint8_t RAM_FUNC read_vram(uint16_t addr) {
    return video_ram[addr];
}

static void RAM_FUNC write_vram(uint16_t addr, uint8_t data) {
    if (addr >= 0x1fff) {
        ego_log("error vram addr: %d\n", addr);
    }
    else {
        video_ram[addr] = data;
    }
}

static void RAM_FUNC sprite_draw(sprite_t *sp) {

    int xpos = sp->xpos;
    int ypos = sp->ypos;
    int shape_no = sp->shape_no;

    int width = ego_gfx.shapes[shape_no].width;
    int height = ego_gfx.shapes[shape_no].height;
    int bpp = ego_gfx.shapes[shape_no].bitsperpix;

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

    // ego_log("blitwidth: %d, blitheight %d\n", ego_gfx.blitwidth, ego_gfx.blitheight);
    // ego_log("sprite_draw: shape_no:%d, x:%d y:%d, width:%d, height: %d, bpp: %d\n", shape_no, xpos, ypos, width, height, bpp);

    if (bytex >= ego_gfx.blitwidth + width || ypos > ego_gfx.blitheight + height) {
        return;
    }

    //
    // compute once the "and" mask for shapes having bitsperpixel == 2
    //
    if (ego_gfx.shapes[shape_no].bitsperpix == 2 && ego_gfx.shapes[shape_no].mask == NULL) {
        ego_gfx.shapes[shape_no].mask = malloc(width * height);
        ego_gfx.shapes[shape_no].shadow = malloc(width * height);

        vpos = 0;
        for (y = 0; y < height; y++) {
            for (x = 0; x < width; x++) {
                mask = 0xc0;
                shadow = 0x80;

                data = ego_gfx.shapes[shape_no].data[vpos];
                ego_gfx.shapes[shape_no].mask[vpos] = 0;
                ego_gfx.shapes[shape_no].shadow[vpos] = 0;

                for (i = 0; i < 4; i++) {
                    if (!(data & mask)) {
                        ego_gfx.shapes[shape_no].mask[vpos] |= mask;
                    }
                    else {
                        ego_gfx.shapes[shape_no].shadow[vpos] |= shadow;
                    }
                    mask >>= 2;
                    shadow >>= 2;
                }
                // ego_log("%02X ", ego_gfx.shapes[shape_no].mask[vpos]);
                vpos++;
            }
            // ego_log("\n");
        }
    }

    // vertical start in shape data (top bound)
    ystart = height - ypos;
    if (ystart < 0) {
        ystart = 0;
    }

    // vertical number of lines of shape data
    ylen = height - ystart;

    // check bottom bound
    if (ypos >= ego_gfx.blitheight) {
        ylen = ego_gfx.blitheight + height - ypos;
    }

    // line of screen RAM
    vstart = ypos - height;
    if (vstart < 0) {
        vstart = 0;
    }

    xstart = width - bytex;
    if (xstart < 0) {
        xstart = 0;
    }
    xlen = width - xstart;

    xvstart = bytex - width;
    if (xvstart < 0) {
        xvstart = 0;
    }

    if (bytex > ego_gfx.blitwidth) {
        xlen = bytex - ego_gfx.blitwidth;
    }

    if (sp->mode == EGO_MODE_SPECIAL) {
        sppos = &(ego_gfx.shapes[shape_no].shadow[ystart * width + xstart]);
    }
    else {
        sppos = &(ego_gfx.shapes[shape_no].data[ystart * width + xstart]);
    }
    mpos = &(ego_gfx.shapes[shape_no].mask[ystart * width + xstart]);

    // ego_log("sprite bytex:%d pixx:%d xstart:%d xlen:%d xvstart:%d ystart:%d ylen:%d vstart:%d mpos: %p sppos: %p\n", bytex, pixx, xstart, xlen, xvstart, ystart, ylen, vstart, mpos, sppos);

    for (y = 0; y < ylen; y++) {
        vpos = ego_gfx.line_ptr[vstart + y] + xvstart;

        c = 0;
        m = fill_front[pixx];
        if (xstart > 0) {
            c = (sppos[-1]) << (8 - pixx);
            m = (mpos[-1]) << (8 - pixx);
        }

        for (x = 0; x < xlen; x++) {
            b = read_vram(vpos + x);

            if (sp->mode == EGO_MODE_XOR) {
                b ^= (c | (sppos[x] >> pixx));
            }
            else {
                if (sp->mode == EGO_MODE_MASK) {
                    b &= (m | (mpos[x] >> pixx));
                    b |= (c | (sppos[x] >> pixx));
                }
                else {
                    mask = 0xc0;
                    c |= (sppos[x] >> pixx);
                    for (i = 0; i < 4; i++) {
                        if (!(b & mask)) {
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

        if (pixx != 0 && bytex < ego_gfx.blitwidth) {
            b = read_vram(vpos + x);
            if (sp->mode == EGO_MODE_XOR) {
                b ^= c;
            }
            else {
                if (sp->mode == EGO_MODE_SPECIAL) {
                    mask = 0xc0;
                    for (i = 0; i < 4; i++) {
                        if (!(b & mask)) {
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

static void RAM_FUNC render_sprites(void) {

    sprite_t *sp;

    for (int i = EGO_MAX_SPRITES - 1; i >= 0; i--) {
        sp = &ego_gfx.sprites[i];

        if (sp->enabled) {
            // ego_log("calling sprite_draw sprite: %d\n", i);
            sprite_draw(sp);
        }
    }
}

void do_movement() {
    sprite_t *sp;
    uint16_t xpos, ypos;

    for (int i = 0; i < EGO_MAX_SPRITES; i++) {
        sp = &ego_gfx.sprites[i];

        if (sp->enabled) {
            sprite_draw(sp);

            xpos = sp->xpos;
            ypos = sp->ypos;

            xpos += dx_array[i];
            if (xpos >= (ego_gfx.blitwidth - ego_gfx.shapes[sp->shape_no].width) << 3 || xpos <= 0) {
                dx_array[i] *= -1;
                xpos += dx_array[i];
            }

            ypos += dy_array[i];
            if (ypos >= ego_gfx.blitheight || ypos - ego_gfx.shapes[sp->shape_no].height <= 0) {
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

static void RAM_FUNC char_to_video(ego_cmd_char_to_video_params_t params) {
    int x, y;
    int line;
    uint16_t src_pos;
    uint16_t dst_pos;
    uint8_t b, c;

    src_pos = params.src & 0x3fff;
    dst_pos = params.dst & 0x3fff;

    // ego_log("char2gfx src:%04X, inc:%04X, dst:%04X, inc:%04X, width:%02X, heigth:%02X, charset:%02X, scroll:%02X\n", src_pos, params.src_inc, dst_pos, params.dst_inc, params.width, params.height, params.charset_no, params.scroll);

    for (y = 0; y < params.height; y++) {
        for (line = 0; line < 8; line++) {
            b = 0;
            for (x = 0; x < params.width; x++) {
                c = ego_gfx.charsets[params.charset_no][(video_ram[src_pos + x] << 3) + line];
                b |= (c >> params.scroll);
                video_ram[dst_pos + line * params.dst_inc + x] = b;
                b = c << (8 - params.scroll);
            }
            if (params.col38) {
                video_ram[dst_pos + line * params.dst_inc] = 0;
                video_ram[dst_pos + line * params.dst_inc + 39] = 0;
            }
        }
        src_pos += params.src_inc;
        dst_pos += (params.dst_inc << 3);
    }
}

static void print_debug() {
    sprite_t *sp;
    int i;

    for (i = EGO_MAX_SPRITES - 1; i >= 0; i--) {
        sp = &ego_gfx.sprites[i];

        if (sp->enabled) {
            ego_log("sprite:%d, mode:%d, shape:%d, xpos:%d, ypos:%d\n", i, sp->mode, sp->shape_no, sp->xpos, sp->ypos);
        }
    }
}

#pragma region Command handlers

// Resets the device to its power-on state; sent by the core to every device
// at startup.
static void RAM_FUNC cmd_init(const uint8_t *p) {
    video_ram = get_cart_ram();
    cart_d5xx = get_cart_d5xx();

    memset((void *)ego_gfx.sprites, 0x00, sizeof(ego_gfx.sprites));
    memset((void *)ego_gfx.shapes, 0x00, sizeof(ego_gfx.shapes));

    for (int x = 0; x < EGO_MAX_SPRITES; x++) {
        dx_array[x] = 1;
        if (get_rand_32() & 0x01) {
            dx_array[x] *= -1;
        }

        dy_array[x] = 1;
        if (get_rand_32() & 0x01) {
            dy_array[x] *= -1;
        }
    }
}

static void RAM_FUNC cmd_render_sprites(const uint8_t *p) {
    render_sprites();
}

static void RAM_FUNC cmd_set_writeable(const uint8_t *p) {
    set_writeable(true);
    cart_d5xx[EGO_REG_STATUS] |= 0x01;
}

static void RAM_FUNC cmd_set_readonly(const uint8_t *p) {
    set_writeable(false);
    cart_d5xx[EGO_REG_STATUS] &= ~0x01;
}

static void RAM_FUNC cmd_abort(const uint8_t *p) {
}

static void RAM_FUNC cmd_shape_data(const uint8_t *p) {
    const ego_cmd_shape_data_params_t *params = (const void *)p;
    shape_t *shape = &ego_gfx.shapes[params->shape_no];

    shape->width = params->width;
    shape->height = params->height;
    shape->bitsperpix = params->bits_per_pixel;
    if (shape->data != NULL) {
        free(shape->data);
    }
    if (shape->mask != NULL) {
        free(shape->mask);
        shape->mask = NULL;
    }
    shape->data = malloc(shape->width * shape->height);
    stream_in(shape->data, shape->width * shape->height, NULL);
}

static void RAM_FUNC cmd_sprite_data(const uint8_t *p) {
    const ego_cmd_sprite_data_params_t *params = (const void *)p;

    ego_gfx.sprites[params->sprite_no].shape_no = params->shape_no;
}

static void RAM_FUNC cmd_enable_sprite(const uint8_t *p) {
    const ego_cmd_sprite_no_params_t *params = (const void *)p;

    ego_gfx.sprites[params->sprite_no].enabled = true;
}

static void RAM_FUNC cmd_disable_sprite(const uint8_t *p) {
    const ego_cmd_sprite_no_params_t *params = (const void *)p;

    ego_gfx.sprites[params->sprite_no].enabled = false;
}

static void RAM_FUNC cmd_set_sprite_x(const uint8_t *p) {
    const ego_cmd_set_sprite_x_params_t *params = (const void *)p;
    sprite_t *sprite = &ego_gfx.sprites[params->sprite_no];

    sprite->xold = sprite->xpos;
    sprite->xpos = params->x;
}

static void RAM_FUNC cmd_set_sprite_y(const uint8_t *p) {
    const ego_cmd_set_sprite_y_params_t *params = (const void *)p;
    sprite_t *sprite = &ego_gfx.sprites[params->sprite_no];

    sprite->yold = sprite->ypos;
    sprite->ypos = params->y;
}

static void RAM_FUNC cmd_set_sprite_xy(const uint8_t *p) {
    const ego_cmd_set_sprite_xy_params_t *params = (const void *)p;
    sprite_t *sprite = &ego_gfx.sprites[params->sprite_no];

    sprite->xold = sprite->xpos;
    sprite->xpos = params->x;
    sprite->yold = sprite->ypos;
    sprite->ypos = params->y;
}

static void RAM_FUNC cmd_set_blit_width(const uint8_t *p) {
    const ego_cmd_set_blit_width_params_t *params = (const void *)p;

    ego_gfx.blitwidth = params->width;
}

static void RAM_FUNC cmd_set_blit_height(const uint8_t *p) {
    const ego_cmd_set_blit_height_params_t *params = (const void *)p;

    ego_gfx.blitheight = params->height;
}

static void RAM_FUNC line_ptr_done(void) {
    for (uint32_t i = 0; i < ego_gfx.line_no; i++) {
        ego_gfx.line_ptr[i] = ego_gfx.line_staging[2 * i] | (ego_gfx.line_staging[2 * i + 1] & 0x1f) << 8;
    }
}

static void RAM_FUNC cmd_line_ptr(const uint8_t *p) {
    const ego_cmd_line_ptr_params_t *params = (const void *)p;

    ego_gfx.line_no = params->line_count;
    stream_in(ego_gfx.line_staging, ego_gfx.line_no * 2, line_ptr_done);
}

static void RAM_FUNC cmd_fill_mem(const uint8_t *p) {
    const ego_cmd_fill_mem_params_t *params = (const void *)p;
    uint16_t addr = params->addr & 0x3fff;
    uint16_t len = params->length;

    if (addr + len > 0x3fff) {
        len = 0x3fff - addr;
    }
    memset((void *)&video_ram[addr], params->value, len);
}

static void RAM_FUNC cmd_sprite_mode(const uint8_t *p) {
    const ego_cmd_sprite_mode_params_t *params = (const void *)p;

    ego_gfx.sprites[params->sprite_no].mode = params->mode;
}

static void RAM_FUNC cmd_charset(const uint8_t *p) {
    const ego_cmd_charset_params_t *params = (const void *)p;

    if (ego_gfx.charsets[params->charset_no] == NULL) {
        ego_gfx.charsets[params->charset_no] = malloc(2048);
    }
    stream_in(ego_gfx.charsets[params->charset_no], 2048, NULL);
}

static void RAM_FUNC cmd_char_to_video(const uint8_t *p) {
    ego_cmd_char_to_video_params_t params = *(const ego_cmd_char_to_video_params_t *)p;

    params.scroll &= 0x07;
    char_to_video(params);
}

static void RAM_FUNC cmd_set_char(const uint8_t *p) {
    const ego_cmd_set_char_params_t *params = (const void *)p;

    stream_in(&ego_gfx.charsets[params->charset_no][params->char_no << 3], 8, NULL);
}

static void RAM_FUNC cmd_get_char(const uint8_t *p) {
    const ego_cmd_get_char_params_t *params = (const void *)p;

    stream_out(&ego_gfx.charsets[params->charset_no][params->char_no << 3], 8);
}

static void RAM_FUNC cmd_debug(const uint8_t *p) {
    print_debug();
}

#pragma endregion

// Explicitly placed in RAM so that command dispatch never stalls on flash XIP.
static const ego_cmd_desc_t __not_in_flash("gfx_commands") commands[] = {
    [EGO_CMD_INIT] = {0, cmd_init},
    [EGO_CMD_RENDER_SPRITES] = {0, cmd_render_sprites},
    [EGO_CMD_SET_WRITEABLE] = {0, cmd_set_writeable},
    [EGO_CMD_SET_READONLY] = {0, cmd_set_readonly},
    [EGO_CMD_ABORT] = {0, cmd_abort},
    [EGO_CMD_SHAPE_DATA] = {sizeof(ego_cmd_shape_data_params_t), cmd_shape_data},
    [EGO_CMD_SPRITE_DATA] = {sizeof(ego_cmd_sprite_data_params_t), cmd_sprite_data},
    [EGO_CMD_ENA_SPRITE] = {sizeof(ego_cmd_sprite_no_params_t), cmd_enable_sprite},
    [EGO_CMD_DIS_SPRITE] = {sizeof(ego_cmd_sprite_no_params_t), cmd_disable_sprite},
    [EGO_CMD_SET_SPRITE_X] = {sizeof(ego_cmd_set_sprite_x_params_t), cmd_set_sprite_x},
    [EGO_CMD_SET_SPRITE_Y] = {sizeof(ego_cmd_set_sprite_y_params_t), cmd_set_sprite_y},
    [EGO_CMD_SET_SPRITE_XY] = {sizeof(ego_cmd_set_sprite_xy_params_t), cmd_set_sprite_xy},
    [EGO_CMD_SET_BLIT_WIDTH] = {sizeof(ego_cmd_set_blit_width_params_t), cmd_set_blit_width},
    [EGO_CMD_SET_BLIT_HEIGHT] = {sizeof(ego_cmd_set_blit_height_params_t), cmd_set_blit_height},
    [EGO_CMD_LINE_PTR] = {sizeof(ego_cmd_line_ptr_params_t), cmd_line_ptr},
    [EGO_CMD_FILL_MEM] = {sizeof(ego_cmd_fill_mem_params_t), cmd_fill_mem},
    [EGO_CMD_SPRITE_MODE] = {sizeof(ego_cmd_sprite_mode_params_t), cmd_sprite_mode},
    [EGO_CMD_CHARSET] = {sizeof(ego_cmd_charset_params_t), cmd_charset},
    [EGO_CMD_CHAR_TO_VIDEO] = {sizeof(ego_cmd_char_to_video_params_t), cmd_char_to_video},
    [EGO_CMD_SET_CHAR] = {sizeof(ego_cmd_set_char_params_t), cmd_set_char},
    [EGO_CMD_GET_CHAR] = {sizeof(ego_cmd_get_char_params_t), cmd_get_char},
    [EGO_CMD_DEBUG] = {0, cmd_debug},
};

const ego_device_desc_t __not_in_flash("gfx_device") gfx_device = {
    .commands = commands,
    .num_commands = sizeof(commands) / sizeof(commands[0]),
};

static void RAM_FUNC putChar(int xpos, int ypos, uint8_t c) {
    // ego_log("%p %02X ", (ypos * ego_gfx.blitwidth * 8) + xpos, c);

    uint8_t volatile *pscreen = &video_ram[(ypos * ego_gfx.blitwidth * 8) + xpos];
    uint8_t *pchar = &char_font[c << 3];

    for (int i = 0; i < 8; i++) {
        *pscreen = *pchar;
        pchar++;
        pscreen += ego_gfx.blitwidth;
    }
}

void test() {
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
    for (x = 0; x < 200; x++) {
        write_d5xx(EGO_REG_DATA, ptr & 0xff);
        write_d5xx(EGO_REG_DATA, ptr >> 8);
        ptr += width;
    }

    c = 0;
    for (y = 0; y < (height >> 3); y++) {
        for (x = 0; x < width; x++) {
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

    for (y = 0; y < 21; y++) {
        for (x = 0; x < 3; x++) {
            write_d5xx(EGO_REG_DATA, manta[y * 3 + x]);
        }
    }

    for (x = 0; x < EGO_MAX_SPRITES; x++) {
        write_d5xx(EGO_REG_CMD, EGO_CMD_SPRITE_DATA);
        write_d5xx(EGO_REG_DATA, x);    // sprite number
        write_d5xx(EGO_REG_DATA, 0x00); // shape number

        write_d5xx(EGO_REG_CMD, EGO_CMD_SET_SPRITE_XY);
        write_d5xx(EGO_REG_DATA, x);                            // sprite number
        write_d5xx(EGO_REG_DATA, get_rand_32() % (width << 3)); // sprite X lo
        write_d5xx(EGO_REG_DATA, 0);                            // sprite X hi
        write_d5xx(EGO_REG_DATA, get_rand_32() % height); // sprite Y lo
        write_d5xx(EGO_REG_DATA, 0);                      // sprite Y hi

        write_d5xx(EGO_REG_CMD, EGO_CMD_ENA_SPRITE);
        write_d5xx(EGO_REG_DATA, x); // sprite number

        write_d5xx(EGO_REG_CMD, EGO_CMD_SPRITE_MODE);
        write_d5xx(EGO_REG_DATA, x); // sprite number
        write_d5xx(EGO_REG_DATA, 1); // sprite number
    }

    write_d5xx(EGO_REG_CMD, EGO_CMD_RENDER_SPRITES);

    while (true) {
        c = getchar();
        printf("%c", c);

        write_d5xx(EGO_REG_CMD, EGO_CMD_RENDER_SPRITES);
        while (read_d5xx(EGO_REG_STATUS & 0x80))
            ;

        switch (c) {
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
