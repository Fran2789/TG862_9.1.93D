/*
 *
 * us_phy_verify.c
 * Description:
 * PHS (Payload Header Suppression) implementation
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

/*! \file us_phy_verify.c
*  \brief PHS (Payload Header Suppression) implementation
*         Note: PHS is implemented in hardware, this file deals with
*         upstream PHS verification only (implemented by software...)
*
*         Note regarding PHS rules indexing:
*
*         According to MULPI definitions each PHSI-indexed PHS Rule has a 
*         rule index (PHSI) carried in the extended MAC header of packets 
*         whose payload is suppressed by this PHS rule. PSSI range is
*         1 to 254.
*
*         In this CM implementation PHS rules are stored at various levels
*         as follows:
*
*         - Hardware: Rules are stored in a table in h/w memory.
*           Each entry contains (among other parameters) the PHSI.
*         - Kernel-level: A similar table is kept in kernel-space memory.
*           This table is related to US PHS Verification, and is defined in 
*           this file (UsPhsRules[]). It contains US PHS rule parameters that
*           are needed in order to perform US PHS verification. This does NOT
*           include PHSI.
*         - QoS internal data base: Keeps active PHS rules (US and DS).
*         - QoS main data base: Keeps ALL PHS rules.
*
*         The first 3 tables (h/w, kernel, QoS-internal) are indexed
*         0 to 31. A given US PHS rule must reside at the same index in
*         these 3 tables. This index is referred to as the rule's 
*         INTERNAL index.
*         The indexing in the last table (QoS-main) is different, and a 
*         rule may reside there at any index.
*   
*         PHS rules are associated with classifiers. When a classifier
*         matches a packet - the associated PHS rule (if the classifier
*         has one) is applied to the packet. The classifier data structure
*         (QosClassifier_t) holds the associated PHS rule's
*         INTERNAL index, thus pointing at the associated rule in all
*         relevant levels (h/w, kernel, QoS-internal).
*/
/**************************************************************************/
/*      INCLUDES:                                                         */
/**************************************************************************/
#include <linux/init.h>
#include <linux/types.h>
#include <linux/module.h>
#include <linux/kernel.h>
#include <linux/skbuff.h>
#include "pal.h"
#include "dfltr_class_ctrl.h"
#include "us_phs.h"

/* note: prints function name for you */
#define PRINTK(fmt, args...) printk("%s: " fmt, __FUNCTION__ , ## args)

/* #define DPHS_DEBUG */              /* Uncomment to enable debug prints */
#ifdef DPHS_DEBUG
#define DPRINTK(fmt, args...) printk("%s: " fmt, __FUNCTION__ , ## args)
#else
#define DPRINTK(fmt, args...)
#endif

/**************************************************************************/
/*      EXTERNS Declaration:                                              */
/**************************************************************************/

/**************************************************************************/
/*      DEFINES:                                                          */
/**************************************************************************/

/**************************************************************************/
/*      LOCAL DECLARATIONS:                                               */
/**************************************************************************/

/**************************************************************************/
/*      LOCAL VARIABLES:                                                  */
/**************************************************************************/

/* Upstream PHS Rules Table: */
/* This table is used for US PHS verification. */
/* Indexed by the same indices used at h/w level (0 .. 31) */
static UsPhsRule_t UsPhsRules[PHS_MAX_RULES];

/**************************************************************************/
/*      LOCAL PROTOTYPES:                                                 */
/**************************************************************************/
static int UsPhsRule_Add(Uint32 phsIndex, UsPhsRule_t *phsRule);
static int UsPhsRule_Delete(Uint32 phsIndex);

/**************************************************************************/
/*      INTERFACE FUNCTIONS                                               */
/**************************************************************************/


/**************************************************************************/
/*! \fn     void UsPhsRules_Init(void)
 **************************************************************************
 *  \brief  Initialize Upstream PHS Rules table
 *  \return None.
 */
void UsPhsRules_Init(void)
{
    DPRINTK("Clear PHS Rules Table (kernel-level) [%d bytes]\n", sizeof(UsPhsRules));
    PAL_osMemSet(UsPhsRules, 0, sizeof(UsPhsRules));
}


/**************************************************************************/
/*! \fn         int UsPhsRules_Set(int subtype, Uint32 param1, Uint32 param2, void *buf)
 **************************************************************************
 *  \brief      Set operations on upstream PHS Rules Table
 *  \param[in]  subtype - set command subtype:
 *                          - PHSIOC_S_SUBTYPE_ADD
 *                          - PHSIOC_S_SUBTYPE_DEL
 *
 *                      PHSIOC_S_SUBTYPE_ADD    PHSIOC_S_SUBTYPE_DEL
 *                      --------------------    --------------------
 *  \param[in]  param1  Rule internal index     Rule internal index
 *  \param[in]  param2  Not used                Not used           
 *  \param[in]  buf     PHS rule to add         Not used    
 *  \param[in]  bufLen  Size of buf             Not used
 *                      containingthe rule
 *
 *  \param[out] None.
 *  \return     0 or error code
 */
int UsPhsRules_Set(int subtype, Uint32 param1, Uint32 param2, void *buf, int bufLen)
{
	int ret = 0;

    DPRINTK("Modify PHS Rules Table (kernel-level) [subtype %d] [param1 %d] [param2 %d] [bufLen %d]\n", 
           subtype, param1, param2, bufLen);
	switch (subtype) 
	{
    case US_PHSIOC_S_SUBTYPE_ADD:
        if (bufLen != US_PHS_RULE_SIZE) 
        {
            PRINTK("US_PHSIOC_S_SUBTYPE_ADD Warning - Bad length [%d]\n", bufLen);
            ret = -EINVAL;
        }
        else
        {
            ret = UsPhsRule_Add(param1, (UsPhsRule_t *) buf);
        }
		break;
    case US_PHSIOC_S_SUBTYPE_DEL:
        ret = UsPhsRule_Delete(param1);
        break;
	default:
		ret = -EINVAL;
	}

	return ret;
}


/**************************************************************************/
/*! \fn         int UsPhsVerify(struct sk_buff *skb, Uint32 phsIndex) 
 **************************************************************************
 *  \brief      Perform US PHS Verification for PHS rules with PHSV=0
 *
 *              Note:
 *              PHS verification is not supported in case of fragmented
 *              packet where the first fragment does not fully contain
 *              the PHS field (PHSF). In such cases PHS verification is
 *              not performed and Payload Header Suppression is not
 *              applied to the packet.
 *
 *  \param[in]  skb - holds the US packet that is a candidate for PHS.
 *  \param[in]  phsIndex - Internal index: Identifies the PHS rule to apply
 *  \param[out] None
 *  \return     0 if No verification needed or verification done successfully.
 *              -1 if verification failed.
 *              Note:
 *              Payload Header Suppression will be applied to the packet
 *              if and only if we return 0.
 */
int UsPhsVerify(struct sk_buff *skb, Uint32 phsIndex) 
{
    register Uint32          i;
    register Uint8           phsSize;
    register Uint8          *phsMask;
    register Uint8          *phsField;
    register Uint8          *pktData;
    UsPhsRule_t             *phsRule;
    Uint32                   pktDataLen;

    /* PHS internal index validity check */
    if (phsIndex >= PHS_MAX_RULES) 
    {
        DPRINTK("Invalid PHS internal index [%d], PHS will not be applied\n", phsIndex);
        return -1; /* Don't apply PHS to the packet */
    }

    /* Do we have such a rule? */
    if (!UsPhsRules[phsIndex].isValid)
    {
        DPRINTK("PHS rule [internal index %d] not found, PHS will not be applied\n", phsIndex);
        return -1; /* Don't apply PHS to the packet */
    }

    /* Note MULPI's PHSV encoding: 0=Verify, 1=Don't verify */
    if (UsPhsRules[phsIndex].phsVerify == 1)
    {
        DPRINTK("PHS rule [internal index %d] [PHSV 1] PHS will be applied without verification\n", 
                phsIndex);
        return 0; /* Do apply PHS to the packet (verification not required) */
    }

    /* PHS Rule exists with PHSV != 1: Perform PHS verification */
    phsRule = &UsPhsRules[phsIndex];
    DPRINTK("Verifying PHS rule [internal index %d] [PHHS %d] [PHSV %d] [PHSM 0x%x...]\n", 
            phsIndex, phsRule->phsSize, phsRule->phsVerify, 
            *((Uint32 *)(phsRule->phsMask)));

    /* Note: data_len > 0 indicates the packet spans over some skb fragments. */
    /* In this case data_len is fragment's length while len is total          */
    /* packet length.                                                         */
    if (skb->data_len)
        pktDataLen = skb->data_len;    /* Fragmented packet */
    else
        pktDataLen = skb->len;         /* Non-fragmented packet */


#ifdef CONFIG_TI_PACKET_PROCESSOR

    /* PP does not support PHS-Verify functionality.      */
    /* Therefore, mark the packet not to start a session. */
    skb->pp_packet_info.flags |= TI_HIL_PACKET_FLAG_PP_SESSION_BYPASS;
           
#endif /* CONFIG_TI_PACKET_PROCESSOR */

    /* Check if PHSF is fully contained in the skb. */
    if (pktDataLen < phsRule->phsSize)
    {
#ifdef DPHS_DEBUG
        if (skb->data_len)
            /* We don't support verification over framented packets. */
            DPRINTK("PHS will NOT be applied: Fragmented packet, first fragment is shorter than PHSF [frag len: %d]\n",
                    pktDataLen);
        else
            DPRINTK("PHS will NOT be applied: Packet is shorter than PHSF [pkt len: %d]\n", 
                    pktDataLen);
#endif
        return -1; /* Don't apply PHS to the packet */
    }

    /* Verify */
    phsSize = phsRule->phsSize;
    phsMask = phsRule->phsMask;
    phsField = phsRule->phsField;
    pktData = skb->data;
    for (i = 0; i < phsSize; i++) 
    {
        /* 
         * Indexing:
         * i indexes PHSF bytes
         * i indexes PHSM bits
         * i / 8 (i >> 3) indexes PHSM bytes 
         * i % 8 (i & 7)  indexes bits within the selected PHSM byte, 
         * where bits in a PHSM byte are numberred from 0 (MSBit)
         * to 7 (LSBit),
         */
        if ((phsMask[i / 8]) & (0x80 >> (i % 8)))
        {
            if (pktData[i] != phsField[i]) 
            {
                DPRINTK("Verification failed [offset 0x%x] [expected 0x%x] [found 0x%x] - PHS will NOT be applied!\n", 
                        i, phsField[i], pktData[i]);
                return -1; /* Don't apply PHS to the packet (verification failed) */
            }
        }
    }

    return 0; /* Do apply PHS to the packet (verification passed) */
}


/**************************************************************************/
/*      LOCAL FUNCTIONS		                                              */
/**************************************************************************/

/**************************************************************************/
/*! \fn         static int UsPhsRule_Add(Uint32 phsIndex, UsPhsRule_t *phsRule)
 **************************************************************************
 *  \brief      Add a new rule to the kernel-level PHS Rules Table
 *  \param[in]  phsIndex - Rule's internal index
 *  \param[in]  phsRule -  The rule to be added
 *  \param[out] None
 *  \return     0 or error code
 */
static int UsPhsRule_Add(Uint32 phsIndex, UsPhsRule_t *phsRule)
{
	Uint32 lockKey;

    DPRINTK("Adding PHS Rule (kernel-level) [internal index %d] [%d bytes] [PHHS %d] [PHSV %d]...\n",
            phsIndex, US_PHS_RULE_SIZE, phsRule->phsSize, phsRule->phsVerify);

    /* Verify index validity */
    if (phsIndex >= PHS_MAX_RULES) 
    {
        PRINTK("Warning - Invalid internal index [%d]\n", phsIndex);
        return -EINVAL;
    }

    /* Verify that rule size does not exceed the max size supported by h/w */
    if (phsRule->phsSize > PHS_FIELD_MAX_SIZE) 
    {
        PRINTK("Warning - PHS rule too big [%d]\n", phsRule->phsSize);
        return -EINVAL;
    }

    PAL_osProtectEntry(PAL_OSPROTECT_INTERRUPT, &lockKey);

    /* Check if rule already exists */
    if (UsPhsRules[phsIndex].isValid) 
    {
        PAL_osProtectExit(PAL_OSPROTECT_INTERRUPT, lockKey);
		PRINTK("Warning - Cannot add PHS rule [%d] - already exists\n", phsIndex);
		return -EINVAL;
	}

    /* Make sure the isValid flag is set only AFTER the  */
    /* entire entry is updated                           */
    phsRule->isValid = False;
    PAL_osMemCopy(&UsPhsRules[phsIndex], phsRule, US_PHS_RULE_SIZE);
    UsPhsRules[phsIndex].isValid = True;

    PAL_osProtectExit(PAL_OSPROTECT_INTERRUPT, lockKey);
    return 0;
}

/**************************************************************************/
/*! \fn         static int UsPhsRule_Delete(Uint32 phsIndex)
 **************************************************************************
 *  \brief      Delete a rule from the kernel-level PHS Rules Table
 *  \param[in]  phsIndex - Rule's internal index
 *  \param[out] None
 *  \return     0 or error code
 */
static int UsPhsRule_Delete(Uint32 phsIndex)
{
	Uint32  lockKey;

    DPRINTK("Deleting PHS Rule (kernel-level) [internal index %d]...\n", phsIndex);

    if (phsIndex >= PHS_MAX_RULES) 
    {
        DPRINTK("Warning - Invalid internal index [%d] - no rule to delete\n", phsIndex);
        return 0;
    }

    PAL_osProtectEntry(PAL_OSPROTECT_INTERRUPT, &lockKey);

#ifdef DPHS_DEBUG
    if (!UsPhsRules[phsIndex].isValid) 
		DPRINTK("Warning - No such rule [%d] - nothing to delete\n", phsIndex);
#endif

    /* Delete the rule by marking it as invalid */
    UsPhsRules[phsIndex].isValid = False;

    /* Clean-up rule's entry */
    PAL_osMemSet(&UsPhsRules[phsIndex], 0, US_PHS_RULE_SIZE);
    PAL_osProtectExit(PAL_OSPROTECT_INTERRUPT, lockKey);
    return 0;
}




