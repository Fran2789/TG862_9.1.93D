/*
* FILE PURPOSE: Default definitions for tiudrv.h that may be overwritten by
*               the platform specific file.
* 
* Copyright 2007-2008, Texas Instruments, Inc.
*/
#ifndef __TIUHAL_CFG_ALL_H__
#define __TIUHAL_CFG_ALL_H__

/* Platform specific */
#define TIUHW_MAX_TCIDS 4    /* How many physical ports are there on the platform? */
#define TIUHW_MAX_TIDS        2    /* How many "TIDS" do we have? */
#define TIUHW_MIN_TIDS        1    /* How many "TIDS" do we have? */
#define TIUHW_CMN_CS_TYPE        1   /* Enable if using common chipselect type */

/* TIUHAL Specifc values */
#define TIUHAL_USE_CMN_REG_T /* 8bit */
#define TIUHAL_USE_CMN_DATA_T /* 8bit */
#define TIUHAL_HAS_SELECT_TID 1   /* set this if the platform provides a mechanism to steer the chipselect to a specific channel (selection)*/
#define TIUHAL_HAS_DESELECT_TID 1 /* set this if the platform provides a mechanism to steer the chipselect to a specific channel (deselection)*/
#undef  TIUHAL_HAS_TID_INTS       /* set this if the TIUHAL needs to support interrutps directly (normally for FPGA)*/ 
#undef  TIUHAL_HAS_DETECT         /* set this if the TIUHAL needs to support hook detection via a pin reading (normally for FPGA) */
#undef  TIUHAL_HAS_RINGER         /* set this if ring generation control is done via the HAL (FPGA/SOC) */
#undef  TIUHAL_HAS_TEARDOWN       /* set this if init() in TIUHAL allocated resources that need to be freed */
#define TIUHAL_USE_CMN_CAP_T 1    /* set this if using the common capaibilites structure */
#define TIUHAL_USE_CMN_INIT_ERR_T 1 /* set this if using the common initialization error types */
#define TIUHAL_USE_CMN_IO_ERROR_T 1 /* set this if using the common I/O error types */
#undef  TIUHAL_SUPPORT_TID_INTERRUPTS   /* set this if TIUHAL supports TID tinerrupts */
#endif

