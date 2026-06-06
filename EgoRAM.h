#define MAX_SPRITES 32

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
