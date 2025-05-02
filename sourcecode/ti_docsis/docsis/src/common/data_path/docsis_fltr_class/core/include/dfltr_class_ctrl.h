/*
 *
 * dfltr_class_ctrl.h
 * Description:
 * Protocol filters/QOS classifiers control header file
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

#ifndef _FC_CTRL_H_
#define _FC_CTRL_H_

/**************************************************************************/
/*      INCLUDES                                                          */
/**************************************************************************/
#include <_tistdtypes.h>

/**************************************************************************/
/*      INTERFACE  Defines and Structs                                    */
/**************************************************************************/
/*Filter/Classifier module ioctl data structure*/
typedef struct fc_ioctl_data_s
{
    int subtype;        /* ioctl command subtype */
    int buf_len;        /* buffer length */
    Uint32 param1;      /* additional interger parameter*/
    Uint32 param2;      /* additional interger parameter*/
    void  *buf;         /* data buffer   */
}fc_ioctl_data_t;

#define FC_IOCTL_MAGIC          'f'
/***************************Main Ioctl commands**************************/
/*Filter ioctls*/
#define FLTRIOC_S_LLC       _IOW(FC_IOCTL_MAGIC, 0, fc_ioctl_data_t)
#define FLTRIOC_G_LLC       _IOR(FC_IOCTL_MAGIC, 1, fc_ioctl_data_t)
#define FLTRIOC_S_IP        _IOW(FC_IOCTL_MAGIC, 2, fc_ioctl_data_t)
#define FLTRIOC_G_IP        _IOR(FC_IOCTL_MAGIC, 3, fc_ioctl_data_t)
/*Classifiers ioctls*/
#define CLIOC_S_CL          _IOW(FC_IOCTL_MAGIC, 4, fc_ioctl_data_t)
#define CLIOC_G_CL          _IOR(FC_IOCTL_MAGIC, 5, fc_ioctl_data_t)
/*Interface Table ioctls*/
#define FCIOC_S_IF          _IOW(FC_IOCTL_MAGIC, 6, fc_ioctl_data_t)
#define FCIOC_G_IF          _IOR(FC_IOCTL_MAGIC, 7, fc_ioctl_data_t)
/*Start ioctl*/
#define FCIOC_S_START       _IOW(FC_IOCTL_MAGIC, 8, fc_ioctl_data_t)
#define FLTRIOC_S_STP       _IOW(FC_IOCTL_MAGIC, 9, fc_ioctl_data_t)
/*PHS ioctls*/
#define US_PHSIOC_S_RULE    _IOW(FC_IOCTL_MAGIC, 10,fc_ioctl_data_t)
/*L2VPN ioctls*/
#define L2VPN_S_CL          _IOW(FC_IOCTL_MAGIC, 11, fc_ioctl_data_t)

/*L2VPN ioctls*/
#define FCIOC_R_IF          _IOW(FC_IOCTL_MAGIC, 12, fc_ioctl_data_t)
/*Restart ioctl*/
#define FCIOC_S_RESTART       _IOW(FC_IOCTL_MAGIC, 13, fc_ioctl_data_t)

/*Reset Interface Table ioctls*/
#ifdef DSG
#define FLTRIOC_S_DSG        _IOW(FC_IOCTL_MAGIC, 14, fc_ioctl_data_t)
#define FLTRIOC_G_DSG        _IOR(FC_IOCTL_MAGIC, 15, fc_ioctl_data_t)
#define FC_IOC_MAX_NUM          15
#else
#define FC_IOC_MAX_NUM          13
#endif

/* Set ioctl subtypes*/
#define FLTRIOC_S_SUBTYPE_ADD           0 /*add filter*/
#define FLTRIOC_S_SUBTYPE_DEL           1 /*delete filter*/
#define FLTRIOC_S_SUBTYPE_UPD           2 /*update filter*/
#define FLTRIOC_S_SUBTYPE_UNMATCH_ACT   3 /*get action in case of unmatched*/
#define FLTRIOC_S_SUBTYPE_IPFLTR_ENABLED    4 /*set IP filters enabled flag*/
#define FLTRIOC_S_SUBTYPE_PRINT         5 /*print filters pp counters */
#define FLTRIOC_S_SUBTYPE_PRINT_FILTER_DATA         6 /*print filters data */

#define CLIOC_S_SUBTYPE_ADD             0 /*add classifier*/
#define CLIOC_S_SUBTYPE_DEL             1 /*delete classifer*/
#define CLIOC_S_SUBTYPE_UPD             2 /*update classifer*/
#define CLIOC_S_SUBTYPE_PRINT           3 /*print classifer pp counters */

#define US_PHSIOC_S_SUBTYPE_ADD         0 /*add an upstream PHS Rule*/
#define US_PHSIOC_S_SUBTYPE_DEL         1 /*delete an upstream PHS Rule*/

#define L2VPN_S_SUBTYPE_SET_SF          0 /*set L2vpn SF*/
#define L2VPN_S_SUBTYPE_CLEAR           1 /*clear L2VPN info*/
#define L2VPN_S_REGISTER_BR_CB          2 /*Un/Register L2VPN bridge packet handler CB*/

/* Get ioctl subtypes*/
#define FLTRIOC_G_SUBTYPE_FLTR          0 /*get filter*/
#define FLTRIOC_G_SUBTYPE_MATCHES       1 /*get num filter matches*/
#define FLTRIOC_G_SUBTYPE_UNMATCH_ACT   2 /*get action in case of unmatched*/
#define FLTRIOC_G_SUBTYPE_PP_MATCHES    3 /*get packet processor num filter matches*/
#define CLIOC_G_SUBTYPE_CLASS           0 /*get classifier*/
#define CLIOC_G_SUBTYPE_MATCHES         1 /*get num classifiers matches*/
#define CLIOC_G_SUBTYPE_PP_MATCHES      2 /*get packet processor num classifiers matches*/

#endif
