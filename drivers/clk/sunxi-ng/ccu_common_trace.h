
#undef TRACE_SYSTEM
#define TRACE_SYSTEM clk-sunxi

/*
 * TRACE_SYSTEM_VAR default to TRACE_SYSTEM, but must be a valid C
 * variable.
 * This isn't exported to the userspace.
 */
#undef TRACE_SYSTEM_VAR
#define TRACE_SYSTEM_VAR clk_sunxi

#if !defined(_TRACE_CLK_SUNXI_COMMON_H) || defined(TRACE_HEADER_MULTI_READ)
#define _TRACE_CLK_SUNXI_COMMON_H

#include <linux/tracepoint.h>

#include "ccu_common.h"

DECLARE_EVENT_CLASS(clk_hw_template,
	TP_PROTO(struct clk_hw *hw),
	TP_ARGS(hw),
	TP_STRUCT__entry(
		__string(name, hw->init->name)
	),
	TP_fast_assign(
		__assign_str(name, hw->init->name);
	),
	TP_printk("%s", __get_str(name))
);

DECLARE_EVENT_CLASS(clk_sunxi_common_template,
	TP_PROTO(struct ccu_common *common),
	TP_ARGS(common),
	TP_STRUCT__entry(
		__string(name, common->hw.init->name)
	),
	TP_fast_assign(
		__assign_str(name, common->hw.init->name);
	),
	TP_printk("%s", __get_str(name))
);

DEFINE_EVENT(clk_hw_template, clk_sunxi_register,
	TP_PROTO(struct clk_hw *hw),
	TP_ARGS(hw));

DEFINE_EVENT(clk_sunxi_common_template, clk_sunxi_lock,
	TP_PROTO(struct ccu_common *common),
	TP_ARGS(common));

DEFINE_EVENT(clk_sunxi_common_template, clk_sunxi_lock_complete,
	TP_PROTO(struct ccu_common *common),
	TP_ARGS(common));

DEFINE_EVENT(clk_sunxi_common_template, clk_sunxi_cpu_pll_reset,
	TP_PROTO(struct ccu_common *common),
	TP_ARGS(common));

#endif /* _TRACE_CLK_SUNXI_COMMON_H */

#undef TRACE_INCLUDE_PATH
#define TRACE_INCLUDE_PATH .
#undef TRACE_INCLUDE_FILE
#define TRACE_INCLUDE_FILE ccu_common_trace

/* This part must be outside protection */
#include <trace/define_trace.h>
