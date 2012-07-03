/*
 * Code for AM335X EVM.
 *
 * Copyright (C) 2011 Texas Instruments, Inc. - http://www.ti.com/
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License as
 * published by the Free Software Foundation version 2.
 *
 * This program is distributed "as is" WITHOUT ANY WARRANTY of any
 * kind, whether express or implied; without even the implied warranty
 * of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 */
#include <linux/kernel.h>
#include <linux/init.h>
#include <linux/i2c.h>
#include <linux/module.h>
#include <linux/i2c/at24.h>
#include <linux/phy.h>
#include <linux/gpio.h>
#include <linux/spi/spi.h>
#include <linux/spi/flash.h>
#include <linux/gpio_keys.h>
#include <linux/input.h>
#include <linux/input/matrix_keypad.h>
#include <linux/mtd/mtd.h>
#include <linux/mtd/nand.h>
#include <linux/mtd/partitions.h>
#include <linux/platform_device.h>
#include <linux/clk.h>
#include <linux/err.h>
#include <linux/wl12xx.h>
#include <linux/ethtool.h>
#include <linux/mfd/tps65910.h>
#include <linux/mfd/tps65217.h>
#include <linux/pwm_backlight.h>

/* LCD controller is similar to DA850 */
#include <video/da8xx-fb.h>

#include <mach/hardware.h>
#include <mach/board-am335xevm.h>

#include <asm/mach-types.h>
#include <asm/mach/arch.h>
#include <asm/mach/map.h>
#include <asm/hardware/asp.h>

#include <plat/irqs.h>
#include <plat/board.h>
#include <plat/common.h>
#include <plat/lcdc.h>
#include <plat/usb.h>
#include <plat/mmc.h>
#include <plat/emif.h>

#include "board-flash.h"
#include "cpuidle33xx.h"
#include "mux.h"
#include "devices.h"
#include "hsmmc.h"


#define CALIXTO_PHY_ID		0x0007c0f0
#define CALIXTO_PHY_MASK	0xfffffff6

/* Convert GPIO signal to GPIO pin number */
#define GPIO_TO_PIN(bank, gpio) (32 * (bank) + (gpio))



static const struct display_panel disp_panel = {
	WVGA,
	16,
	16,
	COLOR_ACTIVE,
};

static struct lcd_ctrl_config lcd_cfg = {
	&disp_panel,
	.ac_bias		= 255,
	.ac_bias_intrpt		= 0,
	.dma_burst_sz		= 16,
	.bpp			= 16,
	.fdd			= 255,
	.tft_alt_mode		= 0,
	.stn_565_mode		= 0,
	.mono_8bit_mode		= 0,
	.invert_line_clock	= 1,
	.invert_frm_clock	= 1,
	.sync_edge		= 0,
	.sync_ctrl		= 1,
	.raster_order		= 0,
};

struct da8xx_lcdc_platform_data calixtovga_pdata = {
	.manu_name		= "VGA",
	.controller_data	= &lcd_cfg,
	.type			= "CalixtoVGA",
};



/* LCD backlight platform Data */
#define AM335X_BACKLIGHT_MAX_BRIGHTNESS        100
#define AM335X_BACKLIGHT_DEFAULT_BRIGHTNESS    100
#define AM335X_PWM_PERIOD_NANO_SECONDS        (1000000 * 10)

#define PWM_DEVICE_ID   "ecap.0"

static struct platform_pwm_backlight_data am335x_backlight_data = {
	.pwm_id         = PWM_DEVICE_ID,
	.ch             = -1,
	.max_brightness = AM335X_BACKLIGHT_MAX_BRIGHTNESS,
	.dft_brightness = AM335X_BACKLIGHT_DEFAULT_BRIGHTNESS,
	.pwm_period_ns  = AM335X_PWM_PERIOD_NANO_SECONDS,
};


#include "common.h"

/* TSc controller */
#include <linux/input/ti_tscadc.h>
#include <linux/lis3lv02d.h>

static struct resource tsc_resources[]  = {
	[0] = {
		.start  = AM33XX_TSC_BASE,
		.end    = AM33XX_TSC_BASE + SZ_8K - 1,
		.flags  = IORESOURCE_MEM,
	},
	[1] = {
		.start  = AM33XX_IRQ_ADC_GEN,
		.end    = AM33XX_IRQ_ADC_GEN,
		.flags  = IORESOURCE_IRQ,
	},
};

static struct tsc_data am335x_touchscreen_data  = {
	.wires  = 4,
	.x_plate_resistance = 200,
};

static struct platform_device tsc_device = {
	.name   = "tsc",
	.id     = -1,
	.dev    = {
			.platform_data  = &am335x_touchscreen_data,
	},
	.num_resources  = ARRAY_SIZE(tsc_resources),
	.resource       = tsc_resources,
};

static u8 am335x_iis_serializer_direction1[] = {
	INACTIVE_MODE,	INACTIVE_MODE,	TX_MODE,	RX_MODE,
	INACTIVE_MODE,	INACTIVE_MODE,	INACTIVE_MODE,	INACTIVE_MODE,
	INACTIVE_MODE,	INACTIVE_MODE,	INACTIVE_MODE,	INACTIVE_MODE,
	INACTIVE_MODE,	INACTIVE_MODE,	INACTIVE_MODE,	INACTIVE_MODE,
};



static struct snd_platform_data am335x_evm_snd_data1 = {
	.tx_dma_offset	= 0x46000000,	/* McASP0 */
	.rx_dma_offset	= 0x46000000,
	.op_mode	= DAVINCI_MCASP_IIS_MODE,
	.num_serializer	= ARRAY_SIZE(am335x_iis_serializer_direction1),
	.tdm_slots	= 2,
	.serial_dir	= am335x_iis_serializer_direction1,
	.asp_chan_q	= EVENTQ_2,
	.version	= MCASP_VERSION_3,
	.txnumevt	= 1,
	.rxnumevt	= 1,
};

static struct omap2_hsmmc_info am335x_mmc[] __initdata = {
	{
		.mmc            = 1,
		.caps           = MMC_CAP_4_BIT_DATA,
		.gpio_cd        = GPIO_TO_PIN(0, 6),
		.gpio_wp        = GPIO_TO_PIN(3, 18),
		.ocr_mask       = MMC_VDD_32_33 | MMC_VDD_33_34, /* 3V3 */
	},
	{
		.mmc            = 0,	/* will be set at runtime */
	},
	{
		.mmc            = 0,	/* will be set at runtime */
	},
	{}      /* Terminator */
};


#ifdef CONFIG_OMAP_MUX
static struct omap_board_mux board_mux[] __initdata = {
	AM33XX_MUX(I2C0_SDA, OMAP_MUX_MODE0 | AM33XX_SLEWCTRL_SLOW |
			AM33XX_INPUT_EN | AM33XX_PIN_OUTPUT),
	AM33XX_MUX(I2C0_SCL, OMAP_MUX_MODE0 | AM33XX_SLEWCTRL_SLOW |
			AM33XX_INPUT_EN | AM33XX_PIN_OUTPUT),
	{ .reg_offset = OMAP_MUX_TERMINATOR },
};
#else
#define	board_mux	NULL
#endif

/* module pin mux structure */
struct pinmux_config {
	const char *string_name; /* signal name format */
	int val; /* Options for the mux register value */
};

struct evm_dev_cfg {
	void (*device_init)(int evm_id, int profile);

/*
* If the device is required on both baseboard & daughter board (ex i2c),
* specify DEV_ON_BASEBOARD
*/
#define DEV_ON_BASEBOARD	0
#define DEV_ON_DGHTR_BRD	1
	u32 device_on;

	u32 profile;	/* Profiles (0-7) in which the module is present */
};

/* AM335X - CPLD Register Offsets */
#define	CPLD_DEVICE_HDR	0x00 /* CPLD Header */
#define	CPLD_DEVICE_ID	0x04 /* CPLD identification */
#define	CPLD_DEVICE_REV	0x0C /* Revision of the CPLD code */
#define	CPLD_CFG_REG	0x10 /* Configuration Register */

static struct i2c_client *cpld_client;
static u32 am335x_evm_id;
static struct omap_board_config_kernel am335x_evm_config[] __initdata = {
};

/*
* EVM Config held in On-Board eeprom device.
*
* Header Format
*
*  Name			Size	Contents
*			(Bytes)
*-------------------------------------------------------------
*  Header		4	0xAA, 0x55, 0x33, 0xEE
*
*  Board Name		8	Name for board in ASCII.
*				example "A33515BB" = "AM335X
				Low Cost EVM board"
*
*  Version		4	Hardware version code for board in
*				in ASCII. "1.0A" = rev.01.0A
*
*  Serial Number	12	Serial number of the board. This is a 12
*				character string which is WWYY4P16nnnn, where
*				WW = 2 digit week of the year of production
*				YY = 2 digit year of production
*				nnnn = incrementing board number
*
*  Configuration option	32	Codes(TBD) to show the configuration
*				setup on this board.
*
*  Available		32720	Available space for other non-volatile
*				data.
*/

static bool daughter_brd_detected;

#define GP_EVM_REV_IS_1_0		0x1
#define GP_EVM_REV_IS_1_1A		0x2
#define GP_EVM_REV_IS_UNKNOWN		0xFF
static unsigned int gp_evm_revision = GP_EVM_REV_IS_UNKNOWN;
unsigned int gigabit_enable = 1;

#define EEPROM_MAC_ADDRESS_OFFSET	60 /* 4+8+4+12+32 */
#define EEPROM_NO_OF_MAC_ADDR		3
static char am335x_mac_addr[EEPROM_NO_OF_MAC_ADDR][ETH_ALEN];

#define AM335X_EEPROM_HEADER		0xEE3355AA



static u32 am335x_get_profile_selection(void)
{
	return 0;
}


static struct pinmux_config lcdc_pin_mux[] = {
	{"lcd_data0.lcd_data0",		OMAP_MUX_MODE0 | AM33XX_PIN_OUTPUT
						       | AM33XX_PULL_DISA},
	{"lcd_data1.lcd_data1",		OMAP_MUX_MODE0 | AM33XX_PIN_OUTPUT
						       | AM33XX_PULL_DISA},
	{"lcd_data2.lcd_data2",		OMAP_MUX_MODE0 | AM33XX_PIN_OUTPUT
						       | AM33XX_PULL_DISA},
	{"lcd_data3.lcd_data3",		OMAP_MUX_MODE0 | AM33XX_PIN_OUTPUT
						       | AM33XX_PULL_DISA},
	{"lcd_data4.lcd_data4",		OMAP_MUX_MODE0 | AM33XX_PIN_OUTPUT
						       | AM33XX_PULL_DISA},
	{"lcd_data5.lcd_data5",		OMAP_MUX_MODE0 | AM33XX_PIN_OUTPUT
						       | AM33XX_PULL_DISA},
	{"lcd_data6.lcd_data6",		OMAP_MUX_MODE0 | AM33XX_PIN_OUTPUT
						       | AM33XX_PULL_DISA},
	{"lcd_data7.lcd_data7",		OMAP_MUX_MODE0 | AM33XX_PIN_OUTPUT
						       | AM33XX_PULL_DISA},
	{"lcd_data8.lcd_data8",		OMAP_MUX_MODE0 | AM33XX_PIN_OUTPUT
						       | AM33XX_PULL_DISA},
	{"lcd_data9.lcd_data9",		OMAP_MUX_MODE0 | AM33XX_PIN_OUTPUT
						       | AM33XX_PULL_DISA},
	{"lcd_data10.lcd_data10",	OMAP_MUX_MODE0 | AM33XX_PIN_OUTPUT
						       | AM33XX_PULL_DISA},
	{"lcd_data11.lcd_data11",	OMAP_MUX_MODE0 | AM33XX_PIN_OUTPUT
						       | AM33XX_PULL_DISA},
	{"lcd_data12.lcd_data12",	OMAP_MUX_MODE0 | AM33XX_PIN_OUTPUT
						       | AM33XX_PULL_DISA},
	{"lcd_data13.lcd_data13",	OMAP_MUX_MODE0 | AM33XX_PIN_OUTPUT
						       | AM33XX_PULL_DISA},
	{"lcd_data14.lcd_data14",	OMAP_MUX_MODE0 | AM33XX_PIN_OUTPUT
						       | AM33XX_PULL_DISA},
	{"lcd_data15.lcd_data15",	OMAP_MUX_MODE0 | AM33XX_PIN_OUTPUT},
	{"lcd_vsync.lcd_vsync",		OMAP_MUX_MODE0 | AM33XX_PIN_OUTPUT},
	{"lcd_hsync.lcd_hsync",		OMAP_MUX_MODE0 | AM33XX_PIN_OUTPUT},
	{"lcd_pclk.lcd_pclk",		OMAP_MUX_MODE0 | AM33XX_PIN_OUTPUT},
	{"lcd_ac_bias_en.lcd_ac_bias_en", OMAP_MUX_MODE0 | AM33XX_PIN_OUTPUT},
	{NULL, 0},
};


/* Pin mux for nand flash module */
static struct pinmux_config nand_pin_mux[] = {
	{"gpmc_ad0.gpmc_ad0",	  OMAP_MUX_MODE0 | AM33XX_PIN_INPUT_PULLUP},
	{"gpmc_ad1.gpmc_ad1",	  OMAP_MUX_MODE0 | AM33XX_PIN_INPUT_PULLUP},
	{"gpmc_ad2.gpmc_ad2",	  OMAP_MUX_MODE0 | AM33XX_PIN_INPUT_PULLUP},
	{"gpmc_ad3.gpmc_ad3",	  OMAP_MUX_MODE0 | AM33XX_PIN_INPUT_PULLUP},
	{"gpmc_ad4.gpmc_ad4",	  OMAP_MUX_MODE0 | AM33XX_PIN_INPUT_PULLUP},
	{"gpmc_ad5.gpmc_ad5",	  OMAP_MUX_MODE0 | AM33XX_PIN_INPUT_PULLUP},
	{"gpmc_ad6.gpmc_ad6",	  OMAP_MUX_MODE0 | AM33XX_PIN_INPUT_PULLUP},
	{"gpmc_ad7.gpmc_ad7",	  OMAP_MUX_MODE0 | AM33XX_PIN_INPUT_PULLUP},
	{"gpmc_wait0.gpmc_wait0", OMAP_MUX_MODE0 | AM33XX_PIN_INPUT_PULLUP},
	{"gpmc_wpn.gpmc_wpn",	  OMAP_MUX_MODE7 | AM33XX_PIN_INPUT_PULLUP},
	{"gpmc_csn0.gpmc_csn0",	  OMAP_MUX_MODE0 | AM33XX_PULL_DISA},
	{"gpmc_advn_ale.gpmc_advn_ale",  OMAP_MUX_MODE0 | AM33XX_PULL_DISA},
	{"gpmc_oen_ren.gpmc_oen_ren",	 OMAP_MUX_MODE0 | AM33XX_PULL_DISA},
	{"gpmc_wen.gpmc_wen",     OMAP_MUX_MODE0 | AM33XX_PULL_DISA},
	{"gpmc_ben0_cle.gpmc_ben0_cle",	 OMAP_MUX_MODE0 | AM33XX_PULL_DISA},
	{NULL, 0},
};

static struct pinmux_config tsc_pin_mux[] = {
	{"ain0.ain0",           OMAP_MUX_MODE0 | AM33XX_INPUT_EN},
	{"ain1.ain1",           OMAP_MUX_MODE0 | AM33XX_INPUT_EN},
	{"ain2.ain2",           OMAP_MUX_MODE0 | AM33XX_INPUT_EN},
	{"ain3.ain3",           OMAP_MUX_MODE0 | AM33XX_INPUT_EN},
	{"vrefp.vrefp",         OMAP_MUX_MODE0 | AM33XX_INPUT_EN},
	{"vrefn.vrefn",         OMAP_MUX_MODE0 | AM33XX_INPUT_EN},
	{NULL, 0},
};

/* Pin mux for nand flash module */

/* Module pin mux for SPI fash */
static struct pinmux_config spi0_pin_mux[] = {
	{"spi0_sclk.spi0_sclk", OMAP_MUX_MODE0 | AM33XX_PULL_ENBL
							| AM33XX_INPUT_EN},
	{"spi0_d0.spi0_d0", OMAP_MUX_MODE0 | AM33XX_PULL_ENBL | AM33XX_PULL_UP
							| AM33XX_INPUT_EN},
	{"spi0_d1.spi0_d1", OMAP_MUX_MODE0 | AM33XX_PULL_ENBL
							| AM33XX_INPUT_EN},
	{"spi0_cs0.spi0_cs0", OMAP_MUX_MODE0 | AM33XX_PULL_ENBL | AM33XX_PULL_UP
							| AM33XX_INPUT_EN},
	{NULL, 0},
};


static struct pinmux_config audio_pin_mux[] = {
	{"mii1_txclk.mcasp0_aclkx", OMAP_MUX_MODE6 | AM33XX_PULL_ENBL
		| AM33XX_INPUT_EN},
	{"mcasp0_fsx.mcasp0_fsx", OMAP_MUX_MODE0 | AM33XX_PULL_ENBL
		| AM33XX_PULL_UP | AM33XX_INPUT_EN},
	{"mii1_rxd3.mcasp0_axr0", OMAP_MUX_MODE6 | AM33XX_PULL_ENBL
		| AM33XX_INPUT_EN},
	{"mcasp0_axr1.mcasp0_axr1", OMAP_MUX_MODE0 | AM33XX_PULL_ENBL
		| AM33XX_INPUT_EN},
	{"mcasp0_ahclkx.mcasp0_ahclkx", OMAP_MUX_MODE0 | AM33XX_PULL_ENBL
		| AM33XX_PULL_UP | AM33XX_INPUT_EN},
	{NULL, 0},
};







static struct pinmux_config mii2_pin_mux[] = {
        {"gpmc_wpn.mii2_rxerr", OMAP_MUX_MODE1 | AM33XX_PIN_INPUT_PULLDOWN},
        {"gpmc_a0.mii2_txen", OMAP_MUX_MODE1 | AM33XX_PIN_OUTPUT},
        {"gpmc_a1.mii2_rxdv", OMAP_MUX_MODE1 | AM33XX_PIN_INPUT_PULLDOWN},
        {"gpmc_a2.mii2_txd3", OMAP_MUX_MODE1 | AM33XX_PIN_OUTPUT},
        {"gpmc_a3.mii2_txd2", OMAP_MUX_MODE1 | AM33XX_PIN_OUTPUT},
        {"gpmc_a4.mii2_txd1", OMAP_MUX_MODE1 | AM33XX_PIN_OUTPUT},
        {"gpmc_a5.mii2_txd0", OMAP_MUX_MODE1 | AM33XX_PIN_OUTPUT},
        {"gpmc_a6.mii2_txclk", OMAP_MUX_MODE1 | AM33XX_PIN_INPUT_PULLDOWN},
        {"gpmc_a7.mii2_rxclk", OMAP_MUX_MODE1 | AM33XX_PIN_INPUT_PULLDOWN},
        {"gpmc_a8.mii2_rxd3", OMAP_MUX_MODE1 | AM33XX_PIN_INPUT_PULLDOWN},
        {"gpmc_a9.mii2_rxd2", OMAP_MUX_MODE1 | AM33XX_PIN_INPUT_PULLDOWN},
        {"gpmc_a10.mii2_rxd1", OMAP_MUX_MODE1 | AM33XX_PIN_INPUT_PULLDOWN},
        {"gpmc_a11.mii2_rxd0", OMAP_MUX_MODE1 | AM33XX_PIN_INPUT_PULLDOWN},
        {"mdio_data.mdio_data", OMAP_MUX_MODE0 | AM33XX_PIN_INPUT_PULLUP},
        {"mdio_clk.mdio_clk", OMAP_MUX_MODE0 | AM33XX_PIN_OUTPUT_PULLUP},
        {NULL, 0},
};

static struct pinmux_config mii1_pin_mux[] = {
	{"mii1_rxerr.mii1_rxerr", OMAP_MUX_MODE0 | AM33XX_PIN_INPUT_PULLDOWN},
	{"mii1_txen.mii1_txen", OMAP_MUX_MODE0 | AM33XX_PIN_OUTPUT},
	{"mii1_rxdv.mii1_rxdv", OMAP_MUX_MODE0 | AM33XX_PIN_INPUT_PULLDOWN},
	{"mii1_txd3.mii1_txd3", OMAP_MUX_MODE0 | AM33XX_PIN_OUTPUT},
	{"mii1_txd2.mii1_txd2", OMAP_MUX_MODE0 | AM33XX_PIN_OUTPUT},
	{"mii1_txd1.mii1_txd1", OMAP_MUX_MODE0 | AM33XX_PIN_OUTPUT},
	{"mii1_txd0.mii1_txd0", OMAP_MUX_MODE0 | AM33XX_PIN_OUTPUT},
	{"mii1_txclk.mii1_txclk", OMAP_MUX_MODE0 | AM33XX_PIN_INPUT_PULLDOWN},
	{"mii1_rxclk.mii1_rxclk", OMAP_MUX_MODE0 | AM33XX_PIN_INPUT_PULLDOWN},
	{"mii1_rxd3.mii1_rxd3", OMAP_MUX_MODE0 | AM33XX_PIN_INPUT_PULLDOWN},
	{"mii1_rxd2.mii1_rxd2", OMAP_MUX_MODE0 | AM33XX_PIN_INPUT_PULLDOWN},
	{"mii1_rxd1.mii1_rxd1", OMAP_MUX_MODE0 | AM33XX_PIN_INPUT_PULLDOWN},
	{"mii1_rxd0.mii1_rxd0", OMAP_MUX_MODE0 | AM33XX_PIN_INPUT_PULLDOWN},
	{"mdio_data.mdio_data", OMAP_MUX_MODE0 | AM33XX_PIN_INPUT_PULLUP},
	{"mdio_clk.mdio_clk", OMAP_MUX_MODE0 | AM33XX_PIN_OUTPUT_PULLUP},
	{NULL, 0},
};






/* Module pin mux for rmii1 */
static struct pinmux_config rmii1_pin_mux[] = {
	{"mii1_crs.rmii1_crs_dv", OMAP_MUX_MODE1 | AM33XX_PIN_INPUT_PULLDOWN},
	{"mii1_rxerr.mii1_rxerr", OMAP_MUX_MODE1 | AM33XX_PIN_INPUT_PULLDOWN},
	{"mii1_txen.mii1_txen", OMAP_MUX_MODE1 | AM33XX_PIN_OUTPUT},
	{"mii1_txd1.mii1_txd1", OMAP_MUX_MODE1 | AM33XX_PIN_OUTPUT},
	{"mii1_txd0.mii1_txd0", OMAP_MUX_MODE1 | AM33XX_PIN_OUTPUT},
	{"mii1_rxd1.mii1_rxd1", OMAP_MUX_MODE1 | AM33XX_PIN_INPUT_PULLDOWN},
	{"mii1_rxd0.mii1_rxd0", OMAP_MUX_MODE1 | AM33XX_PIN_INPUT_PULLDOWN},
	{"rmii1_refclk.rmii1_refclk", OMAP_MUX_MODE0 | AM33XX_PIN_INPUT_PULLDOWN},
	{"mdio_data.mdio_data", OMAP_MUX_MODE0 | AM33XX_PIN_INPUT_PULLUP},
	{"mdio_clk.mdio_clk", OMAP_MUX_MODE0 | AM33XX_PIN_OUTPUT_PULLUP},
	{NULL, 0},
};

static struct pinmux_config i2c0_pin_mux[] = {
	{"i2c0_sda.i2c0_sda",    OMAP_MUX_MODE0 | AM33XX_SLEWCTRL_SLOW |
					AM33XX_PULL_ENBL | AM33XX_INPUT_EN},
	{"i2c0_scl.i2c0_scl",   OMAP_MUX_MODE0 | AM33XX_SLEWCTRL_SLOW |
					AM33XX_PULL_ENBL | AM33XX_INPUT_EN},
	{NULL, 0},
};



/* Module pin mux for mmc0 */
static struct pinmux_config mmc0_pin_mux[] = {
	{"mmc0_dat3.mmc0_dat3",	OMAP_MUX_MODE0 | AM33XX_PIN_INPUT_PULLUP},
	{"mmc0_dat2.mmc0_dat2",	OMAP_MUX_MODE0 | AM33XX_PIN_INPUT_PULLUP},
	{"mmc0_dat1.mmc0_dat1",	OMAP_MUX_MODE0 | AM33XX_PIN_INPUT_PULLUP},
	{"mmc0_dat0.mmc0_dat0",	OMAP_MUX_MODE0 | AM33XX_PIN_INPUT_PULLUP},
	{"mmc0_clk.mmc0_clk",	OMAP_MUX_MODE0 | AM33XX_PIN_INPUT_PULLUP},
	{"mmc0_cmd.mmc0_cmd",	OMAP_MUX_MODE0 | AM33XX_PIN_INPUT_PULLUP},
	{"mcasp0_aclkx.mmc0_sdcd",  OMAP_MUX_MODE4 | AM33XX_PIN_INPUT_PULLUP},
	{NULL, 0},
};



static struct pinmux_config mmc0_no_cd_pin_mux[] = {
	{"mmc0_dat3.mmc0_dat3",	OMAP_MUX_MODE0 | AM33XX_PIN_INPUT_PULLUP},
	{"mmc0_dat2.mmc0_dat2",	OMAP_MUX_MODE0 | AM33XX_PIN_INPUT_PULLUP},
	{"mmc0_dat1.mmc0_dat1",	OMAP_MUX_MODE0 | AM33XX_PIN_INPUT_PULLUP},
	{"mmc0_dat0.mmc0_dat0",	OMAP_MUX_MODE0 | AM33XX_PIN_INPUT_PULLUP},
	{"mmc0_clk.mmc0_clk",	OMAP_MUX_MODE0 | AM33XX_PIN_INPUT_PULLUP},
	{"mmc0_cmd.mmc0_cmd",	OMAP_MUX_MODE0 | AM33XX_PIN_INPUT_PULLUP},
	{"mcasp0_aclkr.mmc0_sdwp", OMAP_MUX_MODE4 | AM33XX_PIN_INPUT_PULLDOWN},
	{NULL, 0},
};




/* Module pin mux for uart3 */
static struct pinmux_config uart3_pin_mux[] = {
	{"spi0_cs1.uart3_rxd", AM33XX_PIN_INPUT_PULLUP},
	{"ecap0_in_pwm0_out.uart3_txd", AM33XX_PULL_ENBL},
	{NULL, 0},
};

static struct pinmux_config d_can_gp_pin_mux[] = {
	{"uart0_ctsn.d_can1_tx", OMAP_MUX_MODE2 | AM33XX_PULL_ENBL},
	{"uart0_rtsn.d_can1_rx", OMAP_MUX_MODE2 | AM33XX_PIN_INPUT_PULLUP},
	{NULL, 0},
};

static struct pinmux_config d_can_ia_pin_mux[] = {
	{"uart0_rxd.d_can0_tx", OMAP_MUX_MODE2 | AM33XX_PULL_ENBL},
	{"uart0_txd.d_can0_rx", OMAP_MUX_MODE2 | AM33XX_PIN_INPUT_PULLUP},
	{NULL, 0},
};

/* Module pin mux for uart2 */
static struct pinmux_config uart2_pin_mux[] = {
	{"spi0_sclk.uart2_rxd", OMAP_MUX_MODE1 | AM33XX_SLEWCTRL_SLOW |
						AM33XX_PIN_INPUT_PULLUP},
	{"spi0_d0.uart2_txd", OMAP_MUX_MODE1 | AM33XX_PULL_UP |
						AM33XX_PULL_DISA |
						AM33XX_SLEWCTRL_SLOW},
	{NULL, 0},
};


/*
* @pin_mux - single module pin-mux structure which defines pin-mux
*			details for all its pins.
*/
static void setup_pin_mux(struct pinmux_config *pin_mux)
{
	int i;

	for (i = 0; pin_mux->string_name != NULL; pin_mux++)
		omap_mux_init_signal(pin_mux->string_name, pin_mux->val);

}

/* Matrix GPIO Keypad Support for profile-0 only: TODO */


/*
* @evm_id - evm id which needs to be configured
* @dev_cfg - single evm structure which includes
*				all module inits, pin-mux defines
* @profile - if present, else PROFILE_NONE
* @dghtr_brd_flg - Whether Daughter board is present or not
*/
static void _configure_device(int evm_id, struct evm_dev_cfg *dev_cfg,
	int profile)
{
	int i;

	/*
	* Only General Purpose & Industrial Auto Motro Control
	* EVM has profiles. So check if this evm has profile.
	* If not, ignore the profile comparison
	*/

	/*
	* If the device is on baseboard, directly configure it. Else (device on
	* Daughter board), check if the daughter card is detected.
	*/
	if (profile == PROFILE_NONE) {
		for (i = 0; dev_cfg->device_init != NULL; dev_cfg++) {
			if (dev_cfg->device_on == DEV_ON_BASEBOARD)
				dev_cfg->device_init(evm_id, profile);
			else if (daughter_brd_detected == true)
				dev_cfg->device_init(evm_id, profile);
		}
	} else {
		for (i = 0; dev_cfg->device_init != NULL; dev_cfg++) {
			if (dev_cfg->profile & profile) {
				if (dev_cfg->device_on == DEV_ON_BASEBOARD)
					dev_cfg->device_init(evm_id, profile);
				else if (daughter_brd_detected == true)
					dev_cfg->device_init(evm_id, profile);
			}
		}
	}
}


/* pinmux for usb0 drvvbus */
static struct pinmux_config usb0_pin_mux[] = {
	{"usb0_drvvbus.usb0_drvvbus",    OMAP_MUX_MODE0 | AM33XX_PIN_OUTPUT},
	{NULL, 0},
};

/* pinmux for usb1 drvvbus */
static struct pinmux_config usb1_pin_mux[] = {
	{"usb1_drvvbus.usb1_drvvbus",    OMAP_MUX_MODE0 | AM33XX_PIN_OUTPUT},
	{NULL, 0},
};

/* pinmux for profibus */
static struct pinmux_config profibus_pin_mux[] = {
	{"uart1_rxd.pr1_uart0_rxd_mux1", OMAP_MUX_MODE5 | AM33XX_PIN_INPUT},
	{"uart1_txd.pr1_uart0_txd_mux1", OMAP_MUX_MODE5 | AM33XX_PIN_OUTPUT},
	{"mcasp0_fsr.pr1_pru0_pru_r30_5", OMAP_MUX_MODE5 | AM33XX_PIN_OUTPUT},
	{NULL, 0},
};

/* Module pin mux for eCAP0 */
static struct pinmux_config ecap0_pin_mux[] = {
	{"ecap0_in_pwm0_out.ecap0_in_pwm0_out",
		OMAP_MUX_MODE0 | AM33XX_PIN_OUTPUT},
	{NULL, 0},
};

static int backlight_enable;

#define AM335XEVM_WLAN_PMENA_GPIO	GPIO_TO_PIN(1, 30)
#define AM335XEVM_WLAN_IRQ_GPIO		GPIO_TO_PIN(3, 17)

struct wl12xx_platform_data am335xevm_wlan_data = {
	.irq = OMAP_GPIO_IRQ(AM335XEVM_WLAN_IRQ_GPIO),
	.board_ref_clock = WL12XX_REFCLOCK_38_XTAL, /* 38.4Mhz */
};

/* Module pin mux for wlan and bluetooth */
static struct pinmux_config mmc2_wl12xx_pin_mux[] = {
	{"gpmc_a1.mmc2_dat0", OMAP_MUX_MODE3 | AM33XX_PIN_INPUT_PULLUP},
	{"gpmc_a2.mmc2_dat1", OMAP_MUX_MODE3 | AM33XX_PIN_INPUT_PULLUP},
	{"gpmc_a3.mmc2_dat2", OMAP_MUX_MODE3 | AM33XX_PIN_INPUT_PULLUP},
	{"gpmc_ben1.mmc2_dat3", OMAP_MUX_MODE3 | AM33XX_PIN_INPUT_PULLUP},
	{"gpmc_csn3.mmc2_cmd", OMAP_MUX_MODE3 | AM33XX_PIN_INPUT_PULLUP},
	{"gpmc_clk.mmc2_clk", OMAP_MUX_MODE3 | AM33XX_PIN_INPUT_PULLUP},
	{NULL, 0},
};

static struct pinmux_config uart1_wl12xx_pin_mux[] = {
	{"uart1_ctsn.uart1_ctsn", OMAP_MUX_MODE0 | AM33XX_PIN_OUTPUT},
	{"uart1_rtsn.uart1_rtsn", OMAP_MUX_MODE0 | AM33XX_PIN_INPUT},
	{"uart1_rxd.uart1_rxd", OMAP_MUX_MODE0 | AM33XX_PIN_INPUT_PULLUP},
	{"uart1_txd.uart1_txd", OMAP_MUX_MODE0 | AM33XX_PULL_ENBL},
	{NULL, 0},
};

static struct pinmux_config wl12xx_pin_mux_evm_rev1_1a[] = {
	{"gpmc_a0.gpio1_16", OMAP_MUX_MODE7 | AM33XX_PIN_OUTPUT},
	{"mcasp0_ahclkr.gpio3_17", OMAP_MUX_MODE7 | AM33XX_PIN_INPUT},
	{"mcasp0_ahclkx.gpio3_21", OMAP_MUX_MODE7 | AM33XX_PIN_OUTPUT},
	{NULL, 0},
 };

static struct pinmux_config wl12xx_pin_mux_evm_rev1_0[] = {
	{"gpmc_csn1.gpio1_30", OMAP_MUX_MODE7 | AM33XX_PIN_OUTPUT},
	{"mcasp0_ahclkr.gpio3_17", OMAP_MUX_MODE7 | AM33XX_PIN_INPUT},
	{"gpmc_csn2.gpio1_31", OMAP_MUX_MODE7 | AM33XX_PIN_OUTPUT},
	{NULL, 0},
 };

static void enable_ecap0(int evm_id, int profile)
{
	backlight_enable = true;
}

/* Setup pwm-backlight */
static struct platform_device am335x_backlight = {
	.name           = "pwm-backlight",
	.id             = -1,
	.dev            = {
		.platform_data  = &am335x_backlight_data,
	}
};

static int __init ecap0_init(void)
{
	int status = 0;

	if (backlight_enable) {
		setup_pin_mux(ecap0_pin_mux);
		platform_device_register(&am335x_backlight);
	}
	return status;
}
late_initcall(ecap0_init);

static int __init conf_disp_pll(int rate)
{
	struct clk *disp_pll;
	int ret = -EINVAL;

	disp_pll = clk_get(NULL, "dpll_disp_ck");
	if (IS_ERR(disp_pll)) {
		pr_err("Cannot clk_get disp_pll\n");
		goto out;
	}

	ret = clk_set_rate(disp_pll, rate);
	clk_put(disp_pll);
out:
	return ret;
}

static void lcdc_init(int evm_id, int profile)
{

	setup_pin_mux(lcdc_pin_mux);

	if (conf_disp_pll(300000000)) {
		pr_info("Failed configure display PLL, not attempting to"
				"register LCDC\n");
		return;
	}

	if (am33xx_register_lcdc(&calixtovga_pdata))
		pr_info("Failed to register LCDC device\n");
	return;
}

static void tsc_init(int evm_id, int profile)
{
	int err;

	if (gp_evm_revision == GP_EVM_REV_IS_1_1A) {
		am335x_touchscreen_data.analog_input = 1;
		pr_info("TSC connected to beta GP EVM\n");
	} else {
		am335x_touchscreen_data.analog_input = 0;
		pr_info("TSC connected to alpha GP EVM\n");
	}
	setup_pin_mux(tsc_pin_mux);
	err = platform_device_register(&tsc_device);
	if (err)
		pr_err("failed to register touchscreen device\n");
}




static void mii2_init(int evm_id, int profile)
{
	setup_pin_mux(mii2_pin_mux);
	return;
}


static void rmii1_init(int evm_id, int profile)
{
	setup_pin_mux(rmii1_pin_mux);
	return;
}

static void usb0_init(int evm_id, int profile)
{
	setup_pin_mux(usb0_pin_mux);
	return;
}

static void usb1_init(int evm_id, int profile)
{
	setup_pin_mux(usb1_pin_mux);
	return;
}

/* setup uart3 */
static void uart3_init(int evm_id, int profile)
{
	setup_pin_mux(uart3_pin_mux);
	return;
}

/* setup uart2 */
static void uart2_init(int evm_id, int profile)
{
	setup_pin_mux(uart2_pin_mux);
	return;
}

/* setup haptics */
#define HAPTICS_MAX_FREQ (250)


/* NAND partition information */
static struct mtd_partition am335x_nand_partitions[] = {
/* All the partition sizes are listed in terms of NAND block size */
	{
		.name           = "SPL",
		.offset         = 0,			/* Offset = 0x0 */
		.size           = SZ_128K,
	},
	{
		.name           = "SPL.backup1",
		.offset         = MTDPART_OFS_APPEND,	/* Offset = 0x20000 */
		.size           = SZ_128K,
	},
	{
		.name           = "SPL.backup2",
		.offset         = MTDPART_OFS_APPEND,	/* Offset = 0x40000 */
		.size           = SZ_128K,
	},
	{
		.name           = "SPL.backup3",
		.offset         = MTDPART_OFS_APPEND,	/* Offset = 0x60000 */
		.size           = SZ_128K,
	},
	{
		.name           = "U-Boot",
		.offset         = MTDPART_OFS_APPEND,   /* Offset = 0x80000 */
		.size           = 15 * SZ_128K,
	},
	{
		.name           = "U-Boot Env",
		.offset         = MTDPART_OFS_APPEND,   /* Offset = 0x260000 */
		.size           = 1 * SZ_128K,
	},
	{
		.name           = "Kernel",
		.offset         = MTDPART_OFS_APPEND,   /* Offset = 0x280000 */
		.size           = 40 * SZ_128K,
	},
	{
		.name           = "File System",
		.offset         = MTDPART_OFS_APPEND,   /* Offset = 0x780000 */
		.size           = MTDPART_SIZ_FULL,
	},
};

/* SPI 0/1 Platform Data */
/* SPI flash information */
static struct mtd_partition am335x_spi_partitions[] = {
	/* All the partition sizes are listed in terms of erase size */
	{
		.name       = "SPL",
		.offset     = 0,			/* Offset = 0x0 */
		.size       = SZ_128K,
	},
	{
		.name       = "U-Boot",
		.offset     = MTDPART_OFS_APPEND,	/* Offset = 0x20000 */
		.size       = 2 * SZ_128K,
	},
	{
		.name       = "U-Boot Env",
		.offset     = MTDPART_OFS_APPEND,	/* Offset = 0x60000 */
		.size       = 2 * SZ_4K,
	},
	{
		.name       = "Kernel",
		.offset     = MTDPART_OFS_APPEND,	/* Offset = 0x62000 */
		.size       = 28 * SZ_128K,
	},
	{
		.name       = "File System",
		.offset     = MTDPART_OFS_APPEND,	/* Offset = 0x3E2000 */
		.size       = MTDPART_SIZ_FULL,		/* size ~= 4.1 MiB */
	}
};

static const struct flash_platform_data am335x_spi_flash = {
	.type      = "w25q64",
	.name      = "spi_flash",
	.parts     = am335x_spi_partitions,
	.nr_parts  = ARRAY_SIZE(am335x_spi_partitions),
};

/*
 * SPI Flash works at 80Mhz however SPI Controller works at 48MHz.
 * So setup Max speed to be less than that of Controller speed
 */
static struct spi_board_info am335x_spi0_slave_info[] = {
	{
		.modalias      = "m25p80",
		.platform_data = &am335x_spi_flash,
		.irq           = -1,
		.max_speed_hz  = 24000000,
		.bus_num       = 1,
		.chip_select   = 0,
	},
};

static struct spi_board_info am335x_spi1_slave_info[] = {
	{
		.modalias      = "m25p80",
		.platform_data = &am335x_spi_flash,
		.irq           = -1,
		.max_speed_hz  = 12000000,
		.bus_num       = 2,
		.chip_select   = 0,
	},
};

static void evm_nand_init(int evm_id, int profile)
{
	setup_pin_mux(nand_pin_mux);
	board_nand_init(am335x_nand_partitions,
		ARRAY_SIZE(am335x_nand_partitions), 0, 0);
}

/* TPS65217 voltage regulator support */

/* 1.8V */
static struct regulator_consumer_supply tps65217_dcdc1_consumers[] = {
	{
		.supply = "vdds_osc",
	},
	{
		.supply = "vdds_pll_ddr",
	},
	{
		.supply = "vdds_pll_mpu",
	},
	{
		.supply = "vdds_pll_core_lcd",
	},
	{
		.supply = "vdds_sram_mpu_bb",
	},
	{
		.supply = "vdds_sram_core_bg",
	},
	{
		.supply = "vdda_usb0_1p8v",
	},
	{
		.supply = "vdds_ddr",
	},
	{
		.supply = "vdds",
	},
	{
		.supply = "vdds_hvx_1p8v",
	},
	{
		.supply = "vdda_adc",
	},
	{
		.supply = "ddr2",
	},
};

/* 1.1V */
static struct regulator_consumer_supply tps65217_dcdc2_consumers[] = {
	{
		.supply = "vdd_mpu",
	},
};

/* 1.1V */
static struct regulator_consumer_supply tps65217_dcdc3_consumers[] = {
	{
		.supply = "vdd_core",
	},
};

/* 1.8V LDO */
static struct regulator_consumer_supply tps65217_ldo1_consumers[] = {
	{
		.supply = "vdds_rtc",
	},
};

/* 3.3V LDO */
static struct regulator_consumer_supply tps65217_ldo2_consumers[] = {
	{
		.supply = "vdds_any_pn",
	},
};

/* 3.3V LDO */
static struct regulator_consumer_supply tps65217_ldo3_consumers[] = {
	{
		.supply = "vdds_hvx_ldo3_3p3v",
	},
	{
		.supply = "vdda_usb0_3p3v",
	},
};

/* 3.3V LDO */
static struct regulator_consumer_supply tps65217_ldo4_consumers[] = {
	{
		.supply = "vdds_hvx_ldo4_3p3v",
	},
};

static struct regulator_init_data tps65217_regulator_data[] = {
	/* dcdc1 */
	{
		.constraints = {
			.min_uV = 900000,
			.max_uV = 1800000,
			.boot_on = 1,
			.always_on = 1,
		},
		.num_consumer_supplies = ARRAY_SIZE(tps65217_dcdc1_consumers),
		.consumer_supplies = tps65217_dcdc1_consumers,
	},

	/* dcdc2 */
	{
		.constraints = {
			.min_uV = 900000,
			.max_uV = 3300000,
			.valid_ops_mask = (REGULATOR_CHANGE_VOLTAGE |
				REGULATOR_CHANGE_STATUS),
			.boot_on = 1,
			.always_on = 1,
		},
		.num_consumer_supplies = ARRAY_SIZE(tps65217_dcdc2_consumers),
		.consumer_supplies = tps65217_dcdc2_consumers,
	},

	/* dcdc3 */
	{
		.constraints = {
			.min_uV = 900000,
			.max_uV = 1500000,
			.valid_ops_mask = (REGULATOR_CHANGE_VOLTAGE |
				REGULATOR_CHANGE_STATUS),
			.boot_on = 1,
			.always_on = 1,
		},
		.num_consumer_supplies = ARRAY_SIZE(tps65217_dcdc3_consumers),
		.consumer_supplies = tps65217_dcdc3_consumers,
	},

	/* ldo1 */
	{
		.constraints = {
			.min_uV = 1000000,
			.max_uV = 3300000,
			.valid_ops_mask = REGULATOR_CHANGE_STATUS,
			.boot_on = 1,
			.always_on = 1,
		},
		.num_consumer_supplies = ARRAY_SIZE(tps65217_ldo1_consumers),
		.consumer_supplies = tps65217_ldo1_consumers,
	},

	/* ldo2 */
	{
		.constraints = {
			.min_uV = 900000,
			.max_uV = 3300000,
			.valid_ops_mask = (REGULATOR_CHANGE_VOLTAGE |
				REGULATOR_CHANGE_STATUS),
			.boot_on = 1,
			.always_on = 1,
		},
		.num_consumer_supplies = ARRAY_SIZE(tps65217_ldo2_consumers),
		.consumer_supplies = tps65217_ldo2_consumers,
	},

	/* ldo3 */
	{
		.constraints = {
			.min_uV = 1800000,
			.max_uV = 3300000,
			.valid_ops_mask = (REGULATOR_CHANGE_VOLTAGE |
				REGULATOR_CHANGE_STATUS),
			.boot_on = 1,
			.always_on = 1,
		},
		.num_consumer_supplies = ARRAY_SIZE(tps65217_ldo3_consumers),
		.consumer_supplies = tps65217_ldo3_consumers,
	},

	/* ldo4 */
	{
		.constraints = {
			.min_uV = 1800000,
			.max_uV = 3300000,
			.valid_ops_mask = (REGULATOR_CHANGE_VOLTAGE |
				REGULATOR_CHANGE_STATUS),
			.boot_on = 1,
			.always_on = 1,
		},
		.num_consumer_supplies = ARRAY_SIZE(tps65217_ldo4_consumers),
		.consumer_supplies = tps65217_ldo4_consumers,
	},
};

static struct tps65217_board beaglebone_tps65217_info = {
	.tps65217_init_data = &tps65217_regulator_data[0],
};




static void mmc2_wl12xx_init(int evm_id, int profile)
{
	setup_pin_mux(mmc2_wl12xx_pin_mux);

	am335x_mmc[1].mmc = 3;
	am335x_mmc[1].name = "wl1271";
	am335x_mmc[1].caps = MMC_CAP_4_BIT_DATA | MMC_CAP_POWER_OFF_CARD
				| MMC_PM_KEEP_POWER;
	am335x_mmc[1].nonremovable = true;
	am335x_mmc[1].gpio_cd = -EINVAL;
	am335x_mmc[1].gpio_wp = -EINVAL;
	am335x_mmc[1].ocr_mask = MMC_VDD_32_33 | MMC_VDD_33_34; /* 3V3 */

	/* mmc will be initialized when mmc0_init is called */
	return;
}

static void uart1_wl12xx_init(int evm_id, int profile)
{
	setup_pin_mux(uart1_wl12xx_pin_mux);
}

static void wl12xx_bluetooth_enable(void)
{
	int status = gpio_request(am335xevm_wlan_data.bt_enable_gpio,
		"bt_en\n");
	if (status < 0)
		pr_err("Failed to request gpio for bt_enable");

	pr_info("Configure Bluetooth Enable pin...\n");
	gpio_direction_output(am335xevm_wlan_data.bt_enable_gpio, 0);
}

static int wl12xx_set_power(struct device *dev, int slot, int on, int vdd)
{
	if (on) {
		gpio_set_value(am335xevm_wlan_data.wlan_enable_gpio, 1);
		mdelay(70);
	}
	else
		gpio_set_value(am335xevm_wlan_data.wlan_enable_gpio, 0);

	return 0;
}

static void wl12xx_init(int evm_id, int profile)
{
	struct device *dev;
	struct omap_mmc_platform_data *pdata;
	int ret;

	/* Register WLAN and BT enable pins based on the evm board revision */
	if (gp_evm_revision == GP_EVM_REV_IS_1_1A) {
		am335xevm_wlan_data.wlan_enable_gpio = GPIO_TO_PIN(1, 16);
		am335xevm_wlan_data.bt_enable_gpio = GPIO_TO_PIN(3, 21);
	}
	else {
		am335xevm_wlan_data.wlan_enable_gpio = GPIO_TO_PIN(1, 30);
		am335xevm_wlan_data.bt_enable_gpio = GPIO_TO_PIN(1, 31);
	}

	wl12xx_bluetooth_enable();

	if (wl12xx_set_platform_data(&am335xevm_wlan_data))
		pr_err("error setting wl12xx data\n");

	dev = am335x_mmc[1].dev;
	if (!dev) {
		pr_err("wl12xx mmc device initialization failed\n");
		goto out;
	}

	pdata = dev->platform_data;
	if (!pdata) {
		pr_err("Platfrom data of wl12xx device not set\n");
		goto out;
	}

	ret = gpio_request_one(am335xevm_wlan_data.wlan_enable_gpio,
		GPIOF_OUT_INIT_LOW, "wlan_en");
	if (ret) {
		pr_err("Error requesting wlan enable gpio: %d\n", ret);
		goto out;
	}

	if (gp_evm_revision == GP_EVM_REV_IS_1_1A)
		setup_pin_mux(wl12xx_pin_mux_evm_rev1_1a);
	else
		setup_pin_mux(wl12xx_pin_mux_evm_rev1_0);

	pdata->slots[0].set_power = wl12xx_set_power;
out:
	return;
}

static void d_can_init(int evm_id, int profile)
{
	switch (evm_id) {
	case IND_AUT_MTR_EVM:
		if ((profile == PROFILE_0) || (profile == PROFILE_1)) {
			setup_pin_mux(d_can_ia_pin_mux);
			/* Instance Zero */
			am33xx_d_can_init(0);
		}
		break;
	case GEN_PURP_EVM:
		if (profile == PROFILE_1) {
			setup_pin_mux(d_can_gp_pin_mux);
			/* Instance One */
			am33xx_d_can_init(1);
		}
		break;
	default:
		break;
	}
}

static void mmc0_init(int evm_id, int profile)
{
	setup_pin_mux(mmc0_pin_mux);

	omap2_hsmmc_init(am335x_mmc);
	return;
}

static struct i2c_board_info am335x_i2c_boardinfo[] = {
	{
		I2C_BOARD_INFO("tlv320aic3x", 0x18),
},
	{
		I2C_BOARD_INFO("tps65217", TPS65217_I2C_ID),
		.platform_data  = &beaglebone_tps65217_info,
	},
};


static void mmc0_no_cd_init(int evm_id, int profile)
{
	setup_pin_mux(mmc0_no_cd_pin_mux);

	omap2_hsmmc_init(am335x_mmc);
	return;
}


/* setup spi0 */
static void spi0_init(int evm_id, int profile)
{
	setup_pin_mux(spi0_pin_mux);
	spi_register_board_info(am335x_spi0_slave_info,
			ARRAY_SIZE(am335x_spi0_slave_info));
	return;
}



static int calixto_phy_fixup(struct phy_device *phydev)
{

	return 0;
}


static void profibus_init(int evm_id, int profile)
{
	setup_pin_mux(profibus_pin_mux);
	return;
}

static struct evm_dev_cfg calixto_dev_cfg[] = {
	{rmii1_init,	DEV_ON_BASEBOARD, PROFILE_NONE},
	{mii2_init,	DEV_ON_BASEBOARD, PROFILE_NONE},
	{usb0_init,	DEV_ON_BASEBOARD, PROFILE_NONE},
	{usb1_init,	DEV_ON_BASEBOARD, PROFILE_NONE},
	{mmc0_init,	DEV_ON_BASEBOARD, PROFILE_NONE},
	{NULL, 0, 0},
};



static void setup_calixto_board(void)
{
	pr_info("The board is a AM335x Calixto EVM Rev 0.2\n");

	/* Beagle Bone has Micro-SD slot which doesn't have Write Protect pin */
	am335x_mmc[0].gpio_wp = -EINVAL;

	_configure_device(LOW_COST_EVM, calixto_dev_cfg, PROFILE_NONE);

	phy_register_fixup_for_uid(CALIXTO_PHY_ID, CALIXTO_PHY_MASK,
					calixto_phy_fixup);
	am33xx_cpsw_init(0);
}



static struct regulator_init_data am335x_dummy = {
	.constraints.always_on	= true,
};

static struct regulator_consumer_supply am335x_vdd1_supply[] = {
	REGULATOR_SUPPLY("vdd_mpu", NULL),
};

static struct regulator_init_data am335x_vdd1 = {
	.constraints = {
		.min_uV			= 600000,
		.max_uV			= 1500000,
		.valid_modes_mask	= REGULATOR_MODE_NORMAL,
		.valid_ops_mask		= REGULATOR_CHANGE_VOLTAGE,
		.always_on		= 1,
	},
	.num_consumer_supplies	= ARRAY_SIZE(am335x_vdd1_supply),
	.consumer_supplies	= am335x_vdd1_supply,
};

static struct tps65910_board am335x_tps65910_info = {
	.tps65910_pmic_init_data[TPS65910_REG_VRTC]	= &am335x_dummy,
	.tps65910_pmic_init_data[TPS65910_REG_VIO]	= &am335x_dummy,
	.tps65910_pmic_init_data[TPS65910_REG_VDD1]	= &am335x_vdd1,
	.tps65910_pmic_init_data[TPS65910_REG_VDD2]	= &am335x_dummy,
	.tps65910_pmic_init_data[TPS65910_REG_VDD3]	= &am335x_dummy,
	.tps65910_pmic_init_data[TPS65910_REG_VDIG1]	= &am335x_dummy,
	.tps65910_pmic_init_data[TPS65910_REG_VDIG2]	= &am335x_dummy,
	.tps65910_pmic_init_data[TPS65910_REG_VPLL]	= &am335x_dummy,
	.tps65910_pmic_init_data[TPS65910_REG_VDAC]	= &am335x_dummy,
	.tps65910_pmic_init_data[TPS65910_REG_VAUX1]	= &am335x_dummy,
	.tps65910_pmic_init_data[TPS65910_REG_VAUX2]	= &am335x_dummy,
	.tps65910_pmic_init_data[TPS65910_REG_VAUX33]	= &am335x_dummy,
	.tps65910_pmic_init_data[TPS65910_REG_VMMC]	= &am335x_dummy,
};

/*
* Daughter board Detection.
* Every board has a ID memory (EEPROM) on board. We probe these devices at
* machine init, starting from daughter board and ending with baseboard.
* Assumptions :
*	1. probe for i2c devices are called in the order they are included in
*	   the below struct. Daughter boards eeprom are probed 1st. Baseboard
*	   eeprom probe is called last.
*/


static struct omap_musb_board_data musb_board_data = {
	.interface_type	= MUSB_INTERFACE_ULPI,
	.mode           = MUSB_HOST,
	.power		= 500,
	.instances	= 1,
};

static struct omap_musb_board_data musb1_board_data = {
	.interface_type	= MUSB_INTERFACE_ULPI,
	.mode           = MUSB_OTG,
	.power		= 500,
	.instances	= 1,
};

static void __init am335x_evm_i2c_init(void)
{
	/* Initially assume Low Cost EVM Config */
	am335x_evm_id = LOW_COST_EVM;
	omap_register_i2c_bus(1, 100, am335x_i2c_boardinfo,
				ARRAY_SIZE(am335x_i2c_boardinfo));
}

static struct resource am335x_rtc_resources[] = {
	{
		.start		= AM33XX_RTC_BASE,
		.end		= AM33XX_RTC_BASE + SZ_4K - 1,
		.flags		= IORESOURCE_MEM,
	},
	{ /* timer irq */
		.start		= AM33XX_IRQ_RTC_TIMER,
		.end		= AM33XX_IRQ_RTC_TIMER,
		.flags		= IORESOURCE_IRQ,
	},
	{ /* alarm irq */
		.start		= AM33XX_IRQ_RTC_ALARM,
		.end		= AM33XX_IRQ_RTC_ALARM,
		.flags		= IORESOURCE_IRQ,
	},
};

static struct platform_device am335x_rtc_device = {
	.name           = "omap_rtc",
	.id             = -1,
	.num_resources	= ARRAY_SIZE(am335x_rtc_resources),
	.resource	= am335x_rtc_resources,
};

static int am335x_rtc_init(void)
{
	void __iomem *base;
	struct clk *clk;

	clk = clk_get(NULL, "rtc_fck");
	if (IS_ERR(clk)) {
		pr_err("rtc : Failed to get RTC clock\n");
		return -1;
	}

	if (clk_enable(clk)) {
		pr_err("rtc: Clock Enable Failed\n");
		return -1;
	}

	base = ioremap(AM33XX_RTC_BASE, SZ_4K);

	if (WARN_ON(!base))
		return -ENOMEM;

	/* Unlock the rtc's registers */
	__raw_writel(0x83e70b13, base + 0x6c);
	__raw_writel(0x95a4f1e0, base + 0x70);

	/*
	 * Enable the 32K OSc
	 * TODO: Need a better way to handle this
	 * Since we want the clock to be running before mmc init
	 * we need to do it before the rtc probe happens
	 */
	__raw_writel(0x48, base + 0x54);

	iounmap(base);

	return  platform_device_register(&am335x_rtc_device);
}

/* Enable clkout2 */
static struct pinmux_config clkout2_pin_mux[] = {
	{"xdma_event_intr1.clkout2", OMAP_MUX_MODE3 | AM33XX_PIN_OUTPUT},
	{NULL, 0},
};

static void __init clkout2_enable(void)
{
	struct clk *ck_32;

	ck_32 = clk_get(NULL, "clkout2_ck");
	if (IS_ERR(ck_32)) {
		pr_err("Cannot clk_get ck_32\n");
		return;
	}

	clk_enable(ck_32);

	setup_pin_mux(clkout2_pin_mux);
}

void __iomem *am33xx_emif_base;

void __iomem * __init am33xx_get_mem_ctlr(void)
{

	am33xx_emif_base = ioremap(AM33XX_EMIF0_BASE, SZ_32K);

	if (!am33xx_emif_base)
		pr_warning("%s: Unable to map DDR2 controller",	__func__);

	return am33xx_emif_base;
}

void __iomem *am33xx_get_ram_base(void)
{
	return am33xx_emif_base;
}

static struct resource am33xx_cpuidle_resources[] = {
	{
		.start		= AM33XX_EMIF0_BASE,
		.end		= AM33XX_EMIF0_BASE + SZ_32K - 1,
		.flags		= IORESOURCE_MEM,
	},
};

/* AM33XX devices support DDR2 power down */
static struct am33xx_cpuidle_config am33xx_cpuidle_pdata = {
	.ddr2_pdown	= 1,
};

static struct platform_device am33xx_cpuidle_device = {
	.name			= "cpuidle-am33xx",
	.num_resources		= ARRAY_SIZE(am33xx_cpuidle_resources),
	.resource		= am33xx_cpuidle_resources,
	.dev = {
		.platform_data	= &am33xx_cpuidle_pdata,
	},
};

static void __init am33xx_cpuidle_init(void)
{
	int ret;

	am33xx_cpuidle_pdata.emif_base = am33xx_get_mem_ctlr();

	ret = platform_device_register(&am33xx_cpuidle_device);

	if (ret)
		pr_warning("AM33XX cpuidle registration failed\n");

}

static void __init am335x_evm_init(void)
{
	am33xx_cpuidle_init();
	am33xx_mux_init(board_mux);
	omap_serial_init();
	am335x_rtc_init();
	clkout2_enable();
	am335x_evm_i2c_init();
	omap_sdrc_init(NULL, NULL);
	printk(KERN_INFO"usb init\n\r");
	usb_musb_init(&musb_board_data, &musb1_board_data);
	setup_calixto_board();
	if (clk_add_alias("pruss", NULL, "icss_uart_gclk", NULL))
		pr_warn("failed to create an alias: icss_uart_gclk --> pruss\n");
	if (clk_add_alias("pruss", NULL, "icss_fck", NULL))
		pr_warn("failed to create an alias: icss_fck --> pruss\n");
	/* Create an alias for gfx/sgx clock */
	if (clk_add_alias("sgx_ck", NULL, "gfx_fclk", NULL))
		pr_warn("failed to create an alias: gfx_fclk --> sgx_ck\n");
        lcdc_init(DEV_ON_BASEBOARD , PROFILE_0|PROFILE_1|PROFILE_2|PROFILE_7);
}

static void __init am335x_evm_map_io(void)
{
	omap2_set_globals_am33xx();
	omapam33xx_map_common_io();
}

MACHINE_START(AM335XEVM, "am335xevm")
	/* Maintainer: Texas Instruments */
	.atag_offset	= 0x100,
	.map_io		= am335x_evm_map_io,
	.init_early	= am33xx_init_early,
	.init_irq	= ti81xx_init_irq,
	.handle_irq     = omap3_intc_handle_irq,
	.timer		= &omap3_am33xx_timer,
	.init_machine	= am335x_evm_init,
MACHINE_END

MACHINE_START(AM335XIAEVM, "am335xiaevm")
	/* Maintainer: Texas Instruments */
	.atag_offset	= 0x100,
	.map_io		= am335x_evm_map_io,
	.init_irq	= ti81xx_init_irq,
	.init_early	= am33xx_init_early,
	.timer		= &omap3_am33xx_timer,
	.init_machine	= am335x_evm_init,
MACHINE_END
