/*
 *
 * dpp_counters_ctl.h
 * Description:
 * DOCSIS Packet Processor counters control header file
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

#ifndef _DPP_COUNTERS_CTL_H_
#define _DPP_COUNTERS_CTL_H_

/**************************************************************************/
/*      INCLUDES                                                          */
/**************************************************************************/

/**************************************************************************/
/*      INTERFACE  Defines and Structs                                    */
/**************************************************************************/

#define DPP_COUNTERS_IOCTL_MAGIC			'f'
#define DPP_COUNTERS_IOCTL_MAX_NUM           4

/*! \var typedef struct DppCountersIoctlData DppCountersIoctlData_t
    \brief Structure defines the format of the the Packet Processor counters ioctl data structure. 
*/
typedef struct DppCountersIoctlData
{
	int buf_len;		/* buffer length */
	void   *buf;	/* data buffer   */

}DppCountersIoctlData_t;


/************************** Main Ioctl commands *************************/

/* get counters ioctls */
#define DPP_COUNTERS_G_DEV          _IOR(DPP_COUNTERS_IOCTL_MAGIC, 0, DppCountersIoctlData_t)
#define DPP_COUNTERS_G_TPPORT       _IOR(DPP_COUNTERS_IOCTL_MAGIC, 1, DppCountersIoctlData_t)
#define DPP_COUNTERS_G_SF           _IOR(DPP_COUNTERS_IOCTL_MAGIC, 2, DppCountersIoctlData_t)

/* print counters ioctls */
#define DPP_COUNTERS_P_DEV          _IOW(DPP_COUNTERS_IOCTL_MAGIC, 3, DppCountersIoctlData_t)
#define DPP_COUNTERS_P_TPPORT       _IOW(DPP_COUNTERS_IOCTL_MAGIC, 4, DppCountersIoctlData_t)

/**************************************************************************/
/*      INTERFACE FUNCTIONS Prototypes:                                   */
/**************************************************************************/

/*! \fn int DppCountersCtl_Init(void)
 *  \brief DOCSIS Packet Processor counters Ctl initialization.
 *  \param[in] no input.
 *  \param[out] no output.
 *  \return 0 or error code.
 */
int DppCountersCtl_Init(void);

/*! \fn int DppCountersCtl_Exit(void)
 *  \brief DOCSIS Packet Processor counters Ctl exit.
 *  \param[in] no input.
 *  \param[out] no output.
 *  \return 0 or error code.
 */
int DppCountersCtl_Exit(void);

#endif
