# Proposal: Multiple devices inside the cartridge

Status: IMPLEMENTED 2026-10-18 (selector register and graphics state
grouping; graphics is still the only device). Same day: the graphics
device was extracted into its own compile unit, see "Device isolation"
below.

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
  `_Static_assert`). `EGO_DEVICE_DEBUG` is 0 and active after reset, so
  every client selects its device before sending commands.
- A global `ego_device` holds the active device on core 1. Writing the
  selector aborts any command in progress (`ego_state` back to IDLE).
  Commands and data for devices that are not implemented are ignored.
- Each device groups its entire state in one struct. For graphics that
  is `ego_gfx_t` (blit width/height, line pointer table and staging,
  shapes, sprites, charsets), instantiated once as `ego_gfx`. The
  parser machinery (`ego_cmd`, `ego_state`, `ego_params`, stream
  in/out pointers) stays shared across devices.

## Device isolation (implemented 2026-10-18)

Each device is a separate compile unit, so its internals need no device
prefix — file-level `static` provides the isolation. The graphics
device lives in `ego_gfx_device.c`/`ego_gfx_device.h`:

- `ego_gfx_device.c` holds everything private: the state struct and its one
  instance, the wire-format parameter structs, the command handlers,
  the sprite/blit engine and the command table. Names stay short
  (`cmd_set_char`, `sprite_draw`, `commands[]`).
- A device's `_device.h` header exposes only its descriptor (plus, for
  graphics, `gfx_print_debug()` and `test()`). The command enum —
  uniformly named `EgoCmd` in every device — lives in the device's
  wire-protocol header (`ego_<device>.h`, e.g. `ego_gfx.h`), which only
  the device's compile unit and its tests include, and is synced with
  the equates file per 01_RULES.md.
- `EgoRAM.h` is the core contract: registers, devices, parser phases
  and the d5xx/log services. `ego_device.h` holds everything a device
  implementation needs: `EGO_CMD_INIT`, `EGO_PARAMS_MAX`, `EGO_PACKED`,
  `ego_cmd_desc_t`/`ego_device_desc_t` and the parser services
  `stream_in()`/`stream_out()` (stream_out serves the first byte in
  EGO_REG_DATA, the rest on subsequent reads).
- The core (`EgoRAM.c`) dispatches generically through
  `ego_devices[]`, indexed by `EgoDevice` and placed in RAM like the
  command tables. Writing an unknown device number to EGO_REG_DEVICE
  is ignored, so the readable register always names a real device.

Every device's command 0 is `EGO_CMD_INIT` (defined once in EgoRAM.h and
the equates file): it resets the device to its power-on state. At
startup `main()` loops over `ego_devices[]` and sends each device its
Init command, so a device's `ego_device_desc_t` is its complete
interface. Device-specific command enums therefore start at 1.

A new device = one compile unit exporting its `ego_device_desc_t` with
a `cmd_init` handler at `[EGO_CMD_INIT]`, one `EgoDevice` enum value
(both files, 01_RULES.md) and one row in `ego_devices[]`.

## The debug device (implemented 2026-10-18)

`EGO_DEVICE_DEBUG` (0, the default device after reset) in
`ego_debug_device.c`/`ego_debug_device.h` is the template for further devices:
`EGO_CMD_DEBUG` (1, its first command after Init) prints the firmware
state to the log output, today by calling `gfx_print_debug()`. A client
that talks to the cartridge without selecting its device first hits the
harmless debug device instead of triggering graphics operations. This
settled the numbering question:
command values restart per device, since the selector disambiguates.
The command enums are private to the device compile units (uniformly
named `EgoCmd`) and scoped MADS enums like `ego_gfx_cmd` in the equates
file, so names only need to be unique within a device.
