/*
 * Copyright (C) 2013-2014 Allwinner Tech Co., Ltd
 * Author: Sugar <shuge@allwinnertech.com>
 *
 * Copyright (C) 2014 Maxime Ripard
 * Maxime Ripard <maxime.ripard@free-electrons.com>
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 */

#include <linux/clk.h>
#include <linux/delay.h>
#include <linux/dmaengine.h>
#include <linux/dmapool.h>
#include <linux/interrupt.h>
#include <linux/module.h>
#include <linux/of_dma.h>
#include <linux/of_device.h>
#include <linux/platform_device.h>
#include <linux/reset.h>
#include <linux/slab.h>
#include <linux/types.h>

#include "scheduled-dma.h"
#include "virt-dma.h"

/*
 * Common registers
 */
#define DMA_IRQ_EN(x)		((x) * 0x04)
#define DMA_IRQ_HALF			BIT(0)
#define DMA_IRQ_PKG			BIT(1)
#define DMA_IRQ_QUEUE			BIT(2)

#define DMA_IRQ_CHAN_NR			8
#define DMA_IRQ_CHAN_WIDTH		4


#define DMA_IRQ_STAT(x)		((x) * 0x04 + 0x10)

#define DMA_STAT		0x30

/*
 * sun8i specific registers
 */
#define SUN8I_DMA_GATE		0x20
#define SUN8I_DMA_GATE_ENABLE	0x4

/*
 * Channels specific registers
 */
#define DMA_CHAN_BASE(chan)	(0x100 + (chan) * 0x40)

#define DMA_CHAN_ENABLE(chan)	(DMA_CHAN_BASE(chan) + 0x00)
#define DMA_CHAN_ENABLE_START		BIT(0)
#define DMA_CHAN_ENABLE_STOP		0

#define DMA_CHAN_PAUSE(chan)	(DMA_CHAN_BASE(chan) + 0x04)
#define DMA_CHAN_PAUSE_PAUSE		BIT(1)
#define DMA_CHAN_PAUSE_RESUME		0

#define DMA_CHAN_LLI_ADDR(chan)	(DMA_CHAN_BASE(chan) + 0x08)

#define DMA_CHAN_CUR_CFG(chan)	(DMA_CHAN_BASE(chan) + 0x0c)
#define DMA_CHAN_CFG_SRC_DRQ(x)		((x) & 0x1f)
#define DMA_CHAN_CFG_SRC_IO_MODE	BIT(5)
#define DMA_CHAN_CFG_SRC_LINEAR_MODE	(0 << 5)
#define DMA_CHAN_CFG_SRC_BURST(x)	(((x) & 0x3) << 7)
#define DMA_CHAN_CFG_SRC_WIDTH(x)	(((x) & 0x3) << 9)

#define DMA_CHAN_CFG_DST_DRQ(x)		(DMA_CHAN_CFG_SRC_DRQ(x) << 16)
#define DMA_CHAN_CFG_DST_IO_MODE	(DMA_CHAN_CFG_SRC_IO_MODE << 16)
#define DMA_CHAN_CFG_DST_LINEAR_MODE	(DMA_CHAN_CFG_SRC_LINEAR_MODE << 16)
#define DMA_CHAN_CFG_DST_BURST(x)	(DMA_CHAN_CFG_SRC_BURST(x) << 16)
#define DMA_CHAN_CFG_DST_WIDTH(x)	(DMA_CHAN_CFG_SRC_WIDTH(x) << 16)

#define DMA_CHAN_CUR_SRC(chan)	(DMA_CHAN_BASE(chan) + 0x10)

#define DMA_CHAN_CUR_DST(chan)	(DMA_CHAN_BASE(chan) + 0x14)

#define DMA_CHAN_CUR_CNT(chan)	(DMA_CHAN_BASE(chan) + 0x18)

#define DMA_CHAN_CUR_PARA(chan)	(DMA_CHAN_BASE(chan) + 0x1c)


/*
 * Various hardware related defines
 */
#define LLI_LAST_ITEM	0xfffff800
#define NORMAL_WAIT	8
#define DRQ_SDRAM	1

/*
 * Hardware channels / ports representation
 *
 * The hardware is used in several SoCs, with differing numbers
 * of channels and endpoints. This structure ties those numbers
 * to a certain compatible string.
 */
struct sun6i_dma_config {
	u32 nr_max_channels;
	u32 nr_max_requests;
	u32 nr_max_vchans;
};

/*
 * Hardware representation of the LLI
 *
 * The hardware will be fed the physical address of this structure,
 * and read its content in order to start the transfer.
 */
struct sun6i_dma_lli {
	u32			cfg;
	u32			src;
	u32			dst;
	u32			len;
	u32			para;
	u32			p_lli_next;

	/*
	 * This field is not used by the DMA controller, but will be
	 * used by the CPU to go through the list (mostly for dumping
	 * or freeing it).
	 */
	struct sun6i_dma_lli	*v_lli_next;
};

struct sun6i_dma_dev {
	void __iomem		*base;
	struct clk		*clk;
	int			irq;
	struct reset_control	*rstc;
	const struct sun6i_dma_config *cfg;
};

static inline s8 convert_burst(u32 maxburst)
{
	switch (maxburst) {
	case 1:
		return 0;
	case 8:
		return 2;
	default:
		return -EINVAL;
	}
}

static inline s8 convert_buswidth(enum dma_slave_buswidth addr_width)
{
	if ((addr_width < DMA_SLAVE_BUSWIDTH_1_BYTE) ||
	    (addr_width > DMA_SLAVE_BUSWIDTH_4_BYTES))
		return -EINVAL;

	return addr_width >> 1;
}

static void *sun6i_dma_lli_queue(void *prev_v_lli,
				 void *v_lli,
				 dma_addr_t p_lli)
{
	struct sun6i_dma_lli *prev = (struct sun6i_dma_lli*)prev_v_lli;
	struct sun6i_dma_lli *next = (struct sun6i_dma_lli*)v_lli;

	if (prev) {
		prev->p_lli_next = p_lli;
		prev->v_lli_next = v_lli;
	}

	next->p_lli_next = LLI_LAST_ITEM;
	next->v_lli_next = NULL;

	return next;
}

static inline int sun6i_dma_cfg_lli(struct sun6i_dma_lli *lli,
				    dma_addr_t src,
				    dma_addr_t dst, u32 len,
				    struct dma_slave_config *config)
{
	u8 src_width, dst_width, src_burst, dst_burst;

	if (config) {
		src_burst = convert_burst(config->src_maxburst);
		if (src_burst)
			return src_burst;

		dst_burst = convert_burst(config->dst_maxburst);
		if (dst_burst)
			return dst_burst;

		src_width = convert_buswidth(config->src_addr_width);
		if (src_width)
			return src_width;

		dst_width = convert_buswidth(config->dst_addr_width);
		if (dst_width)
			return dst_width;

		lli->cfg = DMA_CHAN_CFG_SRC_BURST(src_burst) |
			DMA_CHAN_CFG_SRC_WIDTH(src_width) |
			DMA_CHAN_CFG_DST_BURST(dst_burst) |
			DMA_CHAN_CFG_DST_WIDTH(dst_width);
	}

	lli->src = src;
	lli->dst = dst;
	lli->len = len;
	lli->para = NORMAL_WAIT;

	return 0;
}

static int sun6i_dma_channel_start(struct sdma_channel *schan,
				   struct sdma_desc *sdesc)
{
	struct sun6i_dma_dev *sdc = schan->private;
	u32 irq_val, irq_reg, irq_offset;

	irq_reg = schan->index / DMA_IRQ_CHAN_NR;
	irq_offset = schan->index % DMA_IRQ_CHAN_NR;

	irq_val = readl(sdc->base + DMA_IRQ_EN(irq_offset));
	irq_val |= DMA_IRQ_QUEUE << (irq_offset * DMA_IRQ_CHAN_WIDTH);
	writel(irq_val, sdc->base + DMA_IRQ_EN(irq_offset));

	writel(sdesc->p_lli, sdc->base + DMA_CHAN_LLI_ADDR(schan->index));
	writel(DMA_CHAN_ENABLE_START, sdc->base + DMA_CHAN_ENABLE(schan->index));

	return 0;
}

static irqreturn_t sun6i_dma_interrupt(int irq, void *dev_id)
{
	struct sdma *sdma = dev_id;
	struct sun6i_dma_dev *sdev = sdma_priv(sdma);
	int i, j, ret = IRQ_NONE;
	u32 status;

	for (i = 0; i < sdev->cfg->nr_max_channels / DMA_IRQ_CHAN_NR; i++) {
		status = readl(sdev->base + DMA_IRQ_STAT(i));
		if (!status)
			continue;

		dev_dbg(sdma->ddev.dev, "DMA irq status %s: 0x%x\n",
			i ? "high" : "low", status);

		writel(status, sdev->base + DMA_IRQ_STAT(i));

		for (j = 0; (j < DMA_IRQ_CHAN_NR) && status; j++) {
			if (status & DMA_IRQ_QUEUE) {
				struct sdma_channel *schan = sdma->channels + j;
				struct sdma_desc *sdesc;

				sdesc = sdma_report(sdma, schan, SDMA_REPORT_TRANSFER);
				if (sdesc)
					sun6i_dma_channel_start(schan, sdesc);
			}

			status = status >> DMA_IRQ_CHAN_WIDTH;
		}

		ret = IRQ_HANDLED;
	}

	return ret;
}

static int sun6i_dma_lli_init(void *v_lli, void *sreq_priv,
			      enum sdma_transfer_type type,
			      enum dma_transfer_direction dir,
			      dma_addr_t src,
			      dma_addr_t dst, u32 len,
			      struct dma_slave_config *config)
{
	struct sun6i_dma_lli *lli = v_lli;
	s8 burst, width;
	int ret;

	ret = sun6i_dma_cfg_lli(lli, src, dst, len,
				config);
	if (ret)
		return ret;

	switch (type) {
	case SDMA_TRANSFER_MEMCPY:
		burst = convert_burst(8);
		width = convert_buswidth(DMA_SLAVE_BUSWIDTH_4_BYTES);

		lli->cfg |= DMA_CHAN_CFG_SRC_DRQ(DRQ_SDRAM) |
			DMA_CHAN_CFG_DST_DRQ(DRQ_SDRAM) |
			DMA_CHAN_CFG_DST_LINEAR_MODE |
			DMA_CHAN_CFG_SRC_LINEAR_MODE |
			DMA_CHAN_CFG_SRC_BURST(burst) |
			DMA_CHAN_CFG_DST_BURST(burst) |
			DMA_CHAN_CFG_SRC_WIDTH(width) |
			DMA_CHAN_CFG_DST_WIDTH(width);
		break;

	case SDMA_TRANSFER_SLAVE:
		if (dir == DMA_MEM_TO_DEV) {
			lli->cfg |= DMA_CHAN_CFG_DST_IO_MODE |
				DMA_CHAN_CFG_SRC_LINEAR_MODE |
				DMA_CHAN_CFG_DST_DRQ((u32)sreq_priv) |
				DMA_CHAN_CFG_SRC_DRQ(DRQ_SDRAM);
		} else {
			lli->cfg |= DMA_CHAN_CFG_DST_LINEAR_MODE |
				DMA_CHAN_CFG_SRC_IO_MODE |
				DMA_CHAN_CFG_DST_DRQ(DRQ_SDRAM) |
				DMA_CHAN_CFG_SRC_DRQ((u32)sreq_priv);
		}

		break;

	default:
		break;
	}

	return 0;
}

static bool sun6i_dma_lli_has_next(void *v_lli)
{
	struct sun6i_dma_lli *lli = v_lli;

	return lli->v_lli_next != NULL;
}	

static void *sun6i_dma_lli_next(void *v_lli)
{
	struct sun6i_dma_lli *lli = v_lli;

	return lli->v_lli_next;
}

static size_t sun6i_dma_lli_size(void *v_lli)
{
	struct sun6i_dma_lli *lli = v_lli;

	return lli->len;
}

static int sun6i_dma_channel_pause(struct sdma_channel *schan)
{
	struct sun6i_dma_dev *sdc = schan->private;

	writel(DMA_CHAN_PAUSE_PAUSE, sdc->base + DMA_CHAN_PAUSE(schan->index));

	return 0;
}

static int sun6i_dma_channel_resume(struct sdma_channel *schan)
{
	struct sun6i_dma_dev *sdc = schan->private;

	writel(DMA_CHAN_PAUSE_RESUME, sdc->base + DMA_CHAN_PAUSE(schan->index));

	return 0;
}

static int sun6i_dma_channel_terminate(struct sdma_channel *schan)
{
	struct sun6i_dma_dev *sdc = schan->private;

	writel(DMA_CHAN_ENABLE_STOP, sdc->base + DMA_CHAN_ENABLE(schan->index));
	writel(DMA_CHAN_PAUSE_RESUME, sdc->base + DMA_CHAN_PAUSE(schan->index));

	return 0;
}

static size_t sun6i_dma_channel_residue(struct sdma_channel *schan)
{
	struct sun6i_dma_dev *sdc = schan->private;

	return readl(sdc->base + DMA_CHAN_CUR_CNT(schan->index));
}

static struct dma_chan *sun6i_dma_of_xlate(struct of_phandle_args *dma_spec,
					   struct of_dma *ofdma)
{
	struct sdma *sdma = ofdma->of_dma_data;
	struct sun6i_dma_dev *sdev = sdma_priv(sdma);
	struct sdma_request *sreq;
	struct dma_chan *chan;
	u32 port = dma_spec->args[0];

	if (port > sdev->cfg->nr_max_requests)
		return NULL;

	chan = dma_get_any_slave_channel(&sdma->ddev);
	if (!chan)
		return NULL;

	sreq = to_sdma_request(chan);
	sreq->private = (void *)port;

	return chan;
}

static struct sdma_ops sun6i_dma_ops = {
	.channel_pause		= sun6i_dma_channel_pause,
	.channel_residue	= sun6i_dma_channel_residue,
	.channel_resume		= sun6i_dma_channel_resume,
	.channel_start		= sun6i_dma_channel_start,
	.channel_terminate	= sun6i_dma_channel_terminate,

	.lli_has_next		= sun6i_dma_lli_has_next,
	.lli_init		= sun6i_dma_lli_init,
	.lli_next		= sun6i_dma_lli_next,
	.lli_queue		= sun6i_dma_lli_queue,
	.lli_size		= sun6i_dma_lli_size,
};

/*
 * For A31:
 *
 * There's 16 physical channels that can work in parallel.
 *
 * However we have 30 different endpoints for our requests.
 *
 * Since the channels are able to handle only an unidirectional
 * transfer, we need to allocate more virtual channels so that
 * everyone can grab one channel.
 *
 * Some devices can't work in both direction (mostly because it
 * wouldn't make sense), so we have a bit fewer virtual channels than
 * 2 channels per endpoints.
 */

static struct sun6i_dma_config sun6i_a31_dma_cfg = {
	.nr_max_channels = 16,
	.nr_max_requests = 30,
	.nr_max_vchans   = 53,
};

/*
 * The A23 only has 8 physical channels, a maximum DRQ port id of 24,
 * and a total of 37 usable source and destination endpoints.
 */

static struct sun6i_dma_config sun8i_a23_dma_cfg = {
	.nr_max_channels = 8,
	.nr_max_requests = 24,
	.nr_max_vchans   = 37,
};

static struct of_device_id sun6i_dma_match[] = {
	{ .compatible = "allwinner,sun6i-a31-dma", .data = &sun6i_a31_dma_cfg },
	{ .compatible = "allwinner,sun8i-a23-dma", .data = &sun8i_a23_dma_cfg },
	{ /* sentinel */ }
};

static int sun6i_dma_probe(struct platform_device *pdev)
{
	const struct sun6i_dma_config *cfg;
	const struct of_device_id *device;
	struct sun6i_dma_dev *sdc;
	struct resource *res;
	struct sdma *sdma;
	int ret;

	device = of_match_device(sun6i_dma_match, &pdev->dev);
	if (!device)
		return -ENODEV;
	cfg = device->data;

	sdma = sdma_alloc(&pdev->dev,
			  cfg->nr_max_channels,
			  cfg->nr_max_vchans,
			  sizeof(struct sun6i_dma_lli),
			  sizeof(*sdc));
	if (IS_ERR(sdma))
		return PTR_ERR(sdma);

	sdc = sdma_priv(sdma);
	sdma_set_chan_private(sdma, sdc);
	sdc->cfg = cfg;

	res = platform_get_resource(pdev, IORESOURCE_MEM, 0);
	sdc->base = devm_ioremap_resource(&pdev->dev, res);
	if (IS_ERR(sdc->base))
		return PTR_ERR(sdc->base);

	sdc->irq = platform_get_irq(pdev, 0);
	if (sdc->irq < 0) {
		dev_err(&pdev->dev, "Cannot claim IRQ\n");
		return sdc->irq;
	}

	sdc->clk = devm_clk_get(&pdev->dev, NULL);
	if (IS_ERR(sdc->clk)) {
		dev_err(&pdev->dev, "No clock specified\n");
		return PTR_ERR(sdc->clk);
	}

	sdc->rstc = devm_reset_control_get(&pdev->dev, NULL);
	if (IS_ERR(sdc->rstc)) {
		dev_err(&pdev->dev, "No reset controller specified\n");
		return PTR_ERR(sdc->rstc);
	}

	platform_set_drvdata(pdev, sdma);

	dma_cap_set(DMA_PRIVATE, sdma->ddev.cap_mask);
	dma_cap_set(DMA_MEMCPY, sdma->ddev.cap_mask);
	dma_cap_set(DMA_SLAVE, sdma->ddev.cap_mask);

	sdma->ddev.copy_align		= 4;
	sdma->ddev.src_addr_widths	= BIT(DMA_SLAVE_BUSWIDTH_1_BYTE) |
					  BIT(DMA_SLAVE_BUSWIDTH_2_BYTES) |
					  BIT(DMA_SLAVE_BUSWIDTH_4_BYTES);
	sdma->ddev.dst_addr_widths	= BIT(DMA_SLAVE_BUSWIDTH_1_BYTE) |
					  BIT(DMA_SLAVE_BUSWIDTH_2_BYTES) |
					  BIT(DMA_SLAVE_BUSWIDTH_4_BYTES);
	sdma->ddev.directions		= BIT(DMA_DEV_TO_MEM) |
					  BIT(DMA_MEM_TO_DEV);
	sdma->ddev.residue_granularity	= DMA_RESIDUE_GRANULARITY_BURST;
	sdma->ddev.dev = &pdev->dev;

	ret = reset_control_deassert(sdc->rstc);
	if (ret) {
		dev_err(&pdev->dev, "Couldn't deassert the device from reset\n");
		goto err_free;
	}

	ret = clk_prepare_enable(sdc->clk);
	if (ret) {
		dev_err(&pdev->dev, "Couldn't enable the clock\n");
		goto err_reset_assert;
	}

	ret = devm_request_irq(&pdev->dev, sdc->irq, sun6i_dma_interrupt, 0,
			       dev_name(&pdev->dev), sdma);
	if (ret) {
		dev_err(&pdev->dev, "Cannot request IRQ\n");
		goto err_clk_disable;
	}

	ret = sdma_register(sdma, &sun6i_dma_ops);
	if (ret) {
		dev_warn(&pdev->dev, "Failed to register DMA engine device\n");
		goto err_clk_disable;
	}

	ret = of_dma_controller_register(pdev->dev.of_node, sun6i_dma_of_xlate,
					 sdma);
	if (ret) {
		dev_err(&pdev->dev, "of_dma_controller_register failed\n");
		goto err_dma_unregister;
	}

	/*
	 * sun8i variant requires us to toggle a dma gating register,
	 * as seen in Allwinner's SDK. This register is not documented
	 * in the A23 user manual.
	 */
	if (of_device_is_compatible(pdev->dev.of_node,
				    "allwinner,sun8i-a23-dma"))
		writel(SUN8I_DMA_GATE_ENABLE, sdc->base + SUN8I_DMA_GATE);

	return 0;

err_dma_unregister:
	sdma_unregister(sdma);
err_clk_disable:
	clk_disable_unprepare(sdc->clk);
err_reset_assert:
	reset_control_assert(sdc->rstc);
err_free:
	sdma_free(sdma);

	return ret;
}

static int sun6i_dma_remove(struct platform_device *pdev)
{
	struct sdma *sdma = platform_get_drvdata(pdev);
	struct sun6i_dma_dev *sdc = sdma_priv(sdma);

	of_dma_controller_free(pdev->dev.of_node);
	sdma_unregister(sdma);
	clk_disable_unprepare(sdc->clk);
	reset_control_assert(sdc->rstc);
	sdma_free(sdma);

	return 0;
}

static struct platform_driver sun6i_dma_driver = {
	.probe		= sun6i_dma_probe,
	.remove		= sun6i_dma_remove,
	.driver = {
		.name		= "sun6i-dma",
		.of_match_table	= sun6i_dma_match,
	},
};
module_platform_driver(sun6i_dma_driver);

MODULE_DESCRIPTION("Allwinner A31 DMA Controller Driver");
MODULE_AUTHOR("Sugar <shuge@allwinnertech.com>");
MODULE_AUTHOR("Maxime Ripard <maxime.ripard@free-electrons.com>");
MODULE_LICENSE("GPL");
