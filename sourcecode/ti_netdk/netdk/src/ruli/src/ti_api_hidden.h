/*
 * ti_api_hidden.h
 *
 * TI API Wrappers and Data Structures for Host Name / SRV RR Query Submissions.
 *      1. ti_init_dnsclient
 *      2. ti_query_hostname
 *      3. ti_query_srvrr
 *      4. ti_exit_dnsclient
 *      5. ti_dns_start_query_scheduler
 *      6. ti_dns_parse_hostname_answer
 *      7. ti_dns_parse_srvrr_answer
 *      8. ti_dns_clean_hostname_query
 *      9. ti_dns_clean_srvrr_query
 *
 * Copyright (C) 2009 Texas Instruments Incorporated - http://www.ti.com/
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License as
 * published by the Free Software Foundation version 2.
 *
 * This program is distributed “as is” WITHOUT ANY WARRANTY of any
 * kind, whether express or implied; without even the implied warranty
 * of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the
 * GNU General Public License for more details.
*/

/**************************************************************************
 * Data structure definitions.
 *************************************************************************/

/* Data structure to maintain resolver settings */
typedef struct {
    char*                   interface;      /* Interface to use for sending DNS queries */

    /* RULI Settings */
    unsigned int            timeout;        /* Timeout in seconds for a DNS query */
    unsigned int            retry;          /* Number of times each server configured is tried */
    ruli_conf_handler_t*    handler;        /* CONF file handler */
    ruli_search_res_t*      search_res;     /* RULI Search context */
    ruli_res_t*             ruli_res_ctx;   /* RULI Resolver context */
    ruli_list_t*            server_list;    /* List of DNS servers to use */

    /* LIBOOP Settings */
    oop_source_sys*         source_sys;     /* OOP System Source */ 
    oop_source*             source;         /* OOP Source - Event registration interface */
} ti_resolver_ctx_t;
