/*
 * Copyright (C) 2016 Maxime Ripard
 * Maxime Ripard <maxime.ripard@free-electrons.com>
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License as
 * published by the Free Software Foundation; either version 2 of
 * the License, or (at your option) any later version.
 */

#include <linux/clk-provider.h>
#include <linux/spinlock.h>

#include "ccu_frac.h"

#define CREATE_TRACE_POINTS
#include "ccu_frac_trace.h"

bool ccu_frac_helper_is_enabled(struct ccu_common *common,
				struct ccu_frac_internal *cf)
{
	if (!(common->features & CCU_FEATURE_FRACTIONAL))
		return false;

	return !(readl(common->base + common->reg) & cf->enable);
}

void ccu_frac_helper_enable(struct ccu_common *common,
			    struct ccu_frac_internal *cf)
{
	unsigned long flags;
	u32 reg;

	if (!(common->features & CCU_FEATURE_FRACTIONAL))
		return;

	trace_clk_sunxi_frac_enable(common);

	spin_lock_irqsave(common->lock, flags);
	reg = readl(common->base + common->reg);
	writel(reg & ~cf->enable, common->base + common->reg);
	spin_unlock_irqrestore(common->lock, flags);
}

void ccu_frac_helper_disable(struct ccu_common *common,
			     struct ccu_frac_internal *cf)
{
	unsigned long flags;
	u32 reg;

	if (!(common->features & CCU_FEATURE_FRACTIONAL))
		return;

	trace_clk_sunxi_frac_disable(common);

	spin_lock_irqsave(common->lock, flags);
	reg = readl(common->base + common->reg);
	writel(reg | cf->enable, common->base + common->reg);
	spin_unlock_irqrestore(common->lock, flags);
}

bool ccu_frac_helper_has_rate(struct ccu_common *common,
			      struct ccu_frac_internal *cf,
			      unsigned long rate)
{
	if (!(common->features & CCU_FEATURE_FRACTIONAL))
		return false;

	return (cf->rates[0] == rate) || (cf->rates[1] == rate);
}

unsigned long ccu_frac_helper_read_rate(struct ccu_common *common,
					struct ccu_frac_internal *cf)
{
	u32 reg;

	if (!(common->features & CCU_FEATURE_FRACTIONAL))
		return 0;

	reg = readl(common->base + common->reg);

	trace_clk_sunxi_frac_read_rate(common, cf,
				       !!(reg & cf->select));

	return (reg & cf->select) ? cf->rates[1] : cf->rates[0];
}

int ccu_frac_helper_set_rate(struct ccu_common *common,
			     struct ccu_frac_internal *cf,
			     unsigned long rate, u32 lock)
{
	unsigned long flags;
	u32 reg, sel;

	if (!(common->features & CCU_FEATURE_FRACTIONAL))
		return -EINVAL;

	if (cf->rates[0] == rate)
		sel = 0;
	else if (cf->rates[1] == rate)
		sel = cf->select;
	else
		return -EINVAL;

	spin_lock_irqsave(common->lock, flags);
	reg = readl(common->base + common->reg);
	reg &= ~cf->select;
	writel(reg | sel, common->base + common->reg);
	spin_unlock_irqrestore(common->lock, flags);

	trace_clk_sunxi_frac_set_rate(common, rate);

	ccu_helper_wait_for_lock(common, lock);

	return 0;
}
