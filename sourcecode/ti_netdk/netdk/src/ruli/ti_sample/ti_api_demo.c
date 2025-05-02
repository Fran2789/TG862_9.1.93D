/**************************************************************************
 * FILE PURPOSE	:  	ti_api_demo
 **************************************************************************
 * FILE NAME	:   ti_api_demo.c
 *
 * DESCRIPTION	:
 * Sample Program to demonstrate the use of various TI API Wrappers for 
 * Host Name / SRV RR Query Submissions. Illustrates the use of following APIs:
 *      1. ti_dns_init_resolver
 *      2. ti_dns_query_hostname
 *      3. ti_dns_query_srvrr
 *      4. ti_dns_exit_resolver
 *      5. ti_dns_start_query_scheduler
 *      6. ti_dns_parse_hostname_answer
 *      7. ti_dns_parse_srvrr_answer
 *      8. ti_dns_clean_hostname_answer
 *      9. ti_dns_clean_srvrr_answer
 *     10. ti_dns_get_hostname_qid
 *     11. ti_dns_get_hostname_resctx
 *     12. ti_dns_get_srvrr_qid
 *     13. ti_dns_get_srvrr_resctx
 *     14. ti_dns_get_resctx
 *     15. ti_dns_cancel_hostname_query
 *     16. ti_dns_cancel_srvrr_query
 *
 *	CALL-INs:
 *
 *	CALL-OUTs:
 *
 *	User-Configurable Items:
 *
  GPL LICENSE SUMMARY

  Copyright(c) 2012-2013 Intel Corporation.

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
#include <pthread.h>

#include "stdout_srv_list.h"

#undef TI_API_DEMO_DBG 

/**************************************************************************
 * FUNCTION NAME : on_answer_hostname
 **************************************************************************
 * DESCRIPTION   :
 * Call back function. This function is called when any event is received 
 * by the RULI library from Liboop. Events could be anything like a 
 * DNS answer / socket error / timeout etc. This function thus should:
 *  1.  Allocate ti_hostname_answer_t data structure to hold the IP addresses 
 *      received in the answer with other info like TTL etc.
 *  2.  Call ti_dns_parse_hostname_answer API which parses the answers and
 *      returns the error code of type TI_DNS_ERROR_CODE. If no error, fills 
 *      the ti_hostname_answer_t data structures with the IP addresses received.
 *  3.  Check if any error is returned and if so free the ti_hostname_answer_t 
 *      data structure and retry later.
 *  4.  If no error, process the configuration received.
 *  5.  Finally free ti_hostname_answer_t data structure allocated and call 
 *      ti_clean_hostname_query() to clean the call back arguments.
 *************************************************************************/
static void *on_answer_hostname(int qry_buf, void *arg)
{
    ti_hostname_answer_t* hostname_answer = NULL;
    int ret = -1;
    int j = 0, q_id = -1, res_ctx = -1;

    /* Retrieve query id and RULI resolver context if needed */
    q_id = ti_dns_get_hostname_qid(qry_buf);
    res_ctx = ti_dns_get_hostname_resctx(qry_buf);
#ifdef TI_API_DEMO_DBG    
    printf("on_answer_hostname q_id = %d res_ctx = %d\n", q_id, res_ctx);
#endif    
    
    /* Allocate space for holding the DNS answer */
    if((hostname_answer = (ti_hostname_answer_t *) ruli_malloc(sizeof(ti_hostname_answer_t))) == NULL) {
        /* Call ti_dns_clean_hostname_query to free the query and buffers */
        ti_dns_clean_hostname_query(qry_buf, arg);
        return OOP_CONTINUE;
    }

    memset((void *)hostname_answer, 0, sizeof(ti_hostname_answer_t));

    /* Initalize the IP address list */
    if(ruli_list_new(&hostname_answer->addr_list)) {
        ruli_free(hostname_answer);
        hostname_answer = NULL;
        /* Call ti_dns_clean_hostname_query to free the query and buffers */
        ti_dns_clean_hostname_query(qry_buf, arg);
        return OOP_CONTINUE;
    }

    /* Call ti_dns_parse_hostname_answer to validate the answer and
     * return the appropriate error code. If no error, this API fills
     * up the addr_list with all the IP addresses returned by DNS server
     * in response to hostname queried.
     */
    ret = ti_dns_parse_hostname_answer(qry_buf, arg, hostname_answer);

    printf("HostName queried: %s \n", (char *) arg);
    printf("Ret from parse_hostname_answer: (%d)%s  sizeof addr_list: %d \n", ret, ti_dns_error2str(ret),ruli_list_size(&hostname_answer->addr_list));

    if(ret == TI_DNS_SUCCESS)
    {
        /* No error - go ahead and extract the addresses */
        for (j = 0; j < ruli_list_size(&hostname_answer->addr_list); ++j) {
            ruli_addr_t *addr = (ruli_addr_t *) ruli_list_get(&hostname_answer->addr_list, j);
            switch (ruli_addr_family(addr)) {
                case PF_INET:
	                printf("IPv4/");
	                break;
                case PF_INET6:
	                printf("IPv6/");
	                break;
                default:
    	            printf("?/");
            }
            ruli_addr_print(stdout, addr);
            printf(" ");
            printf("TTL: %u", hostname_answer->ttl);
            printf(" ");
        }

        printf("\n");
    }
    else
    {
        /* error - do something */
    }

    /* Call ti_dns_clean_hostname_query to free the query and buffers */
    ti_dns_clean_hostname_query(qry_buf, arg);

    /* Free the server list */
    if(ruli_list_size(&hostname_answer->addr_list) > 0)
        ruli_list_dispose_trivial(&hostname_answer->addr_list);

    /* Free the answer buffers */
    ruli_free(hostname_answer);
    hostname_answer = NULL;

    return OOP_CONTINUE;
}

/**************************************************************************
 * FUNCTION NAME : on_answer_srvrr
 **************************************************************************
 * DESCRIPTION   :
 * Call back function. This function is called when any event is received 
 * by the RULI library from Liboop. Events could be anything like a 
 * DNS answer / socket error / timeout etc. This function thus should:
 *  1.  Call ti_dns_parse_srvrr_answer with the callback arguments 
 *      received. This API parses the answer and returns an error code of
 *      type TI_DNS_ERROR_CODE.
 *  2.  Check if any error is returned and if so do the appropriate
 *      error handling; 
 *  3.  If no error, process the configuration received. 
 *  4.  Finally call ti_dns_clean_srvrr_query before returning 
 *      from callback function to clean up the call back function 
 *      arguments.
 *************************************************************************/
static void *on_answer_srvrr(int search_buf, void *search_arg)
{
    srv_qbuf_t *qbuf = (srv_qbuf_t *) search_arg;
    ruli_list_t* srv_list = NULL;
    int ret = -1, q_id = -1, res_ctx = -1;

    /* Make sure that the argument to call back function is valid */
    assert(search_buf);

    /* Call ti_dns_parse_srvrr_answer to validate the answer and
     * return the appropriate error code. If no error, obtain 
     * a pointer to the srv_list (SRV RR answer list) from the
     * query buffer. 
     */
    ret = ti_dns_parse_srvrr_answer(search_buf, search_arg);

    printf("Ret from parse_srvrr_answer: (%d)%s \n", ret, ti_dns_error2str(ret));

    /* Retrieve query id and RULI resolver context if needed */
    q_id = ti_dns_get_srvrr_qid(search_buf);
    res_ctx = ti_dns_get_srvrr_resctx(search_buf);
#ifdef TI_API_DEMO_DBG    
    printf("on_answer_srvrr q_id = %d res_ctx = %d\n", q_id, res_ctx);
#endif    

    /* Success case - Retrieve and display SRV list sorted accg to RFC 2782 */
    if(ret == TI_DNS_SUCCESS)
    {
        /* Obtain pointer to srv_list */
        srv_list = ruli_search_srv_answer_list(search_buf);

        printf("sizeof SRV RR answer list: %d \n", ruli_list_size(srv_list));

        char fullname[RULI_LIMIT_DNAME_TEXT_BUFSZ];

        snprintf(fullname, RULI_LIMIT_DNAME_TEXT_BUFSZ, "%s.%s",
               qbuf->txt_service, qbuf->txt_domain);

        /* Display the SRV address list */
        show_srv_list(fullname, srv_list);
    }
    else
    {
        /* error - do something */
    }

    /* Clean up the call bak fucntion arguments */
    ti_dns_clean_srvrr_query(search_buf, qbuf);

    return OOP_CONTINUE;
}

/**************************************************************************
 * FUNCTION NAME : main
 **************************************************************************
 * DESCRIPTION   :
 * Entry point to the sample program. Steps to be followed for using TI 
 * RULI API are:
 *  1.  call ti_dns_init_resolver(timeout, retry_count, interface_name,
 *                                serverc, serverv)
 *  2.  On return from init check, if returned res_handle is NULL. 
 *      If so, there was a failure in acessing resolver settings. Exit.
 *  3.  If no error (valid res handle), store the ti resolver context handle 
 *      returned by init.
 *  4.  Create a thread and invoke ti_dns_start_query scheduler from the
 *      thread context. This API is blocking, make sure you call it from
 *      a context separate from the application itself.This API schedules 
 *      and dispatches events in a loop. On any event trigger like 
 *      error/timeout/success, it calls back the callback function 
 *      registered by the event.
 *      Note: If the RULI scheduler is started and has no events to 
 *      will exit immediately since it wont have events to process.
 *  5.  Ready for query submission:
 *          a. To submit a host name query call:
 *              int ti_dns_query_hostname(int ti_res_handle, char *domain, 
 *                      void *(*call)(int qry,void *arg))
 *          b. To submit a SRVRR query call:
 *              int ti_query_srvrr(int ti_res_handle, char *domain, 
 *                      void *(*call)(int qry,void *arg))
 *      Check for error (-1). If so, call ti_exit_dnsclient for cleanup.
 *  6.  on_answer_hostname / on_answer_srvrr is called when an answer 
 *      is received / timeout occurs. Do necessary processing by calling
 *      ti_dns_parse_hostname_answer / ti_dns_parse_srvrr_answer to obtain
 *      error codes. Process the results. Call the appropriate cleanup 
 *      functions (ti_dns_clean_hostname_query / ti_dns_clean_srvrr_query) 
 *      to cleanup the call back function arguments and buffers.
 *  6.  Finally, call ti_exit_dnsclient(int ti_res_handle) to cleanup 
 *      the resolver context.
 *************************************************************************/
int main()
{
    int ret = 0;
    pthread_t* thread = NULL;
    pthread_t* thread_2 = NULL;
    int ti_res_handle = (int)NULL;
    int ti_res_handle_2 = (int)NULL;
    int res_ctx_1 = -1, res_ctx_2 = -1;
    int mode = 0, q_id = -1;
    char* serverv[5];

    printf("Enter your choice: \n");
    printf("1. Take DNS servers from the program\n");
    printf("2. Use resolv.conf\n");
    scanf("%d", &mode);
  
    switch(mode) {
        case 1:
                /* Read DNS servers from console/prog */
                serverv[0] = (char *)malloc(sizeof(char) * 15);
                strcpy(serverv[0], "192.168.21.134");
                ti_res_handle = ti_dns_init_resolver(10, 3, "dbr1", 1, serverv);
                res_ctx_1 = ti_dns_get_resctx(ti_res_handle);
                printf("res_ctx_1 = %d \n", res_ctx_1);
                ti_res_handle_2 = ti_dns_init_resolver(10, 3, "dbr1", 1, serverv);
                res_ctx_2 = ti_dns_get_resctx(ti_res_handle_2);
                printf("res_ctx_2 = %d \n", res_ctx_2);
                break;
        case 2:
                /* Read DNS servers from /etc/resolv.conf */
                ti_res_handle = ti_dns_init_resolver(10, 3, "dbr1", 0, NULL);
                break;
        default:
                /* Query scheduler using resolv.conf */
                ti_res_handle = ti_dns_init_resolver(10, 3, "dbr1", 0, NULL);
                break;
    }

    /* Validate the resolver handle. If not valid, cant proceed */
    if(!ti_res_handle) {
        printf("error in init resolver\n");
        exit(1);
    }

    /* Create the thread and configure it to run the scheduler API */
    if((thread = (pthread_t *)malloc(sizeof(pthread_t))) == NULL) {
        printf("pthread malloc error \n");
        exit(1);
    }
    if((thread_2 = (pthread_t *)malloc(sizeof(pthread_t))) == NULL) {
        printf("pthread malloc error \n");
        exit(1);
    }

    printf("Enter your choice: \n");
    printf("1. Send a hostname query \n");
    printf("2. Send a SRV RR query \n");
    printf("3. Send multiple queries (hostname + SRVRR) \n");
    scanf("%d", &mode);
  
    switch(mode) {
        case 1:
                printf("Submitting Hostname Queries \n");
                q_id = ti_dns_query_hostname(ti_res_handle, "phil-sip.server.com", on_answer_hostname);
                printf("q_id = %d \n", q_id);
                q_id = ti_dns_query_hostname(ti_res_handle_2, "labsip.server.com", on_answer_hostname);
                printf("q_id = %d \n", q_id);
                printf("Done submitting HostName Queries \n");
                break;
        case 2:
                printf("Submitting SRV RR queries \n");
                q_id = ti_dns_query_srvrr(ti_res_handle, "_http._tcp.server.com", on_answer_srvrr);
                printf("q_id = %d \n", q_id);
                q_id=ti_dns_query_srvrr(ti_res_handle, "_http._tcp.phil-sip.server.com", on_answer_srvrr);
                printf("q_id = %d \n", q_id);
                q_id=ti_dns_query_srvrr(ti_res_handle, "_sip._tcp.server.com", on_answer_srvrr);
                printf("q_id = %d \n", q_id);
                q_id=ti_dns_query_srvrr(ti_res_handle, "_sip._udp.server.com", on_answer_srvrr);
                printf("q_id = %d \n", q_id);
                q_id=ti_dns_query_srvrr(ti_res_handle, "_sip._tcp.labsip.server.com", on_answer_srvrr);
                printf("q_id = %d \n", q_id);
                printf("Done submitting SRV RR queries \n");
                break;
        case 3:
                printf("Submitting HostName + SRVRR queries \n");
                q_id=ti_dns_query_srvrr(ti_res_handle, "_http._tcp.server.com", on_answer_srvrr);
                printf("q_id = %d \n", q_id);
                q_id=ti_dns_query_srvrr(ti_res_handle, "_http._tcp.phil-sip.server.com", on_answer_srvrr);
                printf("q_id = %d \n", q_id);
                q_id=ti_dns_query_srvrr(ti_res_handle, "_sip._tcp.server.com", on_answer_srvrr);
                printf("q_id = %d \n", q_id);
                q_id=ti_dns_query_srvrr(ti_res_handle, "_sip._udp.server.com", on_answer_srvrr);
                printf("q_id = %d \n", q_id);
                q_id=ti_dns_query_srvrr(ti_res_handle, "_sip._tcp.labsip.server.com", on_answer_srvrr);
                printf("q_id = %d \n", q_id);
                q_id=ti_dns_query_hostname(ti_res_handle, "phil-sip.server.com", on_answer_hostname);
                printf("q_id = %d \n", q_id);
                q_id=ti_dns_query_hostname(ti_res_handle, "labsip.server.com", on_answer_hostname);
                printf("q_id = %d \n", q_id);
                printf("Done submitting HostName + SRVRR queries \n");
                break;
        default:
                printf("Submitting HostName + SRVRR queries \n");
                ti_dns_query_srvrr(ti_res_handle, "_http._tcp.server.com", on_answer_srvrr);
                ti_dns_query_srvrr(ti_res_handle, "_http._tcp.phil-sip.server.com", on_answer_srvrr);
                ti_dns_query_srvrr(ti_res_handle, "_sip._tcp.server.com", on_answer_srvrr);
                ti_dns_query_srvrr(ti_res_handle, "_sip._udp.server.com", on_answer_srvrr);
                ti_dns_query_srvrr(ti_res_handle, "_sip._tcp.labsip.server.com", on_answer_srvrr);
                ti_dns_query_hostname(ti_res_handle, "phil-sip.server.com", on_answer_hostname);
                ti_dns_query_hostname(ti_res_handle, "labsip.server.com", on_answer_hostname);
                printf("Done submitting HostName + SRVRR queries \n");
                break;
    }

    ret = pthread_create(thread, NULL, &ti_dns_start_query_scheduler, (void *)ti_res_handle);
    ret = pthread_create(thread_2, NULL, &ti_dns_start_query_scheduler, (void *)ti_res_handle_2);

    if(thread)
        pthread_join(*thread, NULL);
    if(thread_2)
        pthread_join(*thread_2, NULL);

    /* Destroy the resolver context before creating another one */
    ti_dns_exit_resolver(ti_res_handle);
    ti_dns_exit_resolver(ti_res_handle_2);

    exit(0);
}
 
   
