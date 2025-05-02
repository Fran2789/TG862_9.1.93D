/*
 *
 * gw_parental_control_db.h
 * Description:
 * DOCSIS GW parental control database header file
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

#ifndef _GW_PARENTAL_CONTROL_DB_H_
#define _GW_PARENTAL_CONTROL_DB_H_



/**************************************************************************/
/*      INCLUDES                                                          */
/**************************************************************************/
#include <_tistdtypes.h>

/**************************************************************************/
/*  Various defines                                                       */
/**************************************************************************/
typedef enum
{
    PARENTAL_CONTROL_DEV_HOOK_NONE         = 0x00000000,
    PARENTAL_CONTROL_DEV_HOOK_KEYWORD_FILT = 0x00000001,
    PARENTAL_CONTROL_DEV_HOOK_CPE_MAC_FILT = 0x00000002,
    PARENTAL_CONTROL_DEV_HOOK_TRUSTED_MAC  = 0x00000004
} GwParentalControlDevHookFeatures_e;

/**************************************************************************/
/*      INTERFACE  Defines and Structs                                    */
/**************************************************************************/

/**************************************************************************/
/*! \fn int GwParentalControlDb_Init(void)                                     
 **************************************************************************
 *  \brief GW parental control data base initialization.
 *  \param[in] no input.
 *  \param[out] no output.
 *  \return OK or error status.
 **************************************************************************/
int GwParentalControlDb_Init(void);

/**************************************************************************/
/*! \fn int GwParentalControlDb_SetEnableStatus(Uint32 status)                                   
 **************************************************************************
 *  \brief Set GW parental control enable status.
 *  \param[in] status -  enable status
 *  \param[out] no output.
 *  \return OK or error status.
 **************************************************************************/
int GwParentalControlDb_SetEnableStatus(Uint32 status, Uint8* buf, int bufLen);

/**************************************************************************/
/*! \fn int GwParentalControlDb_AddMac(Uint8 *buf, int bufLen)                                   
 **************************************************************************
 *  \brief Set trusted mac address.
 *  \param[in] buf - mac address
 *  \param[in] bufLen - buffer len
 *  \param[out] no output.
 *  \return OK or error status.
 **************************************************************************/
int GwParentalControlDb_AddMac(Uint8 *buf, int bufLen);


/**************************************************************************/
/*! \fn int GwParentalControlDb_DelMac(Uint8 *buf, int bufLen)                                  
 **************************************************************************
 *  \brief delete trusted mac address.
 *  \param[in] buf - mac address
 *  \param[in] bufLen - buffer len
 *  \param[out] no output.
 *  \return OK or error status.
 **************************************************************************/
int GwParentalControlDb_DelMac(Uint8 *buf, int bufLen);

/**************************************************************************/
/*! \fn int GwParentalControlDb_Print(void)                                     
 **************************************************************************
 *  \brief Print GW parental control data base.
 *  \param[in] no input.
 *  \param[out] no output.
 *  \return OK or error status.
 **************************************************************************/
int GwParentalControlDb_Print(void);

/**************************************************************************/
/*! \fn int GwParentalControlDb_FlushAllMac(void)                                     
 **************************************************************************
 *  \brief delete all trusted mac addresses from data base.
 *  \param[in] no input.
 *  \param[out] no output.
 *  \return OK or error status.
 **************************************************************************/
int GwParentalControlDb_FlushAllMac(void);


/**************************************************************************/
/*! \fn int GwCpeMacFilteringDb_AddMac(Uint8 *buf, int bufLen)                                   
 **************************************************************************
 *  \brief Adds a MAC address to the WAN filtering DB.
 *  \param[in] buf - mac address
 *  \param[in] bufLen - buffer len
 *  \param[out] no output.
 *  \return OK or error status.
 **************************************************************************/
int GwCpeMacFilteringDb_AddMac(Uint8 *buf, int bufLen);

/**************************************************************************/
/*! \fn int GwCpeMacFilteringDb_DelMac(Uint8 *buf, int bufLen)                                  
 **************************************************************************
 *  \brief Delete a MAC address from the WAN filtering DB.
 *  \param[in] buf - mac address
 *  \param[in] bufLen - buffer len
 *  \param[out] no output.
 *  \return OK or error status.
 **************************************************************************/
int GwCpeMacFilteringDb_DelMac(Uint8 *buf, int bufLen);

/**************************************************************************/
/*! \fn int GwCpeMacFilteringDb_FlushAllMac(void)                                     
 **************************************************************************
 *  \brief delete all mac addresses from the WAN filtering DB.
 *  \param[in] no input.
 *  \param[out] no output.
 *  \return OK or error status.
 **************************************************************************/
int GwCpeMacFilteringDb_FlushAllMac(void);

/**************************************************************************/
/*! \fn int GwCpeMacFilteringDb_Print(void)                                     
 **************************************************************************
 *  \brief Print GW parental control data base.
 *  \param[in] no input.
 *  \param[out] no output.
 *  \return OK or error status.
 **************************************************************************/
int GwCpeMacFilteringDb_Print(void);

#endif

