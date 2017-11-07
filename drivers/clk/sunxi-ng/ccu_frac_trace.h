
#undef TRACE_SYSTEM
#define TRACE_SYSTEM clk-sunxi

/*
 * TRACE_SYSTEM_VAR default to TRACE_SYSTEM, but must be a valid C
 * variable.
 * This isn't exported to the userspace.
 */
#undef TRACE_SYSTEM_VAR
#define TRACE_SYSTEM_VAR clk_sunxi

#if !defined(_TRACE_CLK_SUNXI_FRAC_H) || defined(TRACE_HEADER_MULTI_READ)
#define _TRACE_CLK_SUNXI_FRAC_H

#include <linux/tracepoint.h>

#include "ccu_common.h"

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

DEFINE_EVENT(clk_sunxi_common_template, clk_sunxi_frac_enable,
	TP_PROTO(struct ccu_common *common),
	TP_ARGS(common));

DEFINE_EVENT(clk_sunxi_common_template, clk_sunxi_frac_disable,
	TP_PROTO(struct ccu_common *common),
	TP_ARGS(common));

TRACE_EVENT(clk_sunxi_frac_read_rate,
	TP_PROTO(struct ccu_common *common, struct ccu_frac_internal *cf,
		 bool select),
	TP_ARGS(common, cf, select),
	TP_STRUCT__entry(
		__string(name, common->hw.init->name)
		__field(struct ccu_frac_internal *, cf)
		__field(bool, select)
	),
	TP_fast_assign(
		__assign_str(name, common->hw.init->name);
		__entry->cf = cf;
		__entry->select = select;
	),
	TP_printk("%s rates %lu[%c] / %lu[%c]", __get_str(name),
		  __entry->cf->rates[0], __entry->select == 0 ? '*' : ' ',
		  __entry->cf->rates[1], __entry->select == 1 ? '*' : ' ')
);

#endif /* _TRACE_CLK_SUNXI_FRAC_H */

#undef TRACE_INCLUDE_PATH
#define TRACE_INCLUDE_PATH .
#undef TRACE_INCLUDE_FILE
#define TRACE_INCLUDE_FILE ccu_frac_trace

/* This part must be outside protection */
#include <trace/define_trace.h>
