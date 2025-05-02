/*
 *
 * dbridge_mac_ageing.h
 * Description:
 * DOCSIS bridge MAC ageing header file
 *
 * BSD LICENSE 
 *
 *  Copyright(c) 2012 Intel Corporation. All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without 
 * modification, are permitted provided that the following conditions 
 * are met:
 *
 *   * Redistributions of source code must retain the above copyright 
 *     notice, this list of conditions and the following disclaimer.
 *   * Redistributions in binary form must reproduce the above copyright 
 *     notice, this list of conditions and the following disclaimer in 
 *     the documentation and/or other materials provided with the 
 *     distribution.
 *   * Neither the name of Intel Corporation nor the names of its 
 *     contributors may be used to endorse or promote products derived 
 *     from this software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS 
 * "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT 
 * LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR 
 * A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT 
 * OWNER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, 
 * SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT 
 * LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, 
 * DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY 
 * THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT 
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE 
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
*/

#ifndef _DBRIDGE_MAC_AGEING_H_
#define _DBRIDGE_MAC_AGEING_H_

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
/*! \fn int DbridgeMacAgeing_Init(void)                                     
 **************************************************************************
 *  \brief Docsis bridge MAC Ageing initialization.
 *  \param[in] no input.
 *  \param[out] no output.
 *  \return OK or error status.
 **************************************************************************/
int DbridgeMacAgeing_Init(void);

/**************************************************************************/
/*! \fn int DbridgeMacAgeing_HandleEvent(Bool isOperUp, Char *name)                                    
 **************************************************************************
 *  \brief Docsis bridge MAC Ageing initialization.
 *  \param[in] no input.
 *  \param[out] no output.
 *  \return OK or error status.
 **************************************************************************/
int DbridgeMacAgeing_HandleEvent(Bool isOperUp, Uint32 sysIndex);

/**************************************************************************/
/*! \fn int DbridgeMacAgeing_SetStatusAndTimer(Bool macAgeingEnable, Uint32 timerValue)                                    
 **************************************************************************
 *  \brief set Docsis bridge MAC Ageing configuration.
 *  \param[in] macAgeingEnable - enable / disable status.
 *  \param[in] timerValue - holdoff timer value.
 *  \return OK or error status.
 **************************************************************************/
int DbridgeMacAgeing_SetStatusAndTimer(Bool macAgeingEnable, Uint32 timerValue);

#endif

