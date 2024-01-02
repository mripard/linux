#include <kunit/test.h>
#include <linux/clk.h>
#include <linux/clk-provider.h>

#define DUMMY_CLOCK_INIT_RATE	(42 * 1000 * 1000)
#define DUMMY_CLOCK_RATE_1	(142 * 1000 * 1000)
#define DUMMY_CLOCK_RATE_2	(242 * 1000 * 1000)

struct clk_dummy_context {
	struct clk_hw hw;
	unsigned long rate;
	unsigned int check_called;
};

#define hw_to_dummy(_hw) \
	container_of_const(_hw, struct clk_dummy_context, hw)

static int clk_dummy_check_request(struct clk_hw *hw,
				   struct clk_request *req)
{
	struct clk_dummy_context *ctx = hw_to_dummy(hw);

	ctx->check_called++;

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

struct clk_div_context {
	struct clk_hw hw;
	unsigned int div;
	unsigned int check_called;
};

#define hw_to_div(_hw) \
	container_of_const(_hw, struct clk_div_context, hw)

static int clk_div_check_request(struct clk_hw *hw,
				   struct clk_request *req)
{
	struct clk_div_context *ctx = hw_to_div(hw);

	ctx->check_called++;

	return 0;
}

static unsigned long clk_div_recalc_rate(struct clk_hw *hw,
					 unsigned long parent_rate)
{
	struct clk_div_context *ctx = hw_to_div(hw);

	return parent_rate / ctx->div;
}

static const struct clk_ops clk_div_ro_ops = {
	.check_request = clk_div_check_request,
	.recalc_rate = clk_div_recalc_rate,
};

struct clk_mux_context {
	struct clk_hw hw;
	unsigned int current_parent;
	unsigned int check_called;
};

#define hw_to_mux(_hw) \
	container_of_const(_hw, struct clk_mux_context, hw)

static int clk_mux_check_request(struct clk_hw *hw,
				   struct clk_request *req)
{
	struct clk_mux_context *ctx = hw_to_mux(hw);

	ctx->check_called++;

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

KUNIT_DEFINE_ACTION_WRAPPER(clk_hw_unregister_wrapper,
			    clk_hw_unregister,
			    struct clk_hw *);

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
clk_test_create_dummy_div(struct kunit *test,
			  const struct clk_hw *parent,
			  const char *name,
			  unsigned long flags,
			  unsigned int div)
{
	const struct clk_init_data data = {
		.flags = flags,
		.name = name,
		.parent_hws = &parent,
		.num_parents = 1,
		.ops = &clk_div_ro_ops,
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

static struct clk_hw *
clk_test_create_mux(struct kunit *test,
		    const struct clk_hw	**parent_hws, size_t num_parents,
		    const char *name,
		    unsigned long flags,
		    unsigned int default_parent)
{
	const struct clk_init_data data = {
		.flags = flags,
		.name = name,
		.parent_hws = parent_hws,
		.num_parents = num_parents,
		.ops = &clk_multiple_parents_mux_ops,
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

KUNIT_DEFINE_ACTION_WRAPPER(clk_request_put_wrapper,
			    clk_request_put,
			    struct clk_request *);

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

	child = clk_test_create_dummy_div(test,
					  parent,
					  "test",
					  0,
					  1);
	KUNIT_ASSERT_NOT_ERR_OR_NULL(test, child);

	clk = clk_hw_get_clk(child, NULL);
	KUNIT_ASSERT_NOT_ERR_OR_NULL(test, clk);

	req = clk_request_get(clk);
	KUNIT_ASSERT_NOT_ERR_OR_NULL(test, req);

	ret = kunit_add_action_or_reset(test, &clk_request_put_wrapper, req);
	KUNIT_ASSERT_EQ(test, ret, 0);

	ret = clk_request_add_clock_rate(req, clk, 144000000);
	KUNIT_ASSERT_EQ(test, ret, 0);

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

	req = clk_request_get(clk);
	KUNIT_ASSERT_NOT_ERR_OR_NULL(test, req);

	ret = kunit_add_action_or_reset(test, &clk_request_put_wrapper, req);
	KUNIT_ASSERT_EQ(test, ret, 0);

	ret = clk_request_add_clock_rate(req, clk, 144000000);
	KUNIT_ASSERT_EQ(test, ret, 0);

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

	req = clk_request_get(clk);
	KUNIT_ASSERT_NOT_ERR_OR_NULL(test, req);

	ret = kunit_add_action_or_reset(test, &clk_request_put_wrapper, req);
	KUNIT_ASSERT_EQ(test, ret, 0);

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

	child = clk_test_create_dummy_div(test,
					  parent,
					  "test",
					  0,
					  1);
	KUNIT_ASSERT_NOT_ERR_OR_NULL(test, child);
	child_ctx = hw_to_div(child);

	clk = clk_hw_get_clk(child, NULL);
	KUNIT_ASSERT_NOT_ERR_OR_NULL(test, clk);

	req = clk_request_get(clk);
	KUNIT_ASSERT_NOT_ERR_OR_NULL(test, req);

	ret = kunit_add_action_or_reset(test, &clk_request_put_wrapper, req);
	KUNIT_ASSERT_EQ(test, ret, 0);

	ret = clk_request_add_clock_rate(req, clk, 144000000);
	KUNIT_ASSERT_EQ(test, ret, 0);
	KUNIT_ASSERT_EQ(test, clk_request_len(req), 1);

	ret = clk_request_check(req);
	KUNIT_EXPECT_EQ(test, ret, 0);
	KUNIT_EXPECT_EQ(test, parent_ctx->check_called, 0);
	KUNIT_EXPECT_EQ(test, child_ctx->check_called, 1);
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

	child = clk_test_create_dummy_div(test,
					  parent,
					  "test",
					  CLK_SET_RATE_PARENT,
					  1);
	KUNIT_ASSERT_NOT_ERR_OR_NULL(test, child);
	child_ctx = hw_to_div(child);

	clk = clk_hw_get_clk(child, NULL);
	KUNIT_ASSERT_NOT_ERR_OR_NULL(test, clk);

	req = clk_request_get(clk);
	KUNIT_ASSERT_NOT_ERR_OR_NULL(test, req);

	ret = kunit_add_action_or_reset(test, &clk_request_put_wrapper, req);
	KUNIT_ASSERT_EQ(test, ret, 0);

	ret = clk_request_add_clock_rate(req, clk, 144000000);
	KUNIT_ASSERT_EQ(test, ret, 0);
	KUNIT_ASSERT_EQ(test, clk_request_len(req), 2);

	ret = clk_request_check(req);
	KUNIT_EXPECT_EQ(test, ret, 0);
	KUNIT_EXPECT_EQ(test, parent_ctx->check_called, 1);
	KUNIT_EXPECT_EQ(test, child_ctx->check_called, 1);
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

	child = clk_test_create_dummy_div(test,
					  parent,
					  "test",
					  CLK_SET_RATE_PARENT,
					  1);
	KUNIT_ASSERT_NOT_ERR_OR_NULL(test, child);

	clk = clk_hw_get_clk(child, NULL);
	KUNIT_ASSERT_NOT_ERR_OR_NULL(test, clk);

	req = clk_request_get(clk);
	KUNIT_ASSERT_NOT_ERR_OR_NULL(test, req);

	ret = kunit_add_action_or_reset(test, &clk_request_put_wrapper, req);
	KUNIT_ASSERT_EQ(test, ret, 0);

	ret = clk_request_add_clock_rate(req, clk, 144000000);
	KUNIT_ASSERT_EQ(test, ret, 0);

	KUNIT_EXPECT_EQ(test, clk_request_len(req), 2);
	KUNIT_EXPECT_TRUE(test, clk_hw_is_in_request(child, req));
	KUNIT_EXPECT_TRUE(test, clk_hw_is_in_request(parent, req));
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

	child_1 = clk_test_create_dummy_div(test,
					    parent,
					    "test-1",
					    0,
					    1);
	KUNIT_ASSERT_NOT_ERR_OR_NULL(test, child_1);

	child_2 = clk_test_create_dummy_div(test,
					    parent,
					    "test-2",
					    0,
					    1);
	KUNIT_ASSERT_NOT_ERR_OR_NULL(test, child_2);

	clk = clk_hw_get_clk(parent, NULL);
	KUNIT_ASSERT_NOT_ERR_OR_NULL(test, clk);

	req = clk_request_get(clk);
	KUNIT_ASSERT_NOT_ERR_OR_NULL(test, req);

	ret = kunit_add_action_or_reset(test, &clk_request_put_wrapper, req);
	KUNIT_ASSERT_EQ(test, ret, 0);

	ret = clk_request_add_clock_rate(req, clk, 144000000);
	KUNIT_ASSERT_EQ(test, ret, 0);

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

	middle = clk_test_create_dummy_div(test,
					   top,
					   "middle",
					   CLK_SET_RATE_PARENT,
					   1);
	KUNIT_ASSERT_NOT_ERR_OR_NULL(test, middle);

	bottom = clk_test_create_dummy_div(test,
					   middle,
					   "bottom",
					   0,
					   1);
	KUNIT_ASSERT_NOT_ERR_OR_NULL(test, bottom);

	clk = clk_hw_get_clk(middle, NULL);
	KUNIT_ASSERT_NOT_ERR_OR_NULL(test, clk);

	req = clk_request_get(clk);
	KUNIT_ASSERT_NOT_ERR_OR_NULL(test, req);

	ret = kunit_add_action_or_reset(test, &clk_request_put_wrapper, req);
	KUNIT_ASSERT_EQ(test, ret, 0);

	ret = clk_request_add_clock_rate(req, clk, 144000000);
	KUNIT_ASSERT_EQ(test, ret, 0);

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

	child_1 = clk_test_create_dummy_div(test,
					    parent,
					    "test-1",
					    CLK_SET_RATE_PARENT,
					    1);
	KUNIT_ASSERT_NOT_ERR_OR_NULL(test, child_1);

	child_2 = clk_test_create_dummy_div(test,
					    parent,
					    "test-2",
					    CLK_SET_RATE_PARENT,
					    1);
	KUNIT_ASSERT_NOT_ERR_OR_NULL(test, child_2);

	clk = clk_hw_get_clk(child_1, NULL);
	KUNIT_ASSERT_NOT_ERR_OR_NULL(test, clk);

	req = clk_request_get(clk);
	KUNIT_ASSERT_NOT_ERR_OR_NULL(test, req);

	ret = kunit_add_action_or_reset(test, &clk_request_put_wrapper, req);
	KUNIT_ASSERT_EQ(test, ret, 0);

	ret = clk_request_add_clock_rate(req, clk, 144000000);
	KUNIT_ASSERT_EQ(test, ret, 0);

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

	middle_left = clk_test_create_dummy_div(test,
						top,
						"middle-left",
						0,
						1);
	KUNIT_ASSERT_NOT_ERR_OR_NULL(test, middle_left);

	middle_right = clk_test_create_dummy_div(test,
						 top,
						 "middle-right",
						 0,
						 1);
	KUNIT_ASSERT_NOT_ERR_OR_NULL(test, middle_right);

	bottom_left_left =
		clk_test_create_dummy_div(test,
					  middle_left,
					  "bottom-left-left",
					  CLK_SET_RATE_PARENT,
					  1);
	KUNIT_ASSERT_NOT_ERR_OR_NULL(test, bottom_left_left);

	bottom_left_right =
		clk_test_create_dummy_div(test,
					  middle_left,
					  "bottom-left-right",
					  CLK_SET_RATE_PARENT,
					  1);
	KUNIT_ASSERT_NOT_ERR_OR_NULL(test, bottom_left_right);

	bottom_right_left =
		clk_test_create_dummy_div(test,
					  middle_right,
					  "bottom-right-left",
					  CLK_SET_RATE_PARENT,
					  1);
	KUNIT_ASSERT_NOT_ERR_OR_NULL(test, bottom_right_left);

	bottom_right_right =
		clk_test_create_dummy_div(test,
					  middle_right,
					  "bottom-right-right",
					  CLK_SET_RATE_PARENT,
					  1);
	KUNIT_ASSERT_NOT_ERR_OR_NULL(test, bottom_right_right);

	clk = clk_hw_get_clk(bottom_left_left, NULL);
	KUNIT_ASSERT_NOT_ERR_OR_NULL(test, clk);

	req = clk_request_get(clk);
	KUNIT_ASSERT_NOT_ERR_OR_NULL(test, req);

	ret = kunit_add_action_or_reset(test, &clk_request_put_wrapper, req);
	KUNIT_ASSERT_EQ(test, ret, 0);

	ret = clk_request_add_clock_rate(req, clk, 144000000);
	KUNIT_ASSERT_EQ(test, ret, 0);

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

	middle_left = clk_test_create_dummy_div(test,
						top,
						"middle-left",
						CLK_SET_RATE_PARENT,
						1);
	KUNIT_ASSERT_NOT_ERR_OR_NULL(test, middle_left);

	middle_right = clk_test_create_dummy_div(test,
						 top,
						 "middle-right",
						 CLK_SET_RATE_PARENT,
						 1);
	KUNIT_ASSERT_NOT_ERR_OR_NULL(test, middle_right);

	bottom_left_left =
		clk_test_create_dummy_div(test,
					  middle_left,
					  "bottom-left-left",
					  CLK_SET_RATE_PARENT,
					  1);
	KUNIT_ASSERT_NOT_ERR_OR_NULL(test, bottom_left_left);

	bottom_left_right =
		clk_test_create_dummy_div(test,
					  middle_left,
					  "bottom-left-right",
					  CLK_SET_RATE_PARENT,
					  1);
	KUNIT_ASSERT_NOT_ERR_OR_NULL(test, bottom_left_right);

	bottom_right_left =
		clk_test_create_dummy_div(test,
					  middle_right,
					  "bottom-right-left",
					  CLK_SET_RATE_PARENT,
					  1);
	KUNIT_ASSERT_NOT_ERR_OR_NULL(test, bottom_right_left);

	bottom_right_right =
		clk_test_create_dummy_div(test,
					  middle_right,
					  "bottom-right-right",
					  CLK_SET_RATE_PARENT,
					  1);
	KUNIT_ASSERT_NOT_ERR_OR_NULL(test, bottom_right_right);

	clk = clk_hw_get_clk(bottom_left_left, NULL);
	KUNIT_ASSERT_NOT_ERR_OR_NULL(test, clk);

	req = clk_request_get(clk);
	KUNIT_ASSERT_NOT_ERR_OR_NULL(test, req);

	ret = kunit_add_action_or_reset(test, &clk_request_put_wrapper, req);
	KUNIT_ASSERT_EQ(test, ret, 0);

	ret = clk_request_add_clock_rate(req, clk, 144000000);
	KUNIT_ASSERT_EQ(test, ret, 0);

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

static void clk_request_test_9(struct kunit *test)
{
	/*
	 * TODO: Test that if a clock decides to change parent as a
	 * fallout of the first request validation, and it doesn't have
	 * CLK_SET_RATE_PARENT, only that clock will be in the request.
	 */
}

static void clk_request_test_10(struct kunit *test)
{
	/*
	 * TODO: Test that if a clock decides to change parent as a
	 * fallout of the first request validation, and it doesn't have
	 * CLK_SET_RATE_PARENT, that clock will be a second top-most
	 * clock, with only its children in that tree.
	 */
}

static void clk_request_test_11(struct kunit *test)
{
	/*
	 * TODO: Test that if the clock that was the trigger of the
	 * request wants to change its parent, and if it has
	 * SET_PARENT_RATE, the old clocks that were there by side
	 * effect (its old parent, its old siblings) will no longer be
	 * part of the request, and its new parent and siblings will be.
	 */
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

static void clk_request_test_13(struct kunit *test)
{
	/*
	 * TODO: Look at the A64 exact use case and add a test for that one
	 * https://lore.kernel.org/linux-clk/20230825-pll-mipi_keep_rate-v1-0-35bc43570730@oltmanns.dev/
	 */
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
	KUNIT_CASE(clk_request_test_lone_clock_set_rate),
	KUNIT_CASE(clk_request_test_lone_clock_set_rate_checked),
	KUNIT_CASE(clk_request_test_lone_mux_clock),
	KUNIT_CASE(clk_request_test_parent_clock),
	KUNIT_CASE(clk_request_test_parent_clock_3_levels),
	KUNIT_CASE(clk_request_test_siblings_clocks_set_rate),
	KUNIT_CASE(clk_request_test_siblings_3_levels_set_rate_all_levels),
	KUNIT_CASE(clk_request_test_siblings_3_levels_set_rate_last_level),
	KUNIT_CASE(clk_request_test_single_clock_checked),
	KUNIT_CASE(clk_request_test_4),
	KUNIT_CASE(clk_request_test_9),
	KUNIT_CASE(clk_request_test_10),
	KUNIT_CASE(clk_request_test_11),
	KUNIT_CASE(clk_request_test_12),
	KUNIT_CASE(clk_request_test_13),
	KUNIT_CASE(clk_request_test_14),
	{}
};

static struct kunit_suite clk_request_test_suite = {
	.name = "clk_request",
	.test_cases = clk_request_test_cases,
};

kunit_test_suite(clk_request_test_suite);
