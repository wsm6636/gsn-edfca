/*
 * Copyright (C) 2008-2017 Forlinx, Inc. All Rights Reserved.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.

 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.

 * You should have received a copy of the GNU General Public License along
 * with this program; if not, write to the Free Software Foundation, Inc.,
 */

#include <linux/types.h>
#include <linux/init.h>
#include <linux/delay.h>
#include <linux/platform_device.h>
#include <linux/err.h>
#include <linux/clk.h>
#include <linux/console.h>
#include <linux/io.h>
#include <linux/bitops.h>
#include <linux/spinlock.h>
#include <linux/mxcfb.h>
#include <linux/backlight.h>
#include <video/mipi_display.h>

//#include <mach/hardware.h>
//#include <mach/clock.h>
//#include <mach/mipi_dsi.h>

#include "mipi_dsi.h"

#define MIPI_DSI_MAX_RET_PACK_SIZE				(0x4)

#define EK79007BL_MAX_BRIGHT     (255)
#define EK79007BL_DEF_BRIGHT     (255)

#define EK79007_MAX_DPHY_CLK					(600)
#define EK79007_TWO_DATA_LANE					(0x2)

#define EK79007_CMD_CTRL_RESET                  (0x01)
#define EK79007_CMD_CTRL_RESET_PARAM_1          (0x00)

#define EK79007_CMD_GETPOWER_MODE			    (0x0A)
#define EK79007_CMD_GETPOWER_MODE_LEN			(0x1)

#define EK79007_CMD_SETGAMMA0					(0x80)
#define EK79007_CMD_SETGAMMA0_PARAM_1           (0x47)

#define EK79007_CMD_SETGAMMA1                   (0x81)
#define EK79007_CMD_SETGAMMA1_PARAM_1           (0x40)

#define EK79007_CMD_SETGAMMA2                   (0x82)
#define EK79007_CMD_SETGAMMA2_PARAM_1           (0x04)

#define EK79007_CMD_SETGAMMA3                   (0x83)
#define EK79007_CMD_SETGAMMA3_PARAM_1           (0x77)

#define EK79007_CMD_SETGAMMA4                   (0x84)
#define EK79007_CMD_SETGAMMA4_PARAM_1           (0x0f)

#define EK79007_CMD_SETGAMMA5                   (0x85)
#define EK79007_CMD_SETGAMMA5_PARAM_1           (0x70)

#define EK79007_CMD_SETGAMMA6                   (0x86)
#define EK79007_CMD_SETGAMMA6_PARAM_1           (0x70)

#define EK79007_CMD_SETPANEL1					(0xB2)
#define EK79007_CMD_SETPANEL1_TWOLANE		(0x1 << 4)

#define CHECK_RETCODE(ret)					\
do {								\
	if (ret < 0) {						\
		dev_err(&mipi_dsi->pdev->dev,			\
			"%s ERR: ret:%d, line:%d.\n",		\
			__func__, ret, __LINE__);		\
		return ret;					\
	}							\
} while (0)

static int ek79007bl_brightness;
static int mipid_init_backlight(struct mipi_dsi_info *mipi_dsi);

static struct fb_videomode truly_lcd_modedb[] = {
	{
	 "TRULY-EK79007-WVGA", 60, 1024, 600, 19531,
	 160, 150,
	 23, 11,
	 10, 1,
	 FB_SYNC_OE_LOW_ACT,
	 FB_VMODE_NONINTERLACED,
	 0,
	},
};

static struct mipi_lcd_config lcd_config = {
	.virtual_ch		= 0x0,
	.data_lane_num  = EK79007_TWO_DATA_LANE,
	.max_phy_clk    = EK79007_MAX_DPHY_CLK,
	.dpi_fmt		= MIPI_RGB888,
};
void mipid_ek79007_get_lcd_videomode(struct fb_videomode **mode, int *size,
		struct mipi_lcd_config **data)
{
//	if (cpu_is_mx6dl())
//		truly_lcd_modedb[0].pixclock = 37037; /* 27M clock*/
	*mode = &truly_lcd_modedb[0];
	*size = ARRAY_SIZE(truly_lcd_modedb);
	*data = &lcd_config;
}

int mipid_ek79007_lcd_setup(struct mipi_dsi_info *mipi_dsi)
{
	u32 buf[DSI_CMD_BUF_MAXSIZE];
	int err;

	dev_dbg(&mipi_dsi->pdev->dev, "MIPI DSI LCD setup ICs of EK79007.\n");

//reset the panel,other operation must after 50ms 
	buf[0] = EK79007_CMD_CTRL_RESET; 
	err=mipi_dsi->mipi_dsi_pkt_write(mipi_dsi, MIPI_DSI_GENERIC_SHORT_WRITE_1_PARAM,
		buf,0);
	CHECK_RETCODE(err);
    msleep(50);
//set the max size of return packet size.
	buf[0] = MIPI_DSI_MAX_RET_PACK_SIZE;
	err = mipi_dsi->mipi_dsi_pkt_write(mipi_dsi,
				MIPI_DSI_SET_MAXIMUM_RETURN_PACKET_SIZE,
				buf, 0);
	CHECK_RETCODE(err);
//set gamma curve related setting
	/* Set gamma curve related setting */
    buf[0] = EK79007_CMD_SETGAMMA0 |
		( EK79007_CMD_SETGAMMA0_PARAM_1 << 8);
	err = mipi_dsi->mipi_dsi_pkt_write(mipi_dsi, MIPI_DSI_GENERIC_SHORT_WRITE_2_PARAM,
		buf, 0);
	CHECK_RETCODE(err);
	buf[0] = EK79007_CMD_SETGAMMA1 |
		( EK79007_CMD_SETGAMMA1_PARAM_1 << 8);
	err = mipi_dsi->mipi_dsi_pkt_write(mipi_dsi, MIPI_DSI_GENERIC_SHORT_WRITE_2_PARAM,
		buf, 0);
	CHECK_RETCODE(err);
	buf[0] = EK79007_CMD_SETGAMMA2 |
		( EK79007_CMD_SETGAMMA2_PARAM_1 << 8);
	err = mipi_dsi->mipi_dsi_pkt_write(mipi_dsi, MIPI_DSI_GENERIC_SHORT_WRITE_2_PARAM,
		buf, 0);
	CHECK_RETCODE(err);
	buf[0] = EK79007_CMD_SETGAMMA3 |
		( EK79007_CMD_SETGAMMA3_PARAM_1 << 8);
	err = mipi_dsi->mipi_dsi_pkt_write(mipi_dsi, MIPI_DSI_GENERIC_SHORT_WRITE_2_PARAM,
		buf, 0);
	CHECK_RETCODE(err);
	buf[0] = EK79007_CMD_SETGAMMA4 |
		( EK79007_CMD_SETGAMMA4_PARAM_1 << 8);
	err = mipi_dsi->mipi_dsi_pkt_write(mipi_dsi, MIPI_DSI_GENERIC_SHORT_WRITE_2_PARAM,
		buf, 0);
	CHECK_RETCODE(err);
	buf[0] = EK79007_CMD_SETGAMMA5 |
		( EK79007_CMD_SETGAMMA5_PARAM_1 << 8);
	err = mipi_dsi->mipi_dsi_pkt_write(mipi_dsi, MIPI_DSI_GENERIC_SHORT_WRITE_2_PARAM,
		buf, 0);
	CHECK_RETCODE(err);
	buf[0] = EK79007_CMD_SETGAMMA6 |
		( EK79007_CMD_SETGAMMA6_PARAM_1 << 8);
	err = mipi_dsi->mipi_dsi_pkt_write(mipi_dsi, MIPI_DSI_GENERIC_SHORT_WRITE_2_PARAM,
		buf, 0);
	CHECK_RETCODE(err);

//set the data num lane of the lcd panel
/*2 lane */
	buf[0] = EK79007_CMD_SETPANEL1 |
		(EK79007_CMD_SETPANEL1_TWOLANE<< 8);
	err = mipi_dsi->mipi_dsi_pkt_write(mipi_dsi, MIPI_DSI_GENERIC_SHORT_WRITE_2_PARAM,
		buf, 0);
	CHECK_RETCODE(err);
    msleep(2);

#if 0 
	//for debug by bluesky of zdg
	buf[0] = EK79007_CMD_SETPANEL1;
	err =  mipi_dsi_pkt_read(mipi_dsi,
			MIPI_DSI_GENERIC_READ_REQUEST_1_PARAM,
			buf, EK79007_CMD_GETPOWER_MODE_LEN);
	if (!err ) {
		dev_info(&mipi_dsi->pdev->dev,
				"MIPI DSI LCD LANE COUNT:0x%x.\n", buf[0]);
	} else {
		dev_err(&mipi_dsi->pdev->dev,
			     "mipi_dsi_pkt_read err:%d, data:0x%x.\n",
			     err, buf[0]);
		dev_info(&mipi_dsi->pdev->dev,
				"MIPI DSI LCD not detected!\n");
		return err;
	}
#endif
#if 1
	/* exit sleep mode and set display on */
	buf[0] = MIPI_DCS_EXIT_SLEEP_MODE;
	err = mipi_dsi->mipi_dsi_pkt_write(mipi_dsi, MIPI_DSI_GENERIC_SHORT_WRITE_1_PARAM,
		buf, 0);
	CHECK_RETCODE(err);
	/* To allow time for the supply voltages
	 * and clock circuits to stabilize.
	 */
	msleep(5);
	buf[0] = MIPI_DCS_SET_DISPLAY_ON;
	err = mipi_dsi->mipi_dsi_pkt_write(mipi_dsi, MIPI_DSI_GENERIC_SHORT_WRITE_1_PARAM,
		buf, 0);
	CHECK_RETCODE(err);
#endif

	err = mipid_init_backlight(mipi_dsi);
	return err;
}

static int mipid_bl_update_status(struct backlight_device *bl)
{
	u32 buf;

	int brightness = bl->props.brightness;
	struct mipi_dsi_info *mipi_dsi = bl_get_data(bl);

	if (bl->props.power != FB_BLANK_UNBLANK ||
	    bl->props.fb_blank != FB_BLANK_UNBLANK)
		brightness = 0;
#if 0
	buf = HX8369_CMD_WRT_DISP_BRIGHT |
			((brightness & HX8369BL_MAX_BRIGHT) << 8);
	mipi_dsi_pkt_write(mipi_dsi, MIPI_DSI_GENERIC_SHORT_WRITE_2_PARAM,
		&buf, 0);
#endif

	ek79007bl_brightness = brightness & EK79007BL_MAX_BRIGHT;

	dev_dbg(&bl->dev, "mipid backlight bringtness:%d.\n", brightness);
	
	return 0;
}

static int mipid_bl_get_brightness(struct backlight_device *bl)
{
	return ek79007bl_brightness;
}

static int mipi_bl_check_fb(struct backlight_device *bl, struct fb_info *fbi)
{
	return 0;
}

static const struct backlight_ops mipid_lcd_bl_ops = {
	.update_status = mipid_bl_update_status,
	.get_brightness = mipid_bl_get_brightness,
	.check_fb = mipi_bl_check_fb,
};

static int mipid_init_backlight(struct mipi_dsi_info *mipi_dsi)
{

	struct backlight_properties props;
	struct backlight_device	*bl;

	if (mipi_dsi->bl) {
		pr_debug("mipid backlight already init!\n");
		return 0;
	}
	memset(&props, 0, sizeof(struct backlight_properties));
	props.max_brightness = EK79007BL_MAX_BRIGHT;
	props.type = BACKLIGHT_RAW;
	bl = backlight_device_register("mipid-bl", &mipi_dsi->pdev->dev,
		mipi_dsi, &mipid_lcd_bl_ops, &props);
	if (IS_ERR(bl)) {
		pr_err("error %ld on backlight register\n", PTR_ERR(bl));
		return PTR_ERR(bl);
	}
	mipi_dsi->bl = bl;
	bl->props.power = FB_BLANK_UNBLANK;
	bl->props.fb_blank = FB_BLANK_UNBLANK;
	bl->props.brightness = EK79007BL_DEF_BRIGHT;

	mipid_bl_update_status(bl);

	return 0;
}

