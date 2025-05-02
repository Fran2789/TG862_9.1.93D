/*******************************************************************************
 * FILE PURPOSE:    RTOS HAL 
 *******************************************************************************
 * FILE NAME:       rtos_hal.h
 *
 * DESCRIPTION:     This include file contains definitions to abstract the
 *                  RTOS for the TIUHAL & TIUDRV layer.
 *               
 *
 * (C) Copyright 2003-2008, Texas Instruments, Inc.
 ******************************************************************************/


#ifndef __RTOS_HAL_H__
#define __RTOS_HAL_H__
#ifdef __KERNEL__
#include <linux/module.h>
#include <linux/kernel.h>
#include <stdarg.h>
#include <linux/vmalloc.h>
#include <linux/string.h>
#include <asm-arm/arch-avalanche/generic/haltypes.h>
#include <asm-arm/arch-avalanche/generic/soc.h>

/*******************************************************************************
 *
 * System servies needed by TIUHW
 *
 ******************************************************************************/

#define DRV_MDELAY(X,Y)			mdelay((Y))
#define DRV_DEBUG_MSG(SPY_KEY,...)		printk(KERN_INFO __VA_ARGS__ )
#define DRV_ERROR_MSG(SPY_KEY,...)		printk(KERN_ERR __VA_ARGS__ )	
#define DRV_TRACE_MSG(...)		    /* DO NOTHING */
/*#define DRV_TRACE_MSG(...)		    printk(KERN_ERR __VA_ARGS__ )  */

#define DRV_ALLOC(X)			vmalloc((X))
#define DRV_FREE(X)				vfree((X))
#define	DRV_GETENV(X)			
#define DRV_STRTOL(X,Y,Z)			simple_strtol((X),(Y),(Z))

#define INT_CONNECT(name,intr,hndlr)    \
    ((request_irq(intr,hndlr,0,name,NULL)) ? FALSE : TRUE)

#define INT_DISCONNECT(intr)            \
    free_irq(intr,NULL)

#define INT_HANDLER_PROTO(name)         \
    void name(int irq, void *dev_id, struct pt_regs *reg)
    
/*******************************************************************************
 *
 * local types needed by TIUHW that are NOT present in haltypes.h
 *
 ******************************************************************************/
#ifndef SINT8
#define SINT8 INT8
#endif

#ifndef SINT16
#define SINT16 short
#endif

#ifndef TCID
#define TCID   UINT8
#endif

#ifndef SINT32
#define SINT32 INT32
#endif

#ifndef CHAR
#define CHAR char
#endif

#ifndef OK
#define OK TRUE
#endif

#ifndef NOK
#define NOK FALSE
#endif

#ifndef PRIVATE
#define PRIVATE static
#endif
#else 

/*******************************************************************************
 *  Userspace version of the above kernel funcitonality/macros...
 ******************************************************************************/
#include "swpform.h"
#include "ggconfig.h"
#include "tsgtcid.h"
#define DRV_MDELAY(X,Y)			XtSleep(0,MSEC_TO_TICKS(Y))
#define DRV_DEBUG_MSG(SPY_KEY,...)		XdbgPrintf(__VA_ARGS__ )
#define DRV_ERROR_MSG(SPY_KEY,...)		XdbgPrintf(__VA_ARGS__ )	
#define DRV_TRACE_MSG(...)		    /* DO NOTHING */
#define DRV_ALLOC(X)			malloc((X))
#define DRV_FREE(X)				free((X))
		
#define DRV_STRTOL(X,Y,Z)		strtol((X),(Y),(Z))
/* #define DRV_TRACE_MSG(...)		    XdbgPrintf(__VA_ARGS__)  */

#endif /* __KERNEL__ */
#endif /* __RTOS_HAL_H__ */
