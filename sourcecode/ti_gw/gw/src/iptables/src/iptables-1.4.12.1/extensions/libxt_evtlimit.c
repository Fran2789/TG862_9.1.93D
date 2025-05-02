/* Shared library add-on to iptables to add ARRIS evtlimit support.
 *
 * ARRIS
 */
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <xtables.h>
#include <linux/netfilter/x_tables.h>
#include <linux/netfilter/xt_evtlimit.h>

#define XT_EVTLIMIT_THRESHOLD   	100
#define XT_EVTLIMIT_PASS_PERCENT	10
#define XT_EVTLIMIT_MAX_COUNT       10000

#define EQUAL_ERROR                 (0.01)
#define EQUAL(a, b) ((a) > (b) ? ((a) - (b) <= EQUAL_ERROR) : ((b) - (a) <= EQUAL_ERROR))
#define abs(_v) ((_v) < 0 ? -(_v) : (_v))

enum {
	O_THRESHOLD = 0,
	O_PASSPERCENT,
	O_TYPE,
	O_MAXCOUNT,
};

static void evtlimit_help(void)
{
	printf(
"evtlimit match options:\n"
"  --threshold number            event limit threshold: default %u\n"
"                                [Packets exceeds this number will be dropped\n"
"                                at specified percentage]\n"
"  --percent   number            percent number of matching packets which exceeds\n"
"                                specified threshold, default:%u%%\n"
"  --type         number         event type\n"
"  --max-count    number         max counter per event type\n\n",
XT_EVTLIMIT_THRESHOLD,
XT_EVTLIMIT_PASS_PERCENT);
}

static const struct xt_option_entry evtlimit_opts[] = {
	{.name = "threshold", .id = O_THRESHOLD, .type = XTTYPE_UINT32, .flags = XTOPT_PUT, XTOPT_POINTER(struct xt_evtrateinfo, threshold)},
    {.name = "type", .id = O_TYPE, .type = XTTYPE_UINT32, .flags = XTOPT_PUT, XTOPT_POINTER(struct xt_evtrateinfo, type)},
    {.name = "max-count", .id = O_MAXCOUNT, .type = XTTYPE_UINT32, .flags = XTOPT_PUT, XTOPT_POINTER(struct xt_evtrateinfo, max_count)},
	{.name = "percent", .id = O_PASSPERCENT, .type = XTTYPE_UINT32, .flags = XTOPT_PUT, XTOPT_POINTER(struct xt_evtrateinfo, percent),
	 .min = 0, .max = 100},
	XTOPT_TABLEEND,
};

struct fraction
{
    unsigned short n;
    unsigned short d;
};

static struct fraction evtlimit_rates[]=
{
    {1,100} /*1%*/, {1, 50} /*2%*/, {1, 20}/*5%*/, {1, 10} /*10%*/, {1, 7} /*15%*/,
    {1, 5} /*20%*/, {1, 4} /*25%*/,{3, 10}/*30%*/, {2, 5} /*40%*/, {1, 2} /*50%*/,
    {3, 5} /*60%*/, {7,10} /*70%*/,{4,  5}/*80%*/, {9,10} /*90%*/, {1, 1} /*100%*/
};

static void evtlimit_init(struct xt_entry_match *m)
{
	struct xt_evtrateinfo *r = (struct xt_evtrateinfo *)m->data;

    memset(r, 0, sizeof(*r));
	r->threshold = XT_EVTLIMIT_THRESHOLD;
    r->percent = XT_EVTLIMIT_PASS_PERCENT;
    r->type = 0;
    r->max_count = XT_EVTLIMIT_MAX_COUNT;
}

static void evtlimit_parse(struct xt_option_call *cb)
{
    struct xt_evtrateinfo *r = cb->data;
	xtables_option_parse(cb);

    float pass_rate, error, min_error;
    int n, i;
    
    /* calculate */
    pass_rate = r->percent / 100.;
    min_error = 1.;
    n = 0;

    for (i = 0; i < ARRAY_SIZE(evtlimit_rates); ++i)
    {
        error = ((float)evtlimit_rates[i].n) / evtlimit_rates[i].d;
        if (EQUAL(error, pass_rate))
        {
            n = i;
            break;
        }

        error = abs(error - pass_rate);
        if (error < min_error)
        {
            min_error = error;
            n = i;
        }
    }

    r->count1 = evtlimit_rates[n].n;
    r->count2 = evtlimit_rates[n].d;
    
}

static void evtlimit_print(const void *ip, const struct xt_entry_match *match, int numeric)
{
	const struct xt_evtrateinfo *r = (const void *)match->data;
	printf(" threshold %u", r->threshold);
	printf(" percent %u", r->percent);
    printf(" type %u", r->type);
    printf(" max-count %u", r->max_count);
}

static void evtlimit_save(const void *ip, const struct xt_entry_match *match)
{
	const struct xt_evtrateinfo *r = (const void *)match->data;

	printf(" --threshold %u", r->threshold);
    printf(" --percent %u", r->percent);
    printf(" --type %u", r->type);
    printf(" --max-count %u", r->max_count);
	//if (r->burst != XT_LIMIT_BURST)
		//printf(" --limit-burst %u", r->burst);
}

static struct xtables_match evtlimit_match = {
	.family		= NFPROTO_UNSPEC,
	.name		= "evtlimit",
	.version	= XTABLES_VERSION,
	.size		= XT_ALIGN(sizeof(struct xt_evtrateinfo)),
	.userspacesize	= offsetof(struct xt_evtrateinfo, count1),
	.help		= evtlimit_help,
	.init		= evtlimit_init,
	.x6_parse	= evtlimit_parse,
	.print		= evtlimit_print,
	.save		= evtlimit_save,
	.x6_options	= evtlimit_opts,
};

void _init(void)
{
	xtables_register_match(&evtlimit_match);
}

