#include <kunit/test.h>
#include <kunit/test-bug.h>

#include <linux/clk-provider.h>

#include "clk_kunit_helpers.h"

#define DEFAULT_PARENT_OFFSET	(1000 * FREQ_1MHZ)

static int clk_mux_check_request(struct clk_hw *hw,
				 unsigned int try,
				 struct clk_hw_request *req)
{
	struct clk_mux_context *ctx = hw_to_mux(hw);
	struct kunit *test = kunit_get_current_test();
	unsigned long long parent_rate;
	unsigned int num_parents = clk_hw_get_num_parents(hw);
	struct clk_hw *parent;

	ctx->check_called++;

	if (ctx->flags & CLK_KUNIT_MUX_ITERATE_PARENT) {
		int parent_idx;

		parent_idx = clk_hw_get_parent_index(hw);
		KUNIT_ASSERT_GE(test, parent_idx, 0);

		parent_idx = (parent_idx + 1) + try;
		if (parent_idx >= num_parents)
			parent_idx = num_parents - 1;

		parent = clk_hw_get_parent_by_index(hw, parent_idx);
	} else {
		parent = clk_hw_get_parent(hw);
	}
	KUNIT_ASSERT_NOT_ERR_OR_NULL(test, parent);

	if (clk_hw_get_flags(hw) & CLK_SET_RATE_PARENT) {
		parent_rate = clk_hw_request_get_requested_rate(req);

		if (ctx->flags & CLK_KUNIT_MUX_CHANGE_PARENT_RATE)
			parent_rate += DEFAULT_PARENT_OFFSET;
	} else {
		parent_rate = clk_hw_get_rate(parent);
	}

	clk_hw_request_set_desired_parent(req,
					  parent,
					  parent_rate);

	return 0;
}

static int clk_multiple_parents_mux_set_parent(struct clk_hw *hw, u8 index)
{
	struct clk_mux_context *ctx = hw_to_mux(hw);

	if (index >= clk_hw_get_num_parents(hw))
		return -EINVAL;

	ctx->current_parent = index;

	return 0;
}

static u8 clk_multiple_parents_mux_get_parent(struct clk_hw *hw)
{
	struct clk_mux_context *ctx = hw_to_mux(hw);

	return ctx->current_parent;
}

static const struct clk_ops clk_multiple_parents_mux_ops = {
	.check_request = clk_mux_check_request,
	.get_parent = clk_multiple_parents_mux_get_parent,
	.set_parent = clk_multiple_parents_mux_set_parent,
	.determine_rate = __clk_mux_determine_rate_closest,
};

struct clk_hw *clk_test_create_mux(struct kunit *test,
				   const struct clk_hw **parent_hws, size_t num_parents,
				   const char *name,
				   unsigned long flags,
				   unsigned long mux_flags,
				   unsigned int default_parent)
{
	const struct clk_init_data data = {
		.flags = flags,
		.name = name,
		.parent_hws = parent_hws,
		.num_parents = num_parents,
		.ops = &clk_mux_ops,
	};
	struct clk_mux_context *ctx;
	struct clk_hw *hw;
	int ret;

	ctx = kunit_kzalloc(test, sizeof(*ctx), GFP_KERNEL);
	KUNIT_ASSERT_NOT_ERR_OR_NULL(test, ctx);

	hw = &ctx->hw;
	hw->init = &data;
	ctx->flags = mux_flags;
	ctx->current_parent = default_parent;

	ret = clk_hw_register(NULL, hw);
	KUNIT_ASSERT_EQ(test, ret, 0);

	ret = kunit_add_action_or_reset(test, &clk_hw_unregister_wrapper, hw);
	KUNIT_ASSERT_EQ(test, ret, 0);

	return hw;
}
