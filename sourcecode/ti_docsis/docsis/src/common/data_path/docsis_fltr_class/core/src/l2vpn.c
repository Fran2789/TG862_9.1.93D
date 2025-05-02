/*
 *
 * l2vpn.c
 * Description:
 * L2VPN implementation
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

/*! \file l2vpn.c
*   \brief L2VPN implementation
*/
/**************************************************************************/
/*      INCLUDES:                                                         */
/**************************************************************************/

#include <linux/init.h>
#include <linux/types.h>
#include <linux/module.h>
#include <linux/kernel.h>
#include "l2vpn.h"
#include "dfltr_class_utils.h"
#include "dfltr_class_ctrl.h"
#include "dbridge_netdev.h"

/**************************************************************************/
/*      EXTERNS Declaration:                                              */
/**************************************************************************/

/**************************************************************************/
/*      DEFINES:                                                          */
/**************************************************************************/
#define L2VPN_SF_MAX_NUMBER 32
/**************************************************************************/
/*      LOCAL DECLARATIONS:                                               */
/**************************************************************************/

/**************************************************************************/
/*      LOCAL VARIABLES:                                                  */
/**************************************************************************/
static Uint8    L2vpnRelatedSf[L2VPN_SF_MAX_NUMBER];

/**************************************************************************/
/*      LOCAL PROTOTYPES:                                                 */
/**************************************************************************/

/**************************************************************************/
/*      INTERFACE FUNCTIONS                                               */
/**************************************************************************/

/**************************************************************************/
/*! \fn void L2vpn_Init(void)
 **************************************************************************
 *  \brief init l2vpn data
 *  \param[in] - None
 *  \param[out] - None 
 *  \return
 */
void L2vpn_Init(void)
{
    memset(L2vpnRelatedSf, 0, L2VPN_SF_MAX_NUMBER*sizeof(Uint8));
}

/**************************************************************************/
/*! \fn L2vpn_Set(int subtype, Uint32 param1, Uint32 param2, void *buf)
 **************************************************************************
 *  \brief set l2vpn data in internal DB
 *  \param[in] subtype - ioctl subtype
 *  \param[in] param1 - ioctl first param
 *  \param[in] param2 - ioctl second param
 *  \param[in] buf - ioctl data buffer
 *  \return OK
 */
int L2vpn_Set(int subtype, Uint32 param1, Uint32 param2, void *buf)
{
    int ret = 0;

    extern int intel_register_l2vpn_packet_handler(char *br_name, int (*packet_handler)(struct sk_buff *skb, int *l2vpnRelate)); 
    extern int QosClass_L2vpnData(struct sk_buff *skb, int *l2vpnRelate);
    switch (subtype) 
    {
    case L2VPN_S_SUBTYPE_SET_SF:
        /*param1 - sf index
          param2 - indication if SF related to L2VPN*/
        if (param1 < L2VPN_SF_MAX_NUMBER)
        {
            L2vpnRelatedSf[param1] = param2;
        }
        break;

    case L2VPN_S_SUBTYPE_CLEAR:
        L2vpn_Init();
        break;

    case L2VPN_S_REGISTER_BR_CB:
        if (param1)
        {
            /* Register Bridge L2VPN CB*/
            intel_register_l2vpn_packet_handler(LBRIGE_CONNECTION_DEV_NAME, QosClass_L2vpnData);
        }
        else
        {
            /* Unregister Bridge L2VPN CB*/
            intel_register_l2vpn_packet_handler(LBRIGE_CONNECTION_DEV_NAME, NULL);
        }
        break;

    default:
        return -EINVAL;
    }
    
    return ret;
}



/**************************************************************************/
/*! \fn L2vpn_GetRelatedSf(Int16 sfIndex, Int32* l2vpnRelate)
 **************************************************************************
 *  \brief get l2vpn indication for SF related to L2vpn
 *  \param[in] sfIndex - Sf index
 *  \param[out] l2vpnRelate - indication for SF related to L2vpn
 *  \return OK
 */
int L2vpn_GetRelatedSf(Int16 sfIndex, Int32* l2vpnRelate)
{
    if (sfIndex < L2VPN_SF_MAX_NUMBER)
    {
        *l2vpnRelate = L2vpnRelatedSf[sfIndex];
    }
    else
    {
        *l2vpnRelate = 0;
    }
    
    return 0;
}
