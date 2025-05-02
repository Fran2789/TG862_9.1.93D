/*
 *
 * dbridge_hal_filter.c
 * Description:
 * DOCSIS bridge address filtering implementation
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

#define _DBRIDGE_HAL_FILTER_C_

/*! \file dbridge_hal_filter.c
    \brief Implementation of the address filtering module interface.
*/

/**************************************************************************/
/*      INCLUDES:                                                         */
/**************************************************************************/
#include <puma_autoconf.h>
#include <linux/kernel.h>
#include <hardware.h>
#include <puma.h>
#include <_tistdtypes.h>

#include "dbridge_hal_filter.h"

/**************************************************************************/
/*      EXTERNS Declaration:                                              */
/**************************************************************************/

/**************************************************************************/
/*      LOCAL VARIABLES:                                                  */
/**************************************************************************/
static unsigned int HAL_MacDsUnicastDataFilterSerach(unsigned char *addr, unsigned int* entryIdx);

/**************************************************************************/
/*      DEFINES:                                                          */
/**************************************************************************/

#ifndef BIT_0
#define  BIT_0    0x00000001
#define  BIT_1    0x00000002
#define  BIT_2    0x00000004
#define  BIT_3    0x00000008
#define  BIT_4    0x00000010
#define  BIT_5    0x00000020
#define  BIT_6    0x00000040
#define  BIT_7    0x00000080
#define  BIT_8    0x00000100
#define  BIT_9    0x00000200
#define  BIT_10   0x00000400
#define  BIT_11   0x00000800
#define  BIT_12   0x00001000
#define  BIT_13   0x00002000
#define  BIT_14   0x00004000
#define  BIT_15   0x00008000
#define  BIT_16   0x00010000
#define  BIT_17   0x00020000
#define  BIT_18   0x00040000
#define  BIT_19   0x00080000
#define  BIT_20   0x00100000
#define  BIT_21   0x00200000
#define  BIT_22   0x00400000
#define  BIT_23   0x00800000
#define  BIT_24   0x01000000
#define  BIT_25   0x02000000
#define  BIT_26   0x04000000
#define  BIT_27   0x08000000
#define  BIT_28   0x10000000
#define  BIT_29   0x20000000
#define  BIT_30   0x40000000
#define  BIT_31   0x80000000
#define  BIT_32   0x00000001
#define  BIT_33   0x00000002
#define  BIT_34   0x00000004
#define  BIT_35   0x00000008
#define  BIT_36   0x00000010
#define  BIT_37   0x00000020
#define  BIT_38   0x00000040
#define  BIT_39   0x00000080
#define  BIT_40   0x00000100
#define  BIT_41   0x00000200
#define  BIT_42   0x00000400
#define  BIT_43   0x00000800
#define  BIT_44   0x00001000
#define  BIT_45   0x00002000
#define  BIT_46   0x00004000
#define  BIT_47   0x00008000
#define  BIT_48   0x00010000
#define  BIT_49   0x00020000
#define  BIT_50   0x00040000
#define  BIT_51   0x00080000
#define  BIT_52   0x00100000
#define  BIT_53   0x00200000
#define  BIT_54   0x00400000
#define  BIT_55   0x00800000
#define  BIT_56   0x01000000
#define  BIT_57   0x02000000
#define  BIT_58   0x04000000
#define  BIT_59   0x08000000
#define  BIT_60   0x10000000
#define  BIT_61   0x20000000
#define  BIT_62   0x40000000
#define  BIT_63   0x80000000
#endif

typedef struct{unsigned short data;} unaligned_Uint16;
typedef struct{unsigned long data;} unaligned_Uint32;
#define GET_UNALIGNED_UINT16(p) ((unaligned_Uint16 *)(p))->data
#define GET_UNALIGNED_UINT32(p) ((unaligned_Uint32 *)(p))->data

#if PUMA6_OR_NEWER_SOC_TYPE

    /********************************/
    /*  UNICAST CAM                 */
    /********************************/
    #define MAC_DS_CAM_UCAST_BASE_OFFSET         (0x01100000)
    #define MAC_DS_CAM_UCAST_ENTRIES_NUM         (65)

    /********************************/
    /*  MULTICAST HASH              */
    /********************************/
    #define MAC_DS_HASH_BASE_OFFSET                 (0x01102200)
    #define MAC_DSG2_HASH_BASE_OFFSET               (0x01102400)

    /************************/
    /*  MSC clock domain    */
    /************************/
    #define MAC_GLB_MSC_REGS_BASE_OFFSET            (0x01310000)    /* This is the base offset of MSC GCLK and SW Reset Registers */

    #define MAC_MSC_US_GLB_RESET_EN_SET_REG         (0x14 + MAC_GLB_MSC_REGS_BASE_OFFSET)
    #define MAC_MSC_US_GLB_RESET_EN_CLR_REG         (0x20 + MAC_GLB_MSC_REGS_BASE_OFFSET)

    #define MAC_GLB_MSC_RESET_DS_PDSP2_CORE(reg)         ( (reg) |= (BIT_15))          /* MSC_US_GLB_RESET_EN_SET register       */
    #define MAC_GLB_MSC_RESET_DS_PDSP2_SS(reg)           ( (reg) |= (BIT_14))          /* MSC_US_GLB_RESET_EN_SET register       */

#else

#define MAC_DS_CAM_UCAST_BASE_OFFSET         0x00058000
#define MAC_DS_CAM_UCAST_ENTRIES_NUM         64
#define MAC_DS_CAM_UCAST_ENTRIES_NUM_REV_2_0 65

#define PUMA5_HW_REV_1_X   0
#define PUMA5_HW_REV_2_0   1

    /********************************/
    /*  MULTICAST HASH              */
    /********************************/
    #define MAC_DS_HASH_BASE_OFFSET             0x0005A000

/************************/
/*  MSC clock domain    */
/************************/
#define MAC_GLB_MSC_REGS_BASE_OFFSET            0x00098000

#define MAC_GLB_MSC_RESET_SET_REG               (0x4  + MAC_GLB_MSC_REGS_BASE_OFFSET)
#define MAC_GLB_MSC_RESET_CLR_REG               (0x8  + MAC_GLB_MSC_REGS_BASE_OFFSET)

    #define MAC_GLB_MSC_RESET_DS_PDSP2(reg)     ( (reg) |= (BIT_5) )

#endif


#define MAC_DS_CAM_UCAST_ENTRY_SIZE          (0x8)

#define MAC_DS_CAM_UCAST_ADDR_L_REG(idx)    (((idx)*MAC_DS_CAM_UCAST_ENTRY_SIZE) + 0 + MAC_DS_CAM_UCAST_BASE_OFFSET)
#define MAC_DS_CAM_UCAST_ADDR_H_REG(idx)    (((idx)*MAC_DS_CAM_UCAST_ENTRY_SIZE) + 4 + MAC_DS_CAM_UCAST_BASE_OFFSET)



#define MAC_DS_HASH_ADDR_L_REG              (0x0  + MAC_DS_HASH_BASE_OFFSET)
#define MAC_DS_HASH_ADDR_H_REG              (0x4  + MAC_DS_HASH_BASE_OFFSET)

#define MAC_DS_HASH_SEARCH_ADDR_L_REG       (0x8  + MAC_DS_HASH_BASE_OFFSET)
#define MAC_DS_HASH_SEARCH_ADDR_H_REG       (0xC  + MAC_DS_HASH_BASE_OFFSET)

#define MAC_DS_HASH_RESULT_REG              (0x10 + MAC_DS_HASH_BASE_OFFSET)

/********************************/
/*  MAC_DS_HASH_ADDR_L_REG      */
#define MAC_DS_ADDR_ADD(reg)                ((reg) &= ~BIT_16)
#define MAC_DS_ADDR_DEL(reg)                ((reg) |=  BIT_16)

/********************************/
/*  MAC_DS_HASH_RESULT_REG      */
#define MAC_DS_HASH_FOUND(reg)              (((reg)&BIT_0) ? True : False)




/**************************************************************************/
/*      LOCAL DECLARATIONS:                                               */
/**************************************************************************/

/**************************************************************************/
/*      INTERFACE FUNCTIONS Implementation:                               */
/**************************************************************************/

/**************************************************************************/
/*! \fn int HAL_MacDsUnicastDataFilterAdd( unsigned char *addr )
 **************************************************************************
 *  \brief this function Add Unicast MAC address filter
 *  \brif existing cpe MAC that add from config file .
 *  \param[in] mac address.
 *  \return OK or error status.
 **************************************************************************/
int HAL_MacDsUnicastDataFilterAdd( unsigned char *addr, unsigned int *hwIndex ) // ARRIS MOD
{
    unsigned int    regContent = 0;
    // unsigned int entryIdx = 0;   // ARRIS DELETE
    unsigned char   zero_addr[6] = {0,0,0,0,0,0};

    // ARRIS ADD : Don't add if already in the table
    if (HAL_MacDsUnicastDataFilterSerach(addr, hwIndex) == 0)
    {
        return (0);
    }
    // END ARRIS

    if (HAL_MacDsUnicastDataFilterSerach(zero_addr, hwIndex) == -1)    // ARRIS MOD
    {
        return (-1);
    }

    regContent = (unsigned int)GET_UNALIGNED_UINT16(addr);
    (*(volatile unsigned int*)(AVALANCHE_DOCSIS_SS_BASE + MAC_DS_CAM_UCAST_ADDR_L_REG(*hwIndex)))= regContent;  // ARRIS MOD
    regContent = (unsigned int)GET_UNALIGNED_UINT32(addr + 2);
    (*(volatile unsigned int*)(AVALANCHE_DOCSIS_SS_BASE + MAC_DS_CAM_UCAST_ADDR_H_REG(*hwIndex)))= regContent;  // ARRIS MOD
    
    return (0);
}

/**************************************************************************/
/*! \fn int HAL_MacDsUnicastDataFilterDelete( unsigned char *addr )
 **************************************************************************
 *  \brief this function delete Unicast MAC address filter
 *  \brif existing cpe MAC that add from config file .
 *  \param[in] mac address.
 *  \return OK or error status.
 **************************************************************************/
int HAL_MacDsUnicastDataFilterDelete( unsigned char *addr )
{
    unsigned int    entryIdx = 0;
    
    if (HAL_MacDsUnicastDataFilterSerach(addr, &entryIdx) == -1)
    {
        return (-1);
    }
        
    (*(volatile unsigned int*)(AVALANCHE_DOCSIS_SS_BASE + MAC_DS_CAM_UCAST_ADDR_L_REG(entryIdx)))= 0;
    (*(volatile unsigned int*)(AVALANCHE_DOCSIS_SS_BASE + MAC_DS_CAM_UCAST_ADDR_H_REG(entryIdx)))= 0;

    return (0);
}

// ARRIS ADD
/**************************************************************************/
/*! \fn int HAL_MacDsUnicastDataFilterDeleteAtIndex( unsigned int entryIdx )
 **************************************************************************
 *  \brief this function deletes the Unicast MAC address filter at a given index
 *  \brif existing cpe MAC that add from config file .
 *  \param[in] index of the filter to delete
 *  \return OK or error status.
 **************************************************************************/
int HAL_MacDsUnicastDataFilterDeleteAtIndex( unsigned int entryIdx )
{
    unsigned int macDsCamUcastEntriesNum = 0;

#if PUMA6_OR_NEWER_SOC_TYPE
    macDsCamUcastEntriesNum = MAC_DS_CAM_UCAST_ENTRIES_NUM;
#else
    /* If revision is 1.0 or 1.1 */
    if (system_rev != PUMA5_HW_REV_2_0)
    {
        macDsCamUcastEntriesNum = MAC_DS_CAM_UCAST_ENTRIES_NUM;
    }

    /* If revision is 2.0 */
    else
    {
        macDsCamUcastEntriesNum = MAC_DS_CAM_UCAST_ENTRIES_NUM_REV_2_0;
    }
#endif
    if (entryIdx < macDsCamUcastEntriesNum)
    {
        (*(volatile unsigned int*)(AVALANCHE_DOCSIS_SS_BASE + MAC_DS_CAM_UCAST_ADDR_L_REG(entryIdx)))= 0;
        (*(volatile unsigned int*)(AVALANCHE_DOCSIS_SS_BASE + MAC_DS_CAM_UCAST_ADDR_H_REG(entryIdx)))= 0;
    
        return (0);
    }
    else
    {
        return (-1);
    }
}

/**************************************************************************/
/*! \fn int HAL_MacDsUnicastDataFilterGetAtIndex( unsigned int entryIdx, unsigned char *macaddress )
 **************************************************************************
 *  \brief this function retrieves Unicast MAC address filter at a given index
 *  \brif existing cpe MAC that add from config file .
 *  \param[in] index of the filter to get
 *  \param[out] hi - high order 
 *  \return OK or error status.
 **************************************************************************/
int HAL_MacDsUnicastDataFilterGetAtIndex( unsigned int entryIdx, unsigned char* macaddress )
{
    unsigned int regContent;
    unsigned int macDsCamUcastEntriesNum = 0;

#if PUMA6_OR_NEWER_SOC_TYPE
    macDsCamUcastEntriesNum = MAC_DS_CAM_UCAST_ENTRIES_NUM;
#else
    /* If revision is 1.0 or 1.1 */
    if (system_rev != PUMA5_HW_REV_2_0)
    {
        macDsCamUcastEntriesNum = MAC_DS_CAM_UCAST_ENTRIES_NUM;
    }

    /* If revision is 2.0 */
    else
    {
        macDsCamUcastEntriesNum = MAC_DS_CAM_UCAST_ENTRIES_NUM_REV_2_0;
    }
#endif

    if (entryIdx < macDsCamUcastEntriesNum)
    {
        regContent = (*(volatile unsigned int*)( AVALANCHE_DOCSIS_SS_BASE + MAC_DS_CAM_UCAST_ADDR_L_REG(entryIdx) ));
        macaddress[0] = (regContent & 0x0000FF00) >> 8;
        macaddress[1] = (regContent & 0x000000FF);
    
        regContent = (*(volatile unsigned int*)( AVALANCHE_DOCSIS_SS_BASE + MAC_DS_CAM_UCAST_ADDR_H_REG(entryIdx) ));
        macaddress[2] = (regContent & 0xFF000000) >> 24;
        macaddress[3] = (regContent & 0x00FF0000) >> 16;
        macaddress[4] = (regContent & 0x0000FF00) >> 8;
        macaddress[5] = (regContent & 0x000000FF);
    
        return (0);
    }
    else
    {
        return (-1);
    }
}
// END ARRIS

/**************************************************************************/
/*! \fn static unsigned int HAL_MacDsUnicastDataFilterSerach(unsigned char *addr, unsigned int* entryIdx)
 **************************************************************************
 *  \brief Search a MAC address in the HW Unicast filter table. 
 *  \param[in] addr - pointer to a buffer that holds the relevant MAC address.
 *  \param[out] entryIdx - the returned entry index in the HW Unicast filter table.
 *  \return 0 for OK if successful, 1 for ERROR otherwise  
 **************************************************************************/
static unsigned int HAL_MacDsUnicastDataFilterSerach(unsigned char *addr, unsigned int* entryIdx)
{
    unsigned int regContent = 0;
    unsigned int searchContent = 0;
    unsigned int index = 0;
    unsigned int macDsCamUcastEntriesNum = 0;

    printk("HAL_MacDsUnicastDataFilterAdd: system_rev = %d\n", system_rev);

#if PUMA6_OR_NEWER_SOC_TYPE
    macDsCamUcastEntriesNum = MAC_DS_CAM_UCAST_ENTRIES_NUM;
#else
    /* If revision is 1.0 or 1.1 */
    if (system_rev != PUMA5_HW_REV_2_0)
    {
        macDsCamUcastEntriesNum = MAC_DS_CAM_UCAST_ENTRIES_NUM;
    }

    /* If revision is 2.0 */
    else
    {
        macDsCamUcastEntriesNum = MAC_DS_CAM_UCAST_ENTRIES_NUM_REV_2_0;
    }
#endif

    /* search the MAC address in the HW Unicast filter table */
    for (index = 0; index < macDsCamUcastEntriesNum; index++)
    {
        regContent = (*(volatile unsigned int*)( AVALANCHE_DOCSIS_SS_BASE + MAC_DS_CAM_UCAST_ADDR_L_REG(index) ));
        searchContent = (unsigned int)GET_UNALIGNED_UINT16(addr);

        if (regContent != searchContent)
        {
            continue;
        }

        else
        {
            regContent = (*(volatile unsigned int*)( AVALANCHE_DOCSIS_SS_BASE + MAC_DS_CAM_UCAST_ADDR_H_REG(index) ));
            searchContent = (unsigned int)GET_UNALIGNED_UINT32(addr + 2);

            if (regContent == searchContent)
            {
                *entryIdx = index;
                return 0;
            }
        }
    }

    *entryIdx = macDsCamUcastEntriesNum;
    return -1;
}

/**************************************************************************/
/*! \fn int HAL_MacDsMulticastDataFilterAdd( unsigned char *addr )
 **************************************************************************
 *  \brief this function add Multicast MAC address filter
 *  \brif existing cpe MAC that add from config file .
 *  \param[in] mac address.
 *  \return OK or error status.
 **************************************************************************/
int HAL_MacDsMulticastDataFilterAdd( unsigned char  *addr )
{
    unsigned int    regContent;

    regContent = (unsigned int)GET_UNALIGNED_UINT16(addr);
    MAC_DS_ADDR_ADD( regContent );
    (*(volatile unsigned int*)(AVALANCHE_DOCSIS_SS_BASE + MAC_DS_HASH_ADDR_L_REG))= regContent;
#if PUMA6_OR_NEWER_SOC_TYPE
    (*(volatile unsigned int*)(AVALANCHE_DOCSIS_SS_BASE + MAC_DS_HASH_ADDR_L_REG + (MAC_DSG2_HASH_BASE_OFFSET - MAC_DS_HASH_BASE_OFFSET)))= regContent;
#endif

    regContent = (unsigned int)GET_UNALIGNED_UINT32(addr + 2);
    (*(volatile unsigned int*)(AVALANCHE_DOCSIS_SS_BASE + MAC_DS_HASH_ADDR_H_REG))= regContent;
#if PUMA6_OR_NEWER_SOC_TYPE
    (*(volatile unsigned int*)(AVALANCHE_DOCSIS_SS_BASE + MAC_DS_HASH_ADDR_H_REG + (MAC_DSG2_HASH_BASE_OFFSET - MAC_DS_HASH_BASE_OFFSET)))= regContent;
#endif

    return (0);
}


/**************************************************************************/
/*! \fn int HAL_MacDsMulticastDataFilterDelete( unsigned char *addr )
 **************************************************************************
 *  \brief this function delete Multicast MAC address filter
 *  \brif existing cpe MAC that add from config file .
 *  \param[in] mac address.
 *  \return OK or error status.
 **************************************************************************/
int HAL_MacDsMulticastDataFilterDelete( unsigned char  *addr )
{
    unsigned int    regContent;

    regContent = (unsigned int)GET_UNALIGNED_UINT16(addr);
    MAC_DS_ADDR_DEL( regContent );
    (*(volatile unsigned int*)(AVALANCHE_DOCSIS_SS_BASE + MAC_DS_HASH_ADDR_L_REG))= regContent;
#if PUMA6_OR_NEWER_SOC_TYPE
    (*(volatile unsigned int*)(AVALANCHE_DOCSIS_SS_BASE + MAC_DS_HASH_ADDR_L_REG + (MAC_DSG2_HASH_BASE_OFFSET - MAC_DS_HASH_BASE_OFFSET)))= regContent;
#endif

    regContent = (unsigned int)GET_UNALIGNED_UINT32(addr + 2);
    (*(volatile unsigned int*)(AVALANCHE_DOCSIS_SS_BASE + MAC_DS_HASH_ADDR_H_REG))= regContent;
#if PUMA6_OR_NEWER_SOC_TYPE
    (*(volatile unsigned int*)(AVALANCHE_DOCSIS_SS_BASE + MAC_DS_HASH_ADDR_H_REG + (MAC_DSG2_HASH_BASE_OFFSET - MAC_DS_HASH_BASE_OFFSET)))= regContent;
#endif

    return (0);
}


/**************************************************************************/
/*! \fn int HAL_MacDsMulticastDataFilterSearch( unsigned char *addr )
 **************************************************************************
 *  \brief this function search for Multicast MAC address filter
 *  \brif existing cpe MAC that add from config file .
 *  \param[in] mac address.
 *  \return 0 - find or -1.
 **************************************************************************/
int HAL_MacDsMulticastDataFilterSearch( unsigned char  *addr )
{
    unsigned int  regContent;

    regContent = (unsigned int)GET_UNALIGNED_UINT16(addr);
    (*(volatile unsigned int*)(AVALANCHE_DOCSIS_SS_BASE + MAC_DS_HASH_SEARCH_ADDR_L_REG))= regContent;

    regContent = (unsigned int)GET_UNALIGNED_UINT32(addr + 2);
    (*(volatile unsigned int*)(AVALANCHE_DOCSIS_SS_BASE + MAC_DS_HASH_SEARCH_ADDR_H_REG))= regContent;

    regContent = (*(volatile unsigned int*)(AVALANCHE_DOCSIS_SS_BASE + MAC_DS_HASH_RESULT_REG));

    if (MAC_DS_HASH_FOUND(regContent))
    {
        return (0);
    }
    else
    {
        return (-1);
    }
}

/**************************************************************************/
/*! \fn int HAL_MacDsMulticastDeleteAll(void)
 **************************************************************************
 *  \brief this function delete all Multicast MAC address filter
 *  \brif existing cpe MAC that add from config file .
 *  \return OK or error status.
 **************************************************************************/
int HAL_MacDsMulticastDeleteAll(void)
{
    volatile unsigned int regContent;

#if PUMA6_OR_NEWER_SOC_TYPE
    MAC_GLB_MSC_RESET_DS_PDSP2_CORE(regContent);
    MAC_GLB_MSC_RESET_DS_PDSP2_SS(regContent);
    (*(volatile unsigned int*)(AVALANCHE_DOCSIS_SS_BASE + MAC_MSC_US_GLB_RESET_EN_SET_REG))= regContent;
    (*(volatile unsigned int*)(AVALANCHE_DOCSIS_SS_BASE + MAC_MSC_US_GLB_RESET_EN_CLR_REG))= regContent;
#else
    regContent = 0;
    MAC_GLB_MSC_RESET_DS_PDSP2( regContent );
    (*(volatile unsigned int*)(AVALANCHE_DOCSIS_SS_BASE + MAC_GLB_MSC_RESET_SET_REG))= regContent;
    (*(volatile unsigned int*)(AVALANCHE_DOCSIS_SS_BASE + MAC_GLB_MSC_RESET_CLR_REG))= regContent;
#endif

    return 0;

}


