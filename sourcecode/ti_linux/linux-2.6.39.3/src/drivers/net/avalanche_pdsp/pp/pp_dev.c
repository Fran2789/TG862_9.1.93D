/*
  GPL LICENSE SUMMARY

  Copyright(c) 2014 Intel Corporation.

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
 
#include <linux/proc_fs.h>
#include <linux/inet_lro.h>
#include "puma6_cppi.h"
#include <asm-arm/arch-avalanche/puma6/puma6_pp.h>


#include <linux/kernel.h>
#include <linux/module.h>
#include <linux/fs.h>       /* everything... */
#include <linux/types.h>    /* size_t */
#include <linux/uaccess.h>
#include <linux/cdev.h>     /* cdev utilities */
#include <linux/device.h>
#include <linux/netdevice.h>
#include <linux/inetdevice.h>
#include <linux/semaphore.h>
#include <linux/slab.h>
#include <linux/ti_hil.h>
#include <linux/in6.h>
#include <mach/puma.h>
#include <avalanche_pdsp_api.h>
#include <avalanche_pp_api.h>
#include "pp_db.h"
#include "pp_hal.h"



MODULE_AUTHOR("Intel Corporation");
MODULE_LICENSE("GPL");
MODULE_DESCRIPTION("Packet Processor Driver");

#define DEVICE_MAJOR                    101
#define DEVICE_NAME                     "pp"
#define BUFFER_FOR_PRINTF_SIZE          3000											/* global buff size*/


#define BUFF_FREE(len)                  ( BUFFER_FOR_PRINTF_SIZE - (len))		/* BUFF_FREE returns the number of free bytes in global buffer */
#define FOR_ALL_PIDS(i)                 for (; i < AVALANCHE_PP_MAX_PID; i++)
#define LIST_FOR_EACH(pos, head)        for (; prefetch(pos->next), (pos) != (head); (pos) = (pos->next) )
#define WR_TO_PAGE                      if (len < count) { memcpy(page + curr_off, buff, len); curr_off += len; count -= len ; } else { *start = page; *eof = 1; return curr_off; }		/* write to page*/
#define CHECK_SNPRINTF(val, len)        if (val > 0) (len) += (val) 				    /* check if snprintf seccess*/


Uint32 g_proc_xsession_handle = AVALANCHE_PP_MAX_ACCELERATED_SESSIONS;
static Uint8  g_proc_session_cmd[100];
static Uint8* g_proc_session_argv[10];
Uint32 g_proc_queue_id = AVALANCHE_PP_QOS_QUEUE_MAX_INDX;
Uint32 g_proc_cluster_id = AVALANCHE_PP_QOS_CLST_MAX_INDX;
Uint32 g_proc_vpid_id = AVALANCHE_PP_MAX_VPID;

/*global buffer for all procs*/
static char buff[BUFFER_FOR_PRINTF_SIZE];

/*global array for sessions handles*/
static Uint32 tmp2k[AVALANCHE_PP_MAX_ACCELERATED_SESSIONS]; 

static long __ppIoctl ( struct file * filp , unsigned int cmd , unsigned long arg )
{
    switch (cmd)
    {
    case PP_DRIVER_FLUSH_ALL_SESSIONS:
        {
            avalanche_pp_flush_sessions( AVALANCHE_PP_MAX_VPID, PP_LIST_ID_ALL );
        }
        break;

    case PP_DRIVER_KERNEL_POST_INIT:
        {
            avalanche_pp_kernel_post_init();
        }
        break;

    case PP_DRIVER_PSM:
        {
            avalanche_pp_psm_ioctl_param_t     usr_param;
            if (copy_from_user(&usr_param, (void __user *)arg, sizeof(usr_param)))
            {
                printk(KERN_ERR"\n%s: failed to copy from user\n", __FUNCTION__);
                return -EFAULT;
            }
            avalanche_pp_psm(usr_param);
        }
        break;
    case    PP_DRIVER_SET_QOS_CLST_MAX_CREDIT:
        {
            avalanche_pp_Qos_ioctl_params_t Qos_param;
            if (copy_from_user(&Qos_param, (void __user *)arg, sizeof(Qos_param)))
            {
                printk(KERN_ERR"\n%s: failed to copy from user\n", __FUNCTION__);
                return -EFAULT;
            }
            avalanche_pp_qos_set_cluster_max_global_credit(Qos_param.bytePkts, Qos_param.index, Qos_param.newValue);
        }
        break;

    case PP_DRIVER_SET_QOS_QUEUE_MAX_CREDIT:
        {
            avalanche_pp_Qos_ioctl_params_t Qos_param;
            if (copy_from_user(&Qos_param, (void __user *)arg, sizeof(Qos_param)))
            {
                printk(KERN_ERR"\n%s: failed to copy from user\n", __FUNCTION__);
                return -EFAULT;
            }
            avalanche_pp_qos_set_queue_max_credit(Qos_param.bytePkts, Qos_param.index, Qos_param.newValue);
        }
        break;

    case PP_DRIVER_SET_QOS_QUEUE_ITERATION_CREDIT:
        {
            avalanche_pp_Qos_ioctl_params_t Qos_param;
            if (copy_from_user(&Qos_param, (void __user *)arg, sizeof(Qos_param)))
            {
                printk(KERN_ERR"\n%s: failed to copy from user\n", __FUNCTION__);
                return -EFAULT;
            }
            avalanche_pp_qos_set_queue_iteration_credit(Qos_param.bytePkts, Qos_param.index, Qos_param.newValue);
        }
        break;
        
    case PP_DRIVER_ADD_VPID:
        {
            avalanche_pp_dev_ioctl_param_t  usr_dev;
            struct net_device *dev;

            if (copy_from_user(&usr_dev, (void __user *)arg, sizeof(usr_dev)))
            {
                printk(KERN_ERR"\n%s: failed to copy from user\n", __FUNCTION__);
                return -EFAULT;
            }

            dev = dev_get_by_name(&init_net, usr_dev.device_name);

            if (dev)
            {
                if (usr_dev.qos_virtual_scheme_idx == AVALANCHE_PP_NETDEV_PP_QOS_PROFILE_DEFAULT)
                {
                    dev->qos_virtual_scheme_idx = NETDEV_PP_QOS_PROFILE_DEFAULT;
                }
                else
                {
                    dev->qos_virtual_scheme_idx = usr_dev.qos_virtual_scheme_idx; 
                }
                ti_hil_pp_event(TI_PP_ADD_VPID, (void *)dev);
                dev_put(dev);
            }
        }
        break; 

    case PP_DRIVER_DELETE_VPID:
        {
            avalanche_pp_dev_ioctl_param_t  usr_dev;
            struct net_device * dev;

            if (copy_from_user(&usr_dev, (void __user *)arg, sizeof(usr_dev)))
            {
                printk(KERN_ERR"\n%s: failed to copy from user\n", __FUNCTION__);
                return -EFAULT;
            }

            dev = dev_get_by_name (&init_net, usr_dev.device_name);

            if(dev)
            {
                ti_hil_pp_event (TI_PP_REMOVE_VPID, (void *)dev);
                dev_put(dev);
            }
        }
        break;

    case PP_DRIVER_SET_DS_LITE_US_FRAG_IPV4:
        {
            avalanche_pp_dslite_ioctl_param_t  val;
            if (copy_from_user(&val, (void __user *)arg, sizeof(val)))
            {
                printk(KERN_ERR"\n%s: failed to copy from user\n", __FUNCTION__);
                return -EFAULT;
            }

            gPpHalDsLiteUsFragIPv4 = val;
            
        }
        break;

    case PP_DRIVER_SET_MTA_ADDR:
        {
            avalanche_pp_mtaMacAddr_ioctl_param_t  mtaMacAddr;
            
            if (copy_from_user(&mtaMacAddr, (void __user *)arg, sizeof(mtaMacAddr)))
            {
                printk(KERN_ERR"\n%s: failed to copy from user\n", __FUNCTION__);
                return -EFAULT;
            }
            
            avalanche_pp_set_mta_mac_address(mtaMacAddr);
        }
        break;
        
    case PP_DRIVER_SET_ACK_SUPP:
        {
            avalanche_pp_ackSupp_ioctl_param_t  usr_param;            
            if (copy_from_user(&usr_param, (void __user *)arg, sizeof(usr_param)))
            {
                printk(KERN_ERR"\n%s: failed to copy from user\n", __FUNCTION__);
                return -EFAULT;
            }
            avalanche_pp_set_ack_suppression(usr_param);
        }
        break;
         
    default:
        break;
    }

    return 0;
}

static struct file_operations   ppCdevFops =
{
    .owner              = THIS_MODULE,
    .unlocked_ioctl     = __ppIoctl,
//    .write              = __pdspDownload,
};

/*LUT2 proc func for display SESSIONS info*/
int LUT2_display_session    (Int32 session_handle, char *buffer, int size);
int LUT2_display_l2_ingress(AVALANCHE_PP_INGRESS_SESSION_PROPERTY_t* ingress_property, char *buffer, int size);
int LUT2_display_l2_egress(AVALANCHE_PP_EGRESS_SESSION_PROPERTY_t* egress_property, char *buffer, int size);
int LUT2_display_ipv4_ingress(AVALANCHE_PP_INGRESS_SESSION_PROPERTY_t* ingress_property, char *buffer, int size);
int LUT2_display_ipv4_egress(AVALANCHE_PP_EGRESS_SESSION_PROPERTY_t* egress_property, char *buffer, int size);
int LUT2_display_ipv6_ingress(AVALANCHE_PP_INGRESS_SESSION_PROPERTY_t* ingress_property, char *buffer, int size);
int LUT2_display_ipv6_egress(AVALANCHE_PP_EGRESS_SESSION_PROPERTY_t* egress_property, char *buffer, int size);
const Int8* LUT2_display_ipv6_addr(const void *cp, Int8 *buf, size_t len);


/**************************************************************************************/
/*! \fn static int PID_BUSY_read_proc (char *page, char **start, 
*                  off_t off, int count,int *eof, void *data)
 **************************************************************************************
 *  \brief This function reads all busy PIDs
 *  \return return the number of bytes wirtten to the buffer.
 **************************************************************************************/
static int PID_BUSY_read_proc (char *page, char **start, off_t off, int count, int *eof, void *data)
{ 
    int             len = 0;
    static int      i;
    int             curr_off = 0;
    static Uint8    endOfData = 0;
    int             temp;
    int             prntRtrn;
    static Uint8    num_entries;
    static AVALANCHE_PP_PID_t *pidList[AVALANCHE_PP_MAX_PID];
    
    if (endOfData == 1)
    {
        endOfData = 0;
        return 0;
    }

    if (off == 0) 
    {
        i = 0;
        prntRtrn = snprintf(buff ,BUFF_FREE( len), "\n**** PID BUSY : ****\n");CHECK_SNPRINTF(prntRtrn, len);

        /*get active pids list*/
        avalanche_pp_pid_get_list( &num_entries, pidList );
    }

    while ( i < num_entries )
    {      
        /* Print the PID Information on the console. */
        prntRtrn = snprintf(buff+len, BUFF_FREE( len), "##########\n");CHECK_SNPRINTF(prntRtrn, len);
        prntRtrn = snprintf(buff+len, BUFF_FREE( len), "# PID %02d #\n", pidList[i]->pid_handle);CHECK_SNPRINTF(prntRtrn, len);
        prntRtrn = snprintf(buff+len, BUFF_FREE( len), "##########\n\n");CHECK_SNPRINTF(prntRtrn, len);
        prntRtrn = snprintf(buff+len, BUFF_FREE( len), "Type                = %d [", pidList[i]->type);CHECK_SNPRINTF(prntRtrn, len);
        switch (pidList[i]->type)
        {
            case AVALANCHE_PP_PID_TYPE_UNDEFINED:      prntRtrn = snprintf(buff+len, BUFF_FREE( len), "Undefined");CHECK_SNPRINTF(prntRtrn, len);        break;
            case AVALANCHE_PP_PID_TYPE_ETHERNET:       prntRtrn = snprintf(buff+len, BUFF_FREE( len), "Ethernet");CHECK_SNPRINTF(prntRtrn, len);         break;
            case AVALANCHE_PP_PID_TYPE_INFRASTRUCTURE: prntRtrn = snprintf(buff+len, BUFF_FREE( len), "Infrastructure");CHECK_SNPRINTF(prntRtrn, len);   break;
            case AVALANCHE_PP_PID_TYPE_USBBULK:        prntRtrn = snprintf(buff+len, BUFF_FREE( len), "USB Bulk");CHECK_SNPRINTF(prntRtrn, len);         break;
            case AVALANCHE_PP_PID_TYPE_CDC:            prntRtrn = snprintf(buff+len, BUFF_FREE( len), "CDC");CHECK_SNPRINTF(prntRtrn, len);              break;
            case AVALANCHE_PP_PID_TYPE_DOCSIS:         prntRtrn = snprintf(buff+len, BUFF_FREE( len), "DOCSIS");CHECK_SNPRINTF(prntRtrn, len);           break;
            case AVALANCHE_PP_PID_TYPE_ETHERNETSWITCH: prntRtrn = snprintf(buff+len, BUFF_FREE( len), "Ethernet Switch");CHECK_SNPRINTF(prntRtrn, len);  break;
            default:                                   prntRtrn = snprintf(buff+len, BUFF_FREE( len), "???");CHECK_SNPRINTF(prntRtrn, len);              break;
        }
        prntRtrn = snprintf(buff+len, BUFF_FREE( len), "]\n");CHECK_SNPRINTF(prntRtrn, len);
        prntRtrn = snprintf(buff+len, BUFF_FREE( len), "PriMapping          = %d\n", pidList[i]->pri_mapping);CHECK_SNPRINTF(prntRtrn, len);
        temp = pidList[i]->dflt_pri_drp;
        prntRtrn = snprintf(buff+len, BUFF_FREE( len), "Priority            = 0x%02X [Priority=%d, DropPrecedence=%d]\n", temp, temp&0x7, (temp>>3)&0x3);CHECK_SNPRINTF(prntRtrn, len);
        temp = pidList[i]->ingress_framing;
        prntRtrn = snprintf(buff+len, BUFF_FREE( len), "Framing             = 0x%02X ", temp);CHECK_SNPRINTF(prntRtrn, len);
        if (temp)
        {
            prntRtrn = snprintf(buff+len, BUFF_FREE( len), "[ ");CHECK_SNPRINTF(prntRtrn, len);
            if (temp & AVALANCHE_PP_PID_INGRESS_ETHERNET)  {prntRtrn = snprintf(buff+len, BUFF_FREE( len), "ETHERNET ");CHECK_SNPRINTF(prntRtrn, len);}
            if (temp & AVALANCHE_PP_PID_INGRESS_IPV4)      {prntRtrn = snprintf(buff+len, BUFF_FREE( len), "IPV4 ");CHECK_SNPRINTF(prntRtrn, len);}
            if (temp & AVALANCHE_PP_PID_INGRESS_IPV6)      {prntRtrn = snprintf(buff+len, BUFF_FREE( len), "IPV6 ");CHECK_SNPRINTF(prntRtrn, len);}
            if (temp & AVALANCHE_PP_PID_INGRESS_IPOE)      {prntRtrn = snprintf(buff+len, BUFF_FREE( len), "IPOE ");CHECK_SNPRINTF(prntRtrn, len);}
            if (temp & AVALANCHE_PP_PID_INGRESS_IPOA)      {prntRtrn = snprintf(buff+len, BUFF_FREE( len), "IPOA ");CHECK_SNPRINTF(prntRtrn, len);}
            if (temp & AVALANCHE_PP_PID_INGRESS_PPPOE)     {prntRtrn = snprintf(buff+len, BUFF_FREE( len), "PPPOE ");CHECK_SNPRINTF(prntRtrn, len);}
            if (temp & AVALANCHE_PP_PID_INGRESS_PPPOA)     {prntRtrn = snprintf(buff+len, BUFF_FREE( len), "PPPOA ");CHECK_SNPRINTF(prntRtrn, len);}
            prntRtrn = snprintf(buff+len, BUFF_FREE( len), "]");CHECK_SNPRINTF(prntRtrn, len);
        }
        prntRtrn = snprintf(buff+len, BUFF_FREE( len), "\n");CHECK_SNPRINTF(prntRtrn, len);
        temp = pidList[i]->priv_flags;
        prntRtrn = snprintf(buff+len, BUFF_FREE( len), "Flags               = 0x%02X ", temp);CHECK_SNPRINTF(prntRtrn, len);
        if (temp)
        {
            prntRtrn = snprintf(buff+len, BUFF_FREE( len), "[ ");CHECK_SNPRINTF(prntRtrn, len);
            if (temp & AVALANCHE_PP_PID_VLAN_PRIO_MAP)         {prntRtrn = snprintf(buff+len, BUFF_FREE( len), "fMapVLAN ");CHECK_SNPRINTF(prntRtrn, len);}
            if (temp & AVALANCHE_PP_PID_DIFFSRV_PRIO_MAP)      {prntRtrn = snprintf(buff+len, BUFF_FREE( len), "fMapDIFSRV ");CHECK_SNPRINTF(prntRtrn, len);}
            if (temp & AVALANCHE_PP_PID_PRIO_OFF_TX_DST_TAG)   {prntRtrn = snprintf(buff+len, BUFF_FREE( len), "fUseDestTag ");CHECK_SNPRINTF(prntRtrn, len);}
            if (temp & AVALANCHE_PP_PID_CLASSIFY_BYPASS)       {prntRtrn = snprintf(buff+len, BUFF_FREE( len), "fRxDisable ");CHECK_SNPRINTF(prntRtrn, len);}
            if (temp & AVALANCHE_PP_PID_DISCARD_ALL_RX)        {prntRtrn = snprintf(buff+len, BUFF_FREE( len), "fDiscardRx ");CHECK_SNPRINTF(prntRtrn, len);}
            prntRtrn = snprintf(buff+len, BUFF_FREE( len), "]");CHECK_SNPRINTF(prntRtrn, len);
        }
        prntRtrn = snprintf(buff+len, BUFF_FREE( len), "\n");CHECK_SNPRINTF(prntRtrn, len);
        prntRtrn = snprintf(buff+len, BUFF_FREE( len), "TxDestTag           = 0x%04X\n", pidList[i]->dflt_dst_tag);CHECK_SNPRINTF(prntRtrn, len);
        prntRtrn = snprintf(buff+len, BUFF_FREE( len), "TxQueueBase         = %d [%s]\n\n", pidList[i]->dflt_fwd_q, PAL_CPPI41_GET_QNAME( pidList[i]->dflt_fwd_q ) );CHECK_SNPRINTF(prntRtrn, len);
        WR_TO_PAGE

        /* Set next begin of page */   
        *start = page;
        *eof = 1;
        len = 0;
        i++; 

        /* Return number of bytes writen */            
        return curr_off ; 
    }

    prntRtrn = snprintf(buff, BUFF_FREE( len), "\n**** END OF PID BUSY ****\n");CHECK_SNPRINTF(prntRtrn, len); 
    WR_TO_PAGE

    /* Set next begin of page */   
    *start = page;
    *eof = 1;
    endOfData = 1;

    /* Return number of bytes writen */
    return curr_off ;
}
/**************************************************************************************/
/*! \fn static int PID_FREE_read_proc (char *page, char **start, 
*                  off_t off, int count,int *eof, void *data)
 **************************************************************************************
 *  \brief This function reads all free PIDs
 *  \return return the number of bytes wirtten to the buffer.
 **************************************************************************************/
static int PID_FREE_read_proc (char *page, char **start, off_t off, int count, int *eof, void *data)
{ 
    int           len = 0;
    static int    i;
    int           curr_off = 0;
    static Uint8  endOfData = 0;
    int           prntRtrn;

    if (endOfData == 1)
    {
        endOfData = 0;
        return 0;
    }

    if (off == 0) 
    {
        i = 0;
        prntRtrn = snprintf(buff, BUFF_FREE( len), "\n**** PID FREE : ****\n");CHECK_SNPRINTF(prntRtrn, len);
        
    }

	PP_DB_LOCK();
	
    /*for each PID check if not active*/
    FOR_ALL_PIDS(i) 
    {
        if (PP_DB.repository_PIDs[i].status != PP_DB_STATUS_ACTIVE)
        {
            prntRtrn = snprintf(buff+len, BUFF_FREE( len), " (%2d)%4d ", i, PP_DB.repository_PIDs[i].pid.pid_handle);CHECK_SNPRINTF(prntRtrn, len);
        }
    }

    PP_DB_UNLOCK();
    prntRtrn = snprintf(buff+len, BUFF_FREE( len), "\n**** END OF PID FREE ****\n");CHECK_SNPRINTF(prntRtrn, len);
    WR_TO_PAGE

    /* Set next begin of page */   
    *start = page;
    *eof = 1;
    endOfData = 1;
	
    /* Return number of bytes writen */
    return curr_off ;
}

/**************************************************************************************/
/*! \fn static int VPID_BUSY_read_proc (char *page, char **start, 
*                  off_t off, int count,int *eof, void *data)
 **************************************************************************************
 *  \brief This function reads all busy VPIDs
 *        
 *  \return return the number of bytes wirtten to the buffer.
 **************************************************************************************/
static int VPID_BUSY_read_proc (char *page, char **start, off_t off, int count, int *eof, void *data)
{ 
    int             len = 0;
    int             curr_off = 0;
    static Uint8    endOfData = 0;
    int             prntRtrn;
    static Uint8    vpidIndex;
    static Uint8    num_entries;
    static AVALANCHE_PP_VPID_INFO_t *vpid_list[AVALANCHE_PP_MAX_VPID];
    
    if (endOfData == 1)
    {
        endOfData = 0;
        return 0;
    }
	
    if (off == 0) 
    {
        vpidIndex = 0;     
        prntRtrn = snprintf(buff, BUFF_FREE( len), "\n**** VPID BUSY : ****\n");CHECK_SNPRINTF(prntRtrn, len);
        prntRtrn = snprintf(buff+len, BUFF_FREE( len), "-------------------------------\n");CHECK_SNPRINTF(prntRtrn, len);

        /*get active vpids list*/
        avalanche_pp_vpid_get_list( AVALANCHE_PP_MAX_PID, &num_entries, vpid_list );
    }

    while ( vpidIndex < num_entries )
    {
        AVALANCHE_PP_VPID_STATS_t vpid_stats;
        Int8* vpid_type;

        switch (vpid_list[vpidIndex]->type)
        {
            case AVALANCHE_PP_VPID_ETHERNET     : vpid_type = "ETHERNET   " ; break;
            case AVALANCHE_PP_VPID_VLAN         : vpid_type = "VLAN       " ; break;
            case AVALANCHE_PP_VPID_PPPoE        : vpid_type = "PPPoE      " ; break;
            case AVALANCHE_PP_VPID_VLAN_PPPoE   : vpid_type = "VLAN_PPPoE " ; break;
            default:                              vpid_type = "UNKNOWN"     ; break;
        }

        /* Get the VPID statistics. */
        avalanche_pp_get_stats_vpid(vpid_list[vpidIndex]->vpid_handle, &vpid_stats);

        /* Print the statistics on the console. */
        prntRtrn = snprintf(buff+len, BUFF_FREE( len), "---------------- VPID %d (PID %d) ----------------\n", vpid_list[vpidIndex]->vpid_handle, vpid_list[vpidIndex]->parent_pid_handle );CHECK_SNPRINTF(prntRtrn, len);
        prntRtrn = snprintf(buff+len, BUFF_FREE( len), "                 type: %s \n", vpid_type );CHECK_SNPRINTF(prntRtrn, len);
        prntRtrn = snprintf(buff+len, BUFF_FREE( len), "Rx Unicast   Packets: %u\n",     vpid_stats.rx_unicast_pkt);CHECK_SNPRINTF(prntRtrn, len);
        prntRtrn = snprintf(buff+len, BUFF_FREE( len), "Rx Broadcast Packets: %u\n",     vpid_stats.rx_broadcast_pkt);CHECK_SNPRINTF(prntRtrn, len);
        prntRtrn = snprintf(buff+len, BUFF_FREE( len), "Rx Multicast Packets: %u\n",     vpid_stats.rx_multicast_pkt);CHECK_SNPRINTF(prntRtrn, len);
        prntRtrn = snprintf(buff+len, BUFF_FREE( len), "Rx Bytes            : %llu\n",   vpid_stats.rx_byte);CHECK_SNPRINTF(prntRtrn, len);
        prntRtrn = snprintf(buff+len, BUFF_FREE( len), "Rx Discard          : %u\n",     vpid_stats.rx_discard_pkt);CHECK_SNPRINTF(prntRtrn, len);
        prntRtrn = snprintf(buff+len, BUFF_FREE( len), "Tx Unicast   Packets: %u\n",     vpid_stats.tx_unicast_pkt);CHECK_SNPRINTF(prntRtrn, len);
        prntRtrn = snprintf(buff+len, BUFF_FREE( len), "Tx Broadcast Packets: %u\n",     vpid_stats.tx_broadcast_pkt);CHECK_SNPRINTF(prntRtrn, len);
        prntRtrn = snprintf(buff+len, BUFF_FREE( len), "Tx Multicast Packets: %u\n",     vpid_stats.tx_multicast_pkt);CHECK_SNPRINTF(prntRtrn, len);
        prntRtrn = snprintf(buff+len, BUFF_FREE( len), "Tx Bytes            : %llu\n",   vpid_stats.tx_byte);CHECK_SNPRINTF(prntRtrn, len);
        prntRtrn = snprintf(buff+len, BUFF_FREE( len), "Tx Errors           : %u\n",     vpid_stats.tx_error);CHECK_SNPRINTF(prntRtrn, len);
        prntRtrn = snprintf(buff+len, BUFF_FREE( len), "Tx Discards         : %u\n",     vpid_stats.tx_discard_pkt);CHECK_SNPRINTF(prntRtrn, len);
        prntRtrn = snprintf(buff+len, BUFF_FREE( len), "-----------------------------------------\n");CHECK_SNPRINTF(prntRtrn, len);
        WR_TO_PAGE
        len = 0;
        vpidIndex++;

        /* Set next begin of page */   
        *start = page;
        *eof = 1;

        /* Return number of bytes writen */
        return curr_off ;
    }

    prntRtrn = snprintf(buff+len, BUFF_FREE( len), "\n**** END OF VPID BUSY ****\n");CHECK_SNPRINTF(prntRtrn, len);
    WR_TO_PAGE

    /* Set next begin of page */   
    *start = page;
    *eof = 1;
    endOfData = 1;

    /* Return number of bytes writen */
    return curr_off ;
}

/**************************************************************************************/
/*! \fn static int VPID_FREE_read_proc (char *page, char **start, 
*                  off_t off, int count,int *eof, void *data)
 **************************************************************************************
 *  \brief This function reads all free VPIDs
 *  \return return the number of bytes wirtten to the buffer.
 **************************************************************************************/
static int VPID_FREE_read_proc (char *page, char **start, off_t off, int count, int *eof, void *data)
{ 
    int           len = 0;
    int           curr_off = 0;
    static struct list_head *   pos = NULL;
    static Uint8  endOfData = 0;
    int           prntRtrn;

    if (endOfData == 1)
    {
        endOfData = 0;
        return 0;
    }
	
	PP_DB_LOCK();
	
    if (off == 0) 
    {
        pos = PP_DB.pool_VPIDs[ PP_DB_POOL_FREE ].next;
        prntRtrn = snprintf(buff, BUFF_FREE( len), "\n**** VPID FREE : ****\n");CHECK_SNPRINTF(prntRtrn, len);
        
    }

    /*for each VPID from list print handle*/
    LIST_FOR_EACH( pos, &PP_DB.pool_VPIDs[ PP_DB_POOL_FREE ] )
    {
        PP_DB_VPID_Entry_t * entry;
        entry = list_entry(pos, PP_DB_VPID_Entry_t, link);
        prntRtrn = snprintf(buff+len, BUFF_FREE( len), " %4d ", entry->handle);CHECK_SNPRINTF(prntRtrn, len);
    }

    PP_DB_UNLOCK();
    prntRtrn = snprintf(buff+len, BUFF_FREE( len), "\n**** END OF VPID FREE ****\n");CHECK_SNPRINTF(prntRtrn, len);   
    WR_TO_PAGE
    
    /* Set next begin of page */   
    *start = page;
    *eof = 1;
    endOfData = 1;

    /* Return number of bytes writen */
    return curr_off ;
}
/**************************************************************************************/
/*! \fn static int VPID_LISTS_read_proc (char *page, char **start, 
*                  off_t off, int count,int *eof, void *data)
 **************************************************************************************
 *  \brief This function reads all INGRESS EGRESS TCP TDOX
 *  \return return the number of bytes wirtten to the buffer.
 **************************************************************************************/
static int VPID_LISTS_read_proc (char *page, char **start, off_t off, int count, int *eof, void *data)
{ 
    int           len = 0;
    int           curr_off = 0;
    static Uint8  endOfData = 0;
    int           prntRtrn;
    static Uint8  counter;
    static int    sessionIndex;
    static Uint32 num_entries;

    if (endOfData == 1)
    {
        endOfData = 0;
        return 0;
    }
    
	if (off == 0) 
    {
        if (g_proc_vpid_id < 0 || g_proc_vpid_id > 31)
        {
            prntRtrn = snprintf(buff+len, BUFF_FREE( len), "vpid %d not valid \n", g_proc_vpid_id);CHECK_SNPRINTF(prntRtrn, len);
            WR_TO_PAGE

            /* Set next begin of page */   
            *start = page;
            *eof = 1;
            endOfData = 1;

            /* Return number of bytes writen */
            return curr_off ;
        }
        
        sessionIndex = 0;
        counter = 1;
        
        prntRtrn = snprintf(buff+len, BUFF_FREE( len), "---------------- VPID %d ----------------\n", g_proc_vpid_id);CHECK_SNPRINTF(prntRtrn, len);
        if (((PP_LIST_ID_e)(data)) == PP_LIST_ID_INGRESS)     {prntRtrn = snprintf(buff+len, BUFF_FREE( len), "---------- Ingress Sessions: ------------\n");CHECK_SNPRINTF(prntRtrn, len);}
        if (((PP_LIST_ID_e)(data)) == PP_LIST_ID_EGRESS)      {prntRtrn = snprintf(buff+len, BUFF_FREE( len), "---------- Egress Sessions: -------------\n");CHECK_SNPRINTF(prntRtrn, len);}
        if (((PP_LIST_ID_e)(data)) == PP_LIST_ID_EGRESS_TCP)  {prntRtrn = snprintf(buff+len, BUFF_FREE( len), "---------- Tcp Sessions:   --------------\n");CHECK_SNPRINTF(prntRtrn, len);}
        if (((PP_LIST_ID_e)(data)) == PP_LIST_ID_EGRESS_TDOX) {prntRtrn = snprintf(buff+len, BUFF_FREE( len), "---------- Tdox Sessions:  --------------\n");CHECK_SNPRINTF(prntRtrn, len);}
        WR_TO_PAGE
        len = 0;
        
        /*get active session list*/
        avalanche_pp_session_get_list( g_proc_vpid_id, ((PP_LIST_ID_e)(data)), &num_entries, tmp2k );

    }
	 
    /*print handle for all sessions in global tmp2k*/
    while ( sessionIndex < num_entries )
    {
        len = 0;
        prntRtrn = snprintf(buff, BUFF_FREE( len), " %4d ", tmp2k[sessionIndex] );CHECK_SNPRINTF(prntRtrn, len);
        if (counter == 25)
        {
            prntRtrn = snprintf(buff+len, BUFF_FREE( len), "\n"); CHECK_SNPRINTF(prntRtrn, len); 
            counter = 0;
        }
        WR_TO_PAGE
        sessionIndex++;
        counter++;
    }

    len = 0;
    prntRtrn = snprintf(buff, BUFF_FREE( len), "\n\n");CHECK_SNPRINTF(prntRtrn, len);
    WR_TO_PAGE

    /* Set next begin of page */   
    *start = page;
    *eof = 1;
    endOfData = 1;

    /* Return number of bytes writen */
    return curr_off ;
}
/**************************************************************************************/
/*! \fn static int GLOBAL_read_proc (char *page, char **start, 
*                  off_t off, int count,int *eof, void *data)
 **************************************************************************************
 *  \brief This function reads all GLOBAL stat
 *  \return return the number of bytes wirtten to the buffer.
 **************************************************************************************/
static int GLOBAL_read_proc (char *page, char **start, off_t off, int count, int *eof, void *data)
{ 
    int                         len = 0;
    int                         curr_off = 0;
    static Uint8                endOfData = 0;
    int                         prntRtrn;
    AVALANCHE_PP_GLOBAL_STATS_t pp_stats;
    
    if (endOfData == 1)
    {
        endOfData = 0;
        return 0;
    }

    /*Get the global stats through the PPM */
    avalanche_pp_get_stats_global(&pp_stats);

    prntRtrn = snprintf(buff+len, BUFF_FREE( len), "\nPPDSP Counters:\n===============\n"); CHECK_SNPRINTF(prntRtrn, len);
    prntRtrn = snprintf(buff+len, BUFF_FREE( len), "rx_pkts                 = %u\n", pp_stats.ppdsp_rx_pkts); CHECK_SNPRINTF(prntRtrn, len);
    prntRtrn = snprintf(buff+len, BUFF_FREE( len), "pkts_frwrd_to_cpdsp1    = %u\n", pp_stats.ppdsp_pkts_frwrd_to_cpdsp1); CHECK_SNPRINTF(prntRtrn, len);
    prntRtrn = snprintf(buff+len, BUFF_FREE( len), "not_enough_descriptors  = %u\n\n", pp_stats.ppdsp_not_enough_descriptors); CHECK_SNPRINTF(prntRtrn, len);

    prntRtrn = snprintf(buff+len, BUFF_FREE( len), "CPDSP1 Counters:\n===============\n"); CHECK_SNPRINTF(prntRtrn, len);
    prntRtrn = snprintf(buff+len, BUFF_FREE( len), "rx_pkts                 = %u\n", pp_stats.cpdsp1_rx_pkts); CHECK_SNPRINTF(prntRtrn, len);
    prntRtrn = snprintf(buff+len, BUFF_FREE( len), "lut1_search_attempts    = %u\n", pp_stats.cpdsp1_lut1_search_attempts); CHECK_SNPRINTF(prntRtrn, len);
    prntRtrn = snprintf(buff+len, BUFF_FREE( len), "lut1_matches            = %u\n", pp_stats.cpdsp1_lut1_matches); CHECK_SNPRINTF(prntRtrn, len);
    prntRtrn = snprintf(buff+len, BUFF_FREE( len), "pkts_frwrd_to_cpdsp2    = %u\n\n", pp_stats.cpdsp1_pkts_frwrd_to_cpdsp2); CHECK_SNPRINTF(prntRtrn, len);

    prntRtrn = snprintf(buff+len, BUFF_FREE( len), "CPDSP2 Counters:\n===============\n"); CHECK_SNPRINTF(prntRtrn, len);
    prntRtrn = snprintf(buff+len, BUFF_FREE( len), "rx_pkts                 = %u\n", pp_stats.cpdsp2_rx_pkts); CHECK_SNPRINTF(prntRtrn, len);
    prntRtrn = snprintf(buff+len, BUFF_FREE( len), "lut2_search_attempts    = %u\n", pp_stats.cpdsp2_lut2_search_attempts); CHECK_SNPRINTF(prntRtrn, len);
    prntRtrn = snprintf(buff+len, BUFF_FREE( len), "lut2_matches            = %u\n", pp_stats.cpdsp2_lut2_matches); CHECK_SNPRINTF(prntRtrn, len);
    prntRtrn = snprintf(buff+len, BUFF_FREE( len), "pkts_frwrd_to_mpdsp     = %u\n", pp_stats.cpdsp2_pkts_frwrd_to_mpdsp); CHECK_SNPRINTF(prntRtrn, len);
    prntRtrn = snprintf(buff+len, BUFF_FREE( len), "synch_timeout_events    = %u\n", pp_stats.cpdsp2_synch_timeout_events); CHECK_SNPRINTF(prntRtrn, len);
    prntRtrn = snprintf(buff+len, BUFF_FREE( len), "reassembly_db_full      = %u\n", pp_stats.cpdsp2_reassembly_db_full); CHECK_SNPRINTF(prntRtrn, len);
    prntRtrn = snprintf(buff+len, BUFF_FREE( len), "reassembly_db_timeout   = %u\n\n", pp_stats.cpdsp2_reassembly_db_timeout); CHECK_SNPRINTF(prntRtrn, len);

    prntRtrn = snprintf(buff+len, BUFF_FREE( len), "MPDSP Counters:\n==============\n"); CHECK_SNPRINTF(prntRtrn, len);
    prntRtrn = snprintf(buff+len, BUFF_FREE( len), "rx_pkts                 = %u\n", pp_stats.mpdsp_rx_pkts); CHECK_SNPRINTF(prntRtrn, len);
    prntRtrn = snprintf(buff+len, BUFF_FREE( len), "ipv4_rx_pkts            = %u\n", pp_stats.mpdsp_ipv4_rx_pkts); CHECK_SNPRINTF(prntRtrn, len);
    prntRtrn = snprintf(buff+len, BUFF_FREE( len), "ipv6_rx_pkts            = %u\n", pp_stats.mpdsp_ipv6_rx_pkts); CHECK_SNPRINTF(prntRtrn, len);
    prntRtrn = snprintf(buff+len, BUFF_FREE( len), "frwrd_to_host           = %u\n", pp_stats.mpdsp_frwrd_to_host); CHECK_SNPRINTF(prntRtrn, len);
    prntRtrn = snprintf(buff+len, BUFF_FREE( len), "frwrd_to_qpdsp          = %u\n", pp_stats.mpdsp_frwrd_to_qpdsp); CHECK_SNPRINTF(prntRtrn, len);
    prntRtrn = snprintf(buff+len, BUFF_FREE( len), "frwrd_to_synch_q        = %u\n", pp_stats.mpdsp_frwrd_to_synch_q); CHECK_SNPRINTF(prntRtrn, len);
    prntRtrn = snprintf(buff+len, BUFF_FREE( len), "discards                = %u\n", pp_stats.mpdsp_discards); CHECK_SNPRINTF(prntRtrn, len);
    prntRtrn = snprintf(buff+len, BUFF_FREE( len), "synchq_overflow_events  = %u\n\n", pp_stats.mpdsp_synchq_overflow_events); CHECK_SNPRINTF(prntRtrn, len);

    if (pp_stats.qpdsp_ooo_discards)
    {
        prntRtrn = snprintf(buff + len, BUFF_FREE(len), "QPDSP Counters:\n==============\n"); CHECK_SNPRINTF(prntRtrn, len); 
        prntRtrn = snprintf(buff+len, BUFF_FREE( len), "Out Of Order (Discards) = %u\n\n", pp_stats.qpdsp_ooo_discards); CHECK_SNPRINTF(prntRtrn, len);
    }
    
    WR_TO_PAGE

    /* Set next begin of page */   
    *start = page;
    *eof = 1;
    endOfData = 1;

    /* Return number of bytes writen */
    return curr_off ;
}
/**************************************************************************************/
/*! \fn static int BRIEF_read_proc (char *page, char **start, 
*                  off_t off, int count,int *eof, void *data)
 **************************************************************************************
 *  \brief This function reads all BRIEF stat
 *  \return return the number of bytes wirtten to the buffer.
 **************************************************************************************/
static int BRIEF_read_proc (char *page, char **start, off_t off, int count, int *eof, void *data)
{ 
    int                         len = 0;
    int                         curr_off = 0;
    static Uint8                endOfData = 0;
    int                         prntRtrn;
    static Uint8                curr_pos = 0;
    static Uint8                vpidIndex;
    static Uint8                vpids_num_entries;
    static AVALANCHE_PP_VPID_INFO_t *vpid_list[AVALANCHE_PP_MAX_VPID];
    static Uint32               sessionIndex;
    static Uint32               sessions_num_entries;
    AVALANCHE_PP_GLOBAL_STATS_t pp_stats;

    if (endOfData == 1)
    {
        endOfData = 0;
        return 0;
    }

    if (off == 0)
    {
        vpidIndex = 0;
        sessionIndex = 0;
        curr_pos = 0;

        /*get active vpid list*/
        avalanche_pp_vpid_get_list( AVALANCHE_PP_MAX_PID, &vpids_num_entries, vpid_list );
        /*get active session list*/
        avalanche_pp_session_get_list( AVALANCHE_PP_MAX_VPID, PP_LIST_ID_ALL, &sessions_num_entries, tmp2k);

        /* GLOBAL */
        {
            avalanche_pp_get_stats_global(&pp_stats);

            prntRtrn = snprintf(buff + len, BUFF_FREE( len), "\nPPDSP Counters:\n===============\n"); CHECK_SNPRINTF(prntRtrn, len);
            prntRtrn = snprintf(buff + len, BUFF_FREE( len), "rx_pkts                 = %u\n", pp_stats.ppdsp_rx_pkts); CHECK_SNPRINTF(prntRtrn, len);
            prntRtrn = snprintf(buff + len, BUFF_FREE( len), "pkts_frwrd_to_cpdsp1    = %u\n", pp_stats.ppdsp_pkts_frwrd_to_cpdsp1); CHECK_SNPRINTF(prntRtrn, len);
            prntRtrn = snprintf(buff + len, BUFF_FREE( len), "not_enough_descriptors  = %u\n\n", pp_stats.ppdsp_not_enough_descriptors); CHECK_SNPRINTF(prntRtrn, len);

            prntRtrn = snprintf(buff + len, BUFF_FREE( len), "CPDSP1 Counters:\n===============\n"); CHECK_SNPRINTF(prntRtrn, len);
            prntRtrn = snprintf(buff + len, BUFF_FREE( len), "rx_pkts                 = %u\n", pp_stats.cpdsp1_rx_pkts); CHECK_SNPRINTF(prntRtrn, len);
            prntRtrn = snprintf(buff + len, BUFF_FREE( len), "lut1_search_attempts    = %u\n", pp_stats.cpdsp1_lut1_search_attempts); CHECK_SNPRINTF(prntRtrn, len);
            prntRtrn = snprintf(buff + len, BUFF_FREE( len), "lut1_matches            = %u\n", pp_stats.cpdsp1_lut1_matches); CHECK_SNPRINTF(prntRtrn, len);
            prntRtrn = snprintf(buff + len, BUFF_FREE( len), "pkts_frwrd_to_cpdsp2    = %u\n\n", pp_stats.cpdsp1_pkts_frwrd_to_cpdsp2); CHECK_SNPRINTF(prntRtrn, len);

            prntRtrn = snprintf(buff + len, BUFF_FREE( len), "CPDSP2 Counters:\n===============\n"); CHECK_SNPRINTF(prntRtrn, len);
            prntRtrn = snprintf(buff + len, BUFF_FREE( len), "rx_pkts                 = %u\n", pp_stats.cpdsp2_rx_pkts); CHECK_SNPRINTF(prntRtrn, len);
            prntRtrn = snprintf(buff + len, BUFF_FREE( len), "lut2_search_attempts    = %u\n", pp_stats.cpdsp2_lut2_search_attempts); CHECK_SNPRINTF(prntRtrn, len);
            prntRtrn = snprintf(buff + len, BUFF_FREE( len), "lut2_matches            = %u\n", pp_stats.cpdsp2_lut2_matches); CHECK_SNPRINTF(prntRtrn, len);
            prntRtrn = snprintf(buff + len, BUFF_FREE( len), "pkts_frwrd_to_mpdsp     = %u\n", pp_stats.cpdsp2_pkts_frwrd_to_mpdsp); CHECK_SNPRINTF(prntRtrn, len);
            prntRtrn = snprintf(buff + len, BUFF_FREE( len), "synch_timeout_events    = %u\n", pp_stats.cpdsp2_synch_timeout_events); CHECK_SNPRINTF(prntRtrn, len);
            prntRtrn = snprintf(buff + len, BUFF_FREE( len), "reassembly_db_full      = %u\n", pp_stats.cpdsp2_reassembly_db_full); CHECK_SNPRINTF(prntRtrn, len);
            prntRtrn = snprintf(buff + len, BUFF_FREE( len), "reassembly_db_timeout   = %u\n\n", pp_stats.cpdsp2_reassembly_db_timeout); CHECK_SNPRINTF(prntRtrn, len);

            prntRtrn = snprintf(buff + len, BUFF_FREE( len), "MPDSP Counters:\n==============\n"); CHECK_SNPRINTF(prntRtrn, len);
            prntRtrn = snprintf(buff + len, BUFF_FREE( len), "rx_pkts                 = %u\n", pp_stats.mpdsp_rx_pkts); CHECK_SNPRINTF(prntRtrn, len);
            prntRtrn = snprintf(buff + len, BUFF_FREE( len), "ipv4_rx_pkts            = %u\n", pp_stats.mpdsp_ipv4_rx_pkts); CHECK_SNPRINTF(prntRtrn, len);
            prntRtrn = snprintf(buff + len, BUFF_FREE( len), "ipv6_rx_pkts            = %u\n", pp_stats.mpdsp_ipv6_rx_pkts); CHECK_SNPRINTF(prntRtrn, len);
            prntRtrn = snprintf(buff + len, BUFF_FREE( len), "frwrd_to_host           = %u\n", pp_stats.mpdsp_frwrd_to_host); CHECK_SNPRINTF(prntRtrn, len);
            prntRtrn = snprintf(buff + len, BUFF_FREE( len), "frwrd_to_qpdsp          = %u\n", pp_stats.mpdsp_frwrd_to_qpdsp); CHECK_SNPRINTF(prntRtrn, len);
            prntRtrn = snprintf(buff + len, BUFF_FREE( len), "frwrd_to_synch_q        = %u\n", pp_stats.mpdsp_frwrd_to_synch_q); CHECK_SNPRINTF(prntRtrn, len);
            prntRtrn = snprintf(buff + len, BUFF_FREE( len), "discards                = %u\n", pp_stats.mpdsp_discards); CHECK_SNPRINTF(prntRtrn, len);
            prntRtrn = snprintf(buff + len, BUFF_FREE( len), "synchq_overflow_events  = %u\n\n", pp_stats.mpdsp_synchq_overflow_events); CHECK_SNPRINTF(prntRtrn, len);

            if (pp_stats.qpdsp_ooo_discards)
            {
                prntRtrn = snprintf(buff + len, BUFF_FREE(len), "QPDSP Counters:\n==============\n"); CHECK_SNPRINTF(prntRtrn, len); 
                prntRtrn = snprintf(buff+len, BUFF_FREE( len), "Out Of Order (Discards) = %u\n\n", pp_stats.qpdsp_ooo_discards); CHECK_SNPRINTF(prntRtrn, len);
            }
            WR_TO_PAGE
        }  
        /* Set next begin of page */   
        *start = page;
        *eof = 1;

        /* Return number of bytes writen */
        return curr_off ;
    }
    
    if (curr_pos == 0)
    {  
        /* VPIDs */
        {
            /* Cycle through all the active VPID and get stats */
            while ( vpidIndex < vpids_num_entries )
            {
                AVALANCHE_PP_VPID_STATS_t vpid_stats;

                /* Get the VPID statistics. */
                avalanche_pp_get_stats_vpid(vpid_list[vpidIndex]->vpid_handle, &vpid_stats);
                prntRtrn = snprintf(buff + len, BUFF_FREE( len), "VPID index=%02d, handle=%02d: Rx=%10d, Tx=%10d\n", vpidIndex, vpid_list[vpidIndex]->vpid_handle,
                                    vpid_stats.rx_unicast_pkt + vpid_stats.rx_broadcast_pkt  +vpid_stats.rx_multicast_pkt,
                                    vpid_stats.tx_unicast_pkt + vpid_stats.tx_broadcast_pkt + vpid_stats.tx_multicast_pkt);CHECK_SNPRINTF(prntRtrn, len);
                vpidIndex++;
            }
            curr_pos = 1;
            WR_TO_PAGE

            /* Set next begin of page */                  
            *start = page;
            *eof = 1;

            /* Return number of bytes writen */
            return curr_off ;               
        }   
    }

    if (curr_pos == 1)
    {      
        /* SESSIONs */
        {   
            /* Cycle through all the busy sessions and get stats */  
            while ( sessionIndex < sessions_num_entries )
            {
                AVALANCHE_PP_SESSION_STATS_t session_stats;
                len = 0;

                /* Get the session statistics */
                avalanche_pp_get_stats_session(tmp2k[sessionIndex], &session_stats);
                prntRtrn = snprintf(buff , BUFF_FREE( len), "SESSION index=%d, handle=%d: Fwd=%d\n", sessionIndex, tmp2k[sessionIndex], session_stats.packets_forwarded);CHECK_SNPRINTF(prntRtrn, len); 
                WR_TO_PAGE
                sessionIndex++;
            }
        }
    }
    len = 0;
    prntRtrn = snprintf(buff, BUFF_FREE( len), "\n");CHECK_SNPRINTF(prntRtrn, len);
    WR_TO_PAGE

    /* Set next begin of page */   
    *start = page;
    *eof = 1;
    endOfData = 1;

    /* Return number of bytes writen */
    return curr_off ;
}
/**************************************************************************************/
/*! \fn static int LUT1_BUSY_read_proc (char *page, char **start, 
*                  off_t off, int count,int *eof, void *data)
 **************************************************************************************
 *  \brief This function reads all busy LUT1 sessions
 *  \return return the number of bytes wirtten to the buffer.
 **************************************************************************************/
static int LUT1_BUSY_read_proc (char *page, char **start, off_t off, int count, int *eof, void *data)
{ 
    int           len = 0;
    int           curr_off = 0;
    static struct list_head *   pos = NULL;
    static Uint8  endOfData = 0;
    int           prntRtrn;

    if (endOfData == 1)
    {
        endOfData = 0;
        return 0;
    }
	
	PP_DB_LOCK();
	
    if (off == 0) 
    {
        pos = PP_DB.pool_lut1[AVALANCHE_PP_SESSIONS_POOL_DATA][PP_DB_POOL_BUSY].next;
        prntRtrn = snprintf(buff+len, BUFF_FREE( len), "\n**** LUT1 BUSY : ****\n");CHECK_SNPRINTF(prntRtrn, len);
    }
	
    /*for each LUT1 sessions from list print handle*/
    LIST_FOR_EACH( pos, &PP_DB.pool_lut1[ AVALANCHE_PP_SESSIONS_POOL_DATA ][ PP_DB_POOL_BUSY ] )
    {
        PP_DB_Session_LUT1_hash_entry_t * entry;
        entry = list_entry(pos, PP_DB_Session_LUT1_hash_entry_t, link);
        prntRtrn = snprintf(buff+len, BUFF_FREE( len), " %4d ", entry->handle);CHECK_SNPRINTF(prntRtrn, len);
    }

    PP_DB_UNLOCK();
    prntRtrn = snprintf(buff+len, BUFF_FREE( len), "\n**** END OF LUT1 BUSY ****\n");CHECK_SNPRINTF(prntRtrn, len);
    WR_TO_PAGE

    /* Set next begin of page */   
    *start = page;
    *eof = 1;
    endOfData = 1;

    /* Return number of bytes writen */
    return curr_off ;
}
/**************************************************************************************/
/*! \fn static int LUT1_FREE_read_proc (char *page, char **start, 
*                  off_t off, int count,int *eof, void *data)
 **************************************************************************************
 *  \brief This function reads all free LUT1 sessions
 *  \return return the number of bytes wirtten to the buffer.
 **************************************************************************************/
static int LUT1_FREE_read_proc (char *page, char **start, off_t off, int count, int *eof, void *data)
{ 
    int           len = 0;
    int           curr_off = 0;
    static struct list_head *   pos = NULL;
    static Uint8  endOfData = 0;
    int           prntRtrn;

    if (endOfData == 1)
    {
        endOfData = 0;
        return 0;
    }

	PP_DB_LOCK();
	 
    if (off == 0) 
    {
        pos = PP_DB.pool_lut1[AVALANCHE_PP_SESSIONS_POOL_DATA][PP_DB_POOL_FREE].next;
        prntRtrn = snprintf(buff+len, BUFF_FREE( len), "\n**** LUT1 FREE : ****\n");CHECK_SNPRINTF(prntRtrn, len);
    }

    /*for each LUT1 sessions from list print handle*/
    LIST_FOR_EACH( pos, &PP_DB.pool_lut1[ AVALANCHE_PP_SESSIONS_POOL_DATA ][ PP_DB_POOL_FREE ] )
    {
        PP_DB_Session_LUT1_hash_entry_t * entry;
        entry = list_entry(pos, PP_DB_Session_LUT1_hash_entry_t, link);
        prntRtrn = snprintf(buff+len, BUFF_FREE( len), " %4d ", entry->handle);CHECK_SNPRINTF(prntRtrn, len);
    }

    PP_DB_UNLOCK();
    prntRtrn = snprintf(buff+len, BUFF_FREE( len), "\n**** END OF LUT1 FREE ****\n");CHECK_SNPRINTF(prntRtrn, len);   
    WR_TO_PAGE

    /* Set next begin of page */   
    *start = page;
    *eof = 1;
    endOfData = 1;

    /* Return number of bytes writen */
    return curr_off ;
}
/**************************************************************************************/
/*! \fn static int LUT2_BUSY_read_proc (char *page, char **start, 
*                  off_t off, int count,int *eof, void *data)
 **************************************************************************************
 *  \brief This function reads all busy LUT2 sessions info
 *  \return return the number of bytes wirtten to the buffer.
 **************************************************************************************/
static int LUT2_BUSY_read_proc (char *page, char **start, off_t off, int count, int *eof, void *data)
{ 
    int           len = 0;
    int           curr_off = 0;
    static Uint8  endOfData = 0;
    int           prntRtrn;
    static Uint32 sessionIndex;
    static Uint32 num_entries;

    if (endOfData == 1)
    {
        endOfData = 0;
        return 0;
    }
	
    if (off == 0) 
    {
        prntRtrn = snprintf(buff+len, BUFF_FREE( len), "\n**** LUT2 BUSY : ****\n");CHECK_SNPRINTF(prntRtrn, len);
        WR_TO_PAGE
        len = 0;
        sessionIndex = 0;

        /*get active session list*/
        avalanche_pp_session_get_list( AVALANCHE_PP_MAX_VPID, PP_LIST_ID_ALL, &num_entries, tmp2k);
    }

    while ( sessionIndex < num_entries )
    {
        /* Print the Session Information on the console */
        prntRtrn = snprintf(buff+len, BUFF_FREE( len), "\n");CHECK_SNPRINTF(prntRtrn, len);
        prntRtrn = snprintf(buff+len, BUFF_FREE( len), "####################\n");CHECK_SNPRINTF(prntRtrn, len);
        prntRtrn = snprintf(buff+len, BUFF_FREE( len), "# Session %03d info #\n", tmp2k[sessionIndex]);CHECK_SNPRINTF(prntRtrn, len);
        prntRtrn = snprintf(buff+len, BUFF_FREE( len), "####################\n\n");CHECK_SNPRINTF(prntRtrn, len);
        prntRtrn = LUT2_display_session(tmp2k[sessionIndex], buff+len, BUFF_FREE( len));CHECK_SNPRINTF(prntRtrn, len);

        /*clean buffer and start over with next session*/
        WR_TO_PAGE
        *start = page;
        *eof = 1;
        sessionIndex++;  
        return curr_off;
    }

    prntRtrn = snprintf(buff+len, BUFF_FREE( len), "\nDetected %d sessions in packet processor\n\n", sessionIndex );CHECK_SNPRINTF(prntRtrn, len);
    prntRtrn = snprintf(buff+len, BUFF_FREE( len), "\n**** END OF LUT2 BUSY ****\n");CHECK_SNPRINTF(prntRtrn, len);
    WR_TO_PAGE

    /* Set next begin of page */   
    *start = page;
    *eof = 1;
    endOfData = 1;

    /* Return number of bytes writen */
    return curr_off ;
}

/**************************************************************************************/
/*! \fn static int LUT2_FREE_read_proc (char *page, char **start, 
*                  off_t off, int count,int *eof, void *data)
 **************************************************************************************
 *  \brief This function reads all free LUT2 sessions handles
 *  \return return the number of bytes wirtten to the buffer.
 **************************************************************************************/
static int LUT2_FREE_read_proc (char *page, char **start, off_t off, int count, int *eof, void *data)
{ 
    int           len = 0;
    int           curr_off = 0;
    static struct list_head *   pos = NULL;
    static Uint8  endOfData = 0;
    int           prntRtrn;
    static int    counter;
    static int    sessionType;
    static int    sessionIndex;

    if (endOfData == 1)
    {
        endOfData = 0;
        return 0;
    }
	
    if (off == 0) 
    {
        sessionType = AVALANCHE_PP_SESSIONS_POOL_DATA;
        prntRtrn = snprintf(buff+len, BUFF_FREE( len), "\n**** LUT2 FREE : ****\n");CHECK_SNPRINTF(prntRtrn, len);
        WR_TO_PAGE
        len = 0;
        counter = 1;
        sessionIndex = 0;

        PP_DB_LOCK();
        pos = PP_DB.pool_lut2[sessionType][PP_DB_POOL_FREE].next;

        /*for each LUT2 sessions from list copy handle to global tmp2k*/
        while ( sessionType < AVALANCHE_PP_SESSIONS_POOL_MAX ) 
        {
            LIST_FOR_EACH(pos, &PP_DB.pool_lut2[sessionType][PP_DB_POOL_FREE])
            {
                PP_DB_Session_LUT2_hash_entry_t * entry;
                entry = list_entry(pos, PP_DB_Session_LUT2_hash_entry_t, link);
                tmp2k[sessionIndex++] = entry->handle;

            }
            sessionType++;
            pos = PP_DB.pool_lut2[sessionType][PP_DB_POOL_FREE].next;
        }
        PP_DB_UNLOCK();
        tmp2k[sessionIndex] = -1;
        sessionIndex = 0;
    }
  
    /*print handle for all sessions in global tmp2k*/
    while ( tmp2k[sessionIndex] != -1)
    {
        len = 0;
        prntRtrn = snprintf(buff, BUFF_FREE( len), " %4d ", tmp2k[sessionIndex]); CHECK_SNPRINTF(prntRtrn, len);
        if (counter == 25)
        {
            prntRtrn = snprintf(buff+len, BUFF_FREE( len), "\n"); CHECK_SNPRINTF(prntRtrn, len);
            counter = 0;
        }
        WR_TO_PAGE
        counter++;
        sessionIndex++;
    }

    len = 0;
    prntRtrn = snprintf(buff, BUFF_FREE( len), "\n**** END OF LUT2 FREE ****\n");CHECK_SNPRINTF(prntRtrn, len);
    WR_TO_PAGE

    /* Set next begin of page */ 
    *start = page;
    *eof = 1;
    endOfData = 1;

    /* Return number of bytes writen */
    return curr_off ;
}

/**************************************************************************/
/*! \fn int LUT2_display_session(Int32 session_handle, char *buffer, int size) 
 *  before this func you must call PP_DB_UNLOCK() and after this func call PP_DB_UNLOCK() 
 **************************************************************************
 *  \brief The function displays the session properties.
 *  \param[in] param1 - session handle param2 - pointer to current location in global buffer param3 - free bytes in global buff 
 *  \param[out] baffer - this function write on global buffer
 *  \return number of bytes wirtten to the buffer.
 **************************************************************************/
int LUT2_display_session(Int32 session_handle, char *buffer, int size)
{
    AVALANCHE_PP_SESSION_INFO_t     *session_info;
    AVALANCHE_PP_VPID_INFO_t        *vpid;
    AVALANCHE_PP_PID_t              *pid;
    int                             prntRtrn;
    int                             len = 0;

    /* Get the pointer to the Session Information */
    if (avalanche_pp_session_get_info(session_handle, &session_info) != PP_RC_SUCCESS)
    {
        return 0;
    }

    /* Print the Session Parameters. */
    prntRtrn = snprintf(buffer+len, size-len, "Property : %s\n", session_info->is_routable_session ? "Routed": "Bridged");CHECK_SNPRINTF(prntRtrn, len);
    prntRtrn = snprintf(buffer+len, size-len, "Timeout  : %d sec\n", session_info->session_timeout / 1000000);CHECK_SNPRINTF(prntRtrn, len);
    prntRtrn = snprintf(buffer+len, size-len, "Priority : %d\n", session_info->priority);CHECK_SNPRINTF(prntRtrn, len);

    /* Print the Ingress Session Properties; which include the L2/L3 and L4 properties. */
    prntRtrn = snprintf(buffer+len, size-len,  "\nIngress Properties");CHECK_SNPRINTF(prntRtrn, len);
    if ( session_info->ingress.lookup.LUT1.u.fields.L3.entry_type == AVALANCHE_PP_LUT_ENTRY_L3_GRE)
    {
        prntRtrn = snprintf(buffer+len, size-len,  " (DS GRE - The ingress properties are of the encapsulated packet)");CHECK_SNPRINTF(prntRtrn, len);
    }
    prntRtrn = snprintf(buffer+len, size-len,  "\n------------------\n");CHECK_SNPRINTF(prntRtrn, len);

    avalanche_pp_vpid_get_info(session_info->ingress.vpid_handle, &vpid);
    avalanche_pp_pid_get_info(vpid->parent_pid_handle, &pid);
    if (AVALANCHE_PP_PID_TYPE_DOCSIS == pid->type)
    {
        prntRtrn = snprintf(buffer+len, size-len,  "Ingress VPID = %d [CNI] %s\n", session_info->ingress.vpid_handle, (vpid->flags & AVALANCHE_PP_VPID_FLG_TX_DISBL)?"[DROP]":"");CHECK_SNPRINTF(prntRtrn, len);
    }
    else if (AVALANCHE_PP_PID_TYPE_ETHERNET == pid->type)
    {
        prntRtrn = snprintf(buffer+len, size-len,  "Ingress VPID = %d [ETH] %s\n", session_info->ingress.vpid_handle, (vpid->flags & AVALANCHE_PP_VPID_FLG_TX_DISBL)?"[DROP]":"");CHECK_SNPRINTF(prntRtrn, len);
    }
    else if (AVALANCHE_PP_PID_TYPE_INFRASTRUCTURE == pid->type)
    {
        prntRtrn = snprintf(buffer+len, size-len,  "Ingress VPID = %d [VOICE DSP] %s\n", session_info->ingress.vpid_handle, (vpid->flags & AVALANCHE_PP_VPID_FLG_TX_DISBL)?"[DROP]":"");CHECK_SNPRINTF(prntRtrn, len);
    }
    else
    {
        prntRtrn = snprintf(buffer+len, size-len,  "Ingress VPID = %d %s\n", session_info->ingress.vpid_handle, (vpid->flags & AVALANCHE_PP_VPID_FLG_TX_DISBL)?"[DROP]":"");CHECK_SNPRINTF(prntRtrn, len);
    }
    
    if (session_info->ingress.lookup.LUT1.u.fields.L2.entry_type == AVALANCHE_PP_LUT_ENTRY_L2_ETHERNET)
    {
        prntRtrn = LUT2_display_l2_ingress(&session_info->ingress, buffer+len, size-len);CHECK_SNPRINTF(prntRtrn, len);
    }
    else
    {
        prntRtrn = snprintf(buffer+len, size-len,  "No L2 Properties\n");CHECK_SNPRINTF(prntRtrn, len);
    }

    if (session_info->ingress.lookup.LUT1.u.fields.L3.entry_type == AVALANCHE_PP_LUT_ENTRY_L3_UNDEFINED)
    {
        prntRtrn = snprintf(buffer+len, size-len,  "No L3/L4 Properties\n");CHECK_SNPRINTF(prntRtrn, len);
    }
    else
    {
        if ((session_info->ingress.lookup.LUT1.u.fields.L3.entry_type == AVALANCHE_PP_LUT_ENTRY_L3_IPV6) ||
            ((session_info->ingress.lookup.LUT1.u.fields.L3.entry_type != AVALANCHE_PP_LUT_ENTRY_L3_IPV4) &&
             (session_info->ingress.lookup.LUT2.u.fields.WAN_addr_IP.v6[3] != 0)))
        {
            prntRtrn = snprintf(buffer+len, size-len,  "IP Version   = IPv6\n");CHECK_SNPRINTF(prntRtrn, len);
            prntRtrn = LUT2_display_ipv6_ingress(&session_info->ingress, buffer+len, size-len);CHECK_SNPRINTF(prntRtrn, len);
        }
        else
        {
            prntRtrn = snprintf(buffer+len, size-len,  "IP Version   = IPv4\n");CHECK_SNPRINTF(prntRtrn, len);
            prntRtrn = LUT2_display_ipv4_ingress(&session_info->ingress, buffer+len, size-len);CHECK_SNPRINTF(prntRtrn, len);
        }
    }

    prntRtrn = snprintf(buffer+len, size-len,  "\nEgress Properties");CHECK_SNPRINTF(prntRtrn, len);
    prntRtrn = snprintf(buffer+len, size-len,  "\n-----------------\n");CHECK_SNPRINTF(prntRtrn, len);

    /* Print the Ingress Session Properties; which include the L2/L3 and L4 properties. */
    avalanche_pp_vpid_get_info(session_info->egress.vpid_handle, &vpid);
    avalanche_pp_pid_get_info (vpid->parent_pid_handle, &pid);
    if (AVALANCHE_PP_PID_TYPE_DOCSIS == pid->type)
    {
        prntRtrn = snprintf(buffer+len, size-len,  "Egress VPID  = %d [CNI] %s\n", session_info->egress.vpid_handle, (vpid->flags & AVALANCHE_PP_VPID_FLG_TX_DISBL)?"[DROP]":"");CHECK_SNPRINTF(prntRtrn, len);
    }
    else if (AVALANCHE_PP_PID_TYPE_ETHERNET == pid->type)
    {
        prntRtrn = snprintf(buffer+len, size-len,  "Egress VPID  = %d [ETH] %s\n", session_info->egress.vpid_handle, (vpid->flags & AVALANCHE_PP_VPID_FLG_TX_DISBL)?"[DROP]":"");CHECK_SNPRINTF(prntRtrn, len);
    }
    else if (AVALANCHE_PP_PID_TYPE_INFRASTRUCTURE == pid->type)
    {
        prntRtrn = snprintf(buffer+len, size-len,  "Egress VPID  = %d [VOICE DSP] %s\n", session_info->egress.vpid_handle, (vpid->flags & AVALANCHE_PP_VPID_FLG_TX_DISBL)?"[DROP]":"");CHECK_SNPRINTF(prntRtrn, len);
    }
    else
    {
        prntRtrn = snprintf(buffer+len, size-len,  "Egress VPID  = %d %s\n", session_info->egress.vpid_handle, (vpid->flags & AVALANCHE_PP_VPID_FLG_TX_DISBL)?"[DROP]":"");CHECK_SNPRINTF(prntRtrn, len);
    }

    if (session_info->egress.l2_packet_type == AVALANCHE_PP_LUT_ENTRY_L2_ETHERNET)
    {
        prntRtrn = LUT2_display_l2_egress(&session_info->egress, buffer+len, size-len);CHECK_SNPRINTF(prntRtrn, len);
    }
    else
    {
        prntRtrn = snprintf(buffer+len, size-len,  "No L2 Properties\n");CHECK_SNPRINTF(prntRtrn, len);
    }

    if (session_info->egress.l3_packet_type == AVALANCHE_PP_LUT_ENTRY_L3_IPV4)
    {
        prntRtrn = snprintf(buffer+len, size-len,  "IP Version   = IPv4\n");CHECK_SNPRINTF(prntRtrn, len);
        prntRtrn = LUT2_display_ipv4_egress(&session_info->egress, buffer+len, size-len);CHECK_SNPRINTF(prntRtrn, len);
    }
    else if (session_info->egress.l3_packet_type == AVALANCHE_PP_LUT_ENTRY_L3_IPV6)
    {
        prntRtrn = snprintf(buffer+len, size-len,  "IP Version   = IPv6\n");CHECK_SNPRINTF(prntRtrn, len);
        prntRtrn = LUT2_display_ipv6_egress(&session_info->egress, buffer+len, size-len);CHECK_SNPRINTF(prntRtrn, len);
    }
    else
    {
        prntRtrn = snprintf(buffer+len, size-len,  "No L3/L4 Properties\n");CHECK_SNPRINTF(prntRtrn, len);
    }

    /*tdox information*/
    if (session_info->egress.enable & AVALANCHE_PP_EGRESS_FIELD_ENABLE_TDOX_ENABLED)
    {
        prntRtrn = snprintf(buffer+len, size-len,  "\nTdox Properties");CHECK_SNPRINTF(prntRtrn, len);
        prntRtrn = snprintf(buffer+len, size-len,  "\n-----------------\n");CHECK_SNPRINTF(prntRtrn, len);
        prntRtrn = snprintf(buffer+len, size-len,  "pkts_forwarded   : %u\n", session_info->tdox_stats.packets_forwarded);CHECK_SNPRINTF(prntRtrn, len);
        prntRtrn = snprintf(buffer+len, size-len,  "bytes_forwarded  : %u\n", session_info->tdox_stats.bytes_forwarded);CHECK_SNPRINTF(prntRtrn, len);
        prntRtrn = snprintf(buffer+len, size-len,  "last_update_time : 0x%08X\n", session_info->tdox_stats.last_update_time);CHECK_SNPRINTF(prntRtrn, len);
    }

    if (len <= size)
    {
        return len; 
    }
    return 0;
}

/**************************************************************************/
/*! \fn int LUT2_display_session(Int32 session_handle, char *buffer, int size) 
 *  before this func you must call PP_DB_UNLOCK() and after this func call PP_DB_UNLOCK() 
 **************************************************************************
 *  \brief The function displays the session properties.
 *  \param[in] param1 - session handle param2 - pointer to current location in global buffer param3 - free bytes in global buff 
 *  \param[out] baffer - this function write on global buffer
 *  \return number of bytes wirtten to the buffer.
 **************************************************************************/
int display_session_information(Int32 session_handle, char *buffer, int size)
{
    AVALANCHE_PP_SESSION_INFO_t     *session_info;
    AVALANCHE_PP_INGRESS_SESSION_PROPERTY_t* ingress_property;
    AVALANCHE_PP_EGRESS_SESSION_PROPERTY_t* egress_property;
    AVALANCHE_PP_VPID_INFO_t        *vpid;
    AVALANCHE_PP_PID_t              *pid;
    Uint32                          prntRtrn;
    Uint32                          len = 0;


    /* Get the pointer to the Session Information */
    if (avalanche_pp_session_get_info(session_handle, &session_info) != PP_RC_SUCCESS)
    {
        return 0;
    }

    /* General information */
    prntRtrn = snprintf(buffer+len, size-len, "\nGeneral Information\n");CHECK_SNPRINTF(prntRtrn, len);
    prntRtrn = snprintf(buffer+len, size-len, "===================\n");CHECK_SNPRINTF(prntRtrn, len);
    prntRtrn = snprintf(buffer+len, size-len, "session_handle   : %d\n", session_info->session_handle);CHECK_SNPRINTF(prntRtrn, len);
    prntRtrn = snprintf(buffer+len, size-len, "session_timeout  : %d sec\n", session_info->session_timeout / 1000000);CHECK_SNPRINTF(prntRtrn, len);
    prntRtrn = snprintf(buffer+len, size-len, "priority         : %d\n", session_info->priority);CHECK_SNPRINTF(prntRtrn, len);
    prntRtrn = snprintf(buffer+len, size-len, "cluster          : %d\n", session_info->cluster);CHECK_SNPRINTF(prntRtrn, len);
    prntRtrn = snprintf(buffer+len, size-len, "is_routable      : %d\n", session_info->is_routable_session);CHECK_SNPRINTF(prntRtrn, len);
    prntRtrn = snprintf(buffer+len, size-len, "session_pool     : %d\n", session_info->session_pool);CHECK_SNPRINTF(prntRtrn, len);
    
    /* Ingress Parameters */
    ingress_property = &session_info->ingress;
    prntRtrn = snprintf(buffer+len, size-len, "\nIngress Information\n");CHECK_SNPRINTF(prntRtrn, len);
    prntRtrn = snprintf(buffer+len, size-len, "===================\n");CHECK_SNPRINTF(prntRtrn, len);
    avalanche_pp_vpid_get_info(ingress_property->vpid_handle, &vpid);
    avalanche_pp_pid_get_info(vpid->parent_pid_handle, &pid);
    if (AVALANCHE_PP_PID_TYPE_DOCSIS == pid->type) 
    {
        prntRtrn = snprintf(buffer+len, size-len, "vpid_handle      : %d [CNI] %s\n", ingress_property->vpid_handle, (vpid->flags & AVALANCHE_PP_VPID_FLG_TX_DISBL)?"[DROP]":"");CHECK_SNPRINTF(prntRtrn, len);
    }
    else if (AVALANCHE_PP_PID_TYPE_ETHERNET == pid->type)
    {
        prntRtrn = snprintf(buffer+len, size-len, "vpid_handle      : %d [ETH] %s\n", ingress_property->vpid_handle, (vpid->flags & AVALANCHE_PP_VPID_FLG_TX_DISBL)?"[DROP]":"");CHECK_SNPRINTF(prntRtrn, len);
    }
    else if (AVALANCHE_PP_PID_TYPE_INFRASTRUCTURE == pid->type)
    {
        prntRtrn = snprintf(buffer+len, size-len, "vpid_handle      : %d [VOICE DSP] %s\n", ingress_property->vpid_handle, (vpid->flags & AVALANCHE_PP_VPID_FLG_TX_DISBL)?"[DROP]":"");CHECK_SNPRINTF(prntRtrn, len);
    }
    else
    {
        prntRtrn = snprintf(buffer+len, size-len, "vpid_handle      : %d [Unknown] %s\n", ingress_property->vpid_handle, (vpid->flags & AVALANCHE_PP_VPID_FLG_TX_DISBL)?"[DROP]":"");CHECK_SNPRINTF(prntRtrn, len);
    }

    prntRtrn = snprintf(buffer+len, size-len, "dev_type         : %d [%s]\n", ingress_property->dev_type, ingress_property->dev_type==AVALANCHE_PP_DEV_TYPE_WAN?"WAN":(ingress_property->dev_type==AVALANCHE_PP_DEV_TYPE_LAN?"LAN":"Unknown"));CHECK_SNPRINTF(prntRtrn, len);

    prntRtrn = snprintf(buffer+len, size-len, "\n    LUT1 L2 Information\n");CHECK_SNPRINTF(prntRtrn, len);
    prntRtrn = snprintf(buffer+len, size-len, "    -------------------\n");CHECK_SNPRINTF(prntRtrn, len);

    if (ingress_property->lookup.LUT1.u.fields.L2.enable_flags & AVALANCHE_PP_LUT1_FIELD_ENABLE_MAC_DST)
    {
        prntRtrn = snprintf(buffer+len, size-len, "    dstmac           : %02X:%02X:%02X:%02X:%02X:%02X\n", 
                            ingress_property->lookup.LUT1.u.fields.L2.dstmac[0], 
                            ingress_property->lookup.LUT1.u.fields.L2.dstmac[1], 
                            ingress_property->lookup.LUT1.u.fields.L2.dstmac[2], 
                            ingress_property->lookup.LUT1.u.fields.L2.dstmac[3], 
                            ingress_property->lookup.LUT1.u.fields.L2.dstmac[4], 
                            ingress_property->lookup.LUT1.u.fields.L2.dstmac[5]);
                            CHECK_SNPRINTF(prntRtrn, len);
    }
    else
    {
        prntRtrn = snprintf(buffer+len, size-len, "    dstmac           : XXX\n");CHECK_SNPRINTF(prntRtrn, len);
    }
    if (ingress_property->lookup.LUT1.u.fields.L2.enable_flags & AVALANCHE_PP_LUT1_FIELD_ENABLE_MAC_SRC)
    {
        prntRtrn = snprintf(buffer+len, size-len, "    srcmac           : %02X:%02X:%02X:%02X:%02X:%02X\n", 
                            ingress_property->lookup.LUT1.u.fields.L2.srcmac[0], 
                            ingress_property->lookup.LUT1.u.fields.L2.srcmac[1], 
                            ingress_property->lookup.LUT1.u.fields.L2.srcmac[2], 
                            ingress_property->lookup.LUT1.u.fields.L2.srcmac[3], 
                            ingress_property->lookup.LUT1.u.fields.L2.srcmac[4], 
                            ingress_property->lookup.LUT1.u.fields.L2.srcmac[5]);
                            CHECK_SNPRINTF(prntRtrn, len);
    }
    else
    {
        prntRtrn = snprintf(buffer+len, size-len, "    srcmac           : XXX\n");CHECK_SNPRINTF(prntRtrn, len);
    }
    if (ingress_property->lookup.LUT1.u.fields.L2.enable_flags & AVALANCHE_PP_LUT1_FIELD_ENABLE_ETH_TYPE)
    {
        prntRtrn = snprintf(buffer+len, size-len, "    eth_type         : 0x%04X\n", ingress_property->lookup.LUT1.u.fields.L2.eth_type);CHECK_SNPRINTF(prntRtrn, len);
    }
    else
    {
        prntRtrn = snprintf(buffer+len, size-len, "    eth_type         : XXX\n");CHECK_SNPRINTF(prntRtrn, len);
    }
    if (ingress_property->lookup.LUT1.u.fields.L2.enable_flags & AVALANCHE_PP_LUT1_FIELD_ENABLE_L2_ENTRY_TYPE)
    {
        prntRtrn = snprintf(buffer+len, size-len, "    entry_type       : %d [%s]\n", 
                            ingress_property->lookup.LUT1.u.fields.L2.entry_type,
                            ingress_property->lookup.LUT1.u.fields.L2.entry_type==AVALANCHE_PP_LUT_ENTRY_L2_ETHERNET?"ETH":"Undefined");
                            CHECK_SNPRINTF(prntRtrn, len);
    }
    else
    {
        prntRtrn = snprintf(buffer+len, size-len, "    entry_type       : XXX\n");CHECK_SNPRINTF(prntRtrn, len);
    }
    prntRtrn = snprintf(buffer+len, size-len, "    pid_handle       : %d\n", ingress_property->lookup.LUT1.u.fields.L2.pid_handle);CHECK_SNPRINTF(prntRtrn, len);
    prntRtrn = snprintf(buffer+len, size-len, "    enable_flags     : 0x%02X [ ", ingress_property->lookup.LUT1.u.fields.L2.enable_flags);CHECK_SNPRINTF(prntRtrn, len);

    if (ingress_property->lookup.LUT1.u.fields.L2.enable_flags & AVALANCHE_PP_LUT1_FIELD_ENABLE_PID)
    {
        prntRtrn = snprintf(buffer + len, size - len, "PID "); CHECK_SNPRINTF(prntRtrn, len);
    }
    if (ingress_property->lookup.LUT1.u.fields.L2.enable_flags & AVALANCHE_PP_LUT1_FIELD_ENABLE_MAC_DST)
    {
        prntRtrn = snprintf(buffer + len, size - len, "MAC_DST "); CHECK_SNPRINTF(prntRtrn, len);
    }
    if (ingress_property->lookup.LUT1.u.fields.L2.enable_flags & AVALANCHE_PP_LUT1_FIELD_ENABLE_MAC_SRC)
    {
        prntRtrn = snprintf(buffer + len, size - len, "MAC_SRC "); CHECK_SNPRINTF(prntRtrn, len);
    }
    if (ingress_property->lookup.LUT1.u.fields.L2.enable_flags & AVALANCHE_PP_LUT1_FIELD_ENABLE_ETH_TYPE)
    {
        prntRtrn = snprintf(buffer + len, size - len, "ETH_TYPE "); CHECK_SNPRINTF(prntRtrn, len);
    }
    if (ingress_property->lookup.LUT1.u.fields.L2.enable_flags & AVALANCHE_PP_LUT1_FIELD_ENABLE_L2_ENTRY_TYPE)
    {
        prntRtrn = snprintf(buffer + len, size - len, "L2_ENTRY_TYPE "); CHECK_SNPRINTF(prntRtrn, len);
    }

    prntRtrn = snprintf(buffer + len, size - len, "]\n"); CHECK_SNPRINTF(prntRtrn, len);
    
    prntRtrn = snprintf(buffer+len, size-len, "\n    LUT1 L3 Information\n");CHECK_SNPRINTF(prntRtrn, len);
    prntRtrn = snprintf(buffer+len, size-len, "    -------------------\n");CHECK_SNPRINTF(prntRtrn, len);

    if ((ingress_property->lookup.LUT1.u.fields.L3.entry_type == AVALANCHE_PP_LUT_ENTRY_L3_IPV6) ||
        ((ingress_property->lookup.LUT1.u.fields.L3.entry_type != AVALANCHE_PP_LUT_ENTRY_L3_IPV4) &&
         (ingress_property->lookup.LUT2.u.fields.WAN_addr_IP.v6[3] != 0)))
    {
        const   Int8* buf;
        Int8    ipv6addr[64];

        if (ingress_property->lookup.LUT1.u.fields.L3.enable_flags & AVALANCHE_PP_LUT1_FIELD_ENABLE_LAN_IPv6)
        {
            buf = LUT2_display_ipv6_addr((void*)ingress_property->lookup.LUT1.u.fields.L3.LAN_addr_IP.v6, ipv6addr, sizeof(ipv6addr));
            if (buf != NULL)
            {
                prntRtrn = snprintf(buffer+len, size-len,  "    LAN_addr_IP      : %s\n", buf);CHECK_SNPRINTF(prntRtrn, len);
            }
            else
            {
                prntRtrn = snprintf(buffer+len, size-len,  "    LAN_addr_IP      : XXX\n");CHECK_SNPRINTF(prntRtrn, len);
            }
        }
        else
        {
            prntRtrn = snprintf(buffer+len, size-len,  "    LAN_addr_IP      : XXX\n");CHECK_SNPRINTF(prntRtrn, len);
        }
    }
    else
    {
        if (ingress_property->lookup.LUT1.u.fields.L3.enable_flags & AVALANCHE_PP_LUT1_FIELD_ENABLE_LAN_IPv4)
        {
            Uint8 *ipPtr = (Uint8 *)&ingress_property->lookup.LUT1.u.fields.L3.LAN_addr_IP.v4;
            prntRtrn = snprintf(buffer+len, size-len,  "    LAN_addr_IP      : 0x%08X [ %d.%d.%d.%d ]\n", ingress_property->lookup.LUT1.u.fields.L3.LAN_addr_IP.v4, ipPtr[0], ipPtr[1], ipPtr[2], ipPtr[3]);CHECK_SNPRINTF(prntRtrn, len);
        }
        else
        {
            prntRtrn = snprintf(buffer+len, size-len,  "    LAN_addr_IP      : XXX\n");CHECK_SNPRINTF(prntRtrn, len);
        }
    }
    if (ingress_property->lookup.LUT1.u.fields.L3.enable_flags & AVALANCHE_PP_LUT1_FIELD_ENABLE_L3_ENTRY_TYPE)
    {
        prntRtrn = snprintf(buffer+len, size-len, "    entry_type       : %d [%s]\n", 
                            ingress_property->lookup.LUT1.u.fields.L3.entry_type,
                            ingress_property->lookup.LUT1.u.fields.L3.entry_type==AVALANCHE_PP_LUT_ENTRY_L3_IPV4?"IPv4":
                            (ingress_property->lookup.LUT1.u.fields.L3.entry_type==AVALANCHE_PP_LUT_ENTRY_L3_IPV6?"IPv6":
                            (ingress_property->lookup.LUT1.u.fields.L3.entry_type==AVALANCHE_PP_LUT_ENTRY_L3_DSLITE?"Ds-Lite":
                            ((ingress_property->lookup.LUT1.u.fields.L3.entry_type==AVALANCHE_PP_LUT_ENTRY_L3_GRE?"GRE":"Undefined")))));
                            CHECK_SNPRINTF(prntRtrn, len);
    }
    else
    {
        prntRtrn = snprintf(buffer+len, size-len, "    entry_type       : XXX\n");CHECK_SNPRINTF(prntRtrn, len);
    }
    if (ingress_property->lookup.LUT1.u.fields.L3.enable_flags & AVALANCHE_PP_LUT1_FIELD_ENABLE_IP_PROTOCOL)
    {
        prntRtrn = snprintf(buffer+len, size-len, "    ip_protocol      : 0x%02X ", ingress_property->lookup.LUT1.u.fields.L3.ip_protocol);CHECK_SNPRINTF(prntRtrn, len);
        switch (ingress_property->lookup.LUT1.u.fields.L3.ip_protocol)
        {
        case IPPROTO_IPIP:
            prntRtrn = snprintf(buffer+len, size-len,  "[IPinIP]\n");CHECK_SNPRINTF(prntRtrn, len);
            break;
        case IPPROTO_TCP:
            prntRtrn = snprintf(buffer+len, size-len,  "[TCP]\n");CHECK_SNPRINTF(prntRtrn, len);
            break;
        case IPPROTO_UDP:
            prntRtrn = snprintf(buffer+len, size-len,  "[UDP]\n");CHECK_SNPRINTF(prntRtrn, len);
            break;
        case IPPROTO_GRE:
            prntRtrn = snprintf(buffer+len, size-len,  "[GRE]\n");CHECK_SNPRINTF(prntRtrn, len);
            break;
        case IPPROTO_IPV6:
            prntRtrn = snprintf(buffer+len, size-len,  "[IPv6-in-IPv4 tunnelling]\n");CHECK_SNPRINTF(prntRtrn, len);
            break;
        default:
            prntRtrn = snprintf(buffer+len, size-len,  "[???]\n");CHECK_SNPRINTF(prntRtrn, len);
            break;
        }
    }
    else
    {
        prntRtrn = snprintf(buffer+len, size-len, "    ip_protocol      : XXX\n");CHECK_SNPRINTF(prntRtrn, len);
    }
    prntRtrn = snprintf(buffer+len, size-len, "    enable_flags     : 0x%02X [ ", ingress_property->lookup.LUT1.u.fields.L3.enable_flags);CHECK_SNPRINTF(prntRtrn, len);
    if (ingress_property->lookup.LUT1.u.fields.L3.enable_flags & AVALANCHE_PP_LUT1_FIELD_ENABLE_LAN_IPv6)
    {
        prntRtrn = snprintf(buffer + len, size - len, "LAN_IPv6 "); CHECK_SNPRINTF(prntRtrn, len);
    }
    else if (ingress_property->lookup.LUT1.u.fields.L3.enable_flags & AVALANCHE_PP_LUT1_FIELD_ENABLE_LAN_IPv4)
    {
        prntRtrn = snprintf(buffer + len, size - len, "LAN_IPv4 "); CHECK_SNPRINTF(prntRtrn, len);
    }
    if (ingress_property->lookup.LUT1.u.fields.L3.enable_flags & AVALANCHE_PP_LUT1_FIELD_ENABLE_IP_PROTOCOL)
    {
        prntRtrn = snprintf(buffer + len, size - len, "IP_PROTOCOL "); CHECK_SNPRINTF(prntRtrn, len);
    }
    if (ingress_property->lookup.LUT1.u.fields.L3.enable_flags & AVALANCHE_PP_LUT1_FIELD_ENABLE_PPoE_SESS_ID)
    {
        prntRtrn = snprintf(buffer + len, size - len, "PPoE_SESS_ID "); CHECK_SNPRINTF(prntRtrn, len);
    }
    if (ingress_property->lookup.LUT1.u.fields.L3.enable_flags & AVALANCHE_PP_LUT1_FIELD_ENABLE_L3_ENTRY_TYPE)
    {
        prntRtrn = snprintf(buffer + len, size - len, "L3_ENTRY_TYPE "); CHECK_SNPRINTF(prntRtrn, len);
    }
    prntRtrn = snprintf(buffer + len, size - len, "]\n"); CHECK_SNPRINTF(prntRtrn, len);
    
    prntRtrn = snprintf(buffer+len, size-len, "\n    LUT2 Information\n");CHECK_SNPRINTF(prntRtrn, len);
    prntRtrn = snprintf(buffer+len, size-len, "    ----------------\n");CHECK_SNPRINTF(prntRtrn, len);

    if ((session_info->ingress.lookup.LUT1.u.fields.L3.entry_type == AVALANCHE_PP_LUT_ENTRY_L3_IPV6) ||
        ((session_info->ingress.lookup.LUT1.u.fields.L3.entry_type != AVALANCHE_PP_LUT_ENTRY_L3_IPV4) &&
        (session_info->ingress.lookup.LUT2.u.fields.WAN_addr_IP.v6[3] != 0)))
    {
        const   Int8* buf;
        Int8    ipv6addr[64];

        buf = LUT2_display_ipv6_addr((void*)ingress_property->lookup.LUT2.u.fields.WAN_addr_IP.v6, ipv6addr, sizeof(ipv6addr));
        if (buf != NULL)
        {
            prntRtrn = snprintf(buffer+len, size-len,  "    WAN_addr_IP      : %s\n", buf);CHECK_SNPRINTF(prntRtrn, len);
        }
    }
    else
    {
        Uint8 *ipPtr = (Uint8 *)&ingress_property->lookup.LUT2.u.fields.WAN_addr_IP.v4;
        prntRtrn = snprintf(buffer+len, size-len,  "    WAN_addr_IP      : 0x%08X [ %d.%d.%d.%d ]\n", ingress_property->lookup.LUT2.u.fields.WAN_addr_IP.v4, ipPtr[0], ipPtr[1], ipPtr[2], ipPtr[3]);CHECK_SNPRINTF(prntRtrn, len);
    }
    if ((ingress_property->lookup.LUT2.u.fields.entry_type == AVALANCHE_PP_LUT_ENTRY_L3_DSLITE) && 
        (ingress_property->lookup.LUT2.u.fields.enable_flags & AVALANCHE_PP_LUT2_FIELD_ENABLE_DSLITE_IPV4))
    {
        Uint8 *ipPtr = (Uint8 *)&ingress_property->lookup.LUT2.u.fields.IP.v4_dsLite;
        prntRtrn = snprintf(buffer+len, size-len, "    Ds-Lite DST IP   : 0x%08X [ %d.%d.%d.%d ]\n", ingress_property->lookup.LUT2.u.fields.IP.v4_dsLite, ipPtr[0], ipPtr[1], ipPtr[2], ipPtr[3]);CHECK_SNPRINTF(prntRtrn, len);
    }
    else if (ingress_property->lookup.LUT2.u.fields.enable_flags & AVALANCHE_PP_LUT2_FIELD_ENABLE_IPV6_FLOW)
    {
        Uint8 *flowLabel = (Uint8 *)&ingress_property->lookup.LUT2.u.fields.IP.v6_FlowLabel[0];
        prntRtrn = snprintf(buffer+len, size-len, "    FlowLabel        : 0x%02X.%02X.%02X\n", flowLabel[0], flowLabel[1], flowLabel[2]);CHECK_SNPRINTF(prntRtrn, len);
    }
    if (ingress_property->lookup.LUT2.u.fields.enable_flags & AVALANCHE_PP_LUT2_FIELD_ENABLE_1ST_VLAN)
    {
        prntRtrn = snprintf(buffer+len, size-len, "    firstVLAN        : 0x%04X\n", ingress_property->lookup.LUT2.u.fields.firstVLAN);CHECK_SNPRINTF(prntRtrn, len);
    }
    if (ingress_property->lookup.LUT2.u.fields.enable_flags & AVALANCHE_PP_LUT2_FIELD_ENABLE_2ND_VLAN)
    {
        prntRtrn = snprintf(buffer+len, size-len, "    secondVLAN       : 0x%04X\n", ingress_property->lookup.LUT2.u.fields.secondVLAN);CHECK_SNPRINTF(prntRtrn, len);
    }
    prntRtrn = snprintf(buffer+len, size-len, "    L4_SrcPort       : 0x%04X [%d]\n", ingress_property->lookup.LUT2.u.fields.L4_SrcPort, ingress_property->lookup.LUT2.u.fields.L4_SrcPort);CHECK_SNPRINTF(prntRtrn, len);
    prntRtrn = snprintf(buffer+len, size-len, "    L4_DstPort       : 0x%04X [%d]\n", ingress_property->lookup.LUT2.u.fields.L4_DstPort, ingress_property->lookup.LUT2.u.fields.L4_DstPort);CHECK_SNPRINTF(prntRtrn, len);
    prntRtrn = snprintf(buffer+len, size-len, "    TOS              : 0x%02X\n", ingress_property->lookup.LUT2.u.fields.TOS);CHECK_SNPRINTF(prntRtrn, len);
    prntRtrn = snprintf(buffer+len, size-len, "    LUT1_key         : %d\n", ingress_property->lookup.LUT2.u.fields.LUT1_key);CHECK_SNPRINTF(prntRtrn, len);
    prntRtrn = snprintf(buffer+len, size-len, "    entry_type       : %d [%s]\n", 
                        ingress_property->lookup.LUT2.u.fields.entry_type,
                        ingress_property->lookup.LUT2.u.fields.entry_type==AVALANCHE_PP_LUT_ENTRY_L3_IPV4?"IPv4":
                        (ingress_property->lookup.LUT2.u.fields.entry_type==AVALANCHE_PP_LUT_ENTRY_L3_IPV6?"IPv6":
                        (ingress_property->lookup.LUT2.u.fields.entry_type==AVALANCHE_PP_LUT_ENTRY_L3_DSLITE?"Ds-Lite":
                        ((ingress_property->lookup.LUT2.u.fields.entry_type==AVALANCHE_PP_LUT_ENTRY_L3_GRE?"GRE":"Undefined")))));
                        CHECK_SNPRINTF(prntRtrn, len);
    prntRtrn = snprintf(buffer+len, size-len, "    enable_flags     : 0x%02X [ ", ingress_property->lookup.LUT2.u.fields.enable_flags);CHECK_SNPRINTF(prntRtrn, len);
    if (ingress_property->lookup.LUT2.u.fields.enable_flags & AVALANCHE_PP_LUT2_FIELD_ENABLE_WAN_IP)
    {
        prntRtrn = snprintf(buffer+len, size-len, "WAN_IP ");CHECK_SNPRINTF(prntRtrn, len);
    }
    if ((ingress_property->lookup.LUT2.u.fields.entry_type == AVALANCHE_PP_LUT_ENTRY_L3_DSLITE) && 
        (ingress_property->lookup.LUT2.u.fields.enable_flags & AVALANCHE_PP_LUT2_FIELD_ENABLE_DSLITE_IPV4))
    {
        prntRtrn = snprintf(buffer+len, size-len, "DSLITE_IPV4 ");CHECK_SNPRINTF(prntRtrn, len);
    }
    else if (ingress_property->lookup.LUT2.u.fields.enable_flags & AVALANCHE_PP_LUT2_FIELD_ENABLE_IPV6_FLOW)
    {
        prntRtrn = snprintf(buffer+len, size-len, "IPV6_FLOW ");CHECK_SNPRINTF(prntRtrn, len);
    }
    if (ingress_property->lookup.LUT2.u.fields.enable_flags & AVALANCHE_PP_LUT2_FIELD_ENABLE_1ST_VLAN)
    {
        prntRtrn = snprintf(buffer+len, size-len, "1ST_VLAN ");CHECK_SNPRINTF(prntRtrn, len);
    }
    if (ingress_property->lookup.LUT2.u.fields.enable_flags & AVALANCHE_PP_LUT2_FIELD_ENABLE_2ND_VLAN)
    {
        prntRtrn = snprintf(buffer+len, size-len, "2ND_VLAN ");CHECK_SNPRINTF(prntRtrn, len);
    }
    if (ingress_property->lookup.LUT2.u.fields.enable_flags & AVALANCHE_PP_LUT2_FIELD_ENABLE_SRC_PORT)
    {
        prntRtrn = snprintf(buffer+len, size-len, "SRC_PORT ");CHECK_SNPRINTF(prntRtrn, len);
    }
    if (ingress_property->lookup.LUT2.u.fields.enable_flags & AVALANCHE_PP_LUT2_FIELD_ENABLE_DST_PORT)
    {
        prntRtrn = snprintf(buffer+len, size-len, "DST_PORT ");CHECK_SNPRINTF(prntRtrn, len);
    }
    if (ingress_property->lookup.LUT2.u.fields.enable_flags & AVALANCHE_PP_LUT2_FIELD_ENABLE_IP_TOS)
    {
        prntRtrn = snprintf(buffer+len, size-len, "IP_TOS ");CHECK_SNPRINTF(prntRtrn, len);
    }
    prntRtrn = snprintf(buffer+len, size-len, "]\n");CHECK_SNPRINTF(prntRtrn, len);

    prntRtrn = snprintf(buffer+len, size-len, "\nEgress Information\n");CHECK_SNPRINTF(prntRtrn, len);
    prntRtrn = snprintf(buffer+len, size-len, "==================\n");CHECK_SNPRINTF(prntRtrn, len);
    egress_property = &session_info->egress;
    avalanche_pp_vpid_get_info(egress_property->vpid_handle, &vpid);
    avalanche_pp_pid_get_info (vpid->parent_pid_handle, &pid);
    if (AVALANCHE_PP_PID_TYPE_DOCSIS == pid->type) 
    {
        prntRtrn = snprintf(buffer+len, size-len, "vpid_handle      : %d [CNI] %s\n", egress_property->vpid_handle, (vpid->flags & AVALANCHE_PP_VPID_FLG_TX_DISBL)?"[DROP]":"");CHECK_SNPRINTF(prntRtrn, len);
    }
    else if (AVALANCHE_PP_PID_TYPE_ETHERNET == pid->type)
    {
        prntRtrn = snprintf(buffer+len, size-len, "vpid_handle      : %d [ETH] %s\n", egress_property->vpid_handle, (vpid->flags & AVALANCHE_PP_VPID_FLG_TX_DISBL)?"[DROP]":"");CHECK_SNPRINTF(prntRtrn, len);
    }
    else if (AVALANCHE_PP_PID_TYPE_INFRASTRUCTURE == pid->type)
    {
        prntRtrn = snprintf(buffer+len, size-len, "vpid_handle      : %d [VOICE DSP] %s\n", egress_property->vpid_handle, (vpid->flags & AVALANCHE_PP_VPID_FLG_TX_DISBL)?"[DROP]":"");CHECK_SNPRINTF(prntRtrn, len);
    }
    else
    {
        prntRtrn = snprintf(buffer+len, size-len, "vpid_handle      : %d [Unknown] %s\n", egress_property->vpid_handle, (vpid->flags & AVALANCHE_PP_VPID_FLG_TX_DISBL)?"[DROP]":"");CHECK_SNPRINTF(prntRtrn, len);
    }

    prntRtrn = snprintf(buffer+len, size-len, "dev_type         : %d [%s]\n", egress_property->dev_type, egress_property->dev_type==AVALANCHE_PP_DEV_TYPE_WAN?"WAN":(egress_property->dev_type==AVALANCHE_PP_DEV_TYPE_LAN?"LAN":"Unknown"));CHECK_SNPRINTF(prntRtrn, len);
    prntRtrn = snprintf(buffer+len, size-len, "enable           : 0x%04X [ ", egress_property->enable);CHECK_SNPRINTF(prntRtrn, len);
    if (egress_property->enable & AVALANCHE_PP_EGRESS_FIELD_ENABLE_L2)
    {
        prntRtrn = snprintf(buffer + len, size - len, "L2 "); CHECK_SNPRINTF(prntRtrn, len);
    }
    if (egress_property->enable & AVALANCHE_PP_EGRESS_FIELD_ENABLE_VLAN)
    {
        prntRtrn = snprintf(buffer + len, size - len, "VLAN "); CHECK_SNPRINTF(prntRtrn, len);
    }
    if (egress_property->enable & AVALANCHE_PP_EGRESS_FIELD_ENABLE_IP)
    {
        prntRtrn = snprintf(buffer + len, size - len, "IP "); CHECK_SNPRINTF(prntRtrn, len);
    }
    if (egress_property->enable & AVALANCHE_PP_EGRESS_FIELD_ENABLE_L4)
    {
        prntRtrn = snprintf(buffer + len, size - len, "L4 "); CHECK_SNPRINTF(prntRtrn, len);
    }
    if (egress_property->enable & AVALANCHE_PP_EGRESS_FIELD_ENABLE_PSI)
    {
        prntRtrn = snprintf(buffer + len, size - len, "PSI "); CHECK_SNPRINTF(prntRtrn, len);
    }
    if (egress_property->enable & AVALANCHE_PP_EGRESS_FIELD_ENABLE_ENCAPSULATION)
    {
        prntRtrn = snprintf(buffer + len, size - len, "ENCAP "); CHECK_SNPRINTF(prntRtrn, len);
    }
    if (egress_property->enable & AVALANCHE_PP_EGRESS_FIELD_ENABLE_TDOX_ENABLED)
    {
        prntRtrn = snprintf(buffer + len, size - len, "TDOX "); CHECK_SNPRINTF(prntRtrn, len);
    }
    if (egress_property->enable & AVALANCHE_PP_EGRESS_FIELD_ENABLE_TDOX_SKIP_TIMESTAMP)
    {
        prntRtrn = snprintf(buffer + len, size - len, "TDOX_SKIP_TIME "); CHECK_SNPRINTF(prntRtrn, len);
    }
    if (egress_property->enable & AVALANCHE_PP_EGRESS_FIELD_ENABLE_TCP_SYN)
    {
        prntRtrn = snprintf(buffer + len, size - len, "TCP_SYN "); CHECK_SNPRINTF(prntRtrn, len);
    }
    prntRtrn = snprintf(buffer + len, size - len, "]\n"); CHECK_SNPRINTF(prntRtrn, len);

    if (egress_property->enable & AVALANCHE_PP_EGRESS_FIELD_ENABLE_PSI)
    {
        prntRtrn = snprintf(buffer+len, size-len, "us_psi_word      : 0x%08X\n", egress_property->psi_word);CHECK_SNPRINTF(prntRtrn, len);
    }
    if (egress_property->enable & AVALANCHE_PP_EGRESS_FIELD_ENABLE_TDOX_ENABLED)
    {
        prntRtrn = snprintf(buffer+len, size-len, "tdox_handle      : %d\n", egress_property->tdox_handle);CHECK_SNPRINTF(prntRtrn, len);
        prntRtrn = snprintf(buffer+len, size-len, "tdox_tcp_ack_num : %u\n", egress_property->tdox_tcp_ack_number);CHECK_SNPRINTF(prntRtrn, len);
    }
    prntRtrn = snprintf(buffer+len, size-len, "l2_packet_type   : %d [%s]\n", 
                        egress_property->l2_packet_type,
                        egress_property->l2_packet_type==AVALANCHE_PP_LUT_ENTRY_L2_ETHERNET?"ETH":"Undefined");
                        CHECK_SNPRINTF(prntRtrn, len);
    if (egress_property->enable & AVALANCHE_PP_EGRESS_FIELD_ENABLE_ENCAPSULATION)
    {
        Uint32 i, j;
        prntRtrn = snprintf(buffer+len, size-len, "wrapHeaderLen    : %d\n", egress_property->wrapHeaderLen);CHECK_SNPRINTF(prntRtrn, len);
        prntRtrn = snprintf(buffer+len, size-len, "wrapHeaderLenOff : %d\n", egress_property->wrapHeaderDataLenOffset);CHECK_SNPRINTF(prntRtrn, len);
        
        prntRtrn = snprintf(buffer+len, size-len, "wrapHeader       : ");
        for (i = 0, j = 0; i < egress_property->wrapHeaderLen; i += 4, j++)
        {
            prntRtrn = snprintf(buffer+len, size-len, "%08X.", *((Uint32*)(egress_property->wrapHeader) + j));
        }
        CHECK_SNPRINTF(prntRtrn, len);
    }
    if (egress_property->enable & AVALANCHE_PP_EGRESS_FIELD_ENABLE_L2)
    {
        prntRtrn = snprintf(buffer+len, size-len, "dstmac           : %02X:%02X:%02X:%02X:%02X:%02X\n", 
                            egress_property->dstmac[0], 
                            egress_property->dstmac[1], 
                            egress_property->dstmac[2], 
                            egress_property->dstmac[3], 
                            egress_property->dstmac[4], 
                            egress_property->dstmac[5]);
                            CHECK_SNPRINTF(prntRtrn, len);
        prntRtrn = snprintf(buffer+len, size-len, "srcmac           : %02X:%02X:%02X:%02X:%02X:%02X\n", 
                            egress_property->srcmac[0], 
                            egress_property->srcmac[1], 
                            egress_property->srcmac[2], 
                            egress_property->srcmac[3], 
                            egress_property->srcmac[4], 
                            egress_property->srcmac[5]);
                            CHECK_SNPRINTF(prntRtrn, len);
    }
    else
    {
        prntRtrn = snprintf(buffer+len, size-len, "dstmac           : XXX\n");CHECK_SNPRINTF(prntRtrn, len);
        prntRtrn = snprintf(buffer+len, size-len, "dstmac           : XXX\n");CHECK_SNPRINTF(prntRtrn, len);
    }

    if (egress_property->enable & AVALANCHE_PP_EGRESS_FIELD_ENABLE_VLAN)
    {
        prntRtrn = snprintf(buffer+len, size-len, "vlan             : 0x%04X\n", egress_property->vlan);CHECK_SNPRINTF(prntRtrn, len);
    }
    prntRtrn = snprintf(buffer+len, size-len, "eth_type         : 0x%04X\n", egress_property->eth_type);CHECK_SNPRINTF(prntRtrn, len);

    if (egress_property->enable & AVALANCHE_PP_EGRESS_FIELD_ENABLE_IP)
    {
        prntRtrn = snprintf(buffer+len, size-len, "l3_packet_type   : %d [%s]\n", 
                        egress_property->l3_packet_type,
                        egress_property->l3_packet_type==AVALANCHE_PP_LUT_ENTRY_L3_IPV4?"IPv4":
                        (egress_property->l3_packet_type==AVALANCHE_PP_LUT_ENTRY_L3_IPV6?"IPv6":"Undefined"));
                        CHECK_SNPRINTF(prntRtrn, len);
        prntRtrn = snprintf(buffer+len, size-len, "tunnel_type       : %d [%s]\n", 
                         egress_property->tunnel_type,
                         egress_property->tunnel_type==AVALANCHE_PP_LUT_ENTRY_L3_DSLITE?"Ds-Lite":
                         (egress_property->tunnel_type==AVALANCHE_PP_LUT_ENTRY_L3_GRE?"GRE":"Undefined"));
        prntRtrn = snprintf(buffer+len, size-len, "ip_protocol      : 0x%02X ", egress_property->ip_protocol);CHECK_SNPRINTF(prntRtrn, len);
        switch (egress_property->ip_protocol)
        {
        case IPPROTO_IPIP:
            prntRtrn = snprintf(buffer+len, size-len,  "[IPinIP]\n");CHECK_SNPRINTF(prntRtrn, len);
            break;
        case IPPROTO_TCP:
            prntRtrn = snprintf(buffer+len, size-len,  "[TCP]\n");CHECK_SNPRINTF(prntRtrn, len);
            break;
        case IPPROTO_UDP:
            prntRtrn = snprintf(buffer+len, size-len,  "[UDP]\n");CHECK_SNPRINTF(prntRtrn, len);
            break;
        case IPPROTO_GRE:
            prntRtrn = snprintf(buffer+len, size-len,  "[GRE]\n");CHECK_SNPRINTF(prntRtrn, len);
            break;
        case IPPROTO_IPV6:
            prntRtrn = snprintf(buffer+len, size-len,  "[IPv6-in-IPv4 tunnelling]\n");CHECK_SNPRINTF(prntRtrn, len);
            break;
        default:
            prntRtrn = snprintf(buffer+len, size-len,  "[???]\n");CHECK_SNPRINTF(prntRtrn, len);
            break;
        }
        prntRtrn = snprintf(buffer+len, size-len, "TOS              : 0x%02X\n", egress_property->TOS);CHECK_SNPRINTF(prntRtrn, len);

        if (egress_property->l3_packet_type == AVALANCHE_PP_LUT_ENTRY_L3_IPV4)
        {
            Uint8   *ipPtr = (Uint8 *)&egress_property->SRC_IP.v4;
            prntRtrn = snprintf(buffer+len, size-len,  "SRC_IP           : 0x%08X [ %d.%d.%d.%d ]\n", egress_property->SRC_IP.v4, ipPtr[0], ipPtr[1], ipPtr[2], ipPtr[3]);CHECK_SNPRINTF(prntRtrn, len);
            ipPtr = (Uint8 *)&egress_property->DST_IP.v4;
            prntRtrn = snprintf(buffer+len, size-len,  "DST_IP           : 0x%08X [ %d.%d.%d.%d ]\n", egress_property->DST_IP.v4, ipPtr[0], ipPtr[1], ipPtr[2], ipPtr[3]);CHECK_SNPRINTF(prntRtrn, len);
        }
        else if (egress_property->l3_packet_type == AVALANCHE_PP_LUT_ENTRY_L3_IPV6)
        {
            Int8    ipv6addr[64];
            const   Int8* buf;

            buf = LUT2_display_ipv6_addr((void*)egress_property->SRC_IP.v6, ipv6addr, sizeof(ipv6addr));
            prntRtrn = snprintf(buffer+len, size-len,  "SRC_IP           : %s\n", buf);CHECK_SNPRINTF(prntRtrn, len);
            buf = LUT2_display_ipv6_addr((void*)egress_property->DST_IP.v6, ipv6addr, sizeof(ipv6addr));
            prntRtrn = snprintf(buffer+len, size-len,  "DST_IP           : %s\n", buf);CHECK_SNPRINTF(prntRtrn, len);
        }
    }
    if (egress_property->enable & AVALANCHE_PP_EGRESS_FIELD_ENABLE_L4)
    {
        prntRtrn = snprintf(buffer+len, size-len,  "L4_SrcPort       : 0x%04X [%d]\n", egress_property->L4_SrcPort, egress_property->L4_SrcPort);CHECK_SNPRINTF(prntRtrn, len);
        prntRtrn = snprintf(buffer+len, size-len,  "L4_SrcPort       : 0x%04X [%d]\n", egress_property->L4_DstPort, egress_property->L4_DstPort);CHECK_SNPRINTF(prntRtrn, len);
    }
    else
    {
        prntRtrn = snprintf(buffer+len, size-len,  "L4_SrcPort       : XXX\n");CHECK_SNPRINTF(prntRtrn, len);
        prntRtrn = snprintf(buffer+len, size-len,  "L4_DstPort       : XXX\n");CHECK_SNPRINTF(prntRtrn, len);
    }

    if (egress_property->enable & AVALANCHE_PP_EGRESS_FIELD_ENABLE_TDOX_ENABLED)
    {
        prntRtrn = snprintf(buffer+len, size-len,  "\nTdox Information");CHECK_SNPRINTF(prntRtrn, len);
        prntRtrn = snprintf(buffer+len, size-len,  "\n================\n");CHECK_SNPRINTF(prntRtrn, len);
        prntRtrn = snprintf(buffer+len, size-len,  "pkts_forwarded   : %u\n", session_info->tdox_stats.packets_forwarded);CHECK_SNPRINTF(prntRtrn, len);
        prntRtrn = snprintf(buffer+len, size-len,  "bytes_forwarded  : %u\n", session_info->tdox_stats.bytes_forwarded);CHECK_SNPRINTF(prntRtrn, len);
        prntRtrn = snprintf(buffer+len, size-len,  "last_update_time : 0x%08X\n", session_info->tdox_stats.last_update_time);CHECK_SNPRINTF(prntRtrn, len);
    }

    if (len <= size)
    {
        return len; 
    }
    return 0;
}


/**************************************************************************/
/*! \fn int LUT2_display_l2_ingress
    (AVALANCHE_PP_INGRESS_SESSION_PROPERTY_t* ingress_property, char *buffer, int size) 
 **************************************************************************
 *  \brief The function prints the Layer2 Information.
 *  \param[in] param1 - pointer to ingress_property param2 - pointer to current location in global buffer 
    param3 - free bytes in global buff 
 *  \param[out] baffer - this function write on global buffer
 *  \return number of bytes wirtten to the buffer.
 **************************************************************************/
int LUT2_display_l2_ingress(AVALANCHE_PP_INGRESS_SESSION_PROPERTY_t* ingress_property, char *buffer, int size)
{
    int len = 0;
    int prntRtrn;

    if (ingress_property->lookup.LUT1.u.fields.L2.enable_flags & AVALANCHE_PP_LUT1_FIELD_ENABLE_MAC_DST)
    {
        prntRtrn = snprintf(buffer+len, size-len, "Dst. MAC     = %02x:%02x:%02x:%02x:%02x:%02x\n", 
                ingress_property->lookup.LUT1.u.fields.L2.dstmac[0], ingress_property->lookup.LUT1.u.fields.L2.dstmac[1], ingress_property->lookup.LUT1.u.fields.L2.dstmac[2], 
                ingress_property->lookup.LUT1.u.fields.L2.dstmac[3], ingress_property->lookup.LUT1.u.fields.L2.dstmac[4], ingress_property->lookup.LUT1.u.fields.L2.dstmac[5]);CHECK_SNPRINTF(prntRtrn, len);
    }
    else
    {
        prntRtrn = snprintf(buffer+len, size-len, "Dst. MAC     = XXX\n");CHECK_SNPRINTF(prntRtrn, len);
    }

    if (ingress_property->lookup.LUT1.u.fields.L2.enable_flags & AVALANCHE_PP_LUT1_FIELD_ENABLE_MAC_SRC)
    {
        prntRtrn = snprintf(buffer+len, size-len, "Src. MAC     = %02x:%02x:%02x:%02x:%02x:%02x\n", 
                ingress_property->lookup.LUT1.u.fields.L2.srcmac[0], ingress_property->lookup.LUT1.u.fields.L2.srcmac[1], ingress_property->lookup.LUT1.u.fields.L2.srcmac[2], 
                ingress_property->lookup.LUT1.u.fields.L2.srcmac[3], ingress_property->lookup.LUT1.u.fields.L2.srcmac[4], ingress_property->lookup.LUT1.u.fields.L2.srcmac[5]);CHECK_SNPRINTF(prntRtrn, len);
    }
    else
    {
        prntRtrn = snprintf(buffer+len, size-len, "Src. MAC     = XXX\n");CHECK_SNPRINTF(prntRtrn, len);
    }

    if (ingress_property->lookup.LUT2.u.fields.enable_flags & AVALANCHE_PP_LUT2_FIELD_ENABLE_1ST_VLAN)
    {
        prntRtrn = snprintf(buffer+len, size-len, "1st VLAN     = 0x%04x\n", ingress_property->lookup.LUT2.u.fields.firstVLAN);CHECK_SNPRINTF(prntRtrn, len);
    }
    if (ingress_property->lookup.LUT2.u.fields.enable_flags & AVALANCHE_PP_LUT2_FIELD_ENABLE_2ND_VLAN)
    {
        prntRtrn = snprintf(buffer+len, size-len, "2nd VLAN     = 0x%04x\n", ingress_property->lookup.LUT2.u.fields.secondVLAN);CHECK_SNPRINTF(prntRtrn, len);
    }

    if (len <= size)
    {
        return len; 
    }
    return 0;
}

/**************************************************************************/
/*! \fn int LUT2_display_l2_egress 
    (AVALANCHE_PP_EGRESS_SESSION_PROPERTY_t* egress_property, char *buffer, int size)
 **************************************************************************
 *  \brief The function prints the Layer2 Information.
 *  \param[in] param1 - pointer to egress_property param2 - pointer to current location in global buffer 
    param3 - free bytes in global buff 
 *  \param[out] baffer - this function write on global buffer
 *  \return number of bytes wirtten to the buffer.
 **************************************************************************/
int LUT2_display_l2_egress(AVALANCHE_PP_EGRESS_SESSION_PROPERTY_t* egress_property, char *buffer, int size)
{
    int len = 0;
    int prntRtrn;

    AVALANCHE_PP_VPID_INFO_t *vpid;

    if (egress_property->enable & AVALANCHE_PP_EGRESS_FIELD_ENABLE_L2)
    {
        prntRtrn = snprintf(buffer+len, size-len, "Dst. MAC     = %02x:%02x:%02x:%02x:%02x:%02x\n", 
                egress_property->dstmac[0], egress_property->dstmac[1], egress_property->dstmac[2], 
                egress_property->dstmac[3], egress_property->dstmac[4], egress_property->dstmac[5]);CHECK_SNPRINTF(prntRtrn, len);
        prntRtrn = snprintf(buffer+len, size-len, "Src. MAC     = %02x:%02x:%02x:%02x:%02x:%02x\n", 
                egress_property->srcmac[0], egress_property->srcmac[1], egress_property->srcmac[2], 
                egress_property->srcmac[3], egress_property->srcmac[4], egress_property->srcmac[5]);CHECK_SNPRINTF(prntRtrn, len);
    }
    else
    {
        prntRtrn = snprintf(buffer+len, size-len, "Dst. MAC     = XXX\n");CHECK_SNPRINTF(prntRtrn, len);
        prntRtrn = snprintf(buffer+len, size-len, "Src. MAC     = XXX\n");CHECK_SNPRINTF(prntRtrn, len);
    }

    if (avalanche_pp_vpid_get_info(egress_property->vpid_handle, &vpid) == PP_RC_SUCCESS)
    {
        if (vpid->type == AVALANCHE_PP_VPID_VLAN)
        {
            prntRtrn = snprintf(buffer+len, size-len, "VLAN (VPID)  = 0x%04x\n", vpid->vlan_identifier);CHECK_SNPRINTF(prntRtrn, len);
        }
    }
    if (egress_property->enable & AVALANCHE_PP_EGRESS_FIELD_ENABLE_VLAN)
    {
        prntRtrn = snprintf(buffer+len, size-len, "VLAN (PKT)   = 0x%04x\n", egress_property->vlan);CHECK_SNPRINTF(prntRtrn, len);
    }

    if (len <= size)
    {
        return len; 
    }
    return 0;
}
/**************************************************************************/
/*! \fn int LUT2_display_ipv4_ingress 
    (AVALANCHE_PP_INGRESS_SESSION_PROPERTY_t* ingress_property, char *buffer, int size)
 **************************************************************************
 *  \brief The function prints the IPv4 Information.
 *  \param[in] param1 - pointer to ingress_property param2 - pointer to current location in global buffer 
    param3 - free bytes in global buff 
 *  \param[out] baffer - this function write on global buffer
 *  \return number of bytes wirtten to the buffer.
 **************************************************************************/
int LUT2_display_ipv4_ingress(AVALANCHE_PP_INGRESS_SESSION_PROPERTY_t* ingress_property, char *buffer, int size)
{
    int len = 0;
    int prntRtrn;

    if (ingress_property->lookup.LUT1.u.fields.L3.enable_flags & AVALANCHE_PP_LUT1_FIELD_ENABLE_LAN_IPv4)
    {
        Uint8 *ipPtr = (Uint8 *)&ingress_property->lookup.LUT1.u.fields.L3.LAN_addr_IP.v4;
        prntRtrn = snprintf(buffer+len, size-len,  "LAN IP       = 0x%08X [ %d.%d.%d.%d ]\n", ingress_property->lookup.LUT1.u.fields.L3.LAN_addr_IP.v4, ipPtr[0], ipPtr[1], ipPtr[2], ipPtr[3]);CHECK_SNPRINTF(prntRtrn, len);
    }
    else
    {
        prntRtrn = snprintf(buffer+len, size-len,  "LAN IP       = XXX\n");CHECK_SNPRINTF(prntRtrn, len);
    }

    if (ingress_property->lookup.LUT2.u.fields.enable_flags & AVALANCHE_PP_LUT2_FIELD_ENABLE_WAN_IP)
    {
        Uint8 *ipPtr = (Uint8 *)&ingress_property->lookup.LUT2.u.fields.WAN_addr_IP.v4;
        prntRtrn = snprintf(buffer+len, size-len,  "WAN IP       = 0x%08X [ %d.%d.%d.%d ]\n", ingress_property->lookup.LUT2.u.fields.WAN_addr_IP.v4, ipPtr[0], ipPtr[1], ipPtr[2], ipPtr[3]);CHECK_SNPRINTF(prntRtrn, len);
    }
    else
    {
        prntRtrn = snprintf(buffer+len, size-len,  "WAN IP       = XXX\n");CHECK_SNPRINTF(prntRtrn, len);
    }

    if (ingress_property->lookup.LUT1.u.fields.L3.enable_flags & AVALANCHE_PP_LUT1_FIELD_ENABLE_IP_PROTOCOL)
    {
        prntRtrn = snprintf(buffer+len, size-len,  "Protocol     = 0x%02X ", ingress_property->lookup.LUT1.u.fields.L3.ip_protocol);CHECK_SNPRINTF(prntRtrn, len);
        switch (ingress_property->lookup.LUT1.u.fields.L3.ip_protocol)
        {
        case IPPROTO_IPIP:
            prntRtrn = snprintf(buffer+len, size-len,  "[IPinIP]\n");CHECK_SNPRINTF(prntRtrn, len);
            break;
        case IPPROTO_TCP:
            prntRtrn = snprintf(buffer+len, size-len,  "[TCP]\n");CHECK_SNPRINTF(prntRtrn, len);
            break;
        case IPPROTO_UDP:
            prntRtrn = snprintf(buffer+len, size-len,  "[UDP]\n");CHECK_SNPRINTF(prntRtrn, len);
            break;
        case IPPROTO_GRE:
            prntRtrn = snprintf(buffer+len, size-len,  "[GRE]\n");CHECK_SNPRINTF(prntRtrn, len);
            break;
        case IPPROTO_IPV6:
            prntRtrn = snprintf(buffer+len, size-len,  "[IPv6-in-IPv4 tunnelling]\n");CHECK_SNPRINTF(prntRtrn, len);
            break;
        default:
            prntRtrn = snprintf(buffer+len, size-len,  "[???]\n");CHECK_SNPRINTF(prntRtrn, len);
            break;
        }
    }
    else
    {
        prntRtrn = snprintf(buffer+len, size-len,  "Protocol     = XXX\n");CHECK_SNPRINTF(prntRtrn, len);
    }

    if (ingress_property->lookup.LUT2.u.fields.enable_flags & AVALANCHE_PP_LUT2_FIELD_ENABLE_IP_TOS)
    {
        prntRtrn = snprintf(buffer+len, size-len,  "TOS          = 0x%02X\n", ingress_property->lookup.LUT2.u.fields.TOS);CHECK_SNPRINTF(prntRtrn, len);
    }
    else
    {
        prntRtrn = snprintf(buffer+len, size-len,  "TOS          = XXX\n");CHECK_SNPRINTF(prntRtrn, len);
    }

    if (ingress_property->lookup.LUT2.u.fields.enable_flags & AVALANCHE_PP_LUT2_FIELD_ENABLE_DST_PORT)
    {
        prntRtrn = snprintf(buffer+len, size-len,  "Dst Port     = 0x%04X [%d]\n", ingress_property->lookup.LUT2.u.fields.L4_DstPort, ingress_property->lookup.LUT2.u.fields.L4_DstPort);CHECK_SNPRINTF(prntRtrn, len);
    }
    else
    {
        prntRtrn = snprintf(buffer+len, size-len,  "Dst Port     = XXX\n");CHECK_SNPRINTF(prntRtrn, len);
    }
    
    if (ingress_property->lookup.LUT2.u.fields.enable_flags & AVALANCHE_PP_LUT2_FIELD_ENABLE_SRC_PORT)
    {
        prntRtrn = snprintf(buffer+len, size-len,  "Src Port     = 0x%04X [%d]\n", ingress_property->lookup.LUT2.u.fields.L4_SrcPort, ingress_property->lookup.LUT2.u.fields.L4_SrcPort);CHECK_SNPRINTF(prntRtrn, len);
    }
    else
    {
        prntRtrn = snprintf(buffer+len, size-len,  "Src Port     = XXX\n");CHECK_SNPRINTF(prntRtrn, len);
    }

    if (len <= size)
    {
        return len; 
    }
    return 0;
}
/**************************************************************************/
/*! \fn int LUT2_display_ipv4_egress 
    (AVALANCHE_PP_EGRESS_SESSION_PROPERTY_t* egress_property, char *buffer, int size)
 **************************************************************************
 *  \brief The function prints the IPv4 Information.
 *  \param[in] param1 - pointer to egress_property param2 - pointer to current location in global buffer 
    param3 - free bytes in global buff 
 *  \param[out] baffer - this function write on global buffer
 *  \return number of bytes wirtten to the buffer.
 **************************************************************************/
int LUT2_display_ipv4_egress(AVALANCHE_PP_EGRESS_SESSION_PROPERTY_t* egress_property, char *buffer, int size)
{
    int     len = 0;
    int     prntRtrn;
    Uint8   *ipPtr;

    if (egress_property->enable & AVALANCHE_PP_EGRESS_FIELD_ENABLE_IP)
    {
        ipPtr = (Uint8 *)&egress_property->SRC_IP.v4;
        prntRtrn = snprintf(buffer+len, size-len,  "SRC IP       = 0x%08X [ %d.%d.%d.%d ]\n", egress_property->SRC_IP.v4, ipPtr[0], ipPtr[1], ipPtr[2], ipPtr[3]);CHECK_SNPRINTF(prntRtrn, len);
        ipPtr = (Uint8 *)&egress_property->DST_IP.v4;
        prntRtrn = snprintf(buffer+len, size-len,  "DST IP       = 0x%08X [ %d.%d.%d.%d ]\n", egress_property->DST_IP.v4, ipPtr[0], ipPtr[1], ipPtr[2], ipPtr[3]);CHECK_SNPRINTF(prntRtrn, len);

        prntRtrn = snprintf(buffer+len, size-len,  "Protocol     = 0x%02X ", egress_property->ip_protocol);CHECK_SNPRINTF(prntRtrn, len);
        switch (egress_property->ip_protocol)
        {
        case IPPROTO_IPIP:
            prntRtrn = snprintf(buffer+len, size-len,  "[IPinIP]\n");CHECK_SNPRINTF(prntRtrn, len);
            break;
        case IPPROTO_TCP:
            prntRtrn = snprintf(buffer+len, size-len,  "[TCP]\n");CHECK_SNPRINTF(prntRtrn, len);
            break;
        case IPPROTO_UDP:
            prntRtrn = snprintf(buffer+len, size-len,  "[UDP]\n");CHECK_SNPRINTF(prntRtrn, len);
            break;
        case IPPROTO_GRE:
            prntRtrn = snprintf(buffer+len, size-len,  "[GRE]\n");CHECK_SNPRINTF(prntRtrn, len);
            break;
        case IPPROTO_IPV6:
            prntRtrn = snprintf(buffer+len, size-len,  "[IPv6-in-IPv4 tunnelling]\n");CHECK_SNPRINTF(prntRtrn, len);
            break;
        default:
            prntRtrn = snprintf(buffer+len, size-len,  "[???]\n");CHECK_SNPRINTF(prntRtrn, len);
            break;
        }

        prntRtrn = snprintf(buffer+len, size-len,  "TOS          = 0x%02X\n", egress_property->TOS);CHECK_SNPRINTF(prntRtrn, len);
    }
    else
    {
        prntRtrn = snprintf(buffer+len, size-len,  "SRC IP       = XXX\n");CHECK_SNPRINTF(prntRtrn, len);
        prntRtrn = snprintf(buffer+len, size-len,  "DST IP       = XXX\n");CHECK_SNPRINTF(prntRtrn, len);
        prntRtrn = snprintf(buffer+len, size-len,  "Protocol     = XXX\n");CHECK_SNPRINTF(prntRtrn, len);
        prntRtrn = snprintf(buffer+len, size-len,  "TOS          = XXX\n");CHECK_SNPRINTF(prntRtrn, len);
    }
    
    if (egress_property->enable & AVALANCHE_PP_EGRESS_FIELD_ENABLE_L4)
    {
        prntRtrn = snprintf(buffer+len, size-len,  "Dst Port     = 0x%04X [%d]\n", egress_property->L4_DstPort, egress_property->L4_DstPort);CHECK_SNPRINTF(prntRtrn, len);
        prntRtrn = snprintf(buffer+len, size-len,  "Src Port     = 0x%04X [%d]\n", egress_property->L4_SrcPort, egress_property->L4_SrcPort);CHECK_SNPRINTF(prntRtrn, len);
    }
    else
    {
        prntRtrn = snprintf(buffer+len, size-len,  "Dst Port     = XXX\n");CHECK_SNPRINTF(prntRtrn, len);
        prntRtrn = snprintf(buffer+len, size-len,  "Src Port     = XXX\n");CHECK_SNPRINTF(prntRtrn, len);
    }
    
    if (len <= size)
    {
        return len; 
    }
    return 0;
}
/**************************************************************************/
/*! \fn int LUT2_display_ipv6_ingress 
    (AVALANCHE_PP_INGRESS_SESSION_PROPERTY_t* ingress_property, char *buffer, int size)
 **************************************************************************
 *  \brief The function prints the IPv6 Information.
 *  \param[in] param1 - pointer to ingress_property param2 - pointer to current location in global buffer 
    param3 - free bytes in global buff 
 *  \param[out] baffer - this function write on global buffer
 *  \return number of bytes wirtten to the buffer.
 **************************************************************************/
int LUT2_display_ipv6_ingress(AVALANCHE_PP_INGRESS_SESSION_PROPERTY_t* ingress_property, char *buffer, int size)
{
    int     len = 0;
    int     prntRtrn;
    Int8    ipv6addr[64];
    const   Int8* buf;

    if (ingress_property->lookup.LUT1.u.fields.L3.enable_flags & AVALANCHE_PP_LUT1_FIELD_ENABLE_LAN_IPv6)
    {
        buf = LUT2_display_ipv6_addr((void*)ingress_property->lookup.LUT1.u.fields.L3.LAN_addr_IP.v6, ipv6addr, sizeof(ipv6addr));
        if (buf != NULL)
        {
            prntRtrn = snprintf(buffer+len, size-len,  "LAN IP       = %s\n", buf);CHECK_SNPRINTF(prntRtrn, len);
        }
        else
        {
            prntRtrn = snprintf(buffer+len, size-len,  "LAN IP       = XXX\n");CHECK_SNPRINTF(prntRtrn, len);
        }
    }
    else
    {
        prntRtrn = snprintf(buffer+len, size-len,  "LAN IP       = XXX\n");CHECK_SNPRINTF(prntRtrn, len);
    }

    if (ingress_property->lookup.LUT2.u.fields.enable_flags & AVALANCHE_PP_LUT2_FIELD_ENABLE_WAN_IP)
    {
        buf = LUT2_display_ipv6_addr((void*)ingress_property->lookup.LUT2.u.fields.WAN_addr_IP.v6, ipv6addr, sizeof(ipv6addr));
        if (buf != NULL)
        {
            prntRtrn = snprintf(buffer+len, size-len,  "WAN IP       = %s\n", buf);CHECK_SNPRINTF(prntRtrn, len);
        }
        else
        {
            prntRtrn = snprintf(buffer+len, size-len,  "WAN IP       = XXX\n");CHECK_SNPRINTF(prntRtrn, len);
        }
    }
    else
    {
        prntRtrn = snprintf(buffer+len, size-len,  "WAN IP       = XXX\n");CHECK_SNPRINTF(prntRtrn, len);
    }

    if (ingress_property->lookup.LUT1.u.fields.L3.enable_flags & AVALANCHE_PP_LUT1_FIELD_ENABLE_IP_PROTOCOL)
    {
        prntRtrn = snprintf(buffer+len, size-len,  "Protocol     = 0x%02X ", ingress_property->lookup.LUT1.u.fields.L3.ip_protocol);CHECK_SNPRINTF(prntRtrn, len);
        switch (ingress_property->lookup.LUT1.u.fields.L3.ip_protocol)
        {
        case IPPROTO_IPIP:
            prntRtrn = snprintf(buffer+len, size-len,  "[IPinIP]\n");CHECK_SNPRINTF(prntRtrn, len);
            break;
        case IPPROTO_TCP:
            prntRtrn = snprintf(buffer+len, size-len,  "[TCP]\n");CHECK_SNPRINTF(prntRtrn, len);
            break;
        case IPPROTO_UDP:
            prntRtrn = snprintf(buffer+len, size-len,  "[UDP]\n");CHECK_SNPRINTF(prntRtrn, len);
            break;
        case IPPROTO_GRE:
            prntRtrn = snprintf(buffer+len, size-len,  "[GRE]\n");CHECK_SNPRINTF(prntRtrn, len);
            break;
        case IPPROTO_IPV6:
            prntRtrn = snprintf(buffer+len, size-len,  "[IPv6-in-IPv4 tunnelling]\n");CHECK_SNPRINTF(prntRtrn, len);
            break;
        default:
            prntRtrn = snprintf(buffer+len, size-len,  "[???]\n");CHECK_SNPRINTF(prntRtrn, len);
            break;
        }
    }
    else
    {
        prntRtrn = snprintf(buffer+len, size-len,  "Protocol     = XXX\n");CHECK_SNPRINTF(prntRtrn, len);
    }

    if (ingress_property->lookup.LUT2.u.fields.enable_flags & AVALANCHE_PP_LUT2_FIELD_ENABLE_IP_TOS)
    {
        prntRtrn = snprintf(buffer+len, size-len,  "TOS          = 0x%02X\n", ingress_property->lookup.LUT2.u.fields.TOS);CHECK_SNPRINTF(prntRtrn, len);
    }
    else
    {
        prntRtrn = snprintf(buffer+len, size-len,  "TOS          = XXX\n");CHECK_SNPRINTF(prntRtrn, len);
    }

    if (ingress_property->lookup.LUT2.u.fields.enable_flags & AVALANCHE_PP_LUT2_FIELD_ENABLE_DST_PORT)
    {
        prntRtrn = snprintf(buffer+len, size-len,  "Dst Port     = 0x%04X [%d]\n", ingress_property->lookup.LUT2.u.fields.L4_DstPort, ingress_property->lookup.LUT2.u.fields.L4_DstPort);CHECK_SNPRINTF(prntRtrn, len);
    }
    else
    {
        prntRtrn = snprintf(buffer+len, size-len,  "Dst Port     = XXX\n");CHECK_SNPRINTF(prntRtrn, len);
    }
    
    if (ingress_property->lookup.LUT2.u.fields.enable_flags & AVALANCHE_PP_LUT2_FIELD_ENABLE_SRC_PORT)
    {
        prntRtrn = snprintf(buffer+len, size-len,  "Src Port     = 0x%04X [%d]\n", ingress_property->lookup.LUT2.u.fields.L4_SrcPort, ingress_property->lookup.LUT2.u.fields.L4_SrcPort);CHECK_SNPRINTF(prntRtrn, len);
    }
    else
    {
        prntRtrn = snprintf(buffer+len, size-len,  "Src Port     = XXX\n");CHECK_SNPRINTF(prntRtrn, len);
    }

    if ((ingress_property->lookup.LUT2.u.fields.entry_type == AVALANCHE_PP_LUT_ENTRY_L3_DSLITE) && 
        (ingress_property->lookup.LUT2.u.fields.enable_flags & AVALANCHE_PP_LUT2_FIELD_ENABLE_DSLITE_IPV4))
    {
        Uint8 *ipPtr = (Uint8 *)&ingress_property->lookup.LUT2.u.fields.IP.v4_dsLite;
        prntRtrn = snprintf(buffer+len, size-len,  "DsLite DstIP = 0x%08X [ %d.%d.%d.%d ]\n", ingress_property->lookup.LUT2.u.fields.IP.v4_dsLite, ipPtr[0], ipPtr[1], ipPtr[2], ipPtr[3]);CHECK_SNPRINTF(prntRtrn, len);
    }
    else if (ingress_property->lookup.LUT2.u.fields.enable_flags & AVALANCHE_PP_LUT2_FIELD_ENABLE_IPV6_FLOW)
    {
        Uint8 *flowLabel = (Uint8 *)&ingress_property->lookup.LUT2.u.fields.IP.v6_FlowLabel[0];
        prntRtrn = snprintf(buffer+len, size-len,  "Flow Label   = 0x%02X.%02X.%02X\n", flowLabel[0], flowLabel[1], flowLabel[2]);CHECK_SNPRINTF(prntRtrn, len);
    }

    if (len <= size)
    {
        return len; 
    }
    return 0;
}
/**************************************************************************/
/*! \fn int LUT2_display_ipv6_egress 
    (AVALANCHE_PP_EGRESS_SESSION_PROPERTY_t* egress_property, char *buffer, int size)
 **************************************************************************
 *  \brief The function prints the IPv6 Information.
 *  \param[in] param1 - pointer to egress_property param2 - pointer to current location in global buffer 
    param3 - free bytes in global buff 
 *  \param[out] baffer - this function write on global buffer
 *  \return number of bytes wirtten to the buffer.
 **************************************************************************/
int LUT2_display_ipv6_egress(AVALANCHE_PP_EGRESS_SESSION_PROPERTY_t* egress_property, char *buffer, int size)
{
    int     len = 0;
    int     prntRtrn;
    Int8    ipv6addr[64];
    const   Int8* buf;

    if (egress_property->enable & AVALANCHE_PP_EGRESS_FIELD_ENABLE_IP)
    {
        buf = LUT2_display_ipv6_addr((void*)egress_property->SRC_IP.v6, ipv6addr, sizeof(ipv6addr));
        prntRtrn = snprintf(buffer+len, size-len,  "Src IP       = %s\n", buf);CHECK_SNPRINTF(prntRtrn, len);
        buf = LUT2_display_ipv6_addr((void*)egress_property->DST_IP.v6, ipv6addr, sizeof(ipv6addr));
        prntRtrn = snprintf(buffer+len, size-len,  "Dst IP       = %s\n", buf);CHECK_SNPRINTF(prntRtrn, len);

        prntRtrn = snprintf(buffer+len, size-len,  "Protocol     = 0x%02X ", egress_property->ip_protocol);CHECK_SNPRINTF(prntRtrn, len);
        switch (egress_property->ip_protocol)
        {
        case IPPROTO_IPIP:
            prntRtrn = snprintf(buffer+len, size-len,  "[IPinIP]\n");CHECK_SNPRINTF(prntRtrn, len);
            break;
        case IPPROTO_TCP:
            prntRtrn = snprintf(buffer+len, size-len,  "[TCP]\n");CHECK_SNPRINTF(prntRtrn, len);
            break;
        case IPPROTO_UDP:
            prntRtrn = snprintf(buffer+len, size-len,  "[UDP]\n");CHECK_SNPRINTF(prntRtrn, len);
            break;
        case IPPROTO_GRE:
            prntRtrn = snprintf(buffer+len, size-len,  "[GRE]\n");CHECK_SNPRINTF(prntRtrn, len);
            break;
        case IPPROTO_IPV6:
            prntRtrn = snprintf(buffer+len, size-len,  "[IPv6-in-IPv4 tunnelling]\n");CHECK_SNPRINTF(prntRtrn, len);
            break;
        default:
            prntRtrn = snprintf(buffer+len, size-len,  "[???]\n");CHECK_SNPRINTF(prntRtrn, len);
            break;
        }

        prntRtrn = snprintf(buffer+len, size-len,  "TOS          = 0x%02X\n", egress_property->TOS);CHECK_SNPRINTF(prntRtrn, len);
    }
    else
    {
        prntRtrn = snprintf(buffer+len, size-len,  "SRC IP       = XXX\n");CHECK_SNPRINTF(prntRtrn, len);
        prntRtrn = snprintf(buffer+len, size-len,  "DST IP       = XXX\n");CHECK_SNPRINTF(prntRtrn, len);
        prntRtrn = snprintf(buffer+len, size-len,  "Protocol     = XXX\n");CHECK_SNPRINTF(prntRtrn, len);
        prntRtrn = snprintf(buffer+len, size-len,  "TOS          = XXX\n");CHECK_SNPRINTF(prntRtrn, len);
    }

    if (egress_property->enable & AVALANCHE_PP_EGRESS_FIELD_ENABLE_L4)
    {
        prntRtrn = snprintf(buffer+len, size-len,  "Dst Port     = 0x%04X [%d]\n", egress_property->L4_DstPort, egress_property->L4_DstPort);CHECK_SNPRINTF(prntRtrn, len);
        prntRtrn = snprintf(buffer+len, size-len,  "Src Port     = 0x%04X [%d]\n", egress_property->L4_SrcPort, egress_property->L4_SrcPort);CHECK_SNPRINTF(prntRtrn, len);
    }
    else
    {
        prntRtrn = snprintf(buffer+len, size-len,  "Dst Port     = XXX\n");CHECK_SNPRINTF(prntRtrn, len);
        prntRtrn = snprintf(buffer+len, size-len,  "Src Port     = XXX\n");CHECK_SNPRINTF(prntRtrn, len);
    }
    if (len <= size)
    {
        return len; 
    }
    return 0;
}
/**************************************************************************
 * FUNCTION NAME : LUT2_display_ipv6_addr
 **************************************************************************
 * DESCRIPTION   :
 *  The function converts a numeric address into a text string suitable for presentation.
 **************************************************************************/
const Int8* LUT2_display_ipv6_addr (const void *cp, Int8 *buf, size_t len)
{
    size_t xlen;

    const struct in6_addr *s = (const struct in6_addr *)cp;

    xlen = snprintf(buf, len, "%x:%x:%x:%x:%x:%x:%x:%x",
                    ntohs(s->s6_addr16[0]), ntohs(s->s6_addr16[1]),
                    ntohs(s->s6_addr16[2]), ntohs(s->s6_addr16[3]),
                    ntohs(s->s6_addr16[4]), ntohs(s->s6_addr16[5]),
                    ntohs(s->s6_addr16[6]), ntohs(s->s6_addr16[7]));

    if (xlen > len)
    {
        return NULL;
    }

    return buf;
}

/**************************************************************************************/
/*! \fn static int VER_read_proc (char *page, char **start, 
*                  off_t off, int count,int *eof, void *data)
 **************************************************************************************
 *  \brief This function reads VERSION
 *  \return return the number of bytes wirtten to the buffer.
 **************************************************************************************/
static int VER_read_proc (char *page, char **start, off_t off, int count, int *eof, void *data)
{ 
    int           len = 0;
    int           prntRtrn;
    int           curr_off = 0;
    static Uint8  endOfData = 0;
    
    AVALANCHE_PP_VERSION_t   version;

    if (endOfData == 1)
    {
        endOfData = 0;
        return 0;
    }

    if (PP_RC_SUCCESS != avalanche_pp_version_get( &version ))
    {
        prntRtrn = snprintf(buff+len, BUFF_FREE( len), " Version retrieve failed ... \n");CHECK_SNPRINTF(prntRtrn, len);
    }
    else
    {
        prntRtrn = snprintf(buff+len, BUFF_FREE( len), " Packet Processor Firmware Version : %d.%d.%d.%d \n", version.v0, version.v1, version.v2, version.v3 );CHECK_SNPRINTF(prntRtrn, len);
    }
    WR_TO_PAGE

    /* Set next begin of page */   
    *start = page;
    *eof = 1;
    endOfData = 1;

    /* Return number of bytes writen */
    return curr_off ;
}

/**************************************************************************/
/*! \fn static int XSESSION_write_proc(struct file *fp, const char * buf, unsigned long count, void * data)                                     
 **************************************************************************
 *  \brief This function sets the session handle for the XSESSION_read_proc
 *  \return OK or error status.
 **************************************************************************/
static int XSESSION_write_proc(struct file *fp, const char * buf, unsigned long count, void * data)
{
    unsigned char local_buf[10];
	int ret_val = 0;

	if (count > 10)
	{
		printk(KERN_ERR "\n%s[%d]: Buffer Overflow\n", __FUNCTION__, __LINE__);
		return -EFAULT;
	}

	if (copy_from_user(local_buf, buf, count))
    {
        return -EFAULT;
    }

	local_buf[count - 1]='\0'; 
    ret_val = count;
	sscanf(local_buf, "%d" ,&g_proc_xsession_handle);

	return ret_val;
}


/**************************************************************************************/
/*! \fn static int XSESSION_read_proc (char *page, char **start, off_t off, int count,int *eof, void *data)
 **************************************************************************************
 *  \brief This function reads extended session information
 *  \return return the number of bytes wirtten to the buffer.
 **************************************************************************************/
static int XSESSION_read_proc(char *page, char **start, off_t off, int count, int *eof, void *data)
{ 
    int           len = 0;
    int           prntRtrn;
    int           curr_off = 0;
    static Uint8  endOfData = 0;
    static Uint8  curr_pos = 0;
    
    if (endOfData == 1)
    {
        curr_pos = 0;
        endOfData = 0;
        return 0;
    }

    if (g_proc_xsession_handle < AVALANCHE_PP_MAX_ACCELERATED_SESSIONS)
    {
        /*first page*/
        if (curr_pos == 0)
        {
            prntRtrn = display_session_information(g_proc_xsession_handle, buff + len, BUFF_FREE(len)); CHECK_SNPRINTF(prntRtrn, len); 
            WR_TO_PAGE
            /* Set next begin of page */   
            *start = page;
            *eof = 1;
            /*if write to buffer succes then write page and return to next position*/
            if (curr_off != 0)
            {
                curr_pos++;
            }
            /* Return number of bytes writen */
            return curr_off ;
        }
        /*second page*/
        if (curr_pos == 1)
        {
            pp_hal_display_session_extended_info(g_proc_xsession_handle, buff + len, &prntRtrn); CHECK_SNPRINTF(prntRtrn, len);         
            WR_TO_PAGE
        }
    }
    

    /* Set next begin of page */   
    *start = page;
    *eof = 1;
    if (curr_off > 0)
    {
        endOfData = 1;
    }

    /* Return number of bytes writen */
    return curr_off ;
}

/**************************************************************************/
/*! \fn static int SESSION_write_proc(struct file *fp, const char * buf, unsigned long count, void * data)                                     
 **************************************************************************
 *  \brief This function sets the session handle for the SESSION_read_proc
 *  \return OK or error status.
 **************************************************************************/
static int SESSION_write_proc(struct file *fp, const char * buf, unsigned long count, void * data)
{
	int ret_val = 0;

	if (count > 100)
	{
		printk(KERN_ERR "\n%s[%d]: Buffer Overflow\n", __FUNCTION__, __LINE__);
		return -EFAULT;
	}

	if (copy_from_user(g_proc_session_cmd, buf, count))
    {
        return -EFAULT;
    }

	g_proc_session_cmd[count]='\0'; 

    ret_val = count;
	
	return ret_val;
}

Uint8 g_session_list[AVALANCHE_PP_MAX_ACCELERATED_SESSIONS / 8];


AVALANCHE_PP_RET_e handler_active_sessions_all( AVALANCHE_PP_SESSION_INFO_t *  ptr_session, Ptr data )
{
    g_session_list[ptr_session->session_handle / 8] |= 1 << (ptr_session->session_handle % 8);
    return PP_RC_SUCCESS;
}

AVALANCHE_PP_RET_e handler_active_sessions_wan2lan( AVALANCHE_PP_SESSION_INFO_t *  ptr_session, Ptr data )
{
    if (ptr_session->ingress.dev_type == AVALANCHE_PP_DEV_TYPE_WAN &&
        ptr_session->egress.dev_type  == AVALANCHE_PP_DEV_TYPE_LAN)
    {
        g_session_list[ptr_session->session_handle / 8] |= 1 << (ptr_session->session_handle % 8);
    }
    return PP_RC_SUCCESS;
}

AVALANCHE_PP_RET_e handler_active_sessions_lan2wan( AVALANCHE_PP_SESSION_INFO_t *  ptr_session, Ptr data )
{
    if (ptr_session->ingress.dev_type == AVALANCHE_PP_DEV_TYPE_LAN &&
        ptr_session->egress.dev_type  == AVALANCHE_PP_DEV_TYPE_WAN)
    {
        g_session_list[ptr_session->session_handle / 8] |= 1 << (ptr_session->session_handle % 8);
    }
    return PP_RC_SUCCESS;
}

AVALANCHE_PP_RET_e handler_active_sessions_lan2lan( AVALANCHE_PP_SESSION_INFO_t *  ptr_session, Ptr data )
{
    if (ptr_session->ingress.dev_type == AVALANCHE_PP_DEV_TYPE_LAN &&
        ptr_session->egress.dev_type  == AVALANCHE_PP_DEV_TYPE_LAN)
    {
        g_session_list[ptr_session->session_handle / 8] |= 1 << (ptr_session->session_handle % 8);
    }
    return PP_RC_SUCCESS;
}

AVALANCHE_PP_RET_e handler_active_sessions_mac_da( AVALANCHE_PP_SESSION_INFO_t *  ptr_session, Ptr data )
{
    if (memcmp(ptr_session->ingress.lookup.LUT1.u.fields.L2.dstmac, data, sizeof(ptr_session->ingress.lookup.LUT1.u.fields.L2.dstmac)) == 0)
    {
        g_session_list[ptr_session->session_handle / 8] |= 1 << (ptr_session->session_handle % 8);
    }
    return PP_RC_SUCCESS;
}

AVALANCHE_PP_RET_e handler_active_sessions_mac_sa( AVALANCHE_PP_SESSION_INFO_t *  ptr_session, Ptr data )
{
    if (memcmp(ptr_session->ingress.lookup.LUT1.u.fields.L2.srcmac, data, sizeof(ptr_session->ingress.lookup.LUT1.u.fields.L2.srcmac)) == 0)
    {
        g_session_list[ptr_session->session_handle / 8] |= 1 << (ptr_session->session_handle % 8);
    }
    return PP_RC_SUCCESS;
}

AVALANCHE_PP_RET_e handler_active_sessions_mac_all( AVALANCHE_PP_SESSION_INFO_t *  ptr_session, Ptr data )
{
    handler_active_sessions_mac_da(ptr_session, data);
    handler_active_sessions_mac_sa(ptr_session, data);
    return PP_RC_SUCCESS;
}

AVALANCHE_PP_RET_e handler_active_sessions_ipv4_dst( AVALANCHE_PP_SESSION_INFO_t *  ptr_session, Ptr data )
{
    if (((ptr_session->ingress.dev_type == AVALANCHE_PP_DEV_TYPE_WAN) && (ptr_session->ingress.lookup.LUT1.u.fields.L3.LAN_addr_IP.v4 == *(Uint32*)data)) ||
        ((ptr_session->ingress.dev_type == AVALANCHE_PP_DEV_TYPE_LAN) && (ptr_session->ingress.lookup.LUT2.u.fields.WAN_addr_IP.v4 == *(Uint32*)data)))
    {
        g_session_list[ptr_session->session_handle / 8] |= 1 << (ptr_session->session_handle % 8);
    }

    return PP_RC_SUCCESS;
}

AVALANCHE_PP_RET_e handler_active_sessions_ipv4_src( AVALANCHE_PP_SESSION_INFO_t *  ptr_session, Ptr data )
{
    if (((ptr_session->ingress.dev_type == AVALANCHE_PP_DEV_TYPE_WAN) && (ptr_session->ingress.lookup.LUT2.u.fields.WAN_addr_IP.v4== *(Uint32*)data)) ||
        ((ptr_session->ingress.dev_type == AVALANCHE_PP_DEV_TYPE_LAN) && (ptr_session->ingress.lookup.LUT1.u.fields.L3.LAN_addr_IP.v4 == *(Uint32*)data)))
    {
        g_session_list[ptr_session->session_handle / 8] |= 1 << (ptr_session->session_handle % 8);
    }

    return PP_RC_SUCCESS;
}

AVALANCHE_PP_RET_e handler_active_sessions_ipv4_all( AVALANCHE_PP_SESSION_INFO_t *  ptr_session, Ptr data )
{
    handler_active_sessions_ipv4_dst(ptr_session, data);
    handler_active_sessions_ipv4_src(ptr_session, data);
    return PP_RC_SUCCESS;
}

AVALANCHE_PP_RET_e handler_active_sessions_ipv6_dst( AVALANCHE_PP_SESSION_INFO_t *  ptr_session, Ptr data )
{
    if (((ptr_session->ingress.dev_type == AVALANCHE_PP_DEV_TYPE_WAN) && (ipv6_addr_cmp((struct in6_addr*)data, (struct in6_addr*)ptr_session->ingress.lookup.LUT1.u.fields.L3.LAN_addr_IP.v6) == 0)) ||
        ((ptr_session->ingress.dev_type == AVALANCHE_PP_DEV_TYPE_LAN) && (ipv6_addr_cmp((struct in6_addr*)data, (struct in6_addr*)ptr_session->ingress.lookup.LUT2.u.fields.WAN_addr_IP.v6) == 0)))
    {
        g_session_list[ptr_session->session_handle / 8] |= 1 << (ptr_session->session_handle % 8);
    }

    return PP_RC_SUCCESS;
}

AVALANCHE_PP_RET_e handler_active_sessions_ipv6_src( AVALANCHE_PP_SESSION_INFO_t *  ptr_session, Ptr data )
{
    if (((ptr_session->ingress.dev_type == AVALANCHE_PP_DEV_TYPE_WAN) && (ipv6_addr_cmp((struct in6_addr*)data, (struct in6_addr*)ptr_session->ingress.lookup.LUT2.u.fields.WAN_addr_IP.v6) == 0)) ||
        ((ptr_session->ingress.dev_type == AVALANCHE_PP_DEV_TYPE_LAN) && (ipv6_addr_cmp((struct in6_addr*)data, (struct in6_addr*)ptr_session->ingress.lookup.LUT1.u.fields.L3.LAN_addr_IP.v6) == 0)))
    {
        g_session_list[ptr_session->session_handle / 8] |= 1 << (ptr_session->session_handle % 8);
    }

    return PP_RC_SUCCESS;
}

AVALANCHE_PP_RET_e handler_active_sessions_ipv6_all( AVALANCHE_PP_SESSION_INFO_t *  ptr_session, Ptr data )
{
    handler_active_sessions_ipv6_dst(ptr_session, data);
    handler_active_sessions_ipv6_src(ptr_session, data);
    return PP_RC_SUCCESS;
}

AVALANCHE_PP_RET_e handler_active_sessions_tcp( AVALANCHE_PP_SESSION_INFO_t *  ptr_session, Ptr data )
{
    if (ptr_session->ingress.lookup.LUT1.u.fields.L3.ip_protocol == IPPROTO_TCP || ptr_session->egress.ip_protocol == IPPROTO_TCP)
    {
        g_session_list[ptr_session->session_handle / 8] |= 1 << (ptr_session->session_handle % 8);
    }
    return PP_RC_SUCCESS;
}

AVALANCHE_PP_RET_e handler_active_sessions_udp( AVALANCHE_PP_SESSION_INFO_t *  ptr_session, Ptr data )
{
    if (ptr_session->ingress.lookup.LUT1.u.fields.L3.ip_protocol == IPPROTO_UDP || ptr_session->egress.ip_protocol == IPPROTO_UDP)
    {
        g_session_list[ptr_session->session_handle / 8] |= 1 << (ptr_session->session_handle % 8);
    }
    return PP_RC_SUCCESS;
}

AVALANCHE_PP_RET_e handler_active_sessions_dslite( AVALANCHE_PP_SESSION_INFO_t *  ptr_session, Ptr data )
{
    if (ptr_session->ingress.lookup.LUT1.u.fields.L3.entry_type == AVALANCHE_PP_LUT_ENTRY_L3_DSLITE || ptr_session->egress.tunnel_type == AVALANCHE_PP_LUT_ENTRY_L3_DSLITE)
    {
        g_session_list[ptr_session->session_handle / 8] |= 1 << (ptr_session->session_handle % 8);
    }
    return PP_RC_SUCCESS;
}

AVALANCHE_PP_RET_e handler_active_sessions_gre( AVALANCHE_PP_SESSION_INFO_t *  ptr_session, Ptr data )
{
    if (ptr_session->ingress.lookup.LUT1.u.fields.L3.entry_type == AVALANCHE_PP_LUT_ENTRY_L3_GRE || ptr_session->egress.tunnel_type == AVALANCHE_PP_LUT_ENTRY_L3_GRE)
    {
        g_session_list[ptr_session->session_handle / 8] |= 1 << (ptr_session->session_handle % 8);
    }
    return PP_RC_SUCCESS;
}

AVALANCHE_PP_RET_e handler_active_sessions_drop( AVALANCHE_PP_SESSION_INFO_t *  ptr_session, Ptr data )
{
    AVALANCHE_PP_VPID_INFO_t * ptr_vpid;

    avalanche_pp_vpid_get_info(ptr_session->egress.vpid_handle, &ptr_vpid);
    if (ptr_vpid->flags & (AVALANCHE_PP_VPID_FLG_TX_DISBL | AVALANCHE_PP_VPID_FLG_RX_DISBL))
    {
        g_session_list[ptr_session->session_handle / 8] |= 1 << (ptr_session->session_handle % 8);
    }
    return PP_RC_SUCCESS;
}


/**************************************************************************************/
/*! \fn static int SESSION_read_proc (char *page, char **start, off_t off, int count,int *eof, void *data)
 **************************************************************************************
 *  \brief This function reads extended session information
 *  \return return the number of bytes wirtten to the buffer.
 **************************************************************************************/
static int SESSION_read_proc(char *page, char **start, off_t off, int count, int *eof, void *data)
{ 
    int           len = 0;
    int           prntRtrn;
    int           curr_off = 0;
    static Uint8  endOfData = 0;
    int           argc = 0;
    char *        ptr_next_tok;
    char *        delimitters = " \n\t";
    char *        ptr_cmd;
    AVALANCHE_EXEC_HOOK_FN_t handler = NULL;
    static Uint32 i, j;
    static Uint32 first, last, total;
    Ptr session_data = NULL;
    Uint8   mac[6];
    Uint32  ipv4;
    struct in6_addr ipv6;
    
    if (endOfData == 1)
    {
        endOfData = 0;
        return 0;
    }
    memset((void *)&g_proc_session_argv[0], 0, sizeof(g_proc_session_argv));

    ptr_next_tok = g_proc_session_cmd;      /* Extract the first command */

    /* Parse all the commands typed */
    while (1)
    {
        ptr_cmd = strsep(&ptr_next_tok, delimitters);
        if (ptr_cmd == NULL)
        {
            /* 'strsep' returns null if there was no tok when it gets to '\0' */
            break;
        }

        g_proc_session_argv[argc++] = ptr_cmd;

        if (ptr_next_tok == NULL)
        {
            /* no next tok*/
            break;
        }
        /* Validate if the user entered more commands.*/
        if (argc >= 10)
        {
            prntRtrn = snprintf(buff+len, BUFF_FREE(len), "\nERROR: Incorrect, too many parameters dropping the command\n");CHECK_SNPRINTF(prntRtrn, len);
            goto SESSION_read_proc_end;
        }
    }
 
    /* Display Command Handlers */
    if (strncmp(g_proc_session_argv[0], "all", strlen("all")) == 0)
    {
        handler = handler_active_sessions_all;
    }
    else if (strncmp(g_proc_session_argv[0], "wan2lan", strlen("wan2lan")) == 0)
    {
        handler = handler_active_sessions_wan2lan;
    }
    else if (strncmp(g_proc_session_argv[0], "lan2wan", strlen("lan2wan")) == 0)
    {
        handler = handler_active_sessions_lan2wan;
    }
    else if (strncmp(g_proc_session_argv[0], "lan2lan", strlen("lan2lan")) == 0)
    {
        handler = handler_active_sessions_lan2lan;
    }
    else if (strncmp(g_proc_session_argv[0], "mac", strlen("mac")) == 0)
    {
        unsigned int mac_len = 0;
        Int8 *endptr;

        if (argc != 3)
        {
            prntRtrn = snprintf(buff+len, BUFF_FREE(len), "\nERROR: Incorrect number of parameters for mac command\n");CHECK_SNPRINTF(prntRtrn, len);
            goto SESSION_read_proc_end;
        }

        if (strncmp(g_proc_session_argv[1], "da", strlen("da")) == 0)
        {
            handler = handler_active_sessions_mac_da;
        }
        else if (strncmp(g_proc_session_argv[1], "sa", strlen("sa")) == 0)
        {
            handler = handler_active_sessions_mac_sa;
        }
        else if (strncmp(g_proc_session_argv[1], "all", strlen("all")) == 0)
        {
            handler = handler_active_sessions_mac_all;
        }
        else
        {
            prntRtrn = snprintf(buff+len, BUFF_FREE(len), "\nERROR: Incorrect option for mac command\n");CHECK_SNPRINTF(prntRtrn, len);
            goto SESSION_read_proc_end;
        }
        
        endptr = g_proc_session_argv[2];   /* Copy the MAC */
        while (True)
        {
            Uint8 val;
            val = (Uint8)simple_strtol(endptr, (char**)&endptr, 16);
            if ((*endptr != ':') && (*endptr != '-') && (*endptr != '\0'))
            {
                prntRtrn = snprintf(buff+len, BUFF_FREE(len), "\nERROR: Incorrect parameter for mac command\n");CHECK_SNPRINTF(prntRtrn, len);
                goto SESSION_read_proc_end;
            }
            mac[mac_len++] = val;
            if ((*endptr == '\0') || (mac_len == 6))
            {
                break;
            }
            endptr++;
        }
        session_data = mac;
    }
    else if (strncmp(g_proc_session_argv[0], "ipv4", strlen("ipv4")) == 0)
    {
        unsigned int ip_len = 0;
        Int8 *endptr;

        if (argc != 3)
        {
            prntRtrn = snprintf(buff+len, BUFF_FREE(len), "\nERROR: Incorrect number of parameters for ipv4 command\n");CHECK_SNPRINTF(prntRtrn, len);
            goto SESSION_read_proc_end;
        }

        if (strncmp(g_proc_session_argv[1], "dst", strlen("dst")) == 0)
        {
            handler = handler_active_sessions_ipv4_dst;
        }
        else if (strncmp(g_proc_session_argv[1], "src", strlen("src")) == 0)
        {
            handler = handler_active_sessions_ipv4_src;
        }
        else if (strncmp(g_proc_session_argv[1], "all", strlen("all")) == 0)
        {
            handler = handler_active_sessions_ipv4_all;
        }
        else
        {
            prntRtrn = snprintf(buff+len, BUFF_FREE(len), "\nERROR: Incorrect option for ipv4 command\n");CHECK_SNPRINTF(prntRtrn, len);
            goto SESSION_read_proc_end;
        }
        
        endptr = g_proc_session_argv[2];   /* Copy the IPv4 */
        ip_len = 0;
        while (True)
        {
            Uint8 val;
            val = (Uint8)simple_strtol(endptr, (char**)&endptr, 10);
            if ((*endptr != '.') && (*endptr != '\0'))
            {
                prntRtrn = snprintf(buff+len, BUFF_FREE(len), "\nERROR: Incorrect parameter for ipv4 command\n");CHECK_SNPRINTF(prntRtrn, len);
                goto SESSION_read_proc_end;
            }
            ipv4 |= val << (8*(3-ip_len));
            ip_len++;
            if ((*endptr == '\0') || (ip_len == 4))
            {
                break;
            }
            endptr++;
        }
        session_data = (Uint8*)&ipv4;
    }
    else if (strncmp(g_proc_session_argv[0], "ipv6", strlen("ipv6")) == 0)
    {
        unsigned int ip_len = 0;
        Int8 *endptr;

        if (argc != 3)
        {
            prntRtrn = snprintf(buff+len, BUFF_FREE(len), "\nERROR: Incorrect number of parameters for ipv6 command\n");CHECK_SNPRINTF(prntRtrn, len);
            goto SESSION_read_proc_end;
        }

        if (strncmp(g_proc_session_argv[1], "dst", strlen("dst")) == 0)
        {
            handler = handler_active_sessions_ipv6_dst;
        }
        else if (strncmp(g_proc_session_argv[1], "src", strlen("src")) == 0)
        {
            handler = handler_active_sessions_ipv6_src;
        }
        else if (strncmp(g_proc_session_argv[1], "all", strlen("all")) == 0)
        {
            handler = handler_active_sessions_ipv6_all;
        }
        else
        {
            prntRtrn = snprintf(buff+len, BUFF_FREE(len), "\nERROR: Incorrect option for ipv6 command\n");CHECK_SNPRINTF(prntRtrn, len);
            goto SESSION_read_proc_end;
        }
        
        endptr = g_proc_session_argv[2];   /* Copy the IPv4 */
        ip_len = 0;
        while (True)
        {
            Uint16 val;
            val = (Uint16)simple_strtol(endptr, (char**)&endptr, 16);
            if ((*endptr != ':') && (*endptr != '\0'))
            {
                prntRtrn = snprintf(buff+len, BUFF_FREE(len), "\nERROR: Incorrect parameter for ipv6 command\n");CHECK_SNPRINTF(prntRtrn, len);
                goto SESSION_read_proc_end;
            }
            ipv6.in6_u.u6_addr16[ip_len++] = val;
            if ((*endptr == '\0') || (ip_len == 8))
            {
                break;
            }
            endptr++;
        }
        session_data = (Uint8*)&ipv6;
    }
    else if (strncmp(g_proc_session_argv[0], "tcp", strlen("tcp")) == 0)
    {
        handler = handler_active_sessions_tcp;
    }
    else if (strncmp(g_proc_session_argv[0], "udp", strlen("udp")) == 0)
    {
        handler = handler_active_sessions_udp;
    }
    else if (strncmp(g_proc_session_argv[0], "dslite", strlen("dslite")) == 0)
    {
        handler = handler_active_sessions_dslite;
    }
    else if (strncmp(g_proc_session_argv[0], "gre", strlen("gre")) == 0)
    {
        handler = handler_active_sessions_gre;
    }
    else if (strncmp(g_proc_session_argv[0], "drop", strlen("drop")) == 0)
    {
        handler = handler_active_sessions_drop;
    }

    if (handler != NULL)
    {
        Uint32 curr;
        Uint32 bytes = 0;
        Bool do_print = False;

        if (off == 0)
        {
            i = j = total = 0;
            first = last = AVALANCHE_PP_MAX_ACCELERATED_SESSIONS;
            memset(g_session_list, 0, sizeof(g_session_list));
            avalanche_pp_session_list_execute(AVALANCHE_PP_MAX_VPID, PP_LIST_ID_ALL, handler, session_data);
        }
        
        for (; i < AVALANCHE_PP_MAX_ACCELERATED_SESSIONS / 8; i++)
        {
            if (do_print)
            {
                WR_TO_PAGE
                *start = page;
                *eof = 1;
                return curr_off; 
            }
            j %= 8;
            for (; j < 8; j++) 
            {
                if (do_print)
                {
                    WR_TO_PAGE
                    *start = page;
                    *eof = 1;
                    return curr_off; 
                }
                if (g_session_list[i] & (1 << j)) 
                {
                    total++;
                    curr = (i * 8) + j;
                    if (first == AVALANCHE_PP_MAX_ACCELERATED_SESSIONS)
                    {
                        first = last = curr;
                    }
                    else 
                    {
                        if (curr != last + 1)
                        {
                            if (first == last)
                            {
                                prntRtrn = snprintf(buff + len, BUFF_FREE(len), "%d ", first); CHECK_SNPRINTF(prntRtrn, len);
                                bytes += 5;
                            }
                            else
                            {
                                prntRtrn = snprintf(buff + len, BUFF_FREE(len), "[%d-%d] ", first, last); CHECK_SNPRINTF(prntRtrn, len); 
                                bytes += 12;
                            }
                            if (bytes > 100)
                            {
                                prntRtrn = snprintf(buff + len, BUFF_FREE(len), "\n"); CHECK_SNPRINTF(prntRtrn, len);
                                do_print = True;
                            }
                            first = curr; 
                        }
                        last = curr;
                    }
                }
            }
        }
        if (first != AVALANCHE_PP_MAX_ACCELERATED_SESSIONS)
        {
            if (first == last)
            {
                prntRtrn = snprintf(buff + len, BUFF_FREE(len), "%d ", curr); CHECK_SNPRINTF(prntRtrn, len); 
            }
            else
            {
                prntRtrn = snprintf(buff + len, BUFF_FREE(len), "[%d-%d] ", first, last); CHECK_SNPRINTF(prntRtrn, len); 
            }
        }

        prntRtrn = snprintf(buff + len, BUFF_FREE(len), "\nTotal matching sessions: %d\n", total); CHECK_SNPRINTF(prntRtrn, len); 
    }

SESSION_read_proc_end:
    WR_TO_PAGE
    *start = page;
    *eof = 1;
    endOfData = 1;
    return curr_off;
}

/**************************************************************************/
/*! \fn static int QOS_QUEUE_write_proc(struct file *fp, const char * buf, unsigned long count, void * data)                                     
 **************************************************************************
 *  \brief This function sets the queue Id for the XQQ_read_proc
 *  \return OK or error status.
 **************************************************************************/
static int QOS_QUEUE_write_proc(struct file *fp, const char * buf, unsigned long count, void * data)
{
    unsigned char local_buf[10];
	int ret_val = 0;

	if (count > 10)
	{
		printk(KERN_ERR "\n%s[%d]: Buffer Overflow\n", __FUNCTION__, __LINE__);
		return -EFAULT;
	}

	if (copy_from_user(local_buf, buf, count))
    {
        return -EFAULT;
    }

	local_buf[count - 1]='\0'; 
    ret_val = count;
	sscanf(local_buf, "%d" ,&g_proc_queue_id);

	return ret_val;
}


/**************************************************************************************/
/*! \fn static int QOS_QUEUE_read_proc (char *page, char **start, off_t off, int count,int *eof, void *data)
 **************************************************************************************
 *  \brief This function reads extended QoS Queue information
 *  \return return the number of bytes wirtten to the buffer.
 **************************************************************************************/
static int QOS_QUEUE_read_proc(char *page, char **start, off_t off, int count, int *eof, void *data)
{ 
    int           len = 0;
    int           prntRtrn;
    int           curr_off = 0;
    static Uint8  endOfData = 0;
    
    if (endOfData == 1)
    {
        endOfData = 0;
        return 0;
    }
    
   if (g_proc_queue_id <= AVALANCHE_PP_QOS_QUEUE_MAX_INDX)
   {
       prntRtrn = snprintf(buff + len, BUFF_FREE( len), "\n"); CHECK_SNPRINTF(prntRtrn, len);
       prntRtrn = snprintf(buff + len, BUFF_FREE( len), "###############################\n"); CHECK_SNPRINTF(prntRtrn, len);
       prntRtrn = snprintf(buff + len, BUFF_FREE( len), "# QoS Queue %03d extended info #\n", g_proc_queue_id); CHECK_SNPRINTF(prntRtrn, len);
       prntRtrn = snprintf(buff + len, BUFF_FREE( len), "###############################\n\n"); CHECK_SNPRINTF(prntRtrn, len);
       pp_hal_display_qos_queue_info(g_proc_queue_id, buff+len, &prntRtrn); CHECK_SNPRINTF(prntRtrn, len);
       WR_TO_PAGE
   }
    

    /* Set next begin of page */   
    *start = page;
    *eof = 1;
    if (curr_off > 0)
    {
        endOfData = 1;
    }

    /* Return number of bytes writen */
    return curr_off ;
}
/**************************************************************************/
/*! \fn static int QOS_CLUSTER_write_proc(struct file *fp, const char * buf, unsigned long count, void * data)                                     
 **************************************************************************
 *  \brief This function sets the cluster Id for the XQQ_read_proc
 *  \return OK or error status.
 **************************************************************************/
static int QOS_CLUSTER_write_proc(struct file *fp, const char * buf, unsigned long count, void * data)
{
    unsigned char local_buf[10];
	int ret_val = 0;

	if (count > 10)
	{
		printk(KERN_ERR "\n%s[%d]: Buffer Overflow\n", __FUNCTION__, __LINE__);
		return -EFAULT;
	}

	if (copy_from_user(local_buf, buf, count))
    {
        return -EFAULT;
    }

	local_buf[count - 1]='\0'; 
    ret_val = count;
	sscanf(local_buf, "%d" ,&g_proc_cluster_id);

	return ret_val;
}


/**************************************************************************************/
/*! \fn static int QOS_CLUSTER_read_proc (char *page, char **start, off_t off, int count,int *eof, void *data)
 **************************************************************************************
 *  \brief This function reads extended QoS Cluster information
 *  \return return the number of bytes wirtten to the buffer.
 **************************************************************************************/
static int QOS_CLUSTER_read_proc(char *page, char **start, off_t off, int count, int *eof, void *data)
{ 
    int           len = 0;
    int           prntRtrn;
    int           curr_off = 0;
    static Uint8  endOfData = 0;
    
    if (endOfData == 1)
    {
        endOfData = 0;
        return 0;
    }
    
   if (g_proc_cluster_id <= AVALANCHE_PP_QOS_CLST_MAX_INDX)
   {
       prntRtrn = snprintf(buff + len, BUFF_FREE( len), "\n"); CHECK_SNPRINTF(prntRtrn, len);
       prntRtrn = snprintf(buff + len, BUFF_FREE( len), "################################\n"); CHECK_SNPRINTF(prntRtrn, len);
       prntRtrn = snprintf(buff + len, BUFF_FREE( len), "# QoS Cluster %02d extended info #\n", g_proc_cluster_id); CHECK_SNPRINTF(prntRtrn, len);
       prntRtrn = snprintf(buff + len, BUFF_FREE( len), "################################\n\n"); CHECK_SNPRINTF(prntRtrn, len);
       pp_hal_display_qos_cluster_info(g_proc_cluster_id, buff+len, &prntRtrn); CHECK_SNPRINTF(prntRtrn, len);
       WR_TO_PAGE
   }
    
    /* Set next begin of page */   
    *start = page;
    *eof = 1;
    if (curr_off > 0)
    {
        endOfData = 1;
    }

    /* Return number of bytes writen */
    return curr_off ;
}

/**************************************************************************/
/*! \fn static int VPID_LISTS_write_proc(struct file *fp, const char * buf, unsigned long count, void * data)                                     
 **************************************************************************
 *  \brief This function sets the VPID Id for the VPID_LISTS_read_proc
 *  \return OK or error status.
 **************************************************************************/
static int VPID_LISTS_write_proc(struct file *fp, const char * buf, unsigned long count, void * data)
{
    unsigned char local_buf[10];
	int ret_val = 0;

	if (count > 10)
	{
		printk(KERN_ERR "\n%s[%d]: Buffer Overflow\n", __FUNCTION__, __LINE__);
		return -EFAULT;
	}

	if (copy_from_user(local_buf, buf, count))
    {
        return -EFAULT;
    }

	local_buf[count - 1]='\0'; 
    ret_val = count;
	sscanf(local_buf, "%d" ,&g_proc_vpid_id);

	return ret_val;
}

/**************************************************************************/
/*! \fn static int SET_REASSEMBLY_DB_TIMEOUT_proc(struct file *fp, const char * buf, unsigned long count, void * data)                                     
 **************************************************************************
 *  \brief This function sets the Reassembl DB timeout in PP
 *  \return OK or error status.
 **************************************************************************/
static int SET_REASSEMBLY_DB_TIMEOUT_proc(struct file *fp, const char * buf, unsigned long count, void * data)
{
    unsigned char local_buf[10];
	int ret_val = 0;
    Int32 userVar;
    Int32 tmp;

	if (count > 10)
	{
		printk(KERN_ERR "\n%s[%d]: Buffer Overflow\n", __FUNCTION__, __LINE__);
		return -EFAULT;
	}

	if (copy_from_user(local_buf, buf, count))
    {
        return -EFAULT;
    }

	local_buf[count - 1]='\0'; 
    ret_val = count;
	sscanf(local_buf, "%d" ,&userVar);
       
    Uint8* dest = (Uint8*) IO_PHY2VIRT(PP_HAL_REASSEMBLY_DB_TIMEOUT_THRESHOLD_ADD);// Reassembly Timeout counters threshold
    tmp = *dest;
    tmp &= PP_HAL_REASSEMBLY_DB_TIMEOUT_THRESHOLD_MASK;                            // We want to change only the first 8bit
    tmp |= userVar;
    *dest = tmp;
	return ret_val;
}

static void         __module_pp_exit (void)
{
    printk ( KERN_INFO " MODULE: PP Stop  ...\n");

    /* Freeing the major number */
    unregister_chrdev ( DEVICE_MAJOR , DEVICE_NAME );
}

static int __init   __module_pp_init (void)
{
    struct proc_dir_entry *dir = (struct proc_dir_entry *)init_net.proc_net;
    struct proc_dir_entry *dir_PID;
    struct proc_dir_entry *dir_VPID;
    struct proc_dir_entry *dir_VPIDLISTS;
    struct proc_dir_entry *dir_REASSDBTIMEOUT;
    struct proc_dir_entry *dir_LUT1;
    struct proc_dir_entry *dir_LUT2;
    struct proc_dir_entry *dir_HAL;
    struct proc_dir_entry *dir_SESSION;
    struct proc_dir_entry *dir_XSESSION;
    struct proc_dir_entry *dir_XQQ;
    struct proc_dir_entry *dir_XQC;

    printk ( " MODULE: PP Start ...\n");

    /* ************************************************************************ */
    /*                                                                          */
    /*         Interface device initialization stuff ....                       */
    /*                                                                          */
    /* ************************************************************************ */

    /* Registering device */
    if ( register_chrdev ( DEVICE_MAJOR , DEVICE_NAME , &ppCdevFops ) < 0 )
    {
        printk ( KERN_WARNING "memory: cannot obtain major number %d\n", DEVICE_MAJOR );
        return -EIO;
    }

    avalanche_pp_hw_init();
    pp_hal_init();
    pp_db_init();

    PP_DB.status = PP_DB_STATUS_ACTIVE;

#ifdef CONFIG_TI_HIL_PROFILE_INTRUSIVE_PP2K
    {
        extern TI_HIL_PROFILE hil_intrusive_profile;

        /* Load the Intrusive mode HIL Profile for the system */
        if (ti_hil_register_profile(&hil_intrusive_profile) < 0)
            return -1;
    }
#endif /* CONFIG_TI_HIL_PROFILE_INTRUSIVE */

    avalanche_pp_event_poll_timer_init();

    /******************************************************************/
    /* Init all proc's files */
    /* create the "PP" directory in "net" directory */
    if (NULL != (dir = proc_mkdir("pp",dir)))
    {
        /********** PIDs proc's init **********/             
        if (NULL == (dir_PID = proc_mkdir("PID",dir))) {   printk("%s:%d ERROR ....\n",__FUNCTION__,__LINE__); return -1;  }
        if (NULL == (create_proc_read_entry( "busy" , 0, dir_PID, PID_BUSY_read_proc, (void *)0 ))) {   printk("%s:%d ERROR ....\n",__FUNCTION__,__LINE__); return -1;  }
        if (NULL == (create_proc_read_entry( "free" , 0, dir_PID, PID_FREE_read_proc, (void *)0 ))) {   printk("%s:%d ERROR ....\n",__FUNCTION__,__LINE__); return -1;  }     
       
        /********** VPIDs proc's init *********/   
        if (NULL == (dir_VPID = proc_mkdir("VPID",dir))) {   printk("%s:%d ERROR ....\n",__FUNCTION__,__LINE__); return -1;  }
        if (NULL == (create_proc_read_entry( "busy" , 0, dir_VPID, VPID_BUSY_read_proc, (void *)0 ))) {   printk("%s:%d ERROR ....\n",__FUNCTION__,__LINE__); return -1;  }
        if (NULL == (create_proc_read_entry( "free" , 0, dir_VPID, VPID_FREE_read_proc, (void *)0 ))) {   printk("%s:%d ERROR ....\n",__FUNCTION__,__LINE__); return -1;  }
        if (NULL == (create_proc_read_entry( "ingress" , 0, dir_VPID, VPID_LISTS_read_proc, (void *)PP_LIST_ID_INGRESS ))) {   printk("%s:%d ERROR ....\n",__FUNCTION__,__LINE__); return -1;  }
        if (NULL == (create_proc_read_entry( "egress" , 0, dir_VPID, VPID_LISTS_read_proc, (void *)PP_LIST_ID_EGRESS ))) {   printk("%s:%d ERROR ....\n",__FUNCTION__,__LINE__); return -1;  }
        if (NULL == (create_proc_read_entry( "tcp" , 0, dir_VPID, VPID_LISTS_read_proc, (void *)PP_LIST_ID_EGRESS_TCP ))) {   printk("%s:%d ERROR ....\n",__FUNCTION__,__LINE__); return -1;  }
        if (NULL == (create_proc_read_entry( "tdox" , 0, dir_VPID, VPID_LISTS_read_proc, (void *)PP_LIST_ID_EGRESS_TDOX ))) {   printk("%s:%d ERROR ....\n",__FUNCTION__,__LINE__); return -1;  } 
        /********** LUT1s proc's init *********/   
        if (NULL == (dir_LUT1 = proc_mkdir("LUT1",dir))) {   printk("%s:%d ERROR ....\n",__FUNCTION__,__LINE__); return -1;  }
        if (NULL == (create_proc_read_entry( "busy" , 0, dir_LUT1, LUT1_BUSY_read_proc, (void *)0 ))) {   printk("%s:%d ERROR ....\n",__FUNCTION__,__LINE__); return -1;  }
        if (NULL == (create_proc_read_entry( "free" , 0, dir_LUT1, LUT1_FREE_read_proc, (void *)0 ))) {   printk("%s:%d ERROR ....\n",__FUNCTION__,__LINE__); return -1;  }

        /********** LUT2s proc's init *********/   
        if (NULL == (dir_LUT2 = proc_mkdir("LUT2",dir))) {   printk("%s:%d ERROR ....\n",__FUNCTION__,__LINE__); return -1;  }
        if (NULL == (create_proc_read_entry( "busy" , 0, dir_LUT2, LUT2_BUSY_read_proc, (void *)0 ))) {   printk("%s:%d ERROR ....\n",__FUNCTION__,__LINE__); return -1;  }
        if (NULL == (create_proc_read_entry( "free" , 0, dir_LUT2, LUT2_FREE_read_proc, (void *)0 ))) {   printk("%s:%d ERROR ....\n",__FUNCTION__,__LINE__); return -1;  }

        /******************************************************************/
        if (NULL == (create_proc_read_entry( "global" , 0, dir, GLOBAL_read_proc, (void *)0 ))) {   printk("%s:%d ERROR ....\n",__FUNCTION__,__LINE__); return -1;  }
        if (NULL == (create_proc_read_entry( "brief" , 0, dir, BRIEF_read_proc, (void *)0 ))) {   printk("%s:%d ERROR ....\n",__FUNCTION__,__LINE__); return -1;  }
        if (NULL == (create_proc_read_entry( "version" , 0, dir, VER_read_proc, (void *)0 ))) {   printk("%s:%d ERROR ....\n",__FUNCTION__,__LINE__); return -1;  }

        if (NULL == (dir_HAL = proc_mkdir("HAL",dir))) {   printk("%s:%d ERROR ....\n",__FUNCTION__,__LINE__); return -1;  }

        dir_XSESSION = create_proc_entry("xsession" ,0644, dir_HAL);
        dir_XSESSION->read_proc  = XSESSION_read_proc;
        dir_XSESSION->write_proc = XSESSION_write_proc;

        dir_XQQ = create_proc_entry("qosQueue" ,0644, dir_HAL);
        dir_XQQ->read_proc  = QOS_QUEUE_read_proc;
        dir_XQQ->write_proc = QOS_QUEUE_write_proc;

        dir_XQC = create_proc_entry("qosCluster" ,0644, dir_HAL);
        dir_XQC->read_proc  = QOS_CLUSTER_read_proc;
        dir_XQC->write_proc = QOS_CLUSTER_write_proc;

        dir_VPIDLISTS = create_proc_entry("vpidLists" ,0644, dir);
        dir_VPIDLISTS->write_proc = VPID_LISTS_write_proc;

        dir_REASSDBTIMEOUT = create_proc_entry("setReassDbTimeout" ,0644, dir);
        dir_REASSDBTIMEOUT->write_proc = SET_REASSEMBLY_DB_TIMEOUT_proc;

        dir_SESSION = create_proc_entry("session" ,0644, dir_HAL);
        dir_SESSION->read_proc  = SESSION_read_proc;
        dir_SESSION->write_proc = SESSION_write_proc;


    }
    else
    {
        printk(KERN_ALERT "Error: Could not create /proc/net/pp/ \n");
        return -ENOMEM;
    }
    /******************************************************************/
    
    return 0;
}



module_init(__module_pp_init);
module_exit(__module_pp_exit);

