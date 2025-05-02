/*
 *
 * qos_classifier.c
 * Description:
 * QOS Classifiers implementation
 *
 * Copyright (C) 2008-2014 Texas Instruments Incorporated - http://www.ti.com/ 
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

/*! \file qos_classifier.c
*   \brief QOS Classifiers implementation
*/
/**************************************************************************/
/*      INCLUDES:                                                         */
/**************************************************************************/

#include <linux/init.h>
#include <linux/types.h>
#include <linux/module.h>
#include <linux/kernel.h>
#include <linux/skbuff.h>
#include <linux/if_ether.h>
#include <linux/if_vlan.h>
#include <linux/ip.h>
#include <linux/in.h>
#include <linux/ipv6.h>
#include <linux/udp.h>
#include <linux/list.h>
#include <net/llc_pdu.h>
#include <net/ipv6.h>
#include <net/icmp.h>
#include <linux/ti_hil.h>
#include "pal.h"
#include "qos_classifier.h"
#include "dfltr_class_utils.h"
#include "dfltr_class_ctrl.h"
#include "docsis_mng.h"
#include "us_phs_verify.h"
#include "l2vpn.h"
#include "dbridge_common.h"
#include "dbridge_netdev.h"

//#define DCLASS_DEBUG
#ifdef DCLASS_DEBUG
/* note: prsints function name for you */
#  define DPRINTK(fmt, args...) printk(KERN_DEBUG "%s: " fmt, __FUNCTION__ , ## args)
#  define DWPRINTK(fmt, args...) printk(KERN_WARNING "%s: " fmt, __FUNCTION__ , ## args)
#  define DEPRINTK(fmt, args...) printk(KERN_ERR "%s: " fmt, __FUNCTION__ , ## args)
#  define DIPRINTK(fmt, args...) printk(KERN_INFO "%s: " fmt, __FUNCTION__ , ## args)
#else
#  define DPRINTK(fmt, args...)
#  define DWPRINTK(fmt, args...)
#  define DEPRINTK(fmt, args...)
#  define DIPRINTK(fmt, args...)
#endif

#pragma pack(1)
struct CM_mng_t 
{
    unsigned char da[6];
    unsigned char sa[6];
    unsigned short len;
    unsigned char dsap;
    unsigned char ssap;
    unsigned char control;
    unsigned char version;
    unsigned char type;
    unsigned char reserved;
    unsigned char data[];
};
#pragma pack()

/**************************************************************************/
/*      EXTERNS Declaration:                                              */
/**************************************************************************/
extern int Dbridge_register_QosClassifierDataCB(int (*Dbridge_QosClassifierDataCB)
           (struct sk_buff *skb, int *sfIndex, int *phsIndex));
// ARRIS MODIFY - added okToIncClassMib parameter
extern int Dbridge_register_L2vpnDataCB(int (*Dbridge_L2vpnData)(struct sk_buff *skb,int *l2vpnRelated,int okToIncClassMib));
static void PrintClassData(QosClassifier_t *cl);

/**************************************************************************/
/*      DEFINES:                                                          */
/**************************************************************************/
#ifdef CONFIG_MACH_PUMA6
#include "avalanche_pp_api.h"
#define MAX_SESSIONS        AVALANCHE_PP_MAX_ACCELERATED_SESSIONS    /* Max sessions supported in PP */
#else
#include <linux/ti_ppm.h> 
#define MAX_SESSIONS        TI_PP_MAX_ACCLERABLE_SESSIONS    /* Max sessions supported in PP */
#endif

#define MAX_QOS_CLASSIFIERS 100     /* Max number of QoS classifiers*/
#define MAX_UDC_CLASSIFIERS 64      /* Max number of UDC classifiers*/    
#define MAX_L2VPN_SF        32
/*Primary SF flow index*/
#define QOS_SF_PRIMARY_SF_INDEX 0

/*802.1Q header parsing macros*/
#define VLAN_TCI_GET_PRIO(X)(((X) & 0xE000 ) >> 13)
#define VLAN_TCI_GET_ID(X)  ((X) & VLAN_VID_MASK)

/*IPv6 header parsing macros*/
#define IPV6VER(h)    ((h->vcf[0]&0xF0)>>2)
#define IPV6CLASS(h)  (((h->vcf[0]&0x0F)<<4) | ((h->vcf[1]&0xF0)>>4))
#define IPV6FLABEL(h) (((h->vcf[1]&0x0F)<<16) | (h->vcf[2]<<8) | (h->vcf[3]))

/*Upstream Drop Classifier points to NULL Service Flow*/
#define IS_UPSTREAM_DROP_CLASSIFIER(cl) ((cl)->sfID == 0)

#define BIT_SHIFT_INVERT_64(x) ( 0x8000000000000000 >> (x) )
/*! \var    typedef ClassParamMatch_e
    \brief  classifier parameter match status for packet
*/
typedef enum
{
    CL_PARAM_NOMATCH,
    CL_PARAM_MATCH,
    CL_PARAM_UNRELEVANT,
}ClassParamMatch_e;

/*! \var    typedef struct ClassListEntry_t
    \brief  entry in classifiers list type
*/
typedef struct _ipFilterListEntry
{
    struct list_head list;
    QosClassifier_t  cl;
}ClassListEntry_t;

/*! \var    typedef struct _macAddrBytes
    \brief  integer view of MAC address
*/
#pragma pack(1)
typedef struct _macAddrBytes
{
    Uint32 mac4MSB; /*4 most significant bytes of MAC address*/
    Uint16 mac2LSB; /*2 least significant bytes of MAC address*/
}MacAddrBytes_t;
#pragma pack()

/**************************************************************************/
/*      LOCAL DECLARATIONS:                                               */
/**************************************************************************/
/**************************************************************************/
/*      LOCAL VARIABLES:                                                  */
/**************************************************************************/
static LIST_HEAD(classListHead);      /*Classifiers list (contains both QoS and UDC)*/
static Uint32    numUdcClassifiers;
static Uint32    numQoSClassifiers;
/**************************************************************************/
/*      LOCAL PROTOTYPES:                                                 */
/**************************************************************************/
static int AddClassifier(QosClassifier_t *newClass);
static int DelClassifier(Uint16 classId, Uint32 sfId);
static int DelAllClassifiersOfSf(Uint32 sfId);
static int UpdateClassifier(QosClassifier_t *cl);
static int GetClassifier(Uint16 classID, Uint32 sfId, QosClassifier_t *buf);
static int GetClassMatches(Uint16 classID, Uint32 sfId, Uint64 *matches);
static int GetPpClassMatches(Uint16 classID, Uint32 sfId, Uint64 *matches);
static int PrintPpClassMatches(void);
static int CreateNewClassListEntry(ClassListEntry_t **entry, QosClassifier_t *cl);
static int GetClassListEntryByIndex(Uint16 clID, Uint32 sfId, ClassListEntry_t **entry);
static ClassParamMatch_e MatchByLlc(struct ethhdr *ethHeader, Uint8 classEthType, Uint16 encProto, QosClassifier_t *cl);
static ClassParamMatch_e CompareClassifiersByLlc(QosClassifier_t *cl1, QosClassifier_t *cl2);
static ClassParamMatch_e MatchByVlan(Uint16 ethEncapsProto, Uint16 vlanPrio, Uint16 vlanId, QosClassifier_t *cl);
static ClassParamMatch_e CompareClassifiersByVlan(QosClassifier_t *cl1, QosClassifier_t *cl2);
static ClassParamMatch_e MatchByIpv4(struct iphdr  *ipHdr, Bool udpTcpHdrPresent, Uint8 ipIpLayerProto, Uint8 *ipUpLayerHdr, QosClassifier_t *cl);
static ClassParamMatch_e CompareClassifiersByIpv4(QosClassifier_t *cl1, QosClassifier_t *cl2);
static ClassParamMatch_e MatchByIpv6(struct Ipv6hdr_s  *ipHdr, Bool udpTcpHdrPresent, Uint8 ipUpLayerProto, Uint8 *ipUpLayerHdr, QosClassifier_t *cl);
static ClassParamMatch_e CompareClassifiersByIpv6(QosClassifier_t *cl1, QosClassifier_t *cl2);
static ClassParamMatch_e MatchByUdpTcp(struct udphdr *udpTcpHdr, QosClassifier_t *cl);
static ClassParamMatch_e CompareClassifiersByUdpTcp(QosClassifier_t *cl1, QosClassifier_t *cl2);
static void ParseVlanTCI(struct vlan_hdr *vlanHdr, Uint16 *prio, Uint16 *id);
static void SetClassEntryData(ClassListEntry_t *entry, QosClassifier_t *cl);
// ARRIS MODIFY - added okToIncClassMib parameter
static int QosClass_RetrieveClassifyData(struct sk_buff *skb, int *sfIndex, int *phsIndex, int *l2vpnRelate, int okToIncClassMib);
static int QosClass_DeletePpSessionsOfCorrelatingClassifiers(QosClassifier_t *cl1);

/**************************************************************************/
/*      INTERFACE FUNCTIONS                                               */
/**************************************************************************/

/**************************************************************************/
/*! \fn int QosClass_ClassifyData(struct sk_buff *skb, Uint32 srcIfIndex, Int16 *sfIndex, Int16 *phsIndex)
 **************************************************************************
 *  \brief Classify MAC data packet
 *  \param[in] sk_buff *skb - pointer to packet skb
 *  \param[out] Int16 *sfIndex - SF index
 *  \param[out] Int16 *phsIndex - PHS index
 *  \return  0 or error code
 */
int QosClass_ClassifyData(struct sk_buff *skb, int *sfIndex, int *phsIndex)
{
    // ARRIS MODIFY - added okToIncClassMib parameter
    return QosClass_RetrieveClassifyData(skb, sfIndex, phsIndex, NULL, True);
}

// ARRIS MODIFY - added okToIncClassMib parameter
/**************************************************************************/
/*! \fn int QosClass_L2vpnData(struct sk_buff *skb, int *l2vpnRelate, int okToIncClassMib)
 **************************************************************************
 *  \brief Classify L2VPN MAC data packet
 *  \param[in] sk_buff *skb - pointer to packet skb
 *  \param[out] l2vpnRelate - is l2vpn packet indication
 *  \return  0 or error code
 */
int QosClass_L2vpnData(struct sk_buff *skb, int *l2vpnRelate, int okToIncClassMib)
{

    int sfIndex;
    int phsIndex;
    int ret = 0;

    // ARRIS MODIFY - added okToIncClassMib parameter
    ret = QosClass_RetrieveClassifyData(skb, &sfIndex, &phsIndex, l2vpnRelate, okToIncClassMib);

    if (*l2vpnRelate)
    {
        /* The packet is L2VPN related, set this bit in ti_meta_info */
        skb->ti_meta_info |=  CNID_L2VPN_RELATED;
    }

    /* Set the ti_meta_info bit that indicates that this packet was checked if it is L2VPN related or not */
    skb->ti_meta_info |=  US_L2VPN_CHECKED;

    return ret;
}


/**************************************************************************/
/*! \fn int QosClass_ClassifyMacMng(Uint8 *buf, Int16 *sfIndex, Int16 *phsIndex) 
 **************************************************************************
 *  \brief Classify MAC management packet
 *  \param[in] Uint8 *buf - pointer to packet buffer
 *  \param[out] Int16 *sfIndex - SF index
 *  \param[out] Int16 *phsIndex - PHS index
 *  \return  0 or error code
 */
int QosClass_ClassifyMacMng(Uint8 *buf, Int16 *sfIndex, Int16 *phsIndex) 
{
   struct list_head *pos;
   ClassListEntry_t *entry;
   QosClassifier_t   *cl;
   Bool clMatch;
   struct CM_mng_t *macHdr;
   Uint32 lockKey;
   Uint8 prot1 = 0, prot2 = 0xFF;


   DPRINTK("%s - Start - \n",__FUNCTION__);

   *sfIndex = QOS_SF_PRIMARY_SF_INDEX;
   *phsIndex = -1;
   clMatch = False;

   macHdr = (struct CM_mng_t *)buf;

   /* go over the classifier table and try to find a match */
   PAL_osProtectEntry(PAL_OSPROTECT_INTERRUPT, &lockKey);
   list_for_each(pos, &classListHead)
   {
       entry = list_entry(pos, ClassListEntry_t, list);
       cl = &(entry->cl);

       if (!cl->isLlcCriterion) /*If LLC matching criteria is used*/
           continue;
        
       DPRINTK("%s Checking LLC classifier: enetProtocolType=%d \n",__FUNCTION__, cl->llc.enetProtocolType);

       if(cl->llc.enetProtocolType != FC_LLC_PROTO_MAC) 
           continue;

       /* MAC Msg - test type range */
       prot1 = (Uint8)(cl->llc.enetProtocol>>8);
       prot2 = (Uint8)(cl->llc.enetProtocol & 0x00FF);

       DPRINTK("%s Checking LLC-MAC classifier: MAC type=%d Prot1=%d  Prot2=%d  \n", 
              __FUNCTION__, macHdr->type, prot1, prot2);


       if((macHdr->type < prot1) || (macHdr->type > prot2))
       {
          continue; /* type out of range */
       }
       /* Classification succeeded - we have a match ! */
       clMatch = True;
       DPRINTK("%s Packet matches to LLC-MAC criteria; ClsfID=%d  SFID=0x%x  sfIndex=%d  phsIndex=%d \n",
              __FUNCTION__, cl->classID, cl->sfID, cl->sfIndex, cl->phsIndex);
       break;
   }

   if (clMatch) 
   {
      if (!IS_UPSTREAM_DROP_CLASSIFIER(cl))
      {
          DPRINTK("%s: QoS Classifier %u of SF %u matched\n",__FUNCTION__,cl->classID, cl->sfID);
      }
      else
      {
          DPRINTK("%s: Upstream Drop Classifier %u matched\n",__FUNCTION__,cl->classID);
      }
   
      /*In case of UDC both SF and PHS Index would be -1 */
      *sfIndex = cl->sfIndex;
      *phsIndex = cl->phsIndex;

      cl->pktCounter++;
    //  PAL_osProtectExit(PAL_OSPROTECT_INTERRUPT, lockKey);    ARRIS MOVE
   }

   PAL_osProtectExit(PAL_OSPROTECT_INTERRUPT, lockKey);    // ARRIS ADD

   return 0;
}

/**************************************************************************/
/*! \fn void QosClass_Init(void)
 **************************************************************************
 *  \brief Init classifiers module
 *  \return  NA
 */
void QosClass_Init(void)
{
    numQoSClassifiers = 0;
    numUdcClassifiers = 0;
}

/**************************************************************************/
/*! \fn void QosClass_Cleanup(void)
 **************************************************************************
 *  \brief Cleanup QOS classifiers module
 *  \return  OK
 */
int QosClass_Cleanup(void)
{
    struct list_head *pos;
    ClassListEntry_t *curr=NULL;
    Uint32 lockKey;

    PAL_osProtectEntry(PAL_OSPROTECT_INTERRUPT, &lockKey);
    /* free classifiers list */
    pos = classListHead.next;
    while(pos != &classListHead)
    {
        curr = list_entry(pos, ClassListEntry_t, list);
        pos = pos->next;
        list_del(&curr->list);
        PAL_osMemFree(0, curr, sizeof(ClassListEntry_t));
    }
    numQoSClassifiers = 0;
    numUdcClassifiers = 0;
    PAL_osProtectExit(PAL_OSPROTECT_INTERRUPT, lockKey);

    return 0;
}



/**************************************************************************/
/*! \fn int QosClass_Start(void)
 **************************************************************************
 *  \brief Start QOS classifiers module operation
 *  \return  OK/NOK
 */
int QosClass_Start(void)
{
    // ARRIS ADD Start 
    // L2vpn call back function was not getting registered when there was no upstream classifier.
    if (numQoSClassifiers || numUdcClassifiers) 
    {
        /*Register Data classifer callback in the Docsis Bridge */
        if (Dbridge_register_QosClassifierDataCB(QosClass_ClassifyData))
        {
            return -1;
    }    
    }

        if (Dbridge_register_L2vpnDataCB(QosClass_L2vpnData))
            return -1;
    // ARRIS ADD End

    if(HAL_DocsisRegisterQosClassifierMacMgmtCB(QosClass_ClassifyMacMng))
        return -1;

    
    return 0;
}

/**************************************************************************/
/*! \fn int QosClass_Stop(void)
 **************************************************************************
 *  \brief Stop QOS classifiers module operation
 *  \return  OK/NOK
 */
int QosClass_Stop(void)
{
    /*Unregister Data classifer callback in the Docsis Bridge */
    if (Dbridge_register_QosClassifierDataCB(NULL))
        return -1;
    if (Dbridge_register_L2vpnDataCB(NULL))
        return -1;
    return 0;
}

/**************************************************************************/
/*! \fn int QosClass_Set(int subtype, Uint32 param1, Uint32 param2, void *buf)
 **************************************************************************
 *  \brief Set Qos Classifier data
 *  \param[in] int subtype - set command sub - type
 *  \param[in]  Uint32 param1 - generic int parameter
 *  \param[in]  Uint32 param2 - generic int parameter
 *  \param[in] void *buf - input data buffer
 *  \return  0 or error code
 */
int QosClass_Set(int subtype, Uint32 param1, Uint32 param2, void *buf)
{
    int ret = 0;

    switch (subtype) 
    {
    case CLIOC_S_SUBTYPE_ADD:
        ret = AddClassifier((QosClassifier_t *)buf);
        break;
    case CLIOC_S_SUBTYPE_DEL:
        ret = DelClassifier(param1, param2);
        break;
    case CLIOC_S_SUBTYPE_UPD:
        ret = UpdateClassifier((QosClassifier_t *)buf);
        break;
    case CLIOC_S_SUBTYPE_PRINT:
        ret = PrintPpClassMatches();
        break;
    default:
        return -EINVAL;
    }
    return ret;
}

/**************************************************************************/
/*! \fn int QosClass_Get(int subtype, Uint32 param1, Uint32 param2, void *buf)
 **************************************************************************
 *  \brief Get Qos Classifier data
 *  \param[in] int subtype - set command sub - type
 *  \param[in] Uint32 param1 - generic input parameter
 *  \param[in] Uint32 param2 - generic input parameter
 *  \param[out] void *buf - output data buffer
 *  \return  0 or error code
 */
int QosClass_Get(int subtype, Uint32 param1, Uint32 param2, void *buf)
{
    int ret = 0;

    switch (subtype) 
    {
    case CLIOC_G_SUBTYPE_CLASS:
        ret = GetClassifier(param1, param2, (QosClassifier_t *)buf);
        break;
    case CLIOC_G_SUBTYPE_MATCHES:
        ret = GetClassMatches(param1, param2, (Uint64 *)buf);
        break;
    case CLIOC_G_SUBTYPE_PP_MATCHES:
        ret = GetPpClassMatches(param1, param2, (Uint64 *)buf);
        break;
    default:
        return -EINVAL;
    }
    return ret;
}

/**************************************************************************/
/*      LOCAL FUNCTIONS                                                   */
/**************************************************************************/

/**************************************************************************/
/*! \fn static int AddClassifier(QosClassifier_t *newClass)
 **************************************************************************
 *  \brief Add new Classifer to the classifers list
 *  \param[in] QosClassifier_t *newClass - classifier data
 *  \return  OK/NOK
 */
static int AddClassifier(QosClassifier_t *newClass)
{
    Uint32 lockKey;
    struct list_head *pos;
    ClassListEntry_t *curr=NULL, *new;

    DPRINTK("%s: class ID = %u, SF = %u\n",__FUNCTION__, newClass->classID, newClass->sfID);

    if (IS_UPSTREAM_DROP_CLASSIFIER(newClass)) 
    {
        if (numUdcClassifiers == MAX_UDC_CLASSIFIERS) {
            DWPRINTK("Failed to add UDC classifier - the classifer table is full\n");
            return -EINVAL;
        }
    }
    else
    {
        if (numQoSClassifiers == MAX_QOS_CLASSIFIERS) {
            DWPRINTK("Failed to add QOS classifier - the classifer table is full\n");
            return -EINVAL;
        }
    }

    PAL_osProtectEntry(PAL_OSPROTECT_INTERRUPT, &lockKey);    // ARRIS ADD

    /* find a place in the list according to the index */
    list_for_each(pos, &classListHead)
    {
        curr = list_entry(pos, ClassListEntry_t, list);
        if (curr->cl.priority <= newClass->priority) 
            break;
    }

    /*Create new list entry*/
    if (CreateNewClassListEntry(&new, newClass)){
        DEPRINTK("Failed to add Classifer - failed to create entry in the list\n");
        PAL_osProtectExit(PAL_OSPROTECT_INTERRUPT, lockKey);    // ARRIS ADD
        return -EINVAL;
    }

    /* insert before the current entry */
    // PAL_osProtectEntry(PAL_OSPROTECT_INTERRUPT, &lockKey);    ARRIS MOVE
    list_add(&new->list, pos->prev);
    if (IS_UPSTREAM_DROP_CLASSIFIER(newClass)) 
        numUdcClassifiers++;
    else
        numQoSClassifiers++;
    /*If it is the first classifier that is added register the class callback in the bridge*/
    if (numUdcClassifiers + numQoSClassifiers == 1) 
    {
        if (Dbridge_register_QosClassifierDataCB(QosClass_ClassifyData))
        {
            PAL_osProtectExit(PAL_OSPROTECT_INTERRUPT, lockKey);
            return -EFAULT;
        }
    }

    PAL_osProtectExit(PAL_OSPROTECT_INTERRUPT, lockKey);
	
    /* Delete PP sessions of classifiers correlating to this classifier */
    QosClass_DeletePpSessionsOfCorrelatingClassifiers(newClass);
    /* Delete all voice sessions, so that these sessions will be classified once again */
    ti_hil_pp_event(TI_DOCSIS_VOICE_SESSIONS_DEL, NULL);

    DPRINTK("Successfully added classifier classId=0x%x of SFID=0x%x\n", newClass->classID, newClass->sfID);
    return 0;
}

/**************************************************************************/
/*! \fn static int DelClassifier(Uint16 classId, Uint32 sfId)
 **************************************************************************
 *  \brief Delete a classifier with the given index
 *  \param[in] Uint16 classId - classifier ID
 *  \param[in] Int16 sfId - SF ID
 *  \return  0 or error code
 */
static int DelClassifier(Uint16 classId, Uint32 sfId)
{
    Uint32 lockKey;
    ClassListEntry_t *entry;

    DPRINTK("%s: class ID = %u  SFID = %u\n",__FUNCTION__, classId, sfId);

    if (!numQoSClassifiers && !numUdcClassifiers) 
    {
        /* The classiifier list is empty; Do nothing. Return OK */
        return 0;
    }

    /* If Clsf ID is 0 - delete all classifiers of the specified Service Flow */
    if(classId == 0)
    {
        DelAllClassifiersOfSf(sfId);
        return 0;
    }

    PAL_osProtectEntry(PAL_OSPROTECT_INTERRUPT, &lockKey);    // ARRIS ADD

    /* Find the specified classifier and delete it from the list */
    /* If it is not found - do nothing, it is already deleted    */
    if (GetClassListEntryByIndex(classId, sfId, &entry) != 0)
    {
        DWPRINTK("Classifer to delete is not found in the list,  classId=0x%x SFID=0x%x\n", classId, sfId);
        PAL_osProtectExit(PAL_OSPROTECT_INTERRUPT, lockKey);    // ARRIS ADD
        return 0;
    }
    
    /* delete current entry */
  //  PAL_osProtectEntry(PAL_OSPROTECT_INTERRUPT, &lockKey);    ARRIS MOVE
	
	/* Delete PP sessions related to this classifier before deleting the classifier */
    QosClass_DeletePpSessionsOfCorrelatingClassifiers(&entry->cl);
	
    list_del(&entry->list);
    if (IS_UPSTREAM_DROP_CLASSIFIER(&entry->cl))
        numUdcClassifiers --;
    else
        numQoSClassifiers --;
    /*If it was the last classifier unregister the class callback in the bridge*/
    if (!numQoSClassifiers && !numUdcClassifiers) 
    {
        if (Dbridge_register_QosClassifierDataCB(NULL))
        {    
            PAL_osProtectExit(PAL_OSPROTECT_INTERRUPT, lockKey);    // ARRIS ADD
            return -EFAULT;
        }
    }
    PAL_osMemFree(0, entry, sizeof(ClassListEntry_t));    // ARRIS ADD

    PAL_osProtectExit(PAL_OSPROTECT_INTERRUPT, lockKey);

   // PAL_osMemFree(0, entry, sizeof(ClassListEntry_t));    ARRIS MOVE

    DPRINTK("Successfully deleted classifier classId=0x%x of SFID=0x%x\n", classId, sfId);
    
    return 0;
}

/**************************************************************************/
/*! \fn static int DelAllClassOfSf(Uint32 sfId)
 **************************************************************************
 *  \brief Delete all classifiers of the given SFID
 *  \param[in] Int16 sfId - SF ID
 *  \return  0 or error code
 */
static int DelAllClassifiersOfSf(Uint32 sfId)
{
    Uint32 lockKey;
    struct list_head *pos = NULL, *next=NULL;
    ClassListEntry_t *curr=NULL;

    DPRINTK("%s: SFID = %u\n",__FUNCTION__, sfId);

    PAL_osProtectEntry(PAL_OSPROTECT_INTERRUPT, &lockKey);    // ARRIS ADD

    /* go over the list and search for classifiers of the specified SF */
    pos = classListHead.next; /* the first element of the list */
    while(pos != &classListHead)
    {
        next = pos->next;  /* keep ptr to the next element in the list */

        curr = list_entry(pos, ClassListEntry_t, list);
        if(curr->cl.sfID == sfId)
        {
            DPRINTK("%s: Found Classifier for deleting; ClsfID = %u SFID = %u\n",
                   __FUNCTION__, curr->cl.classID  ,sfId);

            /* delete the found entry */
          //  PAL_osProtectEntry(PAL_OSPROTECT_INTERRUPT, &lockKey);    ARRIS MOVE
			
            /* Delete PP sessions related to this classifier before deleting the classifier */
            QosClass_DeletePpSessionsOfCorrelatingClassifiers(&curr->cl);

               list_del(&curr->list);
               numQoSClassifiers --; 
          //  PAL_osProtectExit(PAL_OSPROTECT_INTERRUPT, lockKey);    ARRIS MOVE

            PAL_osMemFree(0, curr, sizeof(ClassListEntry_t));
        }

        pos = next;
    } /* End of while */

    PAL_osProtectExit(PAL_OSPROTECT_INTERRUPT, lockKey);    // ARRIS ADD

    /* Always return OK */
    return 0;
}



/**************************************************************************/
/*! \fn static int UpdateClassifier(QosClassifier_t *cl)
 **************************************************************************
 *  \brief Update classifier with the given index
 *  \param[in] QosClassifier_t *cl - classifier data
 *  \return  0 or error code
 */
static int UpdateClassifier(QosClassifier_t *cl)
{
    Uint32 lockKey;
    ClassListEntry_t *entry;

    DPRINTK("%s: class ID = %u, SF = %u\n",__FUNCTION__, cl->classID, cl->sfID);

    PAL_osProtectEntry(PAL_OSPROTECT_INTERRUPT, &lockKey);    // ARRIS ADD

    if ((numQoSClassifiers || numUdcClassifiers) && 
        (GetClassListEntryByIndex(cl->classID, cl->sfID, &entry) == 0))
    {
        
       /* update current entry */
     //  PAL_osProtectEntry(PAL_OSPROTECT_INTERRUPT, &lockKey);    ARRIS MOVE
        
        /* Delete PP sessions related to this classifier before updating the classifier */
        QosClass_DeletePpSessionsOfCorrelatingClassifiers(cl);
        
       SetClassEntryData(entry, cl);
    //   PAL_osProtectExit(PAL_OSPROTECT_INTERRUPT, lockKey);    ARRIS MOVE
    }
    else
    {
        /* Classifier is not found in the list - add it */
        AddClassifier(cl);
    }

    PAL_osProtectExit(PAL_OSPROTECT_INTERRUPT, lockKey);    // ARRIS ADD

    DPRINTK("Successfully updated classifier classId=0x%x of SFID=0x%x\n", cl->classID, cl->sfID);
    
    return 0;
}

/**************************************************************************/
/*! \fn static int GetClassifier(Uint16 classID, Uint32 sfId, QosClassifier_t *buf)
 **************************************************************************
 *  \brief Get classifier by index
 *  \param[in] Uint16 classID - classifier ID
 *  \param[in]  Int16 sfId - SF ID
 *  \param[out] QosClassifier_t *buf - output data buffer
 *  \return  0 or error code
 */
static int GetClassifier(Uint16 classID, Uint32 sfId, QosClassifier_t *buf)
{
    ClassListEntry_t *entry;
    Uint32 lockKey;    // ARRIS ADD

    PAL_osProtectEntry(PAL_OSPROTECT_INTERRUPT, &lockKey);    // ARRIS ADD

    if (GetClassListEntryByIndex(classID, sfId, &entry) != 0) 
    {
        DIPRINTK("%s: Classifer %d, SFID 0x%x is not found\n", __FUNCTION__,classID, sfId);
        PAL_osProtectExit(PAL_OSPROTECT_INTERRUPT, lockKey);    // ARRIS ADD
        return -EINVAL;
    }
    PAL_osMemCopy(buf, &(entry->cl), sizeof(QosClassifier_t));

    PAL_osProtectExit(PAL_OSPROTECT_INTERRUPT, lockKey);    // ARRIS ADD

    return 0;
}

/**************************************************************************/
/*! \fn static int GetClassMatches(Uint16 classID, Int16 sfIndex, Uint64 *matches)
 **************************************************************************
 *  \brief  Get number of matches for a classifier with a given ID
 *  \param[in]  Uint16 classID- classifier id
 *  \param[in]  Int32 sfId - SF ID
 *  \param[out] Uint8 *matches - output buffer
 *  \return  0 or error code
 */
static int GetClassMatches(Uint16 classID, Uint32 sfId, Uint64 *matches)
{
    Uint64 dummyPktCounter = 0;
    ClassListEntry_t *entry;
    Uint32 lockKey;    // ARRIS ADD

    PAL_osProtectEntry(PAL_OSPROTECT_INTERRUPT, &lockKey);    // ARRIS ADD

    if (GetClassListEntryByIndex(classID, sfId, &entry) != 0) 
    {
        DIPRINTK("%s: classifier %d, SFID 0x%x is not found\n", __FUNCTION__, classID, sfId);
        PAL_osMemCopy(matches, &dummyPktCounter, sizeof(Uint64));
        PAL_osProtectExit(PAL_OSPROTECT_INTERRUPT, lockKey);    // ARRIS ADD
        return 0;
    }
    PAL_osMemCopy(matches, &entry->cl.pktCounter, sizeof(Uint64));

    PAL_osProtectExit(PAL_OSPROTECT_INTERRUPT, lockKey);    // ARRIS ADD
    return 0;
}

/**************************************************************************/
/*! \fn static int GetPpClassMatches(Uint16 classID, Int16 sfIndex, Uint64 *matches)
 **************************************************************************
 *  \brief  Get number of packet processor matches for a classifier with a given ID
 *  \param[in]  Uint16 classID- classifier id
 *  \param[in]  Int32 sfId - SF ID
 *  \param[out] Uint8 *matches - output buffer
 *  \return  0 or error code
 */
static int GetPpClassMatches(Uint16 classID, Uint32 sfId, Uint64 *matches)
{
    Uint32 lockKey;
    Uint64 dummyPpPktCounter = 0;
    ClassListEntry_t *entry;
    Uint64 deadSessionsMatches = 0;
    Uint64 currSessionsMatches = 0;
    DppCountSessionEntry_t *currSessionEntry = NULL;

    PAL_osProtectEntry(PAL_OSPROTECT_INTERRUPT, &lockKey);

    /* get matched from dead sessions */
    if (GetClassListEntryByIndex(classID, sfId, &entry) != 0) 
    {
        DIPRINTK("%s: classifier %d, SFID 0x%x is not found\n", __FUNCTION__, classID, sfId);
        PAL_osMemCopy(matches, &dummyPpPktCounter, sizeof(Uint64));
        PAL_osProtectExit(PAL_OSPROTECT_INTERRUPT, lockKey);
        return 0;
    }
    PAL_osMemCopy(&deadSessionsMatches, &entry->cl.ppCounters.ppPktCounter, sizeof(Uint64));

    /* get matched of current sessions */
    if (entry->cl.ppCounters.activeSessionsList != NULL)
    {
        currSessionEntry = entry->cl.ppCounters.activeSessionsList;

        while (currSessionEntry != NULL)
        {
#ifdef CONFIG_MACH_PUMA5
            TI_PP_SESSION_STATS             sessionStats;
            if (ti_ppm_get_session_stats(currSessionEntry->sessionHandle, &sessionStats) < 0)
#else
            AVALANCHE_PP_SESSION_STATS_t    sessionStats;
            if (PP_RC_SUCCESS != avalanche_pp_get_stats_session(currSessionEntry->sessionHandle, &sessionStats))
#endif
            {
                DIPRINTK("Failed to get session %d statistics for ClassID %d and SfID 0x%x\n", 
                        currSessionEntry->sessionHandle, classID, sfId);
                PAL_osProtectExit(PAL_OSPROTECT_INTERRUPT, lockKey);
                return -EINVAL;
            }

            currSessionsMatches += sessionStats.packets_forwarded;
            currSessionEntry = currSessionEntry->qosClassInfo.nextSession;
        }
    }

    *matches =  deadSessionsMatches + currSessionsMatches;

    PAL_osProtectExit(PAL_OSPROTECT_INTERRUPT, lockKey);
	return 0;
}

/**************************************************************************/
/*! \fn static int GetPpClassMatches(Uint16 classID, Int16 sfIndex, Uint64 *matches)
 **************************************************************************
 *  \brief Print classifiers packet processor matches
 *  \param[in]   no inputs
 *  \param[out]  no outputs
 *  \return  0 or error code
 */
static int PrintPpClassMatches(void)
{
    Uint64 pktCnt = 0;
    Uint64 ppPktCnt = 0;
    struct list_head *pos = NULL;
    ClassListEntry_t *curr = NULL;
    Uint32 lockKey;    // ARRIS ADD

	printk("\n---------------------------------------");
	printk("\n Qos-Classifier match packets counters ");
	printk("\n---------------------------------------");
	printk("\nClassID\t SfID  \tHostPktCount\tPpPktCount");
	printk("\n-------\t-------\t------------\t----------");

    PAL_osProtectEntry(PAL_OSPROTECT_INTERRUPT, &lockKey);    // ARRIS ADD

    list_for_each(pos, &classListHead)
    {
        curr = list_entry(pos, ClassListEntry_t, list);

        GetClassMatches(curr->cl.classID, curr->cl.sfID, &pktCnt);
        GetPpClassMatches(curr->cl.classID, curr->cl.sfID, &ppPktCnt);
        printk("\n0x%x\t0x%x\t%Lu\t\t%Lu", curr->cl.classID, curr->cl.sfID, 
               pktCnt, ppPktCnt);
    }
    
    PAL_osProtectExit(PAL_OSPROTECT_INTERRUPT, lockKey);    // ARRIS ADD
        
	printk("\n");
	printk("\n");
	return 0;
}

/**************************************************************************/
/*! \fn static int CreateNewClassListEntry(ClassListEntry_t **entry, QosClassifier_t *cl)
 **************************************************************************
 *  \brief Create new entry in the classifiers list
 *  \param[out] ClassListEntry_t **entry - pointer to the allocated entry
 *  \param[in]  QosClassifier_t *cl - classifier data
 *  \return  OK/NOK
 */
static int CreateNewClassListEntry(ClassListEntry_t **entry, QosClassifier_t *cl)
{
    PAL_Result res;

    res = PAL_osMemAlloc(0, sizeof(ClassListEntry_t), 0, (Ptr *)entry);
    if (res != PAL_SOK) {
        DEPRINTK("%s: Failed to allocate dynamic memory\n",__FUNCTION__);
        return -1;
    }

    SetClassEntryData(*entry, cl);

    return 0;
}

/**************************************************************************/
/*! \fn static int GetClassListEntryByIndex(Uint16 clId, Uint32 sfId, ClassListEntry_t **entry)
 **************************************************************************
 *  \brief Get classifiers list entry by class ID and SF index
 *  \param[in]  Uint16 clID - class ID
 *  \param[in]  Int16 sfId - SF ID
 *  \param[out] ClassListEntry_t **entry - pointer to the found entry
 *  \return  OK/NOK
 */
static int GetClassListEntryByIndex(Uint16 clId, Uint32 sfId, ClassListEntry_t **entry)
{
    struct list_head *pos;
    ClassListEntry_t *curr=NULL;

    // ARRIS NOTE - the caller of this function is responsible for protecting
    // the list entry returned to it.

    /* find a place in the list according to the index */
    list_for_each(pos, &classListHead)
    {
        curr = list_entry(pos, ClassListEntry_t, list);
        if ((curr->cl.classID == clId) && (curr->cl.sfID == sfId)) 
            break;
    }
    if (pos != &classListHead) 
    {
        *entry = curr;
        return 0;
    }
    else
        return -1;
}

/**************************************************************************/
/*! \fn static void ParseVlanTCI(struct vlan_hdr *vlanHdr, Uint16 *prio, Uint16 *id)
 **************************************************************************
 *  \brief Parse the TCI field of VLAN header
 *  \param[in]  struct vlan_hdr *vlanHdr - vlan header 
 *  \param[out] Uint16 *prio - priority
 *  \param[out] Uint16 *id - id
 *  \return  NA
 */
static void ParseVlanTCI(struct vlan_hdr *vlanHdr, Uint16 *prio, Uint16 *id)
{
    *prio = VLAN_TCI_GET_PRIO(ntohs(vlanHdr->h_vlan_TCI));
    *id = VLAN_TCI_GET_ID(ntohs(vlanHdr->h_vlan_TCI));
}

/**************************************************************************/
/*! \fn static ClassParamMatch_e MatchByLlc(struct ethhdr *ethHeader, Uint8 classEthType,
                       Uint16 encProto, QosClassifier_t  *cl)
 **************************************************************************
 *  \brief Verify if the classifier matches packet by LLC criterias
 *  \param[in]  struct ethhdr *ethHeader - packet ethernet header
 *  \param[in]  Uint8 classEthType - ethernet type in terms of classifiers
 *  \param[in]  Uint16 encProto - Etherenet encapsulated protocol
 *  \param[in]  QosClassifier_t  *cl - classifier
 *  \return  Match/No Match
 */
static ClassParamMatch_e MatchByLlc(struct ethhdr *ethHeader, Uint8 classEthType,
                       Uint16 encProto, QosClassifier_t  *cl)
{
    MacAddrBytes_t *destMac, *srcMac;

   /*If LLC Ethernet protocol type is specified*/
   if (cl->clParamsBitmap & CL_BIT_ETHER_TYPE)
   {
        /* Zero or MAC Mng classifier */
        if ((cl->llc.enetProtocolType == FC_LLC_PROTO_TYPE_NONE) ||
            (cl->llc.enetProtocolType == FC_LLC_PROTO_MAC)) {
            return CL_PARAM_NOMATCH;
        }
        /* Not a catch-all classifier */
        if (cl->llc.enetProtocolType != FC_LLC_PROTO_ALL_PDU) 
        {
            /*Check encapsulation type*/
            if(cl->llc.enetProtocolType != classEthType)
                return CL_PARAM_NOMATCH;

            /*Check encapsulation protocol value*/
            if ( (classEthType == FC_LLC_PROTO_ETHERTYPE) && 
                 (encProto != cl->llc.enetProtocol))
            {
                return CL_PARAM_NOMATCH;/* Ethernet protocol isn't matched */
            }
 
            if ( (classEthType == FC_LLC_PROTO_DSAP) &&
                 (encProto != PAL_UINT16_LOW8(cl->llc.enetProtocol)))
            {
                return CL_PARAM_NOMATCH;/* DSAP byte isn't matched */
            }
        }
   }
   /*If destination MAC is specified*/
   if (cl->clParamsBitmap & CL_BIT_DEST_MAC) 
   {
       /*The structure is packed so there won't be unaligned address exception*/
       destMac = (MacAddrBytes_t *)(ethHeader->h_dest);
       if (((destMac->mac4MSB & cl->llc.dstMacMask4bytes) != cl->llc.dstMacAddr4bytes) ||
           ((destMac->mac2LSB & cl->llc.dstMacMask2bytes) != cl->llc.dstMacAddr2bytes)) {
           return CL_PARAM_NOMATCH; /* MAC addresses do not match */
       }

   }
   /*If source MAC is specified*/
   if (cl->clParamsBitmap & CL_BIT_SOUR_MAC)
   {
       /*The structure is packed so there won't be unaligned address exception*/
       srcMac =  (MacAddrBytes_t *)(ethHeader->h_source);
       if ((srcMac->mac4MSB != cl->llc.srcMacAddr4bytes) || 
           (srcMac->mac2LSB != cl->llc.srcMacAddr2bytes)) {
           return CL_PARAM_NOMATCH; /* MAC addresses do not match */
       }
    }
    return CL_PARAM_MATCH; /*the classifier matches*/
}

static ClassParamMatch_e CompareClassifiersByLlc(QosClassifier_t *cl1, QosClassifier_t *cl2)
{
    /* If LLC Ethernet protocol type is specified */
    DPRINTK("%s Compare CL_BIT_ETHER_TYPE, cl1=%d, cl2=%d\n", __FUNCTION__, cl1->clParamsBitmap & CL_BIT_ETHER_TYPE, cl2->clParamsBitmap & CL_BIT_ETHER_TYPE);
    if ((cl1->clParamsBitmap & CL_BIT_ETHER_TYPE) || (cl2->clParamsBitmap & CL_BIT_ETHER_TYPE))
    {
        /* Zero or MAC Mng classifier */
        DPRINTK("%s Check enetProtocolType, cl1=%d, cl2=%d\n", __FUNCTION__, cl1->llc.enetProtocolType, cl2->llc.enetProtocolType);
        if ((cl1->llc.enetProtocolType == FC_LLC_PROTO_TYPE_NONE) || (cl1->llc.enetProtocolType == FC_LLC_PROTO_MAC) ||
            (cl2->llc.enetProtocolType == FC_LLC_PROTO_TYPE_NONE) || (cl2->llc.enetProtocolType == FC_LLC_PROTO_MAC)) 
        {
            DPRINTK("%s Zero or MAC Mng classifier\n", __FUNCTION__);
            return CL_PARAM_NOMATCH;
        }

        /* Not a catch-all classifier */
        if ((cl1->llc.enetProtocolType != FC_LLC_PROTO_ALL_PDU) && (cl2->llc.enetProtocolType != FC_LLC_PROTO_ALL_PDU))
        {
            /*Check encapsulation type*/
            if (cl1->llc.enetProtocolType != cl2->llc.enetProtocolType)
            {
                DPRINTK("%s enetProtocolType mismatch (%d != %d)\n", __FUNCTION__, cl1->llc.enetProtocolType, cl2->llc.enetProtocolType);
                return CL_PARAM_NOMATCH;
            }
            DPRINTK("%s enetProtocolType match (%d = %d)\n", __FUNCTION__, cl1->llc.enetProtocolType, cl2->llc.enetProtocolType);

            /*Check encapsulation protocol value*/
            if ( (cl1->llc.enetProtocol != cl2->llc.enetProtocol))
            {
                DPRINTK("%s enetProtocol mismatch (%d != %d)\n", __FUNCTION__, cl1->llc.enetProtocol, cl2->llc.enetProtocol);
                return CL_PARAM_NOMATCH;    /* Ethernet protocol isn't matched */
            }
            DPRINTK("%s enetProtocol match (%d = %d)\n", __FUNCTION__, cl1->llc.enetProtocol, cl2->llc.enetProtocol);
        }
    }

    /*If destination MAC is specified*/
    DPRINTK("%s Compare CL_BIT_DEST_MAC, cl1=%d, cl2=%d\n", __FUNCTION__, cl1->clParamsBitmap & CL_BIT_DEST_MAC, cl2->clParamsBitmap & CL_BIT_DEST_MAC);
    if ((cl1->clParamsBitmap & CL_BIT_DEST_MAC) && (cl2->clParamsBitmap & CL_BIT_DEST_MAC))
    {
        /*The structure is packed so there won't be unaligned address exception*/
        if ( (cl1->llc.dstMacAddr4bytes != cl2->llc.dstMacAddr4bytes) ||
             (cl1->llc.dstMacAddr2bytes != cl2->llc.dstMacAddr2bytes)) 
        {
            DPRINTK("%s dstMacAddr mismatch (%d != %d)\n", __FUNCTION__, cl1->llc.dstMacAddr4bytes, cl2->llc.dstMacAddr4bytes);
            return CL_PARAM_NOMATCH; /* MAC addresses do not match */
        }
        DPRINTK("%s dstMacAddr smatch (%d = %d)\n", __FUNCTION__, cl1->llc.dstMacAddr4bytes, cl2->llc.dstMacAddr4bytes);

    }
    /*If source MAC is specified*/
    DPRINTK("%s Compare CL_BIT_SOUR_MAC, cl1=%d, cl2=%d\n", __FUNCTION__, cl1->clParamsBitmap & CL_BIT_SOUR_MAC, cl2->clParamsBitmap & CL_BIT_SOUR_MAC);
    if ((cl1->clParamsBitmap & CL_BIT_SOUR_MAC) && (cl2->clParamsBitmap & CL_BIT_SOUR_MAC))
    {
        /*The structure is packed so there won't be unaligned address exception*/
        if ((cl1->llc.srcMacAddr4bytes != cl2->llc.srcMacAddr4bytes) || 
            (cl1->llc.srcMacAddr2bytes != cl2->llc.srcMacAddr2bytes)) 
        {
            DPRINTK("%s srcMacAddr mismatch (%d != %d)\n", __FUNCTION__, cl1->llc.srcMacAddr4bytes, cl2->llc.srcMacAddr4bytes);
            return CL_PARAM_NOMATCH; /* MAC addresses do not match */
        }
        DPRINTK("%s srcMacAddr smatch (%d = %d)\n", __FUNCTION__, cl1->llc.srcMacAddr4bytes, cl2->llc.srcMacAddr4bytes);
    }
    return CL_PARAM_MATCH; /*the classifier matches*/
}

/**************************************************************************/
/*! \fn static ClassParamMatch_e MatchByVlan(Uint16 ethEncapsProto, Uint16 vlanPrio, 
                        Uint16 vlanId, QosClassifier_t   *cl)
 **************************************************************************
 *  \brief Verify if the classifier matches packet by 802.1q criterias
 *  \param[in]  Uint16 vlanPrio - vlan priority
 *  \param[in]  Uint16 vlanId - vlan ID
 *  \param[in]  Uint16 ethEncapsProto - Etherenet encapsulated protocol
 *  \param[in]  QosClassifier_t  *cl - classifier
 *  \return  Match/No match
 */
static ClassParamMatch_e MatchByVlan(Uint16 ethEncapsProto, Uint16 vlanPrio, 
                        Uint16 vlanId, QosClassifier_t   *cl)
{
    /*If priority parameter specified*/
    if (cl->clParamsBitmap & CL_BIT_PRIORITY)
    {
        if ( (vlanPrio < cl->vlan.prioLow )||
             (vlanPrio > cl->vlan.prioHigh) ){
            return CL_PARAM_NOMATCH; 
        }
    }
    if (cl->clParamsBitmap & CL_BIT_VLAN_ID) 
    {
        if (vlanId != cl->vlan.vlanId) {
             return CL_PARAM_NOMATCH;  
        }
    }
    return CL_PARAM_MATCH;
}

static ClassParamMatch_e CompareClassifiersByVlan(QosClassifier_t *cl1, QosClassifier_t *cl2)
{
    /*If priority parameter specified*/
    DPRINTK("%s Compare CL_BIT_PRIORITY, cl1=%d, cl2=%d\n", __FUNCTION__, cl1->clParamsBitmap & CL_BIT_PRIORITY, cl2->clParamsBitmap & CL_BIT_PRIORITY);
    if ((cl1->clParamsBitmap & CL_BIT_PRIORITY) && (cl2->clParamsBitmap & CL_BIT_PRIORITY))
    {
        if ((cl1->vlan.prioHigh < cl2->vlan.prioLow) || (cl1->vlan.prioLow > cl2->vlan.prioHigh))
        {
            DPRINTK("%s prio mismatch (%d-%d != %d-%d)\n", __FUNCTION__, cl1->vlan.prioLow, cl1->vlan.prioHigh, cl2->vlan.prioLow, cl2->vlan.prioHigh);
            return CL_PARAM_NOMATCH; 
        }
        DPRINTK("%s prio match (%d-%d = %d-%d)\n", __FUNCTION__, cl1->vlan.prioLow, cl1->vlan.prioHigh, cl2->vlan.prioLow, cl2->vlan.prioHigh);
    }

    DPRINTK("%s Compare CL_BIT_VLAN_ID, cl1=%d, cl2=%d\n", __FUNCTION__, cl1->clParamsBitmap & CL_BIT_VLAN_ID, cl2->clParamsBitmap & CL_BIT_VLAN_ID);
    if ((cl1->clParamsBitmap & CL_BIT_VLAN_ID) && (cl2->clParamsBitmap & CL_BIT_VLAN_ID))
    {
        if (cl1->vlan.vlanId != cl2->vlan.vlanId) 
        {
            DPRINTK("%s vlanId mismatch (%d != %d)\n", __FUNCTION__, cl1->vlan.vlanId, cl2->vlan.vlanId);
             return CL_PARAM_NOMATCH;  
        }
        DPRINTK("%s vlanId match (%d = %d)\n", __FUNCTION__, cl1->vlan.vlanId, cl2->vlan.vlanId);
    }
    return CL_PARAM_MATCH;
}

/**************************************************************************/
/*! \fn static ClassParamMatch_e MatchByIpv4(struct iphdr  *ipHdr, struct udphdr *udpTcpHdr, 
                                Uint8 ipUpLayerProto, Uint8 *ipUpLayerHdr, QosClassifier_t *cl)
 **************************************************************************
 *  \brief Verify if the classifier matches packet by IPv4 criterias
 *  \param[in]  struct iphdr  *ipHdr - packet IPv4 header
 *  \param[in]  Bool udpTcpHdrPresent - If packet contains UDP/TCP header (is UDP/TCP)
 *  \param[in]  Uint8 ipUpLayerProto - IP encapsulated protocol
 *  \param[in]  Uint8 *ipUpLayerHdr - Data of packet
 *  \param[in]  QosClassifier_t  *cl - classifier
 *  \return  Match/No match
 */
static ClassParamMatch_e MatchByIpv4(struct iphdr  *ipHdr, Bool udpTcpHdrPresent,
                                     Uint8 ipUpLayerProto, Uint8 *ipUpLayerHdr, QosClassifier_t *cl)
{
    struct icmphdr *icmp;

    /*Check IP protocol*/
    if (cl->clParamsBitmap & CL_BIT_IP_PROTOCOL)
    {
        if ((cl->ip.v4.protocol != FC_IP_PROTOCOL_ALL) &&
            (cl->ip.v4.protocol != ipUpLayerProto) &&
            ((cl->ip.v4.protocol != FC_IP_PROTOCOL_UDP_TCP) || !udpTcpHdrPresent))
        {
            return CL_PARAM_NOMATCH;
        }
    }
    /*Check addresses */
    if (cl->clParamsBitmap & CL_BIT_IP_SOUR_ADDR)
    {
        if ((ntohl(ipHdr->saddr) & cl->ip.v4.srcMask) != (cl->ip.v4.srcAddr & cl->ip.v4.srcMask))
        {
            return CL_PARAM_NOMATCH;
        }
    }
    if (cl->clParamsBitmap & CL_BIT_IP_DEST_ADDR)
    {
        if ((ntohl(ipHdr->daddr) & cl->ip.v4.dstMask) != (cl->ip.v4.dstAddr  & cl->ip.v4.dstMask))
        {
            return CL_PARAM_NOMATCH;
        }
    }
    /*Check ToS*/
    if (cl->clParamsBitmap & CL_BIT_IP_TOS)
    {
        if (((ipHdr->tos & cl->ip.v4.tosMask) > (cl->ip.v4.tosHigh & cl->ip.v4.tosMask)) ||
             ((ipHdr->tos & cl->ip.v4.tosMask) < (cl->ip.v4.tosLow & cl->ip.v4.tosMask)))
        {
            return CL_PARAM_NOMATCH;
        }
    }
    /* Check ICMP */
    if (cl->clParamsBitmap & CL_BIT_ICMP46_TYPE)
    {
        if (ipUpLayerProto != IPPROTO_ICMP)
        {
            /* No match */
            return CL_PARAM_NOMATCH;
        }
        else
        {
            /* Check ICMP Type */
            icmp = (struct icmphdr*) ipUpLayerHdr;
            DPRINTK("ICMP packet: type %d, low %d, hi %d\n", icmp->type, cl->Icmp46TypeLow, cl->Icmp46TypeHigh);
            if ((icmp->type < cl->Icmp46TypeLow) || (icmp->type > cl->Icmp46TypeHigh))
            {
                /* No match */
                return CL_PARAM_NOMATCH;
            }
        }
    }

    return CL_PARAM_MATCH;
}

static ClassParamMatch_e CompareClassifiersByIpv4(QosClassifier_t *cl1, QosClassifier_t *cl2)
{
    /*Check IP protocol*/
    DPRINTK("%s Compare CL_BIT_IP_PROTOCOL, cl1=%d, cl2=%d\n", __FUNCTION__, cl1->clParamsBitmap & CL_BIT_IP_PROTOCOL, cl2->clParamsBitmap & CL_BIT_IP_PROTOCOL);
    if ((cl1->clParamsBitmap & CL_BIT_IP_PROTOCOL) && (cl2->clParamsBitmap & CL_BIT_IP_PROTOCOL))
    {
        if (cl1->ip.v4.protocol != cl2->ip.v4.protocol)
        {
            DPRINTK("%s protocol mismatch (%d != %d)\n", __FUNCTION__, cl1->ip.v4.protocol, cl2->ip.v4.protocol);
            return CL_PARAM_NOMATCH;
        }
        DPRINTK("%s protocol match (%d = %d)\n", __FUNCTION__, cl1->ip.v4.protocol, cl2->ip.v4.protocol);
    }

    /*Check addresses */
    DPRINTK("%s Compare CL_BIT_IP_SOUR_ADDR, cl1=%d, cl2=%d\n", __FUNCTION__, cl1->clParamsBitmap & CL_BIT_IP_SOUR_ADDR, cl2->clParamsBitmap & CL_BIT_IP_SOUR_ADDR);
    if ((cl1->clParamsBitmap & CL_BIT_IP_SOUR_ADDR) && (cl2->clParamsBitmap & CL_BIT_IP_SOUR_ADDR))
    {
        if ((cl1->ip.v4.srcAddr & cl1->ip.v4.srcMask) != (cl2->ip.v4.srcAddr & cl2->ip.v4.srcMask))
        {
            DPRINTK("%s srcAddr mismatch (%d != %d)\n", __FUNCTION__, cl1->ip.v4.srcAddr & cl1->ip.v4.srcMask, cl2->ip.v4.srcAddr & cl2->ip.v4.srcMask);
            return CL_PARAM_NOMATCH;
        }
        DPRINTK("%s srcAddr match (%d = %d)\n", __FUNCTION__, cl1->ip.v4.srcAddr & cl1->ip.v4.srcMask, cl2->ip.v4.srcAddr & cl2->ip.v4.srcMask);
    }

    DPRINTK("%s Compare CL_BIT_IP_DEST_ADDR, cl1=%d, cl2=%d\n", __FUNCTION__, cl1->clParamsBitmap & CL_BIT_IP_DEST_ADDR, cl2->clParamsBitmap & CL_BIT_IP_DEST_ADDR);
    if ((cl1->clParamsBitmap & CL_BIT_IP_DEST_ADDR) && (cl2->clParamsBitmap & CL_BIT_IP_DEST_ADDR))
    {
        if ((cl1->ip.v4.dstAddr & cl1->ip.v4.dstMask) != (cl2->ip.v4.dstAddr & cl2->ip.v4.dstMask))
        {
            DPRINTK("%s dstAddr mismatch (%d != %d)\n", __FUNCTION__, cl1->ip.v4.dstAddr & cl1->ip.v4.dstMask, cl2->ip.v4.dstAddr & cl2->ip.v4.dstMask);
            return CL_PARAM_NOMATCH;
        }
        DPRINTK("%s dstAddr match (%d = %d)\n", __FUNCTION__, cl1->ip.v4.dstAddr & cl1->ip.v4.dstMask, cl2->ip.v4.dstAddr & cl2->ip.v4.dstMask);
    }

    /*Check ToS*/
    DPRINTK("%s Compare CL_BIT_IP_TOS, cl1=%d, cl2=%d\n", __FUNCTION__, cl1->clParamsBitmap & CL_BIT_IP_TOS, cl2->clParamsBitmap & CL_BIT_IP_TOS);
    if ((cl1->clParamsBitmap & CL_BIT_IP_TOS) && (cl2->clParamsBitmap & CL_BIT_IP_TOS))
    {
        if (((cl1->ip.v4.tosHigh & cl1->ip.v4.tosMask) < (cl2->ip.v4.tosLow & cl2->ip.v4.tosMask)) || ((cl1->ip.v4.tosLow & cl1->ip.v4.tosMask) > (cl2->ip.v4.tosHigh & cl2->ip.v4.tosMask)))
        {
            DPRINTK("%s tos mismatch (%d-%d != %d-%d)\n", __FUNCTION__, cl1->ip.v4.tosLow, cl1->ip.v4.tosHigh, cl2->ip.v4.tosLow, cl2->ip.v4.tosHigh);
            return CL_PARAM_NOMATCH; 
        }
        DPRINTK("%s tos match (%d-%d = %d-%d)\n", __FUNCTION__, cl1->ip.v4.tosLow, cl1->ip.v4.tosHigh, cl2->ip.v4.tosLow, cl2->ip.v4.tosHigh);
    }

    /* Check ICMP */
    DPRINTK("%s Compare CL_BIT_ICMP46_TYPE, cl1=%d, cl2=%d\n", __FUNCTION__, cl1->clParamsBitmap & CL_BIT_ICMP46_TYPE, cl2->clParamsBitmap & CL_BIT_ICMP46_TYPE);
    if ((cl1->clParamsBitmap & CL_BIT_ICMP46_TYPE) && (cl2->clParamsBitmap & CL_BIT_ICMP46_TYPE))
    {
        if ((cl1->Icmp46TypeHigh < cl2->Icmp46TypeLow) || (cl1->Icmp46TypeLow > cl2->Icmp46TypeHigh))
        {
            DPRINTK("%s Icmp46Type mismatch (%d-%d != %d-%d)\n", __FUNCTION__, cl1->Icmp46TypeLow, cl1->Icmp46TypeHigh, cl2->Icmp46TypeLow, cl2->Icmp46TypeHigh);
            /* No match */
            return CL_PARAM_NOMATCH;
        }
        DPRINTK("%s Icmp46Type match (%d-%d = %d-%d)\n", __FUNCTION__, cl1->Icmp46TypeLow, cl1->Icmp46TypeHigh, cl2->Icmp46TypeLow, cl2->Icmp46TypeHigh);
    }

    return CL_PARAM_MATCH;
}

/**************************************************************************/
/*! \fn static ClassParamMatch_e MatchByIpv6(struct Ipv6hdr_s  *ipHdr, Bool udpTcpHdrPresent,
                                             Uint8 ipUpLayerProto, Uint8 *ipUpLayerHdr, QosClassifier_t *cl)
 **************************************************************************
 *  \brief Verify if the classifier matches packet by IPv6 criterias
 *  \param[in]  sstruct Ipv6hdr_s  *ipHdr - packet IPv6 header
 *  \param[in]  Bool udpTcpHdrPresent - If packet contains UDP/TCP header (is UDP/TCP)
 *  \param[in]  Uint8 ipUpLayerProto - IP encapsulated protocol
 *  \param[in]  Uint8 *ipUpLayerHdr - Data of packet
 *  \param[in]  QosClassifier_t  *cl - classifier
 *  \return Match/No match
 */
static ClassParamMatch_e MatchByIpv6(struct Ipv6hdr_s  *ipHdr, Bool udpTcpHdrPresent,
                                     Uint8 ipUpLayerProto, Uint8 *ipUpLayerHdr, QosClassifier_t *cl)
{
    int i;
    struct icmp6hdr *icmp6;

    /*If TOS parameter is specified*/
    if (cl->clParamsBitmap & CL_BIT_IP_TOS) 
    {
        if (((IPV6CLASS(ipHdr) & cl->ip.v6.tcMask) > (cl->ip.v6.tcHigh & cl->ip.v6.tcMask)) ||
            ((IPV6CLASS(ipHdr) & cl->ip.v6.tcMask) < (cl->ip.v6.tcLow & cl->ip.v6.tcMask))){
            return CL_PARAM_NOMATCH; /* no match */
        }
    }
    /*If Flow Label parameter is specified*/
    if (cl->clParamsBitmap & CL_BIT_FLOW_LABEL) 
    {
        if (IPV6FLABEL(ipHdr) != (cl->ip.v6.flowLabel & 0xfffff)){
            return CL_PARAM_NOMATCH; /* no match */
        }
    }
    /*If Next Header parameter is specified*/
    if (cl->clParamsBitmap & CL_BIT_IP_PROTOCOL) 
    {
        if(cl->ip.v6.nextHdr != FC_IP_PROTOCOL_ALL) 
        {
            /*Don't match a packet with ESP header*/
            if (ipUpLayerProto == NEXTHDR_ESP){
                return CL_PARAM_NOMATCH;
            }

            if((cl->ip.v6.nextHdr != ipUpLayerProto) &&
               ((cl->ip.v6.nextHdr != FC_IP_PROTOCOL_UDP_TCP) || !udpTcpHdrPresent)) {
                return CL_PARAM_NOMATCH;
            }

        }
    }
    /*Verify address match if this criteria is specified*/
    if (cl->clParamsBitmap & CL_BIT_IP_SOUR_ADDR)
    {
        for (i=0; i<4; i++) 
        {
            if((ipHdr->saddr.in6_u.u6_addr32[i] & cl->ip.v6.srcMask[i]) != (cl->ip.v6.srcAddr[i] & cl->ip.v6.srcMask[i]))
                return CL_PARAM_NOMATCH;
        }
    }
    if (cl->clParamsBitmap & CL_BIT_IP_DEST_ADDR)
    {
        for (i=0; i<4; i++) 
        {
            if((ipHdr->daddr.in6_u.u6_addr32[i] & cl->ip.v6.dstMask[i]) != (cl->ip.v6.dstAddr[i] & cl->ip.v6.dstMask[i]))
                return CL_PARAM_NOMATCH;
        }
    }
    /* Check ICMP */
    if (cl->clParamsBitmap & CL_BIT_ICMP46_TYPE)
    {
        if (ipUpLayerProto != IPPROTO_ICMPV6)
        {
            /* No match */
            return CL_PARAM_NOMATCH;
        }
        else
        {
            /* Check ICMP Type */
            icmp6 = (struct icmp6hdr*) ipUpLayerHdr;
            DPRINTK("ICMP packet: type %d, low %d, hi %d\n", icmp6->icmp6_type, cl->Icmp46TypeLow, cl->Icmp46TypeHigh);
            if ((icmp6->icmp6_type < cl->Icmp46TypeLow) || (icmp6->icmp6_type > cl->Icmp46TypeHigh))
            {
                return CL_PARAM_NOMATCH;
            }
        }
    }

    return CL_PARAM_MATCH;
}

static ClassParamMatch_e CompareClassifiersByIpv6(QosClassifier_t *cl1, QosClassifier_t *cl2)
{
    int i;

    /*If TOS parameter is specified*/
    DPRINTK("%s Compare CL_BIT_IP_TOS, cl1=%d, cl2=%d\n", __FUNCTION__, cl1->clParamsBitmap & CL_BIT_IP_TOS, cl2->clParamsBitmap & CL_BIT_IP_TOS);
    if ((cl1->clParamsBitmap & CL_BIT_IP_TOS) && (cl2->clParamsBitmap & CL_BIT_IP_TOS))
    {
        if (((cl1->ip.v6.tcHigh & cl1->ip.v6.tcMask) < (cl2->ip.v6.tcLow & cl2->ip.v6.tcMask)) || ((cl1->ip.v6.tcLow & cl1->ip.v6.tcMask) > (cl2->ip.v6.tcHigh & cl2->ip.v6.tcMask)))
        {
            DPRINTK("%s tc mismatch (%d-%d != %d-%d)\n", __FUNCTION__, cl1->ip.v6.tcLow, cl1->ip.v6.tcHigh, cl2->ip.v6.tcLow, cl2->ip.v6.tcHigh);
            return CL_PARAM_NOMATCH; 
        }
        DPRINTK("%s tc match (%d-%d = %d-%d)\n", __FUNCTION__, cl1->ip.v6.tcLow, cl1->ip.v6.tcHigh, cl2->ip.v6.tcLow, cl2->ip.v6.tcHigh);
    }

    /*If Flow Label parameter is specified*/
    DPRINTK("%s Compare CL_BIT_FLOW_LABEL, cl1=%d, cl2=%d\n", __FUNCTION__, cl1->clParamsBitmap & CL_BIT_FLOW_LABEL, cl2->clParamsBitmap & CL_BIT_FLOW_LABEL);
    if ((cl1->clParamsBitmap & CL_BIT_FLOW_LABEL) && (cl2->clParamsBitmap & CL_BIT_FLOW_LABEL))
    {
        if ((cl1->ip.v6.flowLabel & 0xfffff) != (cl2->ip.v6.flowLabel & 0xfffff))
        {
            DPRINTK("%s flowLabel mismatch (%d != %d)\n", __FUNCTION__, cl1->ip.v6.flowLabel & 0xfffff, cl2->ip.v6.flowLabel & 0xfffff);
            return CL_PARAM_NOMATCH;
        }
        DPRINTK("%s flowLabel match (%d = %d)\n", __FUNCTION__, cl1->ip.v6.flowLabel & 0xfffff, cl2->ip.v6.flowLabel & 0xfffff);
    }

    /*If Next Header parameter is specified*/
    DPRINTK("%s Compare CL_BIT_IP_PROTOCOL, cl1=%d, cl2=%d\n", __FUNCTION__, cl1->clParamsBitmap & CL_BIT_IP_PROTOCOL, cl2->clParamsBitmap & CL_BIT_IP_PROTOCOL);
    if ((cl1->clParamsBitmap & CL_BIT_IP_PROTOCOL) && (cl2->clParamsBitmap & CL_BIT_IP_PROTOCOL))
    {
        if (cl1->ip.v6.nextHdr != cl2->ip.v6.nextHdr) 
        {
            DPRINTK("%s nextHdr mismatch (%d != %d)\n", __FUNCTION__, cl1->ip.v6.nextHdr, cl2->ip.v6.nextHdr);
            return CL_PARAM_NOMATCH;
        }
        DPRINTK("%s nextHdr match (%d = %d)\n", __FUNCTION__, cl1->ip.v6.nextHdr, cl2->ip.v6.nextHdr);
    }

    /*Verify address match if this criteria is specified*/
    DPRINTK("%s Compare CL_BIT_IP_SOUR_ADDR, cl1=%d, cl2=%d\n", __FUNCTION__, cl1->clParamsBitmap & CL_BIT_IP_SOUR_ADDR, cl2->clParamsBitmap & CL_BIT_IP_SOUR_ADDR);
    if ((cl1->clParamsBitmap & CL_BIT_IP_SOUR_ADDR) && (cl2->clParamsBitmap & CL_BIT_IP_SOUR_ADDR))
    {
        for (i = 0; i < 4; i++) 
        {
            if((cl1->ip.v6.srcAddr[i] & cl1->ip.v6.srcMask[i]) != (cl2->ip.v6.srcAddr[i] & cl2->ip.v6.srcMask[i]))
            {
                DPRINTK("%s srcAddr mismatch (%d != %d)\n", __FUNCTION__, cl1->ip.v6.srcAddr[i] & cl1->ip.v6.srcMask[i], cl2->ip.v6.srcAddr[i] & cl2->ip.v6.srcMask[i]);
                return CL_PARAM_NOMATCH;
            }
            DPRINTK("%s srcAddr match (%d = %d)\n", __FUNCTION__, cl1->ip.v6.srcAddr[i] & cl1->ip.v6.srcMask[i], cl2->ip.v6.srcAddr[i] & cl2->ip.v6.srcMask[i]);
        }
    }

    DPRINTK("%s Compare CL_BIT_IP_DEST_ADDR, cl1=%d, cl2=%d\n", __FUNCTION__, cl1->clParamsBitmap & CL_BIT_IP_DEST_ADDR, cl2->clParamsBitmap & CL_BIT_IP_DEST_ADDR);
    if ((cl1->clParamsBitmap & CL_BIT_IP_DEST_ADDR) && (cl2->clParamsBitmap & CL_BIT_IP_DEST_ADDR))
    {
        for (i = 0; i < 4; i++) 
        {
            if((cl1->ip.v6.dstAddr[i] & cl1->ip.v6.dstMask[i]) != (cl2->ip.v6.dstAddr[i] & cl2->ip.v6.dstMask[i]))
            {
                DPRINTK("%s dstAddr mismatch (%d != %d)\n", __FUNCTION__, cl1->ip.v6.dstAddr[i] & cl1->ip.v6.dstMask[i], cl2->ip.v6.dstAddr[i] & cl2->ip.v6.dstMask[i]);
                return CL_PARAM_NOMATCH;
            }
            DPRINTK("%s dstAddr match (%d = %d)\n", __FUNCTION__, cl1->ip.v6.dstAddr[i] & cl1->ip.v6.dstMask[i], cl2->ip.v6.dstAddr[i] & cl2->ip.v6.dstMask[i]);
        }
    }

    /* Check ICMP */
    DPRINTK("%s Compare CL_BIT_ICMP46_TYPE, cl1=%d, cl2=%d\n", __FUNCTION__, cl1->clParamsBitmap & CL_BIT_ICMP46_TYPE, cl2->clParamsBitmap & CL_BIT_ICMP46_TYPE);
    if ((cl1->clParamsBitmap & CL_BIT_ICMP46_TYPE) && (cl2->clParamsBitmap & CL_BIT_ICMP46_TYPE))
    {
        if ((cl1->Icmp46TypeHigh < cl2->Icmp46TypeLow) || (cl1->Icmp46TypeLow > cl2->Icmp46TypeHigh))
        {
            DPRINTK("%s Icmp46Type mismatch (%d-%d != %d-%d)\n", __FUNCTION__, cl1->Icmp46TypeLow, cl1->Icmp46TypeHigh, cl2->Icmp46TypeLow, cl2->Icmp46TypeHigh);
            return CL_PARAM_NOMATCH; 
        }
        DPRINTK("%s Icmp46Type match (%d-%d = %d-%d)\n", __FUNCTION__, cl1->Icmp46TypeLow, cl1->Icmp46TypeHigh, cl2->Icmp46TypeLow, cl2->Icmp46TypeHigh);
    }

    return CL_PARAM_MATCH;
}

/**************************************************************************/
/*! \fn static ClassParamMatch_e MatchByUdpTcp(struct udphdr *udpTcpHdr, QosClassifier_t *cl)
 **************************************************************************
 *  \brief Verify if the classifier matches packet by UDP/TCP criterias
 *  \param[in]  struct udphdr *udpTcpHdr - packet UDP/TCP header
 *  \param[in]  QosClassifier_t  *cl - classifier
 *  \return  Match/No match
 */
static ClassParamMatch_e MatchByUdpTcp(struct udphdr *udpTcpHdr, QosClassifier_t *cl)
{
    /* TCP/UDP - test ports range */
    if (((ntohs(udpTcpHdr->source) < cl->udpTcp.srcPortStart) ||
         (ntohs(udpTcpHdr->source) > cl->udpTcp.srcPortEnd) ||
         (ntohs(udpTcpHdr->dest) < cl->udpTcp.dstPortStart) ||
         (ntohs(udpTcpHdr->dest) > cl->udpTcp.dstPortEnd))) 
    {
        return CL_PARAM_NOMATCH; /* no match */
    }
    return  CL_PARAM_MATCH; 
}

static ClassParamMatch_e CompareClassifiersByUdpTcp(QosClassifier_t *cl1, QosClassifier_t *cl2)
{
    /* TCP/UDP - test ports range */
    if ((cl1->udpTcp.srcPortEnd < cl2->udpTcp.srcPortStart) || (cl1->udpTcp.srcPortStart > cl2->udpTcp.srcPortEnd))
    {
        DPRINTK("%s srcPort mismatch (%d-%d != %d-%d)\n", __FUNCTION__, cl1->udpTcp.srcPortStart, cl1->udpTcp.srcPortEnd, cl2->udpTcp.srcPortStart, cl2->udpTcp.srcPortEnd);
        return CL_PARAM_NOMATCH; 
    }
    DPRINTK("%s srcPort match (%d-%d = %d-%d)\n", __FUNCTION__, cl1->udpTcp.srcPortStart, cl1->udpTcp.srcPortEnd, cl2->udpTcp.srcPortStart, cl2->udpTcp.srcPortEnd);
	
	if ((cl1->udpTcp.dstPortEnd < cl2->udpTcp.dstPortStart) || (cl1->udpTcp.dstPortStart > cl2->udpTcp.dstPortEnd))
    {
        DPRINTK("%s dstPort mismatch (%d-%d != %d-%d)\n", __FUNCTION__, cl1->udpTcp.dstPortStart, cl1->udpTcp.dstPortEnd, cl2->udpTcp.dstPortStart, cl2->udpTcp.dstPortEnd);
        return CL_PARAM_NOMATCH; 
    }
    DPRINTK("%s dstPort match (%d-%d = %d-%d)\n", __FUNCTION__, cl1->udpTcp.dstPortStart, cl1->udpTcp.dstPortEnd, cl2->udpTcp.dstPortStart, cl2->udpTcp.dstPortEnd);

    return  CL_PARAM_MATCH; 
}

/**************************************************************************/
/*! \fn static void SetClassEntryData(ClassListEntry_t *entry, QosClassifier_t *cl)
 **************************************************************************
 *  \brief Set Classifier data into the list entry
 *  \param[out] ClassListEntry_t *entry - Classifier entry in the list
 *  \param[in]  QosClassifier_t *cl - classifier data
 *  \return  NA
 */
static void SetClassEntryData(ClassListEntry_t *entry, QosClassifier_t *cl)
{
    Uint32 sysIfIndex, cmim;
    int i;

    /*Translate the CMIM to system interface numbering*/
    cmim = (Uint32) cl->cmim;

    cl->cmim = 0;
    for (i=DOCSIS_MIN_IF_INDEX; i<=DOCSIS_MAX_IF_CMIM_INDEX; i++) 
    {
        if ((0x80000000 >> i) & cmim) 
        {
            DPRINTK("Docsis CMIM bit %d is on\n", 32-i-1);

            /*Translate interface index from docsis numbering to Kernel numbering */
            if (FcUtils_GetSysIfIndexByDocsis(i, &sysIfIndex) != 0) 
            {
                DEPRINTK("%s: Failed translate docsis interface index %u to system one\n",__FUNCTION__, i );
                return;
            }
            cl->cmim |= (BIT_SHIFT_INVERT_64(sysIfIndex-1));
        }
    }
    DPRINTK("Setting Classifier %u data:\n", cl->classID);
    DPRINTK("Docsis CMIM=0x%x, system CMIM=0x%llx\n", cmim, cl->cmim);

    PrintClassData(cl);

    PAL_osMemCopy(&(entry->cl), cl, sizeof(QosClassifier_t));
}

/**************************************************************************/
/*! \fn static void PrintClassData(QosClassifier_t *cl)
 **************************************************************************
 *  \brief Print Classifier data
 *  \param[in]  QosClassifier_t *cl - classifier data
 *  \return  NA
 */
static void PrintClassData(QosClassifier_t *cl)
{
    int i;

    DPRINTK("classID = %u\n", cl->classID);
    DPRINTK("sfIndex = %u\n", cl->sfIndex);
    DPRINTK("phsIndex = %u\n", cl->phsIndex);
    DPRINTK("priority = %u\n", cl->priority);
    DPRINTK("pktCounter = %u\n", cl->pktCounter);
    DPRINTK("ppPktCounter = %u\n", cl->ppCounters.ppPktCounter);

    /*Which classification criterias are used in this classifier*/
    DPRINTK("isLlcCriterion = %u\n", cl->isLlcCriterion);
    DPRINTK("isVlanCriterion = %u\n", cl->isVlanCriterion);
    DPRINTK("isIpv4Criterion = %u\n", cl->isIpv4Criterion);
    DPRINTK("isIpv6Criterion = %u\n", cl->isIpv6Criterion);
    DPRINTK("isTcpUdpCriterion = %u\n", cl->isTcpUdpCriterion);
    DPRINTK("isCmimCriterion = %u\n", cl->isCmimCriterion);

    /* Bitmap of parameters which are relevant */
    DPRINTK("clParamsBitmap = %08X\n", cl->clParamsBitmap);

    /* Classifier parameters */
    if (cl->isIpv4Criterion) 
    {
        DPRINTK("IPv4 parameters:\n");
        DPRINTK("srcAddr = 0x%.8x\n",cl->ip.v4.srcAddr);
        DPRINTK("srcMask = 0x%.8x\n",cl->ip.v4.srcMask);
        DPRINTK("dstAddr = 0x%.8x\n",cl->ip.v4.dstAddr);
        DPRINTK("dstMask = 0x%.8x\n",cl->ip.v4.dstMask);
        DPRINTK("protocol = %u\n",cl->ip.v4.protocol);
        DPRINTK("tosLow = %u\n",cl->ip.v4.tosLow);
        DPRINTK("tosHigh = %u\n",cl->ip.v4.tosHigh);
        DPRINTK("tosMask = 0x%x\n",cl->ip.v4.tosMask);
        DPRINTK("Icmp46TypeLow = %u\n",cl->Icmp46TypeLow);
        DPRINTK("Icmp46TypeHigh = %u\n",cl->Icmp46TypeHigh);
    }
    if (cl->isIpv6Criterion) 
    {
        DPRINTK("IPv6 parameters:\n");
        DPRINTK("flowLabel = %u\n", cl->ip.v6.flowLabel); 
        DPRINTK("srcAddr = 0x"); 
        for (i=0; i<4; i++) 
            DPRINTK("%.8x ",cl->ip.v6.srcAddr[i]);
        DPRINTK("\nsrcMask = 0x");
        for (i=0; i<4; i++) 
            DPRINTK("%.8x ",cl->ip.v6.srcMask[i]);
        DPRINTK("\ndstAddr = 0x");
        for (i=0; i<4; i++) 
            DPRINTK("%.8x ",cl->ip.v6.dstAddr[i]);
        DPRINTK("\ndstMask = 0x");
        for (i=0; i<4; i++) 
            DPRINTK("%.8x ",cl->ip.v6.dstMask[i]);
        DPRINTK("\n"); 
        DPRINTK("nextHdr = %u\n", cl->ip.v6.nextHdr);
        DPRINTK("tosLow = %u\n",cl->ip.v6.tcLow);
        DPRINTK("tosHigh = %u\n",cl->ip.v6.tcHigh);
        DPRINTK("tosMask = 0x%x\n",cl->ip.v6.tcMask);
        DPRINTK("Icmp46TypeLow = %u\n",cl->Icmp46TypeLow);
        DPRINTK("Icmp46TypeHigh = %u\n",cl->Icmp46TypeHigh);
    }

    if (cl->isTcpUdpCriterion) 
    {
        DPRINTK("udpTcp parameters:\n");
        DPRINTK("srcPortStart = %u\n", cl->udpTcp.srcPortStart);
        DPRINTK("srcPortEnd = %u\n", cl->udpTcp.srcPortEnd);
        DPRINTK("dstPortStart = %u\n", cl->udpTcp.dstPortStart);
        DPRINTK("dstPortEnd = %u\n", cl->udpTcp.dstPortEnd);
    }

    if (cl->isLlcCriterion) 
    {
#ifdef DCLASS_DEBUG
        Uint8 *fourByteBuf, *twoByteBuf;
        DPRINTK("llc parameters:\n");
        fourByteBuf = (Uint8 *)&cl->llc.dstMacAddr4bytes;
        twoByteBuf = (Uint8 *)&cl->llc.dstMacAddr2bytes;
        DPRINTK("dstMacAddr = %02X:%02X:%02X:%02X:%02X:%02X\n",
               fourByteBuf[0], fourByteBuf[1],fourByteBuf[2],fourByteBuf[3],
               twoByteBuf[0],twoByteBuf[1]);
        fourByteBuf = (Uint8 *)&cl->llc.dstMacMask4bytes;
        twoByteBuf = (Uint8 *)&cl->llc.dstMacMask2bytes;
        DPRINTK("dstMacMask = %02X:%02X:%02X:%02X:%02X:%02X\n",
               fourByteBuf[0], fourByteBuf[1],fourByteBuf[2],fourByteBuf[3],
               twoByteBuf[0],twoByteBuf[1]);

        fourByteBuf = (Uint8 *)&cl->llc.srcMacAddr4bytes;
        twoByteBuf = (Uint8 *)&cl->llc.srcMacAddr2bytes;
        DPRINTK("srcMacAddr = %02X:%02X:%02X:%02X:%02X:%02X\n",
               fourByteBuf[0], fourByteBuf[1],fourByteBuf[2],fourByteBuf[3],
               twoByteBuf[0],twoByteBuf[1]);
#endif
        DPRINTK("enetProtocol = %u\n", cl->llc.enetProtocol);
        DPRINTK("enetProtocolType = %u\n", cl->llc.enetProtocolType);
    }

    if (cl->isVlanCriterion) 
    {
        DPRINTK("vlan parameters:\n");
        DPRINTK("vlanId = %u\n", cl->vlan.vlanId);
        DPRINTK("prioLow = %u\n", cl->vlan.prioLow);
        DPRINTK("prioHigh = %u\n", cl->vlan.prioHigh);
    }

    if (cl->isCmimCriterion) 
    {
        DPRINTK("CMIM = %llX\n", cl->cmim);
    }

}

// ARRIS MODIFY - added okToIncClassMib parameter
/**************************************************************************/
/*! \fn int QosClass_RetrieveClassifyData(struct sk_buff *skb, int *sfIndex, int *phsIndex, int *l2vpnRelate, int okToIncClassMib)
 **************************************************************************
 *  \brief Classify MAC data packet
 *  \param[in]  sk_buff *skb - pointer to packet skb
 *  \param[out] int *sfIndex - SF index
 *  \param[out] int *phsIndex - PHS index
 *  \param[out] int *l2vpnRelate - SF related to l2vpn indication
 *  \return  0 or error code
 */
// ARRIS MODIFY - added okToIncClassMib parameter
static int QosClass_RetrieveClassifyData(struct sk_buff *skb, int *sfIndex, int *phsIndex, int *l2vpnRelate, int okToIncClassMib)
{
    struct list_head *pos;
    ClassListEntry_t *entry;
    QosClassifier_t  *cl;
    struct ethhdr *ethHeader;
    struct vlan_hdr *vlanHdr;
    void  *ipHdr;
    Uint8 *ipUpLayerHdr, *ethData;
    EthEncapsType_e ethEncapsType; /*Ethernet encapsulation type*/
    Uint8 ipUpLayerProto;
    Uint16 ethEncapsProto, vlanPrio, vlanId;
    Uint32 classEthType; /*Ethernet encapsulation type in classifier terms*/
    ClassParamMatch_e llcMatch, vlanMatch, ipv4Match, ipv6Match, udpTcpMatch,
                      cmimMatch, clMatch;
    Bool vlanHdrPresent, ipv4HdrPresent, ipv6HdrPresent, udpTcpHdrPresent;
    Uint32 lockKey, srcIfIndex;

    DPRINTK("%s -Start\n",__FUNCTION__);

    *sfIndex = QOS_SF_PRIMARY_SF_INDEX;
    *phsIndex = 0xFF;
    clMatch = False;
    cl = NULL;

    /*If no active classifiers don't search*/
    if (!numQoSClassifiers && !numUdcClassifiers)
    {
        DPRINTK("%s - No active classifiers\n",__FUNCTION__);
        if (l2vpnRelate)
        {
            L2vpn_GetRelatedSf(QOS_SF_PRIMARY_SF_INDEX, l2vpnRelate);
        }
        // ARRIS ADD Start
        if (l2vpnRelate)
        {
           L2vpn_GetRelatedSf(QOS_SF_PRIMARY_SF_INDEX, l2vpnRelate);
        }
        // ARRIS ADD End

        return 0;
    }


    /*Parse packet network headers*/
    vlanHdrPresent = ipv4HdrPresent = ipv6HdrPresent = udpTcpHdrPresent = False;
    ethHeader = eth_hdr(skb);
    ethData = skb->data + ETH_HLEN;
#if 1
    DPRINTK("eth hdr: hdest=%.2x:%.2x:%.2x:%.2x:%.2x:%.2x\n",ethHeader->h_dest[0],
           ethHeader->h_dest[1],ethHeader->h_dest[2],ethHeader->h_dest[3],ethHeader->h_dest[4],
           ethHeader->h_dest[5]);
    DPRINTK("eth hdr: hsource=%.2x:%.2x:%.2x:%.2x:%.2x:%.2x\n",ethHeader->h_source[0],
           ethHeader->h_source[1],ethHeader->h_source[2],ethHeader->h_source[3],ethHeader->h_source[4],
           ethHeader->h_source[5]);

    DPRINTK("h_proto=0x%.4x\n",ethHeader->h_proto);
#endif
    FcUtils_ExtractEthEncapsType(ethHeader, ethData, &ethEncapsType, &ethEncapsProto, &vlanHdrPresent);
    DPRINTK("%s: ethEncapsType=%u, ethEncapsProto=0x%x, vlanHdrPresent=%u, skb data= \n",__FUNCTION__,
           ethEncapsType, ethEncapsProto, vlanHdrPresent);
#if 0
    { 
        int i;
        for (i=0;i<30;i++) {
            DPRINTK("%.2X ",ethData[i]);
        }
    }
    DPRINTK("\n");
#endif
    if (FcUtils_ExtractNetHdrs(ethData, ethEncapsType, ethEncapsProto, vlanHdrPresent, &ipHdr, &vlanHdr) != 0)
    {
        DIPRINTK("%s: Unsupported packet format - no matching classifer!\n", __FUNCTION__);
        return 0;
    }
#if 0
    {
        struct iphdr *iph = (struct iphdr *)ipHdr;
        DPRINTK("Ip header: \n");
        DPRINTK("ver_len =%.2x, tos=%.2x, protocol=%.2x, saddr=%.8x",
           iph->version,iph->tos,iph->protocol,iph->saddr);
    }
#endif
    FcUtils_ExtractIpUpLayerHdr(ethEncapsProto,ipHdr, &ipUpLayerProto, &ipUpLayerHdr);
    DPRINTK("%s ipUpLayerProto=0x%.2x ipUpLayerHdr=%p\n",__FUNCTION__, ipUpLayerProto, ipUpLayerHdr);

    /*Translate the type of encapsulated protocol to classifiers terms*/
    if (ethEncapsType == ETH_ENCAP_V2 || ethEncapsType == ETH_ENCAP_802_2_3_SNAP)
    {
        classEthType = FC_LLC_PROTO_ETHERTYPE;
        if(ethEncapsProto == ETH_P_IP) 
            ipv4HdrPresent = True;
        else if (ethEncapsProto == ETH_P_IPV6) 
            ipv6HdrPresent = True;
    }
    else
        classEthType = FC_LLC_PROTO_DSAP;

    /*Parse IEEE 802.1Q header*/
    vlanPrio = vlanId = 0;
    if (vlanHdrPresent) 
    {
        ParseVlanTCI(vlanHdr, &vlanPrio, &vlanId); 
    }

    if ((ipv4HdrPresent || ipv6HdrPresent) &&
       ((ipUpLayerProto == IPPROTO_UDP) || (ipUpLayerProto == IPPROTO_TCP)))
        udpTcpHdrPresent=True;

    /*Get source interface index from skb*/
    srcIfIndex = skb->ti_docsis_input_dev->ifindex;
    DPRINTK("%s: packet from source interface index %u\n",__FUNCTION__, srcIfIndex);

    /* go over the classifier table and try to find a match */
    PAL_osProtectEntry(PAL_OSPROTECT_INTERRUPT, &lockKey);
    list_for_each(pos, &classListHead)
    {
        entry = list_entry(pos, ClassListEntry_t, list);
        cl = &(entry->cl);

        DPRINTK("%s Checking classifier %u of SF %u\n",__FUNCTION__, cl->classID, cl->sfID);

        llcMatch = vlanMatch = ipv4Match = ipv6Match = udpTcpMatch = cmimMatch = CL_PARAM_UNRELEVANT;
        
        /* Verify all relevant parameters*/
        if (cl->isLlcCriterion) /*If LLC matching criteria is used*/
        {
            DPRINTK("%s Using LLC criterion\n",__FUNCTION__);

            llcMatch = MatchByLlc(ethHeader, classEthType, ethEncapsProto, cl);
            if (llcMatch == CL_PARAM_NOMATCH) 
                continue;
            DPRINTK("Classifier matched by LLC criterion\n");
        }
        if  (cl->isVlanCriterion) 
        {
            DPRINTK("%s Using VLAN criterion\n",__FUNCTION__);

            /*If VLAN header is present attempt to find a match, otherwise return no match*/
            if (vlanHdrPresent)
                vlanMatch = MatchByVlan(ethEncapsProto, vlanPrio, vlanId, cl);
            else
                vlanMatch = CL_PARAM_NOMATCH;

            if (vlanMatch == CL_PARAM_NOMATCH) 
                continue;

            DPRINTK("Classifier matched by 802.1P/Q criterion\n");

        }
        /*If IPv4 matching criterion flag is set if at least one IPv4 parameter is present besides TCP/UDP*/ 
        if (cl->isIpv4Criterion)
        {
            DPRINTK("%s Using ipV4 criterion\n",__FUNCTION__);

            /*If IPv4 header is present attempt to find a match, otherwise return no match*/
            if (ipv4HdrPresent)
                ipv4Match = MatchByIpv4((struct iphdr *)ipHdr, udpTcpHdrPresent, ipUpLayerProto, ipUpLayerHdr, cl);      
            else
                ipv4Match = CL_PARAM_NOMATCH;

            if (ipv4Match == CL_PARAM_NOMATCH) 
                    continue;

            DPRINTK("Classifier matched by IPv4 criterion\n");
        }
        else if (cl->isIpv6Criterion)
        {
            DPRINTK("%s Using ipV6 criterion\n",__FUNCTION__);

            /*If IPv6 header is present attempt to find a match, otherwise return no match*/
            if (ipv6HdrPresent)
                ipv6Match = MatchByIpv6((struct Ipv6hdr_s *)ipHdr, udpTcpHdrPresent, ipUpLayerProto, ipUpLayerHdr, cl);
            else
                ipv6Match = CL_PARAM_NOMATCH;

            if (ipv6Match == CL_PARAM_NOMATCH) 
                continue;

            DPRINTK("Classifier matched by IPv6 criterion\n");
        }
        /*Verify UDP/TCP criterion independetly of IP protocol type*/
        /* If all of TCP/UDP parameters are omitted - default values are used which are
        all the possible range so no need to check if none of them specified*/
        if (cl->isTcpUdpCriterion) 
        {
            DPRINTK("%s Using TCP/UDP criterion\n",__FUNCTION__);

            if (udpTcpHdrPresent)
                udpTcpMatch = MatchByUdpTcp((struct udphdr *)ipUpLayerHdr, cl);
            else
                udpTcpMatch = CL_PARAM_NOMATCH;

            if (udpTcpMatch == CL_PARAM_NOMATCH)
                continue;
            DPRINTK("Classifier matched by TCP/UDP criterion\n");
        }

        /*CMIM classification*/
        if (cl->isCmimCriterion) 
        {
            DPRINTK("%s Using CMIM criterion\n",__FUNCTION__);
            if ((BIT_SHIFT_INVERT_64(srcIfIndex-1)) & cl->cmim) 
                cmimMatch = CL_PARAM_MATCH;
            else
                continue;
            DPRINTK("Classifier matched by CMIM criterion\n");

        }

        /*If we reached here all the parameters are matching or unrelevant*/
        /*If all the relevant parameters match - the classifier matches*/
        if ((llcMatch == CL_PARAM_MATCH) || (vlanMatch== CL_PARAM_MATCH) || 
            (ipv4Match == CL_PARAM_MATCH) || (ipv6Match==CL_PARAM_MATCH) || 
            (udpTcpMatch == CL_PARAM_MATCH) || (cmimMatch == CL_PARAM_MATCH) )
        {
            clMatch = True;
            break;
        }
    }
    if (clMatch) 
    {
        /*In case of UDC SF Index would be -1 */
        *sfIndex = cl->sfIndex;
        if (l2vpnRelate == NULL)
        {
            if (!IS_UPSTREAM_DROP_CLASSIFIER(cl))
            {
                DPRINTK("%s: QoS Classifier %u of SF %u matched, PHS=0x%x\n",__FUNCTION__,cl->classID, cl->sfID, cl->phsIndex);
                /* Check if there is valid PHS rule for this classifier */
                if( cl->phsIndex != 0xFF )
                {
                    if (UsPhsVerify(skb, cl->phsIndex) == 0)
                    {
                        *phsIndex = cl->phsIndex;
                        DPRINTK("%s:PHS verify is succefull, PHS index=0x%x!\n",__FUNCTION__, *phsIndex);
                    }
                }
            }   
            else
            {
                DPRINTK("%s: Upstream Drop Classifier %u matched\n",__FUNCTION__,cl->classID);
            }
    
        //    PAL_osProtectEntry(PAL_OSPROTECT_INTERRUPT, &lockKey);    ARRIS MOVE
            // ARRIS MODIFY - start: added check for okToIncClassMib
            if (okToIncClassMib == True)
            {
                cl->pktCounter++;
            }
            // ARRIS MODIFY - end: added check for okToIncClassMib
#ifdef CONFIG_TI_PACKET_PROCESSOR_STATS
            if (skb->pp_packet_info.flags & TI_HIL_PACKET_FLAG_PP_SESSION_INGRESS_RECORDED)
            {
                (skb->pp_packet_info.ti_match_qos_classifier) = cl;
                DPRINTK("set skb->pp_packet_info.ti_match_qos_classifier = %p\n", cl);
            }
#endif
        //    PAL_osProtectExit(PAL_OSPROTECT_INTERRUPT, lockKey);    ARRIS MOVE
        }
    }

    if (l2vpnRelate)
    {
        if (*sfIndex < 0 || *sfIndex >= MAX_L2VPN_SF)
            *l2vpnRelate = 0;
        else
            L2vpn_GetRelatedSf(*sfIndex, l2vpnRelate);
    }

    PAL_osProtectExit(PAL_OSPROTECT_INTERRUPT, lockKey);    // ARRIS ADD

    DPRINTK("%s - Finished, sfIndex=%d, phsIndex=%d\n",__FUNCTION__, *sfIndex, *phsIndex);
    
    return 0;
}


/**************************************************************************/
/*! \fn static int QosClass_DeletePpSessionsOfCorrelatingClassifiers(QosClassifier_t *cl1)
 **************************************************************************
 *  \brief Delets all PP sessions based on classification rules matching to the given classifier
 *  \param[in] QosClassifier_t *cl1 - classifier to base the search on 
 *  \return  0 or error code
 */
static int QosClass_DeletePpSessionsOfCorrelatingClassifiers(QosClassifier_t *cl1)
{
    Uint32 lockKey;
    struct list_head *pos;
    ClassListEntry_t *entry;
    QosClassifier_t  *cl2;
    DppCountSessionEntry_t *currSessionEntry;
    Uint32 numSessionsFound = 0;
    Uint32 *sessList;

    DPRINTK("%s - Start\n", __FUNCTION__);

    if (cl1 == NULL)
    {
        DPRINTK("%s - received classifier is NULL, cannot search PP sessions...\n", __FUNCTION__);
        return -1;
    }

    /*If no active classifiers don't search*/
    if (!numQoSClassifiers && !numUdcClassifiers)
    {
        DPRINTK("%s - No active classifiers\n", __FUNCTION__);
        return 0;
    }

    /* Allocate memory for the PP sessions list. */
	/* We need to allocate MAX_SESSIONS + 1 from the reason that MAX_SESSIONS[0] saved for numSessionsFound */
    sessList = (Uint32 *)kmalloc(sizeof(Uint32)*(MAX_SESSIONS+1), GFP_ATOMIC);
    if (sessList == NULL)
    {
        printk(KERN_ERR "%s kmalloc failed\n", __FUNCTION__);
        return -ENOMEM;
    }

    /* go over the classifier table and try to find a match */
    PAL_osProtectEntry(PAL_OSPROTECT_INTERRUPT, &lockKey);

    list_for_each(pos, &classListHead)
    {
        entry = list_entry(pos, ClassListEntry_t, list);
        cl2 = &(entry->cl);

        DPRINTK("%s Checking classifier %u of SF %u\n", __FUNCTION__, cl2->classID, cl2->sfID);

        /* Verify all relevant parameters*/
        DPRINTK("%s Compare isLlcCriterion, cl1=%d, cl2=%d\n", __FUNCTION__, cl1->isLlcCriterion, cl2->isLlcCriterion);
        if (cl1->isLlcCriterion && cl2->isLlcCriterion)
        {
            DPRINTK("%s Using LLC criterion\n",__FUNCTION__);

            if (CompareClassifiersByLlc(cl1, cl2) == CL_PARAM_NOMATCH) 
            {
                continue;
            }
            DPRINTK("Classifier matched by LLC criterion\n");
        }

        DPRINTK("%s Compare isVlanCriterion, cl1=%d, cl2=%d\n", __FUNCTION__, cl1->isVlanCriterion, cl2->isVlanCriterion);
        if  (cl1->isVlanCriterion && cl2->isVlanCriterion)
        {
            DPRINTK("%s Using VLAN criterion\n",__FUNCTION__);

            /* If VLAN header is present attempt to find a match, otherwise return no match */
            if (CompareClassifiersByVlan(cl1, cl2) == CL_PARAM_NOMATCH) 
            {
                continue;
            }
            DPRINTK("Classifier matched by 802.1P/Q criterion\n");
        }

        /* If IPv4 matching criterion flag is set if at least one IPv4 parameter is present besides TCP/UDP */ 
        DPRINTK("%s Compare isIpv4Criterion, cl1=%d, cl2=%d\n", __FUNCTION__, cl1->isIpv4Criterion, cl2->isIpv4Criterion);
        if (cl1->isIpv4Criterion && cl2->isIpv4Criterion)
        {
            DPRINTK("%s Using ipV4 criterion\n", __FUNCTION__);

            if (CompareClassifiersByIpv4(cl1, cl2) == CL_PARAM_NOMATCH)
            {
                continue;
            }
            DPRINTK("Classifier matched by IPv4 criterion\n");
        }

        DPRINTK("%s Compare isIpv6Criterion, cl1=%d, cl2=%d\n", __FUNCTION__, cl1->isIpv6Criterion, cl2->isIpv6Criterion);
        if (cl1->isIpv6Criterion && cl2->isIpv6Criterion)
        {
            DPRINTK("%s Using ipV6 criterion\n", __FUNCTION__);

            if (CompareClassifiersByIpv6(cl1, cl2) == CL_PARAM_NOMATCH)
            {
                continue;
            }

            DPRINTK("Classifier matched by IPv6 criterion\n");
        }

        DPRINTK("%s Compare isTcpUdpCriterion, cl1=%d, cl2=%d\n", __FUNCTION__, cl1->isTcpUdpCriterion, cl2->isTcpUdpCriterion);
        if ((cl1->isTcpUdpCriterion) && (cl2->isTcpUdpCriterion))
        {
            DPRINTK("%s Using TCP/UDP criterion\n", __FUNCTION__);

            if (CompareClassifiersByUdpTcp(cl1, cl2) == CL_PARAM_NOMATCH)
            {
                continue;
            }
            DPRINTK("Classifier matched by TCP/UDP criterion\n");
        }

        /* CMIM classification */
        DPRINTK("%s Compare isCmimCriterion, cl1=%d, cl2=%d\n", __FUNCTION__, cl1->isCmimCriterion, cl2->isCmimCriterion);
        if ((cl1->isCmimCriterion) && (cl2->isCmimCriterion))
        {
            DPRINTK("%s Using CMIM criterion\n", __FUNCTION__);

            if ((cl1->cmim & cl2->cmim) == 0)
            {
                DPRINTK("%s cmim mismatch (%d != %d)\n", __FUNCTION__, cl1->cmim, cl2->cmim);
                continue;
            }
            printk("Classifier matched by CMIM criterion\n");
        }

        /* Found a matching classifier, get the PP sessions list */
        currSessionEntry = cl2->ppCounters.activeSessionsList;
        while (currSessionEntry != NULL)
        {
            Uint32 i;
            /* We start from entry 1 and not 0 since entry 0 in sessList is reserved for saving numSessionsFound */
            for (i = 1; i <= numSessionsFound; i++)
            {
                if (sessList[i] == currSessionEntry->sessionHandle)
                {
                    DPRINTK("Session %d already in the TBD sessions list (at entry %d)\n", currSessionEntry->sessionHandle, i);
                    break;
                }
            }
            if (i == numSessionsFound + 1)
            {
                numSessionsFound++;
                sessList[numSessionsFound] = currSessionEntry->sessionHandle;
                DPRINTK("Adding session %d to the TBD sessions list (at entry %d)\n", currSessionEntry->sessionHandle, numSessionsFound);
            }
            
            currSessionEntry = currSessionEntry->qosClassInfo.nextSession;
        }
    }
    
    PAL_osProtectExit(PAL_OSPROTECT_INTERRUPT, lockKey);

    if (numSessionsFound)
    {
        DPRINTK("Sending HIL event to delete %d sessions in the TBD sessions list\n", numSessionsFound);

        sessList[0] = numSessionsFound;
        ti_hil_pp_event(TI_DOCSIS_SESSIONS_DEL, (void *)sessList);
    }

    /* Free the session buffer memory */
    kfree(sessList);
    
    return 0;
}


