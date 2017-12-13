#include <linux/bitops.h>
#include <linux/clk.h>
#include <linux/delay.h>
#include <linux/err.h>
#include <linux/io.h>
#include <linux/jiffies.h>
#include <linux/module.h>
#include <linux/of.h>
#include <linux/of_device.h>
#include <linux/platform_device.h>
#include <linux/pwm.h>
#include <linux/slab.h>
#include <linux/spinlock.h>
#include <linux/time.h>

#define PWM_IRQ_ENABLE_REG	0x0000
#define PCIE(ch)	BIT(ch)

#define PWM_IRQ_STATUS_REG	0x0004
#define PIS(ch)	BIT(ch)

#define CAPTURE_IRQ_ENABLE_REG	0x0010
#define CFIE(ch)	BIT(ch << 1 + 1)
#define CRIE(ch)	BIT(ch << 1)

#define CAPTURE_IRQ_STATUS_REG	0x0014
#define CFIS(ch)	BIT(ch << 1 + 1)
#define CRIS(ch)	BIT(ch << 1)

#define CLK_CFG_REG(ch)	(0x0020 + (ch >> 1) * 4)
#define CLK_SRC	BIT(7)
#define CLK_SRC_BYPASS_SEC	BIT(6)
#define CLK_SRC_BYPASS_FIR	BIT(5)
#define CLK_GATING	BIT(4)
#define CLK_DIV_M	GENMASK(3, 0)

#define PWM_DZ_CTR_REG(ch)	(0x0030 + (ch >> 1) * 4)
#define PWM_DZ_INTV	GENMASK(15, 8)
#define PWM_DZ_EN	BIT(0)

#define PWM_ENABLE_REG	0x0040
#define PWM_EN(ch)	BIT(ch)

#define CAPTURE_ENABLE_REG	0x0044
#define CAP_EN(ch)	BIT(ch)

#define PWM_CTR_REG(ch)	(0x0060 + ch * 0x20)
#define PWM_PERIOD_RDY	BIT(11)
#define PWM_PUL_START	BIT(10)
#define PWM_MODE	BIT(9)
#define PWM_ACT_STA	BIT(8)
#define PWM_PRESCAL_K	GENMASK(7, 0)

#define PWM_PERIOD_REG(ch)	(0x0064 + ch * 0x20)
#define PWM_ENTIRE_CYCLE	GENMASK(31, 16)
#define PWM_ACT_CYCLE	GENMASK(15, 0)

#define PWM_CNT_REG(ch)	(0x0068 + ch * 0x20)
#define PWM_CNT_VAL	GENMASK(15, 0)

#define CAPTURE_CTR_REG(ch)	(0x006c + ch * 0x20)
#define CAPTURE_CRLF	BIT(2)
#define CAPTURE_CFLF	BIT(1)
#define CAPINV	BIT(0)

#define CAPTURE_RISE_REG(ch)	(0x0070 + ch * 0x20)
#define CAPTURE_CRLR	GENMASK(15, 0)

#define CAPTURE_FALL_REG(ch)	(0x0074 + ch * 0x20)
#define CAPTURE_CFLR	GENMASK(15, 0)

struct sunxi_pwm_data {
	bool has_prescaler_bypass;
	bool has_rdy;
	unsigned int npwm;
};

struct sunxi_pwm_chip {
	struct pwm_chip chip;
	struct clk *clk;
	void __iomem *base;
	spinlock_t ctrl_lock;
	const struct sunxi_pwm_data *data;
};

static const u16 div_m_table[] = {
	1,
	2,
	4,
	8,
	16,
	32,
	64,
	128,
	256
};

static inline struct sunxi_pwm_chip *to_sunxi_pwm_chip(struct pwm_chip *chip)
{
	return container_of(chip, struct sunxi_pwm_chip, chip);
}

static inline u32 sunxi_pwm_readl(struct sunxi_pwm_chip *chip,
		unsigned long offset)
{
	return readl(chip->base + offset);
}

static inline void sunxi_pwm_writel(struct sunxi_pwm_chip *chip,
		u32 val, unsigned long offset)
{
	writel(val, chip->base + offset);
}

static void sunxi_pwm_set_bit(struct sunxi_pwm_chip *sunxi_pwm,
		unsigned long reg, u32 bit)
{
	u32 val;

	val = sunxi_pwm_readl(sunxi_pwm, reg);
	val |= bit;
	sunxi_pwm_writel(sunxi_pwm, val, reg);
}

static void sunxi_pwm_clear_bit(struct sunxi_pwm_chip *sunxi_pwm,
		unsigned long reg, u32 bit)
{
	u32 val;

	val = sunxi_pwm_readl(sunxi_pwm, reg);
	val &= ~bit;
	sunxi_pwm_writel(sunxi_pwm, val, reg);
}

static void sunxi_pwm_set_value(struct sunxi_pwm_chip *sunxi_pwm,
		unsigned long reg, u32 mask, u32 val)
{
	u32 tmp;

	tmp = sunxi_pwm_readl(sunxi_pwm, reg);
	tmp &= ~mask;
	tmp |= mask & val;
	sunxi_pwm_writel(sunxi_pwm, tmp, reg);
}

static void sunxi_pwm_set_polarity(struct sunxi_pwm_chip *chip, u32 ch,
		enum pwm_polarity polarity)
{
	if (polarity == PWM_POLARITY_NORMAL)
		sunxi_pwm_set_bit(chip, PWM_CTR_REG(ch), PWM_ACT_STA);
	else
		sunxi_pwm_clear_bit(chip, PWM_CTR_REG(ch), PWM_ACT_STA);
}

static void sunxi_dump_reg(struct sunxi_pwm_chip *sunxi_pwm, u8 ch)
{
	dev_dbg(sunxi_pwm->chip.dev, "ch: %d\n"
			"\tPWM_IRQ_ENABLE_REG(%04x): \t0x%08x\n"
			"\tPWM_IRQ_STATUS_REG(%04x): \t0x%08x\n"
			"\tCAPTURE_IRQ_ENABLE_REG(%04x): \t0x%08x\n"
			"\tCAPTURE_IRQ_STATUS_REG(%04x): \t0x%08x\n"
			"\tCLK_CFG_REG(%04x)(%d): \t0x%08x\n"
			"\tPWM_DZ_CTR_REG(%04x)(%d): \t0x%08x\n"
			"\tPWM_ENABLE_REG(%04x): \t0x%08x\n"
			"\tCAPTURE_ENABLE_REG(%04x): \t0x%08x\n"
			"\tPWM_CTR_REG(%04x)(%d): \t0x%08x\n"
			"\tPWM_PERIOD_REG(%04x)(%d): \t0x%08x\n"
			"\tPWM_CNT_REG(%04x)(%d): \t0x%08x\n"
			"\tCAPTURE_CTR_REG(%04x)(%d): \t0x%08x\n"
			"\tCAPTURE_RISE_REG(%04x)(%d): \t0x%08x\n"
			"\tCAPTURE_FALL_REG(%04x)(%d): \t0x%08x\n",
			ch,
			PWM_IRQ_ENABLE_REG,
			readl(sunxi_pwm->base + PWM_IRQ_ENABLE_REG),
			PWM_IRQ_STATUS_REG,
			readl(sunxi_pwm->base + PWM_IRQ_STATUS_REG),
			CAPTURE_IRQ_ENABLE_REG,
			readl(sunxi_pwm->base + CAPTURE_IRQ_ENABLE_REG),
			CAPTURE_IRQ_STATUS_REG,
			readl(sunxi_pwm->base + CAPTURE_IRQ_STATUS_REG),
			CLK_CFG_REG(ch),
			ch, readl(sunxi_pwm->base + CLK_CFG_REG(ch)),
			PWM_DZ_CTR_REG(ch),
			ch, readl(sunxi_pwm->base + PWM_DZ_CTR_REG(ch)),
			PWM_ENABLE_REG,
			readl(sunxi_pwm->base + PWM_ENABLE_REG),
			CAPTURE_ENABLE_REG,
			readl(sunxi_pwm->base + CAPTURE_ENABLE_REG),
			PWM_CTR_REG(ch),
			ch, readl(sunxi_pwm->base + PWM_CTR_REG(ch)),
			PWM_PERIOD_REG(ch),
			ch, readl(sunxi_pwm->base + PWM_PERIOD_REG(ch)),
			PWM_CNT_REG(ch),
			ch, readl(sunxi_pwm->base + PWM_CNT_REG(ch)),
			CAPTURE_CTR_REG(ch),
			ch, readl(sunxi_pwm->base + CAPTURE_CTR_REG(ch)),
			CAPTURE_RISE_REG(ch),
			ch, readl(sunxi_pwm->base + CAPTURE_RISE_REG(ch)),
			CAPTURE_FALL_REG(ch),
			ch, readl(sunxi_pwm->base + CAPTURE_FALL_REG(ch)));
}

static int sunxi_pwm_config(struct sunxi_pwm_chip *sunxi_pwm, u8 ch,
			    const struct pwm_state *state)
{
	u64 clk_rate, clk_div, val;
	u16 prescaler = 0;
	u8 id = 0;

	clk_rate = clk_get_rate(sunxi_pwm->clk);
	dev_dbg(sunxi_pwm->chip.dev, "clock rate:%lld\n", clk_rate);

	if (clk_rate == 24000000)
		sunxi_pwm_clear_bit(sunxi_pwm, CLK_CFG_REG(ch), CLK_SRC);
	else
		sunxi_pwm_set_bit(sunxi_pwm, CLK_CFG_REG(ch), CLK_SRC);

	if (sunxi_pwm->data->has_prescaler_bypass) {
		/* pwm output bypass */
		if (ch % 2)
			sunxi_pwm_set_bit(sunxi_pwm, CLK_CFG_REG(ch),
					CLK_SRC_BYPASS_FIR);
		else
			sunxi_pwm_set_bit(sunxi_pwm, CLK_CFG_REG(ch),
					CLK_SRC_BYPASS_SEC);
		return 0;
	}

	val = state->period * clk_rate;
	do_div(val, NSEC_PER_SEC);
	if (val < 1) {
		dev_err(sunxi_pwm->chip.dev,
				"period expects a larger value\n");
		return -EINVAL;
	}

	/* calculate and set prescalar, div table, pwn entrie cycle */
	clk_div = val;

	while (clk_div > 65535) {
		prescaler++;
		clk_div = val;
		do_div(clk_div, prescaler + 1);
		do_div(clk_div, div_m_table[id]);

		if (prescaler == 255) {
			prescaler = 0;
			id++;
			if (id == 9)
				return -EINVAL;
		}
	}

	sunxi_pwm_set_value(sunxi_pwm, PWM_PERIOD_REG(ch),
			PWM_ENTIRE_CYCLE, clk_div << 16);
	sunxi_pwm_set_value(sunxi_pwm, PWM_CTR_REG(ch),
			PWM_PRESCAL_K, prescaler << 0);
	sunxi_pwm_set_value(sunxi_pwm, CLK_CFG_REG(ch),
			CLK_DIV_M, id << 0);

	/* set duty cycle */
	val = (prescaler + 1) * div_m_table[id] * clk_div;
	val = state->period;
	do_div(val, clk_div);
	clk_div = state->duty_cycle;
	do_div(clk_div, val);

	sunxi_pwm_set_value(sunxi_pwm, PWM_PERIOD_REG(ch),
			PWM_ACT_CYCLE, clk_div << 0);

	return 0;
}

static int sunxi_pwm_apply(struct pwm_chip *chip,
			   struct pwm_device *pwm,
			   const struct pwm_state *state)
{
	int ret;
	struct sunxi_pwm_chip *sunxi_pwm = to_sunxi_pwm_chip(chip);
	struct pwm_state cstate;

	sunxi_dump_reg(sunxi_pwm, pwm->hwpwm);
	pwm_get_state(pwm, &cstate);
	if (!cstate.enabled) {
		ret = clk_prepare_enable(sunxi_pwm->clk);
		if (ret) {
			dev_err(chip->dev, "failed to enable PWM clock\n");
			return ret;
		}
	}

	spin_lock(&sunxi_pwm->ctrl_lock);

	if (state->polarity != cstate.polarity)
		sunxi_pwm_set_polarity(sunxi_pwm, pwm->hwpwm, state->polarity);

	if ((cstate.period != state->period) ||
			(cstate.duty_cycle != state->duty_cycle)) {
		ret = sunxi_pwm_config(sunxi_pwm, pwm->hwpwm, state);
		if (ret) {
			dev_err(chip->dev, "failed to enable PWM clock\n");
			return ret;
		}
	}

	if (state->enabled) {
		sunxi_pwm_set_bit(sunxi_pwm,
				CLK_CFG_REG(pwm->hwpwm), CLK_GATING);

		sunxi_pwm_set_bit(sunxi_pwm,
				PWM_ENABLE_REG, PWM_EN(pwm->hwpwm));
	} else {
		sunxi_pwm_clear_bit(sunxi_pwm,
				CLK_CFG_REG(pwm->hwpwm), CLK_GATING);

		sunxi_pwm_clear_bit(sunxi_pwm,
				PWM_ENABLE_REG, PWM_EN(pwm->hwpwm));
	}

	spin_unlock(&sunxi_pwm->ctrl_lock);
	sunxi_dump_reg(sunxi_pwm, pwm->hwpwm);

	return 0;
}

static void sunxi_pwm_get_state(struct pwm_chip *chip, struct pwm_device *pwm,
		struct pwm_state *state)
{
	struct sunxi_pwm_chip *sunxi_pwm = to_sunxi_pwm_chip(chip);
	u64 clk_rate, tmp;
	u32 val;
	u16 clk_div, act_cycle;
	u8 prescal, id;

	clk_rate = clk_get_rate(sunxi_pwm->clk);

	val = sunxi_pwm_readl(sunxi_pwm, PWM_CTR_REG(pwm->hwpwm));
	if (PWM_ACT_STA & val)
		state->polarity = PWM_POLARITY_NORMAL;
	else
		state->polarity = PWM_POLARITY_INVERSED;

	prescal = PWM_PRESCAL_K & val;

	val = sunxi_pwm_readl(sunxi_pwm, PWM_ENABLE_REG);
	if (PWM_EN(pwm->hwpwm) & val)
		state->enabled = true;
	else
		state->enabled = false;

	val = sunxi_pwm_readl(sunxi_pwm, PWM_PERIOD_REG(pwm->hwpwm));
	act_cycle = PWM_ACT_CYCLE & val;
	clk_div = val >> 16;

	val = sunxi_pwm_readl(sunxi_pwm, CLK_CFG_REG(pwm->hwpwm));
	id = CLK_DIV_M & val;

	tmp = act_cycle * prescal * div_m_table[id] * NSEC_PER_SEC;
	state->duty_cycle = DIV_ROUND_CLOSEST_ULL(tmp, clk_rate);
	tmp = clk_div * prescal * div_m_table[id] * NSEC_PER_SEC;
	state->period = DIV_ROUND_CLOSEST_ULL(tmp, clk_rate);
}

static const struct pwm_ops sunxi_pwm_ops = {
	.apply = sunxi_pwm_apply,
	.get_state = sunxi_pwm_get_state,
	.owner = THIS_MODULE,
};

static const struct sunxi_pwm_data sunxi_pwm_data_r40 = {
	.has_prescaler_bypass = false,
	.has_rdy = true,
	.npwm = 8,
};

static const struct of_device_id sunxi_pwm_dt_ids[] = {
	{
		.compatible = "allwinner,sun8i-r40-pwm",
		.data = &sunxi_pwm_data_r40,
	},
	{},
};
MODULE_DEVICE_TABLE(of, sunxi_pwm_dt_ids);

static int sunxi_pwm_probe(struct platform_device *pdev)
{
	struct sunxi_pwm_chip *pwm;
	struct resource *res;
	int ret;
	const struct of_device_id *match;

	match = of_match_device(sunxi_pwm_dt_ids, &pdev->dev);

	pwm = devm_kzalloc(&pdev->dev, sizeof(*pwm), GFP_KERNEL);
	if (!pwm)
		return -ENOMEM;

	res = platform_get_resource(pdev, IORESOURCE_MEM, 0);
	pwm->base = devm_ioremap_resource(&pdev->dev, res);
	if (IS_ERR(pwm->base))
		return PTR_ERR(pwm->base);

	pwm->clk = devm_clk_get(&pdev->dev, NULL);
	if (IS_ERR(pwm->clk))
		return PTR_ERR(pwm->clk);

	pwm->data = match->data;
	pwm->chip.dev = &pdev->dev;
	pwm->chip.ops = &sunxi_pwm_ops;
	pwm->chip.base = -1;
	pwm->chip.npwm = pwm->data->npwm;
	pwm->chip.of_xlate = of_pwm_xlate_with_flags;
	pwm->chip.of_pwm_n_cells = 3;

	dev_dbg(pwm->chip.dev, "npwm: %d\n", pwm->chip.npwm);

	spin_lock_init(&pwm->ctrl_lock);

	ret = pwmchip_add(&pwm->chip);
	if (ret < 0) {
		dev_err(&pdev->dev, "failed to add PWM chip: %d\n", ret);
		return ret;
	}

	platform_set_drvdata(pdev, pwm);

	return 0;
}

static int sunxi_pwm_remove(struct platform_device *pdev)
{
	struct sunxi_pwm_chip *pwm = platform_get_drvdata(pdev);

	return pwmchip_remove(&pwm->chip);
}

static struct platform_driver sunxi_pwm_driver = {
	.driver = {
		.name = "sun8i-r40-pwm",
		.of_match_table = sunxi_pwm_dt_ids,
	},
	.probe = sunxi_pwm_probe,
	.remove = sunxi_pwm_remove,
};
module_platform_driver(sunxi_pwm_driver);

MODULE_ALIAS("platform:sun8i-r40-pwm");
MODULE_AUTHOR("Hao Zhang <hao5781286@gmail.com>");
MODULE_DESCRIPTION("Allwinner sun8i-r40 PWM driver");
MODULE_LICENSE("GPL v2");
