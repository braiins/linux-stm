#include <linux/bitops.h>
#include <linux/delay.h>
#include <linux/init.h>
#include <linux/kernel.h>
#include <linux/module.h>
#include <video/mipi_display.h>

#include "fbtft.h"

#define DRVNAME "fb_st7796s"

enum st7365p_command {
	DIC = 0xB4,	// Display Inversion Control
	EM = 0xB7,	// Entry Mode Set
	PWR1 = 0xC0,	// Power Control 1
	PWR2 = 0xC1,	// Power Control 2
	PWR3 = 0xC2,	// Power Control 3
	VCMPCTL = 0xC5,	// VCOM Control
	VCMOST = 0xC6,	// VCOM Offset Register
	PGC = 0xE0,	// Positive Gamma Control
	NGC = 0xE1,	// Negative Gamma Control
	DOCA = 0xE8,	// Display Output Ctrl Adjust
	CSCON = 0xF0,	// Command Set Control
};

#define MADCTL_MY	0x80 // Row Address Order
#define MADCTL_MX	0x40 // Column Address Order
#define MADCTL_MV	0x20 // Row/Column Exchange
#define MADCTL_ML	0x10 // Vertical Refresh Order
#define MADCTL_BGR	0x08 // RGB-BGR ORDER
#define MADCTL_RGB	0x00
#define MADCTL_MH	0x04 // Horizontal Refresh Order

#define TFT_NO_ROTATION	(MADCTL_MX)
#define TFT_ROTATE_90	(MADCTL_MV | MADCTL_MX | MADCTL_MY)
#define TFT_ROTATE_180	(MADCTL_MY)
#define TFT_ROTATE_270	(MADCTL_MV)

/**
 * init_display() - initialize the display controller
 */

static int init_display(struct fbtft_par *par)
{
	uint8_t madctrl_data;

	pr_info("ST7796 driver: load");
	pr_info("ST7796 Rotation: %d",par->pdata->rotate);

	write_reg(par, MIPI_DCS_SOFT_RESET);
	mdelay(100);

	write_reg(par, MIPI_DCS_EXIT_SLEEP_MODE);
	mdelay(20);

	write_reg(par, CSCON, 0x00C3);
	write_reg(par, CSCON, 0x0096);


	switch (par->pdata->rotate)
	{
	case 90:
		pr_info("ST7796 Set rotation 90");
		madctrl_data = TFT_ROTATE_90;
		break;

	case 180:
		pr_info("ST7796 Set rotation 180");
		madctrl_data = TFT_ROTATE_180;
		break;

	case 270:
		pr_info("ST7796 Set rotation 270");
		madctrl_data = TFT_ROTATE_270;
		break;

	default:
		pr_info("ST7796 Set rotation 0");
		madctrl_data = TFT_NO_ROTATION;
		break;
	}

	madctrl_data |= MADCTL_RGB;

	pr_info("ST7796 MADCTRL: 0x%0X",madctrl_data);

	write_reg(par, MIPI_DCS_SET_ADDRESS_MODE, madctrl_data);
	write_reg(par, MIPI_DCS_SET_PIXEL_FORMAT, 0x0055);

	write_reg(par, DIC, 0x0001);
	write_reg(par, EM, 0x00C6);

	write_reg(par, PWR2, 0x0015);
	write_reg(par, PWR3, 0x00AF);
	write_reg(par, VCMPCTL, 0x0022);
	write_reg(par, VCMOST, 0x0000);
	write_reg(par, DOCA, 0x0040, 0x008A, 0x0000, 0x0000, 0x0029, 0x0019, 0x00A5, 0x0033);

	write_reg(par, PGC, 0x00F0, 0x0004, 0x0008, 0x0009, 0x0008, 0x0015, 0x002F, 0x0042, 0x0046, 0x0028, 0x0015, 0x0016, 0x0029, 0x002D);
	write_reg(par, NGC, 0x00F0, 0x0004, 0x0009, 0x0009, 0x0008, 0x0015, 0x002E, 0x0046, 0x0046, 0x0028, 0x0015, 0x0015, 0x0029, 0x002D);

	write_reg(par, MIPI_DCS_ENTER_NORMAL_MODE);

	write_reg(par, MIPI_DCS_WRITE_CONTROL_DISPLAY, 0x0024);
	write_reg(par, CSCON, 0x003C);
	write_reg(par, CSCON, 0x0069);
	write_reg(par, MIPI_DCS_SET_DISPLAY_ON);

	write_reg(par, MIPI_DCS_ENTER_INVERT_MODE);

	return 0;
}

/**
 * blank() - blank the display
 */
static int blank(struct fbtft_par *par, bool on)
{
	if (on)
		write_reg(par, MIPI_DCS_SET_DISPLAY_OFF);
	else
		write_reg(par, MIPI_DCS_SET_DISPLAY_ON);
	return 0;
}

static struct fbtft_display display = {
	.regwidth = 8,
	.width = 320,
	.height = 480,
	.fbtftops = {
		.init_display = init_display,
		.blank = blank,
	},
};

FBTFT_REGISTER_DRIVER(DRVNAME, "sitronix,st7796s", &display);

MODULE_ALIAS("spi:" DRVNAME);
MODULE_ALIAS("platform:" DRVNAME);
MODULE_ALIAS("spi:st7796s");
MODULE_ALIAS("platform:st7796s");

MODULE_DESCRIPTION("FB driver for the ST7796S LCD Controller");
MODULE_AUTHOR("NNN");
MODULE_LICENSE("GPL");
