# Rules

## Wire protocol constants must match between C and assembler

The Atari (6502) side and the Pico firmware communicate through shared
constants that are defined twice:

- `EgoRAM.h` (core) and the device compile units such as `ego_gfx.c` —
  used by the Pico firmware (C)
- `asm/EgoRAM-Equates.asm` — used by the Atari-side programs (MADS)

The names and the sequence (and therefore the values) of these definitions
must be the same in both files:

| C enum                                    | EgoRAM-Equates.asm |
|-------------------------------------------|--------------------|
| `EgoReg` (`EGO_REG_*`, EgoRAM.h)          | `.enum ego_reg` members ($D5xx addresses; C holds the offsets) |
| `EgoDevice` (`EGO_DEVICE_*`, EgoRAM.h)    | `.enum ego_device` members |
| `EgoCmd` (`EGO_CMD_*`, each device's .c)  | `.enum ego_<device>_cmd` members |
| `EgoSpriteMode` (`EGO_MODE_*`, ego_gfx.c) | `.enum ego_sprite_mode` members |

Each device's command enum is private to its compile unit and therefore
uniformly named `EgoCmd` in C. On the asm side the commands are scoped
MADS enums (referenced as e.g. `ego_gfx_cmd.RENDER_SPRITES`), so command
names only need to be unique within their device. A C enumerator
`EGO_CMD_X` corresponds to the member `X` of the device's asm enum.

Rules when changing them:

- Keep the same names in both files (apart from the language-specific
  syntax and the `EGO_CMD_` prefix that the scoped asm enums drop).
- Only append new entries at the end, never insert or reorder. The enums
  are auto-numbered on both sides, so inserting an entry silently
  renumbers everything after it and desynchronizes the other side
  without any error.
- `EGO_CMD_INIT = 0` is common to all devices and defined in
  `ego_device.h`; every asm device enum starts with the member
  `INIT = 0` and the device-specific commands follow from 1.
- Update both files in the same commit.

The `_Static_assert` pins (e.g. `EGO_CMD_GET_CHAR == 22` in ego_gfx.c)
make the firmware build fail when the numbering shifts. When appending a
new entry at the end of an enum, move the pin to the new last entry with
its expected value.

Not covered by this rule: `EgoState` and `EgoMode` in `EgoRAM.h` are
internal to the Pico firmware and may be reordered freely.
