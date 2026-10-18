#define EGO_MAX_SPRITES 64
#define EGO_MAX_SHAPES 256

// Operating mode of the cartridge, internal to the firmware.
typedef enum : uint8_t
{
	EGO_MODE_ATARI,
	EGO_MODE_USB
} EgoMode;

#define EGO_UART_ID uart0
#define EGO_BAUD_RATE 115200
#define EGO_UART_TX_PIN 28
#define EGO_UART_RX_PIN 29

// Register offsets within $D5xx. The values are part of the wire protocol
// and must match asm/EgoRAM-Equates.asm.
typedef enum : uint8_t
{
	EGO_REG_CMD,
	EGO_REG_STATUS,
	EGO_REG_DATA
} EgoReg;

_Static_assert(EGO_REG_DATA == 2, "EgoReg values must match asm/EgoRAM-Equates.asm");

// Command bytes written to EGO_REG_CMD. The values are part of the wire protocol
// and must match asm/EgoRAM-Equates.asm, so only append new commands at the end.
typedef enum : uint8_t
{
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

// This is the internal state of the EGO command parser. It is used to keep track of what data is expected next.
// It is not directly related to the commands themselves, but rather to the state of the parser as it processes incoming data.
// Order can be changed without problems, but the order of the states must be maintained for the parser to function correctly.
typedef enum : uint8_t
{
	EGO_ST_IDLE,

	EGO_ST_SHAPE_NO,
	EGO_ST_SHAPE_WIDTH,
	EGO_ST_SHAPE_HEIGHT,
	EGO_ST_SHAPE_BPP,
	EGO_ST_SHAPE_DATA,

	EGO_ST_SPRITE_NO,
	EGO_ST_SPRITE_SHAPE_NO,
	EGO_ST_SPRITE_X_LO,
	EGO_ST_SPRITE_X_HI,
	EGO_ST_SPRITE_Y_LO,
	EGO_ST_SPRITE_Y_HI,
	EGO_ST_SPRITE_ENA,
	EGO_ST_SPRITE_DIS,

	EGO_ST_BLITWITH,
	EGO_ST_BLITHEIGHT,

	EGO_ST_LINE_NO,
	EGO_ST_LINE_DATA_LO,
	EGO_ST_LINE_DATA_HI,

	EGO_ST_FILL_MEM_LO,
	EGO_ST_FILL_MEM_HI,
	EGO_ST_FILL_LEN_LO,
	EGO_ST_FILL_LEN_HI,
	EGO_ST_FILL_BYTE,

	EGO_ST_SPRITE_MODE,

	EGO_ST_CHARSET_NO,
	EGO_ST_CHARSET_DATA,
	EGO_ST_CHR2GFX_SRC_LO,
	EGO_ST_CHR2GFX_SRC_HI,
	EGO_ST_CHR2GFX_SRC_INC_LO,
	EGO_ST_CHR2GFX_SRC_INC_HI,
	EGO_ST_CHR2GFX_DST_LO,
	EGO_ST_CHR2GFX_DST_HI,
	EGO_ST_CHR2GFX_DST_INC_LO,
	EGO_ST_CHR2GFX_DST_INC_HI,
	EGO_ST_CHR2GFX_WIDTH,
	EGO_ST_CHR2GFX_HEIGHT,
	EGO_ST_CHR2GFX_CHARSET_NO,
	EGO_ST_CHR2GFX_SCROLL,
	EGO_ST_CHR2GFX_COL38,
	EGO_ST_CHAR_NO,
	EGO_ST_CHAR_DATA,
	EGO_ST_CHAR_GET_DATA
} EgoState;

// Sprite blit mode, set via EGO_CMD_SPRITE_MODE. The values are part of the
// wire protocol and must match asm/EgoRAM-Equates.asm.
typedef enum : uint8_t
{
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

typedef struct
{
	uint8_t width;
	uint8_t height;
	uint8_t bitsperpix;
	uint8_t *data;
	uint8_t *mask;
	uint8_t *shadow;
} shape_t;

typedef struct
{
	uint8_t enabled;
	EgoSpriteMode mode;
	uint16_t xpos;
	uint16_t ypos;
	uint8_t shape_no;
	uint16_t xold;
	uint16_t yold;
} sprite_t;

void set_blitwidth(uint16_t width);
void set_blitheight(uint16_t heigth);

uint16_t get_blitwidth();
uint16_t get_blitheight();

void sprite_draw_all();
void sprite_draw(sprite_t *sp);
void write_d5xx(uint8_t addr, uint8_t data);
uint8_t read_d5xx(uint8_t addr);
void ego_log(const char *format, ...);
