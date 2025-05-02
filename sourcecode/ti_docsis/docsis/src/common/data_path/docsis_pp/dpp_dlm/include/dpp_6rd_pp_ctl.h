/*
 *
 * dpp_6rd_pp_ctl.h
 * Description:
 * DOCSIS Packet Processor 6RD (protocol 41) control header file
 *
 * DESCRIPTION:   Provides DOCSIS Packet Processor 6RD support(protocol 41) 
 *                configuration and control function. 6RD PP support might bring some
 *                problematic effects, the feature need to be configurable.
 *
 *                Author: Brian Liu
 *
 * Copyright 2013, ARRIS Group, Inc., All rights reserved
*/

#ifndef _DPP_6RD_PP_CTL_H_
#define _DPP_6RD_PP_CTL_H_

/**************************************************************************/
/*      INCLUDES                                                          */
/**************************************************************************/

/**************************************************************************/
/*      INTERFACE  Defines and Structs                                    */
/**************************************************************************/

/**************************************************************************/
/*      INTERFACE FUNCTIONS Prototypes:                                   */
/**************************************************************************/

int ti_hil_enable_6rd_pp(void);
int ti_hil_disable_6rd_pp(void);

/*! \fn int Dpp6rdPPCtl_Init(void)
 *  \brief DOCSIS Packet Processor 6RD (protocol 41) Ctl initialization.
 *  \param[in] no input.
 *  \param[out] no output.
 *  \return 0 or error code.
 */
int Dpp6rdPPCtl_Init(void);

/*! \fn int Dpp6rdPPCtl_Exit(void)
 *  \brief DOCSIS Packet Processor 6RD (protocol 41) Ctl exit.
 *  \param[in] no input.
 *  \param[out] no output.
 *  \return 0 or error code.
 */
int Dpp6rdPPCtl_Exit(void);

#endif

