

;Registers within $D5xx
	.enum ego_reg
	CMD = $d500
	STATUS
	DATA
	DEVICE			;device selector (ego_device)
	.ende

EGO_MEM_SCREEN		= $d540	;$20/$28/$30 bytes of screen memory for LMS
EGO_MEM_CHARSET		= $d580	;8 byte of chararacter $30 when CHBAS is at $d400

;Devices, selected by writing the device number to ego_reg.DEVICE
	.enum ego_device
	DEBUG			;active after reset
	GRAPHICS
	MOS6502
	.ende

;Commands are scoped per device (e.g. "lda #ego_gfx_cmd.RENDER_SPRITES").
;Every device's first command is INIT = 0: reset device to power-on state.

;Commands of ego_device.DEBUG
	.enum ego_debug_cmd
	INIT
	DEBUG			;print debug state to log output
	.ende

;Commands of ego_device.GRAPHICS
	.enum ego_gfx_cmd
	INIT
	RENDER_SPRITES
	SET_WRITEABLE
	SET_READONLY
	ABORT
	SHAPE_DATA
	DEL_SHAPE
	SPRITE_DATA
	DEL_SPRITE
	ENA_SPRITE
	DIS_SPRITE
	SET_SPRITE_X
	SET_SPRITE_Y
	SET_SPRITE_XY
	SET_BLIT_WIDTH
	SET_BLIT_HEIGHT
	LINE_PTR
	FILL_MEM
	SPRITE_MODE
	CHARSET			;TODO: SET_CHARSET
	CHAR_TO_VIDEO
	SET_CHAR		;update 8 bytes for single character
	GET_CHAR		;get 8 bytes for single character
	.ende

;Commands of ego_device.MOS6502
	.enum ego_mos6502_cmd
	INIT
	SET_MEMORY		;addr lo/hi, length lo/hi + length data bytes
	GET_MEMORY		;addr lo/hi, length lo/hi -> length data bytes
	EXECUTE			;cycles lo/hi, runs at least that many cycles
	.ende

;Sprite blit modes of the graphics device (ego_gfx_cmd.SPRITE_MODE)
	.enum ego_sprite_mode
	XOR
	MASK
	SPECIAL
	.ende

