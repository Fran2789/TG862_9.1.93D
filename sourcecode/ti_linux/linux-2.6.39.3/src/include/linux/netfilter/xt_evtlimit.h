#ifndef _XT_EVT_LIMIT_H
#define _XT_EVT_LIMIT_H

#include <linux/types.h>

/* timings are in milliseconds. */
#define XT_LIMIT_SCALE 10000

struct xt_evtlimit_priv;

/* 1/10,000 sec period => max of 10,000/sec.  Min rate is then 429490
   seconds, or one every 59 hours. */
struct xt_evtrateinfo {
	__u32 threshold;
    __u32 type;
    __u32 max_count; /* per type */
	__u32 percent;   /* pass percentage */

	/* Used internally by the kernel */
    __u16 count1, count2;
	struct xt_evtlimit_priv *master;
};
#endif /*_XT_EVT_LIMIT_H*/

