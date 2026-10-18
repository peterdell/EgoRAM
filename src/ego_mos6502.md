# MOS6502 device (EGO_DEVICE_MOS6502 = 2)

Files: `ego_mos6502_device.c`, `ego_mos6502_device.h`, `ego_mos6502.h`
(wire protocol), `fake6502.c`,
`fake6502.h` (CPU core)

A virtual 6502 CPU running inside the cartridge, so the Atari can
offload 6502 code to the Pico, which executes it against its own
memory at a multiple of the real machine's speed. The CPU currently
runs against a private 64 KB memory; sharing memory with the cartridge
will be defined later.

Commands (`EgoMos6502Cmd`): `EGO_CMD_SET_MEMORY` and
`EGO_CMD_GET_MEMORY` transfer a block (addr, length, then the data
bytes streamed in or out), `EGO_CMD_EXECUTE` runs the CPU for at least
the given number of cycles (instruction granularity). Init clears the
memory and resets the CPU.

The CPU core is Fake6502 in the heavily bug-fixed and extended fork by
Ivo van Poorten, downloaded 2026-10-18 from
<https://github.com/ivop/fake6502> (`fake6502.c`, `fake6502.h`,
unmodified). Original author Mike Chambers (2011-2013), fork copyright
Ivo van Poorten (2024). License: BSD 2-Clause, see
`fake6502-LICENSE.txt`.

The core's host interface: the embedder provides `read6502(address)`
and `write6502(address, value)` for the virtual bus — the EgoRAM
device wrapper will map these to cartridge memory — and calls
`reset6502()`, `step6502()` (one instruction, returns cycles),
`irq6502()` and `nmi6502()`. CPU registers are exposed as the globals
`PC`, `SP`, `A`, `X`, `Y` plus `getP()`/`setP()`.
