/*
 *
 * dbridge_l2vpn_ds_db.h
 * Description:
 * DOCSIS bridge L2VPN DS Forwarding database header file
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

#ifndef _DBRIDGE_L2VPN_DS_DB_H_
#define _DBRIDGE_L2VPN_DS_DB_H_

/**************************************************************************/
/*      INCLUDES                                                          */
/**************************************************************************/
#include <_tistdtypes.h>

/**************************************************************************/
/*  Various defines                                                       */
/**************************************************************************/

/**************************************************************************/
/*      INTERFACE  Defines and Structs                                    */
/**************************************************************************/

/**************************************************************************/
/*! \fn int DbridgeL2vpnDsDb_Init(void)                                     
 **************************************************************************
 *  \brief Docsis bridge L2VPN DS data base initialization.
 *  \param[in] no input.
 *  \param[out] no output.
 *  \return OK or error status.
 **************************************************************************/
int DbridgeL2vpnDsDb_Init(void);


/**************************************************************************/
/*! \fn int DbridgeL2vpnDsDb_SetSaid(Uint32 saidIndex, Uint32 cmim)                                   
 **************************************************************************
 *  \brief Set Docsis Bridge L2VPN related SAID with its CMIM.
 *  \param[in] saidIndex - SAID index
 *  \param[in] cmim - interface mask
 *  \param[out] no output.
 *  \return OK or error status.
 **************************************************************************/
int DbridgeL2vpnDsDb_SetSaid(Uint32 saidIndex, Uint32 cmim);


/**************************************************************************/
/*! \fn int DbridgeL2vpnDsDb_DelSaid(Uint32 saidIndex, Uint32 cmim)                                   
 **************************************************************************
 *  \brief Delete Docsis Bridge L2VPN related SAID
 *  \param[in] saidIndex - SAID index
 *  \param[out] no output.
 *  \return OK or error status.
 **************************************************************************/
int DbridgeL2vpnDsDb_DelSaid(Uint32 saidIndex);


/**************************************************************************/
/*! \fn int DbridgeL2vpnDsDb_SetDut(Bool isConfigured, Uint32 cmim)                                   
 **************************************************************************
 *  \brief Set Docsis Bridge L2VPN DUT with its CMIM.
 *  \param[in] isConfigured - True-configured, False-not configured
 *  \param[in] cmim - interface mask
 *  \param[out] no output.
 *  \return OK or error status.
 **************************************************************************/
int DbridgeL2vpnDsDb_SetDut(Bool isConfigured, Uint32 cmim);


/**************************************************************************/
/*! \fn int DbridgeL2vpnDsDb_Print(void)                                     
 **************************************************************************
 *  \brief Print DOCSIS bridge L2VPN DS data base.
 *  \param[in] no input.
 *  \param[out] no output.
 *  \return OK or error status.
 **************************************************************************/
int DbridgeL2vpnDsDb_Print(void);


/**************************************************************************/
/*! \fn int DbridgeL2vpnDb_GetSaidBitmaps(Uint32 saidIndex, Uint32* linuxCmim, Uint32* floodMap)                                     
 **************************************************************************
 *  \brief Get the CMIM and flood map bitmaps according to the SAID index.
 *  \param[in] saidIndex - SAID index value.
 *  \param[out] linuxCmim - Linux CMIM value to be returned.
 *  \param[out] floodMap - Flood Map value to be returned.
 *  \return OK or error status.
 **************************************************************************/
int DbridgeL2vpnDb_GetSaidBitmaps(Uint32 saidIndex, unsigned long long* linuxCmim, Uint32* floodMap);


/**************************************************************************/
/*! \fn int DbridgeL2vpnDb_GetDutConfigured(Bool *dutConfigured)
 **************************************************************************
 *  \brief Get DUT configured or not
 *  \param[out] dutConfigured - True=DUT configured, False=DUT not configured
 *  \return OK or error status.
 **************************************************************************/
int DbridgeL2vpnDb_GetDutConfigured(Bool *dutConfigured);


/**************************************************************************/
/*! \fn int DbridgeL2vpnDb_GetDutBitmaps(Bool *dutConfigured, Uint32* linuxCmim, Uint32* floodMap)
 **************************************************************************
 *  \brief Get the CMIM and flood map bitmaps according for DUT
 *  \param[out] linuxCmim - Linux CMIM value to be returned.
 *  \param[out] floodMap - Flood Map value to be returned.
 *  \return OK or error status.
 **************************************************************************/
int DbridgeL2vpnDb_GetDutBitmaps(unsigned long long* linuxCmim, Uint32* floodMap);


#endif

