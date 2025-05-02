/*
 *
 * pal_sysTimer16.h
 * Description:
 * see below
 *
 *
 * Copyright (C) 2008, Texas Instruments, Incorporated
 *
 *  This program is free software; you can distribute it and/or modify it
 *  under the terms of the GNU General Public License (Version 2) as
 *  published by the Free Software Foundation.
 *
 *  This program is distributed in the hope it will be useful, but WITHOUT
 *  ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or
 *  FITNESS FOR A PARTICULAR PURPOSE.  See the GNU General Public License
 *  for more details.
 *
 *  You should have received a copy of the GNU General Public License along
 *  with this program; if not, write to the Free Software Foundation, Inc.,
 *  59 Temple Place - Suite 330, Boston MA 02111-1307, USA.
 *
 */

/******************************************************************************
 * FILE PURPOSE:    16 bit Timer Module Header
 ********************************************************************************
 * FILE NAME:       timer16.h
 *
 * DESCRIPTION:     Platform and OS independent API for 16 bit timer module
 *
 * REVISION HISTORY:
 * 27 Nov 02 - PSP TII  
 * 
 *******************************************************************************/

#ifndef __TIMER16_H__
#define __TIMER16_H__

/****************************************************************************
 * Type:        PAL_SYS_TIMER16_STRUCT_T
 ****************************************************************************
 * Description: This type defines the hardware configuration of the timer
 *              
 ***************************************************************************/
typedef struct PAL_SYS_TIMER16_STRUCT_tag
{
    UINT32 ctrl_reg;
    UINT32 load_reg;
    UINT32 count_reg;
    UINT32 intr_reg;
} PAL_SYS_TIMER16_STRUCT_T;


/****************************************************************************
 * Type:        PAL_SYS_TIMER16_MODE_T
 ****************************************************************************
 * Description: This type defines different timer modes. 
 *              
 ***************************************************************************/
typedef enum PAL_SYS_TIMER16_MODE_tag
{
    TIMER16_CNTRL_ONESHOT  = 0,
    TIMER16_CNTRL_AUTOLOAD = 2
} PAL_SYS_TIMER16_MODE_T;



/****************************************************************************
 * Type:        PAL_SYS_TIMER16_CTRL_T
 ****************************************************************************
 * Description: This type defines start and stop values for the timer. 
 *              
 ***************************************************************************/
typedef enum PAL_SYS_TIMER16_CTRL_tag
{
    TIMER16_CTRL_STOP = 0,
    TIMER16_CTRL_START
} PAL_SYS_TIMER16_CTRL_T ;


void PAL_sysTimer16GetFreqRange(UINT32  refclk_freq,
                            UINT32 *p_max_usec,
                            UINT32 *p_min_usec);                                

INT32 PAL_sysTimer16SetParams(UINT32 base_address,
                         UINT32 refclk_freq,
                         PAL_SYS_TIMER16_MODE_T mode,
                         UINT32 usec,
                         INT32 *err_nsec
                         );

void PAL_sysTimer16Ctrl(UINT32 base_address, PAL_SYS_TIMER16_CTRL_T status);

UINT32 PAL_sysTimer16GetNSecPerCount(UINT32 base_address, UINT32 load_time_nsec);

/****************************************************************************
 * FUNCTION: PAL_sysTimer16GetNsAfterTick
 ****************************************************************************
 * Description: Calculate nSec that passed since the last timer reload
 * param [in] base_address - of specific timer
 * param [in] ns_per_count - nSec per each timer count
 * Return - time passed since last timer reload
 ***************************************************************************/
static UINT32 inline PAL_sysTimer16GetNsAfterTick(UINT32 base_address, UINT32 ns_per_count)
{
    volatile PAL_SYS_TIMER16_STRUCT_T *p_timer;
    UINT32 load;
    UINT32 count;

    if (base_address == 0) 
    {
        return 0;
    }

    p_timer = (PAL_SYS_TIMER16_STRUCT_T *)(base_address);
    /* Must add 1 to both load and count, but since they are subtracted, not necessary on both. */
    load = p_timer->load_reg;
    count = p_timer->count_reg;

    return (load - count) * ns_per_count;
}

/****************************************************************************
 * FUNCTION: PAL_sysTimer16GetLoad
 ****************************************************************************
 * Description: Get number of counts per reload
 * param [in] base_address - of specific timer
 * Return - Number of counts per reload
 ***************************************************************************/
static UINT32 inline PAL_sysTimer16GetLoad(UINT32 base_address)
{
    volatile PAL_SYS_TIMER16_STRUCT_T *p_timer;

    if (base_address == 0) 
    {
        return 0;
    }

    p_timer = (PAL_SYS_TIMER16_STRUCT_T *)(base_address);

    /* Add 1, timer counts 0 too */
    return p_timer->load_reg + 1;
}


#endif /* __TIMER16_H__ */
