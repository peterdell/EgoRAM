# Proposal: Consistent constant naming

Status: PROPOSAL - not yet implemented.

## Principles

1. Words are written out in full: DEL -> DELETE, ENA -> ENABLE,
   DIS -> DISABLE, MEM -> MEMORY, PTR -> POINTER, LEN -> LENGTH,
   NO -> NUMBER, SRC -> SOURCE, DST -> DESTINATION, INC -> INCREMENT,
   BPP -> BITS_PER_PIXEL.
2. Allowed short forms (established idiom, kept as-is): LO, HI (6502
   byte halves), X, Y, XY (coordinates), CHAR, CHARSET, USB, XOR, BLIT.
3. Group prefixes stay compact: EGO_REG_, EGO_CMD_, EGO_MODE_. Only
   EGO_ST_ becomes EGO_STATE_ (ST is not a recognizable abbreviation).
4. Commands start with a verb. Parser state names reuse the words of
   the command they belong to (e.g. the states of EGO_CMD_CHAR_TO_VIDEO
   are EGO_STATE_CHAR_TO_VIDEO_*).
5. Renames change names only, never values, so the wire protocol is
   unaffected. Per 01_RULES.md, EgoRAM.h and asm/EgoRAM-Equates.asm
   change in the same commit.

## EgoReg

| Current          | Proposed            |
|------------------|---------------------|
| `EGO_REG_CMD`    | `EGO_REG_COMMAND`   |
| `EGO_REG_STATUS` | unchanged           |
| `EGO_REG_DATA`   | unchanged           |

## EgoCmd

| Current                    | Proposed                      | Reason                       |
|----------------------------|-------------------------------|------------------------------|
| `EGO_CMD_RENDER_SPRITES`   | unchanged                     |                              |
| `EGO_CMD_SET_WRITEABLE`    | `EGO_CMD_SET_WRITABLE`        | standard spelling            |
| `EGO_CMD_SET_READONLY`     | `EGO_CMD_SET_READ_ONLY`       | word separation              |
| `EGO_CMD_ABORT`            | unchanged                     |                              |
| `EGO_CMD_SHAPE_DATA`       | `EGO_CMD_SET_SHAPE_DATA`      | verb first                   |
| `EGO_CMD_DEL_SHAPE`        | `EGO_CMD_DELETE_SHAPE`        | DEL -> DELETE                |
| `EGO_CMD_SPRITE_DATA`      | `EGO_CMD_SET_SPRITE_DATA`     | verb first                   |
| `EGO_CMD_DEL_SPRITE`       | `EGO_CMD_DELETE_SPRITE`       | DEL -> DELETE                |
| `EGO_CMD_ENA_SPRITE`       | `EGO_CMD_ENABLE_SPRITE`       | ENA -> ENABLE                |
| `EGO_CMD_DIS_SPRITE`       | `EGO_CMD_DISABLE_SPRITE`      | DIS -> DISABLE               |
| `EGO_CMD_SET_SPRITE_X`     | unchanged                     |                              |
| `EGO_CMD_SET_SPRITE_Y`     | unchanged                     |                              |
| `EGO_CMD_SET_SPRITE_XY`    | unchanged                     |                              |
| `EGO_CMD_SET_BLIT_WIDTH`   | unchanged                     |                              |
| `EGO_CMD_SET_BLIT_HEIGHT`  | unchanged                     |                              |
| `EGO_CMD_LINE_PTR`         | `EGO_CMD_SET_LINE_POINTERS`   | PTR -> POINTER, verb first   |
| `EGO_CMD_FILL_MEM`         | `EGO_CMD_FILL_MEMORY`         | MEM -> MEMORY                |
| `EGO_CMD_SPRITE_MODE`      | `EGO_CMD_SET_SPRITE_MODE`     | verb first                   |
| `EGO_CMD_CHARSET`          | `EGO_CMD_SET_CHARSET`         | verb first (open TODO in asm)|
| `EGO_CMD_CHAR_TO_VIDEO`    | unchanged                     |                              |
| `EGO_CMD_SET_CHAR`         | unchanged                     |                              |
| `EGO_CMD_GET_CHAR`         | unchanged                     |                              |
| `EGO_CMD_DEBUG`            | unchanged                     |                              |

## EgoState (firmware-internal) - OBSOLETE

03_COMMAND_TABLE.md has been implemented; the states below no longer
exist. The remaining EgoState phases (IDLE, HEADER, DATA_IN, DATA_OUT)
need no renaming. This section is kept for reference only.

Prefix `EGO_ST_` -> `EGO_STATE_` throughout; additionally:

| Current                     | Proposed                                       |
|-----------------------------|------------------------------------------------|
| `EGO_ST_SHAPE_NO`           | `EGO_STATE_SHAPE_NUMBER`                       |
| `EGO_ST_SHAPE_BPP`          | `EGO_STATE_SHAPE_BITS_PER_PIXEL`               |
| `EGO_ST_SPRITE_NO`          | `EGO_STATE_SPRITE_NUMBER`                      |
| `EGO_ST_SPRITE_SHAPE_NO`    | `EGO_STATE_SPRITE_SHAPE_NUMBER`                |
| `EGO_ST_SPRITE_ENA`         | `EGO_STATE_SPRITE_ENABLE`                      |
| `EGO_ST_SPRITE_DIS`         | `EGO_STATE_SPRITE_DISABLE`                     |
| `EGO_ST_BLITWITH`           | `EGO_STATE_BLIT_WIDTH` (also fixes typo)       |
| `EGO_ST_BLITHEIGHT`         | `EGO_STATE_BLIT_HEIGHT`                        |
| `EGO_ST_LINE_NO`            | `EGO_STATE_LINE_NUMBER`                        |
| `EGO_ST_FILL_MEM_LO/_HI`    | `EGO_STATE_FILL_ADDRESS_LO/_HI` (it is the address) |
| `EGO_ST_FILL_LEN_LO/_HI`    | `EGO_STATE_FILL_LENGTH_LO/_HI`                 |
| `EGO_ST_CHARSET_NO`         | `EGO_STATE_CHARSET_NUMBER`                     |
| `EGO_ST_CHR2GFX_*`          | `EGO_STATE_CHAR_TO_VIDEO_*` (matches command)  |
| `..._SRC_LO/_HI`            | `..._SOURCE_LO/_HI`                            |
| `..._SRC_INC_LO/_HI`        | `..._SOURCE_INCREMENT_LO/_HI`                  |
| `..._DST_LO/_HI`            | `..._DESTINATION_LO/_HI`                       |
| `..._DST_INC_LO/_HI`        | `..._DESTINATION_INCREMENT_LO/_HI`             |
| `..._COL38`                 | `..._COLUMN_38` (verify meaning first)         |
| `EGO_ST_CHAR_NO`            | `EGO_STATE_CHAR_NUMBER`                        |
| `EGO_ST_CHAR_GET_DATA`      | `EGO_STATE_CHAR_READ_DATA`                     |

All other `EGO_ST_*` names keep their words and only change the prefix.

## Unchanged groups

`EGO_MODE_*` (both EgoMode and EgoSpriteMode), `EGO_MAX_*`, `EGO_UART_*`,
`EGO_BAUD_RATE`, `EGO_MEM_SCREEN`/`EGO_MEM_CHARSET` in the asm equates
(MEM -> MEMORY would apply, see open questions).

## Open questions

1. `EGO_REG_CMD` -> `EGO_REG_COMMAND` makes the register name differ
   from the `EGO_CMD_` prefix. Alternative: keep `EGO_REG_CMD` and
   accept CMD on the allowlist everywhere.
2. The long parser state names (e.g.
   `EGO_STATE_CHAR_TO_VIDEO_DESTINATION_INCREMENT_LO`) are verbose.
   Alternative: keep SRC/DST/INC on the allowlist.
3. `EGO_MEM_SCREEN`/`EGO_MEM_CHARSET` (asm only): rename to
   `EGO_MEMORY_*`, or keep because they name a memory window, not a
   command?
4. `COL38`: confirm it means "38-column (narrow) mode" before renaming.

## Impact and migration

- Name-only changes; all values stay identical, existing binaries and
  the wire protocol are unaffected.
- Affected files: `EgoRAM.h` (~80 definitions/uses), `EgoRAM.c`
  (~250 uses), `asm/EgoRAM-Equates.asm` (definitions),
  `asm/EgoUridium/EgoUridium.asm` + `EgoUridium-Setup.asm` (~120 uses).
- `cvideo.c:421` references `EGO_CMD_MOVEMENT` in commented-out code;
  this constant does not exist and the comment should be removed or
  updated during the rename.
- Mechanical find-and-replace with whole-word matching, longest names
  first (so `EGO_CMD_SET_SPRITE_XY` is replaced before
  `EGO_CMD_SET_SPRITE_X`). One commit covering C and asm together,
  verified by rebuilding the firmware and reassembling EgoUridium and
  EgoTest.
- Update 01_RULES.md examples and the `_Static_assert` messages if
  names change.
