/*
 *
 * us_phs.h
 * Description:
 * Upstream PHS header file
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

/*! \file us_phs.h
*   \brief Upstream PHS header file
*          Note: PHS (Payload Header Suppression) is implemented in hardware, 
*          this file is actually necessary for upstream PHS verification only 
*          (this is implemented by software): It defines the data structures
*          necessary at kernel-level in order to suport US PHS Verification.
*/

#ifndef _US_PHS_H_
#define _US_PHS_H_

/**************************************************************************/
/*      INCLUDES                                                          */
/**************************************************************************/
#include "_tistdtypes.h"

/**************************************************************************/
/*      INTERFACE  Defines and Structs                                    */
/**************************************************************************/

/* PHS Rules Constants derived from Puma-5 hardware definitions         */
/* Note: The following 3 defines must be in accordance with             */
/* the corresponding definitions in "hal_phs.h" (can't include it here) */
#define PHS_MAX_RULES               32 /* (HAL_PHS_MAX_ENTRIES_NUM)     */
#define PHS_MASK_MAX_SIZE           8  /* (HAL_PHS_MASK_MAX_SIZE)       */
#define PHS_FIELD_MAX_SIZE          64 /* (HAL_PHS_FIELD_MAX_SIZE)      */

/*! \var    typedef struct UsPhsRule_t
    \brief  PHS Rule structure:
            Used to keep Upstream PHS Rules in kernel-level in order to support
            US PHS verification. Note: We keep here only rule parameters
            needed for PHS verification. PHSI for instance is not needed.
*/
typedef struct {
    Uint8 isValid:1;                        /* PHS Rule exists for this phsIndex */
    Uint8 phsVerify:1;                      /* PHSV: 0 - Do verify, 1 - Don't verify */
    Uint8 phsSize;                          /* PHSS */
    Uint8 phsMask[PHS_MASK_MAX_SIZE];       /* PHSM */
    Uint8 phsField[PHS_FIELD_MAX_SIZE];     /* PHSF */
} UsPhsRule_t;

#define US_PHS_RULE_SIZE   (sizeof(UsPhsRule_t))

/**************************************************************************/
/*      INTERFACE  Function prototypes                                    */
/**************************************************************************/


#endif /* _US_PHS_H_ */




