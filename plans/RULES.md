# Rules

## Wire protocol constants must match between C and assembler

The Atari (6502) side and the Pico firmware communicate through shared
constants that are defined twice:

- `EgoRAM.h` — used by the Pico firmware (C)
- `asm/EgoRAM-Equates.asm` — used by the Atari-side programs (MADS)

The names and the sequence (and therefore the values) of these definitions
must be the same in both files:

| EgoRAM.h (enum)                | EgoRAM-Equates.asm        |
|--------------------------------|---------------------------|
| `EgoReg` (`EGO_REG_*`)         | `EGO_REG_*`               |
| `EgoCmd` (`EGO_CMD_*`)         | `EGO_CMD_*`               |
| `EgoSpriteMode` (`EGO_MODE_*`) | `EGO_MODE_*`              |

Rules when changing them:

- Keep the same names in both files (apart from the language-specific
  syntax).
- Only append new entries at the end, never insert or reorder. The C enums
  are auto-numbered, so inserting an entry silently renumbers everything
  after it and desynchronizes the Atari side without any error.
- `EGO_CMD_DEBUG = 99` keeps its explicit value in both files.
- Update both files in the same commit.

The `_Static_assert` pins in `EgoRAM.h` (e.g. `EGO_CMD_GET_CHAR == 21`)
make the firmware build fail when the numbering shifts. When appending a
new entry at the end of an enum, move the pin to the new last entry with
its expected value.

Not covered by this rule: `EgoState` and `EgoMode` in `EgoRAM.h` are
internal to the Pico firmware and may be reordered freely.
