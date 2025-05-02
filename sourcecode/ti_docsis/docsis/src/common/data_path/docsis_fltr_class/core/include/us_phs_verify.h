/*
 *
 * us_phs_verify.h
 * Description:
 * Upstream PHS-Verify header file
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

/*! \file us_phs_verify.h
*   \brief Upstream PHS-Verify header file
*          Note: PHS (Payload Header Suppression) is implemented in hardware, 
*          this file deals with upstream PHS verification only 
*          (this is implemented by software).
*/

#ifndef _US_PHS_VERIFY_H_
#define _US_PHS_VERIFY_H_

/**************************************************************************/
/*      INCLUDES                                                          */
/**************************************************************************/
#include <linux/skbuff.h>
#include <_tistdtypes.h>
#include "us_phs.h"

/**************************************************************************/
/*      INTERFACE  Defines and Structs                                    */
/**************************************************************************/

/**************************************************************************/
/*      INTERFACE  Function prototypes                                    */
/**************************************************************************/

/**************************************************************************/
/*! \fn         void UsPhsRules_Init(void)
 **************************************************************************
 *  \brief      Initialize Upstream PHS Rules table
 *  \return     None.
 */
void UsPhsRules_Init(void);


/**************************************************************************/
/*! \fn         int UsPhsRules_Set(int subtype, Uint32 param1, Uint32 param2, void *buf)
 **************************************************************************
 *  \brief      Set operations on upstream PHS Rules Table
 *  \param[in]  subtype - set command subtype:
 *                          - PHSIOC_S_SUBTYPE_ADD
 *                          - PHSIOC_S_SUBTYPE_DEL
 *
 *                      PHSIOC_S_SUBTYPE_ADD    PHSIOC_S_SUBTYPE_DEL
 *                      --------------------    --------------------
 *  \param[in]  param1  Rule internal index     Rule internal index
 *  \param[in]  param2  Not used                Not used           
 *  \param[in]  buf     PHS rule to add         Not used    
 *  \param[in]  bufLen  Size of buf             Not used
 *                      containingthe rule
 *
 *  \param[out] None.
 *  \return     0 or error code
 */
int  UsPhsRules_Set(int subtype, Uint32 param1, Uint32 param2, void *buf, int bufLen);


/**************************************************************************/
/*! \fn         int UsPhsVerify(struct sk_buff *skb, Uint32 phsIndex) 
 **************************************************************************
 *  \brief      Perform US PHS Verification for PHS rules with PHSV=0
 *
 *              Note:
 *              PHS verification is not supported in case of fragmented
 *              packet where the first fragment does not fully contain
 *              the PHS field (PHSF). In such cases PHS verification is
 *              not performed and Payload Header Suppression is not
 *              applied to the packet.
 *
 *  \param[in]  skb - Holds the US packet
 *  \param[in]  phsIndex - Internal index identifying the PHS rule to apply.
 *              Note: This is an internal index used to access the US
 *              PHS rules table. It is NOT PHSI.
 *  \param[out] None
 *  \return     0 if No verification needed or verification done successfully.
 *              -1 if verification failed.
 *              Note:
 *              Payload Header Suppression will be applied to the packet
 *              if and only if we return 0.
 */
int  UsPhsVerify(struct sk_buff *skb, Uint32 phsIndex);

#endif /* _US_PHS_VERIFY_H_ */




