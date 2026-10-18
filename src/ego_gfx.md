# Graphics device (EGO_DEVICE_GRAPHICS = 1)

Files: `ego_gfx_device.c`, `ego_gfx_device.h`, `ego_gfx.h` (wire protocol)

The graphics device renders into the cartridge video RAM: software
sprites with XOR/mask/special blit modes built from uploadable shapes,
uploadable charsets, character-to-bitmap rendering with horizontal
scrolling (CHAR_TO_VIDEO), memory fill and single-character up/download
(SET_CHAR/GET_CHAR). The command set is the `EgoGfxCmd` enum in
`ego_gfx_device.h`; each command's wire format is declared by its
`ego_cmd_*_params_t` struct in `ego_gfx_device.c`.

Origin: developed in this repository. The surrounding cartridge
emulation is based on Robin Edwards' A8PicoCart; the composite video
output in `cvideo.c` is based on Dean Belfield's pico-mposite.
