/*
  GPL LICENSE SUMMARY

  Copyright(c) 2008-2013 Intel Corporation.

  This program is free software; you can redistribute it and/or modify 
  it under the terms of version 2 of the GNU General Public License as
  published by the Free Software Foundation.

  This program is distributed in the hope that it will be useful, but 
  WITHOUT ANY WARRANTY; without even the implied warranty of 
  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU 
  General Public License for more details.

  You should have received a copy of the GNU General Public License 
  along with this program; if not, write to the Free Software 
  Foundation, Inc., 51 Franklin St - Fifth Floor, Boston, MA 02110-1301 USA.
  The full GNU General Public License is included in this distribution 
  in the file called LICENSE.GPL.

  Contact Information:
    Intel Corporation
    2200 Mission College Blvd.
    Santa Clara, CA  97052
*/

/*
 * hal_interrupt_cause.h
 * Description:
 * declaration of functions and types used in the interrupt cause-detection module
*/

#ifndef _HAL_INTERRUPT_CAUSE_H_
#define _HAL_INTERRUPT_CAUSE_H_

#include "_tistdtypes.h"
#include "puma_autoconf.h"
#include "docint.h"



/**************************************************************************/
/*      INTERFACE  Defines and Structs                                    */
/**************************************************************************/

/* A structure that defines receiver parameters used by the SW. */
typedef struct RecieverFecParam {
    Bool value;
    Uint32 intr;
    Uint32 lockbit;
    Uint32 lostbit;
} RecieverFecParam_t;


/* A structure that defines the interrupt Cause and corresponding Mask register . */
/* For PHY Upstream (TX) interrupts.                                              */
typedef struct PhyTxMask {
    Uint32 mask;
    Uint32 cause;
} PhyTxMask_t;

/* Macro for retrieving the global index of an interrupt cause.         */
/* The macro receives the interrupt-group and the specific bit-number   */
/* in that group, and returns the "global" index.                       */
#define HAL_GET_INTERRUPT_CAUSE(group, bitNumber)   (((group)*32)+(bitNumber))

/* Macro for retrieving the interrupt-group of an interrupt.            */
/* The macro receives the global interrupt-cause and returns the group  */
/* index.                                                               */
#define HAL_GET_INTERRUPT_GROUP(cause)              ((cause)/32)

/* Macro for retrieving the bit number of an interrupt inside its group.*/
/* The macro receives the global interrupt-cause and returns the local  */
/* bit-index.                                                           */
#define HAL_GET_INTERRUPT_BIT(cause)                ((cause)%32)


typedef enum
{
    MAC_ERR_INT_GROUP_FIRST,
    MAC_ERR1_INT_GROUP  = MAC_ERR_INT_GROUP_FIRST,

#if PUMA6_OR_NEWER_SOC_TYPE
    MAC_ERR1A_INT_GROUP,
    MAC_ERR1B_INT_GROUP,
    MAC_ERR1C_INT_GROUP,
#endif

    MAC_ERR2_INT_GROUP,

#if PUMA6_OR_NEWER_SOC_TYPE
    MAC_ERR2A_INT_GROUP,
#endif

    MAC_ERR3_INT_GROUP,

#if PUMA6_OR_NEWER_SOC_TYPE
    MAC_ERR3A_INT_GROUP,
#endif

    MAC_ERR4_INT_GROUP,
    MAC_ERR_INT_GROUP_LAST = MAC_ERR4_INT_GROUP,

    MAC_UCD_INT_GROUP,

    MAC_US_FW_INT_GROUP,

    MAC_DS_FW_INT_GROUP,
#if PUMA6_OR_NEWER_SOC_TYPE
    MAC_DS_FW_INT_GROUP2,
#endif

    PHY_INT_GROUP_FIRST,
    PHY_INT_GROUP = PHY_INT_GROUP_FIRST,

#if PUMA6_OR_NEWER_SOC_TYPE
    PHY_INT_GROUP1,
    PHY_INT_GROUP2,
    PHY_INT_GROUP3,
    PHY_INT_GROUP4,
    PHY_INT_GROUP5,
    PHY_INT_GROUP_LAST = PHY_INT_GROUP5,  // The last of all the DS PHY groups (Not including the HSIF)
    PHY_INT_HSIF_GROUP,
#else
    PHY_INT_GROUP_LAST = PHY_INT_GROUP,
#endif

    PHY_PGA_GRT_INT_GROUP,

    PHY_MPT_INT_GROUP,

    MAX_INT_GROUP
} docsisIpInterruptSrcGroups_e;


#define MAX_INT_CAUSE               (32 * (MAX_INT_GROUP))
#define MAC_ERR_INT_GROUPS_NUM      ((MAC_ERR_INT_GROUP_LAST - MAC_ERR_INT_GROUP_FIRST)+ 1)

#if PUMA6_OR_NEWER_SOC_TYPE
#define PHY_INT_GROUPS_NUM          ((PHY_INT_HSIF_GROUP     - PHY_INT_GROUP_FIRST    )+ 1)
#define MAC_DS_FW_INT_GROUPS        2
#else
#define PHY_INT_GROUPS_NUM          1
#define MAC_DS_FW_INT_GROUPS        1
#endif


/**************************************************************************/
/*      INTERFACE VARIABLES (prefix with EXTERN)                          */
/**************************************************************************/


/**************************************************************************/
/*      INTERFACE FUNCTIONS Prototypes:                                   */
/**************************************************************************/

/**********************************/
/*  MAC Errors Interrupt Group    */
/**********************************/
/*! \fn Int32 HAL_IsrGetCauseMacErrorGroup(IntCauseErrorGroup_t *pCause)
 *  \brief retreives the specific causes that asserted an interrupt on the
 *  \MAC-error group interrupt-line.
 *  \param[out] pCause pointer to the structure that will hold the interrupt cause(s).
 *  \return OK if successful, reject or fail otherwise
 */
Int32 HAL_IsrGetCauseMacErrorGroup(unsigned int *pCause);


/****************************************/
/*    MAC UCD-Change Interrupt group    */
/****************************************/
/*! \fn Uint32 HAL_IsrUcdChange_GetCause()
 *  \brief retreives the specific causes that asserted an interrupt on the
 *  \MAC UCD Change interrupt-line.
 *  \return The interrupt cause(s) as a bit-map.
 */
Uint32  HAL_IsrUcdChange_GetCause( void );
int     HAL_IsrUcdChange_ResetStats( char * buf, int count );
int     HAL_IsrUcdChange_PrintStats( char * buf, int count );


/***********************************/
/*    MAC US FW Interrupt group    */
/***********************************/
/*! \fn Uint32 HAL_IsrGetCauseUsFw()
 *  \brief retreives the specific causes that asserted an interrupt on the
 *  \MAC US FW interrupt-line.
 *  \return The interrupt cause(s) as a bit-map.
 */
Uint32  HAL_IsrUsFw_GetCause( void );
int     HAL_IsrUsFw_ResetStats( char * buf, int count );
int     HAL_IsrUsFw_PrintStats( char * buf, int count );


/***********************************/
/*    MAC DS FW Interrupt group    */
/***********************************/
/*! \fn Uint32 HAL_IsrDsFw_GetCause()
 *  \brief retreives the specific causes that asserted an interrupt on the
 *  \MAC DS FW interrupt-line.
 *  \return The interrupt cause(s) as a bit-map.
 */
Uint32  HAL_IsrDsFw_GetCause( unsigned int *pCause );
int     HAL_IsrDsFw_Init(int isPuma6D0);
int     HAL_IsrDsFw_ResetStats( char * buf, int count );
int     HAL_IsrDsFw_PrintStats( char * buf, int count );


/***********************************/
/*         PHY  Interrupts         */
/***********************************/
/*! \fn Uint32 HAL_IsrPhyFec_GetCause( Uint32 *pCause )
 *  \brief retreives the specific causes that asserted an interrupt on the
 *  \PHY interrupt-line.
 *  \return The amount of bits in evaluated group.
 */
Uint32  HAL_IsrPhyFec_GetCause( Uint32 *pCause );
#if PUMA6_OR_NEWER_SOC_TYPE
int     HAL_IsrHsif_PrintStats( char * buf, int count );
#endif
int     HAL_IsrPhyFec_PrintStats( char * buf, int count );
int     HAL_IsrPhyFec_PrintConfig( char * buf, int count );
int     HAL_IsrPhyFec_ResetStats( char * buf, int count );

/***********************************/
/*      Mask/Unmask Service        */
/***********************************/
/*! \fn Int32 HAL_IsrMaskInterrupt(Uint32 cause, Bool maskOp)
 *  \brief Masks or unmasks a speific interrupt.
 *  \param[in] cause defines the specific interrupt that needs to be
 *  \                masked/unmasked.
 *  \param[in] maskOp defines whether to mask or unmask.
 *  \                 True = mask, False= unmask.
 *  \return OK if successful, reject or fail otherwise
 */
Int32 HAL_IsrMaskInterrupt(Uint32 cause, Bool maskOp);


/**************************************************************************/
/*! \fn int HAL_IsrResetPhyFecDB()
 **************************************************************************
 *  \brief Reset Phy-Rx database.
 *  \param[in] none.
 *  \return 0 if successful, reject or fail otherwise.
 **************************************************************************/
Int32   HAL_IsrResetPhyFecDB(void);

void    HAL_IsrStatsEnableSet(Uint32    enable);
void    HAL_IsrStatsEnableGet(Uint32 *  enable);

#endif
