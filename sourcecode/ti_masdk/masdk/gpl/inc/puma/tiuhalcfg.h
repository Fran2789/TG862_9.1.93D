/*
*  Copyright  2008 by Texas Instruments Incorporated.
*
* Description:
*   This file is used to configure TIUHAL options that may differ
*   from the defaults.
*
*/

#ifndef  TIUHALCFG_FILE_HEADER_INC
#define  TIUHALCFG_FILE_HEADER_INC

/* NOTE: DO NOT PLACE ANY OTHER HEADER FILES OTHER THAN THE ONES BELOW! */
#include "tiuhalcfg_all.h"


/******************************************************************************
* The definitions below are used to enable/disable certain environment
* variable support.  It is assumed the customer would disable as needed
* unneeded variables in their final product to cut down the code a bit.
******************************************************************************/
#ifndef  TIUHW_MAX_TIDS
#define TIUHW_MAX_TIDS      2
#endif

#ifndef  TIUHW_MIN_TIDS
#define TIUHW_MIN_TIDS      1
#endif

#define TIUHAL_SUPPORT_TID_INTERRUPTS 1
/***********************************************************************
* 
* The section below is TIUHAL specific.  On the P5 we're communicating
* via the  SOC's internal SPI and TDM interface, the defines below are used 
*
************************************************************************/

#define TIUHAL_HAS_TEARDOWN    1
#define TIUHAL_TDM_CLKOUT       2048000
#define TIUHAL_TDM_CLKIN       10240000
#define TIUHAL_PCM_FS          8000
#define TIUHAL_TDM_FS_PER      (TIUHAL_TDM_CLKOUT/TIUHAL_PCM_FS)

#define TIUHAL_SPI_MAX_CS 			(TIUHW_MAX_TIDS*2) /* How many chip select combinations are we dealing with? */
/* End of HAL specific section */

#ifndef  TIUHW_MAX_TCIDS
#define TIUHW_MAX_TCIDS    4
#endif
#endif   /* ----- #ifndef TIUHALCFG_FILE_HEADER_INC  ----- */

