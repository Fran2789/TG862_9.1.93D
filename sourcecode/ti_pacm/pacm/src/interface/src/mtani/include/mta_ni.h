/*
 *
 * mta_ni.h
 * Description:
 * MTA network device header file
 *
 * Copyright (C) 2009 Texas Instruments Incorporated - http://www.ti.com/
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

#ifndef __MTA_NI_H__
#define __MTA_NI_H__

#include <autoconf.h>
#include <linux/sockios.h>

#ifdef __KERNEL__
#include <linux/skbuff.h>

/**************************************************************************/
/*      INTERFACE FUNCTIONS Prototypes:                                   */
/**************************************************************************/
int mta_rx (struct sk_buff *skb);

#endif

/**************************************************************************/
/*      INTERFACE IOCTLs:                                                 */
/**************************************************************************/
#define SIOCSETVOICEPORTS         (SIOCDEVPRIVATE + 0) /* set the ports of active calls */
#define SIOCGETVOICEPORTS    	  (SIOCDEVPRIVATE + 1) /* get the ports of active calls */
#define SIOCSETVOICEDROPPORTS     (SIOCDEVPRIVATE + 2) /* set ports pair for dropping packets */
#define SIOCGETVOICEDROPPORTS     (SIOCDEVPRIVATE + 3) /* set ports pair for dropping packets */
#define SIOCSETBOOTP              (SIOCDEVPRIVATE + 4) /* set bootp */


#define VOICE_DROP_PORT_SRC 0
#define VOICE_DROP_PORT_DST 1

struct mtani_data
{
    union
    {
        unsigned short voice_port[ CONFIG_TI_PACM_MAX_NUMBER_OF_ENDPOINTS * 2 ];
		unsigned short voice_drop_ports[2];
        unsigned char  voice_dhcp_active;
    };
};



#endif /* __MTA_NI_H__ */

