#include <kunit/test.h>
#include <kunit/test-bug.h>
#include <linux/clk.h>
#include <linux/clk-provider.h>
#include <linux/rational.h>

#include "clk_kunit_helpers.h"

#define FREQ_1KHZ		1000
#define FREQ_1MHZ		(1000 * FREQ_1KHZ)

#define DUMMY_CLOCK_INIT_RATE	(42 * FREQ_1MHZ)
#define DUMMY_CLOCK_RATE_1	(142 * FREQ_1MHZ)
#define DUMMY_CLOCK_RATE_2	(242 * FREQ_1MHZ)
#define DUMMY_CLOCK_RATE_3	(342 * FREQ_1MHZ)

struct clk_dummy_context {
	struct clk_hw hw;
	unsigned long rate;
	unsigned int check_called;
};

#define hw_to_dummy(_hw) \
	container_of_const(_hw, struct clk_dummy_context, hw)

static int clk_dummy_check_request(struct clk_hw *hw,
				   unsigned int try,
				   struct clk_hw_request *req)
{
	struct clk_dummy_context *ctx = hw_to_dummy(hw);
	unsigned long long rate;

	ctx->check_called++;

	rate = clk_hw_request_get_requested_rate(req);
	if (!rate)
		return 0;

	if (clk_hw_get_flags(hw) & CLK_SET_RATE_PARENT)
		clk_hw_request_set_desired_parent(req,
						  clk_hw_get_parent(hw),
						  rate);

	return 0;
}

static unsigned long clk_dummy_recalc_rate(struct clk_hw *hw,
					   unsigned long parent_rate)
{
	struct clk_dummy_context *ctx = hw_to_dummy(hw);

	return ctx->rate;
}

static int clk_dummy_determine_rate(struct clk_hw *hw,
				    struct clk_rate_request *req)
{
	/* Just return the same rate without modifying it */
	return 0;
}

static int clk_dummy_set_rate(struct clk_hw *hw,
			      unsigned long rate,
			      unsigned long parent_rate)
{
	struct clk_dummy_context *ctx = hw_to_dummy(hw);

	ctx->rate = rate;
	return 0;
}

static const struct clk_ops clk_dummy_rate_ops = {
	.check_request = clk_dummy_check_request,
	.recalc_rate = clk_dummy_recalc_rate,
	.determine_rate = clk_dummy_determine_rate,
	.set_rate = clk_dummy_set_rate,
};

struct clk_mux_context {
	struct clk_hw hw;
	unsigned int current_parent;
	unsigned int check_called;
};

#define hw_to_mux(_hw) \
	container_of_const(_hw, struct clk_mux_context, hw)

static int clk_mux_check_request(struct clk_hw *hw,
				 unsigned int try,
				 struct clk_hw_request *req)
{
	struct clk_mux_context *ctx = hw_to_mux(hw);

	ctx->check_called++;

	if (clk_hw_get_flags(hw) & CLK_SET_RATE_PARENT)
		clk_hw_request_set_desired_parent(req,
						  clk_hw_get_parent(hw),
						  clk_hw_request_get_requested_rate(req));

	return 0;
}

static int clk_mux_check_request_iterate_parent(struct clk_hw *hw,
						unsigned int try,
						struct clk_hw_request *req)
{
	struct clk_mux_context *ctx = hw_to_mux(hw);
	struct kunit *test = kunit_get_current_test();
	unsigned long long parent_rate;
	unsigned int num_parents = clk_hw_get_num_parents(hw);
	struct clk_hw *parent;
	int parent_idx;

	ctx->check_called++;

	parent_idx = clk_hw_get_parent_index(hw);
	KUNIT_ASSERT_GE(test, parent_idx, 0);

	parent_idx = (parent_idx + 1) + try;
	if (parent_idx >= num_parents)
		parent_idx = num_parents - 1;

	parent = clk_hw_get_parent_by_index(hw, parent_idx);
	KUNIT_ASSERT_NOT_ERR_OR_NULL(test, parent);

	if (clk_hw_get_flags(hw) & CLK_SET_RATE_PARENT)
		parent_rate = clk_hw_request_get_requested_rate(req);
	else
		parent_rate = clk_hw_get_rate(parent);

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

static const struct clk_ops clk_multiple_parents_mux_ops_iterate_parent = {
	.check_request = clk_mux_check_request_iterate_parent,
	.get_parent = clk_multiple_parents_mux_get_parent,
	.set_parent = clk_multiple_parents_mux_set_parent,
	.determine_rate = __clk_mux_determine_rate_closest,
};

static struct clk_hw *
clk_test_create_dummy(struct kunit *test,
		      const char *name,
		      unsigned long flags,
		      unsigned long long rate)
{
	const struct clk_init_data data = {
		.flags = flags,
		.name = name,
		.ops = &clk_dummy_rate_ops,
	};
	struct clk_dummy_context *ctx;
	struct clk_hw *hw;
	int ret;

	ctx = kunit_kzalloc(test, sizeof(*ctx), GFP_KERNEL);
	KUNIT_ASSERT_NOT_ERR_OR_NULL(test, ctx);

	hw = &ctx->hw;
	hw->init = &data;
	ctx->rate = rate;

	ret = clk_hw_register(NULL, hw);
	KUNIT_ASSERT_EQ(test, ret, 0);

	ret = kunit_add_action_or_reset(test, &clk_hw_unregister_wrapper, hw);
	KUNIT_ASSERT_EQ(test, ret, 0);

	return hw;
}

static struct clk_hw *
clk_test_create_mux_with_ops(struct kunit *test,
			     const struct clk_hw **parent_hws, size_t num_parents,
			     const struct clk_ops *ops,
			     const char *name,
			     unsigned long flags,
			     unsigned int default_parent)
{
	const struct clk_init_data data = {
		.flags = flags,
		.name = name,
		.parent_hws = parent_hws,
		.num_parents = num_parents,
		.ops = ops,
	};
	struct clk_mux_context *ctx;
	struct clk_hw *hw;
	int ret;

	ctx = kunit_kzalloc(test, sizeof(*ctx), GFP_KERNEL);
	KUNIT_ASSERT_NOT_ERR_OR_NULL(test, ctx);

	hw = &ctx->hw;
	hw->init = &data;
	ctx->current_parent = default_parent;

	ret = clk_hw_register(NULL, hw);
	KUNIT_ASSERT_EQ(test, ret, 0);

	ret = kunit_add_action_or_reset(test, &clk_hw_unregister_wrapper, hw);
	KUNIT_ASSERT_EQ(test, ret, 0);

	return hw;
}

static struct clk_hw *
clk_test_create_mux(struct kunit *test,
		    const struct clk_hw **parent_hws, size_t num_parents,
		    const char *name,
		    unsigned long flags,
		    unsigned int default_parent)
{
	return clk_test_create_mux_with_ops(test,
					    parent_hws, num_parents,
					    &clk_multiple_parents_mux_ops,
					    name, flags,
					    default_parent);
}

#define CLK_FRAC_MULT_WIDTH	4
#define CLK_FRAC_DIV_WIDTH	4

struct clk_frac_context {
	struct clk_hw hw;
	unsigned int m;
	unsigned int n;
	unsigned int check_called;
};

#define hw_to_frac(_hw) \
	container_of_const(_hw, struct clk_frac_context, hw)

static int clk_frac_check_request(struct clk_hw *hw,
				  unsigned int try,
				  struct clk_hw_request *req)
{
	struct clk_frac_context *ctx = hw_to_frac(hw);
	unsigned long long rate;

	ctx->check_called++;

	rate = clk_hw_request_get_requested_rate(req);
	if (!rate)
		return 0;

	if (clk_hw_get_flags(hw) & CLK_SET_RATE_PARENT)
		clk_hw_request_set_desired_parent(req,
						  clk_hw_get_parent(hw),
						  rate);

	return 0;
}


static bool is_better_rate(unsigned long target_rate,
			   unsigned long current_rate,
			   unsigned long best_rate)
{
	return abs(current_rate - target_rate) < abs(best_rate - target_rate);
}

static unsigned long clk_frac_round_rate_set_parent(struct clk_hw *hw,
						    unsigned long rate,
						    unsigned long *parent_rate,
						    unsigned int *mult,
						    unsigned int *div)
{
	struct clk_hw *parent = clk_hw_get_parent(hw);
	unsigned long best_parent_rate = *parent_rate;
	unsigned long best_rate = 0;
	unsigned int best_mult = 0;
	unsigned int best_div = 0;
	unsigned int _mult, _div;

	for (_mult = 1; _mult <= GENMASK(CLK_FRAC_MULT_WIDTH - 1, 0); _mult++) {
		for (_div = 1; _div <= GENMASK(CLK_FRAC_DIV_WIDTH - 1, 0); _div++) {
			unsigned long tmp_rate, tmp_parent;

			tmp_parent = clk_hw_round_rate(parent,
						       rate * _div / _mult);
			tmp_rate = tmp_parent * _mult / _div;

			if (is_better_rate(rate, tmp_rate, best_rate)) {
				best_rate = tmp_rate;
				best_parent_rate = tmp_parent;
				best_mult = _mult;
				best_div = _div;
			}
		}
	}

	if (mult)
		*mult = best_mult;
	if (div)
		*div = best_div;

	*parent_rate = best_parent_rate;

	return best_rate;
}

static long clk_frac_round_rate(struct clk_hw *hw, unsigned long rate,
				unsigned long *parent_rate)
{
	unsigned long mult, div;
	u64 ret;

	if (clk_hw_get_flags(hw) & CLK_SET_RATE_PARENT)
		return clk_frac_round_rate_set_parent(hw, rate, parent_rate, NULL, NULL);

	rational_best_approximation(rate, *parent_rate,
				    GENMASK(CLK_FRAC_MULT_WIDTH - 1, 0),
				    GENMASK(CLK_FRAC_DIV_WIDTH - 1, 0),
				    &mult, &div);

	ret = (u64)*parent_rate * mult;
	do_div(ret, div);

	return ret;
}

static unsigned long clk_frac_recalc_rate(struct clk_hw *hw,
					  unsigned long parent_rate)
{
	struct clk_frac_context *ctx = hw_to_frac(hw);
	u64 ret;

	ret = (u64)parent_rate * ctx->m;
	do_div(ret, ctx->n);

	return ret;
}


static int clk_frac_set_rate(struct clk_hw *hw, unsigned long rate,
			     unsigned long parent_rate)
{
	struct clk_frac_context *ctx = hw_to_frac(hw);
	unsigned long m, n;

	rational_best_approximation(rate, parent_rate,
				    GENMASK(7, 0), GENMASK(7, 0),
				    &m, &n);

	ctx->m = m;
	ctx->n = n;

	return 0;
}

static const struct clk_ops clk_frac_ops = {
	.check_request = clk_frac_check_request,
	.recalc_rate = clk_frac_recalc_rate,
	.round_rate = clk_frac_round_rate,
	.set_rate = clk_frac_set_rate,
};

static struct clk_hw *clk_test_create_frac(struct kunit *test,
					   const struct clk_hw *parent,
					   const char *name,
					   unsigned long flags,
					   unsigned int default_mult,
					   unsigned int default_div)
{
	const struct clk_init_data data = {
		.flags = flags,
		.name = name,
		.parent_hws = &parent,
		.num_parents = 1,
		.ops = &clk_frac_ops,
	};
	struct clk_frac_context *ctx;
	struct clk_hw *hw;
	int ret;

	ctx = kunit_kzalloc(test, sizeof(*ctx), GFP_KERNEL);
	KUNIT_ASSERT_NOT_ERR_OR_NULL(test, ctx);

	hw = &ctx->hw;
	hw->init = &data;
	ctx->m = default_mult;
	ctx->n = default_div;

	ret = clk_hw_register(NULL, hw);
	KUNIT_ASSERT_EQ(test, ret, 0);

	ret = kunit_add_action_or_reset(test, &clk_hw_unregister_wrapper, hw);
	KUNIT_ASSERT_EQ(test, ret, 0);

	return hw;
}

KUNIT_DEFINE_ACTION_WRAPPER(clk_request_put_wrapper,
			    clk_request_put,
			    struct clk_request *);

static struct clk_request *
clk_kunit_request_get(struct kunit *test, struct clk *clk)
{
	struct clk_request *req;
	int ret;

	req = clk_request_get(clk);
	KUNIT_ASSERT_NOT_ERR_OR_NULL(test, req);

	ret = kunit_add_action_or_reset(test, &clk_request_put_wrapper, req);
	KUNIT_ASSERT_EQ(test, ret, 0);

	return req;
}

/*
 * Test that a clock that has a rate request for it and doesn't have
 * SET_RATE_PARENT will only affect itself, and none of its parent.
 */
static void clk_request_test_lone_clock(struct kunit *test)
{
	struct clk_hw *parent, *child;
	struct clk_request *req;
	struct clk *clk;
	int ret;

	parent = clk_test_create_dummy(test,
				       "parent",
				       0,
				       DUMMY_CLOCK_RATE_1);
	KUNIT_ASSERT_NOT_ERR_OR_NULL(test, parent);

	child = clk_kunit_create_ro_div(test,
					parent,
					"test",
					0,
					1);
	KUNIT_ASSERT_NOT_ERR_OR_NULL(test, child);

	clk = clk_hw_get_clk(child, NULL);
	KUNIT_ASSERT_NOT_ERR_OR_NULL(test, clk);

	req = clk_kunit_request_get(test, clk);
	KUNIT_ASSERT_NOT_ERR_OR_NULL(test, req);

	ret = clk_request_add_clock_rate(req, clk, 144000000);
	KUNIT_ASSERT_EQ(test, ret, 0);

	ret = clk_request_check(req);
	KUNIT_EXPECT_EQ(test, ret, 0);
	KUNIT_EXPECT_EQ(test, clk_request_len(req), 1);
	KUNIT_EXPECT_TRUE(test, clk_hw_is_in_request(child, req));
}

/*
 * Test that a mux clock that has a rate request for it and doesn't have
 * SET_RATE_PARENT will only affect itself, and none of its parent.
 */
static void clk_request_test_lone_mux_clock(struct kunit *test)
{
	struct clk_hw *parent_1, *parent_2, *child;
	const struct clk_hw *parents[2];
	struct clk_request *req;
	struct clk *clk;
	int ret;

	parent_1 = clk_test_create_dummy(test,
					 "parent-0",
					 0,
					 DUMMY_CLOCK_RATE_1);
	KUNIT_ASSERT_NOT_ERR_OR_NULL(test, parent_1);
	parents[0] = parent_1;

	parent_2 = clk_test_create_dummy(test,
					 "parent-1",
					 0,
					 DUMMY_CLOCK_RATE_2);
	KUNIT_ASSERT_NOT_ERR_OR_NULL(test, parent_2);
	parents[1] = parent_2;

	child = clk_test_create_mux(test,
				    parents, ARRAY_SIZE(parents),
				    "test-mux",
				    0,
				    0);
	KUNIT_ASSERT_NOT_ERR_OR_NULL(test, child);

	clk = clk_hw_get_clk(child, NULL);
	KUNIT_ASSERT_NOT_ERR_OR_NULL(test, clk);

	req = clk_kunit_request_get(test, clk);
	KUNIT_ASSERT_NOT_ERR_OR_NULL(test, req);

	ret = clk_request_add_clock_rate(req, clk, 144000000);
	KUNIT_ASSERT_EQ(test, ret, 0);

	ret = clk_request_check(req);
	KUNIT_EXPECT_EQ(test, ret, 0);
	KUNIT_EXPECT_EQ(test, clk_request_len(req), 1);
	KUNIT_EXPECT_TRUE(test, clk_hw_is_in_request(child, req));
}

/*
 * Test that a single clock that has a rate request for it and doesn't
 * have SET_RATE_PARENT will have its check_request implementation
 * called.
 */
static void clk_request_test_single_clock_checked(struct kunit *test)
{
	struct clk_dummy_context *ctx;
	struct clk_request *req;
	struct clk_hw *hw;
	struct clk *clk;
	int ret;

	hw = clk_test_create_dummy(test,
				   "clk",
				   0,
				   DUMMY_CLOCK_RATE_1);
	KUNIT_ASSERT_NOT_ERR_OR_NULL(test, hw);
	ctx = hw_to_dummy(hw);

	clk = clk_hw_get_clk(hw, NULL);
	KUNIT_ASSERT_NOT_ERR_OR_NULL(test, clk);

	req = clk_kunit_request_get(test, clk);
	KUNIT_ASSERT_NOT_ERR_OR_NULL(test, req);

	ret = clk_request_add_clock_rate(req, clk, 144000000);
	KUNIT_ASSERT_EQ(test, ret, 0);
	KUNIT_ASSERT_EQ(test, clk_request_len(req), 1);

	ret = clk_request_check(req);
	KUNIT_EXPECT_EQ(test, ret, 0);
	KUNIT_EXPECT_EQ(test, ctx->check_called, 1);
}

/*
 * Test that a clock that has a rate request for it and doesn't have
 * SET_RATE_PARENT will have its check_request implementation but not
 * its parents.
 */
static void clk_request_test_lone_clock_checked(struct kunit *test)
{
	struct clk_dummy_context *parent_ctx;
	struct clk_div_context *child_ctx;
	struct clk_hw *parent, *child;
	struct clk_request *req;
	struct clk *clk;
	int ret;

	parent = clk_test_create_dummy(test,
				       "parent",
				       0,
				       DUMMY_CLOCK_RATE_1);
	KUNIT_ASSERT_NOT_ERR_OR_NULL(test, parent);
	parent_ctx = hw_to_dummy(parent);

	child = clk_kunit_create_ro_div(test,
					parent,
					"test",
					0,
					1);
	KUNIT_ASSERT_NOT_ERR_OR_NULL(test, child);
	child_ctx = hw_to_div(child);

	clk = clk_hw_get_clk(child, NULL);
	KUNIT_ASSERT_NOT_ERR_OR_NULL(test, clk);

	req = clk_kunit_request_get(test, clk);
	KUNIT_ASSERT_NOT_ERR_OR_NULL(test, req);

	ret = clk_request_add_clock_rate(req, clk, 144000000);
	KUNIT_ASSERT_EQ(test, ret, 0);
	KUNIT_ASSERT_EQ(test, clk_request_len(req), 1);
	KUNIT_ASSERT_TRUE(test, clk_hw_is_in_request(child, req));
	KUNIT_ASSERT_FALSE(test, clk_hw_is_in_request(parent, req));

	ret = clk_request_check(req);
	KUNIT_EXPECT_EQ(test, ret, 0);
	KUNIT_EXPECT_EQ(test, parent_ctx->check_called, 0);
	KUNIT_EXPECT_EQ(test, child_ctx->check_called, 1);
}

static void clk_request_test_lone_clock_change_parent_rate(struct kunit *test)
{
	struct clk_hw *parent, *child;
	struct clk_request *req;
	struct clk *clk;
	int ret;

	parent = clk_test_create_dummy(test,
				       "parent",
				       0,
				       DUMMY_CLOCK_RATE_1);
	KUNIT_ASSERT_NOT_ERR_OR_NULL(test, parent);

	child = clk_kunit_create_div_with_ops(test,
					      parent,
					      &clk_div_modify_parent_ops,
					      "test",
					      0,
					      1);
	KUNIT_ASSERT_NOT_ERR_OR_NULL(test, child);

	clk = clk_hw_get_clk(child, NULL);
	KUNIT_ASSERT_NOT_ERR_OR_NULL(test, clk);

	req = clk_kunit_request_get(test, clk);
	KUNIT_ASSERT_NOT_ERR_OR_NULL(test, req);

	ret = clk_request_add_clock_rate(req, clk, 144000000);
	KUNIT_ASSERT_EQ(test, ret, 0);

	ret = clk_request_check(req);
	KUNIT_ASSERT_EQ(test, ret, 0);
	KUNIT_EXPECT_EQ(test, clk_request_len(req), 2);
	KUNIT_EXPECT_TRUE(test, clk_hw_is_in_request(child, req));
	KUNIT_EXPECT_TRUE(test, clk_hw_is_in_request(parent, req));
}

static void clk_request_test_lone_clock_change_parent_rate_checked(struct kunit *test)
{
	struct clk_dummy_context *parent_ctx;
	struct clk_div_context *child_ctx;
	struct clk_hw *parent, *child;
	struct clk_request *req;
	struct clk *clk;
	int ret;

	parent = clk_test_create_dummy(test,
				       "parent",
				       0,
				       DUMMY_CLOCK_RATE_1);
	KUNIT_ASSERT_NOT_ERR_OR_NULL(test, parent);
	parent_ctx = hw_to_dummy(parent);

	child = clk_kunit_create_div_with_ops(test,
					      parent,
					      &clk_div_modify_parent_ops,
					      "test",
					      0,
					      1);
	KUNIT_ASSERT_NOT_ERR_OR_NULL(test, child);
	child_ctx = hw_to_div(child);

	clk = clk_hw_get_clk(child, NULL);
	KUNIT_ASSERT_NOT_ERR_OR_NULL(test, clk);

	req = clk_kunit_request_get(test, clk);
	KUNIT_ASSERT_NOT_ERR_OR_NULL(test, req);

	ret = clk_request_add_clock_rate(req, clk, 144000000);
	KUNIT_ASSERT_EQ(test, ret, 0);

	ret = clk_request_check(req);
	KUNIT_EXPECT_EQ(test, ret, 0);
	KUNIT_EXPECT_EQ(test, clk_request_len(req), 2);
	KUNIT_EXPECT_GE(test, parent_ctx->check_called, 1);
	KUNIT_EXPECT_GE(test, child_ctx->check_called, 1);
}

/*
 * Test that a clock that has a rate request for it has SET_RATE_PARENT
 * will affect itself and its parent if it doesn't have any sibling.
 */
static void clk_request_test_lone_clock_set_rate(struct kunit *test)
{
	struct clk_hw *parent, *child;
	struct clk_request *req;
	struct clk *clk;
	int ret;

	parent = clk_test_create_dummy(test,
				       "parent",
				       0,
				       DUMMY_CLOCK_RATE_1);
	KUNIT_ASSERT_NOT_ERR_OR_NULL(test, parent);

	child = clk_kunit_create_ro_div(test,
					parent,
					"test",
					CLK_SET_RATE_PARENT,
					1);
	KUNIT_ASSERT_NOT_ERR_OR_NULL(test, child);

	clk = clk_hw_get_clk(child, NULL);
	KUNIT_ASSERT_NOT_ERR_OR_NULL(test, clk);

	req = clk_kunit_request_get(test, clk);
	KUNIT_ASSERT_NOT_ERR_OR_NULL(test, req);

	ret = clk_request_add_clock_rate(req, clk, 144000000);
	KUNIT_ASSERT_EQ(test, ret, 0);

	ret = clk_request_check(req);
	KUNIT_EXPECT_EQ(test, ret, 0);
	KUNIT_EXPECT_EQ(test, clk_request_len(req), 2);
	KUNIT_EXPECT_TRUE(test, clk_hw_is_in_request(child, req));
	KUNIT_EXPECT_TRUE(test, clk_hw_is_in_request(parent, req));
}

/*
 * Test that a clock that has a rate request for it and has
 * SET_RATE_PARENT is part of the request, will have its check_request
 * function called and its parent too.
 */
static void clk_request_test_lone_clock_set_rate_checked(struct kunit *test)
{
	struct clk_dummy_context *parent_ctx;
	struct clk_div_context *child_ctx;
	struct clk_hw *parent, *child;
	struct clk_request *req;
	struct clk *clk;
	int ret;

	parent = clk_test_create_dummy(test,
				       "parent",
				       0,
				       DUMMY_CLOCK_RATE_1);
	KUNIT_ASSERT_NOT_ERR_OR_NULL(test, parent);
	parent_ctx = hw_to_dummy(parent);

	child = clk_kunit_create_ro_div(test,
					parent,
					"test",
					CLK_SET_RATE_PARENT,
					1);
	KUNIT_ASSERT_NOT_ERR_OR_NULL(test, child);
	child_ctx = hw_to_div(child);

	clk = clk_hw_get_clk(child, NULL);
	KUNIT_ASSERT_NOT_ERR_OR_NULL(test, clk);

	req = clk_kunit_request_get(test, clk);
	KUNIT_ASSERT_NOT_ERR_OR_NULL(test, req);

	ret = clk_request_add_clock_rate(req, clk, 144000000);
	KUNIT_ASSERT_EQ(test, ret, 0);

	ret = clk_request_check(req);
	KUNIT_EXPECT_EQ(test, ret, 0);
	KUNIT_EXPECT_EQ(test, clk_request_len(req), 2);
	KUNIT_EXPECT_GE(test, parent_ctx->check_called, 1);
	KUNIT_EXPECT_GE(test, child_ctx->check_called, 1);
}

/*
 * Test that a clock that has a rate request for it has SET_RATE_PARENT
 * will affect itself, its siblings and their parent.
 */
static void clk_request_test_parent_clock(struct kunit *test)
{
	struct clk_hw *parent, *child_1, *child_2;
	struct clk_request *req;
	struct clk *clk;
	int ret;

	parent = clk_test_create_dummy(test,
				       "parent",
				       0,
				       DUMMY_CLOCK_RATE_1);
	KUNIT_ASSERT_NOT_ERR_OR_NULL(test, parent);

	child_1 = clk_kunit_create_ro_div(test,
					  parent,
					  "test-1",
					  0,
					  1);
	KUNIT_ASSERT_NOT_ERR_OR_NULL(test, child_1);

	child_2 = clk_kunit_create_ro_div(test,
					  parent,
					  "test-2",
					  0,
					  1);
	KUNIT_ASSERT_NOT_ERR_OR_NULL(test, child_2);

	clk = clk_hw_get_clk(parent, NULL);
	KUNIT_ASSERT_NOT_ERR_OR_NULL(test, clk);

	req = clk_kunit_request_get(test, clk);
	KUNIT_ASSERT_NOT_ERR_OR_NULL(test, req);

	ret = clk_request_add_clock_rate(req, clk, 144000000);
	KUNIT_ASSERT_EQ(test, ret, 0);

	ret = clk_request_check(req);
	KUNIT_EXPECT_EQ(test, ret, 0);
	KUNIT_EXPECT_EQ(test, clk_request_len(req), 3);
	KUNIT_EXPECT_TRUE(test, clk_hw_is_in_request(parent, req));
	KUNIT_EXPECT_TRUE(test, clk_hw_is_in_request(child_1, req));
	KUNIT_EXPECT_TRUE(test, clk_hw_is_in_request(child_2, req));
}

static void clk_request_test_parent_clock_3_levels(struct kunit *test)
{
	struct clk_hw *top, *middle, *bottom;
	struct clk_request *req;
	struct clk *clk;
	int ret;

	top = clk_test_create_dummy(test,
				    "top",
				    0,
				    DUMMY_CLOCK_RATE_1);
	KUNIT_ASSERT_NOT_ERR_OR_NULL(test, top);

	middle = clk_kunit_create_ro_div(test,
					 top,
					 "middle",
					 CLK_SET_RATE_PARENT,
					 1);
	KUNIT_ASSERT_NOT_ERR_OR_NULL(test, middle);

	bottom = clk_kunit_create_ro_div(test,
					 middle,
					 "bottom",
					 0,
					 1);
	KUNIT_ASSERT_NOT_ERR_OR_NULL(test, bottom);

	clk = clk_hw_get_clk(middle, NULL);
	KUNIT_ASSERT_NOT_ERR_OR_NULL(test, clk);

	req = clk_kunit_request_get(test, clk);
	KUNIT_ASSERT_NOT_ERR_OR_NULL(test, req);

	ret = clk_request_add_clock_rate(req, clk, 144000000);
	KUNIT_ASSERT_EQ(test, ret, 0);

	ret = clk_request_check(req);
	KUNIT_EXPECT_EQ(test, ret, 0);
	KUNIT_EXPECT_EQ(test, clk_request_len(req), 3);
	KUNIT_EXPECT_TRUE(test, clk_hw_is_in_request(top, req));
	KUNIT_EXPECT_TRUE(test, clk_hw_is_in_request(middle, req));
	KUNIT_EXPECT_TRUE(test, clk_hw_is_in_request(bottom, req));
}

/*
 * Test that a clock that has a rate request for it has SET_RATE_PARENT
 * will affect itself, its siblings and their parent.
 */
static void clk_request_test_siblings_clocks_set_rate(struct kunit *test)
{
	struct clk_hw *parent, *child_1, *child_2;
	struct clk_request *req;
	struct clk *clk;
	int ret;

	parent = clk_test_create_dummy(test,
				       "parent",
				       0,
				       DUMMY_CLOCK_RATE_1);
	KUNIT_ASSERT_NOT_ERR_OR_NULL(test, parent);

	child_1 = clk_kunit_create_ro_div(test,
					    parent,
					    "test-1",
					    CLK_SET_RATE_PARENT,
					    1);
	KUNIT_ASSERT_NOT_ERR_OR_NULL(test, child_1);

	child_2 = clk_kunit_create_ro_div(test,
					    parent,
					    "test-2",
					    CLK_SET_RATE_PARENT,
					    1);
	KUNIT_ASSERT_NOT_ERR_OR_NULL(test, child_2);

	clk = clk_hw_get_clk(child_1, NULL);
	KUNIT_ASSERT_NOT_ERR_OR_NULL(test, clk);

	req = clk_kunit_request_get(test, clk);
	KUNIT_ASSERT_NOT_ERR_OR_NULL(test, req);

	ret = clk_request_add_clock_rate(req, clk, 144000000);
	KUNIT_ASSERT_EQ(test, ret, 0);

	ret = clk_request_check(req);
	KUNIT_EXPECT_EQ(test, ret, 0);
	KUNIT_EXPECT_EQ(test, clk_request_len(req), 3);
	KUNIT_EXPECT_TRUE(test, clk_hw_is_in_request(parent, req));
	KUNIT_EXPECT_TRUE(test, clk_hw_is_in_request(child_1, req));
	KUNIT_EXPECT_TRUE(test, clk_hw_is_in_request(child_2, req));
}

static void clk_request_test_siblings_3_levels_set_rate_last_level(struct kunit *test)
{
	struct clk_hw *top;
	struct clk_hw *middle_left, *middle_right;
	struct clk_hw *bottom_left_left, *bottom_left_right,
		*bottom_right_left, *bottom_right_right;
	struct clk_request *req;
	struct clk *clk;
	int ret;

	top = clk_test_create_dummy(test,
				    "top",
				    0,
				    DUMMY_CLOCK_RATE_1);
	KUNIT_ASSERT_NOT_ERR_OR_NULL(test, top);

	middle_left = clk_kunit_create_ro_div(test,
						top,
						"middle-left",
						0,
						1);
	KUNIT_ASSERT_NOT_ERR_OR_NULL(test, middle_left);

	middle_right = clk_kunit_create_ro_div(test,
						 top,
						 "middle-right",
						 0,
						 1);
	KUNIT_ASSERT_NOT_ERR_OR_NULL(test, middle_right);

	bottom_left_left =
		clk_kunit_create_ro_div(test,
					  middle_left,
					  "bottom-left-left",
					  CLK_SET_RATE_PARENT,
					  1);
	KUNIT_ASSERT_NOT_ERR_OR_NULL(test, bottom_left_left);

	bottom_left_right =
		clk_kunit_create_ro_div(test,
					  middle_left,
					  "bottom-left-right",
					  CLK_SET_RATE_PARENT,
					  1);
	KUNIT_ASSERT_NOT_ERR_OR_NULL(test, bottom_left_right);

	bottom_right_left =
		clk_kunit_create_ro_div(test,
					  middle_right,
					  "bottom-right-left",
					  CLK_SET_RATE_PARENT,
					  1);
	KUNIT_ASSERT_NOT_ERR_OR_NULL(test, bottom_right_left);

	bottom_right_right =
		clk_kunit_create_ro_div(test,
					  middle_right,
					  "bottom-right-right",
					  CLK_SET_RATE_PARENT,
					  1);
	KUNIT_ASSERT_NOT_ERR_OR_NULL(test, bottom_right_right);

	clk = clk_hw_get_clk(bottom_left_left, NULL);
	KUNIT_ASSERT_NOT_ERR_OR_NULL(test, clk);

	req = clk_kunit_request_get(test, clk);
	KUNIT_ASSERT_NOT_ERR_OR_NULL(test, req);

	ret = clk_request_add_clock_rate(req, clk, 144000000);
	KUNIT_ASSERT_EQ(test, ret, 0);

	ret = clk_request_check(req);
	KUNIT_EXPECT_EQ(test, ret, 0);
	KUNIT_EXPECT_EQ(test, clk_request_len(req), 3);
	KUNIT_EXPECT_TRUE(test, clk_hw_is_in_request(middle_left, req));
	KUNIT_EXPECT_TRUE(test, clk_hw_is_in_request(bottom_left_left, req));
	KUNIT_EXPECT_TRUE(test, clk_hw_is_in_request(bottom_left_right, req));
}

static void clk_request_test_siblings_3_levels_set_rate_all_levels(struct kunit *test)
{
	struct clk_hw *top;
	struct clk_hw *middle_left, *middle_right;
	struct clk_hw *bottom_left_left, *bottom_left_right,
		*bottom_right_left, *bottom_right_right;
	struct clk_request *req;
	struct clk *clk;
	int ret;

	top = clk_test_create_dummy(test,
				    "top",
				    0,
				    DUMMY_CLOCK_RATE_1);
	KUNIT_ASSERT_NOT_ERR_OR_NULL(test, top);

	middle_left = clk_kunit_create_ro_div(test,
						top,
						"middle-left",
						CLK_SET_RATE_PARENT,
						1);
	KUNIT_ASSERT_NOT_ERR_OR_NULL(test, middle_left);

	middle_right = clk_kunit_create_ro_div(test,
						 top,
						 "middle-right",
						 CLK_SET_RATE_PARENT,
						 1);
	KUNIT_ASSERT_NOT_ERR_OR_NULL(test, middle_right);

	bottom_left_left =
		clk_kunit_create_ro_div(test,
					  middle_left,
					  "bottom-left-left",
					  CLK_SET_RATE_PARENT,
					  1);
	KUNIT_ASSERT_NOT_ERR_OR_NULL(test, bottom_left_left);

	bottom_left_right =
		clk_kunit_create_ro_div(test,
					  middle_left,
					  "bottom-left-right",
					  CLK_SET_RATE_PARENT,
					  1);
	KUNIT_ASSERT_NOT_ERR_OR_NULL(test, bottom_left_right);

	bottom_right_left =
		clk_kunit_create_ro_div(test,
					  middle_right,
					  "bottom-right-left",
					  CLK_SET_RATE_PARENT,
					  1);
	KUNIT_ASSERT_NOT_ERR_OR_NULL(test, bottom_right_left);

	bottom_right_right =
		clk_kunit_create_ro_div(test,
					  middle_right,
					  "bottom-right-right",
					  CLK_SET_RATE_PARENT,
					  1);
	KUNIT_ASSERT_NOT_ERR_OR_NULL(test, bottom_right_right);

	clk = clk_hw_get_clk(bottom_left_left, NULL);
	KUNIT_ASSERT_NOT_ERR_OR_NULL(test, clk);

	req = clk_kunit_request_get(test, clk);
	KUNIT_ASSERT_NOT_ERR_OR_NULL(test, req);

	ret = clk_request_add_clock_rate(req, clk, 144000000);
	KUNIT_ASSERT_EQ(test, ret, 0);

	ret = clk_request_check(req);
	KUNIT_EXPECT_EQ(test, ret, 0);
	KUNIT_EXPECT_EQ(test, clk_request_len(req), 7);
	KUNIT_EXPECT_TRUE(test, clk_hw_is_in_request(top, req));
	KUNIT_EXPECT_TRUE(test, clk_hw_is_in_request(middle_left, req));
	KUNIT_EXPECT_TRUE(test, clk_hw_is_in_request(middle_right, req));
	KUNIT_EXPECT_TRUE(test, clk_hw_is_in_request(bottom_left_left, req));
	KUNIT_EXPECT_TRUE(test, clk_hw_is_in_request(bottom_left_right, req));
	KUNIT_EXPECT_TRUE(test, clk_hw_is_in_request(bottom_right_left, req));
	KUNIT_EXPECT_TRUE(test, clk_hw_is_in_request(bottom_right_right, req));
}

static void clk_request_test_4(struct kunit *test)
{
	/*
	 * TODO: Test that a clock that has a rate request for it and
	 * has SET_RATE_PARENT is part of the request, and its parent
	 * is called first
	 */
}

/*
 * Test that if the clock that was the trigger of the request wants to
 * change its parent, the old clocks that were there by side effect (its
 * old parent, its old siblings) will no longer be part of the request,
 * and its new parent and siblings will be.
 */
static void clk_request_test_reparent(struct kunit *test)
{
	struct clk_hw *top_left, *top_right;
	struct clk_hw *bottom_left, *bottom_middle, *bottom_right;
	const struct clk_hw *parents[2];
	struct clk_request *req;
	struct clk *clk;
	int ret;

	top_left = clk_test_create_dummy(test,
					 "top-left",
					 0,
					 DUMMY_CLOCK_RATE_1);
	KUNIT_ASSERT_NOT_ERR_OR_NULL(test, top_left);

	top_right = clk_test_create_dummy(test,
					  "top-right",
					  0,
					  DUMMY_CLOCK_RATE_2);
	KUNIT_ASSERT_NOT_ERR_OR_NULL(test, top_right);

	bottom_left = clk_kunit_create_ro_div(test,
						top_left,
						"bottom-left",
						0,
						1);
	KUNIT_ASSERT_NOT_ERR_OR_NULL(test, bottom_left);

	parents[0] = top_left;
	parents[1] = top_right;
	bottom_middle = clk_test_create_mux_with_ops(test,
						     parents, ARRAY_SIZE(parents),
						     &clk_multiple_parents_mux_ops_iterate_parent,
						     "bottom-middle",
						     0,
						     0);
	KUNIT_ASSERT_NOT_ERR_OR_NULL(test, bottom_middle);

	bottom_right = clk_kunit_create_ro_div(test,
						 top_right,
						 "bottom-right",
						 0,
						 1);
	KUNIT_ASSERT_NOT_ERR_OR_NULL(test, bottom_right);

	clk = clk_hw_get_clk(bottom_middle, NULL);
	KUNIT_ASSERT_NOT_ERR_OR_NULL(test, clk);

	req = clk_kunit_request_get(test, clk);
	KUNIT_ASSERT_NOT_ERR_OR_NULL(test, req);

	ret = clk_request_add_clock_rate(req, clk, 144000000);
	KUNIT_ASSERT_EQ(test, ret, 0);

	ret = clk_request_check(req);
	KUNIT_EXPECT_EQ(test, ret, 0);
	KUNIT_EXPECT_EQ(test, clk_request_len(req), 3);
	KUNIT_EXPECT_FALSE(test, clk_hw_is_in_request(top_left, req));
	KUNIT_EXPECT_FALSE(test, clk_hw_is_in_request(bottom_left, req));
	KUNIT_EXPECT_TRUE(test, clk_hw_is_in_request(top_right, req));
	KUNIT_EXPECT_TRUE(test, clk_hw_is_in_request(bottom_middle, req));
	KUNIT_EXPECT_TRUE(test, clk_hw_is_in_request(bottom_right, req));
}

/*
 * Test that if the clock that was the trigger of the request wants to
 * change its parent, and if it has SET_PARENT_RATE, the old clocks that
 * were there by side effect (its old parent, its old siblings) will no
 * longer be part of the request, and its new parent and siblings will
 * be.
 */
static void clk_request_test_reparent_set_rate(struct kunit *test)
{
	struct clk_hw *top_left, *top_right;
	struct clk_hw *bottom_left, *bottom_middle, *bottom_right;
	const struct clk_hw *parents[2];
	struct clk_request *req;
	struct clk *clk;
	int ret;

	top_left = clk_test_create_dummy(test,
					 "top-left",
					 0,
					 DUMMY_CLOCK_RATE_1);
	KUNIT_ASSERT_NOT_ERR_OR_NULL(test, top_left);

	top_right = clk_test_create_dummy(test,
					  "top-right",
					  0,
					  DUMMY_CLOCK_RATE_2);
	KUNIT_ASSERT_NOT_ERR_OR_NULL(test, top_right);

	bottom_left = clk_kunit_create_ro_div(test,
						top_left,
						"bottom-left",
						CLK_SET_RATE_PARENT,
						1);
	KUNIT_ASSERT_NOT_ERR_OR_NULL(test, bottom_left);

	parents[0] = top_left;
	parents[1] = top_right;
	bottom_middle = clk_test_create_mux_with_ops(test,
						     parents, ARRAY_SIZE(parents),
						     &clk_multiple_parents_mux_ops_iterate_parent,
						     "bottom-middle",
						     CLK_SET_RATE_PARENT,
						     0);
	KUNIT_ASSERT_NOT_ERR_OR_NULL(test, bottom_middle);

	bottom_right = clk_kunit_create_ro_div(test,
						 top_right,
						 "bottom-right",
						 CLK_SET_RATE_PARENT,
						 1);
	KUNIT_ASSERT_NOT_ERR_OR_NULL(test, bottom_right);

	clk = clk_hw_get_clk(bottom_middle, NULL);
	KUNIT_ASSERT_NOT_ERR_OR_NULL(test, clk);

	req = clk_kunit_request_get(test, clk);
	KUNIT_ASSERT_NOT_ERR_OR_NULL(test, req);

	ret = clk_request_add_clock_rate(req, clk, 144000000);
	KUNIT_ASSERT_EQ(test, ret, 0);

	ret = clk_request_check(req);
	KUNIT_EXPECT_EQ(test, ret, 0);
	KUNIT_EXPECT_EQ(test, clk_request_len(req), 3);
	KUNIT_EXPECT_FALSE(test, clk_hw_is_in_request(top_left, req));
	KUNIT_EXPECT_FALSE(test, clk_hw_is_in_request(bottom_left, req));
	KUNIT_EXPECT_TRUE(test, clk_hw_is_in_request(top_right, req));
	KUNIT_EXPECT_TRUE(test, clk_hw_is_in_request(bottom_middle, req));
	KUNIT_EXPECT_TRUE(test, clk_hw_is_in_request(bottom_right, req));
}

/*
 * Test that if the clock that was the trigger of the request wants to
 * change its parent, the old clocks that were there by side effect (its
 * old parent, its old siblings) will no longer be part of the request,
 * and its new parent and siblings will be.
 */
static void clk_request_test_reparent_3_parents(struct kunit *test)
{
	struct clk_hw *top_left, *top_center, *top_right;
	struct clk_hw *bottom_left, *bottom_center, *bottom_right;
	struct clk_dummy_context *top_right_ctx;
	struct clk_mux_context *bottom_center_ctx;
	struct clk_div_context *bottom_right_ctx;
	const struct clk_hw *parents[3];
	struct clk_request *req;
	struct clk *clk;
	int ret;

	top_left = clk_test_create_dummy(test,
					 "top-left",
					 0,
					 DUMMY_CLOCK_RATE_1);
	KUNIT_ASSERT_NOT_ERR_OR_NULL(test, top_left);

	top_center = clk_test_create_dummy(test,
					   "top-center",
					   0,
					   DUMMY_CLOCK_RATE_2);
	KUNIT_ASSERT_NOT_ERR_OR_NULL(test, top_center);

	top_right = clk_test_create_dummy(test,
					  "top-right",
					  0,
					  DUMMY_CLOCK_RATE_3);
	KUNIT_ASSERT_NOT_ERR_OR_NULL(test, top_right);
	top_right_ctx = hw_to_dummy(top_right);

	bottom_left = clk_kunit_create_ro_div(test,
					      top_left,
					      "bottom-left",
					      0,
					      1);
	KUNIT_ASSERT_NOT_ERR_OR_NULL(test, bottom_left);

	parents[0] = top_left;
	parents[1] = top_center;
	parents[2] = top_right;
	bottom_center = clk_test_create_mux_with_ops(test,
						     parents, ARRAY_SIZE(parents),
						     &clk_multiple_parents_mux_ops_iterate_parent,
						     "bottom-center",
						     0,
						     0);
	KUNIT_ASSERT_NOT_ERR_OR_NULL(test, bottom_center);
	bottom_center_ctx = hw_to_mux(bottom_center);

	bottom_right = clk_kunit_create_ro_div(test,
					       top_right,
					       "bottom-right",
					       0,
					       1);
	KUNIT_ASSERT_NOT_ERR_OR_NULL(test, bottom_right);
	bottom_right_ctx = hw_to_div(bottom_right);

	clk = clk_hw_get_clk(bottom_center, NULL);
	KUNIT_ASSERT_NOT_ERR_OR_NULL(test, clk);

	req = clk_kunit_request_get(test, clk);
	KUNIT_ASSERT_NOT_ERR_OR_NULL(test, req);

	ret = clk_request_add_clock_rate(req, clk, 144000000);
	KUNIT_ASSERT_EQ(test, ret, 0);

	ret = clk_request_check(req);
	KUNIT_EXPECT_EQ(test, ret, 0);
	KUNIT_EXPECT_EQ(test, clk_request_len(req), 3);
	KUNIT_EXPECT_FALSE(test, clk_hw_is_in_request(top_left, req));
	KUNIT_EXPECT_FALSE(test, clk_hw_is_in_request(top_center, req));
	KUNIT_EXPECT_FALSE(test, clk_hw_is_in_request(bottom_left, req));
	KUNIT_EXPECT_TRUE(test, clk_hw_is_in_request(top_right, req));
	KUNIT_EXPECT_TRUE(test, clk_hw_is_in_request(bottom_center, req));
	KUNIT_EXPECT_TRUE(test, clk_hw_is_in_request(bottom_right, req));
	KUNIT_EXPECT_GE(test, top_right_ctx->check_called, 1);
	KUNIT_EXPECT_GE(test, bottom_center_ctx->check_called, 1);
	KUNIT_EXPECT_GE(test, bottom_right_ctx->check_called, 1);
}

static void clk_request_test_12(struct kunit *test)
{
	/*
	 * TODO: Test that if a clock was part of a request but not the
	 * source of it (ie. child, or parent or sibling with
	 * SET_PARENT_RATE) wants to change its parent, and if it has
	 * SET_PARENT_RATE, we will its new parent the old clocks that were there by side
	 * effect (its old parent, its old siblings) will no longer be
	 * part of the request.
	 */
}

/*
 * TODO: Test that if a clock decides to change parent as a
 * fallout of the first request validation, and it doesn't have
 * CLK_SET_RATE_PARENT, that clock will be a second top-most
 * clock, with only its children in that tree.
 */
static void clk_request_test_reparent_separate_subtree(struct kunit *test)
{
	struct clk_hw *top_left, *top_right;
	struct clk_hw *bottom_left, *bottom_middle, *bottom_right;
	const struct clk_hw *parents[2];
	struct clk_request *req;
	struct clk *clk;
	int ret;

	top_left = clk_test_create_dummy(test,
					 "top-left",
					 0,
					 DUMMY_CLOCK_RATE_1);
	KUNIT_ASSERT_NOT_ERR_OR_NULL(test, top_left);

	top_right = clk_test_create_dummy(test,
					  "top-right",
					  0,
					  DUMMY_CLOCK_RATE_2);
	KUNIT_ASSERT_NOT_ERR_OR_NULL(test, top_right);

	bottom_left = clk_kunit_create_ro_div(test,
						top_left,
						"bottom-left",
						0,
						1);
	KUNIT_ASSERT_NOT_ERR_OR_NULL(test, bottom_left);

	parents[0] = top_left;
	parents[1] = top_right;
	bottom_middle = clk_test_create_mux_with_ops(test,
						     parents, ARRAY_SIZE(parents),
						     &clk_multiple_parents_mux_ops_iterate_parent,
						     "bottom-middle",
						     0,
						     0);
	KUNIT_ASSERT_NOT_ERR_OR_NULL(test, bottom_middle);

	bottom_right = clk_kunit_create_ro_div(test,
						 top_right,
						 "bottom-right",
						 0,
						 1);
	KUNIT_ASSERT_NOT_ERR_OR_NULL(test, bottom_right);

	clk = clk_hw_get_clk(top_left, NULL);
	KUNIT_ASSERT_NOT_ERR_OR_NULL(test, clk);

	req = clk_kunit_request_get(test, clk);
	KUNIT_ASSERT_NOT_ERR_OR_NULL(test, req);

	ret = clk_request_add_clock_rate(req, clk, 144000000);
	KUNIT_ASSERT_EQ(test, ret, 0);

	ret = clk_request_check(req);
	KUNIT_EXPECT_EQ(test, ret, 0);
	KUNIT_EXPECT_EQ(test, clk_request_len(req), 3);
	KUNIT_EXPECT_TRUE(test, clk_hw_is_in_request(top_left, req));
	KUNIT_EXPECT_TRUE(test, clk_hw_is_in_request(bottom_left, req));
	KUNIT_EXPECT_TRUE(test, clk_hw_is_in_request(bottom_middle, req));
	KUNIT_EXPECT_FALSE(test, clk_hw_is_in_request(top_right, req));
	KUNIT_EXPECT_FALSE(test, clk_hw_is_in_request(bottom_right, req));
}

static void clk_request_test_reparent_separate_subtree_set_rate(struct kunit *test)
{
	struct clk_hw *top_left, *top_right;
	struct clk_hw *bottom;
	const struct clk_hw *parents[2];
	struct clk_request *req;
	struct clk *clk;
	int ret;

	top_left = clk_test_create_dummy(test,
					 "top-left",
					 0,
					 DUMMY_CLOCK_RATE_1);
	KUNIT_ASSERT_NOT_ERR_OR_NULL(test, top_left);

	top_right = clk_test_create_dummy(test,
					  "top-right",
					  0,
					  DUMMY_CLOCK_RATE_2);
	KUNIT_ASSERT_NOT_ERR_OR_NULL(test, top_right);

	parents[0] = top_left;
	parents[1] = top_right;
	bottom = clk_test_create_mux_with_ops(test,
					      parents, ARRAY_SIZE(parents),
					      &clk_multiple_parents_mux_ops_iterate_parent,
					      "bottom",
					      CLK_SET_RATE_PARENT,
					      0);
	KUNIT_ASSERT_NOT_ERR_OR_NULL(test, bottom);

	clk = clk_hw_get_clk(top_left, NULL);
	KUNIT_ASSERT_NOT_ERR_OR_NULL(test, clk);

	req = clk_kunit_request_get(test, clk);
	KUNIT_ASSERT_NOT_ERR_OR_NULL(test, req);

	ret = clk_request_add_clock_rate(req, clk, 144000000);
	KUNIT_ASSERT_EQ(test, ret, 0);

	ret = clk_request_check(req);
	KUNIT_EXPECT_EQ(test, ret, 0);
	KUNIT_EXPECT_EQ(test, clk_request_len(req), 3);
	KUNIT_EXPECT_TRUE(test, clk_hw_is_in_request(top_left, req));
	KUNIT_EXPECT_TRUE(test, clk_hw_is_in_request(top_right, req));
	KUNIT_EXPECT_TRUE(test, clk_hw_is_in_request(bottom, req));
}

#define HDMI_TEST_FREQ	(297 * FREQ_1MHZ)
#define TCON0_TEST_FREQ	(200 * FREQ_1MHZ)

/*
 * TODO: Look at the A64 exact use case and add a test for that one
 * https://lore.kernel.org/linux-clk/20230825-pll-mipi_keep_rate-v1-0-35bc43570730@oltmanns.dev/
 */
static void clk_request_test_allwinner_dual_display(struct kunit *test)
{
	struct clk_request *req;
	struct clk_hw *pll_video0;
	struct clk_hw *pll_mipi;
	struct clk_hw *tcon0;
	struct clk_hw *hdmi;
	struct clk *tcon0_clk;
	struct clk *hdmi_clk;
	int ret;

	pll_video0 = clk_test_create_dummy(test,
					   "pll-video0",
					   0,
					   294 * FREQ_1MHZ);
	KUNIT_ASSERT_NOT_ERR_OR_NULL(test, pll_video0);
	KUNIT_ASSERT_EQ(test, clk_hw_get_rate(pll_video0), 294 * FREQ_1MHZ);

	pll_mipi = clk_test_create_frac(test,
					pll_video0,
					"pll_mipi",
					CLK_SET_RATE_PARENT,
					2, 1);
	KUNIT_ASSERT_NOT_ERR_OR_NULL(test, pll_mipi);
	KUNIT_ASSERT_EQ(test, clk_hw_get_rate(pll_mipi), 588 * FREQ_1MHZ);

	tcon0 = clk_kunit_create_div(test,
				     pll_mipi,
				     "tcon0",
				     CLK_SET_RATE_PARENT,
				     1);
	KUNIT_ASSERT_NOT_ERR_OR_NULL(test, tcon0);

	hdmi = clk_kunit_create_div(test,
				    pll_video0,
				    "hdmi",
				    CLK_SET_RATE_PARENT,
				    1);
	KUNIT_ASSERT_NOT_ERR_OR_NULL(test, hdmi);

	tcon0_clk = clk_hw_get_clk(tcon0, NULL);
	KUNIT_ASSERT_NOT_ERR_OR_NULL(test, tcon0_clk);

	req = clk_kunit_request_get(test, tcon0_clk);
	KUNIT_ASSERT_NOT_ERR_OR_NULL(test, req);

	ret = clk_request_add_clock_rate(req, tcon0_clk, TCON0_TEST_FREQ);
	KUNIT_ASSERT_EQ(test, ret, 0);

	ret = clk_request_check(req);
	KUNIT_ASSERT_EQ(test, ret, 0);

	KUNIT_EXPECT_EQ(test, clk_request_len(req), 4);
	KUNIT_EXPECT_TRUE(test, clk_hw_is_in_request(pll_video0, req));
	KUNIT_EXPECT_TRUE(test, clk_hw_is_in_request(pll_mipi, req));
	KUNIT_EXPECT_TRUE(test, clk_hw_is_in_request(hdmi, req));
	KUNIT_EXPECT_TRUE(test, clk_hw_is_in_request(tcon0, req));

	hdmi_clk = clk_hw_get_clk(hdmi, NULL);
	KUNIT_ASSERT_NOT_ERR_OR_NULL(test, hdmi_clk);

	req = clk_kunit_request_get(test, hdmi_clk);
	KUNIT_ASSERT_NOT_ERR_OR_NULL(test, req);

	ret = clk_request_add_clock_rate(req, hdmi_clk, HDMI_TEST_FREQ);
	KUNIT_ASSERT_EQ(test, ret, 0);

	ret = clk_request_check(req);
	KUNIT_ASSERT_EQ(test, ret, 0);

	KUNIT_EXPECT_EQ(test, clk_request_len(req), 4);
	KUNIT_EXPECT_TRUE(test, clk_hw_is_in_request(pll_video0, req));
	KUNIT_EXPECT_TRUE(test, clk_hw_is_in_request(pll_mipi, req));
	KUNIT_EXPECT_TRUE(test, clk_hw_is_in_request(hdmi, req));
	KUNIT_EXPECT_TRUE(test, clk_hw_is_in_request(tcon0, req));
}

/*
 * TODO: Look at the A64 exact use case and add a test for that one
 * https://lore.kernel.org/linux-clk/20230825-pll-mipi_keep_rate-v1-0-35bc43570730@oltmanns.dev/
 */
static void clk_request_test_allwinner_dual_display_checked(struct kunit *test)
{
	struct clk_dummy_context *pll_video0_ctx;
	struct clk_frac_context *pll_mipi_ctx;
	struct clk_div_context *tcon0_ctx;
	struct clk_div_context *hdmi_ctx;
	struct clk_request *req;
	struct clk_hw *pll_video0;
	struct clk_hw *pll_mipi;
	struct clk_hw *tcon0;
	struct clk_hw *hdmi;
	struct clk *tcon0_clk;
	struct clk *hdmi_clk;
	int ret;

	pll_video0 = clk_test_create_dummy(test,
					   "pll-video0",
					   0,
					   294 * FREQ_1MHZ);
	KUNIT_ASSERT_NOT_ERR_OR_NULL(test, pll_video0);
	KUNIT_ASSERT_EQ(test, clk_hw_get_rate(pll_video0), 294 * FREQ_1MHZ);
	pll_video0_ctx = hw_to_dummy(pll_video0);

	pll_mipi = clk_test_create_frac(test,
					pll_video0,
					"pll_mipi",
					CLK_SET_RATE_PARENT,
					2, 1);
	KUNIT_ASSERT_NOT_ERR_OR_NULL(test, pll_mipi);
	KUNIT_ASSERT_EQ(test, clk_hw_get_rate(pll_mipi), 588 * FREQ_1MHZ);
	pll_mipi_ctx = hw_to_frac(pll_mipi);

	tcon0 = clk_kunit_create_div(test,
				     pll_mipi,
				     "tcon0",
				     CLK_SET_RATE_PARENT,
				     1);
	KUNIT_ASSERT_NOT_ERR_OR_NULL(test, tcon0);
	tcon0_ctx = hw_to_div(tcon0);

	hdmi = clk_kunit_create_div(test,
				    pll_video0,
				    "hdmi",
				    CLK_SET_RATE_PARENT,
				    1);
	KUNIT_ASSERT_NOT_ERR_OR_NULL(test, hdmi);
	hdmi_ctx = hw_to_div(hdmi);

	tcon0_clk = clk_hw_get_clk(tcon0, NULL);
	KUNIT_ASSERT_NOT_ERR_OR_NULL(test, tcon0_clk);

	req = clk_kunit_request_get(test, tcon0_clk);
	KUNIT_ASSERT_NOT_ERR_OR_NULL(test, req);

	ret = clk_request_add_clock_rate(req, tcon0_clk, TCON0_TEST_FREQ);
	KUNIT_ASSERT_EQ(test, ret, 0);

	ret = clk_request_check(req);
	KUNIT_ASSERT_EQ(test, ret, 0);

	KUNIT_EXPECT_GE(test, pll_video0_ctx->check_called, 1);
	KUNIT_EXPECT_GE(test, pll_mipi_ctx->check_called, 1);
	KUNIT_EXPECT_GE(test, tcon0_ctx->check_called, 1);
	KUNIT_EXPECT_GE(test, hdmi_ctx->check_called, 1);

	hdmi_clk = clk_hw_get_clk(hdmi, NULL);
	KUNIT_ASSERT_NOT_ERR_OR_NULL(test, hdmi_clk);

	req = clk_kunit_request_get(test, hdmi_clk);
	KUNIT_ASSERT_NOT_ERR_OR_NULL(test, req);

	ret = clk_request_add_clock_rate(req, hdmi_clk, HDMI_TEST_FREQ);
	KUNIT_ASSERT_EQ(test, ret, 0);

	ret = clk_request_check(req);
	KUNIT_ASSERT_EQ(test, ret, 0);

	KUNIT_EXPECT_GE(test, pll_video0_ctx->check_called, 1);
	KUNIT_EXPECT_GE(test, pll_mipi_ctx->check_called, 1);
	KUNIT_EXPECT_GE(test, tcon0_ctx->check_called, 1);
	KUNIT_EXPECT_GE(test, hdmi_ctx->check_called, 1);
}

/*
 * TODO: Look at the A64 exact use case and add a test for that one
 * https://lore.kernel.org/linux-clk/20230825-pll-mipi_keep_rate-v1-0-35bc43570730@oltmanns.dev/
 */
static void clk_request_test_allwinner_dual_display_rate(struct kunit *test)
{
	struct clk_request *req;
	struct clk_hw *pll_video0;
	struct clk_hw *pll_mipi;
	struct clk_hw *tcon0;
	struct clk_hw *hdmi;
	struct clk *tcon0_clk;
	struct clk *hdmi_clk;
	int ret;

	pll_video0 = clk_test_create_dummy(test,
					   "pll-video0",
					   0,
					   294 * FREQ_1MHZ);
	KUNIT_ASSERT_NOT_ERR_OR_NULL(test, pll_video0);
	KUNIT_ASSERT_EQ(test, clk_hw_get_rate(pll_video0), 294 * FREQ_1MHZ);

	pll_mipi = clk_test_create_frac(test,
					pll_video0,
					"pll_mipi",
					CLK_SET_RATE_PARENT,
					2, 1);
	KUNIT_ASSERT_NOT_ERR_OR_NULL(test, pll_mipi);
	KUNIT_ASSERT_EQ(test, clk_hw_get_rate(pll_mipi), 588 * FREQ_1MHZ);

	tcon0 = clk_kunit_create_div(test,
				     pll_mipi,
				     "tcon0",
				     CLK_SET_RATE_PARENT,
				     1);
	KUNIT_ASSERT_NOT_ERR_OR_NULL(test, tcon0);

	hdmi = clk_kunit_create_div(test,
				    pll_video0,
				    "hdmi",
				    CLK_SET_RATE_PARENT,
				    1);
	KUNIT_ASSERT_NOT_ERR_OR_NULL(test, hdmi);

	tcon0_clk = clk_hw_get_clk(tcon0, NULL);
	KUNIT_ASSERT_NOT_ERR_OR_NULL(test, tcon0_clk);

	req = clk_kunit_request_get(test, tcon0_clk);
	KUNIT_ASSERT_NOT_ERR_OR_NULL(test, req);

	ret = clk_request_add_clock_rate(req, tcon0_clk, TCON0_TEST_FREQ);
	KUNIT_ASSERT_EQ(test, ret, 0);

	ret = clk_request_commit(req);
	KUNIT_ASSERT_EQ(test, ret, 0);

	KUNIT_EXPECT_EQ(test, clk_hw_get_rate(tcon0), 200 * FREQ_1MHZ);

	hdmi_clk = clk_hw_get_clk(hdmi, NULL);
	KUNIT_ASSERT_NOT_ERR_OR_NULL(test, hdmi_clk);

	req = clk_kunit_request_get(test, hdmi_clk);
	KUNIT_ASSERT_NOT_ERR_OR_NULL(test, req);

	ret = clk_request_add_clock_rate(req, hdmi_clk, HDMI_TEST_FREQ);
	KUNIT_ASSERT_EQ(test, ret, 0);

	ret = clk_request_commit(req);
	KUNIT_ASSERT_EQ(test, ret, 0);

	KUNIT_EXPECT_EQ(test, clk_hw_get_rate(hdmi), 297 * FREQ_1MHZ);
	KUNIT_EXPECT_EQ(test, clk_hw_get_rate(tcon0), 200 * FREQ_1MHZ);
}

static void clk_request_test_14(struct kunit *test)
{
	/*
	 * TODO: Look at the iMX8MP exact use case and add a test for that one
	 * https://lore.kernel.org/linux-clk/20230918-imx8mp-dtsi-v1-0-1d008b3237c0@skidata.com/
	 */
}

static struct kunit_case clk_request_test_cases[] = {
	KUNIT_CASE(clk_request_test_lone_clock),
	KUNIT_CASE(clk_request_test_lone_clock_checked),
	KUNIT_CASE(clk_request_test_lone_clock_change_parent_rate),
	KUNIT_CASE(clk_request_test_lone_clock_change_parent_rate_checked),
	KUNIT_CASE(clk_request_test_lone_clock_set_rate),
	KUNIT_CASE(clk_request_test_lone_clock_set_rate_checked),
	KUNIT_CASE(clk_request_test_lone_mux_clock),
	KUNIT_CASE(clk_request_test_parent_clock),
	KUNIT_CASE(clk_request_test_parent_clock_3_levels),
	KUNIT_CASE(clk_request_test_reparent),
	KUNIT_CASE(clk_request_test_reparent_3_parents),
	KUNIT_CASE(clk_request_test_reparent_set_rate),
	KUNIT_CASE(clk_request_test_reparent_separate_subtree),
	KUNIT_CASE(clk_request_test_reparent_separate_subtree_set_rate),
	KUNIT_CASE(clk_request_test_siblings_clocks_set_rate),
	KUNIT_CASE(clk_request_test_siblings_3_levels_set_rate_all_levels),
	KUNIT_CASE(clk_request_test_siblings_3_levels_set_rate_last_level),
	KUNIT_CASE(clk_request_test_single_clock_checked),
	KUNIT_CASE(clk_request_test_4),
	KUNIT_CASE(clk_request_test_12),
	KUNIT_CASE(clk_request_test_allwinner_dual_display),
	KUNIT_CASE(clk_request_test_allwinner_dual_display_checked),
	KUNIT_CASE(clk_request_test_allwinner_dual_display_rate),
	KUNIT_CASE(clk_request_test_14),
	{}
};

static struct kunit_suite clk_request_test_suite = {
	.name = "clk_request",
	.test_cases = clk_request_test_cases,
};

kunit_test_suite(clk_request_test_suite);
