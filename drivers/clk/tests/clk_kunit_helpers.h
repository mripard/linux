#ifndef CLK_KUNIT_HELPERS_H_
#define CLK_KUNIT_HELPERS_H_

#include <kunit/test.h>

#include <linux/clk-provider.h>

#define FREQ_1KHZ		1000
#define FREQ_1MHZ		(1000 * FREQ_1KHZ)

KUNIT_DEFINE_ACTION_WRAPPER(clk_hw_unregister_wrapper,
			    clk_hw_unregister,
			    struct clk_hw *);

struct clk_div_context {
	struct clk_hw hw;
	unsigned int div;
	unsigned int check_called;
};

#define hw_to_div(_hw) \
	container_of_const(_hw, struct clk_div_context, hw)

extern const struct clk_ops clk_div_ops;
extern const struct clk_ops clk_div_ro_ops;
extern const struct clk_ops clk_div_modify_parent_ops;

struct clk_hw *clk_kunit_create_div_with_ops(struct kunit *test,
					     const struct clk_hw *parent,
					     const struct clk_ops *ops,
					     const char *name,
					     unsigned long flags,
					     unsigned int div);

struct clk_hw *clk_kunit_create_div(struct kunit *test,
				    const struct clk_hw *parent,
				    const char *name,
				    unsigned long flags,
				    unsigned int div);

struct clk_hw *clk_kunit_create_ro_div(struct kunit *test,
				       const struct clk_hw *parent,
				       const char *name,
				       unsigned long flags,
				       unsigned int div);

#define CLK_KUNIT_MUX_ITERATE_PARENT		BIT(0)
#define CLK_KUNIT_MUX_CHANGE_PARENT_RATE	BIT(1)

struct clk_mux_context {
	struct clk_hw hw;
	unsigned long flags;
	unsigned int current_parent;
	unsigned int check_called;
};

#define hw_to_mux(_hw) \
	container_of_const(_hw, struct clk_mux_context, hw)

struct clk_hw *clk_test_create_mux(struct kunit *test,
				   const struct clk_hw **parent_hws, size_t num_parents,
				   const char *name,
				   unsigned long flags,
				   unsigned long mux_flags,
				   unsigned int default_parent);

#endif // CLK_KUNIT_HELPERS_H_
