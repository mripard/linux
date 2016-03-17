/*
 * Allwinner SoCs AR100 CPU driver
 *
 * Copyright (C) 2016 Maxime Ripard
 *
 * Author: Maxime Ripard <maxime.ripard@free-electrons.com>
 *
 * This file is licensed under the terms of the GNU General Public
 * License version 2.  This program is licensed "as is" without any
 * warranty of any kind, whether express or implied.
 */

#include <linux/clk.h>
#include <linux/firmware.h>
#include <linux/io.h>
#include <linux/module.h>
#include <linux/of.h>
#include <linux/of_address.h>
#include <linux/of_device.h>
#include <linux/platform_device.h>

struct sun6i_ar100 {
	struct device	*dev;

	void __iomem	*cpucfg;
	void __iomem	*sram;

	struct clk	*clk;
};

static const struct of_device_id sun6i_ar100_cpucfg_dt_match[] = {
	{ .compatible = "allwinner,sun8i-a23-cpuconfig" },
	{ },
};

static const struct of_device_id sun6i_ar100_sram_dt_match[] = {
	{ .compatible = "allwinner,sun8i-a33-ar100-sram" },
	{ },
};

static void sun6i_ar100_load_firmware(const struct firmware *fw, void *context)
{
	struct sun6i_ar100 *ar100 = context;
	u32 val;

	if (!fw) {
		dev_err(ar100->dev, "AR100 firmware request failed\n");
		return;
	}
		
	/* Copy the firmware to the SRAM */
	memcpy_toio(ar100->sram, fw->data, fw->size);

	/* And bring back the AR100 */
	val = readl(ar100->cpucfg);
	writel(val | 1, ar100->cpucfg);
}

static int omap_rproc_start(struct rproc *rproc)
{
	struct sun6i_ar100 *ar100 = rproc->priv;

	/* Enable the AR100 clock */
	clk_prepare_enable(ar100->clk);

	/* And bring back the AR100 */
	val = readl(ar100->cpucfg);
	writel(val | 1, ar100->cpucfg);

	return 0;
}

static int omap_rproc_stop(struct rproc *rproc)
{
	struct sun6i_ar100 *ar100 = rproc->priv;

	/* Put back the AR100 in reset */
	val = readl(ar100->cpucfg);
	writel(val & ~1, ar100->cpucfg);

	/* And disable our clock */
	clk_disable_unprepare(ar100->clk);

	return 0;
}

static struct rproc_ops sun6i_ar100_ops = {
	.start		= sun6i_ar100_start,
	.stop		= sun6i_ar100_stop,
};

static int sun6i_ar100_probe(struct platform_device *pdev)
{
	struct device_node *cpucfg_np, *sram_np;
	struct sun6i_ar100 *ar100;
	u32 val;
	int ret;

	rproc = rproc_alloc(&pdev->dev, pdev->name, &sun6i_ar100_ops,
			    SUN6I_AR100_FIRMWARE, sizeof(*ar100));
	if (!rproc)
		return -ENOMEM;
	rproc->has_iommu = false;
	rproc->fw_ops = sun6i_ar100_ops;
	ar100 = rproc->priv;

	ar100->clk = devm_clk_get(&pdev->dev, NULL);
	if (IS_ERR(ar100->clk)) {
		dev_err(&pdev->dev, "Couldn't get the AR100 clock\n");
		return PTR_ERR(ar100->clk);
	}

	/* Retrieve cpuconfig */
	cpucfg_np = of_find_matching_node(NULL, sun6i_ar100_cpucfg_dt_match);
	if (!cpucfg_np) {
		dev_err(&pdev->dev, "Failed to find CPU cfg node\n");
		return -EINVAL;
	}

	ar100->cpucfg = of_iomap(cpucfg_np, 0);
	of_node_put(cpucfg_np);
	if (!ar100->cpucfg) {
		dev_err(&pdev->dev, "Couldn't map cpucfg registers\n");
		return -ENOMEM;
	}

	/* Retrieve SRAM */
	sram_np = of_find_matching_node(NULL, sun6i_ar100_sram_dt_match);
	if (!sram_np) {
		dev_err(&pdev->dev, "Failed to find AR100 SRAM node\n");
		return -EINVAL;
	}

	ar100->sram = of_iomap(sram_np, 0);
	of_node_put(sram_np);
	if (!ar100->sram) {
		dev_err(&pdev->dev, "Couldn't map AR100 SRAM\n");
		return -ENOMEM;
	}

	/* Put the AR100 in reset */
	val = readl(ar100->cpucfg);
	writel(~1UL & val, ar100->cpucfg);

	/* Request the firmware */
	ret = request_firmware_nowait(THIS_MODULE, FW_ACTION_HOTPLUG,
				      "sun8i-a33-ar100-firmware.code",
				      &pdev->dev, GFP_KERNEL, ar100,
				      sun6i_ar100_load_firmware);
	if (ret) {
		dev_err(&pdev->dev, "Couldn't load AR100 firmware\n");
		return ret;
	}

	return 0;
}

static const struct of_device_id sun6i_ar100_dt_match[] = {
	{ .compatible = "allwinner,sun8i-a33-ar100" },
	{ },
};
MODULE_DEVICE_TABLE(of, sun6i_ar100_dt_match);

static struct platform_driver sun6i_ar100_driver = {
	.driver = {
		.name		= "sun6i-ar100",
		.of_match_table	= sun6i_ar100_dt_match,
	},
	.probe	= sun6i_ar100_probe,
};
module_platform_driver(sun6i_ar100_driver);

MODULE_AUTHOR("Maxime Ripard <maxime.ripard@free-electrons.com>");
MODULE_DESCRIPTION("Allwinner sun6i AR100 Driver");
MODULE_LICENSE("GPL");
