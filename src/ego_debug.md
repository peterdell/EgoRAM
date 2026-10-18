# Debug device (EGO_DEVICE_DEBUG = 0)

Files: `ego_debug_device.c`, `ego_debug_device.h`, `ego_debug.h` (wire protocol)

The debug device prints firmware state to the log output (USB serial or
UART, see `ego_log`). It is device 0 and therefore active after reset:
a client that sends commands without selecting a device first hits this
harmless device instead of triggering graphics operations. Its only
command besides Init is `EGO_CMD_DEBUG`, which currently prints the
enabled sprites via `gfx_print_debug()`.

Origin: developed in this repository.
