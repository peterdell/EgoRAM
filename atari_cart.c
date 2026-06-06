/**
 *    _   ___ ___ _       ___          _
 *   /_\ ( _ ) _ (_)__ _ / __|__ _ _ _| |_
 *  / _ \/ _ \  _/ / _/_\ (__/ _` | '_|  _|
 * /_/ \_\___/_| |_\__\_/\___\__,_|_|  \__|
 *
 *
 * Atari 8-bit cartridge for Raspberry Pi Pico
 *
 * Robin Edwards 2023
 *
 * Needs to be a release NOT debug build for the cartridge emulation to work
 *
 * Changes from UnoCart:
 * - Attempts to get S4/S5, RD4/RD5 MMU behaviour correct on 400/800
 *   https://forums.atariage.com/topic/241888-ultimate-cart-sd-multicart-technical-thread/page/10/#comment-4266797
 * - Adds 4k carts (CAR type 58)
 * - Adds Turbsoft carts (CAR types 50,51)
 * - Adds ATRAX 128k carts (CAR type 17)
 * - Adds Microcalc/Utracart (CAR type 52)
 * - Adds Standard 2k cars (CAR type 57)
 * - Adds Phoenix 8k cars (CAR type 39)
 * - Adds Blizzard 4k cars (CAR type 46)
 */

#include <string.h>
#include <stdlib.h>
#include "pico/multicore.h"

#include "pico/stdlib.h"
#include "hardware/sync.h"
#include "hardware/clocks.h"

#define PAL_WIDTH 80 // define 80 columns by 25 rows
#define PAL_HEIGHT 25

// #include "ff.h"
// #include "fatfs_disk.h"

#define ALL_GPIO_MASK 0x3FFFFFFF
#define ADDR_GPIO_MASK 0x00001FFF
#define DATA_GPIO_MASK 0x001FE000
#define CCTL_GPIO_MASK 0x00200000 // gpio 21
#define PHI2_GPIO_MASK 0x00400000 // gpio 22
#define RW_GPIO_MASK 0x00800000	  // gpio 23
#define S4_GPIO_MASK 0x01000000	  // gpio 24
#define S5_GPIO_MASK 0x02000000	  // gpio 25

#define S4_S5_GPIO_MASK 0x03000000
#define CCTL_RW_GPIO_MASK 0x00A00000

#define RD4_PIN 26
#define RD5_PIN 27

#define RD4_LOW gpio_put(RD4_PIN, 0)
#define RD4_HIGH gpio_put(RD4_PIN, 1)
#define RD5_LOW gpio_put(RD5_PIN, 0)
#define RD5_HIGH gpio_put(RD5_PIN, 1)
#define SET_DATA_MODE_OUT gpio_set_dir_out_masked(DATA_GPIO_MASK)
#define SET_DATA_MODE_IN gpio_set_dir_in_masked(DATA_GPIO_MASK)

// #include "rom.h"
// #include "osrom.h"

unsigned char cart_ram[1 * 1024];
unsigned char cart_d5xx[256] = {0};
char errorBuf[40];

#define CART_CMD_OPEN_ITEM 0x00
#define CART_CMD_READ_CUR_DIR 0x01
#define CART_CMD_GET_DIR_ENTRY 0x02
#define CART_CMD_UP_DIR 0x03
#define CART_CMD_ROOT_DIR 0x04
#define CART_CMD_SEARCH 0x05
#define CART_CMD_LOAD_SOFT_OS 0x10
#define CART_CMD_SOFT_OS_CHUNK 0x11
#define CART_CMD_MOUNT_ATR 0x20 // unused, done automatically by firmware
#define CART_CMD_READ_ATR_SECTOR 0x21
#define CART_CMD_WRITE_ATR_SECTOR 0x22
#define CART_CMD_ATR_HEADER 0x23
#define CART_CMD_RESET_FLASH 0xF0
#define CART_CMD_NO_CART 0xFE
#define CART_CMD_ACTIVATE_CART 0xFF

#define CART_TYPE_NONE 0
#define CART_TYPE_8K 1				// 8k
#define CART_TYPE_16K 2				// 16k
#define CART_TYPE_XEGS_32K 3		// 32k
#define CART_TYPE_XEGS_64K 4		// 64k
#define CART_TYPE_XEGS_128K 5		// 128k
#define CART_TYPE_SW_XEGS_32K 6		// 32k
#define CART_TYPE_SW_XEGS_64K 7		// 64k
#define CART_TYPE_SW_XEGS_128K 8	// 128k
#define CART_TYPE_MEGACART_16K 9	// 16k
#define CART_TYPE_MEGACART_32K 10	// 32k
#define CART_TYPE_MEGACART_64K 11	// 64k
#define CART_TYPE_MEGACART_128K 12	// 128k
#define CART_TYPE_BOUNTY_BOB 13		// 40k
#define CART_TYPE_ATARIMAX_1MBIT 14 // 128k
#define CART_TYPE_WILLIAMS_64K 15	// 32k/64k
#define CART_TYPE_OSS_16K_TYPE_B 16 // 16k
#define CART_TYPE_OSS_8K 17			// 8k
#define CART_TYPE_OSS_16K_034M 18	// 16k
#define CART_TYPE_OSS_16K_043M 19	// 16k
#define CART_TYPE_SIC_128K 20		// 128k
#define CART_TYPE_SDX_64K 21		// 64k
#define CART_TYPE_SDX_128K 22		// 128k
#define CART_TYPE_DIAMOND_64K 23	// 64k
#define CART_TYPE_EXPRESS_64K 24	// 64k
#define CART_TYPE_BLIZZARD_16K 25	// 16k
#define CART_TYPE_4K 26				// 4k
#define CART_TYPE_TURBOSOFT_64K 27	// 64k
#define CART_TYPE_TURBOSOFT_128K 28 // 128k
#define CART_TYPE_ATRAX_128K 29		// 128k
#define CART_TYPE_MICROCALC 30		// 32k
#define CART_TYPE_2K 31				// 2k
#define CART_TYPE_PHOENIX_8K 32		// 8k
#define CART_TYPE_BLIZZARD_4K 33	// 4k
#define CART_ROLAND 128
#define CART_TYPE_ATR 254
#define CART_TYPE_XEX 255

extern uint8_t __attribute__((aligned(4))) video_ram[PAL_WIDTH * PAL_HEIGHT * 8];

void __not_in_flash_func(emulate_standard_8k)()
{
	// 8k
	RD4_LOW;
	RD5_HIGH;

	uint32_t pins;
	uint16_t addr;
	while (1)
	{
		// wait for s5 low
		while ((pins = gpio_get_all()) & S5_GPIO_MASK)
			;
		SET_DATA_MODE_OUT;
		// while s5 low
		while (!((pins = gpio_get_all()) & S5_GPIO_MASK))
		{
			addr = pins & ADDR_GPIO_MASK;
			gpio_put_masked(DATA_GPIO_MASK, ((uint32_t)cart_ram[addr]) << 13);
		}
		SET_DATA_MODE_IN;
	}
}

void __not_in_flash_func(emulate_roland)()
{
	// 8k
	RD4_LOW;
	RD5_HIGH;

	bool rd5_high = true;
	uint32_t last;
	uint8_t data;

	uint32_t pins;
	uint16_t addr;
	while (1)
	{
		// wait for phi2 high
		while (!((pins = gpio_get_all()) & PHI2_GPIO_MASK))
			;

		if (!(pins & S5_GPIO_MASK) && rd5_high)
		{ // s5 low
			SET_DATA_MODE_OUT;
			addr = pins & ADDR_GPIO_MASK;
			gpio_put_masked(DATA_GPIO_MASK, ((uint32_t)video_ram[addr]) << 13);
			// wait for phi2 low
			while (gpio_get_all() & PHI2_GPIO_MASK)
				;
			SET_DATA_MODE_IN;
		}
		else if (!(pins & CCTL_RW_GPIO_MASK))
		{ // CCTL low + write
			last = pins;
			// read data bus on falling edge of phi2
			while ((pins = gpio_get_all()) & PHI2_GPIO_MASK)
				last = pins;
			data = (last & DATA_GPIO_MASK) >> 13;
			multicore_fifo_push_blocking(42);
		}
	}
}

void __not_in_flash_func(emulate_standard_16k)()
{
	// 16k
	RD4_HIGH;
	RD5_HIGH;

	uint32_t pins;
	uint16_t addr;
	while (1)
	{
		// wait for either s4 or s5 low
		while (((pins = gpio_get_all()) & S4_S5_GPIO_MASK) == S4_S5_GPIO_MASK)
			;
		SET_DATA_MODE_OUT;
		if (!(pins & S4_GPIO_MASK))
		{
			// while s4 low
			while (!((pins = gpio_get_all()) & S4_GPIO_MASK))
			{
				addr = pins & ADDR_GPIO_MASK;
				gpio_put_masked(DATA_GPIO_MASK, ((uint32_t)cart_ram[addr]) << 13);
			}
		}
		else
		{
			// while s5 low
			while (!((pins = gpio_get_all()) & S5_GPIO_MASK))
			{
				addr = pins & ADDR_GPIO_MASK;
				gpio_put_masked(DATA_GPIO_MASK, ((uint32_t)cart_ram[0x2000 | addr]) << 13);
			}
		}
		SET_DATA_MODE_IN;
	}
}

void __not_in_flash_func(emulate_XEGS_32k)(char switchable)
{
	// 32k
	RD4_HIGH;
	RD5_HIGH;

	uint32_t pins, last;
	uint16_t addr;
	uint8_t data;
	unsigned char *bankPtr = &cart_ram[0];
	bool rd4_high = true, rd5_high = true; // 400/800 MMU

	while (1)
	{
		// wait for phi2 high
		while (!((pins = gpio_get_all()) & PHI2_GPIO_MASK))
			;

		if (!(pins & S4_GPIO_MASK) && rd4_high)
		{ // s4 low
			SET_DATA_MODE_OUT;
			addr = pins & ADDR_GPIO_MASK;
			gpio_put_masked(DATA_GPIO_MASK, ((uint32_t)(*(bankPtr + addr))) << 13);
			// wait for phi2 low
			while (gpio_get_all() & PHI2_GPIO_MASK)
				;
			SET_DATA_MODE_IN;
		}
		else if (!(pins & S5_GPIO_MASK) && rd5_high)
		{ // s5 low
			SET_DATA_MODE_OUT;
			addr = pins & ADDR_GPIO_MASK;
			gpio_put_masked(DATA_GPIO_MASK, ((uint32_t)cart_ram[0x6000 | addr]) << 13);
			// wait for phi2 low
			while (gpio_get_all() & PHI2_GPIO_MASK)
				;
			SET_DATA_MODE_IN;
		}
		else if (!(pins & CCTL_RW_GPIO_MASK))
		{ // CCTL low + write
			last = pins;
			// read data bus on falling edge of phi2
			while ((pins = gpio_get_all()) & PHI2_GPIO_MASK)
				last = pins;
			data = (last & DATA_GPIO_MASK) >> 13;
			// new bank is the low 2 bits written to $D5xx
			bankPtr = &cart_ram[0] + (8192 * (data & 3));
			if (switchable)
			{
				if (data & 0x80)
				{
					RD4_LOW;
					RD5_LOW;
					rd4_high = rd5_high = false;
				}
				else
				{
					RD4_HIGH;
					RD5_HIGH;
					rd4_high = rd5_high = true;
				}
			}
		}
	}
}

void __not_in_flash_func(emulate_XEGS_64k)(char switchable)
{
	// 64k
	RD4_HIGH;
	RD5_HIGH;

	uint32_t pins, last;
	uint16_t addr;
	uint8_t data;
	unsigned char *bankPtr = &cart_ram[0];
	bool rd4_high = true, rd5_high = true; // 400/800 MMU

	while (1)
	{
		// wait for phi2 high
		while (!((pins = gpio_get_all()) & PHI2_GPIO_MASK))
			;

		if (!(pins & S4_GPIO_MASK) && rd4_high)
		{ // s4 low
			SET_DATA_MODE_OUT;
			addr = pins & ADDR_GPIO_MASK;
			gpio_put_masked(DATA_GPIO_MASK, ((uint32_t)(*(bankPtr + addr))) << 13);
			// wait for phi2 low
			while (gpio_get_all() & PHI2_GPIO_MASK)
				;
			SET_DATA_MODE_IN;
		}
		else if (!(pins & S5_GPIO_MASK) && rd5_high)
		{ // s5 low
			SET_DATA_MODE_OUT;
			addr = pins & ADDR_GPIO_MASK;
			gpio_put_masked(DATA_GPIO_MASK, ((uint32_t)cart_ram[0xE000 | addr]) << 13);
			// wait for phi2 low
			while (gpio_get_all() & PHI2_GPIO_MASK)
				;
			SET_DATA_MODE_IN;
		}
		else if (!(pins & CCTL_RW_GPIO_MASK))
		{ // CCTL low + write
			last = pins;
			// read data bus on falling edge of phi2
			while ((pins = gpio_get_all()) & PHI2_GPIO_MASK)
				last = pins;
			data = (last & DATA_GPIO_MASK) >> 13;
			// new bank is the low 3 bits written to $D5xx
			bankPtr = &cart_ram[0] + (8192 * (data & 7));
			if (switchable)
			{
				if (data & 0x80)
				{
					RD4_LOW;
					RD5_LOW;
					rd4_high = rd5_high = false;
				}
				else
				{
					RD4_HIGH;
					RD5_HIGH;
					rd4_high = rd5_high = true;
				}
			}
		}
	}
}

void __not_in_flash_func(emulate_XEGS_128k)(char switchable)
{
	// 128k
	RD4_HIGH;
	RD5_HIGH;

	uint32_t pins, last;
	uint16_t addr;
	uint8_t data;
	unsigned char *bankPtr = &cart_ram[0];
	bool rd4_high = true, rd5_high = true; // 400/800 MMU

	while (1)
	{
		// wait for phi2 high
		while (!((pins = gpio_get_all()) & PHI2_GPIO_MASK))
			;

		if (!(pins & S4_GPIO_MASK) && rd4_high)
		{ // s4 low
			SET_DATA_MODE_OUT;
			addr = pins & ADDR_GPIO_MASK;
			gpio_put_masked(DATA_GPIO_MASK, ((uint32_t)(*(bankPtr + addr))) << 13);
			// wait for phi2 low
			while (gpio_get_all() & PHI2_GPIO_MASK)
				;
			SET_DATA_MODE_IN;
		}
		else if (!(pins & S5_GPIO_MASK) && rd5_high)
		{ // s5 low
			SET_DATA_MODE_OUT;
			addr = pins & ADDR_GPIO_MASK;
			gpio_put_masked(DATA_GPIO_MASK, ((uint32_t)cart_ram[0x1E000 | addr]) << 13);
			// wait for phi2 low
			while (gpio_get_all() & PHI2_GPIO_MASK)
				;
			SET_DATA_MODE_IN;
		}
		else if (!(pins & CCTL_RW_GPIO_MASK))
		{ // CCTL low + write
			last = pins;
			// read data bus on falling edge of phi2
			while ((pins = gpio_get_all()) & PHI2_GPIO_MASK)
				last = pins;
			data = (last & DATA_GPIO_MASK) >> 13;
			// new bank is the low 4 bits written to $D5xx
			bankPtr = &cart_ram[0] + (8192 * (data & 0xF));
			if (switchable)
			{
				if (data & 0x80)
				{
					RD4_LOW;
					RD5_LOW;
					rd4_high = rd5_high = false;
				}
				else
				{
					RD4_HIGH;
					RD5_HIGH;
					rd4_high = rd5_high = true;
				}
			}
		}
	}
}

void __not_in_flash_func(emulate_bounty_bob)()
{
	// 40k
	RD4_HIGH;
	RD5_HIGH;

	uint32_t pins;
	uint16_t addr;
	unsigned char *bankPtr1 = &cart_ram[0], *bankPtr2 = &cart_ram[0x4000];
	while (1)
	{
		// wait for phi2 high
		while (!((pins = gpio_get_all()) & PHI2_GPIO_MASK))
			;

		if (!(pins & S4_GPIO_MASK))
		{ // s4 low
			SET_DATA_MODE_OUT;
			addr = pins & ADDR_GPIO_MASK;
			if (addr & 0x1000)
			{
				gpio_put_masked(DATA_GPIO_MASK, ((uint32_t)(*(bankPtr2 + (addr & 0xFFF)))) << 13);
				if (addr == 0x1FF6)
					bankPtr2 = &cart_ram[0x4000];
				else if (addr == 0x1FF7)
					bankPtr2 = &cart_ram[0x5000];
				else if (addr == 0x1FF8)
					bankPtr2 = &cart_ram[0x6000];
				else if (addr == 0x1FF9)
					bankPtr2 = &cart_ram[0x7000];
			}
			else
			{
				gpio_put_masked(DATA_GPIO_MASK, ((uint32_t)(*(bankPtr1 + (addr & 0xFFF)))) << 13);
				if (addr == 0x0FF6)
					bankPtr1 = &cart_ram[0];
				else if (addr == 0x0FF7)
					bankPtr1 = &cart_ram[0x1000];
				else if (addr == 0x0FF8)
					bankPtr1 = &cart_ram[0x2000];
				else if (addr == 0x0FF9)
					bankPtr1 = &cart_ram[0x3000];
			}
		}
		else if (!(pins & S5_GPIO_MASK))
		{ // s5 low
			SET_DATA_MODE_OUT;
			addr = pins & ADDR_GPIO_MASK;
			gpio_put_masked(DATA_GPIO_MASK, ((uint32_t)cart_ram[0x8000 | addr]) << 13);
		}
		// wait for phi2 low
		while (gpio_get_all() & PHI2_GPIO_MASK)
			;
		SET_DATA_MODE_IN;
	}
}

void __not_in_flash_func(emulate_atarimax_128k)()
{
	// 128k
	RD4_LOW;
	RD5_HIGH;

	uint32_t bank = 0;
	unsigned char *ramPtr;
	uint32_t pins;
	uint16_t addr;
	bool rd5_high = true; // 400/800 MMU

	while (1)
	{
		// select the right SRAM base, based on the cartridge bank
		ramPtr = &cart_ram[0] + (8192 * (bank & 0xF));

		// wait for phi2 high
		while (!((pins = gpio_get_all()) & PHI2_GPIO_MASK))
			;

		if (!(pins & S5_GPIO_MASK) && rd5_high)
		{ // s5 low
			SET_DATA_MODE_OUT;
			addr = pins & ADDR_GPIO_MASK;
			gpio_put_masked(DATA_GPIO_MASK, ((uint32_t)(*(ramPtr + addr))) << 13);
		}
		else if (!(pins & CCTL_GPIO_MASK))
		{ // CCTL low
			addr = pins & ADDR_GPIO_MASK;
			if ((addr & 0xE0) == 0)
			{
				bank = addr & 0xF;
				if (addr & 0x10)
				{
					RD5_LOW;
					rd5_high = false;
				}
				else
				{
					RD5_HIGH;
					rd5_high = true;
				}
			}
		}
		// wait for phi2 low
		while (gpio_get_all() & PHI2_GPIO_MASK)
			;
		SET_DATA_MODE_IN;
	}
}

void __not_in_flash_func(emulate_williams)()
{
	// williams 32k, 64k
	RD4_LOW;
	RD5_HIGH;

	uint32_t bank = 0;
	unsigned char *bankPtr;
	uint32_t pins;
	uint16_t addr;
	bool rd5_high = true; // 400/800 MMU

	while (1)
	{
		// select the right SRAM base, based on the cartridge bank
		bankPtr = &cart_ram[0] + (8192 * bank);
		// wait for phi2 high
		while (!((pins = gpio_get_all()) & PHI2_GPIO_MASK))
			;

		if (!(pins & S5_GPIO_MASK) && rd5_high)
		{ // s5 low
			SET_DATA_MODE_OUT;
			addr = pins & ADDR_GPIO_MASK;
			gpio_put_masked(DATA_GPIO_MASK, ((uint32_t)(*(bankPtr + addr))) << 13);
		}
		else if (!(pins & CCTL_GPIO_MASK))
		{ // CCTL low
			addr = pins & ADDR_GPIO_MASK;
			if ((addr & 0xF0) == 0)
			{
				bank = addr & 0x07;
				if (addr & 0x08)
				{
					RD5_LOW;
					rd5_high = false;
				}
				else
				{
					RD5_HIGH;
					rd5_high = true;
				}
			}
		}
		// wait for phi2 low
		while (gpio_get_all() & PHI2_GPIO_MASK)
			;
		SET_DATA_MODE_IN;
	}
}

void __not_in_flash_func(emulate_OSS_B)()
{
	// OSS type B
	RD5_HIGH;
	RD4_LOW;
	uint32_t pins;
	uint16_t addr;
	uint32_t bank = 1;
	unsigned char *bankPtr;
	bool rd5_high = true; // 400/800 MMU

	while (1)
	{
		// select the right SRAM block, based on the cartridge bank
		bankPtr = &cart_ram[0] + (4096 * bank);
		// wait for phi2 high
		while (!((pins = gpio_get_all()) & PHI2_GPIO_MASK))
			;

		if (!(pins & S5_GPIO_MASK) && rd5_high)
		{ // s5 low
			SET_DATA_MODE_OUT;
			addr = pins & ADDR_GPIO_MASK;
			if (addr & 0x1000)
				gpio_put_masked(DATA_GPIO_MASK, ((uint32_t)cart_ram[addr & 0xFFF]) << 13);
			else
				gpio_put_masked(DATA_GPIO_MASK, ((uint32_t)(*(bankPtr + addr))) << 13);
		}
		else if (!(pins & CCTL_GPIO_MASK))
		{ // CCTL low
			addr = pins & ADDR_GPIO_MASK;
			int a0 = addr & 1, a3 = addr & 8;
			if (a3 && !a0)
			{
				RD5_LOW;
				rd5_high = false;
			}
			else
			{
				RD5_HIGH;
				rd5_high = true;
				if (!a3 && !a0)
					bank = 1;
				else if (!a3 && a0)
					bank = 3;
				else if (a3 && a0)
					bank = 2;
			}
		}
		// wait for phi2 low
		while (gpio_get_all() & PHI2_GPIO_MASK)
			;
		SET_DATA_MODE_IN;
	}
}

void __not_in_flash_func(emulate_OSS_A)(char is034M)
{
	// OSS type A (034M, 043M)
	RD5_HIGH;
	RD4_LOW;
	uint32_t pins;
	uint16_t addr;
	uint32_t bank = 0;
	unsigned char *bankPtr;
	bool rd5_high = true; // 400/800 MMU

	while (1)
	{
		// select the right SRAM block, based on the cartridge bank
		bankPtr = &cart_ram[0] + (4096 * bank);
		// wait for phi2 high
		while (!((pins = gpio_get_all()) & PHI2_GPIO_MASK))
			;

		if (!(pins & S5_GPIO_MASK) && rd5_high)
		{ // s5 low
			SET_DATA_MODE_OUT;
			addr = pins & ADDR_GPIO_MASK;
			if (addr & 0x1000)
				gpio_put_masked(DATA_GPIO_MASK, ((uint32_t)cart_ram[addr | 0x2000]) << 13); // 4k bank #3 always mapped to $Bxxx
			else
				gpio_put_masked(DATA_GPIO_MASK, ((uint32_t)(*(bankPtr + addr))) << 13);
		}
		else if (!(pins & CCTL_GPIO_MASK))
		{ // CCTL low
			addr = pins & 0xF;
			if (addr & 0x8)
			{
				RD5_LOW;
				rd5_high = false;
			}
			else
			{
				RD5_HIGH;
				rd5_high = true;
				if (addr == 0x0)
					bank = 0;
				if (addr == 0x3 || addr == 0x7)
					bank = is034M ? 1 : 2;
				if (addr == 0x4)
					bank = is034M ? 2 : 1;
			}
		}
		// wait for phi2 low
		while (gpio_get_all() & PHI2_GPIO_MASK)
			;
		SET_DATA_MODE_IN;
	}
}

void __not_in_flash_func(emulate_megacart)(int size)
{
	// 16k - 128k
	RD4_HIGH;
	RD5_HIGH;

	uint32_t pins, last;
	uint16_t addr;
	uint8_t data;
	uint32_t bank_mask = 0x00;
	if (size == 32)
		bank_mask = 0x1;
	else if (size == 64)
		bank_mask = 0x3;
	else if (size == 128)
		bank_mask = 0x7;

	bool rd4_high = true, rd5_high = true; // 400/800 MMU

	unsigned char *ramPtr = &cart_ram[0];
	while (1)
	{
		// wait for phi2 high
		while (!((pins = gpio_get_all()) & PHI2_GPIO_MASK))
			;

		if (!(pins & S4_GPIO_MASK) && rd4_high)
		{ // s4 low
			SET_DATA_MODE_OUT;
			addr = pins & ADDR_GPIO_MASK;
			gpio_put_masked(DATA_GPIO_MASK, ((uint32_t)(*(ramPtr + addr))) << 13);
			// wait for phi2 low
			while (gpio_get_all() & PHI2_GPIO_MASK)
				;
			SET_DATA_MODE_IN;
		}
		else if (!(pins & S5_GPIO_MASK) && rd5_high)
		{ // s5 low
			SET_DATA_MODE_OUT;
			addr = pins & ADDR_GPIO_MASK;
			gpio_put_masked(DATA_GPIO_MASK, ((uint32_t)(*(ramPtr + (addr | 0x2000)))) << 13);
			// wait for phi2 low
			while (gpio_get_all() & PHI2_GPIO_MASK)
				;
			SET_DATA_MODE_IN;
		}
		else if (!(pins & CCTL_RW_GPIO_MASK))
		{ // CCTL low + write
			last = pins;
			// read data bus on falling edge of phi2
			while ((pins = gpio_get_all()) & PHI2_GPIO_MASK)
				last = pins;
			data = (last & DATA_GPIO_MASK) >> 13;
			// new bank is the low n bits written to $D5xx
			int bank = data & bank_mask;
			ramPtr = &cart_ram[0] + 16384 * (bank & 0x7);
			if (data & 0x80)
			{
				RD4_LOW;
				RD5_LOW;
				rd4_high = rd5_high = false;
			}
			else
			{
				RD4_HIGH;
				RD5_HIGH;
				rd4_high = rd5_high = true;
			}
		}
	}
}

void __not_in_flash_func(emulate_SIC)()
{
	// 128k
	RD5_HIGH;
	RD4_LOW;

	uint32_t pins, last;
	uint16_t addr;
	uint8_t SIC_byte = 0;
	unsigned char *ramPtr = &cart_ram[0];
	bool rd4_high = false, rd5_high = true; // 400/800 MMU

	while (1)
	{
		// wait for phi2 high
		while (!((pins = gpio_get_all()) & PHI2_GPIO_MASK))
			;

		if (!(pins & S4_GPIO_MASK) && rd4_high)
		{ // s4 low
			SET_DATA_MODE_OUT;
			addr = pins & ADDR_GPIO_MASK;
			gpio_put_masked(DATA_GPIO_MASK, ((uint32_t)(*(ramPtr + addr))) << 13);
			// wait for phi2 low
			while (gpio_get_all() & PHI2_GPIO_MASK)
				;
			SET_DATA_MODE_IN;
		}
		else if (!(pins & S5_GPIO_MASK) && rd5_high)
		{ // s5 low
			SET_DATA_MODE_OUT;
			addr = pins & ADDR_GPIO_MASK;
			gpio_put_masked(DATA_GPIO_MASK, ((uint32_t)(*(ramPtr + (addr | 0x2000)))) << 13);
			// wait for phi2 low
			while (gpio_get_all() & PHI2_GPIO_MASK)
				;
			SET_DATA_MODE_IN;
		}
		else if (!(pins & CCTL_GPIO_MASK))
		{ // CCTL low
			addr = pins & ADDR_GPIO_MASK;
			if ((addr & 0xE0) == 0)
			{
				if (pins & RW_GPIO_MASK)
				{ // read from $D5xx
					SET_DATA_MODE_OUT;
					gpio_put_masked(DATA_GPIO_MASK, ((uint32_t)SIC_byte) << 13);
					// wait for phi2 low
					while (gpio_get_all() & PHI2_GPIO_MASK)
						;
					SET_DATA_MODE_IN;
				}
				else
				{ // write to $D5xx
					last = pins;
					// read data bus on falling edge of phi2
					while ((pins = gpio_get_all()) & PHI2_GPIO_MASK)
						last = pins;
					SIC_byte = (last & DATA_GPIO_MASK) >> 13;
					// switch bank
					ramPtr = &cart_ram[0] + 16384 * (SIC_byte & 0x7);
					if (SIC_byte & 0x40)
					{
						RD5_LOW;
						rd5_high = false;
					}
					else
					{
						RD5_HIGH;
						rd5_high = true;
					}
					if (SIC_byte & 0x20)
					{
						RD4_HIGH;
						rd4_high = true;
					}
					else
					{
						RD4_LOW;
						rd4_high = false;
					}
				}
			}
		}
	}
}

void __not_in_flash_func(emulate_SDX)(int size)
{
	// 64k/128k
	RD5_HIGH;
	RD4_LOW;

	unsigned char *ramPtr = &cart_ram[0];
	uint32_t pins;
	uint16_t addr;
	bool rd5_high = true; // 400/800 MMU

	while (1)
	{
		// wait for phi2 high
		while (!((pins = gpio_get_all()) & PHI2_GPIO_MASK))
			;

		if (!(pins & S5_GPIO_MASK) && rd5_high)
		{ // s5 low
			SET_DATA_MODE_OUT;
			addr = pins & ADDR_GPIO_MASK;
			gpio_put_masked(DATA_GPIO_MASK, ((uint32_t)(*(ramPtr + addr))) << 13);
		}
		else if (!(pins & CCTL_GPIO_MASK))
		{ // CCTL low
			addr = pins & ADDR_GPIO_MASK;
			if ((addr & 0xF0) == 0xE0)
			{
				// 64k & 128k versions
				if (size == 64)
					ramPtr = &cart_ram[0];
				else
					ramPtr = &cart_ram[65536];
				ramPtr += ((~addr) & 0x7) * 8192;
				if (addr & 0x8)
				{
					RD5_LOW;
					rd5_high = false;
				}
				else
				{
					RD5_HIGH;
					rd5_high = true;
				}
			}
			if (size == 128 && (addr & 0xF0) == 0xF0)
			{
				// 128k version only
				ramPtr = &cart_ram[0] + ((~addr) & 0x7) * 8192;
				if (addr & 0x8)
				{
					RD5_LOW;
					rd5_high = false;
				}
				else
				{
					RD5_HIGH;
					rd5_high = true;
				}
			}
		}
		// wait for phi2 low
		while (gpio_get_all() & PHI2_GPIO_MASK)
			;
		SET_DATA_MODE_IN;
	}
}

void __not_in_flash_func(emulate_diamond_express)(uint8_t cctlAddr)
{
	// 64k
	RD5_HIGH;
	RD4_LOW;

	unsigned char *ramPtr = &cart_ram[0];
	uint32_t pins;
	uint16_t addr;
	bool rd5_high = true; // 400/800 MMU

	while (1)
	{
		// wait for phi2 high
		while (!((pins = gpio_get_all()) & PHI2_GPIO_MASK))
			;

		if (!(pins & S5_GPIO_MASK) && rd5_high)
		{ // s5 low
			SET_DATA_MODE_OUT;
			addr = pins & ADDR_GPIO_MASK;
			gpio_put_masked(DATA_GPIO_MASK, ((uint32_t)(*(ramPtr + addr))) << 13);
		}
		else if (!(pins & CCTL_GPIO_MASK))
		{ // CCTL low
			addr = pins & ADDR_GPIO_MASK;
			if ((addr & 0xF0) == cctlAddr)
			{
				ramPtr = &cart_ram[0] + ((~addr) & 0x7) * 8192;
				if (addr & 0x8)
				{
					RD5_LOW;
					rd5_high = false;
				}
				else
				{
					RD5_HIGH;
					rd5_high = true;
				}
			}
		}
		// wait for phi2 low
		while (gpio_get_all() & PHI2_GPIO_MASK)
			;
		SET_DATA_MODE_IN;
	}
}

void __not_in_flash_func(emulate_blizzard)()
{
	// 16k
	RD4_HIGH;
	RD5_HIGH;
	uint32_t pins;
	uint16_t addr;
	bool rd4_high = true, rd5_high = true; // 400/800 MMU

	while (1)
	{
		// wait for phi2 high
		while (!((pins = gpio_get_all()) & PHI2_GPIO_MASK))
			;

		if (!(pins & S4_GPIO_MASK) && rd4_high)
		{ // s4 low
			SET_DATA_MODE_OUT;
			addr = pins & ADDR_GPIO_MASK;
			gpio_put_masked(DATA_GPIO_MASK, ((uint32_t)cart_ram[addr]) << 13);
		}
		else if (!(pins & S5_GPIO_MASK) && rd5_high)
		{ // s5 low
			SET_DATA_MODE_OUT;
			addr = pins & ADDR_GPIO_MASK;
			gpio_put_masked(DATA_GPIO_MASK, ((uint32_t)cart_ram[0x2000 | addr]) << 13);
		}
		else if (!(pins & CCTL_GPIO_MASK))
		{ // CCTL low
			RD4_LOW;
			RD5_LOW;
			rd4_high = rd5_high = false;
		}
		// wait for phi2 low
		while (gpio_get_all() & PHI2_GPIO_MASK)
			;
		SET_DATA_MODE_IN;
	}
}

void __not_in_flash_func(emulate_turbosoft)(int size)
{
	// 64k/128k
	RD4_LOW;
	RD5_HIGH;

	uint32_t bank = 0;
	unsigned char *bankPtr;
	uint32_t pins;
	uint16_t addr;
	bool rd5_high = true; // 400/800 MMU

	uint32_t bank_mask = 0x00;
	if (size == 64)
		bank_mask = 0x7;
	else if (size == 128)
		bank_mask = 0xF;

	while (1)
	{
		// select the right SRAM base, based on the cartridge bank
		bankPtr = &cart_ram[0] + (8192 * bank);
		// wait for phi2 high
		while (!((pins = gpio_get_all()) & PHI2_GPIO_MASK))
			;

		if (!(pins & S5_GPIO_MASK) && rd5_high)
		{ // s5 low
			SET_DATA_MODE_OUT;
			addr = pins & ADDR_GPIO_MASK;
			gpio_put_masked(DATA_GPIO_MASK, ((uint32_t)(*(bankPtr + addr))) << 13);
		}
		else if (!(pins & CCTL_GPIO_MASK))
		{ // CCTL low
			addr = pins & ADDR_GPIO_MASK;
			bank = addr & bank_mask;
			if (addr & 0x10)
			{
				RD5_LOW;
				rd5_high = false;
			}
			else
			{
				RD5_HIGH;
				rd5_high = true;
			}
		}
		// wait for phi2 low
		while (gpio_get_all() & PHI2_GPIO_MASK)
			;
		SET_DATA_MODE_IN;
	}
}

void __not_in_flash_func(emulate_atrax)()
{
	// 128k
	RD4_LOW;
	RD5_HIGH;

	uint32_t bank = 0;
	unsigned char *bankPtr;
	uint32_t pins, last;
	uint16_t addr;
	uint8_t data;
	bool rd5_high = true; // 400/800 MMU

	while (1)
	{
		// select the right SRAM base, based on the cartridge bank
		bankPtr = &cart_ram[0] + (8192 * bank);
		// wait for phi2 high
		while (!((pins = gpio_get_all()) & PHI2_GPIO_MASK))
			;

		if (!(pins & S5_GPIO_MASK) && rd5_high)
		{ // s5 low
			SET_DATA_MODE_OUT;
			addr = pins & ADDR_GPIO_MASK;
			gpio_put_masked(DATA_GPIO_MASK, ((uint32_t)(*(bankPtr + addr))) << 13);
			// wait for phi2 low
			while (gpio_get_all() & PHI2_GPIO_MASK)
				;
			SET_DATA_MODE_IN;
		}
		else if (!(pins & CCTL_RW_GPIO_MASK))
		{ // CCTL low + write
			last = pins;
			// read data bus on falling edge of phi2
			while ((pins = gpio_get_all()) & PHI2_GPIO_MASK)
				last = pins;
			data = (last & DATA_GPIO_MASK) >> 13;
			// new bank is the low 4 bits written to $D5xx
			bank = data & 0xF;
			if (data & 0x80)
			{
				RD5_LOW;
				rd5_high = false;
			}
			else
			{
				RD5_HIGH;
				rd5_high = true;
			}
		}
	}
}

void __not_in_flash_func(emulate_microcalc)()
{
	// 32k
	RD4_LOW;
	RD5_HIGH;

	uint32_t bank = 0;
	unsigned char *bankPtr;
	uint32_t pins;
	uint16_t addr;
	bool rd5_high = true; // 400/800 MMU

	while (1)
	{
		// select the right SRAM base, based on the cartridge bank
		bankPtr = &cart_ram[0] + (8192 * bank);
		// wait for phi2 high
		while (!((pins = gpio_get_all()) & PHI2_GPIO_MASK))
			;

		if (!(pins & S5_GPIO_MASK) && rd5_high)
		{ // s5 low
			SET_DATA_MODE_OUT;
			addr = pins & ADDR_GPIO_MASK;
			gpio_put_masked(DATA_GPIO_MASK, ((uint32_t)(*(bankPtr + addr))) << 13);
		}
		else if (!(pins & CCTL_GPIO_MASK))
		{ // CCTL low
			bank = (bank + 1) % 5;
			if (bank == 4) // disable
			{
				RD5_LOW;
				rd5_high = false;
			}
			else
			{
				RD5_HIGH;
				rd5_high = true;
			}
		}
		// wait for phi2 low
		while (gpio_get_all() & PHI2_GPIO_MASK)
			;
		SET_DATA_MODE_IN;
	}
}

void __not_in_flash_func(emulate_phoenix_8k)()
{
	// 8k
	RD4_LOW;
	RD5_HIGH;

	uint32_t pins;
	uint16_t addr;
	bool rd5_high = true; // 400/800 MMU

	while (1)
	{
		// wait for phi2 high
		while (!((pins = gpio_get_all()) & PHI2_GPIO_MASK))
			;

		if (!(pins & S5_GPIO_MASK) && rd5_high)
		{ // s5 low
			SET_DATA_MODE_OUT;
			addr = pins & ADDR_GPIO_MASK;
			gpio_put_masked(DATA_GPIO_MASK, ((uint32_t)cart_ram[addr]) << 13);
		}
		else if (!(pins & CCTL_GPIO_MASK))
		{ // CCTL low
			RD5_LOW;
			rd5_high = false;
		}
		// wait for phi2 low
		while (gpio_get_all() & PHI2_GPIO_MASK)
			;
		SET_DATA_MODE_IN;
	}
}

void emulate_cartridge(int cartType)
{
	if (cartType == CART_TYPE_8K)
		emulate_standard_8k();
	else if (cartType == CART_TYPE_16K)
		emulate_standard_16k();
	else if (cartType == CART_TYPE_XEGS_32K)
		emulate_XEGS_32k(0);
	else if (cartType == CART_TYPE_XEGS_64K)
		emulate_XEGS_64k(0);
	else if (cartType == CART_TYPE_XEGS_128K)
		emulate_XEGS_128k(0);
	else if (cartType == CART_TYPE_SW_XEGS_32K)
		emulate_XEGS_32k(1);
	else if (cartType == CART_TYPE_SW_XEGS_64K)
		emulate_XEGS_64k(1);
	else if (cartType == CART_TYPE_SW_XEGS_128K)
		emulate_XEGS_128k(1);
	else if (cartType == CART_TYPE_BOUNTY_BOB)
		emulate_bounty_bob();
	else if (cartType == CART_TYPE_ATARIMAX_1MBIT)
		emulate_atarimax_128k();
	else if (cartType == CART_TYPE_WILLIAMS_64K)
		emulate_williams();
	else if (cartType == CART_TYPE_OSS_16K_TYPE_B)
		emulate_OSS_B();
	else if (cartType == CART_TYPE_OSS_8K)
		emulate_OSS_B();
	else if (cartType == CART_TYPE_OSS_16K_034M)
		emulate_OSS_A(1);
	else if (cartType == CART_TYPE_OSS_16K_043M)
		emulate_OSS_A(0);
	else if (cartType == CART_TYPE_MEGACART_16K)
		emulate_megacart(16);
	else if (cartType == CART_TYPE_MEGACART_32K)
		emulate_megacart(32);
	else if (cartType == CART_TYPE_MEGACART_64K)
		emulate_megacart(64);
	else if (cartType == CART_TYPE_MEGACART_128K)
		emulate_megacart(128);
	else if (cartType == CART_TYPE_SIC_128K)
		emulate_SIC();
	else if (cartType == CART_TYPE_SDX_64K)
		emulate_SDX(64);
	else if (cartType == CART_TYPE_SDX_128K)
		emulate_SDX(128);
	else if (cartType == CART_TYPE_DIAMOND_64K)
		emulate_diamond_express(0xD0);
	else if (cartType == CART_TYPE_EXPRESS_64K)
		emulate_diamond_express(0x70);
	else if (cartType == CART_TYPE_BLIZZARD_16K)
		emulate_blizzard();
	else if (cartType == CART_TYPE_4K)
		emulate_standard_8k(); // patch in load_file()
	else if (cartType == CART_TYPE_TURBOSOFT_64K)
		emulate_turbosoft(64);
	else if (cartType == CART_TYPE_TURBOSOFT_128K)
		emulate_turbosoft(128);
	else if (cartType == CART_TYPE_ATRAX_128K)
		emulate_atrax();
	else if (cartType == CART_TYPE_MICROCALC)
		emulate_microcalc();
	else if (cartType == CART_TYPE_2K)
		emulate_standard_8k();
	else if (cartType == CART_TYPE_PHOENIX_8K)
		emulate_phoenix_8k();
	else if (cartType == CART_TYPE_BLIZZARD_4K)
		emulate_phoenix_8k();
	// else if (cartType == CART_TYPE_XEX)
	//	feed_XEX_loader();
	else if (cartType == CART_ROLAND)
		emulate_roland();
	else
	{ // no cartridge (cartType = 0)
		RD4_LOW;
		RD5_LOW;
		while (1)
			;
	}
}

#define ATARI_PHI2_PIN 22 // used on boot to check if we are plugged into an atari or usb

void __not_in_flash_func(atari_cart_main)()
{
	gpio_init(ATARI_PHI2_PIN);
	gpio_set_dir(ATARI_PHI2_PIN, GPIO_IN);

	gpio_init_mask(ALL_GPIO_MASK);

	gpio_set_dir_in_masked(ADDR_GPIO_MASK | DATA_GPIO_MASK | CCTL_GPIO_MASK | PHI2_GPIO_MASK | RW_GPIO_MASK | S4_GPIO_MASK | S5_GPIO_MASK);
	gpio_set_dir(RD4_PIN, GPIO_OUT);
	gpio_set_dir(RD5_PIN, GPIO_OUT);

	emulate_cartridge(CART_ROLAND);
}
