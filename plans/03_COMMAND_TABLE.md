# Proposal: Table-driven command parser

Status: PROPOSAL - not yet implemented.

Alternative to the `EGO_ST_*`/`EGO_STATE_*` rename block in 02_NAMING.md:
instead of renaming ~40 parser states, remove them. Wire protocol,
asm/EgoRAM-Equates.asm and the Atari side are completely unaffected
(EgoState is firmware-internal, exempt in 01_RULES.md).

## Problem

`EgoState` has one enum value per expected parameter byte per command.
Every value must carry the full command context in its name
(`EGO_ST_CHR2GFX_SRC_INC_LO`), the `do_data()` switch in EgoRAM.c has
~40 cases that mostly just store one byte and name the next state, and
adding a command means inventing a new state chain. The states are a
byte counter wearing forty costumes.

## Design

Each command declares its fixed parameter header once, in a table.
Incoming data bytes are buffered; when the header is complete, one
handler runs with the whole parameter block and decodes it using local
variable names. The global state shrinks to four phases:

```c
typedef enum : uint8_t
{
	EGO_ST_IDLE,     // no command in progress
	EGO_ST_HEADER,   // collecting fixed parameter bytes
	EGO_ST_DATA_IN,  // streaming bulk data from the Atari
	EGO_ST_DATA_OUT  // streaming bulk data to the Atari (GET_CHAR)
} EgoState;

typedef struct
{
	uint8_t header_len;                 // fixed parameter bytes after the command byte
	void (*execute)(const uint8_t *p);  // runs when the header is complete
} ego_cmd_desc_t;

static const ego_cmd_desc_t ego_commands[] = {
	[EGO_CMD_RENDER_SPRITES]  = {0,  cmd_render_sprites},
	[EGO_CMD_SET_WRITEABLE]   = {0,  cmd_set_writeable},
	[EGO_CMD_SET_READONLY]    = {0,  cmd_set_readonly},
	[EGO_CMD_ABORT]           = {0,  cmd_abort},
	[EGO_CMD_SHAPE_DATA]      = {4,  cmd_shape_data},      // no, width, height, bpp + stream
	[EGO_CMD_SPRITE_DATA]     = {2,  cmd_sprite_data},     // no, shape_no
	[EGO_CMD_ENA_SPRITE]      = {1,  cmd_enable_sprite},
	[EGO_CMD_DIS_SPRITE]      = {1,  cmd_disable_sprite},
	[EGO_CMD_SET_SPRITE_X]    = {3,  cmd_set_sprite_x},    // no, x_lo, x_hi
	[EGO_CMD_SET_SPRITE_Y]    = {3,  cmd_set_sprite_y},
	[EGO_CMD_SET_SPRITE_XY]   = {5,  cmd_set_sprite_xy},
	[EGO_CMD_SET_BLIT_WIDTH]  = {1,  cmd_set_blit_width},
	[EGO_CMD_SET_BLIT_HEIGHT] = {1,  cmd_set_blit_height},
	[EGO_CMD_LINE_PTR]        = {1,  cmd_line_ptr},        // count + stream
	[EGO_CMD_FILL_MEM]        = {5,  cmd_fill_mem},        // addr, len, byte
	[EGO_CMD_SPRITE_MODE]     = {2,  cmd_sprite_mode},     // no, mode
	[EGO_CMD_CHARSET]         = {1,  cmd_charset},         // no + 2048 stream
	[EGO_CMD_CHAR_TO_VIDEO]   = {13, cmd_char_to_video},
	[EGO_CMD_SET_CHAR]        = {2,  cmd_set_char},        // charset, char + 8 stream
	[EGO_CMD_GET_CHAR]        = {2,  cmd_get_char},        // charset, char -> DATA_OUT
};
```

Parser (replaces the ~40-case switch in `do_data()` and most of
`do_command()`):

```c
static uint8_t ego_params[16];           // >= longest header (13)
static uint8_t ego_param_idx;
static uint8_t *ego_in_ptr;              // DATA_IN target
static uint32_t ego_in_len;
static void (*ego_in_done)(void);        // optional completion hook
static const uint8_t *ego_out_ptr;       // DATA_OUT source
static uint32_t ego_out_len;

void do_command(uint8_t data)            // EGO_REG_CMD written
{
	ego_cmd = data;
	ego_param_idx = 0;
	if (ego_commands[ego_cmd].header_len == 0)
	{
		ego_state = EGO_ST_IDLE;
		ego_commands[ego_cmd].execute(NULL);
	}
	else
		ego_state = EGO_ST_HEADER;
}

void do_data(uint8_t data)               // EGO_REG_DATA written
{
	switch (ego_state)
	{
	case EGO_ST_HEADER:
		ego_params[ego_param_idx++] = data;
		if (ego_param_idx == ego_commands[ego_cmd].header_len)
		{
			ego_state = EGO_ST_IDLE;     // execute() may start DATA_IN/OUT
			ego_commands[ego_cmd].execute(ego_params);
		}
		break;
	case EGO_ST_DATA_IN:
		*ego_in_ptr++ = data;
		if (--ego_in_len == 0)
		{
			ego_state = EGO_ST_IDLE;
			if (ego_in_done) ego_in_done();
		}
		break;
	default:
		break;
	}
}
```

Handlers decode with local names - no global constants needed:

```c
static void cmd_char_to_video(const uint8_t *p)
{
	ego_src_ptr        = p[0] | p[1] << 8;
	ego_src_inc        = p[2] | p[3] << 8;
	ego_dst_ptr        = p[4] | p[5] << 8;
	ego_dst_inc        = p[6] | p[7] << 8;
	ego_chr2gfx_width  = p[8];
	ego_chr2gfx_height = p[9];
	ego_charset_no     = p[10];
	ego_chr2gfx_scroll = p[11] & 0x07;
	ego_chr2gfx_col38  = p[12];
	char_to_video();
}

static void cmd_shape_data(const uint8_t *p)
{
	shape_t *sh = &shape_array[p[0]];
	sh->width = p[1];
	sh->height = p[2];
	sh->bitsperpix = p[3];
	free(sh->data);                      // free(NULL) is a no-op
	sh->data = malloc(sh->width * sh->height);
	stream_in(sh->data, sh->width * sh->height, NULL);
}
```

`stream_in(ptr, len, done)` sets `ego_in_*` and `ego_state =
EGO_ST_DATA_IN`. `cmd_get_char()` preloads `cart_d5xx[EGO_REG_DATA]`
with the first byte and enters `EGO_ST_DATA_OUT`; `read_d5xx()` handles
that phase generically (copy `*ego_out_ptr++`, count down, back to
IDLE), replacing the `EGO_ST_CHAR_GET_DATA` special case.

### Variable-length commands

| Command       | Header          | Stream                                  |
|---------------|-----------------|-----------------------------------------|
| SHAPE_DATA    | no,w,h,bpp (4)  | w*h bytes into shape data               |
| CHARSET       | no (1)          | 2048 bytes into charset                 |
| SET_CHAR      | charset,char (2)| 8 bytes into charset at char offset     |
| LINE_PTR      | count (1)       | count*2 bytes, completion hook unpacks  |
| GET_CHAR      | charset,char (2)| 8 bytes out via read_d5xx               |

LINE_PTR streams the lo/hi byte pairs into a 512-byte staging buffer;
its `ego_in_done` hook unpacks them into `ego_line_ptr[]` applying the
`& 0x1f` mask on the high bytes. (Alternative: a per-byte stream filter,
but one completion hook is simpler.)

## Behavior notes (intentional differences)

- Mid-header side effects move to the end of the header: today
  SHAPE_DATA mallocs after height (before bpp arrives) and CHARSET
  mallocs on the charset number byte; afterwards both allocate in the
  handler once all header bytes are in. ABORT during a header then has
  no side effects at all, which is an improvement.
- xold/ypos capture for SET_SPRITE_X/Y/XY happens in the handler
  (xold = xpos before overwriting), same net result as today.
- A command byte written mid-parse restarts parsing, as today.
- DEL_SHAPE/DEL_SPRITE are currently no-ops without parameters; they
  get table entries ({1, cmd_delete_*}) when implemented.

## Timing

Parsing runs on core 1 behind the multicore FIFO (`write_d5xx` only
pushes), so the bus-facing path is untouched. The per-byte work in the
HEADER and DATA_IN phases (store + compare) is cheaper than the current
40-case switch. The `execute()` call fires on the last header byte,
equivalent to today's final-state actions. `read_d5xx` stays on the bus
path; the DATA_OUT phase is the same amount of work as the current
EGO_ST_CHAR_GET_DATA case.

## Consequences

- `EgoState` shrinks from 43 values to 4; the state-rename section of
  02_NAMING.md becomes obsolete (the EgoCmd/EgoReg renames there still
  apply, and the table/handler names should use the renamed commands).
- Adding a command = one enum value (per 01_RULES.md), one table row,
  one handler.
- The table doubles as protocol documentation: each command's parameter
  layout is visible in one place and can later feed a generator for the
  asm equates or README.
- Risk: one-shot rewrite of the parser core. Mitigation: the commands
  are exercised end-to-end by EgoUridium; verify with firmware build +
  EgoUridium and EgoTest on real hardware or emulator, especially
  SHAPE_DATA upload, CHAR_TO_VIDEO scrolling and GET_CHAR readback
  (bullets).

## Migration steps

1. Add the phase enum, table, buffers and `stream_in()` alongside the
   existing parser.
2. Port the handlers command by command, deleting their `EGO_ST_*`
   cases as they move.
3. Remove the old state enum values; `EGO_ST_IDLE` keeps its role.
4. Rebuild firmware; reassemble and run EgoUridium and EgoTest.
5. Update 02_NAMING.md (drop the state table) and the README command
   documentation from the table.
