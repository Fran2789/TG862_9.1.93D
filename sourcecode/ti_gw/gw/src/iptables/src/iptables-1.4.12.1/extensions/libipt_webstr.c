/* Shared library add-on to iptables to add string matching support.  
 *  
 * Copyright (C) 2000 Emmanuel Roger  <winfield@freegates.be> 
 * 
 * ChangeLog 
 *     27.01.2001: Gianni Tedesco <gianni@ecsc.co.uk> 
 *             Changed --tos to --string in save(). Also 
 *             updated to work with slightly modified 
 *             ipt_string_info. 
 */ 
 
/* Shared library add-on to iptables to add webstr matching support.  
 * 
 * Copyright (C) 2003, CyberTAN Corporation 
 * All Rights Reserved. 
 * 
 * Description: 
 *   This is shared library, added to iptables, for web content inspection.  
 *   It was derived from 'string' matching support, declared as above. 
 * 
 */ 
 
/* Shared library add-on to iptables to add webstr matching support.
 * 
 *
 ***********************************************************************8
 Includes Intel Corporation's changes/modifications dated: 02.11.2011 
 Changed/modified portions : 
                            X-tables API adapted for kernel 2.6.39
 Copyright © 2011, Intel Corporation.   
*******************************************************************************
 * Copyright (C) 2011, Intel Ltd.
 * All Rights Reserved.
 *
 * ChangeLog 
 *      10.08.2011: modified to kernel 2.6.18
        02.11.2011: modified to kernel 2.6.39
 */ 
 
#include <stdio.h> 
#include <netdb.h> 
#include <string.h> 
#include <stdlib.h> 
#include <getopt.h> 
 
#include <iptables.h> 
#include <linux/netfilter_ipv4/ipt_webstr.h> 
 
/* Function which prints out usage message. */ 
static void 
webstr_help(void) 
{ 
        printf( 
"WEBSTR match v%s options:\n" 
"--webstr [!] host            Match a http string in a packet\n" 
"--webstr [!] url             Match a http string in a packet\n" 
"--webstr [!] content         Match a http string in a packet\n", 
IPTABLES_VERSION); 
 
        fputc('\n', stdout); 
} 
 
static struct option webstr_opts[] = { 
        { "host", 1, NULL, '1' }, 
        { "url", 1, NULL, '2' }, 
        { "content", 1, NULL, '3' }, 
        { .name = NULL } 
}; 
 
 
static void 
parse_string(const char *s, struct ipt_webstr_info *info) 
{        
        if (strlen(s) <= BM_MAX_NLEN) strcpy(info->string, s); 
        else xtables_error(PARAMETER_PROBLEM, "WEBSTR too long `%s'", s); 
} 

/* Function which parses command options; returns true if it 
   ate an option */ 
static int 
webstr_parse(int c, char **argv, int invert, unsigned int *flags, 
      const void *entry, 
      struct xt_entry_match **match) 
{ 
        struct ipt_webstr_info *stringinfo = (struct ipt_webstr_info *)(*match)->data; 
 
        switch (c) { 
        case '1':
                parse_string(argv[optind-1], stringinfo); 
                if (invert) 
                        stringinfo->invert = 1; 
                stringinfo->len=strlen((char *)&stringinfo->string); 
                stringinfo->type = IPT_WEBSTR_HOST; 
                break; 
 
        case '2': 
                parse_string(argv[optind-1], stringinfo); 
                if (invert) 
                        stringinfo->invert = 1; 
                stringinfo->len=strlen((char *)&stringinfo->string); 
                stringinfo->type = IPT_WEBSTR_URL; 
                break; 
 
        case '3': 
                parse_string(argv[optind-1], stringinfo); 
                if (invert) 
                        stringinfo->invert = 1; 
                stringinfo->len=strlen((char *)&stringinfo->string); 
                stringinfo->type = IPT_WEBSTR_CONTENT; 
                break; 
 
        default: 
                return 0; 
        } 
 
        *flags = 1; 
        return 1; 
} 
 
static void 
print_string(char string[], int invert, int numeric) 
{ 
 
        if (invert) 
                fputc('!', stdout); 
        printf("%s ",string); 
} 
 
/* Final check; must have specified --string. */ 
static void 
webstr_final_check(unsigned int flags) 
{ 
        if (!flags) 
                xtables_error(PARAMETER_PROBLEM, 
                           "WEBSTR match: You must specify `--webstr'"); 
} 
 
/* Prints out the matchinfo. */ 
static void 
webstr_print(const void *ip, 
      const struct xt_entry_match *match, 
      int numeric) 
{ 
        struct ipt_webstr_info *stringinfo = (struct ipt_webstr_info *)match->data; 
 
        printf(" WEBSTR match "); 
 
         
        switch (stringinfo->type) { 
        case IPT_WEBSTR_HOST: 
                printf("host "); 
                break; 
 
        case IPT_WEBSTR_URL: 
                printf("url "); 
                break; 
 
        case IPT_WEBSTR_CONTENT: 
                printf("content "); 
                break; 
 
        default: 
                printf("ERROR "); 
                break; 
        } 
 
        print_string(((struct ipt_webstr_info *)match->data)->string, 
                  ((struct ipt_webstr_info *)match->data)->invert, numeric); 
} 

/* Saves the union ipt_matchinfo in parsable form to stdout. */ 
static void 
webstr_save(const void *ip, const struct xt_entry_match *match) 
{ 
        struct ipt_webstr_info *stringinfo = (struct ipt_webstr_info *)match->data; 
        
        switch (stringinfo->type) 
        { 
        case IPT_WEBSTR_HOST: 
            printf(" --host "); 
            break; 
 
        case IPT_WEBSTR_URL: 
            printf(" --url "); 
            break; 
 
        case IPT_WEBSTR_CONTENT: 
            printf(" --content "); 
            break; 
 
        default: 
            printf("ERROR "); 
            break; 
        }
 
        print_string(((struct ipt_webstr_info *)match->data)->string, 
                  ((struct ipt_webstr_info *)match->data)->invert, 0); 
} 


static struct xtables_match webstr = {  
        .name           = "webstr", 
        .version        = XTABLES_VERSION,
	 	.family		    = NFPROTO_IPV4,
        .size           = XT_ALIGN(sizeof(struct ipt_webstr_info)), 
        .userspacesize  = XT_ALIGN(sizeof(struct ipt_webstr_info)), 
        .help           = webstr_help, 
        .parse          = webstr_parse, 
        .final_check    = webstr_final_check, 
        .print          = webstr_print, 
        .save           = webstr_save, 
        .extra_opts     = webstr_opts, 
}; 
 
void _init(void) 
{ 
        xtables_register_match(&webstr);
} 
