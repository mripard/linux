#include <kunit/test.h>
#include <linux/clk.h>
#include <linux/clk-provider.h>

#define DUMMY_CLOCK_INIT_RATE	(42 * 1000 * 1000)
#define DUMMY_CLOCK_RATE_1	(142 * 1000 * 1000)
#define DUMMY_CLOCK_RATE_2	(242 * 1000 * 1000)

struct clk_core;
struct clk_request;

struct clk_request *
clk_core_start_request(struct clk_core *core);
int clk_request_set_rate(struct clk_request *req,
			 unsigned long long rate);
bool clk_core_is_in_request(struct clk_core *core,
			    struct clk_request *req);


struct clk_dummy_context {
	struct clk_hw hw;
	unsigned long rate;
};

static unsigned long clk_dummy_recalc_rate(struct clk_hw *hw,
					   unsigned long parent_rate)
{
	struct clk_dummy_context *ctx =
		container_of(hw, struct clk_dummy_context, hw);

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
	struct clk_dummy_context *ctx =
		container_of(hw, struct clk_dummy_context, hw);

	ctx->rate = rate;
	return 0;
}

static const struct clk_ops clk_dummy_rate_ops = {
	.recalc_rate = clk_dummy_recalc_rate,
	.determine_rate = clk_dummy_determine_rate,
	.set_rate = clk_dummy_set_rate,
};

struct clk_div_context {
	struct clk_hw hw;
	unsigned int div;
};

static unsigned long clk_div_recalc_rate(struct clk_hw *hw,
					 unsigned long parent_rate)
{
	struct clk_div_context *ctx =
		container_of(hw, struct clk_div_context, hw);

	return parent_rate / ctx->div;
}

static const struct clk_ops clk_div_ro_ops = {
	.recalc_rate = clk_div_recalc_rate,
};

struct clk_mux_ctx {
	struct clk_hw hw;
	unsigned int current_parent;
};

static int clk_multiple_parents_mux_set_parent(struct clk_hw *hw, u8 index)
{
	struct clk_mux_ctx *ctx = container_of(hw, struct clk_mux_ctx, hw);

	if (index >= clk_hw_get_num_parents(hw))
		return -EINVAL;

	ctx->current_parent = index;

	return 0;
}

static u8 clk_multiple_parents_mux_get_parent(struct clk_hw *hw)
{
	struct clk_mux_ctx *ctx = container_of(hw, struct clk_mux_ctx, hw);

	return ctx->current_parent;
}

static const struct clk_ops clk_multiple_parents_mux_ops = {
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
	struct clk_mux_ctx *ctx;
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

/*
 * Test that a clock that has a rate request for it and doesn't have
 * SET_RATE_PARENT will only affect itself, and none of its parent.
 */
static void clk_request_test_lone_clock(struct kunit *test)
{
	struct clk_hw *parent, *child;
	struct clk_request *req;
	struct clk *clk;
	unsigned int len;
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

	req = clk_start_request(clk);
	KUNIT_ASSERT_NOT_ERR_OR_NULL(test, req);

	ret = clk_request_add_clock_rate(req, clk, 144000000);
	KUNIT_ASSERT_EQ(test, ret, 0);

	len = clk_request_len(req);
	KUNIT_EXPECT_EQ(test, len, 1);
	KUNIT_EXPECT_TRUE(test, clk_is_in_request(clk, req));
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
	unsigned int len;
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

	req = clk_start_request(clk);
	KUNIT_ASSERT_NOT_ERR_OR_NULL(test, req);

	ret = clk_request_add_clock_rate(req, clk, 144000000);
	KUNIT_ASSERT_EQ(test, ret, 0);

	len = clk_request_len(req);
	KUNIT_EXPECT_EQ(test, len, 1);
	KUNIT_EXPECT_TRUE(test, clk_is_in_request(clk, req));
}

static void clk_request_test_2(struct kunit *test)
{
	/*
	 * TODO: Test that a clock that has a rate request for it and
	 * doesn't have SET_RATE_PARENT will have its check_request
	 * function called.
	 */
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
	unsigned int len;
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

	req = clk_start_request(clk);
	KUNIT_ASSERT_NOT_ERR_OR_NULL(test, req);

	ret = clk_request_add_clock_rate(req, clk, 144000000);
	KUNIT_ASSERT_EQ(test, ret, 0);

	len = clk_request_len(req);
	KUNIT_EXPECT_EQ(test, len, 2);
	KUNIT_EXPECT_TRUE(test, clk_hw_is_in_request(parent, req));
	KUNIT_EXPECT_TRUE(test, clk_is_in_request(clk, req));
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
	unsigned int len;
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

	req = clk_start_request(clk);
	KUNIT_ASSERT_NOT_ERR_OR_NULL(test, req);

	ret = clk_request_add_clock_rate(req, clk, 144000000);
	KUNIT_ASSERT_EQ(test, ret, 0);

	len = clk_request_len(req);
	KUNIT_EXPECT_EQ(test, len, 3);
	KUNIT_EXPECT_TRUE(test, clk_is_in_request(clk, req));
	KUNIT_EXPECT_TRUE(test, clk_hw_is_in_request(parent, req));
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

	req = clk_start_request(clk);
	KUNIT_ASSERT_NOT_ERR_OR_NULL(test, req);

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

	req = clk_start_request(clk);
	KUNIT_ASSERT_NOT_ERR_OR_NULL(test, req);

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

static void clk_request_test_5(struct kunit *test)
{
	/*
	 * TODO: Test that a clock that has a rate request for it and
	 * has SET_RATE_PARENT is part of the request, will have its
	 * check_request function called and its parent too.
	 */
}

static void clk_request_test_6(struct kunit *test)
{
	/*
	 * TODO: Test that a clock that has a rate request for it and
	 * has SET_RATE_PARENT is part of the request, and its parent,
	 * and all its parent children are part of it.
	 */
}

static void clk_request_test_7(struct kunit *test)
{
	/*
	 * TODO: Test that a clock that has a rate request for it and
	 * has SET_RATE_PARENT is part of the request, and its parent,
	 * and all its parent children are part of it.
	 */
}

static void clk_request_test_8(struct kunit *test)
{
	/*
	 * TODO: Test that a clock that has a rate request for it and
	 * has SET_RATE_PARENT is part of the request, will have its
	 * check_request function called and its parent too.
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
	 * fallout of the first request validation, and it has
	 * CLK_SET_RATE_PARENT, that clock, its parent and all its
	 * siblings will be in the request.
	 */
}

static struct kunit_case clk_request_test_cases[] = {
	KUNIT_CASE(clk_request_test_lone_clock),
	KUNIT_CASE(clk_request_test_lone_clock_set_rate),
	KUNIT_CASE(clk_request_test_lone_mux_clock),
	KUNIT_CASE(clk_request_test_siblings_clocks_set_rate),
	KUNIT_CASE(clk_request_test_siblings_3_levels_set_rate_all_levels),
	KUNIT_CASE(clk_request_test_siblings_3_levels_set_rate_last_level),
	KUNIT_CASE(clk_request_test_2),
	KUNIT_CASE(clk_request_test_4),
	KUNIT_CASE(clk_request_test_5),
	KUNIT_CASE(clk_request_test_6),
	KUNIT_CASE(clk_request_test_7),
	KUNIT_CASE(clk_request_test_8),
	KUNIT_CASE(clk_request_test_9),
	KUNIT_CASE(clk_request_test_10),
	{}
};

static struct kunit_suite clk_request_test_suite = {
	.name = "clk_request",
	.test_cases = clk_request_test_cases,
};

kunit_test_suite(clk_request_test_suite);
