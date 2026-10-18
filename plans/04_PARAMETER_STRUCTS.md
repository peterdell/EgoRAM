# Proposal: Wire-format parameter structs

Status: IMPLEMENTED 2026-10-18 (firmware builds; runs on the same
hardware/emulator verification as 03_COMMAND_TABLE.md since the wire
protocol is unchanged).

Follow-up to 03_COMMAND_TABLE.md: the table-driven parser delivers each
command's fixed parameter bytes as a raw `const uint8_t *p`, and the
handlers unpack them by index (`p[0] | p[1] << 8`). The CHAR_TO_VIDEO
handler additionally parked its nine decoded parameters in globals
(`ego_src_ptr`, `ego_chr2gfx_*`, ...) that were really function
arguments, not state.

## Design

One packed struct per command with parameters, named `ego_cmd_<command>_params_t`
to bind it to its command, mirroring the exact byte sequence the Atari
writes to EGO_REG_DATA after the command byte:

```c
#define EGO_PACKED __attribute__((packed))

typedef struct EGO_PACKED {
    uint8_t sprite_no;
    uint16_t x;
    uint16_t y;
} ego_cmd_set_sprite_xy_params_t;
```

Handlers overlay the struct on the parameter buffer instead of
unpacking bytes:

```c
static void RAM_FUNC cmd_set_sprite_xy(const uint8_t *p) {
    const ego_cmd_set_sprite_xy_params_t *params = (const void *)p;
    sprite_t *sprite = &sprite_array[params->sprite_no];

    sprite->xold = sprite->xpos;
    sprite->xpos = params->x;
    ...
}
```

This works because the wire format is little-endian (lo byte first) and
so is the RP2040, and because packed structs have alignment 1, so the
cast from the byte buffer is always valid. Cortex-M0+ has no unaligned
loads, but GCC accesses packed members byte-wise — essentially the same
instructions as the manual shifts, so the RAM_FUNC handlers cost the
same.

The command table derives header_len from the structs, removing the
magic byte counts:

```c
[EGO_CMD_SET_SPRITE_XY] = {sizeof(ego_cmd_set_sprite_xy_params_t), cmd_set_sprite_xy},
```

A `_Static_assert(sizeof(...) == N, ...)` per struct pins each size to
the wire protocol, in the same spirit as the EgoCmd/EgoReg pins in
EgoRAM.h; padding or an enum-size surprise (EgoSpriteMode is a field of
ego_cmd_sprite_mode_params_t) fails the build instead of shifting bytes.
One more assert checks that the longest header (CHAR_TO_VIDEO, 13
bytes) fits in `ego_params`.

The structs live in EgoRAM.c next to the handlers because nothing else
uses them; each doubles as the authoritative byte-layout documentation
for its command, including the trailing DATA_IN/DATA_OUT streams as
comments.

## What the structs cannot express

Masked fields keep their masks in the handlers, with a comment on the
struct field recording them:

- FILL_MEM / CHAR_TO_VIDEO addresses: `& 0x3fff` (video RAM window)
- CHAR_TO_VIDEO scroll: `& 0x07`
- LINE_PTR high bytes: `& 0x1f` (applied in the stream completion hook)

## CHAR_TO_VIDEO global cleanup

`char_to_video()` now takes `ego_cmd_char_to_video_params_t` by value (13
bytes pass in registers on ARM, and by-value avoids volatile-aliasing
reloads of the fields inside the blit loops). The handler copies the
struct, applies the scroll mask, and calls it. Nine globals were
deleted: `ego_src_ptr`, `ego_dst_ptr`, `ego_src_inc`, `ego_dst_inc`,
`ego_chr2gfx_width/height/scroll/col38` and `ego_charset_no` (CHARSET,
SET_CHAR and GET_CHAR each take the charset number from their own
parameters). `ego_line_no` stays global because LINE_PTR's completion
hook needs it after the DATA_IN phase.

What legitimately remains global: the parser state (`ego_cmd`,
`ego_state`, `ego_params`, the stream in/out pointers) and persistent
device state (`ego_blitwidth/height`, `ego_line_ptr[]`,
`charset_array`, sprites, shapes).

## Result structs: deferred

Only GET_CHAR returns data, and it is a plain 8-byte stream with no
internal structure. Result structs get defined when a command returns
multiple named fields; the DATA_OUT stream mechanism itself stays.

## Consequences

- The wire format of every command is declared in one place and named;
  the byte count can no longer drift from the layout.
- No wire protocol change: asm/EgoRAM-Equates.asm and the Atari clients
  are unaffected.
- 01_RULES.md discipline extends naturally: a new command adds one enum
  value, one params struct with size assert, one table row, one
  handler.
- The 02_NAMING.md renames, when applied, should rename the structs
  with the commands (e.g. `ego_cmd_fill_memory_params_t`).
