#include <linux/bitops.h>
#include <linux/delay.h>
#include <linux/init.h>
#include <linux/kernel.h>
#include <linux/module.h>
#include <video/mipi_display.h>

#include "fbtft.h"

#define DRVNAME "fb_st7365p"

#define DEFAULT_GAMMA \
	"F0 04 08 09 08 15 2F 42 46 28 15 16 29 2D\n" \
	"F0 04 09 09 08 15 2E 46 46 28 15 15 29 2D"

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

/**
 * init_display() - initialize the display controller
 *
 * @par: FBTFT parameter object
 *
 * Return: 0 on success, < 0 if error occurred.
 */
static int init_display(struct fbtft_par *par)
{
	par->fbtftops.reset(par);

	write_reg(par, MIPI_DCS_SOFT_RESET);
	mdelay(100);

	write_reg(par, MIPI_DCS_EXIT_SLEEP_MODE);
	mdelay(20);

	write_reg(par, CSCON, 0x00C3);
	write_reg(par, CSCON, 0x0096);

	write_reg(par, MIPI_DCS_SET_PIXEL_FORMAT, 0x0055);

	write_reg(par, DIC, 0x0001);
	write_reg(par, EM, 0x00C6);

	write_reg(par, PWR2, 0x0015);
	write_reg(par, PWR3, 0x00AF);
	write_reg(par, VCMPCTL, 0x0022);
	write_reg(par, VCMOST, 0x0000);
	write_reg(par, DOCA, 0x0040, 0x008A, 0x0000, 0x0000, 0x0029, 0x0019, 0x00A5, 0x0033);

	write_reg(par, MIPI_DCS_ENTER_NORMAL_MODE);

	write_reg(par, MIPI_DCS_WRITE_CONTROL_DISPLAY, 0x0024);
	write_reg(par, CSCON, 0x003C);
	write_reg(par, CSCON, 0x0069);
	write_reg(par, MIPI_DCS_SET_DISPLAY_ON);

	write_reg(par, MIPI_DCS_ENTER_INVERT_MODE);

	return 0;
}

/**
 * set_var() - apply LCD properties like rotation and BGR mode
 *
 * @par: FBTFT parameter object
 *
 * Return: 0 on success, < 0 if error occurred.
 */
static int set_var(struct fbtft_par *par)
{
	u8 madctl_par = 0;

	// NOTE: The meaning of this attribute has opposite effect on the controller so
	// 'MADCTL_BGR' is used for RGB mode
	// it is possibly connected with usage of 'MIPI_DCS_ENTER_INVERT_MODE'
	if (!par->bgr)
		madctl_par |= MADCTL_BGR;
	switch (par->info->var.rotate) {
	case 0:
		madctl_par |= (MADCTL_MX);
		break;
	case 90:
		madctl_par |= (MADCTL_MV | MADCTL_MX | MADCTL_MY);
		break;
	case 180:
		madctl_par |= (MADCTL_MY);
		break;
	case 270:
		madctl_par |= (MADCTL_MV);
		break;
	default:
		return -EINVAL;
	}

	write_reg(par, MIPI_DCS_SET_ADDRESS_MODE, madctl_par);
	return 0;
}

/**
 * set_gamma() - set gamma curves
 *
 * @par: FBTFT parameter object
 * @curves: gamma curves
 *
 * Before the gamma curves are applied, they are preprocessed with a bitmask
 * to ensure syntactically correct input for the display controller.
 * This implies that the curves input parameter might be changed by this
 * function and that illegal gamma values are auto-corrected and not
 * reported as errors.
 *
 * Return: 0 on success, < 0 if error occurred.
 */
static int set_gamma(struct fbtft_par *par, u32 *curves)
{
	int i;
	int j;
	int c; /* curve index offset */

	/*
	 * Bitmasks for gamma curve command parameters.
	 * The masks are the same for both positive and negative voltage
	 * gamma curves.
	 */
	static const u8 gamma_par_mask[] = {
		0xFF, /* V63[3:0], V0[3:0]*/
		0x3F, /* V1[5:0] */
		0x3F, /* V2[5:0] */
		0x1F, /* V4[4:0] */
		0x1F, /* V6[4:0] */
		0x3F, /* J0[1:0], V13[3:0] */
		0x7F, /* V20[6:0] */
		0x77, /* V36[2:0], V27[2:0] */
		0x7F, /* V43[6:0] */
		0x3F, /* J1[1:0], V50[3:0] */
		0x1F, /* V57[4:0] */
		0x1F, /* V59[4:0] */
		0x3F, /* V61[5:0] */
		0x3F, /* V62[5:0] */
	};

	for (i = 0; i < par->gamma.num_curves; i++) {
		c = i * par->gamma.num_values;
		for (j = 0; j < par->gamma.num_values; j++)
			curves[c + j] &= gamma_par_mask[j];
		write_reg(par, PGC + i,
			  curves[c + 0],  curves[c + 1],  curves[c + 2],
			  curves[c + 3],  curves[c + 4],  curves[c + 5],
			  curves[c + 6],  curves[c + 7],  curves[c + 8],
			  curves[c + 9],  curves[c + 10], curves[c + 11],
			  curves[c + 12], curves[c + 13]);
	}
	return 0;
}

/**
 * blank() - blank the display
 *
 * @par: FBTFT parameter object
 * @on: whether to enable or disable blanking the display
 *
 * Return: 0 on success, < 0 if error occurred.
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
	.gamma_num = 2,
	.gamma_len = 14,
	.gamma = DEFAULT_GAMMA,
	.fbtftops = {
		.init_display = init_display,
		.set_var = set_var,
		.set_gamma = set_gamma,
		.blank = blank,
	},
};

FBTFT_REGISTER_DRIVER(DRVNAME, "sitronix,st7365p", &display);

MODULE_ALIAS("spi:" DRVNAME);
MODULE_ALIAS("platform:" DRVNAME);
MODULE_ALIAS("spi:st7365p");
MODULE_ALIAS("platform:st7365p");

MODULE_DESCRIPTION("FB driver for the ST7365P LCD Controller");
MODULE_AUTHOR("Braiins Systems s.r.o.");
MODULE_LICENSE("GPL");
