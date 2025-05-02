/*
 *
 * dbridge_db.h
 * Description:
 * The DOCSIS bridge database header file
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

#ifndef _DBRIDGE_DB_H_
#define _DBRIDGE_DB_H_


/**************************************************************************/
/*      INCLUDES                                                          */
/**************************************************************************/

#include <linux/netdevice.h>
#include <linux/etherdevice.h>
#include "_tistdtypes.h"
#include "pal.h"
#include "autoconf.h"   //ARRIS ADD
extern int ti_selective_forward(struct sk_buff *skb);

/**************************************************************************/
/*  Various defines                                                       */
/**************************************************************************/

#define DBRIDGE_IFINDEX_CHK(__ifindex,__validentry, __format, __args...) \
{ \
    if (((__ifindex) < 0) || ((__ifindex) >= DBR_MAX_ALL_INTERFACE)) \
    { \
        printk("\n===>>> %s - %d: Currupt " #__ifindex " - %d\n" __format, __func__, __LINE__, __ifindex, ##__args); \
        __validentry = False;\
    } \
    else \
    {  \
        __validentry = True;\
    }\
}

#define DBRIDGE_IFINDEX_CHK_PR_MAC(__ifindex, __mac, __validentry, __format, __args...) \
            DBRIDGE_IFINDEX_CHK(__ifindex, __validentry, "mac %02X:%02X:%02X:%02X:%02X:%02X:: " __format, \
                                (__mac)[0], (__mac)[1], (__mac)[2], (__mac)[3], (__mac)[4], (__mac)[5], ##__args) 

#if 0
#define DBG(format, args...) printk(KERN_ERR "%s - %d: " format "\n", __func__, __LINE__, ##args)
#else
#define DBG(...)
#endif

#ifdef DSG
#define DBR_NUM_DSG_DEVICES         1       /* lbr1 */
/* ARRIS CHANGE - increment  DBR_NUM_DSG_ESAFES to 3 */
#define DBR_NUM_DSG_ESAFES          3      /* ip, tunnel, cable card  */
#else
#define DBR_NUM_DSG_DEVICES         0
#define DBR_NUM_DSG_ESAFES          0
#endif

#define DBR_MAX_ESAFE_INTERFACES    (2 + DBR_NUM_DSG_ESAFES)    /* eMTA, eRouter, DSG */
#define DBR_MAX_INTERFACES          (4 + DBR_MAX_ESAFE_INTERFACES + DBR_NUM_DSG_DEVICES)  /* cable IN+ cmci IN+ lan IP +cable IP + eSAFES + DSG */
#define DBR_MAX_CPE_ENTRY 64
#define DBR_MAX_ALT_SIZE DBR_MAX_CPE_ENTRY + DBR_MAX_ESAFE_INTERFACES + 2 /* CPEs + lan IP +cable IP + eSAFES */
#define DBR_MAX_ALL_INTERFACE       (TI_MAX_DEVICE_INDEX)  /* max interface in the system, Docsis brudge, Local Bridge, esafe, etc... */ 
#if defined(MAC_ADDRESS_LEN) 
    #if (MAC_ADDRESS_LEN != 6)
        #error MAC_ADDRESS_LEN redefined with value != 6
    #endif
#else
    #define MAC_ADDRESS_LEN 6
#endif
#define NETLINK_TI_DOCSIS 17

#define DBR_OFF_MODE				0x0	 
#define DBR_REGISTERED_MODE	        0x1 
#define DBR_PRE_REGISTRATION_MODE	0x2 
#define DBR_NACO_OFF_MODE			0x4 
#define DBR_STANDBY_MODE			0x8 

#define DBR_CMCI_NET_DEV_TYPE		0x1  /* 0000 0001 */
#define DBR_CABLE_NET_DEV_TYPE	    0x2  /* 0000 0010 */
#define DBR_ESAFE_NET_DEV_TYPE      0x4  /* 0000 0100 */
#define DBR_WAN_IP_NET_DEV_TYPE  	0x8  /* 0000 1000 */
#define DBR_LAN_IP_NET_DEV_TYPE  	0x10 /* 0001 0000 */
#ifdef DSG
#define DBR_DSGI_NET_DEV_TYPE        0x20  /* 0010 0000 */
#endif

#define DBR_NO_FILTER_CLASS_FLAG    0x0
#define DBR_FILTER_FLAG             0x01
#define DBR_CLASSIFIER_FLAG         0x02

#ifdef CONFIG_VENDOR_ARRIS_GATEWAY  // ARRIS ADD
#define GW_INTERFACE_SSID_0         0x8880
#define GW_INTERFACE_SSID_1         0x8881
#define GW_INTERFACE_SSID_2         0x8882
#define GW_INTERFACE_SSID_3         0x8883
#define GW_INTERFACE_SSID_4         0x8884
#define GW_INTERFACE_SSID_5         0x8885
#define GW_INTERFACE_SSID_6         0x8886
#define GW_INTERFACE_SSID_7         0x8887
#define GW_INTERFACE_LAN_ETH        0x8888
#define GW_INTERFACE_ALL            0x9000
#endif  // ARRIS ADD END

#define DBRALT_OCCUPIED_HEAD dbridge_db.alt.altOccupied
#define DBRALT_FREE_HEAD dbridge_db.alt.altFree

/*! \var define DBRALT_PROT_DCL(_sav)
    \brief Declare a variable to hold context for multitasking protection
    \param[in] _sav: Name of context variable
*/
#define DBRALT_PROT_DCL(_sav) Uint32 _sav

/*! \var define DBRALT_PROT_ON(_sav)
    \brief Start DBR ALT multitasking protection
    \param[out] _sav: Place to save context to restore, must be declared using DBRALT_PROT_SAV
*/
#define DBRALT_PROT_ON(_sav) \
	PAL_osProtectEntry(PAL_OSPROTECT_INTERRUPT, &(_sav))

/*! \var define DBRALT_PROT_OFF(_sav)
    \brief End DBR ALT multitasking protection
    \param[in] _sav: Same variable used by the previous DBRALT_PROT_ON
*/
#define DBRALT_PROT_OFF(_sav) \
    PAL_osProtectExit(PAL_OSPROTECT_INTERRUPT, _sav)

/*! \var define DBRALT_IS_ALT_EMPTY
    \brief Is the ALT empty
*/
#define DBRALT_IS_ALT_EMPTY list_empty(DBRALT_OCCUPIED_HEAD)

/*! \var define DBRALT_FOREACH_ALT_ENTRY
    \brief Loop over all ALT records
    \param[out] _alt: a variable of type DbridgeAltEntry_t
*/
#define DBRALT_FOREACH_ALT_ENTRY(_alt) \
    list_for_each_entry((_alt), &DBRALT_OCCUPIED_HEAD, listAltEntry)

/*! \var define DBRALT_FOREACH_ALT_ENTRY
    \brief Loop over all ALT records that meet a condition
    \param[out] _alt: a variable of type DbridgeAltEntry_t
    \param[in] _cond: the condition to check 
*/
#define DBRALT_FOREACH_ALT_ENTRY_COND(_alt, _cond) \
    DBRALT_FOREACH_ALT_ENTRY(_alt) \
        if (!(_cond)) \
        { \
            continue; \
        } \
        else 
#ifdef DSG
typedef enum TunnelAction_Type_e
{
    ADD_TUNNEL,
    DEL_TUNNEL,
} TunnelAction_Type_e;
#endif


/**************************************************************************/
/*      INTERFACE  Defines and Structs                                    */
/**************************************************************************/

/*! \var typedef enum Cpe_learning_e
    \brief enum defines the ...
*/
typedef enum Maclearning_e
{
    NOT_LEARN,
	DBRIDGE_INIT,
    CPE_STATIC,
    CPE_DYNAMIC,
#ifdef DSG
    CPE_ESAFE,
#endif
} Maclearning_e;

typedef enum eRouter_mode_e
{
    EROUTER_DISABLE,
    EROUTER_ENABLE,
} eRouter_mode_e;

/*! \var DbridgeMacAddr_t
    \brief Structure hold mac address.
*/
typedef struct DbridgeMacAddr {
    Uint8 mac[MAC_ADDRESS_LEN];
}DbridgeMacAddr_t;

/*! \var DbridgeMacList_t
    \brief Structure defines mac address list.
*/
typedef struct DbridgeMacList {
    DbridgeMacAddr_t macList[DBR_MAX_CPE_ENTRY];
    Uint32 index;
    Uint32 ifIndex;
}DbridgeMacList_t;

/*! \var ti_docsis_nlmsg
    \brief Structure defines the format of netlink socket msg.
*/
struct ti_docsis_nlmsg {
    unsigned int type;
    union
    {
        unsigned long addr;
        DbridgeMacList_t macData;
    }data;
};

/*! \var typedef struct DbridgeDevice DbridgeDevice_t
    \brief Structure defines the format of the network devices attached to the DOCSIS Bridge.
*/

typedef struct DbridgeDevice
{
    /* Linux specific device information. */
	struct net_device*  ptr_interface; 

    /* The function is the registered function that is called to "push"
     * the packet onto the corresponding  interface. */ 
   int     (*push)(struct sk_buff* skb, unsigned int fc_flags);
   /* docsis interface type  - CPE eSAFE CABLE WAN IP or LAN IP */
   int net_dev_type;
}DbridgeDevice_t;

#ifdef DSG
#define DSG_FILTER_TAB_LEN                 64
#define DBR_DSGLOOB_DOCSIS_STANDARD  0
#define DBR_DSGLOOB_DSG_ENABLE       1
#define DBR_DSGLOOB_LOOB_ENABLE      2
typedef struct{
    unsigned char  MacAddress[MAC_ADDRESS_LEN];
}DbridegDsgTunnel;
typedef struct{
    /* dsg working status */
    unsigned char            mode;
    unsigned int            tunlNum;
    DbridegDsgTunnel   dbridegDsgTunnel[DSG_FILTER_TAB_LEN];
    unsigned long      dbridegEstbIP;
	int                dbridgeDsgDeviceIndex;
}DbridegDsgDevice;
#endif
/*! \var typedef struct DbridgeAltEntry DbridgeAltEntry_t
    \brief Structure defines the format of the Address Lookup table entry.
*/
typedef struct DbridgeAltEntry 
{
    /* DB Management */
    struct list_head        listAltEntry;

    /* MAC Address */
    unsigned char           macAddress[MAC_ADDRESS_LEN];  
    
	/* the cpe number in term of insert order to the table */
	/* we need this information for IGMP support */
	unsigned char           cpe_number;
	/* cpe learning from - static, dynamic or not CPE  */
	Maclearning_e macLearningFrom;

    /* DOCSIS Bridge Device on which the MAC Address is present. */
    DbridgeDevice_t*   dbridgeDevice;
	/* the first interface where the packet received from (for example USB or Ethernet)*/
	struct net_device*  sourceInterface;
    // ARRIS ADD
    /* the HW filter index */
    unsigned int hwFilterIndex;
#ifdef CONFIG_VENDOR_ARRIS_GATEWAY
    int gwInterfaceInfo;
#endif    
    // ARRIS END
}DbridgeAltEntry_t;

/*! \var typedef struct DbridgeAltDb_t
    \brief ALT DB
*/
typedef struct
{
    /* DB Management */
    struct list_head        altFree;
    struct list_head        altOccupied;

    /* Address Lookup table; which contains all the MAC Address learnt */
    DbridgeAltEntry_t       altTable[DBR_MAX_ALT_SIZE];
} DbridgeAltDb_t;


/*! \var typedef struct DbridgeDataBase DbridgeDataBase_t
    \brief Structure contains information about the DOCSIS bridge.
*/
typedef struct DbridgeDataBase
{
    /* List of all DOCSIS Bridge device that is attached to the DOCSIS Bridge. */
    DbridgeDevice_t   DbridgeDevices[DBR_MAX_INTERFACES];

    /* ALT */
    DbridgeAltDb_t          alt;
    /* docsis bride working mode */
    unsigned int            mode;
    eRouter_mode_e          eroter_mode;
    /*netlink socket params*/
    struct sock *nlsk;
    int nl_pid; 
#ifdef DSG
    DbridegDsgDevice      dbridgeDsg;
#endif  
    /* ARRIS ADD START */
#ifdef CONFIG_VENDOR_ARRIS_GATEWAY
    unsigned int          erouter_selective_fwd;
#endif
    /* ARRIS ADD END */
}DbridgeDataBase_t;

typedef struct DbridgeDevCounter
{
    Uint64   inUcast;
    Uint64   inMcast;
    Uint64   inBcast;
    Uint64   outUcast;
    Uint64   outMcast;
    Uint64   outBcast;
    unsigned int            In;
    unsigned int            Out;
    unsigned int            Discard;
    unsigned int            UnSupport;
    unsigned int            FilterDrop;
    unsigned int            FwrdRuleDrop;
    
}DbridgeDevCounter_t;

typedef struct DbridgeCounter
{
	unsigned int            StaticCpe;
	unsigned int            DynamicCpe;
    DbridgeDevCounter_t     DevCounters[DBR_MAX_ALL_INTERFACE];
}DbridgeCounters_t;

/*! \var typedef enum DbridgeDb_FieldMask_e
    \brief Mask, denoting which fields to use for selection of ALT entries to delete
            BITMAP
            Try to keep ALL and ANY at the end (just for debug purposes)
            Either ALL or ANY is MANDATORY
*/
typedef enum
{
    DBRALT_MASK_MAC             = 0x00000001,
    DBRALT_MASK_LEARNFROM       = 0x00000002,
    DBRALT_MASK_DESTDEV         = 0x00000004,
    DBRALT_MASK_SRCDEV          = 0x00000008,
    DBRALT_MASK_CPENUM          = 0x00000010,
    DBRALT_MASK_DEVTYPE         = 0x00000020,

    DBRALT_MASK_ALL             = 0x40000000,
    DBRALT_MASK_ANY             = 0x80000000
} DbridgeDb_FieldMask_e;

extern DbridgeDataBase_t dbridge_db;


/**************************************************************************/
/*      Default Values                                          */
/**************************************************************************/



/**************************************************************************/
/*      INTERFACE FUNCTIONS Prototypes:                                   */
/**************************************************************************/

/*! \fn STATUS DbridgeDb_Init(void)
 *  \brief DOCSIS bridge data base initialization.
 *  \param[in] no input.
 *  \param[out] no output.
 *  \return OK or error status.
 */
int DbridgeDb_Init(void);

/*! \fn int DbridgeDb_ReInit(void)                                     
 *  \brief DOCSIS bridge data base re-initialization.
 *  \param[in] no input.
 *  \param[out] no output.
 *  \return OK or error status.
 */
int DbridgeDb_ReInit(void);

/*! \fn int DbridgeDB_SetMaxCpe(unsigned  int mac_cpe_config)                                     
 *  \brief set maximum  supported CPE.
 *  \param[in] Max allow CPEs .
 *  \param[out] no output.
 *  \return OK or error status.
 */
int DbridgeDB_SetMaxCpe(unsigned  int mac_cpe);

/*! \fn int DbridgeDB_GetMaxCpe()                                     
 *  \brief set maximum  supported CPEs.
 *  \param[in] None .
 *  \param[out] None.
 * return Max CPEs.
 */
int DbridgeDB_GetMaxCpe(void);
// ARRIS Add Start
int DbridgeDb_SetCmtsMacAddr(unsigned char* mac_address);
int DbridgeDb_GetCmtsMacAddr(unsigned char* mac_address);
#ifdef CONFIG_MACH_PUMA6
int DbridgeDb_SetAtomMacAddr(unsigned char* mac_address);
int DbridgeDb_GetAtomMacAddr(unsigned char* mac_address);
#endif
// ARRIS Add end


// ARRIS Add Start
#ifdef INCLUDE_SERCOMM_GW
/**************************************************************************/
/*! \fn unsigned int DbridgeDb_Getlbr0InterfaceMask(void)                                     
 **************************************************************************
 *  \brief Return the lbr0 interface mask
 *  \param[in] no input.
 *  \param[out] no output.
 *  \return Interface mask.
 **************************************************************************/
unsigned int DbridgeDb_Getlbr0InterfaceMask(void);
#endif

/*! \fn int DbridgeDB_SetL2vpnPromMode(unsigned  int l2vpnPromMode)
 *  \brief set L2VPN Promiscuous Mode
 *  \param[in] L2VPN Promiscuous Mode setting.
 *  \param[out] no output.
 *  \return OK or error status.
 */
int DbridgeDB_SetL2vpnPromMode(unsigned  int l2vpnPromMode);

/*! \fn unsigned int DbridgeDB_GetL2vpnPromMode()
 *  \brief Get L2VPN Promiscuous Mode
 *  \param[in] None.
 *  \param[out] None.
 * return L2VPN Promiscuous Mode setting.
 */
unsigned int DbridgeDB_GetL2vpnPromMode(void);
// ARRIS Add End
 
/*! \fn int DbridgeDb_AltAddMac(unsigned char* mac_address, Maclearning_e mac_learn_from,struct net_device *ptr_netdev, struct net_device*  sourceInterface)
 *  \brief This function Add MAC address to ALT table or update device information for
 *  \brif existing cpe MAC that add from config file .
 *  \param[in] cpe mac address.
 *  \param[in] mac address from - NOT_CPE, CPE_STATIC or CPE_DYNAMIC,
 *  \param[in] pointer to docsis device table.
 *  \param[in] pointer to first interface where the packet received from.
 *  \param[out] no output.
 *  \return OK or error status.
 */
#ifdef CONFIG_VENDOR_ARRIS_GATEWAY  // ARRIS ADD BEGIN            
int DbridgeDb_AltAddMac(unsigned char* mac_address, Maclearning_e mac_learn_from,DbridgeDevice_t *dbridge_Devices,
                           struct net_device*  sourceInterface, int gwInterfaceInfo);           
#else
int DbridgeDb_AltAddMac(unsigned char* mac_address, Maclearning_e mac_learn_from,DbridgeDevice_t *dbridge_Devices,struct net_device*  sourceInterface);
#endif  // ARRIS ADD END             

/**************************************************************************/
/*! \fn int DbridgeDb_AltDelMac(unsigned char* mac_address, 
            Maclearning_e mac_learn_from, char *dest_dev_name, char *src_dev_name, 
            unsigned char cpe_number, int net_dev_type,, DbridgeDb_FieldMask_e fieldMask)
 **************************************************************************
 *  \brief DEL MAC address from ALT table.
 *          All ALT entries are compared against the input params selected by the mask.
 *          Depending on DBRALT_MASK_ALL/DBRALT_MASK_ANY, entries that match all/any input
 *          are deleted.
 *  \param[in] cpe mac address.
 *  \param[in] mac address from - DBRIDGE_INIT, CPE_STATIC or CPE_DYNAMIC,
 *  \param[in] dest_dev_name - name of destination device
 *  \param[in] src_dev_name - name of source device
 *  \param[in] cpe_number
 *  \param[in] net_dev_type - device type, e.g. DBR_ESAFE_NET_DEV_TYPE, DBR_CMCI_NET_DEV_TYPE
 *  \param[in] field_mask - Which fields are used for deciding which CPEs to del
 *  \param[out] no output.
 *  \return OK or error status.
 **************************************************************************/
int DbridgeDb_AltDelMac(unsigned char* mac_address,
                        Maclearning_e mac_learn_from, 
                        char *dest_dev_name,
                        char *src_dev_name,
                        unsigned char cpe_number,
                        int net_dev_type,
                        DbridgeDb_FieldMask_e field_mask,
                        DbridgeMacList_t *deletedMacList);

/*! \fn int DbridgeDb_AltAddMacByDevName(unsigned char* mac_address, unsigned char* name)
 *  \brief this function Add MAC address to ALT table for cm interface (wan0 or lan0)
 *  \param[in] mac address.
 *  \param[in] device name.
 *  \param[out] no output.
 *  \return OK or error status.
 */
int DbridgeDb_AltAddMacByDevName(unsigned char* mac_address, unsigned char* name);

/*! \fn DDbridgeAltEntry_t* DbridgeDb_AltGetMemberByMac(unsigned char* mac_address)
 *  \brief The function retreives the dbridge ALT table member for the  
 *  \brief corresponding mac address.
 *  \param[in] ALT table index.
 *  \return Success-pointer to the Dbridge ALT table, NULL-If no entry is found.
 */
DbridgeAltEntry_t*  DbridgeDb_AltGetMemberByMac(unsigned char* mac_address);

/*! \fn int DbridgeDb_DevAdd(struct net_device* ptr_net_dev,int (*push)(struct sk_buff* skb))
 *  \brief Add member to device table.
 *  \param[in] pointer to net device.
 *  \param[in] The PUSH Function to send the packet to Interface.
 *  \param[in] docsis interface type  - CPE eSAFE CABLE WAN IP or LAN IP
 *  \return 0=OK or error status.
 */
int DbridgeDb_DevAdd(struct net_device* ptr_net_dev,int (*push)(struct sk_buff* skb, unsigned int fc_flags), int net_dev_type);

/*! \fn DbridgeDevice_t* DbridgeDb_DevGetByInterface (struct net_device* ptr_interface)
 *  \brief The function retreives the dbridge device table member for the corresponding 
 *  \brief network interface.
 *  \param[in] pointer to net net device.
 *  \return Success-pointer to the Dbridge device table, NULL-If no entry is found.
 */
DbridgeDevice_t* DbridgeDb_DevGetByInterface (struct net_device* ptr_interface);

/*! \fn DbridgeDevice_t* DbridgeDb_DevGetByIndex (struct net_device* ptr_interface)
 *  \brief The function retreives the dbridge device table member for the corresponding 
 *  \brief index.
 *  \param[in] index.
 *  \return Success-pointer to the Dbridge device table, NULL-If no entry is found.
 */
DbridgeDevice_t* DbridgeDb_DevGetByIndex(int index);

/*! \fn DbridgeDevice_t* DbridgeDb_DevGetByType(int net_dev_type)
 *  \brief The function retreives the dbridge device table member for the corresponding 
 *  \brief type.
 *  \param[in] net dev  - CMCI, ESAFE CABLE WAN_IP or LAN_IP.
 *  \return Success-pointer to the Dbridge device table, NULL-If no entry is found.
 */
DbridgeDevice_t* DbridgeDb_DevGetByType(int net_dev_type);
#if 1
/*! \fn DDbridgeAltEntry_t*  DbridgeDb_AltGetMemberByIndex(int index);
 *  \brief The function retreives the dbridge ALT table member for the  
 *  \brief corresponding mac address.
 *  \param[in] cpe mac address.
 *  \return Success-pointer to the Dbridge ALT table, NULL-If no entry is found.
 */
DbridgeAltEntry_t*  DbridgeDb_AltGetMemberByIndex(int index);

/*! \fn int DbridgeDb_AltGetInfoByIndex(int index, unsigned char* mac_address, Maclearning_e *mac_learn_from, DbridgeDevice_t *dbridge_Devices)
 *  \brief Get information from ALT table.
 *  \param[in] ALT table index.
 *  \param[out] mac address ,
 *  \param[out] mac address learn from - NOT_CPE, CPE_STATIC or CPE_DYNAMIC,
 *  \param[out] pointer to DbridgeDevice.
 *  \return 0 or error status.
 */
int DbridgeDb_AltGetInfoByIndex(int index, unsigned char* mac_address, Maclearning_e *mac_learn_from, DbridgeDevice_t *dbridge_Devices);

/*! \fn int DbridgeDb_AltGetNumMember(void)
 *  \param[in] None.
 *  \param[out] None.
 *  \return Number of member in ALT table.
 */
int DbridgeDb_AltGetNumMember(void);
#endif
/*! \fn int DbridgeDb_GetNumOfDevice (void)
 *  \brief The function retreives the number of device connect to docsis bridge
 *  \param[in] none.
 *  \return number of device connect to docsis bridge.
 */
#ifdef DSG
DbridgeDevice_t* DbridgeDb_DevGetByName(const char* net_dev_name);
#endif
int DbridgeDb_GetNumOfDevice(void);

/*! \fn void DbridgeDb_SetMode (int mode)
 *  \brief set docsis bridge mode
 *  \param[in] docsis bridge mode - off, registered pre_registration or naco_off 
 *  \return none.
 */
void DbridgeDb_SetMode(int mode);

/*! \fn iint DbridgeDb_GetMode (void)
 *  \brief get docsis bridge mode
 *  \param[in] none
 *  \return docsis bridge mode - off, registered pre_registration or naco_off.
 */
int DbridgeDb_GetMode(void);

/*! \fn iDbridgeCounter_t* Dbridge_GetCounters(void)
 *  \brief get docsis bridge counters
 *  \param[in] none
 *  \return pointer to the Dbridge counter structure.
 */
DbridgeCounters_t* Dbridge_GetCounters(void);

/*! \fn unsigned long long DbridgeDB_GetlbridgeInterfaceMask(void)                                     
 *  \brief Get lbridge Interface Mask.
 *  \param[in] None .
 *  \param[out] None.
 *  \return lbridge Interface Masks.
 */
unsigned long long DbridgeDB_GetlbridgeInterfaceMask(void);

/*! \fn void DbridgeDB_SetlbridgeInterfaceMask(int ifindex)                                     
 *  \brief set interfacelbridge Interface Mask.
 *  \param[in] interface index .
 *  \param[out] None.
 *  \return none.
 */
void DbridgeDB_SetlbridgeInterfaceMask(int ifindex);

/*! \fn int DbridgeDB_GetCpeInterfaceMask(void)                                     
 *  \brief Get CPE Interface Mask.
 *  \param[in] None .
 *  \param[out] None.
 *  \return CPE Interface Mask.
 */
unsigned long long DbridgeDB_GetCpeInterfaceMask(void);

/*! \fn void DbridgeDb_SetMode (int mode)
 *  \brief set docsis bridge erouter mode
 *  \param[in] docsis bridge mode - enable/disable 
 *  \return none.
 */
void DbridgeDb_Erouter_SetMode(int mode);

/*! \fn iint DbridgeDb_Erouter_GetMode (void)
 *  \brief get docsis bridge erouter mode
 *  \param[in] none
 *  \return docsis bridge erouter mode enable/disable.
 */
int DbridgeDb_Erouter_GetMode(void);

/*! \fn void DbridgeDb_SetNlSocket(struct sock *socket)
 *  \brief set docsis bridge netlink socket
 *  \param[in] socket - netlink socket 
 *  \return none.
 */
void DbridgeDb_SetNlSocket(struct sock *socket);

/*! \fn struct sock* DbridgeDb_GetNlSocket(void);
 *  \brief get docsis bridge netlink socket
 *  \param[in] none
 *  \return docsis bridge netlink socket.
 */
struct sock* DbridgeDb_GetNlSocket(void);

/*! \fn void DbridgeDb_SetNlPid(int pid)
 *  \brief set docsis netlink pid
 *  \param[in] pid - netlink pid 
 *  \return none.
 */
void DbridgeDb_SetNlPid(int pid);

/*! \fn int DbridgeDb_GetNlPid (void)
 *  \brief get docsis bridge netlink pid
 *  \param[in] none
 *  \return docsis bridge netlink pid.
 */
int DbridgeDb_GetNlPid(void);

#ifdef DSG
unsigned char DbridgeDB_GetDsgMode(void);
void DbridgeDB_SetDsgMode( unsigned char mode);
int DbridgeDB_IsDsgEnabled(void);
int DbridgeDB_IsLoobEnabled(void);
int DbridgeDB_IsDsgLoobEnabled(void);
int DbridgeDB_GetDsgTunnelNums(void);
int DbridgeDB_AddDsgTunnel(char *addr);
int DbridgeDB_DelDsgTunnel(char *addr);
int DbridgeDB_IsDsgMulticastAddress(char *addr);
int DbridgeDB_GetDsgTunnelMacAddress(int index, unsigned char* mac_address);
int DbridgeDB_SetDsgEstbIPAddress(unsigned long ip);
int DbridgeDB_GetDsgEstbIPAddress(unsigned long *ip);
int DbridgeDB_IsDsgEstbIPAddress(unsigned long ip);
int DbridgeDB_GetDsgDeviceIndex(int* device_name);
int DbridgeDB_SetDsgDeviceIndex(int* device_name);
#endif

/* ARRIS ADD START */
/*! \fn void DbridgeDB_SetMaxCpeBypass(unsigned int)
 *  \brief Set the max cpe bypass value.  This allows embedded devices
 *         such as the MTA to be added to the CPE table without having to 
 *         adjust the MaxCpeAllowed TLV to account for them.
 *  \param[in] value - new CpeBypass value
 *  \param[out] None.
 *  \return None
 */
void DbridgeDB_SetMaxCpeBypass(unsigned int value);

/*! \fn unsigned int DbridgeDB_GetMaxCpeBypass(void)                                     
 *  \brief Get CPE bypass value.
 *  \param[in] None .
 *  \param[out] None.
 *  \return CPE bypass.
 */
unsigned int DbridgeDB_GetMaxCpeBypass(void);

/*! \fn void DbridgeDB_getHwFilters(void)
 *  \brief Debug hook for retreiving the UnusedHwFilters array.
 *  \param[out] filterarray - pointer to the array.
 *  \param[out] num_elements - number of elements in the array.
 *  \return None
 */
void DbridgeDB_getHwFilters(unsigned int **filterarray, unsigned int *num_elements);
#ifdef CONFIG_VENDOR_ARRIS_GATEWAY // ARRIS ADD BEGIN
void DbridgeDb_AltDelCpeByInterface(int interfaceNumber);
void DbridgeDb_SetErouterSelectiveFwd(int enabled);
unsigned int DbridgeDb_GetErouterSelectiveFwd(void);
#endif
/* ARRIS ADD END */
#endif

