/*
 *
 * dbridge_frwd_rules.h
 * Description:
 * Definitions and functions used for CM forwarding rules
 *
 * Copyright (C) 2008 Texas Instruments Incorporated - http://www.ti.com/ 
 * 
 * 
 *  Redistribution and use in source and binary forms, with or without 
 *  modification, are permitted provided that the following conditions 
 *  are met:
 *
 *    Redistributions of source code must retain the above copyright 
 *    notice, this list of conditions and the following disclaimer.
 *
 *    Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the 
 *    documentation and/or other materials provided with the   
 *    distribution.
 *
 *    Neither the name of Texas Instruments Incorporated nor the names of
 *    its contributors may be used to endorse or promote products derived
 *    from this software without specific prior written permission.
 *
 *  THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS 
 *  "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT 
 *  LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR
 *  A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT 
 *  OWNER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, 
 *  SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT 
 *  LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
 *  DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
 *  THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT 
 *  (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE 
 *  OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 *
*/

/*! \file dbridge_frwd_rules.h
*    \brief The file implements the CM Forwarding Rules:
*
*  1)CM Pre-Operational
*   - The CM MUST NOT send any DHCPv4 DHCPDISCOVER or DHCPREQUEST, DHCPv6 Solicit or Request,
*     TFTP-RRQ, HTTP Request, Time Protocol Request, or IPv6 Router Solicitation messages to any interface
*     except the RF Interface;
*
*   - The CM MUST NOT accept any DHCPv4 DHCPOFFER or DHCPACK, DHCPv6 Advertise or Reply, TFTPDATA,
*     HTTP Response, Time Protocol Response, or IPv6 Router Advertisements from the CMCI ports;
*
*  2)CM Operational
*   - Forwarding of frames received from any CPE port conforms to the following specific rules:
*     The CM MUST NOT accept any DHCPv4 DHCPOFFER or DHCPACK, DHCPv6 Advertise or Reply, TFTPDATA,
*     HTTP Response, Time Protocol Response, or IPv6 Router Advertisements from any of the CPE ports
*     for the purposes of configuration, secure software download, or address renewal.
*
*   - Forwarding of frames being sent by the CM IP stack conforms to the following specific rules:
*     The CM MUST NOT forward any DHCPv4 DHCPDISCOVER or DHCPREQUEST, DHCPv6 Solicit or
*     Request, TFTP-RRQ, HTTP Request, Time Protocol Request, or Router Solicitation messages to any ports
*     except the RF port.
*
***************************************************************************/
#ifndef DBRIDGE_FRWD_RULES_H
#define DBRIDGE_FRWD_RULES_H

#include <linux/kernel.h>
#include <linux/skbuff.h>

/* debug flag*/ 
#define DEBUG_FLT    0  

#if DEBUG_FLT  == 1
#define D_PRINTK(X)    printk X;
#else
#define D_PRINTK(X)
#endif

#define  TI_DHCPv4_DHCPDISCOVER_IND       0 
#define  TI_DHCPv4_DHCPREQUEST_IND        1 
#define  TI_DHCPv6_SOLICIT_IND            2 
#define  TI_DHCPv6_REQUEST_IND            3 
#define  TI_TFTP_RRQ_IND                  4 
#define  TI_HTTP_REQUEST_IND              5 
#define  TI_TIME_REQUEST_IND              6 
#define  TI_ROUTER_SOLICITATION_IND       7 
                                            
#define  TI_DHCPv4_DHCPOFFER_IND          8 
#define  TI_DHCPv4_DHCPACK_IND            9 
#define  TI_DHCPv6_ADVERTISE_IND          10
#define  TI_DHCPv6_REPLY_IND              11
#define  TI_TFTP_DATA_IND                 12
#define  TI_HTTP_RESPONSE_IND             13
#define  TI_TIME_RESPONSE_IND             14
#define  TI_ROUTER_ADVERTISEMENTS_IND     15

#define  TI_DHCPv4_DHCPDISCOVER       (1<<TI_DHCPv4_DHCPDISCOVER_IND)   /* = 0x1    = 1     */
#define  TI_DHCPv4_DHCPREQUEST        (1<<TI_DHCPv4_DHCPREQUEST_IND)    /* = 0x2    = 2     */
#define  TI_DHCPv6_SOLICIT            (1<<TI_DHCPv6_SOLICIT_IND)        /* = 0x4    = 4     */
#define  TI_DHCPv6_REQUEST            (1<<TI_DHCPv6_REQUEST_IND)        /* = 0x8    = 8     */
#define  TI_TFTP_RRQ                  (1<<TI_TFTP_RRQ_IND)              /* = 0x10   = 16    */
#define  TI_HTTP_REQUEST              (1<<TI_HTTP_REQUEST_IND)          /* = 0x20   = 32    */
#define  TI_TIME_REQUEST              (1<<TI_TIME_REQUEST_IND)          /* = 0x40   = 64    */
#define  TI_ROUTER_SOLICITATION       (1<<TI_ROUTER_SOLICITATION_IND)   /* = 0x80   = 128   */
                                                                                       
#define  TI_DHCPv4_DHCPOFFER          (1<<TI_DHCPv4_DHCPOFFER_IND)      /* = 0x100  = 256   */
#define  TI_DHCPv4_DHCPACK            (1<<TI_DHCPv4_DHCPACK_IND)        /* = 0x200  = 512   */
#define  TI_DHCPv6_ADVERTISE          (1<<TI_DHCPv6_ADVERTISE_IND)      /* = 0x400  = 1024  */
#define  TI_DHCPv6_REPLY              (1<<TI_DHCPv6_REPLY_IND)          /* = 0x800  = 2048  */
#define  TI_TFTP_DATA                 (1<<TI_TFTP_DATA_IND)             /* = 0x1000 = 4096  */
#define  TI_HTTP_RESPONSE             (1<<TI_HTTP_RESPONSE_IND)         /* = 0x2000 = 8192  */
#define  TI_TIME_RESPONSE             (1<<TI_TIME_RESPONSE_IND)         /* = 0x4000 = 16384 */
#define  TI_ROUTER_ADVERTISEMENTS     (1<<TI_ROUTER_ADVERTISEMENTS_IND) /* = 0x8000 = 32768 */

#define TI_FWRD_RULE_IPSTACK_TO_RF    (TI_DHCPv4_DHCPDISCOVER | TI_DHCPv4_DHCPREQUEST | \
                                     TI_DHCPv6_SOLICIT | TI_DHCPv6_REQUEST | TI_TFTP_RRQ | \
                                     TI_HTTP_REQUEST | TI_TIME_REQUEST | TI_ROUTER_SOLICITATION)

#define TI_FWRD_RULE_FROM_CPE        (TI_DHCPv4_DHCPOFFER | TI_DHCPv4_DHCPACK | \
                                    TI_DHCPv6_ADVERTISE | TI_DHCPv6_REPLY | \
                                    TI_TFTP_DATA | TI_HTTP_RESPONSE | \
                                    TI_TIME_RESPONSE | TI_ROUTER_ADVERTISEMENTS)
// ARRIS define a set of rules that allows TFTP download over the LAN for use by root user
#define TI_FWRD_RULE_IPSTACK_TO_RF_ALLOW_TFTP    TI_DHCPv4_DHCPDISCOVER | TI_DHCPv4_DHCPREQUEST | \
                                     TI_DHCPv6_SOLICIT | TI_DHCPv6_REQUEST | \
                                     TI_HTTP_REQUEST | TI_TIME_REQUEST | TI_ROUTER_SOLICITATION
#define TI_FWRD_RULE_FROM_CPE_ALLOW_TFTP        TI_DHCPv4_DHCPOFFER | TI_DHCPv4_DHCPACK | \
                                    TI_DHCPv6_ADVERTISE | TI_DHCPv6_REPLY | \
                                     TI_HTTP_RESPONSE | \
                                    TI_TIME_RESPONSE | TI_ROUTER_ADVERTISEMENTS
// END ARRIS

struct fwrd_rule{
    int opt_code;
    bool (*do_rule)(const struct sk_buff* skb);
};                                  

 
/**************************************************************************/
/*! \fn bool apply_fwrd_rule(const struct fwrd_rule *rules, unsigned int opt_code,  
 *             unsigned int rule_first, unsigned int rule_last, const struct sk_buff *skb)
 **************************************************************************
 *  \brief  The function checks the input packet according to the given rules.
 *     If the packet is macth to the on of rules the function return true,
 *     otherwise - false. The search process is continuing to the first match.
 *     The function will be search between the "rule_first" and "rule_last". This values
 *     taking from the structure which keeps all rules("struct fwrd_rule *rules")
 *     The input data arraived in the "struct sk_buff *skb".
 * 
 *  \param[in] const struct fwrd_rule *rules - pointer to the array of the rules
 *  \param[in] unsigned int opt_code - option code(bit mask) of the rule or combination of the 
 *                 rules for the look for
 *  \param[in] unsigned int rule_first - start index of the rules array for the search.
 *  \param[in] unsigned int rule_last - last index in the rules array for the search.
 *             The search includes the last rule. 
 *  \param[in] const struct sk_buff *skb
 *  \return true or false
 **************************************************************************/

bool apply_fwrd_rule(const struct fwrd_rule *rules, unsigned int opt_code,  
                    unsigned int rule_first_index, unsigned int rule_last_index, const struct sk_buff *skb);

#endif
