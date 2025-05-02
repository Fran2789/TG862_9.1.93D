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
/*! \file sys_ctx.h
    \brief This file contains all definitions relevant for the system contexts
******************************************************************************/

#ifndef _SYS_CTX_H_
#define _SYS_CTX_H_

/*! \brief Component list in the system
    \enum typedef enum TI_COMPONENT_ID_e */

typedef enum
{
	TI_COMPONENT_UNDEFINED = -1,
 
	TI_COMPONENT_SYSTEM = 0,         /* System */
	TI_COMPONENT_DOCSIS,             /* DOCSIS */
	TI_COMPONENT_PACM,               /* PACM */
	TI_COMPONENT_COMMON_COMPONENTS,  /* Common components */
	TI_COMPONENT_VOICE,              /* Voice */
	TI_COMPONENT_PON_CMF,            /* Config Manager */
	TI_COMPONENT_VENDOR,             /* Vendor ID -- ARRIS - keep this before VFE */
	TI_COMPONENT_VFE,                /* VFE components -- ARRIS - this must come after vendor*/
    TI_COMPONENT_GW,                /* Gateway ID */
    TI_COMPONENT_HUNTER_CREEK,      /* HunterCreek ID */
#ifdef INCLUDE_ARRIS_GW_TR69
    TI_COMPONENT_TR69,                /* TR69 ID */ //ARRIS ADD
#endif

	TI_COMPONENT_LAST,               /* This must be the last entry */
 
} TI_COMPONENT_ID_e;

// ARRIS ADD - Add a handy redefinition for ARRIS use
#define TI_COMPONENT_ARRIS TI_COMPONENT_VENDOR
// END ARRIS ADD

/*! \def TI_CTX_RANGE
    \brief Context range for each component */
#define TI_CTX_RANGE    0x400
 
/*! \def TI_CTX_OFFSET
    \brief Returns the context offset of a component
    \param[in] component accepts components from TI_COMPONENT_ID_e */
#define TI_CTX_OFFSET( component ) ( component * TI_CTX_RANGE )
 
/*! \def TI_CTX_MODULE
    \brief Returns the module context of a component
    \param[in] component accepts components from TI_COMPONENT_ID_e
    \param[in] modules from each module TI_module_MODULE_ID_e */
#define TI_CTX_MODULE( component, module )  ( module + TI_CTX_OFFSET( component ) )

#endif /* _SYS_CTX_H_ */
