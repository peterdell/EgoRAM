#define EGO_MAX_SPRITES 32

#define EGO_MODE_ATARI 0
#define EGO_MODE_USB 1

#define EGO_UART_ID uart0
#define EGO_BAUD_RATE 115200
#define EGO_UART_TX_PIN 28
#define EGO_UART_RX_PIN 29

#define EGO_REG_CMD 0
#define EGO_REG_STATUS 1
#define EGO_REG_DATA 2

#define EGO_CMD_RENDER_SPRITES 0
#define EGO_CMD_START_SPRITE_DATA 1
#define EGO_CMD_SET_WRITEABLE 2
#define EGO_CMD_SET_READONLY 3
#define EGO_CMD_ABORT 4

/*
Sprite data:
b00		logical sprite number
b01		width in bytes
b02		height in lines
bxx.yy	width*heigt data
*/

typedef struct
{
	uint8_t enabled;
	uint8_t width;
	uint8_t height;
	uint16_t xpos;
	uint16_t ypos;
	uint8_t *data;
} sprite_t;

void set_blitwidth(int w);
void set_blitheight(int h);

int get_blitwidth();
int get_blitheight();

sprite_t *sprite_new(uint8_t width, uint8_t height);
void sprite_free(sprite_t *sp);
void sprite_draw_all();
void sprite_draw(sprite_t *sp);
void sprite_draw_xy(sprite_t *sp, uint16_t x, uint16_t y);
void write_d5xx(uint8_t addr, uint8_t data);
void ego_log(const char *format, ...);
