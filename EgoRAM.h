// Places the function in RAM (section ".time_critical.EgoRAM") so it runs
// at full speed without flash wait states. Requires the pico SDK headers to
// be included first.
#define RAM_FUNC __not_in_flash("EgoRAM")

#define EGO_MAX_SPRITES 64
#define EGO_MAX_SHAPES 256

// Operating mode of the cartridge, internal to the firmware.
typedef enum : uint8_t {
    EGO_MODE_ATARI,
    EGO_MODE_USB
} EgoMode;

#define EGO_UART_ID uart0
#define EGO_BAUD_RATE 115200
#define EGO_UART_TX_PIN 28
#define EGO_UART_RX_PIN 29

// Register offsets within $D5xx. The values are part of the wire protocol
// and must match asm/EgoRAM-Equates.asm.
typedef enum : uint8_t {
    EGO_REG_CMD,
    EGO_REG_STATUS,
    EGO_REG_DATA,
    EGO_REG_DEVICE
} EgoReg;

_Static_assert(EGO_REG_DEVICE == 3, "EgoReg values must match asm/EgoRAM-Equates.asm");

// Devices inside the cartridge, selected by writing the device number to
// EGO_REG_DEVICE. Graphics is device 0 and active after reset. The values are
// part of the wire protocol and must match asm/EgoRAM-Equates.asm, so only
// append new devices at the end.
typedef enum : uint8_t {
    EGO_DEVICE_GRAPHICS
} EgoDevice;

_Static_assert(EGO_DEVICE_GRAPHICS == 0, "EgoDevice values must match asm/EgoRAM-Equates.asm");

// Command bytes written to EGO_REG_CMD. The values are part of the wire protocol
// and must match asm/EgoRAM-Equates.asm, so only append new commands at the end.
typedef enum : uint8_t {
    EGO_CMD_RENDER_SPRITES,
    EGO_CMD_SET_WRITEABLE,
    EGO_CMD_SET_READONLY,
    EGO_CMD_ABORT,
    EGO_CMD_SHAPE_DATA,
    EGO_CMD_DEL_SHAPE,
    EGO_CMD_SPRITE_DATA,
    EGO_CMD_DEL_SPRITE,
    EGO_CMD_ENA_SPRITE,
    EGO_CMD_DIS_SPRITE,
    EGO_CMD_SET_SPRITE_X,
    EGO_CMD_SET_SPRITE_Y,
    EGO_CMD_SET_SPRITE_XY,
    EGO_CMD_SET_BLIT_WIDTH,
    EGO_CMD_SET_BLIT_HEIGHT,
    EGO_CMD_LINE_PTR,
    EGO_CMD_FILL_MEM,
    EGO_CMD_SPRITE_MODE,
    EGO_CMD_CHARSET,
    EGO_CMD_CHAR_TO_VIDEO,
    EGO_CMD_SET_CHAR,
    EGO_CMD_GET_CHAR,

    EGO_CMD_DEBUG = 99
} EgoCmd;

_Static_assert(EGO_CMD_GET_CHAR == 21, "EgoCmd values must match asm/EgoRAM-Equates.asm");

// Phase of the EGO command parser. Each command's fixed parameter bytes are
// described by the command table in EgoRAM.c; bulk payloads are streamed in
// the DATA phases. See plans/03_COMMAND_TABLE.md.
typedef enum : uint8_t {
    EGO_ST_IDLE,    // no command in progress
    EGO_ST_HEADER,  // collecting the fixed parameter bytes of a command
    EGO_ST_DATA_IN, // streaming bulk data from the Atari
    EGO_ST_DATA_OUT // streaming bulk data to the Atari
} EgoState;

// Sprite blit mode, set via EGO_CMD_SPRITE_MODE. The values are part of the
// wire protocol and must match asm/EgoRAM-Equates.asm.
typedef enum : uint8_t {
    EGO_MODE_XOR,
    EGO_MODE_MASK,
    EGO_MODE_SPECIAL
} EgoSpriteMode;

_Static_assert(EGO_MODE_SPECIAL == 2, "EgoSpriteMode values must match asm/EgoRAM-Equates.asm");

/*
Sprite data:
b00		logical sprite number
b01		width in bytes
b02		height in lines
bxx.yy	width*heigt data
*/

typedef struct {
    uint8_t width;
    uint8_t height;
    uint8_t bitsperpix;
    uint8_t *data;
    uint8_t *mask;
    uint8_t *shadow;
} shape_t;

typedef struct {
    uint8_t enabled;
    EgoSpriteMode mode;
    uint16_t xpos;
    uint16_t ypos;
    uint8_t shape_no;
    uint16_t xold;
    uint16_t yold;
} sprite_t;

// State of the graphics device (EGO_DEVICE_GRAPHICS), the only device
// implemented so far. Future devices group their state the same way.
typedef struct {
    uint16_t blitwidth;        // blit area width in bytes
    uint16_t blitheight;       // blit area height in lines
    uint32_t line_no;          // line count of a pending LINE_PTR stream
    uint16_t line_ptr[256];    // video RAM offset of each screen line
    uint8_t line_staging[512]; // LINE_PTR DATA_IN buffer (lo/hi pairs)
    shape_t shapes[EGO_MAX_SHAPES];
    sprite_t sprites[EGO_MAX_SPRITES];
    uint8_t *charsets[256];
} ego_gfx_t;

void set_blitwidth(uint16_t width);
void set_blitheight(uint16_t heigth);

uint16_t get_blitwidth();
uint16_t get_blitheight();

void sprite_draw_all();
void sprite_draw(sprite_t *sp);
void write_d5xx(uint8_t addr, uint8_t data);
uint8_t read_d5xx(uint8_t addr);
void ego_log(const char *format, ...);
