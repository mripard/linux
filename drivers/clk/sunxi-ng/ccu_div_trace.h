
#undef TRACE_SYSTEM
#define TRACE_SYSTEM clk-sunxi

/*
 * TRACE_SYSTEM_VAR default to TRACE_SYSTEM, but must be a valid C
 * variable.
 * This isn't exported to the userspace.
 */
#undef TRACE_SYSTEM_VAR
#define TRACE_SYSTEM_VAR clk_sunxi

#if !defined(_TRACE_CLK_SUNXI_DIV_H) || defined(TRACE_HEADER_MULTI_READ)
#define _TRACE_CLK_SUNXI_DIV_H

#include <linux/tracepoint.h>

#include "ccu_div.h"

TRACE_EVENT(clk_sunxi_div_recalc_rate,
	TP_PROTO(struct ccu_div *cd, unsigned long rate),
	TP_ARGS(cd, rate),
	TP_STRUCT__entry(
		__string(name, cd->common.hw.init->name)
		__field(unsigned long, rate)
	),
	TP_fast_assign(
		__assign_str(name, cd->common.hw.init->name);
		__entry->rate = rate;
	),
	TP_printk("clk: %s rate: %luHz",
		  __get_str(name), __entry->rate)
);

TRACE_EVENT(clk_sunxi_div_round_rate,
	TP_PROTO(struct ccu_div *cd, unsigned long req,
		 unsigned long rate),
	TP_ARGS(cd, req, rate),
	TP_STRUCT__entry(
		__string(name, cd->common.hw.init->name)
		__field(unsigned long, req)
		__field(unsigned long, rate)
	),
	TP_fast_assign(
		__assign_str(name, cd->common.hw.init->name);
		__entry->req = req;
		__entry->rate = rate;
	),
	TP_printk("clk: %s req rate: %luHz rate: %luHz",
		  __get_str(name), __entry->req, __entry->rate)
);

TRACE_EVENT(clk_sunxi_div_set_rate,
	TP_PROTO(struct ccu_div *cd, unsigned long rate,
		 unsigned long parent, int div),
	TP_ARGS(cd, rate, parent, div),
	TP_STRUCT__entry(
		__string(name, cd->common.hw.init->name)
		__field(unsigned long, rate)
		__field(unsigned long, parent)
		__field(int, div)
	),
	TP_fast_assign(
		__assign_str(name, cd->common.hw.init->name);
		__entry->rate = rate;
		__entry->parent = parent;
		__entry->div = div;
	),
	TP_printk("clk: %s rate: %luHz (%lu / %d)",
		  __get_str(name), __entry->rate, __entry->parent, __entry->div)
);

#endif /* _TRACE_CLK_SUNXI_DIV_H */

#undef TRACE_INCLUDE_PATH
#define TRACE_INCLUDE_PATH .
#undef TRACE_INCLUDE_FILE
#define TRACE_INCLUDE_FILE ccu_div_trace

/* This part must be outside protection */
#include <trace/define_trace.h>
