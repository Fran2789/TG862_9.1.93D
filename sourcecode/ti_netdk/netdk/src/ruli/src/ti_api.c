/*
 * ti_api.c
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

#include <stdio.h>
#include <assert.h>
#include <stdlib.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <string.h>
#include <strings.h>
#include <errno.h>
#include <unistd.h>
#include <ruli.h>
#include <ti_api.h>
#include <ti_api_hidden.h>

#include "trivial_conf_handler.h"
#include "autoconf.h"
#include "sys_nettypes.h"
/**************************************************************************
 * Global variable declarations.
 *************************************************************************/
const int QBUF_SIZE = sizeof(srv_qbuf_t);   

/* Error Texts */
char *dns_error_texts[] = { 
		  "Success",
		  "Timeout waiting for DNS Response",
		  "DNS Response Unparseable",
		  "DNS Response missing IP address",
		  "DNS Response contains Service Unavailable Error",
		  "General DNS Query Failure",
		  "General DNS Error",
};

/**************************************************************************
 * FUNCTION NAME : ti_dns_error2str
 **************************************************************************
 * DESCRIPTION   :
 * A utility function that converts given an error number to a string.
 *************************************************************************/
char *ti_dns_error2str(TI_DNS_ERROR_CODE error) 
{ 
   if(error < TI_DNS_ERROR_LAST) 
    	return dns_error_texts[error]; 
   else
        return "Unknown error";
}

/**************************************************************************
 * FUNCTION NAME : ti_dns_parse_servers
 **************************************************************************
 * DESCRIPTION   :
 * Parses the servers listed in the input string given the server count,
 * and creates a server list in ruli_list_t format.
 *************************************************************************/
int ti_dns_parse_servers(ti_resolver_ctx_t* ti_res_ctx, int serverc, char **serverv)
{
    int i = 0;

    assert(serverc >= 1);

    for (i = 0; i < serverc; ++i) {
        ruli_addr_t *addr = ruli_addr_parse_new(serverv[i]);

        if (!addr) {
            fprintf(stderr, "ti_api (ti_dns_parse_servers): can't save address: %s\n", serverv[i]);
            return -1;
        }

#ifdef TI_API_DEBUG
    fprintf(stderr, "ti_api (ti_dns_parse_servers) saving server: ");
    ruli_addr_print(stderr, addr);
    fprintf(stderr, "\n");
#endif

        {
            int result = ruli_list_push(ti_res_ctx->server_list, addr);
            assert(!result);
        }
    }

    return 0;
}

/**************************************************************************
 * FUNCTION NAME : create_oop_source
 **************************************************************************
 * DESCRIPTION   :
 * Creates the system event source which will poll on our behalf for 
 * any events like DNS replies / timeout etc.
 *************************************************************************/
int create_oop_source(ti_resolver_ctx_t* ti_res_ctx)
{
    /* Create the system event source */
    ti_res_ctx->source_sys = oop_sys_new();
    if (!ti_res_ctx->source_sys) {
        fprintf(stderr, 
	        "ti_api (create_oop_source): can't create system event source: oop_sys_new() failed\n");
        return -1;
    }

    /* Get the system event registration interface */
    ti_res_ctx->source = oop_sys_source(ti_res_ctx->source_sys);
    if (!ti_res_ctx->source) {
        fprintf(stderr, 
	        "ti_api (create_oop_source): can't get registration interface: oop_sys_source() failed\n");
        return -1;
    }

    return 0;
}

/**************************************************************************
 * FUNCTION NAME : ti_dns_start_query_scheduler
 **************************************************************************
 * DESCRIPTION   :
 * Starts the asynchronous event sink.
 *************************************************************************/
void* ti_dns_start_query_scheduler(int ti_res_handle)
{
    ti_resolver_ctx_t*   ti_res_ctx = (ti_resolver_ctx_t *)ti_res_handle;
    void* oop_result;

    if(!ti_res_ctx) {
        printf("Invalid handle to start ti_dns_start_query_scheduler \n");
        return NULL;
    }

    printf("ti_api (ti_dns_start_query_scheduler): Starting RULI scheduler \n");
    oop_result = oop_sys_run(ti_res_ctx->source_sys);
  
    if (oop_result == OOP_ERROR) {
        fprintf(stderr, 
	        "ti_api (ti_dns_start_query_scheduler): oop system source returned error\n");
    }
    else if (oop_result == OOP_CONTINUE) {
    
        /*
         * Normal termination
        */
    
#ifdef TI_API_DEBUG
        fprintf(stderr, 
	        "ti_api (ti_dns_start_query_scheduler): oop system source had no event registered\n");
#endif
    }
    else if (oop_result == OOP_HALT) {
        fprintf(stderr,
	        "ti_api (ti_dns_start_query_scheduler): some sink requested oop system halt\n");
    }
    else {
        fprintf(stderr,
	        "ti_api (ti_dns_start_query_scheduler): unexpected oop system source result (!)\n");
    }

    return oop_result;
}

/**************************************************************************
 * FUNCTION NAME : ti_dns_general_query_hostname
 **************************************************************************
 * DESCRIPTION   :
 * Given a resolver context handle, hostname to query, type of query and
 * a callback function to notify on any event, this function parses
 * all the input parameters, validates them and submits the query for 
 * processing. On success, this function returns the query id (positive >=0) 
 * of the query submitted and -1 otherwise.
 *************************************************************************/
int ti_dns_general_query_hostname(int ti_res_handle, char *domain, char *type,
                                  void *(*call)(int qry_buf,void *arg), unsigned int ignore_RD_flag)
{
    int              result;
    ruli_res_query_t *qry;
    char             *dom_str;
    /* encoded domain */
    char             *dname_buf;
    int              dname_buf_len;
    int              dname_len;
    char             *i;
    const char       *qclass = "in";
	int              qc, qt;
    ti_resolver_ctx_t*    ti_res_ctx = (ti_resolver_ctx_t *)ti_res_handle;
    int domain_len = 0;

    if(!ti_res_ctx) {
        printf("ti_api (ti_dns_query_hostname): Invalid resolver context handle, Cant submit query\n");
        return -1;
    }

    if(!domain || strlen(domain) <= 0) {
        printf("ti_api (ti_dns_query_hostname): Invalid hostname to query. Cant submit query\n");
        return -1;
    }
    domain_len = strlen(domain);

    qc = ruli_get_qclass_code(qclass);
    if (!qc) {
        fprintf(stderr, "ti_api (ti_dns_query_hostname): can't find query class: %s\n", qclass);
        return -1;
    }

    qt = ruli_get_qtype_code(type);
    if (!qt) {
        fprintf(stderr, "ti_api (ti_dns_query_hostname): can't find query type: %s\n", type);
        return -1;
    }

    /*
     * Allocate space for encoded domain in dname_buf;
     * encoded domain is the stripped (sans dots and leading
     * spaces) equivalent of a given domain name 
     */

    /* Find required size */
    dname_buf_len = ruli_dname_encode_size(domain, domain_len);

    /* Allocate buffer */
    dname_buf = (char *) ruli_malloc(dname_buf_len);
    if (!dname_buf) {
        fprintf(stderr, 
	        "ti_api (ti_dns_query_hostname): ruli_malloc(%d) failed: %s\n",
	         dname_buf_len, strerror(errno));
      
             return -1;
    }

#ifdef TI_API_DEBUG
    {
        /* Debug */
        const int DEBUG_BUFSZ = 256;
        char      debug_buf[DEBUG_BUFSZ];
      
        assert(domain_len < DEBUG_BUFSZ);
      
        memcpy(debug_buf, domain, domain_len);
        debug_buf[domain_len] = '\0';
      
        fprintf(stderr, 
	            "ti_api (ti_dns_query_hostname): dname_buf=%u domain=%s domain_len=%d\n",
	            (unsigned int) dname_buf, debug_buf, domain_len);
    }
#endif
    
    /*
     * Encode domain - Remove any leading spaces, and dots
     * Eg: User entered domain name is x-y.abcdomain.com
     * Encoded domain would be: x-yabcdomaincom. This is
     * how underlying RULI library API would expect to
     * send NAME (A) Queries.
     */
    i = ruli_dname_encode(dname_buf, dname_buf_len, domain, domain_len);
    if (!i) {
        const int DOM_BUFSZ = 256;
        char      dom_buf[DOM_BUFSZ];
        int       dom_len = RULI_MIN(domain_len, DOM_BUFSZ - 1);
      
        memcpy(dom_buf, domain, dom_len);
        dom_buf[dom_len] = '\0';

        fprintf(stderr, 
	            "ti_api (ti_dns_query_hostname): can't encode domain: (total_len=%d displaying=%d) %s\n", 
	            domain_len, dom_len, dom_buf);
        return -1;
    }
    dname_len = i - dname_buf;
    
    /*
     * Allocate space for query
     */
    qry = (ruli_res_query_t *) ruli_malloc(sizeof(ruli_res_query_t));
    if (!qry)
        return -1;
  
    /*
     * Save domain string
     */
    dom_str = (char *) ruli_malloc(domain_len + 1);
    if (!dom_str) {
        ruli_free(qry);
        return -1;
    }

    memcpy(dom_str, domain, domain_len);
    dom_str[domain_len] = '\0';

    /*
     * Send query
     */

#ifdef TI_API_DEBUG
    fprintf(stderr, 
	    "ti_api (ti_dns_query_hostname): DEBUG: submit_query(): domain=%s resolver=%u "
	    "dname_buf=%u qry=%u dname_len=%d class=%d type=%d dname_buf: %s\n", 
	    dom_str, (unsigned int) ti_res_ctx->ruli_res_ctx,
	    (unsigned int) dname_buf, (unsigned int) qry,
	    dname_len, qc, qt, dname_buf);
#endif
    /* Register callback and intialize the query with Class, Type, domain name etc */
    qry->q_on_answer     = call;        /* Give the user supplied call back function pointer */
    qry->q_on_answer_arg = dom_str;     /* User supplied domain name */
    qry->q_domain        = dname_buf;   /* Encoded equivalent of user entered domain name */
    qry->q_domain_len    = dname_len;   /* Length of the encoded domain */
    qry->q_class         = qc;          /* Class, by default IN */
    qry->q_type          = qt;          /* Type, by default A */
    qry->q_options       = RULI_RES_OPT_VOID;

    /* ARRIS ADD START */
    if (ignore_RD_flag)
    {
        qry->q_options |= RULI_RES_OPT_IGNORE_RD;
    }
    /* ARRIS ADD END */

    /* Submit the query */
    result = ruli_res_query_submit(ti_res_ctx->ruli_res_ctx, qry);
    /* Check for any error */
    if (result) {
        fprintf(stderr, 
	        "ti_api (ti_dns_query_hostname): ruli_res_query_submit() failed: %s [%d]\n", 
	        ruli_res_errstr(result), result);
        ruli_free(dom_str);
        ruli_free(qry);

        return -1;
    } 

    return qry->query_id;
}

/**************************************************************************
 * FUNCTION NAME : ti_dns_clean_hostname_query
 **************************************************************************
 * DESCRIPTION   :
 * Frees the buffers allocated to hold the DNS A query. MUST be called 
 * after the answer is parsed.
 *************************************************************************/
int ti_dns_clean_hostname_query(int qry_buf, void *arg)
{
    ruli_res_query_t *qry = (ruli_res_query_t *)qry_buf;
    char* domain = (char *)arg;

    if(!qry || !domain)
        return -1;

    /* Destroy query */
    ruli_res_query_delete(qry);
  
    /* Free buffers */
    ruli_free(domain);
    ruli_free(qry->q_domain);
    ruli_free(qry);

    return 0;
}

/**************************************************************************
 * FUNCTION NAME : ti_dns_parse_hostname_answer
 **************************************************************************
 * DESCRIPTION   :
 * Called from call back function in the invoking application. This function 
 * does the following:
 *  1.  Parse the answer and check if any error code returned. If so, 
 *      return appropriate TI_DNS_ERROR_CODE to application. Before
 *      returning cleansup the call back arguments passed to it.
 *  2.  If no error, process the configuration receieved. Copy all the 
 *      IP addresses recieved in the answer to addr_list data structure 
 *      argument. Also propagate TTL and other info from the answer.
 *      Finally cleanup the callback arguments passed to it.
 *************************************************************************/
int ti_dns_parse_hostname_answer(int qry_buf, void *arg, ti_hostname_answer_t* hostname_answer)
{
    ruli_res_query_t *qry = (ruli_res_query_t *)qry_buf;
    char       *domain  = (char *) arg;
    char       tmp_addr_string[256];
#ifdef TI_API_DEBUG
    const int  BUFSZ    = 1024;
    char       buf[BUFSZ];
    int        str_len;
#endif

    /* Validate Input params */
    assert(qry);
    assert(domain);
    assert(hostname_answer);
    assert(qry->answer_code != RULI_CODE_VOID);

    /* Timeout waiting for an answer? */
    if (qry->answer_code == RULI_CODE_TIMEOUT) {
#ifdef TI_API_DEBUG
        str_len = snprintf(buf, BUFSZ, "%s query-timeout\n", domain);

        assert(str_len < BUFSZ);

        printf("%s \n", buf);
#endif

        //ti_dns_clean_hostname_query(qry_buf, arg);

        return TI_DNS_TIMEOUT;
    }

    /* Error reported? */
    if (qry->answer_code) {
#ifdef TI_API_DEBUG
        str_len = snprintf(buf, BUFSZ, "%s query-failure\n", domain);

        assert(str_len < BUFSZ);

        printf("%s \n", buf);
#endif

        //ti_dns_clean_hostname_query(qry_buf, arg);

        return TI_DNS_QUERY_FAILURE;
    }

#ifdef TI_API_DEBUG
    {
        ruli_msg_header_t msg_hdr;

        msg_hdr = qry->answer_header;

        str_len = snprintf(buf, BUFSZ,
		       "query succeded: domain=%s id=%d "
		       "rcode=%d qd=%d an=%d ns=%d ar=%d "
		       "answer_buf_size=%d answer_msg_len=%d\n", 
		       domain, msg_hdr.id, msg_hdr.rcode, 
		       msg_hdr.qdcount, msg_hdr.ancount, 
		       msg_hdr.nscount, msg_hdr.arcount,
		       qry->answer_buf_size, qry->answer_msg_len);

        assert(str_len < BUFSZ);

        printf("%s \n", buf);
    }
#endif

    //ARRIS ADD - Start
    if (qry->answer_header.rcode)
    {
        printf("Standard query A %s reponse error reply code = %d\n", domain, qry->answer_header.rcode);
        //return TI_DNS_QUERY_FAILURE;
    }
    //ARRIS ADD - End
    
    /* Answer Received? */ 
    {
        ruli_parse_t parse;
        int          result;
        int          i;
        int          size;
        int          addr_count = 0;
	     const int CNAME_BUFSZ = 256;//ARRIS ADD
	     char      cname[CNAME_BUFSZ];//ARRIS ADD
	     char      cname_owner[CNAME_BUFSZ];//ARRIS ADD
	     int       cname_len = 0;//ARRIS ADD
	     int       cname_owner_len = 0;//ARRIS ADD
        
        ruli_parse_new(&parse);

        result = ruli_parse_message(&parse, &qry->answer_header, 
				(ruli_uint8_t *) qry->answer_buf,
				qry->answer_buf_size);
        if (result) {
#ifdef TI_API_DEBUG
            str_len = snprintf(buf, BUFSZ, "%s answer-unparseable\n", domain);
      
            assert(str_len < BUFSZ);
      
            printf("%s \n", buf);
#endif

            //ti_dns_clean_hostname_query(qry_buf, arg);

            return TI_DNS_ANSWER_UNPARSEABLE;
        }

        size = ruli_list_size(&parse.answer_list);

        //ARRIS ADD BEGIN
        //There can be at most one CNAME.
        //Need to store off CNAME strings so that A records
        //can be matched back to the original query domain name.
        for (i = 0; i < size; i++)
        {
            ruli_rr_t   *rr;
            
            rr = (ruli_rr_t *) ruli_list_get(&parse.answer_list, i);
            
            if (rr->type == RULI_RR_TYPE_CNAME)
            {
                ruli_dname_extract((ruli_uint8_t *) qry->answer_buf, 
				        (ruli_uint8_t *) qry->answer_buf + qry->answer_buf_size,
				        (ruli_uint8_t *) cname_owner, 
				        (ruli_uint8_t *) cname_owner + CNAME_BUFSZ,
				        rr->owner,
				        &cname_owner_len);
                
                //If the owner of this CNAME is the requested domain, store the CNAME
                if( ruli_dname_match( domain, strlen(domain), cname_owner, cname_owner_len) )
                { 
	                 ruli_dname_extract((ruli_uint8_t *) qry->answer_buf, 
				            (ruli_uint8_t *) qry->answer_buf + qry->answer_buf_size,
				            (ruli_uint8_t *) cname, 
				            (ruli_uint8_t *) cname + CNAME_BUFSZ,
				            rr->rdata,
				            &cname_len);
                    break; // only one CNAME is allowed
                }
            }
        } //ARRIS END
        
        for (i = 0; i < size; ++i) {
            ruli_addr_t addr;
            ruli_rr_t   *rr;

            rr = (ruli_rr_t *) ruli_list_get(&parse.answer_list, i);

            if (rr->qclass != RULI_RR_CLASS_IN)
	            continue;

            switch (rr->type) {
                case RULI_RR_TYPE_A:
                    /* Validate the IPv4 address received */
                    result = ruli_parse_rr_a(&addr.addr.ipv4, rr->rdata, rr->rdlength);
                    if (result)
                        continue;

	                ruli_addr_init(&addr, PF_INET);

                    break;

                case RULI_RR_TYPE_AAAA:

                    /* Validate the IPv6 address received */
                    result = ruli_parse_rr_aaaa(&addr.addr.ipv6, rr->rdata, rr->rdlength);
                    if (result)
                        continue;

	                ruli_addr_init(&addr, PF_INET6);

                    break;

                default:
                    continue;
            }

            {
	            const int OWNER_BUFSZ = 256;
	            char      owner_buf[OWNER_BUFSZ];
	            int       owner_len;

	            result = ruli_dname_extract((ruli_uint8_t *) qry->answer_buf, 
				    (ruli_uint8_t *) qry->answer_buf + qry->answer_buf_size,
				    (ruli_uint8_t *) owner_buf, 
				    (ruli_uint8_t *) owner_buf + OWNER_BUFSZ,
				    rr->owner,
				    &owner_len);
	            assert(!result);

#ifdef TI_API_DEBUG
    fprintf(stderr, 
		"DEBUG: on_answer(): domain=%s txt_owner=%s "
		"txt_owner_len=%d\n", 
		domain, owner_buf, owner_len);
#endif

	            if (!ruli_dname_match(domain, strlen(domain), owner_buf, owner_len) && //ARRIS MOD LINE
                   !ruli_dname_match(cname, cname_len, owner_buf, owner_len)) //ARRIS MOD LINE
                   continue;
            }

#ifdef TI_API_DEBUG
            str_len = snprintf(buf, BUFSZ, "%s ", domain);
            assert(str_len > 0);
            assert(str_len < BUFSZ);
      
            /* Append the IP address to the answer buffer for display on console */
            {
	            int len = ruli_addr_snprint(buf + str_len , BUFSZ - str_len, &addr);
	            assert(len > 0);
	            assert(len < (BUFSZ - str_len));
	            str_len += len;
            }

            {
	            char *dst = strncat(buf, "\n", BUFSZ);
	            assert(dst);
            }

            printf("%s \n", buf);
#endif

            /* Replicate the address into the addr_list sent by applcn */
            {
	            int len = ruli_addr_snprint(tmp_addr_string , 256, &addr);
	            assert(len > 0);
	            assert(len < 256);
            }

            ruli_addr_t *tmp_addr = ruli_addr_parse_new(tmp_addr_string);

            if (!tmp_addr) {
                fprintf(stderr, "ti_api (ti_dns_parse_hostname_answer): can't save address: %s\n", tmp_addr_string);

                /* Free the query buffer */
                //ti_dns_clean_hostname_query(qry_buf, arg);

                return TI_DNS_ERROR;
            }

            {
                int result = ruli_list_push(&hostname_answer->addr_list, tmp_addr);
                assert(!result);
            }

            /* Propagate the TTL, Class and Type of the query answer,
             * needed for implementing caching logic.
             */
            hostname_answer->ttl = rr->ttl;
            hostname_answer->type = rr->type;
            hostname_answer->qclass = rr->qclass;

            ++addr_count;
    } // End of for
    
    ruli_parse_delete(&parse);

    /* If no answer, print error */
    if (!addr_count) {
#ifdef TI_API_DEBUG
        str_len = snprintf(buf, BUFSZ, "%s answer-missing-address\n", domain);
        assert(str_len < BUFSZ);
        printf("%s \n", buf);
#endif

        /* Free the query buffer */
        //ti_dns_clean_hostname_query(qry_buf, arg);

        return TI_DNS_MISSING_ADDRESS;
    }
  }

  /* only in success case we dont cleanup - let the application to the job.
   * Free the query buffer 
   */
  //ti_dns_clean_hostname_query(qry_buf, arg);

  return TI_DNS_SUCCESS;
}

/**************************************************************************
 * FUNCTION NAME : ti_dns_get_hostname_qid
 **************************************************************************
 * DESCRIPTION   :
 * Called from call back function in the invoking application. Given a query
 * buffer, this function returns its query id.
 * The query id is a positive (>=0) number which identifies a 
 * query submitted uniquely within a given resolver instance. This can be
 * used to differentiate similar queries submitted within an application 
 * using same resolver instance but within different threads.
 *************************************************************************/
int ti_dns_get_hostname_qid(int qry_buf)
{
    ruli_res_query_t *qry = (ruli_res_query_t *)qry_buf;

    /* Validate input parameters */
    if(!qry)
        return -1;

    return qry->query_id;
}

/**************************************************************************
 * FUNCTION NAME : ti_dns_get_hostname_resctx
 **************************************************************************
 * DESCRIPTION   :
 * Called from call back function in the invoking application. Given a query
 * buffer, this function returns the handle to RULI resolver context using 
 * which the query was submitted. A RULI resolver context handle is one
 * which identifies every resolver instance created using ti_dns_init_resolver
 * API uniquely within RULI. This is helpful in differentiating similar queries
 * submitted within an application context using different resolver instances.
 *************************************************************************/
int ti_dns_get_hostname_resctx(int qry_buf)
{
    ruli_res_query_t *qry = (ruli_res_query_t *)qry_buf;

    /* Validate input parameters */
    if(!qry)
        return (int)NULL;

    return (int)qry->resolver;
}

/**************************************************************************
 * FUNCTION NAME : ti_dns_general_query_srvrr
 **************************************************************************
 * DESCRIPTION   :
 * Given a resolver context handle, SRV tuple(_service._protocol.domain) to 
 * query, and a callback function to notify on any event, this function 
 * parses all the input parameters, validates them and submits the query for 
 * processing. On success, returns the query id of the query which is a 
 * positive number (>=0) and -1 otherwise.
 *************************************************************************/
int ti_dns_general_query_srvrr(int ti_res_handle, char *domain, unsigned char type,
                          void *(*call)(int search,void *arg), unsigned int ignore_RD_flag)
{
    srv_qbuf_t *qbuf = NULL;
    unsigned long options = 0;
    ti_resolver_ctx_t* ti_res_ctx = (ti_resolver_ctx_t *)ti_res_handle;

    if(!ti_res_ctx) {
        printf("ti_api (ti_dns_query_srvrr): Invalid resolver context handle, Cant submit query\n");
        return -1;
    }

    if(!domain || strlen(domain) <= 0) {
        printf("ti_api (ti_dns_query_srvrr): Invalid SRV RR to query. Cant submit query\n");
        return -1;
    }

    /*
     * Break full domain name in service + domain into qbuf
     */
    {
        int   domain_len       = strlen(domain);
        const char *past_end   = domain + domain_len;
        const char *i = domain;

        if (*i != '_') {
            fprintf(stderr, 
	            "ti_api (ti_dns_query_srvrr): could not match _service\n");
            return -1;
        }

        for (; i < past_end; ++i) {
            if (*i == '.') {
	            ++i;
	            if (i < past_end) {
	                if (*i != '_')
	                    break;
	            }
            }
        }
    
        if (i >= past_end) {
            fprintf(stderr, 
	            "ti_api (ti_dns_query_srvrr): could not split service/domain\n");
      
            return -1;
        }
    
        /* Allocate qbuf */
        qbuf = (srv_qbuf_t *) ruli_malloc(QBUF_SIZE);
        if (!qbuf) {
            fprintf(stderr, 
	            "ti_api (ti_dns_query_srvrr): could not allocate srv_qbuf_t: ruli_malloc(%d) failed\n",
	            QBUF_SIZE);
      
            return -1;
        }

        qbuf->txt_service_len = i - domain - 1;
        assert(qbuf->txt_service_len < QBUFSZ);
        memcpy(qbuf->txt_service, domain, qbuf->txt_service_len);
        qbuf->txt_service[qbuf->txt_service_len] = '\0';
    
#ifdef TI_API_DEBUG
        fprintf(stderr, 
		   "ti_api (ti_dns_query_srvrr): txt_service=%s txt_service_len=%d", 
		   qbuf->txt_service, qbuf->txt_service_len);
#endif
    
        qbuf->txt_domain_len = past_end - i;
        assert(qbuf->txt_domain_len < QBUFSZ);
        memcpy(qbuf->txt_domain, i, qbuf->txt_domain_len);
        qbuf->txt_domain[qbuf->txt_domain_len] = '\0';
    
#ifdef TI_API_DEBUG
        fprintf(stderr, 
		   "do_query(): txt_domain=%s txt_domain_len=%d", 
		   qbuf->txt_domain, qbuf->txt_domain_len);
#endif
    
    } /* Break full domain name in service + domain into qbuf */
  
    /*
     * Send query. Opens the socket and sends out the query to
     * DNS servers configured in the /etc/resolv.conf
     * If ruli_search_srv_submit() fails to submit the query, 
     * a NULL pointer is returned. This may be caused by lack of 
     * system resources, improper network configuration, or system/network failures.
    */

    {
        /* ARRIS ADD START */
        if (ignore_RD_flag)
        {
            options |= RULI_RES_OPT_IGNORE_RD;
        }
        /* ARRIS ADD END */
        /* dont try to resolve addresses which we dont support */
        if (type == INET_ADDR_TYPE_IPV4)
        {
            options |= RULI_RES_OPT_SRV_NOINET6;
        }
        else if (type == INET_ADDR_TYPE_IPV6)
        {
            options |= RULI_RES_OPT_SRV_NOINET;
        }
/* Dont try to resolve targets to IPv6 addresses */
#ifndef CONFIG_TI_RULI_IPV6    
        options |= RULI_RES_OPT_SRV_NOINET6;
#endif
/* Dont fallback to domain name resolution if DNS server responds with error for
 * a SRV query sent.
 */
#ifndef CONFIG_TI_RULI_SRV_FALLBACK
        options |= RULI_RES_OPT_SRV_NOFALL;
#endif        
        ruli_search_srv_t *search = ruli_search_srv_submit(ti_res_ctx->ruli_res_ctx, /* resolver context */
                               call,    /* User supplied call back function */
						       qbuf,    /* Query buffer */
						       options, /* Options for resolver */
						       qbuf->txt_service,   /* Service name */
						       qbuf->txt_domain,    /* Domain name */
						       -1);
        if (!search) {
            fprintf(stderr, 
	            "ti_api (ti_dns_query_srvrr): could not send SRV query\n");
    
            ruli_free(qbuf);
            return -1;
        }

        return search->srv_query.query.query_id;
    }
}

/**************************************************************************
 * FUNCTION NAME : ti_dns_clean_srvrr_query
 **************************************************************************
 * DESCRIPTION   :
 * Frees the buffers allocated to hold the DNS SRV RR query. MUST be called 
 * after the answer is parsed.
 *************************************************************************/
int ti_dns_clean_srvrr_query(int search_buf, srv_qbuf_t *qbuf)
{
    ruli_search_srv_t *search = (ruli_search_srv_t *) search_buf;

    if(!search || !qbuf)
        return -1;

    /* Destroy the query */
    ruli_search_srv_delete(search);

    /* Free the buffers */
    ruli_free(qbuf);

    return 0;
}

/**************************************************************************
 * FUNCTION NAME : ti_dns_parse_srvrr_answer
 **************************************************************************
 * DESCRIPTION   :
 *  Called from the call back function in the invoking application. This 
 *  function does the following:
 *  1.  Parse the answer received from DNS server for SRV RR request sent.
 *  2.  If an error present in the answer, return the appropriate 
 *      error code of type TI_DNS_ERROR_CODE to the application. Before 
 *      returning free up the call back function arguments passed to this
 *      API.
 *  2.  If no error, return TI_DNS_SUCCESS. Dont free the call back function
 *      arguments yet, the call back function is going to process them 
 *      and free them (since its too much work to replicate and send).
 *************************************************************************/
int ti_dns_parse_srvrr_answer(int search_buf, void *search_arg)
{
    ruli_search_srv_t *search = (ruli_search_srv_t *)search_buf;
    srv_qbuf_t *qbuf = (srv_qbuf_t *) search_arg;
    int        code;

    /* Validate input parameters */
    assert(search);
    assert(qbuf);

    /*
     * This function returns 0 (zero/false) if the underlying 
     * resolver has successfully completed the query, or an error 
     * code otherwise.
     */
    code = ruli_search_srv_code(search);

    assert(code != RULI_SRV_CODE_VOID);

    /*
     * Timeout?
    */ 
    if (code == RULI_SRV_CODE_ALARM) {
#ifdef TI_API_DEBUG
        printf("%s.%s timeout\n", qbuf->txt_service, qbuf->txt_domain);
#endif
        //ti_dns_clean_srvrr_query(search_buf, qbuf);
        return TI_DNS_TIMEOUT;
    }

    /*
     * Service is not provided by that domain?
    */ 
    if (code == RULI_SRV_CODE_UNAVAILABLE) {
#ifdef TI_API_DEBUG
        printf("%s.%s service-not-provided\n", qbuf->txt_service, qbuf->txt_domain);
#endif
        //ti_dns_clean_srvrr_query(search_buf, qbuf);
        return TI_DNS_SRV_CODE_UNAVAIL;
    }

    //ARRIS ADD - Start
    int reply_code = ruli_search_srv_rcode(search);
    if (reply_code)
    {
        printf("Standard query SRV %s.%s reponse error reply code = %d\n", qbuf->txt_service, qbuf->txt_domain, reply_code);
        //return TI_DNS_QUERY_FAILURE;
    }
    //ARRIS ADD - End
    
    /*
     * Other error?
    */ 
    if (code) {
        /*
         * If an error code is returned by the resolver 
         * [through function ruli_sync_srv_code() above], this function 
         * may used to find out if the server sent a bad RCODE as answer.
         */
#ifdef TI_API_DEBUG
        int rcode = ruli_search_srv_rcode(search);
        printf("%s.%s srv-failure code=%d rcode=%d\n", qbuf->txt_service, qbuf->txt_domain, code, rcode);
#endif
        //ti_dns_clean_srvrr_query(search_buf, qbuf);
        return TI_DNS_QUERY_FAILURE;
    }

    return TI_DNS_SUCCESS;
}

/**************************************************************************
 * FUNCTION NAME : ti_dns_get_srvrr_qid
 **************************************************************************
 * DESCRIPTION   :
 * Called from call back function in the invoking application. Given a query
 * buffer, this function returns the query id associated with it.
 * The query id is a positive (>=0) number which identifies a 
 * query submitted uniquely within a given resolver instance. This can be
 * used to differentiate similar queries submitted within an application 
 * using same resolver instance but within different threads.
 *************************************************************************/
int ti_dns_get_srvrr_qid(int search_buf)
{
    ruli_search_srv_t *search = (ruli_search_srv_t *)search_buf;

    /* Validate input parameters */
    if(!search)
        return -1;

    return search->srv_query.query.query_id;
}

/**************************************************************************
 * FUNCTION NAME : ti_dns_get_srvrr_resctx
 **************************************************************************
 * DESCRIPTION   :
 * Called from call back function in the invoking application.  Given a query 
 * buffer, this function returns the handle to RULI resolver context using 
 * which the query was submitted. A RULI resolver context handle is one
 * which identifies every resolver instance created using ti_dns_init_resolver
 * API uniquely within RULI. This is helpful in differentiating similar queries
 * submitted within an application context using different resolver instances.
 *************************************************************************/
int ti_dns_get_srvrr_resctx(int search_buf)
{
    ruli_search_srv_t *search = (ruli_search_srv_t *)search_buf;

    /* Validate input parameters */
    if(!search)
        return (int)NULL;

    return (int)search->srv_query.query.resolver;
}

/**************************************************************************
 * FUNCTION NAME : ti_dns_start
 **************************************************************************
 * DESCRIPTION   :
 * Creates the OOP source (for asynchrnous operation), intializes the 
 * resolver context.
 *************************************************************************/
void ti_dns_start(ti_resolver_ctx_t* ti_res_ctx)
{
    int ret = -1;

    /*
     * Create event source
    */
    ret = create_oop_source(ti_res_ctx);
    if(ret == -1) 
        return;

    /* If servers list if configured by the application explicitly */
    if(ruli_list_size(ti_res_ctx->server_list)) {
        /*
         * Initialize resolver
        */
        if((ti_res_ctx->handler =  (ruli_conf_handler_t *)malloc(sizeof(ruli_conf_handler_t))) == NULL) {

            if(ti_res_ctx->source_sys) {
                oop_sys_delete(ti_res_ctx->source_sys);
                ti_res_ctx->source_sys = NULL;
                ti_res_ctx->source = NULL;
            }

            ruli_list_dispose_trivial(ti_res_ctx->server_list);
            free(ti_res_ctx->server_list);
            ti_res_ctx->server_list = NULL;

            return;
        }
        
        ti_res_ctx->handler->opaque          = ti_res_ctx->server_list;
        ti_res_ctx->handler->search_loader   = load_search_list;
        ti_res_ctx->handler->search_unloader = unload_search_list;
        ti_res_ctx->handler->ns_loader       = load_ns_list;
        ti_res_ctx->handler->ns_unloader     = unload_ns_list;

        
        if((ti_res_ctx->ruli_res_ctx =  (ruli_res_t *)malloc(sizeof(ruli_res_t))) == NULL) {
            free(ti_res_ctx->handler);
            ti_res_ctx->handler = NULL;

            if(ti_res_ctx->source_sys) {
                oop_sys_delete(ti_res_ctx->source_sys);
                ti_res_ctx->source_sys = NULL;
                ti_res_ctx->source = NULL;
            }

            ruli_list_dispose_trivial(ti_res_ctx->server_list);
            free(ti_res_ctx->server_list);
            ti_res_ctx->server_list = NULL;

            return;
        }

        ti_res_ctx->ruli_res_ctx->res_conf_handler = ti_res_ctx->handler;
        ti_res_ctx->ruli_res_ctx->res_source        = ti_res_ctx->source;
        ti_res_ctx->ruli_res_ctx->res_retry         = ti_res_ctx->retry;
        ti_res_ctx->ruli_res_ctx->res_timeout       = ti_res_ctx->timeout;

        if((ti_res_ctx->ruli_res_ctx->res_interface = (char *)malloc(sizeof(char) * (strlen(ti_res_ctx->interface) + 1))) == NULL) {
            ruli_res_delete(ti_res_ctx->ruli_res_ctx);
            free(ti_res_ctx->ruli_res_ctx);
            free(ti_res_ctx->handler);
            ti_res_ctx->ruli_res_ctx = NULL;
            ti_res_ctx->handler = NULL;

            if(ti_res_ctx->source_sys) {
                oop_sys_delete(ti_res_ctx->source_sys);
                ti_res_ctx->source_sys = NULL;
                ti_res_ctx->source = NULL;
            }

            ruli_list_dispose_trivial(ti_res_ctx->server_list);
            free(ti_res_ctx->server_list);
            ti_res_ctx->server_list = NULL;

            return;
        }
        strcpy(ti_res_ctx->ruli_res_ctx->res_interface, ti_res_ctx->interface);

        if(ruli_res_new(ti_res_ctx->ruli_res_ctx)) {
            fprintf(stderr,
                "ti_api (ti_dns_start): can't create ruli resolver\n");
            ruli_res_delete(ti_res_ctx->ruli_res_ctx);
            free(ti_res_ctx->ruli_res_ctx);
            free(ti_res_ctx->handler);
            ti_res_ctx->ruli_res_ctx = NULL;
            ti_res_ctx->handler = NULL;

            if(ti_res_ctx->source_sys) {
                oop_sys_delete(ti_res_ctx->source_sys);
                ti_res_ctx->source_sys = NULL;
                ti_res_ctx->source = NULL;
            }

            ruli_list_dispose_trivial(ti_res_ctx->server_list);
            free(ti_res_ctx->server_list);
            ti_res_ctx->server_list = NULL;

            return;
        }

        assert(ti_res_ctx->ruli_res_ctx);
    } 
    /* Use resolv.conf */
    else {
        /*
         * Initialize resolver. This loads the name servers
         * from the /etc/resolv.conf and initializes the resolver
         * with timeout and retry count values.
        */
        ti_res_ctx->search_res = ruli_search_res_new(ti_res_ctx->source, 
                                                 ti_res_ctx->retry, 
                                                 ti_res_ctx->timeout, 
                                                 ti_res_ctx->interface);
        if (!ti_res_ctx->search_res) {
            fprintf(stderr,
                "ti_api (ti_dns_start): can't create ruli resolver\n");
            return;
        }

        assert(ti_res_ctx->search_res);

        /* Retrieves the handle to resolver configuration context */
        ti_res_ctx->ruli_res_ctx = ruli_search_resolver(ti_res_ctx->search_res);

        assert(ti_res_ctx->ruli_res_ctx);
     }

    return;
}

/**************************************************************************
 * FUNCTION NAME : ti_dns_init_resolver
 **************************************************************************
 * DESCRIPTION   :
 * Initialization function. Must be called before submitting any query
 * using ti_dns_query_hostname/ti_dns_query_srvrr. This function validates 
 * the arguments passed, and initializes the resolver if everything ok.
 * Otherwise, returns -1 to indicate error in the error argument passed.
 * Also, if resolver initialization went well, it returns the handle to
 * resolver context, which the calling application must use in submitting
 * any queries.
 *************************************************************************/
int ti_dns_init_resolver(unsigned int dns_timeout, unsigned int dns_retry_count, 
                         char* dns_interface,
                         int serverc, char** serverv)
{
    ti_resolver_ctx_t*  ti_res_ctx  = NULL;
    int ret = -1;
  
    /* Create the TI Resolver context */
    if((ti_res_ctx = (ti_resolver_ctx_t *)malloc(sizeof(ti_resolver_ctx_t))) == NULL) {
        printf("TI Resolver context could not be created. Malloc error \n");
        return (int)NULL;
    }

    /* Interface to use MUST be configured */
    if(dns_interface == NULL) {
        printf("Interface MUST be configured! \n");
        free(ti_res_ctx);
        ti_res_ctx = NULL;
        return (int)NULL;
    }
    else {
        if((ti_res_ctx->interface = (char *) malloc(sizeof(char) * (strlen(dns_interface) + 1))) == NULL) {
            printf("malloc failure \n");
            free(ti_res_ctx);
            ti_res_ctx = NULL;
            return (int)NULL;
        }
        strcpy(ti_res_ctx->interface, dns_interface);
    }

    /* Initialize the TI resolver context with defaults */
    if(1) {
        ti_res_ctx->timeout = 10;
        ti_res_ctx->retry   = 3;
        ti_res_ctx->handler = NULL;  //ARRIS ADD
        ti_res_ctx->search_res = NULL;
        ti_res_ctx->ruli_res_ctx = NULL;
        ti_res_ctx->source = NULL;
        ti_res_ctx->source_sys = NULL;
        ti_res_ctx->server_list = NULL;
    }

    if(dns_timeout > 0) 
        ti_res_ctx->timeout = dns_timeout;
   
    if(dns_retry_count > 0)
        ti_res_ctx->retry = dns_retry_count;

    if((ti_res_ctx->server_list = (ruli_list_t *) ruli_malloc(sizeof(ruli_list_t))) == NULL) {
        free(ti_res_ctx->interface);
        ti_res_ctx->interface = NULL;
        free(ti_res_ctx);
        ti_res_ctx = NULL;
        return (int)NULL;
    }

    if(ruli_list_new(ti_res_ctx->server_list)) {
        free(ti_res_ctx->server_list);
        ti_res_ctx->server_list = NULL;
        free(ti_res_ctx->interface);
        ti_res_ctx->interface = NULL;
        free(ti_res_ctx);
        ti_res_ctx = NULL;
        return (int)NULL;
    }

    if(serverc > 0) {
        ret = ti_dns_parse_servers(ti_res_ctx, serverc, serverv);
        if(ret == -1) {
            free(ti_res_ctx->server_list);
            ti_res_ctx->server_list = NULL;
            free(ti_res_ctx->interface);
            ti_res_ctx->interface = NULL;
            free(ti_res_ctx);
            ti_res_ctx = NULL;
            return (int)NULL;
        }
    }

    ti_dns_start(ti_res_ctx);

    if(ti_res_ctx->ruli_res_ctx == NULL) {
        free(ti_res_ctx->interface);
        ti_res_ctx->interface = NULL;
        free(ti_res_ctx);
        ti_res_ctx = NULL;
        return (int)NULL;
    }
    else {
        return (int)ti_res_ctx;
    }
}

/**************************************************************************
 * FUNCTION NAME : ti_dns_exit_resolver
 **************************************************************************
 * DESCRIPTION   :
 * Cleanup and exit function. Frees the resolver context and OOP event 
 * source. Also frees any other buffers allocated for processing.
 *************************************************************************/
int ti_dns_exit_resolver(int ti_res_handle) 
{
    ti_resolver_ctx_t*   ti_res_ctx = (ti_resolver_ctx_t *)ti_res_handle;

    if(ti_res_ctx->interface) {
        ruli_free(ti_res_ctx->interface);
        ti_res_ctx->interface = NULL;
    }

    /*
     * Destroy resolver
     */
    if(ti_res_ctx->search_res) {
        ruli_search_res_delete(ti_res_ctx->search_res);
        ti_res_ctx->search_res = NULL;
        ti_res_ctx->ruli_res_ctx = NULL;
    }

    if(ti_res_ctx->handler) {
        if(ti_res_ctx->ruli_res_ctx) {
          ruli_res_delete(ti_res_ctx->ruli_res_ctx);
          free(ti_res_ctx->ruli_res_ctx);
          ti_res_ctx->ruli_res_ctx = NULL;
        }
        free(ti_res_ctx->handler);
        ti_res_ctx->handler = NULL;
    }

    /*
     * Destroy event source
    */
    if(ti_res_ctx->source_sys) {
        oop_sys_delete(ti_res_ctx->source_sys);
        ti_res_ctx->source_sys = NULL;
        ti_res_ctx->source = NULL;
    }

    /* Free the server list */
    ruli_list_dispose_trivial(ti_res_ctx->server_list);
    free(ti_res_ctx->server_list);
    ti_res_ctx->server_list = NULL;

    /* Free the TI resolver context */
    free(ti_res_ctx);
    ti_res_ctx = NULL;

    return 0;
}

/**************************************************************************
 * FUNCTION NAME : ti_dns_get_resctx
 **************************************************************************
 * DESCRIPTION   :
 * Called from the invoking application. Given a handle to ti_resolver_ctx_t
 * data structure, this function returns the RULI resolver context handle
 * associated with it. A RULI resolver context handle is one which identifies 
 * every resolver instance created using ti_dns_init_resolver API uniquely 
 * within RULI. This is helpful in differentiating similar queries submitted 
 * within an application context using different resolver instances.
 *************************************************************************/
int ti_dns_get_resctx(int ti_res_handle)
{
    ti_resolver_ctx_t*   ti_res_ctx = (ti_resolver_ctx_t *)ti_res_handle;

    /* Validate input parameters */
    assert(ti_res_ctx);

    return (int)ti_res_ctx->ruli_res_ctx;
}

/**************************************************************************
 * FUNCTION NAME : ti_dns_cancel_hostname_query
 **************************************************************************
 * DESCRIPTION   :
 * Given a TI resolver handle and a query ID, this API cancels the query.
 * On success, it returns 0 and -1 otherwise.
 *************************************************************************/
int ti_dns_cancel_hostname_query(int ti_res_handle, int query_id)
{
    ti_resolver_ctx_t*   ti_res_ctx = (ti_resolver_ctx_t *)ti_res_handle;
    ruli_res_query_t*    qry = NULL;
    ruli_res_t          *res_ctx = NULL;

    if(!ti_res_ctx || query_id < 0)
        return -1;
    
    res_ctx = ti_res_ctx->ruli_res_ctx;

    if(!res_ctx || (qry = ruli_res_find_query_by_id(&res_ctx->query_list, query_id)) == NULL)
    {
        printf("ERROR: No HostName query found with ID %d within resolver %d \n", query_id, ti_res_handle);
        return -1;
    }

    /* Destroy query */
    ruli_res_query_delete(qry);
  
    /* Free buffers */
    ruli_free(qry->q_domain);
    ruli_free(qry);
    qry = NULL;
   
    printf("Done deleting HostName query %d \n", query_id);

    return 0;
}

/**************************************************************************
 * FUNCTION NAME : ti_dns_cancel_srvrr_query
 **************************************************************************
 * DESCRIPTION   :
 * Given a TI resolver handle and a query ID, this API cancels the SRVRR 
 * query. On success, it returns 0 and -1 otherwise.
 *************************************************************************/
int ti_dns_cancel_srvrr_query(int ti_res_handle, int query_id)
{
    ti_resolver_ctx_t*   ti_res_ctx = (ti_resolver_ctx_t *)ti_res_handle;
    ruli_res_query_t*    qry = NULL;
    ruli_res_t          *res_ctx = NULL;

    if(!ti_res_ctx || query_id < 0)
        return -1;
    
    res_ctx = ti_res_ctx->ruli_res_ctx;

    if(!res_ctx || (qry = ruli_res_find_query_by_id(&res_ctx->query_list, query_id)) == NULL)
    {
        printf("ERROR: No SRVRR query found with ID %d within resolver %d \n", query_id, ti_res_handle);
        return -1;
    }

    /* Destroy the SRV query */
    if(qry->q_on_answer_arg)
        ruli_srv_query_delete(qry->q_on_answer_arg);
    qry = NULL;
   
    printf("Done deleting SRV RR query %d \n", query_id);

    return 0;
}
