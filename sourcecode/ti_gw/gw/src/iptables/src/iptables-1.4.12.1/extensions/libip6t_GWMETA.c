/* 
 * 
  GPL LICENSE SUMMARY

  Copyright(c) 2011 Intel Corporation.

  This program is free software; you can redistribute it and/or modify 
  it under the terms of version 2 of the GNU General Public License as
  published by the Free Software Foundation.

  This program is distributed in the hope that it will be useful, but 
  WITHOUT ANY WARRANTY; without even the implied warranty of 
  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU 
  General Public License for more details.

  You should have received a copy of the GNU General Public License 
  along with this program; if not, write to the Free Software 
  Foundation, Inc., 51 Franklin St - Fifth Floor, Boston, MA 02110-1301 USA.
  The full GNU General Public License is included in this distribution 
  in the file called LICENSE.GPL.

  Contact Information:
    Intel Corporation
    2200 Mission College Blvd.
    Santa Clara, CA  97052
*/
/*
 * Iptables extension module to set flags in ti_gw_meta field of skb.
 *
 * Description:
 *   This module provide extends netfilter feature to set a bitmask on a packets.
 *   In difference from netfiltyer this masks lasts till packet leaves GateWay TCP stack 
 *
 *   The module follows the Netfilter framework, called extended packet
 *   matching modules.
 * Usage :
 *  iptables -t mangle -I POSTROUTING 1 -o rndbr1 -p tcp --dport 6800:6866 -j GWMETA --gwmeta-gwmask 0x0000002
 *  iptables -t mangle -I OUTPUT 1 -o rndbr1 -p tcp --dport 6800:6866 -j GWMETA --gwmeta-gwmask 0x00000002
 *  Packet will have resulted ti_gw_meta ORed mask 0x22 
 */

#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <getopt.h>
#include <xtables.h>
#include <linux/netfilter_ipv6/ip6_tables.h>
#include <linux/netfilter_ipv6/ip6t_GWMETA.h>


#define IP6T_GWMETA_VALID_FLAGS_USED    (1) 


/* Function which prints out usage message. */

static void GWMETA_help(void)
{
	printf( "GWMETA  options:\n"
               " --gwmeta-gwmask 32bit hexadecmal  value\n"
               " --dis-pp no pp session\n"
			  );
}
/* man 3 getopt structure */
static struct option GWMETA_opts[] = {
	{ "gwmeta-gwmask", 1, NULL, '1' },
	{ "dis-pp", 0, NULL, '2'},
	{ "en-pp", 0, NULL, '3'},
	{ .name = NULL }

};



/* Function which parses command options; returns 1 if options are succesfully parsed  */
static int 
GWMETA_parse(int c, char **argv, int invert, unsigned int *flags,
      const void *entry,
      struct xt_entry_target **target)
{
    struct ip6t_gw_skb_rule_info *info = (struct ip6t_gw_skb_rule_info *) (*target)->data;

	if  ( info == NULL )
	{
		xtables_error(PARAMETER_PROBLEM, "NULL paramaeter (*target)->data specified" );
		return 0;
	}

    /* Parse argument number 1, 2,3, 4 */
	switch (c) 
    {
	case '1':  

		if  ( optarg == NULL )
		{
			xtables_error(PARAMETER_PROBLEM, "Mandatory parameter passed as NULL" );
			return 0;  /* EXIT_FAILURE */
		}
		int inp_mask = strtol(optarg, NULL ,16 );
		if (inp_mask <= 0 )
		{
			xtables_error(PARAMETER_PROBLEM,"Invalid GW MASK  specified");
			return 0;
		}
		info->gwmask |= inp_mask;
		info->type = IP6T_GW_META;
		*flags |= IP6T_GWMETA_VALID_FLAGS_USED;
		break;
	case '2':
		info->type = IP6T_GW_DIS_PP;
		*flags |= IP6T_GWMETA_VALID_FLAGS_USED;
		break;
	case '3':
		info->type = IP6T_GW_EN_PP;
		*flags |= IP6T_GWMETA_VALID_FLAGS_USED;
		break;

	default:
		return 0;
	}
	return 1;
}


/* Prints out the targinfo. iptables -L  */

static void GWMETA_print(const void * ip, const struct xt_entry_target *target,
      int numeric)
{
	struct ip6t_gw_skb_rule_info *info = (struct ip6t_gw_skb_rule_info *)target->data;

	printf(" GWMETA ");

	if (info)
	{
		if (info->type == IP6T_GW_META)
		{
			printf("--gwmeta-gwmask:0x%X", info->gwmask);
		}
		else if (info->type == IP6T_GW_DIS_PP)
		{
			printf("--dis-pp");
		}		
		else if (info->type == IP6T_GW_EN_PP)
		{
			printf("--en-pp");
		}		



	}
}
/*
*
* last chance for sanity check. 
* It's called when the user enter a new rule, right after arguments parsing is done.
*  
*/

static void GWMETA_final_check(unsigned int flags)
{
	if (!(flags & IP6T_GWMETA_VALID_FLAGS_USED))
		xtables_error(PARAMETER_PROBLEM,
				"GWMETA: You must specify an parameters");
}
/*
* If we have a ruleset that we want to save, iptables provide the tool 'iptables-save' which dumps all your rules. It obviously needs your extension's help to dump proper rules.
* This is done by calling this function.
*
* 
*/
static void GWMETA_save(const void *ip, const struct xt_entry_target *target)
{
	struct ip6t_gw_skb_rule_info *info = (struct ip6t_gw_skb_rule_info *)target->data;

	if (info)
	{
		if (info->type == IP6T_GW_META)
		{
			printf(" --gwmeta-gwmask 0x%X" , info->gwmask );
		}
		else if (info->type == IP6T_GW_DIS_PP)
		{
			printf(" --dis-pp");
		}
		else if (info->type == IP6T_GW_EN_PP)
		{
			printf(" --en-pp");
		}

	}
}


static struct xtables_target gwmeta_tg6_reg = {
        .name           = "GWMETA",
        .version        = XTABLES_VERSION,
        .family         = NFPROTO_IPV6,
        .size           = XT_ALIGN(sizeof(struct ip6t_gw_skb_rule_info)),
        .userspacesize  = XT_ALIGN(sizeof(struct ip6t_gw_skb_rule_info)),
        .help           = GWMETA_help,
        .parse          = GWMETA_parse,
        .final_check	= GWMETA_final_check,
        .print          = GWMETA_print,
        .save           = GWMETA_save,
        .extra_opts     = GWMETA_opts,
};

void _init(void)
{
/*
*
*  This function is called when the module is loaded by iptables.
*   man dlopen.
*/
	xtables_register_target(&gwmeta_tg6_reg);
}

