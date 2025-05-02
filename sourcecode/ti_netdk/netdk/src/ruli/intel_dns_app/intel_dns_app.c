/*

  BSD LICENSE 

  Copyright(c) 2016 Intel Corporation. All rights reserved.

  Redistribution and use in source and binary forms, with or without 
  modification, are permitted provided that the following conditions 
  are met:

    * Redistributions of source code must retain the above copyright 
      notice, this list of conditions and the following disclaimer.
    * Redistributions in binary form must reproduce the above copyright 
      notice, this list of conditions and the following disclaimer in 
      the documentation and/or other materials provided with the 
      distribution.
    * Neither the name of Intel Corporation nor the names of its 
      contributors may be used to endorse or promote products derived 
      from this software without specific prior written permission.

  THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS 
  "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT 
  LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR 
  A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT 
  OWNER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, 
  SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT 
  LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, 
  DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY 
  THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT 
  (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE 
  OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.

*/

#define _INTEL_DNS_APP_C_

/*! \file intel_dns_app.c
    \brief A template for all C files
*/

/**************************************************************************/
/*      INCLUDES:                                                         */
/**************************************************************************/

#include <stdlib.h>
#include <arpa/inet.h>

#include "sys_types.h"

#include "ruli.h"
#include "ti_api.h"

/**************************************************************************/
/*      EXTERNS Declaration:                                              */
/**************************************************************************/

/**************************************************************************/
/*      DEFINES:                                                          */
/**************************************************************************/

/* Size limits */
#define DNS_SRV_MAX_ADDR_PER_ENTRY      10
#define DNS_SRV_MAX_ENTRIES             10

/* Num elements in array */
#define ARRAY_NUM_ELEMENTS(__arr) (sizeof(__arr) / sizeof(__arr[0]))

/* Output devices */
#define WR_LOG printf
#define WR_RES(__prfx, args...) printf(__prfx args); printf("\n")

/* LOGs */
#define WR_LOG_GENERIC(__prfx, __format, args...) WR_LOG(__prfx "DNS - %s:%s() - %d : " __format "\n", __FILE__, __func__, __LINE__, ##args)
#define LOGE(__format, args...) WR_LOG_GENERIC(OUT_PREFIX(LOGE), __format, ##args)
#define LOGW(__format, args...) WR_LOG_GENERIC(OUT_PREFIX(LOGW), __format, ##args)
#define LOGI(__format, args...) WR_LOG_GENERIC(OUT_PREFIX(LOGI), __format, ##args)
#define LOGD(__format, args...) WR_LOG_GENERIC(OUT_PREFIX(LOGD), __format, ##args)

/* Output prefixes */
#define OUT_PREFIX(__prefix) #__prefix ":"

/* Result prefixes */
#define RESPRE_STATUS       OUT_PREFIX(STATUS)
#define RESPRE_A_RES        OUT_PREFIX(A_RES)
#define RESPRE_IP           OUT_PREFIX(IP)
#define RESPRE_FAMILY       OUT_PREFIX(FAMILY)
#define RESPRE_TTL          OUT_PREFIX(TTL)
#define RESPRE_SRV_RES      OUT_PREFIX(SRV_RES)
#define RESPRE_QDOMAIN      OUT_PREFIX(QDOMAIN)
#define RESPRE_NUM_ENTRIES  OUT_PREFIX(NUM_ENTRIES)
#define RESPRE_DOMAIN       OUT_PREFIX(DOMAIN)
#define RESPRE_NUM_IP       OUT_PREFIX(NUM_IP)
#define RESPRE_PORT         OUT_PREFIX(PORT)

/* Result values */
#define RESVAL_STATUS_OK    "OK"
#define RESVAL_STATUS_NOK   "NOK"
#define RESVAL_FAMILY_IPV4  "IPV4"
#define RESVAL_FAMILY_IPV6  "IPV6"

/* Query types */
#define __ENUM(__name, __str) __name,
#define __STR(__name, __str) __str,
#define QUERY_TYPES(__ENTRY) \
    __ENTRY(QTYP_A, "A") \
    __ENTRY(QTYP_AAAA, "AAAA") \
    __ENTRY(QTYP_SRV_4, "SRV_4") \
    __ENTRY(QTYP_SRV_6, "SRV_6") \

/*! \var typedef enum queryTypes_e
    \brief Query types
*/
typedef enum
{
    QUERY_TYPES(__ENUM)

    QTYP_NUM_QUERY_TYPES
} queryTypes_e; 

/* Input query type strings */
static Char *qtypStr[] = 
{
    QUERY_TYPES(__STR)

    /* Last */
    NULL
};

/*! \var typedef enum inpType_e
    \brief Type of input
*/
typedef enum
{
    INTYP_NONE,
    INTYP_STRING,
    INTYP_STRING_ENUM,
    INTYP_UINT,
    INTYP_STRING_LIST,

    INTYP_NUM_IN_TYPES,
} inpType_e; 

/*! \var typedef struct inParams_t
    \brief List of input params and actions
*/
typedef struct
{
    inpType_e   inpType;
    void        *paramSpecificData;
    void        *outputVarP;
    Char        *descr;
} inParams_t; 

/*! \struct srvAddrEntry_t
 *  /brief  Address info a single server
 */
typedef struct
{
    InetAddr_t ip_address;
    Uint16 port;
} srvAddrEntry_t;

/*! \struct srvEntry_t
 *  /brief  Resolved info of a single domain
 */
typedef struct
{
    Char            domainName[RULI_LIMIT_DNAME_TEXT_BUFSZ];
    Int32           numAddresses;
    srvAddrEntry_t  addr[DNS_SRV_MAX_ADDR_PER_ENTRY];
    Int32           ttl;
}srvEntry_t;

/*! \struct srvRsp_t
 *  /brief  Response of an SRV query
 */
typedef struct
{
    Char         queryDomainFullname[RULI_LIMIT_DNAME_TEXT_BUFSZ];
    Int32        numEntries;
    srvEntry_t   entries[DNS_SRV_MAX_ENTRIES];
    Int32        srvRetStatus;
}srvRsp_t;

/*! \struct DnsDb_t
 *  /brief  Query results
 */
typedef struct
{
    ti_hostname_answer_t*   queryAResp;
    srvRsp_t*               querySrvResp;
    TI_DNS_ERROR_CODE       queryRetStatus;
} DnsDb_t;

/**************************************************************************/
/*      LOCAL DECLARATIONS:                                               */
/**************************************************************************/

/**************************************************************************/
/*      LOCAL VARIABLES:                                                  */
/**************************************************************************/

static Char *dsnStatusStr[] = 
{
    [TI_DNS_SUCCESS] = "SUCCESS",
    [TI_DNS_TIMEOUT] = "TIMEOUT",
    [TI_DNS_ANSWER_UNPARSEABLE] = "ANSWER_UNPARSEABLE",
    [TI_DNS_MISSING_ADDRESS] = "MISSING_ADDRESS",
    [TI_DNS_SRV_CODE_UNAVAIL] = "SRV_CODE_UNAVAIL",
    [TI_DNS_QUERY_FAILURE] = "QUERY_FAILURE",
    [TI_DNS_ERROR] = "ERROR",
};

static DnsDb_t DnsDb;

static Char *inDomain;
static Char *inNetworkDevice;
static queryTypes_e inQtype;
static Uint32 inDnsTimeout;
static Uint32 inDnsRetries;
static Uint32 inNumDnsServers;
static Char **inDnsServers;
static Uint32 inIgnoreDnsRDFlag;  // ARRIS ADD for PD 17105

static inParams_t inParams[] = 
{
    /* The order is the expected order of params */

    /* Domain name */
    { INTYP_STRING,         NULL,               &inDomain,          "Domain" },
    { INTYP_STRING,         NULL,               &inNetworkDevice,   "Network device" },
    { INTYP_STRING_ENUM,    qtypStr,            &inQtype,           "Query type (A, AAAA, SRV_4, SRV_6)" },
    { INTYP_UINT,           NULL,               &inIgnoreDnsRDFlag, "Ignore DNS RD validation flag" },
    { INTYP_UINT,           NULL,               &inDnsTimeout,      "DNS Timeout" },
    { INTYP_UINT,           NULL,               &inDnsRetries,      "DNS Retries" },
    { INTYP_UINT,           NULL,               &inNumDnsServers,   "Num DNS servers" },
    { INTYP_STRING_LIST,    &inNumDnsServers,   &inDnsServers,      "DNS Servers" },

    /* Last */
    { INTYP_NONE }
};

/**************************************************************************/
/*      INTERFACE FUNCTIONS Implementation:                               */
/**************************************************************************/

/**************************************************************************/
/*      LOCAL FUNCTIONS:                                                  */
/**************************************************************************/

/**************************************************************************/
/*! \fn STATUS getParams(int argc, char *argv[])
 **************************************************************************
 *  \brief Get and validate input params
 *  \param[in] argc - number of params
 *  \param[in] argv - params
 *  \return OK/NOK
 **************************************************************************/
STATUS getParams(int argc, char *argv[])
{
    Int currArgC;
    Char *currArgV;
    void *p;
    Char **strarrp;
    int i;
    inParams_t *currInParams;
    
    /* argv[0] is balanced with { INTYP_NONE } */
    /* Exact num params is not known yet, just the min */
    if (argc < ARRAY_NUM_ELEMENTS(inParams))
    {
        LOGE("Got %d params, expected at least %d", argc - 1, ARRAY_NUM_ELEMENTS(inParams) - 1);
        return STATUS_NOK;
    }

    /* Skip argv[0] */
    for (currArgC = 1, currInParams = inParams; (currInParams->inpType != INTYP_NONE) && (currArgC < ARRAY_NUM_ELEMENTS(inParams)); currArgC++, currInParams++) 
    {
        if (currInParams->outputVarP == NULL)
        {
            LOGE("Missing output param for param %d", currArgC);
            return STATUS_NOK;
        }
        currArgV = argv[currArgC];
        switch (currInParams->inpType) 
        {
            case INTYP_STRING:
                p = strdup(currArgV);
                if (p == NULL)
                {
                    LOGE("Failed to alloc mem(%d) for input STRING param %d", strlen(currArgV), currArgC);
                    return STATUS_NOK;
                }
                *((Char **)(currInParams->outputVarP)) = p;
                break;
            case INTYP_STRING_ENUM:
                if (currInParams->paramSpecificData == NULL)
                {
                    LOGE("Missing string-enum list for param %d", currArgC);
                    return STATUS_NOK;
                }
                strarrp = ((Char **)(currInParams->paramSpecificData));
                for (i = 0; (strarrp[i] != NULL) && (strcmp(strarrp[i], currArgV) != 0); i++)
                    ;
                if (strarrp[i] == NULL)
                {
                    LOGE("%s is not a legal input for parameter %d", currArgV, currArgC);
                    return STATUS_NOK;
                }
                *((Int *)(currInParams->outputVarP)) = i;
                break;
            case INTYP_UINT:
                *((Int *)(currInParams->outputVarP)) = strtoul(currArgV, NULL, 0);
                break;
            case INTYP_STRING_LIST:
                {
                    if (currInParams->paramSpecificData == NULL)
                    {
                        LOGE("Missing string list len for param %d", currArgC);
                        return STATUS_NOK;
                    }
                    Uint32 numStrs = *((Int *)(currInParams->paramSpecificData));
                    /* Do we have enough remaining params? */
                    if (numStrs > (argc - currArgC))
                    {
                        LOGE("Not enough params for string list, required %d, remaining %d", numStrs, (argc - currArgC));
                        return STATUS_NOK;
                    }
                    strarrp = malloc(numStrs * sizeof(Char *));
                    if (strarrp == NULL)
                    {
                        LOGE("Failed to alloc mem(%d) for input STRING LIST param %d", numStrs * sizeof(Char *), currArgC);
                        return STATUS_NOK;
                    }
                    for (i = 0; i < numStrs; i++)
                    {
                        strarrp[i] = strdup(argv[currArgC + i]);
                        if (strarrp[i] == NULL)
                        {
                            LOGE("Failed to alloc mem(%d) for input STRING LIST element %d param %d", 
                                 strlen(argv[currArgC + i]), i, currArgC);
                            return STATUS_NOK;
                        }
                    }
                    *((Char ***)(currInParams->outputVarP)) = strarrp;
                    currArgC += i - 1; /* currArgC is on the 1st string, do not count it twice */
                }
                break;
            default:
                LOGE("Illegal INTYP %d", currInParams->inpType);
                return STATUS_NOK;
                break;
        }
    }

    if (currArgC != argc)
    {
        /* Extra unprocessed params */
        LOGW("Got %d params, expected %d, ignoring...", argc, currArgC);
    }

    return STATUS_OK;
}

/**************************************************************************/
/*! \fn void *DNSAnswerHandle(Int32 qryBuf, void *arg)
 **************************************************************************
 *  \brief  This function is called when any event is received
 *      by the RULI library from Liboop. Events could be anything like a DNS answer
 *      / socket error / timeout etc. This function thus:
 *  1.  Allocate addr_list data structure to hold the IP addresses received
 *      in the answer.
 *  2.  Call ti_dns_parse_hostname_answer API which parses the answers and
 *      returns the error code of type TI_DNS_ERROR_CODE. If no error, fills 
 *      the addr_list data structures with the IP addresses received.
 *  3.  Check if any error is returned and if so free the addr_list data 
 *      structure and retry later.
 *  4.  If no error, process the configuration received.
 *  5.  Finally free addr_list data structure allocated and call 
 *      ti_clean_hostname_query() to clean the call back arguments.  
 *
 *  \param[in]  qry_buf:
 *  \param[in]  arg:
 *  \param[out] no outputs
 *  \return     no return values                                    
 **************************************************************************/
static void *DNSAnswerHandle(Int32 qryBuf, void *arg)
{
    TI_DNS_ERROR_CODE ret = TI_DNS_ERROR;
    
    LOGI("received DNS response...parsing");

    /*Allocate space for holding the DNS answer */
    if((DnsDb.queryAResp = (ti_hostname_answer_t *) ruli_malloc(sizeof(ti_hostname_answer_t))) == NULL) 
    {
        LOGE("RULI response: failed to allocate memory");
        ti_dns_clean_hostname_query(qryBuf, arg);
        return OOP_CONTINUE;
    }
    memset((void *)DnsDb.queryAResp, 0, sizeof(ti_hostname_answer_t));

    /*Initalize the list*/
    if(ruli_list_new(&(DnsDb.queryAResp)->addr_list)) 
    {
        LOGE("RULI response: failed to initialize the list");
        ruli_free(DnsDb.queryAResp);
        DnsDb.queryAResp = NULL;
        ti_dns_clean_hostname_query(qryBuf, arg);
        return OOP_CONTINUE;
    }

    /* Call ti_dns_parse_hostname_answer to validate the answer and
     * return the appropriate error code. If no error, this API fills
     * up the addr_list with all the IP addresses returned by DNS server
     * in response to hostname queried.
     */
    ret = ti_dns_parse_hostname_answer(qryBuf, arg, DnsDb.queryAResp);

    LOGI("Ret from parse_hostname_answer: (%d)%s  sizeof addr_list: %d",
         ret, ti_dns_error2str(ret),ruli_list_size(&(DnsDb.queryAResp)->addr_list));

    if(ret != TI_DNS_SUCCESS)
    {
        LOGE("Error parsing RULI list");
        if(ruli_list_size(&(DnsDb.queryAResp)->addr_list) > 0)
            ruli_list_dispose_trivial(&(DnsDb.queryAResp)->addr_list);
        ruli_free(DnsDb.queryAResp);
        DnsDb.queryAResp = NULL;
    }

    DnsDb.queryRetStatus = ret;

    /*Call ti_dns_clean_hostname_query to free the query and buffers */
    ti_dns_clean_hostname_query(qryBuf, arg);
    
    return OOP_CONTINUE;
}

/**************************************************************************/
/*! \fn STATUS DNSfillSrvAnswer(const ruli_list_t *srv_list, srvRsp_t *answer)
 **************************************************************************
 *  \brief fills the answer structure with the data of the DNS SRV result 
 *  \param[in]  srv_list - ruli result data.
 *  \param[out] answer - structure to fill with data.
 *  \return STATUS_OK if succeed, STATUS_NOK otherwise.
 **************************************************************************/
static STATUS DNSfillSrvAnswer(const ruli_list_t *srv_list, srvRsp_t *answer)
{
    Int32 srvListSze = ruli_list_size(srv_list);
    Int32 i;
    Int32 numAddr;
    Int32 numActualEntries = 0;
    Bool gotAddress;

    if(!answer)
        return STATUS_NOK;

    if(srvListSze < 1)
    {
        LOGE("Could not resolve srv, list empty");
        return STATUS_OK;
    }


    for(i=0; ((i<srvListSze) && (numActualEntries<ARRAY_NUM_ELEMENTS(answer->entries))); ++i)
    {
        ruli_srv_entry_t *entry         = (ruli_srv_entry_t *) ruli_list_get(srv_list, i);
        ruli_list_t      *addr_list     = &entry->addr_list;
        Int32            addr_list_size = ruli_list_size(addr_list);
        Int32            j;
        Int32            txt_dname_len;

        if(ruli_dname_decode(answer->entries[numActualEntries].domainName, RULI_LIMIT_DNAME_TEXT_BUFSZ, &txt_dname_len, 
                             entry->target, entry->target_len))
        {
            LOGE("target-decoding-failed");
            continue;
        }
        LOGI("DNS resolution target=%s ", answer->entries[numActualEntries].domainName);

        gotAddress = False;
        for (j=0; ((j<addr_list_size) && (answer->entries[numActualEntries].numAddresses<ARRAY_NUM_ELEMENTS(answer->entries[numActualEntries].addr))); ++j)
        {
            ruli_addr_t *addr = (ruli_addr_t *) ruli_list_get(addr_list, j);

            numAddr = answer->entries[numActualEntries].numAddresses;
            switch (ruli_addr_family(addr))
            {
                case PF_INET:
                    memcpy(answer->entries[numActualEntries].addr[numAddr].ip_address.addr, &(addr->addr.ipv4.s_addr), INET_ADDRLEN);
                    answer->entries[numActualEntries].addr[numAddr].ip_address.family = AF_INET;
                    answer->entries[numActualEntries].addr[numAddr].port = entry->port;
                    answer->entries[numActualEntries].numAddresses++;
                    gotAddress = True;
                    break;
                case PF_INET6:
                    memcpy(answer->entries[numActualEntries].addr[numAddr].ip_address.addr, addr->addr.ipv6.s6_addr, INET6_ADDRLEN);
                    answer->entries[numActualEntries].addr[numAddr].ip_address.family = AF_INET6;
                    answer->entries[numActualEntries].addr[numAddr].port = entry->port;
                    answer->entries[numActualEntries].numAddresses++;
                    gotAddress = True;
                    break;
                default:
                    LOGE("Unsupported address family (%d) received", ruli_addr_family(addr));
                    continue;
            }
        }
        if(gotAddress == True)
        {
            answer->entries[numActualEntries].ttl = entry->ttl;
            numActualEntries++;
        }
            
    }
    answer->numEntries = numActualEntries;
    
    return STATUS_OK;
}

/**************************************************************************/
/*! \fn void *DNSSrvAnswerHandle(Int32 qryBuf, void *arg)
 **************************************************************************
 *  \brief  This function is called when any event is received
 *      by the RULI library from Liboop upon DNS service query. Events could
 *      be anything like a DNS answer / socket error / timeout etc.
 *      This function thus:
 *  1.  Allocate data structure to hold the IP addresses received
 *      in the answer.
 *  2.  Call ti_dns_parse_srvrr_answer API which parses the answers and
 *      returns the error code of type TI_DNS_ERROR_CODE.
 *  3.  Check if any error is returned and if so free the data 
 *      structure and retry later.
 *  4.  If no error, process the configuration received.
 *  5.  Finally free addr_list data structure allocated and call 
 *      ti_dns_clean_srvrr_query() to clean the call back arguments.  
 *
 *  \param[in]  qry_buf:
 *  \param[in]  arg:
 *  \param[out] no outputs
 *  \return     no return values                                    
 **************************************************************************/
static void *DNSSrvAnswerHandle(Int32 qryBuf, void *arg)
{
    
    LOGI("received DNS Srv response...parsing");

    /*Allocate space for holding the DNS answer */
    if((DnsDb.querySrvResp = (srvRsp_t *)ruli_malloc(sizeof(srvRsp_t))) == NULL) 
    {
        LOGE("RULI response: failed to allocate memory");
        ti_dns_clean_srvrr_query(qryBuf, arg);
        return OOP_CONTINUE;
    }
    memset((void *)DnsDb.querySrvResp, 0, sizeof(srvRsp_t));

    /* Call ti_dns_parse_srvrr_answer to validate the answer and
     * return the appropriate error code. If no error, obtain 
     * a pointer to the srv_list (SRV RR answer list) from the
     * query buffer. 
     */
    DnsDb.queryRetStatus = ti_dns_parse_srvrr_answer(qryBuf, arg);

    LOGI("Ret from Parse_srvrr_answer: (%d)%s",
         DnsDb.queryRetStatus, ti_dns_error2str(DnsDb.queryRetStatus));

    if(DnsDb.queryRetStatus == TI_DNS_SUCCESS)
    {
        srv_qbuf_t *qbuf = (srv_qbuf_t *)arg;
        ruli_list_t *srv_list = NULL;

        /*Get the server full name*/
        snprintf(DnsDb.querySrvResp->queryDomainFullname, RULI_LIMIT_DNAME_TEXT_BUFSZ, "%s.%s ", qbuf->txt_service, qbuf->txt_domain);

        /* Obtain pointer to srv_list */
        srv_list = ruli_search_srv_answer_list((ruli_search_srv_t *)qryBuf);
        LOGI("Sizeof SRV RR answer list: %d", ruli_list_size(srv_list));
        DNSfillSrvAnswer(srv_list, DnsDb.querySrvResp);
    }
    else
    {
        LOGE("Error parsing RULI SRV list");
    }
    
    /*Call ti_dns_clean_srvrr_query to free the query and buffers */
    ti_dns_clean_srvrr_query(qryBuf, arg);
    
    return OOP_CONTINUE;
}

/**************************************************************************/
/*! \fn STATUS writeQueryResultsSrv(void)
 **************************************************************************
 *  \brief Write the query results for SRV query
 *  \return OK/NOK
 **************************************************************************/
static STATUS writeQueryResultsSrv(void)
{
    Int32 entry;
    Int32 addr;
    srvRsp_t *querySrvResp = DnsDb.querySrvResp;
    srvEntry_t *currEntry;
    srvAddrEntry_t *currAddr;
    Char ipStr[INET6_ADDRSTRLEN];

    WR_RES(RESPRE_SRV_RES);
    WR_RES(RESPRE_QDOMAIN, "%s", querySrvResp->queryDomainFullname); 
    WR_RES(RESPRE_NUM_ENTRIES, "%d", querySrvResp->numEntries); 
    for (entry = 0, currEntry = querySrvResp->entries; entry < querySrvResp->numEntries; entry++, currEntry++) 
    {
        WR_RES(RESPRE_DOMAIN, "%s", currEntry->domainName);
        WR_RES(RESPRE_NUM_IP, "%d", currEntry->numAddresses); 
        for (addr = 0, currAddr = currEntry->addr; addr < currEntry->numAddresses; addr++, currAddr++) 
        {
            inet_ntop(currAddr->ip_address.family, currAddr->ip_address.addr, ipStr, sizeof(ipStr)); 
            WR_RES(RESPRE_FAMILY, "%s", currAddr->ip_address.family == AF_INET ? RESVAL_FAMILY_IPV4 : RESVAL_FAMILY_IPV6);
            WR_RES(RESPRE_IP, "%s", ipStr);
            WR_RES(RESPRE_PORT, "%d", currAddr->port);
        }
        WR_RES(RESPRE_TTL, "%d", currEntry->ttl); 
    }

    return STATUS_OK;
}

/**************************************************************************/
/*! \fn STATUS writeQueryResultsA(void)
 **************************************************************************
 *  \brief Write the query results for a/aaaa query
 *  \return OK/NOK
 **************************************************************************/
static STATUS writeQueryResultsA(void)
{
    ti_hostname_answer_t *hostnameAnswer = DnsDb.queryAResp; 
    Int32 i = 0;
    void *p;
    Uint8 family;
    Char ipStr[INET6_ADDRSTRLEN];
    Uint32 numEntries;

    if(hostnameAnswer != NULL)
    {
        WR_RES(RESPRE_A_RES);
        /*No errors - go ahead and extract the addresses*/
        numEntries = ruli_list_size(&hostnameAnswer->addr_list); 
        WR_RES(RESPRE_NUM_IP, "%d", numEntries);
        for (i = 0; i < numEntries; ++i) 
        {
            ruli_addr_t *addr = (ruli_addr_t *) ruli_list_get(&hostnameAnswer->addr_list, i);
            if ((ruli_addr_family(addr) == PF_INET) || (ruli_addr_family(addr) == PF_INET6))
            {
                if (ruli_addr_family(addr) == PF_INET)
                {
                    p = &(addr->addr.ipv4);
                    family = AF_INET;
                }
                else
                {
                    p = &(addr->addr.ipv6);
                    family = AF_INET6;
                }
                /* Output */
                inet_ntop(family, p, ipStr, sizeof(ipStr));
                LOGI("RULI found IP: %s, TTL %d", ipStr, hostnameAnswer->ttl);
                WR_RES(RESPRE_FAMILY, "%s", family == AF_INET ? RESVAL_FAMILY_IPV4 : RESVAL_FAMILY_IPV6);
                WR_RES(RESPRE_IP, "%s", ipStr);
            }
        }
        if (numEntries > 0)
        {
            WR_RES(RESPRE_TTL, "%d", hostnameAnswer->ttl);
        }
        return STATUS_OK;
    }

    LOGI("Server DNS query found no IP");

    return STATUS_OK; 
}

/**************************************************************************/
/*! \fn STATUS writeQueryResults(void)
 **************************************************************************
 *  \brief Write the query results
 *  \return OK/NOK
 **************************************************************************/
static STATUS writeQueryResults(void)
{
    if ((inQtype == QTYP_SRV_4) || (inQtype == QTYP_SRV_6))
    {
        return writeQueryResultsSrv();
    }
    else
    {
        return writeQueryResultsA();
    }
}

/**************************************************************************/
/*! \fn STATUS runQuery(void)
 **************************************************************************
 *  \brief Run the query
 *  \return OK/NOK
 **************************************************************************/
static TI_DNS_ERROR_CODE runQuery(void)
{
    Int32       ruliHandle = 0;
    Int32       queryId = -1;
    
    if(inDomain == NULL)
    {
        LOGE("NULL domain name to resolve");
        return TI_DNS_ERROR; 
    }

    LOGI("Initiate DNS resolution for domain %s query type %s", inDomain, qtypStr[inQtype]);

    /*Create a resolver handle*/
    ruliHandle = ti_dns_init_resolver(inDnsTimeout, inDnsRetries, inNetworkDevice, inNumDnsServers, inDnsServers);
    if(ruliHandle == 0)
    {
        LOGE("Failed to ti_dns_init_resolver()");
        return TI_DNS_ERROR; 
    }

    /*Send the query */
    if((inQtype == QTYP_SRV_4) || (inQtype == QTYP_SRV_6))
    {
        queryId = ti_dns_general_query_srvrr(ruliHandle, inDomain, (inQtype == QTYP_SRV_4) ? INET_ADDR_TYPE_IPV4 : INET_ADDR_TYPE_IPV6, DNSSrvAnswerHandle, inIgnoreDnsRDFlag); 
    }
    else
    {
        queryId = ti_dns_general_query_hostname(ruliHandle, inDomain, qtypStr[inQtype], DNSAnswerHandle, inIgnoreDnsRDFlag);
    }

    if(queryId == -1)
    {
        ti_dns_exit_resolver(ruliHandle);
        LOGE("Failed to submit DNS query");
        return TI_DNS_ERROR; 
    }
    
    LOGD("Starting DNS query thread");

    DnsDb.queryRetStatus = TI_DNS_ERROR;
        
    if(ti_dns_start_query_scheduler(ruliHandle) == NULL)
    {
        LOGE("Error starting ruli scheduler");
    }
    else
    {
        if (DnsDb.queryRetStatus == TI_DNS_SUCCESS) 
        {
            LOGI("DNS query completed");
        }
        else
        {
            LOGI("DNS query error %d - %s", DnsDb.queryRetStatus, ti_dns_error2str(DnsDb.queryRetStatus));
        }
    }

    return DnsDb.queryRetStatus;
}

/**************************************************************************/
/*! \fn void usage(void)
 **************************************************************************
 *  \brief Help
 *  \param[in] argc - number of params
 *  \param[in] argv - params
 *  \return NA
 **************************************************************************/
void usage(int argc, char *argv[]) 
{ 
    inParams_t *currInParams;

    printf("Usage : %s\n", argv[0]);
    for (currInParams = inParams; currInParams->inpType != INTYP_NONE; currInParams++) 
    {
        printf("\t%s\n", currInParams->descr); 
    }
}

/**************************************************************************/
/*! \fn int main(int argc, char *argv[])
 **************************************************************************
 *  \brief main
 *  \param[in] argc - number of params
 *  \param[in] argv - params
 *  \return exit()
 **************************************************************************/
int main(int argc, char *argv[])
{
    /*Init DNS internal DB*/
    memset(&DnsDb, 0 , sizeof(DnsDb_t));
    DnsDb.queryRetStatus = TI_DNS_ERROR;

    /* Get and validate input */
    if (getParams(argc, argv) == STATUS_OK)
    {
        /* Start query */
        runQuery();
        /* Write results */
        writeQueryResults();
    }
    else
    {
        usage(argc, argv);
    }

    /* Write status */
    WR_RES(RESPRE_STATUS, "%s", dsnStatusStr[DnsDb.queryRetStatus]); 

    return 0;
}
