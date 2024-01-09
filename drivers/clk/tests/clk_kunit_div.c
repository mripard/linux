
#include <kunit/test.h>

#include <linux/clk-provider.h>

#include "clk_kunit_helpers.h"

#define DIV_WIDTH	4

static int clk_div_check_request(struct clk_hw *hw,
				 unsigned int try,
				 struct clk_hw_request *req)
{
	struct clk_div_context *ctx = hw_to_div(hw);
	unsigned long long rate;

	ctx->check_called++;

	rate = clk_hw_request_get_requested_rate(req);
	if (!rate)
		return 0;

	if (clk_hw_get_flags(hw) & CLK_SET_RATE_PARENT) {
		clk_hw_request_set_desired_parent(req,
						  clk_hw_get_parent(hw),
						  rate * ctx->div);
		return 0;
	}

	return 0;
}

static int clk_div_check_request_modify_parent(struct clk_hw *hw,
					       unsigned int try,
					       struct clk_hw_request *req)
{
	struct clk_div_context *ctx = hw_to_div(hw);
	struct clk_hw *parent = clk_hw_get_parent(hw);
	unsigned long long parent_rate = clk_hw_get_rate(parent);
	unsigned long long rate;

	ctx->check_called++;

	rate = clk_hw_request_get_requested_rate(req);
	if (!rate)
		return 0;

	if (!try)
		clk_hw_request_set_desired_parent_rate(req, parent_rate * 2);

	return 0;
}

static long clk_div_ro_round_rate(struct clk_hw *hw, unsigned long rate,
				  unsigned long *parent_rate)
{
	struct clk_div_context *ctx = hw_to_div(hw);

	return divider_ro_round_rate_parent(hw, clk_hw_get_parent(hw),
					    rate, parent_rate,
					    NULL, DIV_WIDTH,
					    CLK_DIVIDER_ONE_BASED, ctx->div);
}

static long clk_div_round_rate(struct clk_hw *hw, unsigned long rate,
			       unsigned long *parent_rate)
{
	return divider_round_rate_parent(hw, clk_hw_get_parent(hw),
					 rate, parent_rate,
					 NULL, DIV_WIDTH,
					 CLK_DIVIDER_ONE_BASED);
}

static int clk_div_set_rate(struct clk_hw *hw, unsigned long rate,
			    unsigned long parent_rate)
{
	struct clk_div_context *ctx = hw_to_div(hw);
	unsigned int div;

	div = divider_get_val(rate, parent_rate,
			      NULL, DIV_WIDTH,
			      CLK_DIVIDER_ONE_BASED);
	if (div < 0)
		return div;

	ctx->div = div;
	return 0;
}

static unsigned long clk_div_recalc_rate(struct clk_hw *hw,
					 unsigned long parent_rate)
{
	struct clk_div_context *ctx = hw_to_div(hw);

	return parent_rate / ctx->div;
}

const struct clk_ops clk_div_ro_ops = {
	.check_request = clk_div_check_request,
	.recalc_rate = clk_div_recalc_rate,
	.round_rate = clk_div_ro_round_rate,
};

const struct clk_ops clk_div_ops = {
	.check_request = clk_div_check_request,
	.recalc_rate = clk_div_recalc_rate,
	.round_rate = clk_div_round_rate,
	.set_rate = clk_div_set_rate,
};

const struct clk_ops clk_div_modify_parent_ops = {
	.check_request = clk_div_check_request_modify_parent,
	.recalc_rate = clk_div_recalc_rate,
};

struct clk_hw *clk_kunit_create_div_with_ops(struct kunit *test,
					     const struct clk_hw *parent,
					     const struct clk_ops *ops,
					     const char *name,
					     unsigned long flags,
					     unsigned int div)
{
	const struct clk_init_data data = {
		.flags = flags,
		.name = name,
		.parent_hws = &parent,
		.num_parents = 1,
		.ops = ops,
	};
	struct clk_div_context *ctx;
	struct clk_hw *hw;
	int ret;

	ctx = kunit_kzalloc(test, sizeof(*ctx), GFP_KERNEL);
	KUNIT_ASSERT_NOT_ERR_OR_NULL(test, ctx);

	hw = &ctx->hw;
	hw->init = &data;
	ctx->div = div;

	ret = clk_hw_register(NULL, hw);
	KUNIT_ASSERT_EQ(test, ret, 0);

	ret = kunit_add_action_or_reset(test, &clk_hw_unregister_wrapper, hw);
	KUNIT_ASSERT_EQ(test, ret, 0);

	return hw;
}

struct clk_hw *clk_kunit_create_div(struct kunit *test,
				    const struct clk_hw *parent,
				    const char *name,
				    unsigned long flags,
				    unsigned int div)
{
	return clk_kunit_create_div_with_ops(test, parent,
					     &clk_div_ops,
					     name, flags, div);
}

struct clk_hw *clk_kunit_create_ro_div(struct kunit *test,
				       const struct clk_hw *parent,
				       const char *name,
				       unsigned long flags,
				       unsigned int div)
{
	return clk_kunit_create_div_with_ops(test, parent,
					     &clk_div_ro_ops,
					     name, flags, div);
}
