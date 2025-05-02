/*
 * ti_api.h
 *
 * TI API Wrappers for Host Name / SRV RR Query Submissions.
 *      1. ti_init_dnsclient
 *      2. ti_query_hostname
 *      3. ti_query_srvrr
 *      4. ti_exit_dnsclient
 *      5. ti_dns_start_query_scheduler
 *      6. ti_dns_parse_hostname_answer
 *      7. ti_dns_parse_srvrr_answer
 *      8. ti_dns_clean_hostname_query
 *      9. ti_dns_clean_srvrr_query
 *     10. ti_dns_get_hostname_qid
 *     11. ti_dns_get_hostname_resctx
 *     12. ti_dns_get_srvrr_qid
 *     13. ti_dns_get_srvrr_resctx
 *     14. ti_dns_get_resctx
 *     15. ti_dns_cancel_hostname_query
 *     16. ti_dns_cancel_srvrr_query
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
 * Constant definitions.
 *************************************************************************/
#define QBUFSZ RULI_LIMIT_DNAME_TEXT_BUFSZ

/**************************************************************************
 * FUNCTION NAME : ti_dns_query_srvrr
 **************************************************************************
 * DESCRIPTION   :
 * Given a resolver context handle, SRV tuple(_service._protocol.domain) to 
 * query, and a callback function to notify on any event, this function 
 * parses all the input parameters, validates them and submits the query for 
 * processing. On success, returns the query id of the query which is a 
 * positive number (>=0) and -1 otherwise.
 *************************************************************************/
#define ti_dns_query_srvrr(ti_res_handle, domain, call) \
    ti_dns_general_query_srvrr(ti_res_handle, domain, 1, call, 0)
	
/**************************************************************************
 * FUNCTION NAME : ti_dns_query_hostname
 **************************************************************************
 * DESCRIPTION   :
 * Given a resolver context handle, hostname to query, and
 * a callback function to notify on any event, this function parses
 * all the input parameters, validates them and submits the "A" query for 
 * processing. On success, this function returns the query id (positive >=0) 
 * of the query submitted and -1 otherwise.
 *************************************************************************/
#define ti_dns_query_hostname( ti_res_handle, domain, call) \
    ti_dns_general_query_hostname(ti_res_handle, domain, "a", call, 0)

/**************************************************************************
 * Data structure definitions.
 *************************************************************************/
/*
 * Data structure to store SRV query buffers
 */
typedef struct {
    char txt_service[QBUFSZ];       /* Service string - _service._protocol */
    int  txt_service_len;           /* Service string length */
    char txt_domain[QBUFSZ];        /* Domain string - domain.com */
    int  txt_domain_len;            /* Domain string length */
} srv_qbuf_t;

/* TI DNS Error Codes */
typedef enum {
    TI_DNS_SUCCESS,             /* Success */
    TI_DNS_TIMEOUT,             /* Timed out waiting for DNS answer */
    TI_DNS_ANSWER_UNPARSEABLE,  /* DNS answer not in the correct format thus unparseable */
    TI_DNS_MISSING_ADDRESS,     /* Hostname answer doesnt contain an IP address */
    TI_DNS_SRV_CODE_UNAVAIL,    /* DNS server returned Service unavailable for the SRV RR query sent */
    TI_DNS_QUERY_FAILURE,       /* Other error */
	TI_DNS_ERROR,               /* General DNS client processing error */
	TI_DNS_ERROR_LAST, 
} TI_DNS_ERROR_CODE;

/* Data structure to store IP address received as an answer to hostname query with TTL and other additional
   information received in the answer.
*/
typedef struct {
    ruli_uint16_t      type;    /* Query type - A/SRV */
    ruli_uint16_t      qclass;  /* Query class - IN */
    ruli_uint32_t      ttl;     /* Time to live */
    ruli_list_t        addr_list;     /* IP address list (answer) for hostname */
} ti_hostname_answer_t;


/**************************************************************************
 * TI Wrapper API definitions.
 *************************************************************************/

int ti_dns_init_resolver(unsigned int dns_timeout, unsigned int dns_retry_count, char* dns_interface,
                        int serverc, char** serverv);
int ti_dns_exit_resolver(int ti_res_handle);
int ti_dns_get_resctx(int ti_res_handle);
int ti_dns_general_query_srvrr(int ti_res_handle, char *domain, unsigned char type, void *(*call)(int search,void *arg), unsigned int ignore_RD_flag);
int ti_dns_general_query_hostname(int ti_res_handle, char *domain, char *type, void *(*call)(int qry_buf,void *arg), unsigned int ignore_RD_flag);
void* ti_dns_start_query_scheduler(int ti_res_handle);
int ti_dns_parse_hostname_answer(int qry, void *arg, ti_hostname_answer_t* hostname_answer);
int ti_dns_get_hostname_qid(int qry);
int ti_dns_get_hostname_resctx(int qry_buf);
int ti_dns_clean_hostname_query(int qry_buf, void *arg);
int ti_dns_parse_srvrr_answer(int search_buf, void *search_arg);
int ti_dns_get_srvrr_qid(int search_buf);
int ti_dns_get_srvrr_resctx(int search_buf);
int ti_dns_clean_srvrr_query(int search_buf, srv_qbuf_t *qbuf);
char *ti_dns_error2str(TI_DNS_ERROR_CODE error);
int ti_dns_cancel_hostname_query(int ti_res_handle, int query_id);
int ti_dns_cancel_srvrr_query(int ti_res_handle, int query_id);
