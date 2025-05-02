/***************************************************************************
**+----------------------------------------------------------------------+**
**|                                ****                                  |**
**|                                ****                                  |**
**|                                ******o***                            |**
**|                          ********_///_****                           |**
**|                           ***** /_//_/ ****                          |**
**|                            ** ** (__/ ****                           |**
**|                                *********                             |**
**|                                 ****                                 |**
**|                                  ***                                 |**
**|                                                                      |**
**|     Copyright (c) 2006 - 2007    Texas Instruments Incorporated      |**
**|                        ALL RIGHTS RESERVED                           |**
**|                                                                      |**
**| Permission is hereby granted to licensees of Texas Instruments       |**
**| Incorporated (TI) products to use this computer program for the sole |**
**| purpose of implementing a licensee product based on TI products.     |**
**| No other rights to reproduce, use, or disseminate this computer      |**
**| program, whether in part or in whole, are granted.                   |**
**|                                                                      |**
**| TI makes no representation or warranties with respect to the         |**
**| performance of this computer program, and specifically disclaims     |**
**| any responsibility for any damages, special or consequential,        |**
**| connected with the use of this program.                              |**
**|                                                                      |**
**+----------------------------------------------------------------------+**
***************************************************************************/

/*****************************************************************************/
/*! \file sys_event.h
    \brief This file contains the ranges of events for all system components
******************************************************************************/

#ifndef _SYS_EVENT_H_
#define _SYS_EVENT_H_

#include "sys_ctx.h"

/*! \def TI_EVENT_RANGE
    \brief Event range for each component */
#define TI_EVENT_RANGE    0x1000
 
/*! \def TI_EVENT_OFFSET
    \brief Returns the event offset for each component
    \param[in] component accepts components from TI_COMPONENT_ID_e */
#define TI_EVENT_OFFSET( component ) ( component * TI_EVENT_RANGE )

#endif /* _SYS_EVENT_H_ */
