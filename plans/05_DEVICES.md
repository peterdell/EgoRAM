# Proposal: Multiple devices inside the cartridge

Status: IMPLEMENTED 2026-10-18 (selector register and graphics state
grouping; graphics is still the only device).

EgoRAM so far implements only the video/graphics functions. The
cartridge shall host several devices, graphics being one of them, each
with its own command set and state.

## Design

- `EGO_REG_DEVICE` ($D503) is the device selector register. The Atari
  writes an `EGO_DEVICE_*` number to it to pick the device that
  receives subsequent EGO_REG_CMD/EGO_REG_DATA traffic. The register is
  readable, returning the active device.
- `EgoDevice` enumerates the devices. It is wire protocol, synced with
  asm/EgoRAM-Equates.asm per 01_RULES.md (append-only, pinned with a
  `_Static_assert`). `EGO_DEVICE_GRAPHICS` is 0 and active after reset,
  so existing clients that never touch the selector keep working.
- A global `ego_device` holds the active device on core 1. Writing the
  selector aborts any command in progress (`ego_state` back to IDLE).
  Commands and data for devices that are not implemented are ignored.
- Each device groups its entire state in one struct. For graphics that
  is `ego_gfx_t` (blit width/height, line pointer table and staging,
  shapes, sprites, charsets), instantiated once as `ego_gfx`. The
  parser machinery (`ego_cmd`, `ego_state`, `ego_params`, stream
  in/out pointers) stays shared across devices.

## Next steps when the second device arrives

- Give each device its own command table; dispatch in `do_command` /
  `do_data` via `ego_device` (e.g. an `ego_device_desc_t` holding the
  table, its length and the device state pointer).
- Decide whether command numbering restarts per device (likely yes,
  since the selector disambiguates) — document it in 01_RULES.md and
  the equates file, e.g. `EGO_<device>_CMD_*` blocks.
