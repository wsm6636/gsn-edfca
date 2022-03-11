/*
 * Copyright (C) 2012-2015 Freescale Semiconductor, Inc. All Rights Reserved.
 */
/*
 * The code contained herein is licensed under the GNU General Public
 * License. You may obtain a copy of the GNU General Public License
 * Version 2 or later at the following locations:
 *
 * http://www.opensource.org/licenses/gpl-license.html
 * http://www.gnu.org/copyleft/gpl.html
 */

#include <linux/clk.h>
#include <linux/err.h>
#include <linux/init.h>
#include <linux/io.h>
#include <linux/mfd/syscon.h>
#include <linux/mfd/syscon/imx6q-iomuxc-gpr.h>
#include <linux/module.h>
#include <linux/of_device.h>
#include <linux/platform_device.h>
#include <linux/regmap.h>
#include <linux/types.h>
#include <video/of_videomode.h>
#include <video/videomode.h>
#include <linux/ipu.h>
#include "mxc_dispdrv.h"

#define DRIVER_NAME	"ldb"

#define LDB_BGREF_RMODE_INT		(0x1 << 15)

#define LDB_DI1_VS_POL_ACT_LOW		(0x1 << 10)
#define LDB_DI0_VS_POL_ACT_LOW		(0x1 << 9)

#define LDB_BIT_MAP_CH1_JEIDA		(0x1 << 8)
#define LDB_BIT_MAP_CH0_JEIDA		(0x1 << 6)

#define LDB_DATA_WIDTH_CH1_24		(0x1 << 7)
#define LDB_DATA_WIDTH_CH0_24		(0x1 << 5)

#define LDB_CH1_MODE_MASK		(0x3 << 2)
#define LDB_CH1_MODE_EN_TO_DI1		(0x3 << 2)
#define LDB_CH1_MODE_EN_TO_DI0		(0x1 << 2)
#define LDB_CH0_MODE_MASK		(0x3 << 0)
#define LDB_CH0_MODE_EN_TO_DI1		(0x3 << 0)
#define LDB_CH0_MODE_EN_TO_DI0		(0x1 << 0)

#define LDB_SPLIT_MODE_EN		(0x1 << 4)

#define INVALID_BUS_REG			(~0UL)

struct crtc_mux {
	enum crtc crtc;
	u32 val;
};

struct bus_mux {
	int reg;
	int shift;
	int mask;
	int crtc_mux_num;
	const struct crtc_mux *crtcs;
};

struct ldb_info {
	bool split_cap;
	bool dual_cap;
	bool ext_bgref_cap;
	bool clk_fixup;
	int ctrl_reg;
	int bus_mux_num;
	const struct bus_mux *buses;
};

struct ldb_data;

struct ldb_chan {
	struct ldb_data *ldb;
	struct fb_info *fbi;
	struct videomode vm;
	enum crtc crtc;
	int chno;
	bool is_used;
	bool online;
};

struct ldb_data {
	struct regmap *regmap;
	struct device *dev;
	struct mxc_dispdrv_handle *mddh;
	struct ldb_chan chan[2];
	int bus_mux_num;
	const struct bus_mux *buses;
	int primary_chno;
	int ctrl_reg;
	u32 ctrl;
	bool spl_mode;
	bool dual_mode;
	bool clk_fixup;
	struct clk *di_clk[4];
	struct clk *ldb_di_clk[2];
	struct clk *div_3_5_clk[2];
	struct clk *div_7_clk[2];
	struct clk *div_sel_clk[2];
};


static struct fb_videomode ldb_modedb[] = {
        {
         "LDB-WXGA", 60, 1280, 800, 14064,
         70, 80,
         8, 13,
         10, 2,
         0,
         FB_VMODE_NONINTERLACED,
         FB_MODE_IS_DETAILED,},
        {
         "LDB-XGA", 60, 1024, 768, 15385,
         220, 40,
         21, 7,
         60, 10,
         0,
         FB_VMODE_NONINTERLACED,
         FB_MODE_IS_DETAILED,},
        {
         "LDB-1080P60", 60, 1920, 1080, 7692,
         100, 40,
         30, 3,
         10, 2,
         0,
         FB_VMODE_NONINTERLACED,
         FB_MODE_IS_DETAILED,},
};
static int ldb_modedb_sz = ARRAY_SIZE(ldb_modedb);

static int g_ldb_mode = 0;
static int disable_ldb_at_no_enable = 0;
static const struct crtc_mux imx6q_lvds0_crtc_mux[] = {
	{
		.crtc = CRTC_IPU1_DI0,
		.val = IMX6Q_GPR3_LVDS0_MUX_CTL_IPU1_DI0,
	}, {
		.crtc = CRTC_IPU1_DI1,
		.val = IMX6Q_GPR3_LVDS0_MUX_CTL_IPU1_DI1,
	}, {
		.crtc = CRTC_IPU2_DI0,
		.val = IMX6Q_GPR3_LVDS0_MUX_CTL_IPU2_DI0,
	}, {
		.crtc = CRTC_IPU2_DI1,
		.val = IMX6Q_GPR3_LVDS0_MUX_CTL_IPU2_DI1,
	}
};

static const struct crtc_mux imx6q_lvds1_crtc_mux[] = {
	{
		.crtc = CRTC_IPU1_DI0,
		.val = IMX6Q_GPR3_LVDS1_MUX_CTL_IPU1_DI0,
	}, {
		.crtc = CRTC_IPU1_DI1,
		.val = IMX6Q_GPR3_LVDS1_MUX_CTL_IPU1_DI1,
	}, {
		.crtc = CRTC_IPU2_DI0,
		.val = IMX6Q_GPR3_LVDS1_MUX_CTL_IPU2_DI0,
	}, {
		.crtc = CRTC_IPU2_DI1,
		.val = IMX6Q_GPR3_LVDS1_MUX_CTL_IPU2_DI1,
	}
};

static const struct bus_mux imx6q_ldb_buses[] = {
	{
		.reg = IOMUXC_GPR3,
		.shift = 6,
		.mask = IMX6Q_GPR3_LVDS0_MUX_CTL_MASK,
		.crtc_mux_num = ARRAY_SIZE(imx6q_lvds0_crtc_mux),
		.crtcs = imx6q_lvds0_crtc_mux,
	}, {
		.reg = IOMUXC_GPR3,
		.shift = 8,
		.mask = IMX6Q_GPR3_LVDS1_MUX_CTL_MASK,
		.crtc_mux_num = ARRAY_SIZE(imx6q_lvds1_crtc_mux),
		.crtcs = imx6q_lvds1_crtc_mux,
	}
};

static const struct ldb_info imx6q_ldb_info = {
	.split_cap = true,
	.dual_cap = true,
	.ext_bgref_cap = false,
	.clk_fixup = false,
	.ctrl_reg = IOMUXC_GPR2,
	.bus_mux_num = ARRAY_SIZE(imx6q_ldb_buses),
	.buses = imx6q_ldb_buses,
};

static const struct ldb_info imx6qp_ldb_info = {
	.split_cap = true,
	.dual_cap = true,
	.ext_bgref_cap = false,
	.clk_fixup = true,
	.ctrl_reg = IOMUXC_GPR2,
	.bus_mux_num = ARRAY_SIZE(imx6q_ldb_buses),
	.buses = imx6q_ldb_buses,
};

static const struct crtc_mux imx6dl_lvds0_crtc_mux[] = {
	{
		.crtc = CRTC_IPU1_DI0,
		.val = IMX6DL_GPR3_LVDS0_MUX_CTL_IPU1_DI0,
	}, {
		.crtc = CRTC_IPU1_DI1,
		.val = IMX6DL_GPR3_LVDS0_MUX_CTL_IPU1_DI1,
	}, {
		.crtc = CRTC_LCDIF1,
		.val = IMX6DL_GPR3_LVDS0_MUX_CTL_LCDIF,
	}
};

static const struct crtc_mux imx6dl_lvds1_crtc_mux[] = {
	{
		.crtc = CRTC_IPU1_DI0,
		.val = IMX6DL_GPR3_LVDS1_MUX_CTL_IPU1_DI0,
	}, {
		.crtc = CRTC_IPU1_DI1,
		.val = IMX6DL_GPR3_LVDS1_MUX_CTL_IPU1_DI1,
	}, {
		.crtc = CRTC_LCDIF1,
		.val = IMX6DL_GPR3_LVDS1_MUX_CTL_LCDIF,
	}
};

static const struct bus_mux imx6dl_ldb_buses[] = {
	{
		.reg = IOMUXC_GPR3,
		.shift = 6,
		.mask = IMX6DL_GPR3_LVDS0_MUX_CTL_MASK,
		.crtc_mux_num = ARRAY_SIZE(imx6dl_lvds0_crtc_mux),
		.crtcs = imx6dl_lvds0_crtc_mux,
	}, {
		.reg = IOMUXC_GPR3,
		.shift = 8,
		.mask = IMX6DL_GPR3_LVDS1_MUX_CTL_MASK,
		.crtc_mux_num = ARRAY_SIZE(imx6dl_lvds1_crtc_mux),
		.crtcs = imx6dl_lvds1_crtc_mux,
	}
};

static const struct ldb_info imx6dl_ldb_info = {
	.split_cap = true,
	.dual_cap = true,
	.ext_bgref_cap = false,
	.clk_fixup = false,
	.ctrl_reg = IOMUXC_GPR2,
	.bus_mux_num = ARRAY_SIZE(imx6dl_ldb_buses),
	.buses = imx6dl_ldb_buses,
};

static const struct crtc_mux imx6sx_lvds_crtc_mux[] = {
	{
		.crtc = CRTC_LCDIF1,
		.val = IMX6SX_GPR5_DISP_MUX_LDB_CTRL_LCDIF1,
	}, {
		.crtc = CRTC_LCDIF2,
		.val = IMX6SX_GPR5_DISP_MUX_LDB_CTRL_LCDIF2,
	}
};

static const struct bus_mux imx6sx_ldb_buses[] = {
	{
		.reg = IOMUXC_GPR5,
		.shift = 3,
		.mask = IMX6SX_GPR5_DISP_MUX_LDB_CTRL_MASK,
		.crtc_mux_num = ARRAY_SIZE(imx6sx_lvds_crtc_mux),
		.crtcs = imx6sx_lvds_crtc_mux,
	}
};

static const struct ldb_info imx6sx_ldb_info = {
	.split_cap = false,
	.dual_cap = false,
	.ext_bgref_cap = false,
	.clk_fixup = false,
	.ctrl_reg = IOMUXC_GPR6,
	.bus_mux_num = ARRAY_SIZE(imx6sx_ldb_buses),
	.buses = imx6sx_ldb_buses,
};

static const struct crtc_mux imx53_lvds0_crtc_mux[] = {
	{ .crtc = CRTC_IPU1_DI0, },
};

static const struct crtc_mux imx53_lvds1_crtc_mux[] = {
	{ .crtc = CRTC_IPU1_DI1, }
};

static const struct bus_mux imx53_ldb_buses[] = {
	{
		.reg = INVALID_BUS_REG,
		.crtc_mux_num = ARRAY_SIZE(imx53_lvds0_crtc_mux),
		.crtcs = imx53_lvds0_crtc_mux,
	}, {
		.reg = INVALID_BUS_REG,
		.crtc_mux_num = ARRAY_SIZE(imx53_lvds1_crtc_mux),
		.crtcs = imx53_lvds1_crtc_mux,
	}
};

static const struct ldb_info imx53_ldb_info = {
	.split_cap = true,
	.dual_cap = false,
	.ext_bgref_cap = true,
	.clk_fixup = false,
	.ctrl_reg = IOMUXC_GPR2,
	.bus_mux_num = ARRAY_SIZE(imx53_ldb_buses),
	.buses = imx53_ldb_buses,
};

static const struct of_device_id ldb_dt_ids[] = {
	{ .compatible = "fsl,imx6qp-ldb", .data = &imx6qp_ldb_info, },
	{ .compatible = "fsl,imx6q-ldb", .data = &imx6q_ldb_info, },
	{ .compatible = "fsl,imx6dl-ldb", .data = &imx6dl_ldb_info, },
	{ .compatible = "fsl,imx6sx-ldb", .data = &imx6sx_ldb_info, },
	{ .compatible = "fsl,imx53-ldb", .data = &imx53_ldb_info, },
	{ /* sentinel */ }
};
MODULE_DEVICE_TABLE(of, ldb_dt_ids);

/*
 *    "ldb=spl0/1"       --      split mode on DI0/1
 *    "ldb=dul0/1"       --      dual mode on DI0/1
 *    "ldb=sin0/1"       --      single mode on LVDS0/1
 *    "ldb=sep0/1"       --      separate mode begin from LVDS0/1
 *
 *    there are two LVDS channels(LVDS0 and LVDS1) which can transfer video
 *    datas, there two channels can be used as split/dual/single/separate mode.
 *
 *    split mode means display data from DI0 or DI1 will send to both channels
 *    LVDS0+LVDS1.
 *    dual mode means display data from DI0 or DI1 will be duplicated on LVDS0
 *    and LVDS1, it said, LVDS0 and LVDS1 has the same content.
 *    single mode means only work for DI0/DI1->LVDS0 or DI0/DI1->LVDS1.
 *    separate mode means you can make DI0/DI1->LVDS0 and DI0/DI1->LVDS1 work
 *    at the same time.
 */

#define LDB_SPL_DI0     1
#define LDB_SPL_DI1     2
#define LDB_DUL_DI0     3
#define LDB_DUL_DI1     4
#define LDB_SIN0        5
#define LDB_SIN1        6
#define LDB_SEP0        7
#define LDB_SEP1        8

static int __init ldb_setup_mode(char *options)
{
        if (!strcmp(options, "spl0"))
                g_ldb_mode = LDB_SPL_DI0;
        else if (!strcmp(options, "spl1"))
                g_ldb_mode = LDB_SPL_DI1;
        else if (!strcmp(options, "dul0"))
                g_ldb_mode = LDB_DUL_DI0;
        else if (!strcmp(options, "dul1"))
                g_ldb_mode = LDB_DUL_DI1;
        else if (!strcmp(options, "sin0"))
                g_ldb_mode = LDB_SIN0;
        else if (!strcmp(options, "sin1"))
                g_ldb_mode = LDB_SIN1;
        else if (!strcmp(options, "sep0"))
                g_ldb_mode = LDB_SEP0;
        else if (!strcmp(options, "sep1"))
                g_ldb_mode = LDB_SEP1;

        return 1;
}
__setup("ldb=", ldb_setup_mode);

static int ldb_init(struct mxc_dispdrv_handle *mddh,
		    struct mxc_dispdrv_setting *setting)
{
	struct ldb_data *ldb = mxc_dispdrv_getdata(mddh);
	struct device *dev = ldb->dev;
	struct fb_info *fbi = setting->fbi;
	struct ldb_chan *chan;
	struct fb_videomode fb_vm;
	int chno;
	int ret;
	/* select ldb mode from cmdline ,
		set the chanels online and primary port,
		set the ldb mode of spl or dul*/
	if(g_ldb_mode == LDB_SPL_DI0) {
		ldb->primary_chno = 0;
		ldb->chan[1].online = false;
		ldb->spl_mode = true;
		ldb->ctrl |= LDB_SPLIT_MODE_EN;
	} else if(g_ldb_mode == LDB_SPL_DI1) {
		ldb->primary_chno = 1;
                ldb->chan[0].online = false;
		ldb->spl_mode = true;
		ldb->ctrl |= LDB_SPLIT_MODE_EN;
	} else if(g_ldb_mode == LDB_DUL_DI0) {
		ldb->primary_chno = 0;
                ldb->chan[1].online = false;
		ldb->dual_mode = true;
	} else if(g_ldb_mode == LDB_DUL_DI1) {
		ldb->primary_chno = 1;
                ldb->chan[0].online = false;
		ldb->dual_mode = true;
	} else if(g_ldb_mode == LDB_SIN0) {
		ldb->primary_chno = 0;
                ldb->chan[1].online = false;
	} else if(g_ldb_mode == LDB_SIN1) {
		ldb->primary_chno = 1;
                ldb->chan[0].online = false;
	} else if(g_ldb_mode == LDB_SEP0) {
		ldb->primary_chno = 0;
	} else if(g_ldb_mode == LDB_SEP1) {
		ldb->primary_chno = 1;
	}

	chno = ldb->chan[ldb->primary_chno].is_used ?
		!ldb->primary_chno : ldb->primary_chno;

	chan = &ldb->chan[chno];

	if (chan->is_used) {
		dev_err(dev, "LVDS channel%d is already used\n", chno);
		return -EBUSY;
	}
	if (!chan->online) {
		dev_err(dev, "LVDS channel%d is not online\n", chno);
		return -ENODEV;
	}
	if(setting->if_fmt == IPU_PIX_FMT_RGB24) { 
		if(chno == 0)
			ldb->ctrl |= LDB_DATA_WIDTH_CH0_24;
		else 
			ldb->ctrl |= LDB_DATA_WIDTH_CH1_24;
	} else {
		if(chno == 0)
                        ldb->ctrl &= ~LDB_DATA_WIDTH_CH0_24;
                else
                        ldb->ctrl &= ~LDB_DATA_WIDTH_CH1_24;
	}

	if(ldb->dual_mode || ldb->spl_mode) {
		if(setting->if_fmt == IPU_PIX_FMT_RGB24) 
                        ldb->ctrl |= LDB_DATA_WIDTH_CH0_24 | LDB_DATA_WIDTH_CH1_24;
                else
                        ldb->ctrl &= ~(LDB_DATA_WIDTH_CH0_24 | LDB_DATA_WIDTH_CH1_24);
	}

	chan->is_used = true;

	chan->fbi = fbi;
	ret = fb_find_mode(&setting->fbi->var, setting->fbi, setting->dft_mode_str,
                                ldb_modedb, ldb_modedb_sz, NULL, setting->default_bpp);
        if (!ret) {
		fb_videomode_from_videomode(&chan->vm, &fb_vm);
		fb_videomode_to_var(&fbi->var, &fb_vm);
        } 


	setting->crtc = chan->crtc;

	return 0;
}

static int get_di_clk_id(struct ldb_chan chan, int *id)
{
	struct ldb_data *ldb = chan.ldb;
	int i = 0, chno = chan.chno, mask, shift;
	enum crtc crtc;
	u32 val;

	/* no pre-muxing, such as mx53 */
	if (ldb->buses[chno].reg == INVALID_BUS_REG) {
		*id = chno;
		return 0;
	}

	for (; i < ldb->buses[chno].crtc_mux_num; i++) {
		crtc = ldb->buses[chno].crtcs[i].crtc;
		val  = ldb->buses[chno].crtcs[i].val;
		mask = ldb->buses[chno].mask;
		shift = ldb->buses[chno].shift;
		if (chan.crtc == crtc) {
			*id = (val & mask) >> shift;
			return 0;
		}
	}

	return -EINVAL;
}

static int get_mux_val(struct bus_mux bus_mux, enum crtc crtc,
		       u32 *mux_val)
{
	int i = 0;

	for (; i < bus_mux.crtc_mux_num; i++)
		if (bus_mux.crtcs[i].crtc == crtc) {
			*mux_val = bus_mux.crtcs[i].val;
			return 0;
		}

	return -EINVAL;
}

static int find_ldb_chno(struct ldb_data *ldb,
			 struct fb_info *fbi, int *chno)
{
	struct device *dev = ldb->dev;
	int i = 0;

	for (; i < 2; i++)
		if (ldb->chan[i].fbi == fbi) {
			*chno = ldb->chan[i].chno;
			return 0;
		}
	dev_err(dev, "failed to find channel number\n");
	return -EINVAL;
}

static void ldb_disable(struct mxc_dispdrv_handle *mddh,
			struct fb_info *fbi);

static int ldb_setup(struct mxc_dispdrv_handle *mddh,
		     struct fb_info *fbi)
{
	struct ldb_data *ldb = mxc_dispdrv_getdata(mddh);
	struct ldb_chan chan;
	struct device *dev = ldb->dev;
	struct clk *ldb_di_parent, *ldb_di_sel, *ldb_di_sel_parent;
	struct clk *other_ldb_di_sel = NULL;
	struct bus_mux bus_mux;
	int ret = 0, id = 0, chno, other_chno;
	unsigned long serial_clk;
	u32 mux_val;

	ret = find_ldb_chno(ldb, fbi, &chno);
	if (ret < 0)
		return ret;

	other_chno = chno ? 0 : 1;

	chan = ldb->chan[chno];

	bus_mux = ldb->buses[chno];

	ret = get_di_clk_id(chan, &id);
	if (ret < 0) {
		dev_err(dev, "failed to get ch%d di clk id\n",
			chan.chno);
		return ret;
	}

	ret = get_mux_val(bus_mux, chan.crtc, &mux_val);
	if (ret < 0) {
		dev_err(dev, "failed to get ch%d mux val\n",
			chan.chno);
		return ret;
	}

	if (ldb->clk_fixup) {
		/*
		 * ldb_di_sel_parent(plls) -> ldb_di_sel -> ldb_di[chno] ->
		 *
		 *     -> div_3_5[chno] ->
		 * -> |                   |-> div_sel[chno] -> di[id]
		 *     ->  div_7[chno] ->
		 */
		clk_set_parent(ldb->di_clk[id], ldb->div_sel_clk[chno]);
	} else {
		/*
		 * ldb_di_sel_parent(plls) -> ldb_di_sel ->
		 *
		 *     -> div_3_5[chno] ->
		 * -> |                   |-> div_sel[chno] ->
		 *     ->  div_7[chno] ->
		 *
		 * -> ldb_di[chno] -> di[id]
		 */
		clk_set_parent(ldb->di_clk[id], ldb->ldb_di_clk[chno]);
	}
	ldb_di_parent = ldb->spl_mode ? ldb->div_3_5_clk[chno] :
			ldb->div_7_clk[chno];
	clk_set_parent(ldb->div_sel_clk[chno], ldb_di_parent);
	ldb_di_sel = clk_get_parent(ldb_di_parent);
	ldb_di_sel_parent = clk_get_parent(ldb_di_sel);
	serial_clk = ldb->spl_mode ? PICOS2KHZ(fbi->var.pixclock) * 7 / 2 * 1000 :
			 PICOS2KHZ(fbi->var.pixclock) * 7 * 1000;
	clk_set_rate(ldb_di_sel_parent, serial_clk);

	/*
	 * split mode or dual mode:
	 * clock tree for the other channel
	 */
	if (ldb->spl_mode) {
		clk_set_parent(ldb->div_sel_clk[other_chno],
			       ldb->div_3_5_clk[other_chno]);
		other_ldb_di_sel =
			clk_get_parent(ldb->div_3_5_clk[other_chno]);;
	}

	if (ldb->dual_mode) {
		clk_set_parent(ldb->div_sel_clk[other_chno],
			       ldb->div_7_clk[other_chno]);
		other_ldb_di_sel =
			clk_get_parent(ldb->div_7_clk[other_chno]);;
	}

	if (ldb->spl_mode || ldb->dual_mode)
		clk_set_parent(other_ldb_di_sel, ldb_di_sel_parent);

	if (!(fbi->var.sync & FB_SYNC_VERT_HIGH_ACT)) {
		if (ldb->spl_mode && bus_mux.reg == INVALID_BUS_REG)
			/* no pre-muxing, such as mx53 */
			ldb->ctrl |= (id == 0 ? LDB_DI0_VS_POL_ACT_LOW :
					LDB_DI1_VS_POL_ACT_LOW);
		else
			ldb->ctrl |= (chno == 0 ? LDB_DI0_VS_POL_ACT_LOW :
					LDB_DI1_VS_POL_ACT_LOW);
	}

	if (bus_mux.reg != INVALID_BUS_REG)
		regmap_update_bits(ldb->regmap, bus_mux.reg,
				   bus_mux.mask, mux_val);

	regmap_write(ldb->regmap, ldb->ctrl_reg, ldb->ctrl);

	/* disable channel for correct sequence */
	disable_ldb_at_no_enable = 1;
	ldb_disable(mddh, fbi);

	return ret;
}

static int ldb_enable(struct mxc_dispdrv_handle *mddh,
		      struct fb_info *fbi)
{
	struct ldb_data *ldb = mxc_dispdrv_getdata(mddh);
	struct ldb_chan chan;
	struct device *dev = ldb->dev;
	struct bus_mux bus_mux;
	int ret = 0, id = 0, chno, other_chno;

	ret = find_ldb_chno(ldb, fbi, &chno);
	if (ret < 0)
		return ret;

	chan = ldb->chan[chno];

	bus_mux = ldb->buses[chno];

	if (ldb->spl_mode || ldb->dual_mode) {
		other_chno = chno ? 0 : 1;
		clk_prepare_enable(ldb->ldb_di_clk[other_chno]);
	}

	if ((ldb->spl_mode || ldb->dual_mode) &&
	    bus_mux.reg == INVALID_BUS_REG) {
		/* no pre-muxing, such as mx53 */
		ret = get_di_clk_id(chan, &id);
		if (ret < 0) {
			dev_err(dev, "failed to get ch%d di clk id\n",
				chan.chno);
			return ret;
		}

		ldb->ctrl |= id ?
			(LDB_CH0_MODE_EN_TO_DI1 | LDB_CH1_MODE_EN_TO_DI1) :
			(LDB_CH0_MODE_EN_TO_DI0 | LDB_CH1_MODE_EN_TO_DI0);
	} else {
		if (ldb->spl_mode || ldb->dual_mode)
			ldb->ctrl |= LDB_CH0_MODE_EN_TO_DI0 |
				     LDB_CH1_MODE_EN_TO_DI0;
		else
			ldb->ctrl |= chno ? LDB_CH1_MODE_EN_TO_DI1 :
					    LDB_CH0_MODE_EN_TO_DI0;
	}

	regmap_write(ldb->regmap, ldb->ctrl_reg, ldb->ctrl);
	return 0;
}

static void ldb_disable(struct mxc_dispdrv_handle *mddh,
		       struct fb_info *fbi)
{
	struct ldb_data *ldb = mxc_dispdrv_getdata(mddh);
	int ret, chno, other_chno;

	ret = find_ldb_chno(ldb, fbi, &chno);
	if (ret < 0)
		return;

	if (ldb->spl_mode || ldb->dual_mode) {
		ldb->ctrl &= ~(LDB_CH1_MODE_MASK | LDB_CH0_MODE_MASK);
		other_chno = chno ? 0 : 1;
		if(disable_ldb_at_no_enable == 0) {
			clk_disable_unprepare(ldb->ldb_di_clk[other_chno]);
			disable_ldb_at_no_enable = 0;
		}
	} else {
		ldb->ctrl &= ~(chno ? LDB_CH1_MODE_MASK :
				      LDB_CH0_MODE_MASK);
	}

	regmap_write(ldb->regmap, ldb->ctrl_reg, ldb->ctrl);
	return;
}

static struct mxc_dispdrv_driver ldb_drv = {
	.name		= DRIVER_NAME,
	.init		= ldb_init,
	.setup		= ldb_setup,
	.enable		= ldb_enable,
	.disable	= ldb_disable
};

enum {
	LVDS_BIT_MAP_SPWG,
	LVDS_BIT_MAP_JEIDA,
};

static const char *ldb_bit_mappings[] = {
	[LVDS_BIT_MAP_SPWG] = "spwg",
	[LVDS_BIT_MAP_JEIDA] = "jeida",
};

static int of_get_data_mapping(struct device_node *np)
{
	const char *bm;
	int ret, i;

	ret = of_property_read_string(np, "fsl,data-mapping", &bm);
	if (ret < 0)
		return ret;

	for (i = 0; i < ARRAY_SIZE(ldb_bit_mappings); i++)
		if (!strcasecmp(bm, ldb_bit_mappings[i]))
			return i;

	return -EINVAL;
}

static const char *ldb_crtc_mappings[] = {
	[CRTC_IPU_DI0] = "ipu-di0",
	[CRTC_IPU_DI1] = "ipu-di1",
	[CRTC_IPU1_DI0] = "ipu1-di0",
	[CRTC_IPU1_DI1] = "ipu1-di1",
	[CRTC_IPU2_DI0] = "ipu2-di0",
	[CRTC_IPU2_DI1] = "ipu2-di1",
	[CRTC_LCDIF] = "lcdif",
	[CRTC_LCDIF1] = "lcdif1",
	[CRTC_LCDIF2] = "lcdif2",
};

static enum crtc of_get_crtc_mapping(struct device_node *np)
{
	const char *cm;
	enum crtc i;
	int ret;

	ret = of_property_read_string(np, "crtc", &cm);
	if (ret < 0)
		return ret;

	for (i = 0; i < ARRAY_SIZE(ldb_crtc_mappings); i++)
		if (!strcasecmp(cm, ldb_crtc_mappings[i])) {
			switch (i) {
			case CRTC_IPU_DI0:
				i = CRTC_IPU1_DI0;
				break;
			case CRTC_IPU_DI1:
				i = CRTC_IPU1_DI1;
				break;
			case CRTC_LCDIF:
				i = CRTC_LCDIF1;
				break;
			default:
				break;
			}
			return i;
		}

	return -EINVAL;
}

static int mux_count(struct ldb_data *ldb)
{
	int i, j, count = 0;
	bool should_count[CRTC_MAX];
	enum crtc crtc;

	for (i = 0; i < CRTC_MAX; i++)
		should_count[i] = true;

	for (i = 0; i < ldb->bus_mux_num; i++) {
		for (j = 0; j < ldb->buses[i].crtc_mux_num; j++) {
			crtc = ldb->buses[i].crtcs[j].crtc;
			if (should_count[crtc]) {
				count++;
				should_count[crtc] = false;
			}
		}
	}

	return count;
}

static bool is_valid_crtc(struct ldb_data *ldb, enum crtc crtc,
			  int chno)
{
	int i = 0;

	if (chno > ldb->bus_mux_num - 1)
		return false;

	for (; i < ldb->buses[chno].crtc_mux_num; i++)
		if (ldb->buses[chno].crtcs[i].crtc == crtc)
			return true;

	return false;
}

static int ldb_probe(struct platform_device *pdev)
{
	struct device *dev = &pdev->dev;
	const struct of_device_id *of_id =
			of_match_device(ldb_dt_ids, dev);
	const struct ldb_info *ldb_info =
			(const struct ldb_info *)of_id->data;
	struct device_node *np = dev->of_node, *child;
	struct ldb_data *ldb;
	bool ext_ref;
	int i, data_width, mapping, child_count = 0;
	char clkname[16];

	ldb = devm_kzalloc(dev, sizeof(*ldb), GFP_KERNEL);
	if (!ldb)
		return -ENOMEM;

	ldb->regmap = syscon_regmap_lookup_by_phandle(np, "gpr");
	if (IS_ERR(ldb->regmap)) {
		dev_err(dev, "failed to get parent regmap\n");
		return PTR_ERR(ldb->regmap);
	}

	ldb->dev = dev;
	ldb->bus_mux_num = ldb_info->bus_mux_num;
	ldb->buses = ldb_info->buses;
	ldb->ctrl_reg = ldb_info->ctrl_reg;
	ldb->clk_fixup = ldb_info->clk_fixup;
	ldb->primary_chno = -1;

	ext_ref = of_property_read_bool(np, "ext-ref");
	if (!ext_ref && ldb_info->ext_bgref_cap)
		ldb->ctrl |= LDB_BGREF_RMODE_INT;

	ldb->spl_mode = of_property_read_bool(np, "split-mode");
	if (ldb->spl_mode) {
		if (ldb_info->split_cap) {
			ldb->ctrl |= LDB_SPLIT_MODE_EN;
			dev_info(dev, "split mode\n");
		} else {
			dev_err(dev, "cannot support split mode\n");
			return -EINVAL;
		}
	}

	ldb->dual_mode = of_property_read_bool(np, "dual-mode");
	if (ldb->dual_mode) {
		if (ldb_info->dual_cap) {
			dev_info(dev, "dual mode\n");
		} else {
			dev_err(dev, "cannot support dual mode\n");
			return -EINVAL;
		}
	}

	if (ldb->dual_mode && ldb->spl_mode) {
		dev_err(dev, "cannot support dual mode and split mode "
				"simultaneously\n");
		return -EINVAL;
	}

	for (i = 0; i < mux_count(ldb); i++) {
		sprintf(clkname, "di%d_sel", i);
		ldb->di_clk[i] = devm_clk_get(dev, clkname);
		if (IS_ERR(ldb->di_clk[i])) {
			dev_err(dev, "failed to get clk %s\n", clkname);
			return PTR_ERR(ldb->di_clk[i]);
		}
	}

	for_each_child_of_node(np, child) {
		struct ldb_chan *chan;
		enum crtc crtc;
		bool is_primary;
		int ret;

		ret = of_property_read_u32(child, "reg", &i);
		if (ret || i < 0 || i > 1 || i >= ldb->bus_mux_num) {
			dev_err(dev, "wrong LVDS channel number\n");
			return -EINVAL;
		}

		if ((ldb->spl_mode || ldb->dual_mode) && i > 0) {
			dev_warn(dev, "split mode or dual mode, ignoring "
				"second output\n");
			continue;
		}

		if (!of_device_is_available(child))
			continue;

		if (++child_count > ldb->bus_mux_num) {
			dev_err(dev, "too many LVDS channels\n");
			return -EINVAL;
		}

		chan = &ldb->chan[i];
		chan->chno = i;
		chan->ldb = ldb;
		chan->online = true;

		is_primary = of_property_read_bool(child, "primary");

		if (ldb->bus_mux_num == 1 || (ldb->primary_chno == -1 &&
		    (is_primary || ldb->spl_mode || ldb->dual_mode)))
			ldb->primary_chno = chan->chno;

		ret = of_property_read_u32(child, "fsl,data-width",
					   &data_width);
		if (ret || (data_width != 18 && data_width != 24)) {
			dev_err(dev, "data width not specified or invalid\n");
			return -EINVAL;
		}

		mapping = of_get_data_mapping(child);
		switch (mapping) {
		case LVDS_BIT_MAP_SPWG:
			if (data_width == 24) {
				if (i == 0 || ldb->spl_mode || ldb->dual_mode)
					ldb->ctrl |= LDB_DATA_WIDTH_CH0_24;
				if (i == 1 || ldb->spl_mode || ldb->dual_mode)
					ldb->ctrl |= LDB_DATA_WIDTH_CH1_24;
			}
			break;
		case LVDS_BIT_MAP_JEIDA:
			if (data_width == 18) {
				dev_err(dev, "JEIDA only support 24bit\n");
				return -EINVAL;
			}
			if (i == 0 || ldb->spl_mode || ldb->dual_mode)
				ldb->ctrl |= LDB_DATA_WIDTH_CH0_24 |
					     LDB_BIT_MAP_CH0_JEIDA;
			if (i == 1 || ldb->spl_mode || ldb->dual_mode)
				ldb->ctrl |= LDB_DATA_WIDTH_CH1_24 |
					     LDB_BIT_MAP_CH1_JEIDA;
			break;
		default:
			dev_err(dev, "data mapping not specified or invalid\n");
			return -EINVAL;
		}

		crtc = of_get_crtc_mapping(child);
		if (is_valid_crtc(ldb, crtc, chan->chno)) {
			ldb->chan[i].crtc = crtc;
		} else {
			dev_err(dev, "crtc not specified or invalid\n");
			return -EINVAL;
		}

		ret = of_get_videomode(child, &chan->vm, 0);
		if (ret)
			return -EINVAL;

		sprintf(clkname, "ldb_di%d", i);
		ldb->ldb_di_clk[i] = devm_clk_get(dev, clkname);
		if (IS_ERR(ldb->ldb_di_clk[i])) {
			dev_err(dev, "failed to get clk %s\n", clkname);
			return PTR_ERR(ldb->ldb_di_clk[i]);
		}

		sprintf(clkname, "ldb_di%d_div_3_5", i);
		ldb->div_3_5_clk[i] = devm_clk_get(dev, clkname);
		if (IS_ERR(ldb->div_3_5_clk[i])) {
			dev_err(dev, "failed to get clk %s\n", clkname);
			return PTR_ERR(ldb->div_3_5_clk[i]);
		}

		sprintf(clkname, "ldb_di%d_div_7", i);
		ldb->div_7_clk[i] = devm_clk_get(dev, clkname);
		if (IS_ERR(ldb->div_7_clk[i])) {
			dev_err(dev, "failed to get clk %s\n", clkname);
			return PTR_ERR(ldb->div_7_clk[i]);
		}

		sprintf(clkname, "ldb_di%d_div_sel", i);
		ldb->div_sel_clk[i] = devm_clk_get(dev, clkname);
		if (IS_ERR(ldb->div_sel_clk[i])) {
			dev_err(dev, "failed to get clk %s\n", clkname);
			return PTR_ERR(ldb->div_sel_clk[i]);
		}
	}

	if (child_count == 0) {
		dev_err(dev, "failed to find valid LVDS channel\n");
		return -EINVAL;
	}

	if (ldb->primary_chno == -1) {
		dev_err(dev, "failed to know primary channel\n");
		return -EINVAL;
	}

	ldb->mddh = mxc_dispdrv_register(&ldb_drv);
	mxc_dispdrv_setdata(ldb->mddh, ldb);
	dev_set_drvdata(&pdev->dev, ldb);

	return 0;
}

static int ldb_remove(struct platform_device *pdev)
{
	struct ldb_data *ldb = dev_get_drvdata(&pdev->dev);

	mxc_dispdrv_puthandle(ldb->mddh);
	mxc_dispdrv_unregister(ldb->mddh);
	return 0;
}

static struct platform_driver ldb_driver = {
	.driver = {
		.name = DRIVER_NAME,
		.of_match_table	= ldb_dt_ids,
	},
	.probe = ldb_probe,
	.remove = ldb_remove,
};

module_platform_driver(ldb_driver);

MODULE_AUTHOR("Freescale Semiconductor, Inc.");
MODULE_DESCRIPTION("LDB driver");
MODULE_LICENSE("GPL");
MODULE_ALIAS("platform:" DRIVER_NAME);
