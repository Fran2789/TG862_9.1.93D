/*

Copyright (c) 2013-2017 ARRIS Enterprises, LLC

All rights reserved.

Redistribution and use in source and binary forms, with or without 
modification, are permitted provided that the following conditions are met:

1. Redistributions of source code must retain the above copyright notice, 
   this list of conditions and the following disclaimer.
2. Redistributions in binary form must reproduce the above copyright notice, 
   this list of conditions and the following disclaimer in the documentation
   and/or other materials provided with the distribution.
3. The name of the author may not be used to endorse or promote products 
   derived from this software without specific prior written permission.


Alternatively, this software may be distributed under the terms of the
GNU General Public License ("GPL") version 2 as published by the Free
Software Foundation. 
 

THIS SOFTWARE IS PROVIDED BY THE AUTHOR ``AS IS'' AND ANY EXPRESS OR IMPLIED 
WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED WARRANTIES OF 
MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE DISCLAIMED. 
IN NO EVENT SHALL THE AUTHOR BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, 
SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, 
PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; 
OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, 
WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR 
OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF 
ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.

GPLv2 license:

This program is free software; you can redistribute it and/or modify it 
under the terms of the GNU General Public License as published by the 
Free Software Foundation; either version 2 of the License, or 
(at your option) any later version.

This program is distributed in the hope that it will be useful, but 
WITHOUT ANY WARRANTY; without even the implied warranty of MERCHANTABILITY 
or FITNESS FOR A PARTICULAR PURPOSE. See the GNU General Public License 
for more details.

You should have received a copy of the GNU General Public License along with 
this program; if not, write to the Free Software Foundation, Inc., 
51 Franklin Street, Fifth Floor, Boston, MA 02110-1301 USA.

*/

#define DRV_NAME        "RTK83xx realtek switch interrupt handler"
#define DRV_VERSION     "0.0.1"

#include <autoconf.h>
#include <linux/module.h>
#include <linux/kernel.h>
#include <linux/list.h>
#include <linux/sched.h>
#include <linux/interrupt.h>
#include <linux/cdev.h>
#include <linux/eventfd.h>
#include <linux/fs.h>
#include <linux/sockios.h>
#include <linux/mutex.h>
#include <asm/uaccess.h>
#include <asm/irq.h>
#include <asm-arm/arch-avalanche/puma6/puma6.h>

#include <arris/arris_models.h>

/* REALTEK API */
#include "rtk_api.h"
#include "rtk_api_ext.h"
#include "rtl8367b_asicdrv.h"
#include "rtl8367b_asicdrv_port.h"

/* Kernel module API */
#include "extswt_lkm.h"

//#define RTK83XX_DEBUG

/* STP */
#include "swt_stp.h"

#include "rldp.h"
#include <arris/arris_mod_api.h>
#if defined( CONFIG_SYSTEM_MOCA ) || defined ( CONFIG_VENDOR_ARRIS_ENTROPIC_MOCA )
#include <linux/rcupdate.h>
#include <linux/proc_fs.h>
#include <asm/uaccess.h>
#endif

#ifdef RTK83XX_DEBUG
/* note: prints function name for you */
#define DPRINTK(fmt, args...) printk( KERN_ERR "%s: " fmt, __FUNCTION__ , ## args)
#else
#define DPRINTK(fmt, args...)
#endif

MODULE_AUTHOR ("Arris");
MODULE_DESCRIPTION (DRV_NAME);
MODULE_LICENSE("Dual BSD/GPL");
MODULE_VERSION(DRV_VERSION);

/************************************************************************/
/*     RTK switch driver private data                                   */
/************************************************************************/
typedef struct _rtk83xx_vlan_t {
    rtk_vlan_t          vid;
    unsigned char       member_msk;
    unsigned char       untag_msk;
    rtk_fid_t           fid;
    struct list_head    list;
} rtk83xx_vlan_t;

typedef struct _rtk83xx_regfd_t {
    int                 efd;
    struct file*        efd_file;
    struct eventfd_ctx* efd_ctx;
    struct list_head    list;
} rtk83xx_regfd_t;

typedef struct _rtk83xx_dev_t {
    struct cdev             cdev;
    char                    name[16];
    int                     irq_registered;
    int                     api_initialized;
    int                     tpid;
    int                     provider[NUM_PORTS];
    EXTSWT_port_status_t    port_status;
    rtk_vlan_t              pvid[NUM_PORTS];
    struct list_head        vlans;
    struct list_head        reg_fds;
    struct mutex            mutex;
} rtk83xx_dev_t;

/************************************************************************/

/* registered device */
static dev_t            rtk83xx_deviceNo;

/* Character device fops */
static int              rtk83xx_open     (struct inode *inode, struct file *filp);
static long             rtk83xx_ioctl    (struct file *filp, unsigned int, unsigned long);
static int              rtk83xx_release  (struct inode *inode, struct file *filp);

/* private IOCTL handlers */
static long             rtk83xx_getport_status(rtk83xx_dev_t *dev, EXTSWT_port_status_t* ps);
static long             rtk83xx_getport_stats(rtk83xx_dev_t *dev, EXTSWT_port_stats_t* ps);
static long             rtk83xx_getport_iftable_stats(rtk83xx_dev_t *dev, EXTSWT_port_iftable_stats_t* ps)  ;
static long             rtk83xx_add_vlan_port(rtk83xx_dev_t *dev, EXTSWT_vlan_add_port_t* ap);
static long             rtk83xx_add_port_tag(rtk83xx_dev_t *dev, EXTSWT_add_port_tag_t* apt);
static long             rtk83xx_clear_port_stats(rtk83xx_dev_t *dev);
static long             rtk83xx_set_eth_eee_mode(rtk83xx_dev_t *dev, EXTSWT_eth_eee_mode_t mode );
static long             rtk83xx_get_eth_eee_mode(rtk83xx_dev_t *dev, EXTSWT_eth_eee_mode_t *mode );
static long             rtk83xx_get_eth_eee_stats(rtk83xx_dev_t *dev, EXTSWT_eth_eee_stats_t *mode );
static long             rtk83xx_get_eth_eee_stats_clear(rtk83xx_dev_t *dev, EXTSWT_eth_eee_stats_clr_t *mode );

//static long             rtk83xx_certification_config(rtk83xx_dev_t *dev);
static long             rtk83xx_stp_enable(rtk83xx_dev_t *dev, EXTSWT_stpcfg_t* stp);
ARRISMOD_EXTERN( int, arris_loop_detected_hook, unsigned char * );
/* Threaded IRQ handler */
static irqreturn_t      rtk83xx_irq(int irq, void *dev);

struct file_operations rtk83xx_fops = {
    .owner          = THIS_MODULE,
    .unlocked_ioctl = rtk83xx_ioctl,
    .open           = rtk83xx_open,
    .release        = rtk83xx_release
};

/* The one and only switch driver */
rtk83xx_dev_t   rtk83xx_dev;

#if defined( CONFIG_SYSTEM_MOCA ) || defined ( CONFIG_VENDOR_ARRIS_ENTROPIC_MOCA )
static struct proc_dir_entry *moca_subagent_dir, *pid_file;
static int pid = 0;
#endif

static int rtk83xx_open(struct inode *inode, struct file *filp)
{
    rtk83xx_dev_t *dev;

    dev = container_of(inode->i_cdev, rtk83xx_dev_t, cdev);

    /* Setup filep private data to point to our device */
    filp->private_data = dev;

    return 0;
}

int rtk83xx_release(struct inode *inode, struct file *filp)
{
    return 0;
}

static void rtk83xx_read_port_status( rtk83xx_dev_t *dev, uint8_t port_mask )
{
    rtk_mode_ext_t          mode;
    rtk_port_mac_ability_t  mac_ability;
    rtk_port_phy_ability_t  phy_ability;
    rtk_port_t              port;

    if (port_mask & 0x40) 
    {
        // get the internal port status
        if ( rtk_port_macForceLinkExt_get( EXT_PORT_1, &mode, &mac_ability ) == RT_ERR_OK )
        {
            if ( mac_ability.link ) 
            {
                dev->port_status.linkStatus[EXTSWT_INTERNAL_PORT] = TRUE;
                dev->port_status.speed[EXTSWT_INTERNAL_PORT] = EXTSWT_PORT_SPEED_1000M;
            }
            else
            {
                dev->port_status.linkStatus[EXTSWT_INTERNAL_PORT] = FALSE;
            }
            dev->port_status.autonegotiate[EXTSWT_INTERNAL_PORT] = FALSE;
        }
    }

    // Then the external ports
    for (port=0; port<5; port++) 
    {
        if (port_mask & 1 << port) 
        {
            if ( rtk_port_phyStatus_get( port, &dev->port_status.linkStatus[port],
                                         &dev->port_status.speed[port], 
                                         &dev->port_status.duplex[port] ) == RT_ERR_OK )
            {
                rtk_port_phyForceModeAbility_get( port, &phy_ability );
                if (phy_ability.AutoNegotiation) 
                {
                    dev->port_status.autonegotiate[port] = TRUE;
                }
                else
                {
                    dev->port_status.autonegotiate[port] = FALSE;
                }
            }
            else
            {
                // can't read port status - return link up so we don't disconnect telnet
                dev->port_status.linkStatus[port] = TRUE;
                dev->port_status.speed[port] = 0;
                dev->port_status.duplex[port] = 0;
            }
        }
    }
}

/* The threaded IRQ exists simply to wait for userspace to complete clearing
 * the interrupt source
 */
static irqreturn_t rtk83xx_irq(int irq, void *dev_instance)
{
    rtk83xx_dev_t *dev = (rtk83xx_dev_t *)dev_instance;
    rtk_int_info_t   link_up;
    rtk_int_info_t   link_down;
    rtk_int_status_t status;
    rtk83xx_regfd_t  *regfd;
    uint32_t         linkstatus_changed = 0;

    mutex_lock( &dev->mutex );

    /* Which interrupt fired */
    rtk_int_status_get(&status);

    if ( status.value[0] & (1<<INT_TYPE_LINK_STATUS) ) 
    {
        /* Then, clear any pending link status on the switch */
        rtk_int_advanceInfo_get(ADV_PORT_LINKDOWN_PORT_MASK, &link_down);
        rtk_int_advanceInfo_get(ADV_PORT_LINKUP_PORT_MASK, &link_up);

        linkstatus_changed = 1;
    }

    /* Then clear the interrupt on the switch */
    status.value[0] = 0xFF;
    status.value[1] = 0xFF;
    rtk_int_status_set(status);

    /* Puma6 - ack the IRQ */
    ack_irq(AVALANCHE_INT_89);

    if( linkstatus_changed )
    {
        /* Update the modified ports */
        rtk83xx_read_port_status( dev, link_up | link_down );

        /* notify registered listeners of link status change */
        list_for_each_entry(regfd, &dev->reg_fds, list)
        {
            if (regfd->efd_ctx)
            {
                eventfd_signal( regfd->efd_ctx, 1 );
            }
        }        

        /* Query whether ports looped */
        if (link_up)
        {
            rtk_rldp_start_query_timer();
        }
    }

    mutex_unlock( &dev->mutex );

    return IRQ_HANDLED;
}


static long rtk83xx_getport_status(rtk83xx_dev_t *dev, EXTSWT_port_status_t* ps)
{
    // Copy current port status to given structure
    memcpy( (char *)ps, (char *)&dev->port_status, sizeof(EXTSWT_port_status_t));
    return 0;
}

static long rtk83xx_getport_stats(rtk83xx_dev_t *dev, EXTSWT_port_stats_t* ps)
{ 
    rtk_stat_port_get( ps->port, STAT_IfInOctets, &ps->rx_bytes );
    rtk_stat_port_get( ps->port, STAT_IfOutOctets, &ps->tx_bytes );

    return 0;
}

static long rtk83xx_getport_iftable_stats(rtk83xx_dev_t *dev, EXTSWT_port_iftable_stats_t* ps)   
{  
    IfDbStats_Entry_t *ifTableStats_p = &ps->ifTableStats;
    rtk_stat_port_cntr_t Port_cntrs;

	rtk_stat_port_getAll_IfTable(ps->port,&Port_cntrs);

    ifTableStats_p->ifInBcastPkts64 = Port_cntrs.ifInBroadcastPkts;   
    ifTableStats_p->ifOutBcastPkts64 = Port_cntrs.ifOutBrocastPkts;  
    ifTableStats_p->ifInUcastPkts64 = Port_cntrs.ifInUcastPkts ;    
    ifTableStats_p->ifOutUcastPkts64 = Port_cntrs.ifOutUcastPkts;
    ifTableStats_p->ifInMcastPkts64 = Port_cntrs.ifInMulticastPkts;     
    ifTableStats_p->ifOutMcastPkts64 = Port_cntrs.ifOutMulticastPkts;
    ifTableStats_p->ifInDiscards = Port_cntrs.etherStatsDropEvents;        
    ifTableStats_p->ifOutErrors = Port_cntrs.dot3StatsExcessiveCollisions + Port_cntrs.dot3StatsLateCollisions;                                       
    ifTableStats_p->ifInErrors = Port_cntrs.dot3StatsSymbolErrors + 
                                 Port_cntrs.dot3StatsFCSErrors + 
                                 Port_cntrs.etherStatsFragments + 
                                 Port_cntrs.etherStatsJabbers;          

    // Total Number of packets received
    ifTableStats_p->ifInOcts64 = Port_cntrs.ifInOctets;
    // Total Number of packets sent
    ifTableStats_p->ifOutOcts64 = Port_cntrs.ifOutOctets;  

    // These stats are not available yet 
    ifTableStats_p->ifInUnknownProtos = 0;   
    ifTableStats_p->ifOutDiscards = 0;       

    return 0;
}

static long rtk83xx_clear_port_stats(rtk83xx_dev_t *dev)
{
    rtk_port_t              port;

    for (port = 0; port < RTK_PORT_ID_MAX; port++)
    {
        rtk_stat_port_reset( port );
    }

    return 0;
}

// port status will be updated when irq is enabled
static long rtk83xx_clear_port_status(rtk83xx_dev_t *dev)
{    
    memset(&dev->port_status, 0x00, sizeof(dev->port_status));
    return 0;
}

static long rtk83xx_setport_mode(rtk83xx_dev_t *dev, EXTSWT_port_mode_t* pm)
{ 
    rtk_port_phy_ability_t ability;

    // first, get the current ability
    rtk_port_phyAutoNegoAbility_get( pm->port, &ability );

    /* Clear out autonegotiate and speed / duplex ability */
    ability.AutoNegotiation = 0;
    ability.Half_10     = 0;
    ability.Full_10     = 0;
    ability.Half_100    = 0;
    ability.Full_100    = 0;
    ability.Full_1000   = 0;

    if (pm->autonegotiate) 
    {
       // first, get the current ability
       rtk_port_phyAutoNegoAbility_get( pm->port, &ability );

       /* Clear out autonegotiate and speed / duplex ability */
       ability.AutoNegotiation = 0;
       ability.Half_10     = 0;
       ability.Full_10     = 0;
       ability.Half_100    = 0;
       ability.Full_100    = 0;
       ability.Full_1000   = 0;

       ability.AutoNegotiation = 1;

       /* give port all abilities */
       ability.Half_10     = 1;
       ability.Full_10     = 1;
       ability.Half_100    = 1;
       ability.Full_100    = 1;
       ability.Full_1000   = 1;
       rtk_port_phyAutoNegoAbility_set( pm->port, &ability );
    }
    else if (pm->speed == EXTSWT_PORT_SPEED_1000M)
    {
        // need to turn on autonegotiate when set 1000M
        // first, get the current ability
        rtk_port_phyAutoNegoAbility_get( pm->port, &ability );
        
        /* Clear out autonegotiate and speed / duplex ability */
        ability.AutoNegotiation = 0;
        ability.Half_10     = 0;
        ability.Full_10     = 0;
        ability.Half_100    = 0;
        ability.Full_100    = 0;
        ability.Full_1000   = 0;
        
        ability.AutoNegotiation = 1;
        ability.Full_1000   = 1;
        rtk_port_phyAutoNegoAbility_set( pm->port, &ability );
    }
    else 
    {
       // first, get the current ability
       rtk_port_phyForceModeAbility_get( pm->port, &ability );
       /* Clear out autonegotiate and speed / duplex ability */
       ability.AutoNegotiation = 0;
       ability.Half_10     = 0;
       ability.Full_10     = 0;
       ability.Half_100    = 0;
       ability.Full_100    = 0;
       ability.Full_1000   = 0;
       
       switch( pm->speed ){
           case EXTSWT_PORT_SPEED_10M:
               if ( pm->duplex == PORT_HALF_DUPLEX ) 
               {
                   ability.Half_10 = 1;
               }
               else
               {
                   ability.Full_10 = 1;
               }
               break;
           
           case EXTSWT_PORT_SPEED_100M:
               if ( pm->duplex == PORT_HALF_DUPLEX ) 
               {
                   ability.Half_100 = 1;
               }
               else
               {
                   ability.Full_100 = 1;
               }
               break;
           
           default:     
               break;
            
       }
       rtk_port_phyForceModeAbility_set( pm->port, &ability );
    }

    return 0;
}

static long rtk83xx_add_vlan_port(rtk83xx_dev_t *dev, EXTSWT_vlan_add_port_t* ap)
{
    rtk83xx_vlan_t        *cur, *vlan = NULL;
    rtk_svlan_memberCfg_t cfg;
    rtk_api_ret_t         err;
    
    // Find this vlan config in shadow...
    list_for_each_entry(cur, &dev->vlans, list)
    {
        if (cur->vid == ap->vid)
        {
            vlan = cur;
            break;
        }
    }

    // If not found, add a new definition
    if (vlan == NULL)
    {
        vlan = kmalloc( sizeof( rtk83xx_vlan_t ), GFP_KERNEL );
        memset( vlan, 0, sizeof( rtk83xx_vlan_t ) );
        INIT_LIST_HEAD( &vlan->list );
        vlan->vid = ap->vid;
        list_add_tail( &vlan->list, &dev->vlans );
    }

    // retreive current config from the switch
    memset( &cfg, 0, sizeof( rtk_svlan_memberCfg_t ) );
    err = rtk_svlan_memberPortEntry_get(vlan->vid, &cfg);

    if( err == RT_ERR_OK )
    {
        DPRINTK( "OLD vlan entry %d: member_mask=0x%x untag_mask=0x%x\n", vlan->vid, cfg.memberport, cfg.untagport );
    }

    vlan->member_msk |= (1 << ap->port);
    switch( ap->mode )
    {
        case PORT_IS_MEMBER_EGRESS_UNTAGGED:
            vlan->untag_msk |= (1 << ap->port);
            break;

        case PORT_IS_MEMBER_EGRESS_TAGGED:
            vlan->untag_msk &= ~(1 << ap->port);
            break;

        case PORT_IS_MEMBER_EGRESS_UNMODIFIED:
            // No such option for S-TAG - as we never receive incoming STAGS
            break;

        case PORT_IS_NOT_MEMBER_EGRESS_INGRESS_DISCARD:
            printk( KERN_ERR "RTK83XX : ERROR - EXT_SWITCH_PORT_IS_NOT_MEMBER_EGRESS_INGRESS_DISCARD NOT IMPLEMENTED\n");
            break;

        default:
            printk( KERN_ERR "RTK83XX : ERROR - unknown portmemberTag\n");
            break;
    }

    cfg.svid        = vlan->vid;
    cfg.memberport  = vlan->member_msk;
    cfg.untagport   = vlan->untag_msk;

    // Set new VLAN config
    rtk_svlan_memberPortEntry_set(vlan->vid, &cfg);
    
    DPRINTK( "NEW vlan entry %d: member_mask=0x%x untag_mask=0x%x\n", vlan->vid, vlan->member_msk, vlan->untag_msk );

    return 0;
}

static long rtk83xx_delete_vlan_port(rtk83xx_dev_t *dev, EXTSWT_vlan_delete_port_t* dp)
{
    rtk83xx_vlan_t        *cur, *vlan = NULL;
    rtk_svlan_memberCfg_t cfg;
    rtk_api_ret_t         err;

    // Find this vlan config in shadow...
    list_for_each_entry(cur, &dev->vlans, list)
    {
        if( cur->vid == dp->vid )
        {
            vlan = cur;
            break;
        }
    }

    if( vlan )
    {
        // retreive current config from the switch
        memset( &cfg, 0, sizeof( rtk_svlan_memberCfg_t ) );
        err = rtk_svlan_memberPortEntry_get(vlan->vid, &cfg);

        DPRINTK( "OLD vlan entry %d: member_mask=0x%x untag_mask=0x%x\n", err, cfg.memberport, cfg.untagport );

        // Remove port from VLAN
        vlan->untag_msk &= ~(1 << dp->port);
        vlan->member_msk &= ~(1 << dp->port);

        cfg.svid        = vlan->vid;
        cfg.memberport  = vlan->member_msk;
        cfg.untagport   = vlan->untag_msk;

        // Set new VLAN config
        rtk_svlan_memberPortEntry_set(vlan->vid, &cfg);

        DPRINTK( "NEW vlan entry %d: member_mask=0x%x untag_mask=0x%x\n", vlan->vid, vlan->member_msk, vlan->untag_msk );

        if( !vlan->member_msk && !vlan->untag_msk )
        {
            // Delete vlan shadow entry if this VID isn't needed anymore
            list_del( &vlan->list );
            kfree( vlan );
        }
    }
    else
    {
        // Not found!
        printk( KERN_ERR "Vlan entry %d not found!\n", dp->vid );
    }
    
    return 0;
}

static long rtk83xx_add_port_tag(rtk83xx_dev_t *dev, EXTSWT_add_port_tag_t* apt)
{
    printk( KERN_ERR "Set default VID on port %d to %d\n", apt->port, apt->vid);

    dev->pvid[apt->port] = apt->vid;
    rtk_svlan_defaultSvlan_set(apt->port, apt->vid);

    return 0;
}

static long rtk83xx_getvlan_info(rtk83xx_dev_t *dev, EXTSWT_vlan_info_t* vi)
{
    rtk_svlan_memberCfg_t   cfg;
    rtk_port_t              port;
    rtk_vlan_t              vid;
    rtk_api_ret_t           err;
        
    err = rtk_svlan_memberPortEntry_get(vi->vid, &cfg);
    if ( err != RT_ERR_OK )
    {
        // If the SVLAN Doesn't exist, mark the member mask as zero, so we return good info
        cfg.memberport = 0;
    }

    for (port=0; port<8; port++)
    {
        if ( cfg.memberport & (1 << port) )
        {
            if ( cfg.untagport & (1 << port) )
            {
                vi->port_mode[port] = PORT_IS_MEMBER_EGRESS_UNTAGGED;

                // ensure port tag is enabled
                rtk_svlan_defaultSvlan_get( port, &vid );
                    
                if ( vi->vid != vid )
                {
                    printk( KERN_ERR "RTK83XX: port %d marked as UNTAGGED, but VID %d does not match default VID (%d) in switch\n",
                            port, vi->vid, vid );
                }
                else
                {
                    DPRINTK( "RTK83XX: port %d marked as UNTAGGED, and VID %d matches default VID (%d) in switch\n",
                            port, vi->vid, vid );
                }
            }
            else
            {
                vi->port_mode[port] = PORT_IS_MEMBER_EGRESS_TAGGED;
            }
        }
        else
        {
            vi->port_mode[port] = PORT_IS_NOT_MEMBER_EGRESS_INGRESS_DISCARD;
        }
    }

    return 0;
}

static long rtk83xx_set_testmode(rtk83xx_dev_t *dev, EXTSWT_set_testmode_t* tm)
{
    rtk_port_phy_test_mode_t mode;
    rtk_port_t port;

    // First, ensure all ports are set to NORMAL operation
    for (port=0; port <= RTK_PHY_ID_MAX; port++) {
        rtk_port_phyTestMode_get( port, &mode);
        if (mode != PHY_TEST_MODE_NORMAL) {
            rtk_port_phyTestModeAll_set( port, PHY_TEST_MODE_NORMAL);
        }
    }

    return rtk_port_phyTestModeAll_set( tm->port, tm->mode);
}

static long rtk83xx_read_register(rtk83xx_dev_t *dev, EXTSWT_rw_register_t* reg)
{ 
    long result;
        
    /* If the register is a phy register, we need to do a special register read */
    if ( ( reg->reg >= 0x2000 ) && ( reg->reg <= 0x209F ) )
    {
        rtk_port_t          port    = (reg->reg & (~0x201F)) >> 5;
        rtk_port_phy_reg_t  phy_reg = (reg->reg & ( 0x001F));
        result = (long)rtk_port_phyReg_get(port, phy_reg, &reg->val);
    }
    else
    {
        result = (long)rtl8367b_getAsicReg( reg->reg, &reg->val );
    }
    
    if ( result != RT_ERR_OK )
    {
        DPRINTK( "RTK83XX: ERROR when reading reg 0x%08x into 0x%p : result %ld\n", reg->reg, &reg->val, result );
        result = -EIO;
    }
    
    return result;
}

static long rtk83xx_write_register(rtk83xx_dev_t *dev, EXTSWT_rw_register_t* reg)
{ 
    long result;
        
    /* If the register is a phy register, we need to do a special register write */
    if ( ( reg->reg >= 0x2000 ) && ( reg->reg <= 0x209F ) )
    {
        rtk_port_t          port    = (reg->reg & (~0x201F)) >> 5;
        rtk_port_phy_reg_t  phy_reg = (reg->reg & ( 0x001F));
        result = (long)rtk_port_phyReg_set(port, phy_reg, reg->val);
    }
    else
    {
        result = (long)rtl8367b_setAsicReg( reg->reg, reg->val );
    }

    if ( result != RT_ERR_OK )
    {
        DPRINTK( "RTK83XX: ERROR when writing reg 0x%08x with 0x%04x - result %ld\n", reg->reg, reg->val, result );
        result = -EIO;
    }

    return result;
}

static long rtk83xx_config_model(rtk83xx_dev_t *dev, unsigned int model, EXTSWT_mac_t* pMac)
{
    rtk_port_mac_ability_t  mac_cfg;
    rtk_priority_select_t   priDec;
    rtk_port_t              port;
    rtk_int_info_t          link_up;
    rtk_int_info_t          link_down;
    rtk_int_status_t        status;
    rtk_svlan_memberCfg_t   cfg;
    rtk83xx_vlan_t*         vlan = NULL;
    int                     result = 0;
    
    if ( dev->api_initialized == FALSE )
    {
        /* power on delay */
        msleep(1000);

        /* Initialize the Realtek API */
        result = rtk_switch_init();

        if (result)
        {
            printk(KERN_NOTICE "Error initializing realtek API %d", result );
            return result;
        }

        dev->api_initialized = TRUE;

        /* Now force the link up again, realtek forces it down */
        mac_cfg.forcemode = 1;
        mac_cfg.speed     = EXTSWT_PORT_SPEED_1000M;
        mac_cfg.duplex    = PORT_FULL_DUPLEX;
        mac_cfg.link      = 1;
        mac_cfg.nway      = 0;
        mac_cfg.rxpause   = ENABLED;
        mac_cfg.txpause   = ENABLED;

        /* Force Ext1 at RGMII, 1G/Full */
        rtk_port_macForceLinkExt_set(EXT_PORT_1, MODE_EXT_RGMII, &mac_cfg);
        rtk_port_rgmiiDelayExt_set(EXT_PORT_1, 1, 7);

        /* Turn off green mode */
        rtk_switch_greenEthernet_set(DISABLED);
        
        // Init stacked vlans
        rtk_svlan_init();

        // Init Rldp
        if( (result = rtk_rldp_init(pMac)) != RT_ERR_OK )
        {
            printk("rtk_rldp_init error %d \n", result);
        }

        // reporgram VLANS
        memset( &cfg, 0, sizeof( rtk_svlan_memberCfg_t ) );
        list_for_each_entry(vlan, &dev->vlans, list)
        {
            cfg.svid        = vlan->vid;
            cfg.memberport  = vlan->member_msk;
            cfg.untagport   = vlan->untag_msk;

            rtk_svlan_memberPortEntry_set(vlan->vid, &cfg);
        }

        // program TPID
        if ( dev->tpid )
        {
            rtk_svlan_tpidEntry_set( dev->tpid );
        }
        
        // program tag mode and pvid for each port
        for ( port=0; port<NUM_PORTS; port++)
        {
            if ( dev->provider[port] )
            {
                rtk_svlan_servicePort_add(port);
            }

            if ( dev->pvid[port] )
            {
                rtk_svlan_defaultSvlan_set( port, dev->pvid[port] );
            }
        }

        // Initialize 802.1P
        // use 8 strict queues
        result = rtk_qos_init(8);

        // This sets the priority of the priority source : make sure 802.1q is highest
        priDec.svlan_pri = 7;
        priDec.dot1q_pri = 6;
        priDec.dscp_pri  = 5;
        priDec.port_pri  = 4;
        priDec.acl_pri   = 3;
        priDec.cvlan_pri = 2;
        priDec.dmac_pri  = 1;
        priDec.smac_pri  = 0;

        rtk_qos_priSel_set(&priDec);

        /* Get Port Status */
        rtk83xx_read_port_status( &rtk83xx_dev, 0xFF );

        /* Then, clear any pending link status on the switch */
        rtk_int_advanceInfo_get(ADV_PORT_LINKDOWN_PORT_MASK, &link_down);
        rtk_int_advanceInfo_get(ADV_PORT_LINKUP_PORT_MASK, &link_up);

        /* Then clear the interrupt */
        status.value[0] = 0xFF;
        status.value[1] = 0xFF;
        rtk_int_status_set(status);

        /* Then configure hw interrupt on the switch */
        rtk_int_polarity_set(INT_POLAR_LOW);
        rtk_int_control_set(INT_TYPE_LINK_STATUS, ENABLED);

        if( !rtk83xx_dev.irq_registered )
        {
            /* First clear the IRQ in order not to get a false interrupt since INTD is level */
            ack_irq(AVALANCHE_INT_89);
    
            if( request_threaded_irq(AVALANCHE_INT_89, NULL, rtk83xx_irq,
                                     IRQF_TRIGGER_LOW | IRQF_ONESHOT,
                                     rtk83xx_dev.name, &rtk83xx_dev) )
            {
                printk( KERN_ERR " unable to get IRQ #%d !\n", AVALANCHE_INT_89);
            }
            else
            {
                rtk83xx_dev.irq_registered = TRUE;
            }
        }
    }

    rtk_led_blinkRate_set(LED_BLINKRATE_256MS);
    switch( model)
    {
        case TG1642G_HW_MODEL:
        case MG2402G_HW_MODEL:
            // GROUP 0: YELLOW, GROUP 1: RED, GROUP 2: GREEN
            rtk_led_groupConfig_set( LED_GROUP_0, LED_CONFIG_LINK_ACT );
            rtk_led_groupConfig_set( LED_GROUP_1, LED_CONFIG_SPD10010ACT);
            rtk_led_groupConfig_set( LED_GROUP_2, LED_CONFIG_SPD1000ACT);
            break;

        case TG1672G_HW_MODEL:
        default:
            // GROUP 0: YELLOW, GROUP 2: RED, GROUP 1: GREEN
            rtk_led_groupConfig_set( LED_GROUP_0, LED_CONFIG_LINK_ACT );
            rtk_led_groupConfig_set( LED_GROUP_1, LED_CONFIG_SPD1000ACT);
            rtk_led_groupConfig_set( LED_GROUP_2, LED_CONFIG_SPD10010ACT);
            break;
    }

    return 0;
}

static long rtk83xx_port_enable(rtk83xx_dev_t *dev, EXTSWT_port_enable_t* reg)
{ 
    int port = reg->port;
    int enable = reg->enable;
    int ret;

    if (port == EXTSWT_INTERNAL_PORT)
    {
        /* Handle CPU Port Link Status */
        rtk_mode_ext_t          mode;
        rtk_port_mac_ability_t  ability;

        ret = rtk_port_macForceLinkExt_get(EXT_PORT_1, &mode, &ability);
        if ( ret == RT_ERR_OK ) 
        {
            ability.link = enable;
            ability.forcemode=1;
            rtk_port_macForceLinkExt_set(EXT_PORT_1, mode, &ability);
        }
        else
        {
            printk("rtk_port_macForceLinkExt_get error %d\n", ret);        
        }
    }
    else
    {
        rtk_port_mac_ability_t  ability;

        ret = rtk_port_macForceLink_get(port, &ability);
        if ( ret == RT_ERR_OK ) 
        {
            ability.link = enable;
            ability.forcemode=1;
            rtk_port_macForceLink_set(port, &ability);
        }
        else
        {
            printk("rtk_port_macForceLink_get error %d\n", ret);        
        }
    }
        
    return 0;
}

static long rtk83xx_get_phy_port_enable(rtk83xx_dev_t *dev, EXTSWT_phy_port_enable_t* reg)
{ 
    int port = reg->port;
    rtk_enable_t enable = 0;
    int ret;

    if(dev->api_initialized == False)
    {
        reg->enable =  False;
        return 0;
    }
    
    ret = rtk_phy_portEnable_get(port, &enable);
    if ( ret != RT_ERR_OK ) 
    {
        printk("rtk_phy_portEnable_get error %d\n", ret);
    }
    reg->enable = enable;
        
    return 0;
}

static long rtk83xx_phy_port_enable(rtk83xx_dev_t *dev, EXTSWT_phy_port_enable_t* reg)
{ 
    int port = reg->port;
    int enable = reg->enable;
    int ret;
    
    ret = rtk_phy_portEnable_set(port, enable);
    if ( ret != RT_ERR_OK ) 
    {
        printk("rtk_phy_portEnable_set error %d\n", ret);        
    }
    
    return 0;
}

static long rtk83xx_dump_macs(rtk83xx_dev_t *dev, EXTSWT_dump_mac_t* reg, EXTSWT_mac_portmap_t *macBuffer)
{
    int ret;
    rtk_uint32 address = 0;
    rtk_l2_ucastAddr_t l2_data;
    EXTSWT_mac_portmap_t *macPort_p = macBuffer;
    uint32_t macsFound = 0;

    while (1)
    {
        if ((ret = rtk_l2_addr_next_get(READMETHOD_NEXT_L2UC, 0, &address, &l2_data)) != RT_ERR_OK)
        {
            break;
        }

        // Check for space in buffer
        if( ((macsFound + 1) * sizeof(EXTSWT_mac_portmap_t)) < reg->macBufferSize )
        {
            // grab VID
            macPort_p->vid = (unsigned short)l2_data.cvid;
            // grab MAC
            memcpy( macPort_p->macAddress, l2_data.mac.octet, 6 );
            // grab Port
            macPort_p->port = l2_data.port;

            DPRINTK( "MAC=%2.2x-%2.2x-%2.2x-%2.2x-%2.2x-%2.2x Port=%d VID=%d\n", 
                     macPort_p->macAddress[0], macPort_p->macAddress[1], macPort_p->macAddress[2], 
                     macPort_p->macAddress[3], macPort_p->macAddress[4], macPort_p->macAddress[5], 
                     macPort_p->port, macPort_p->vid );

            macPort_p++;
        }
        macsFound++;

        address++;
    }

    reg->macsFound = macsFound;

    DPRINTK( "MACs found=%d\n", reg->macsFound );

    return 0;
}

static long rtk83xx_search_mac(rtk83xx_dev_t *dev, EXTSWT_search_mac_t* reg)
{ 
    int ret;
    rtk_l2_ucastAddr_t l2_entry;     //data from phy

    DPRINTK( "%2.2x-%2.2x-%2.2x-%2.2x-%2.2x-%2.2x\n", 
            reg->mac.octet[0], reg->mac.octet[1], reg->mac.octet[2], 
            reg->mac.octet[3], reg->mac.octet[4], reg->mac.octet[5] );

    l2_entry.ivl = 0;
    l2_entry.cvid = 0;
    l2_entry.fid = 0;
    l2_entry.efid = 0;
    reg->port=0;
    ret = rtk_l2_addr_get((rtk_mac_t *)&reg->mac, &l2_entry);
    if( ret == RT_ERR_OK )
    {
        reg->found = TRUE;
        reg->port=l2_entry.port;
        DPRINTK( "found=1 port=%d\n", reg->port );
    }
    else
    {
        reg->found = FALSE;
    }

    return( ret );
}

static long rtk83xx_add_mac(rtk83xx_dev_t *dev, EXTSWT_add_mac_t* reg)
{ 
    int ret;
    rtk_l2_ucastAddr_t l2_entry;
    rtk_mac_t mac;
    
    memcpy(&mac, &reg->mac, sizeof(mac));
    memset(&l2_entry, 0, sizeof(l2_entry));
    l2_entry.port = reg->port;
    l2_entry.ivl = 0;
    l2_entry.cvid = 0;
    l2_entry.fid = 0;
    l2_entry.efid = 0;
    l2_entry.is_static = (reg->is_static? 1 : 0);
    
    ret = rtk_l2_addr_add(&mac, &l2_entry);
    if (ret != RT_ERR_OK)
    {
        printk("rtk_l2_addr_add error %d\n", ret);
    }
    return 0;
}

static long rtk83xx_del_mac(rtk83xx_dev_t *dev, EXTSWT_del_mac_t* reg)
{ 
    int ret;
    rtk_l2_ucastAddr_t l2_entry;
    rtk_mac_t mac;
    
    memcpy(&mac, &reg->mac, sizeof(mac));
    memset(&l2_entry, 0, sizeof(l2_entry));
    
    l2_entry.port = reg->port;
    l2_entry.ivl = 0;
    l2_entry.cvid = 0;
    l2_entry.fid = 0;
    l2_entry.efid = 0;
    
    ret = rtk_l2_addr_del(&mac, &l2_entry);
    if (ret != RT_ERR_OK)
    {
        printk("rtk_l2_addr_del error %d\n", ret);
    }
    return 0;
}

#if defined( CONFIG_SYSTEM_MOCA ) || defined ( CONFIG_VENDOR_ARRIS_ENTROPIC_MOCA )
/****************************************************************************/
/*
    Routine:        rt83xx_loop_detected
    Description:    
*/
/****************************************************************************/
// Function called from bridge code when it's found a packet that has been looped back into an external 
// switch port.  This code check switch port to determine if it's external switch port
int rt83xx_loop_detected( unsigned char *macAddr )
{
    EXTSWT_search_mac_t search_mac;
    struct task_struct *p = NULL;

    if (pid != 0)
    {
        memset(&search_mac, 0, sizeof(EXTSWT_search_mac_t));
        memcpy(search_mac.mac.octet, macAddr, ETHER_ADDR_LEN);
        
        rtk83xx_search_mac( &rtk83xx_dev, &search_mac );

        if (( search_mac.found ) && ( (search_mac.port) == EXTSWT_INTERNAL_PORT))
        {
            rcu_read_lock();
            p = pid_task(find_pid_ns(pid, &init_pid_ns), PIDTYPE_PID);
            rcu_read_unlock();
            send_sig(SIGUSR1, p, 0);
            pid = 0;
            return( 0 );
        }
    }
    return( 1 );
}
#endif
static long rtk83xx_loop_detect_enable(rtk83xx_dev_t *dev, int enable)
{ 
    int ret;
    
    ret = rtk_rldp_enable(enable);
    if ( ret != RT_ERR_OK ) 
    {
        printk("rtk_rldp_enable error %d\n", ret);        
    }
#if defined( CONFIG_SYSTEM_MOCA ) || defined ( CONFIG_VENDOR_ARRIS_ENTROPIC_MOCA )
    if( enable )
    {
        arris_loop_detected_hook = rt83xx_loop_detected;
        printk( KERN_ERR "Loop Detection Enabled\n" );
    }
    else
#endif
    {
        arris_loop_detected_hook = 0;
        printk( KERN_ERR "Loop Detection Disabled\n" );
    }
    
    return 0;
}


#if 0
static long rtk83xx_certification_config(rtk83xx_dev_t *dev)
{
    /* unneeded for Realtek SVLAN tag implementation */
    
    return 0;
}
#endif


static long rtk83xx_stp_enable(rtk83xx_dev_t *dev, EXTSWT_stpcfg_t* stp )
{
    // STP INIT 
    rtk_mac_t               rma_frame;

    // Initialize 802.1D (Spanning Tree)
    rma_frame.octet[0] = 0x01;
    rma_frame.octet[1] = 0x80;
    rma_frame.octet[2] = 0xC2;
    rma_frame.octet[3] = 0x00;
    rma_frame.octet[4] = 0x00;
    rma_frame.octet[5] = 0x00;

    if (stp->enable) {
        /* Make sure switch inserts the proprietary ethernet header in the trapping frames (STP) */
        rtk_trap_rmaAction_set( &rma_frame, RMA_ACTION_TRAP2CPU);
        rtk_cpu_enable_set(ENABLE);
        rtk_cpu_tagPort_set(RTK_EXT_1_MAC, CPU_INSERT_TO_TRAPPING);
        stp_init();
    } else {
        rtk_trap_rmaAction_set( &rma_frame, RMA_ACTION_FORWARD);
        rtk_cpu_enable_set(ENABLE);
        rtk_cpu_tagPort_set(RTK_EXT_1_MAC, CPU_INSERT_TO_NONE);
        stp_stop();
    }

    return 0;
}

static long rtk83xx_power_down(rtk83xx_dev_t *dev)
{
    /* remember state */
    dev->api_initialized = FALSE;

    /* disable interrupt */
    if ( rtk83xx_dev.irq_registered )
    {
        free_irq(AVALANCHE_INT_89, &rtk83xx_dev);
        rtk83xx_dev.irq_registered = FALSE;
        rtk83xx_clear_port_status(dev);
    }
    
    // Clear port_status struct
    //memset( &dev->port_status, 0, sizeof(dev->port_status) );

    return 0;
}

static long rtk83xx_qinq_enable(rtk83xx_dev_t *dev )
{
    /* Initialize stacked vlan */
    return 0;
}

static long rtk83xx_qinq_provider(rtk83xx_dev_t *dev, EXTSWT_qinq_provider_t* qinqp )
{
    /* Make sure we don't have a provider port */
    dev->tpid = qinqp->tpid;
    dev->provider[qinqp->port] = True;
    
    rtk_svlan_tpidEntry_set(qinqp->tpid);
    rtk_svlan_servicePort_add(qinqp->port);

    return 0;
}

static long rtk83xx_qinq_customer(rtk83xx_dev_t *dev, EXTSWT_qinq_customer_t* qinqc )
{
    /* Make sure we don't have a provider port */
    return 0;
}

static long rtk83xx_register_eventfd(rtk83xx_dev_t *dev, EXTSWT_reg_linkstatus_fd_t* reg)
{
    /* retreive and add the fd to the list */
    rtk83xx_regfd_t* regfd = kmalloc( sizeof( rtk83xx_regfd_t ), GFP_KERNEL );
    memset( regfd, 0, sizeof( rtk83xx_regfd_t ) );
            
    INIT_LIST_HEAD( &regfd->list );
    regfd->efd     = reg->fd;
    regfd->efd_ctx = eventfd_ctx_fdget( regfd->efd );
    
    list_add_tail( &regfd->list, &dev->reg_fds );
    
    return 0;
}

static long rtk83xx_set_eth_eee_mode(rtk83xx_dev_t *dev, EXTSWT_eth_eee_mode_t mode )
{
    int ret;
    unsigned int port;
    unsigned int enable;

    port = mode.port_num;
    enable = mode.enable[port];

    ret = rtk_eee_init();
    if (ret != RT_ERR_OK)
    {
        printk("rtk EEE mode init error %d\n", ret);
    }

    ret = rtk_eee_portEnable_set(port, enable);
    if (ret != RT_ERR_OK)
    {
        printk("rtk EEE set on Port %d error %d\n", port, ret);
    }

    return 0;

}

static long rtk83xx_get_eth_eee_mode(rtk83xx_dev_t *dev, EXTSWT_eth_eee_mode_t *mode )
{
    int ret;
    unsigned int port;
    rtk_enable_t enable = 0;

    port = mode->port_num;

    ret = rtk_eee_init();
    if (ret != RT_ERR_OK)
    {
        printk("rtk EEE mode init error %d\n", ret);
    }

    ret = rtk_eee_portEnable_get(port, &enable);
    if (ret != RT_ERR_OK)
    {
        printk("rtk EEE get on Port %d error %d\n", port, ret);
    }

    mode->enable[port] = enable;

    return 0;

}

static long rtk83xx_get_eth_eee_stats(rtk83xx_dev_t *dev, EXTSWT_eth_eee_stats_t *stats )
{
    unsigned int port;
    rtk_api_ret_t retVal;
    rtl8367b_port_status_t status;
    U_rtl8367b_port_status reg;

    port = stats->port_num;

    if (port > RTK_PORT_ID_MAX)
        return RT_ERR_PORT_ID;

    if ((retVal = rtl8367b_getAsicPortStatus(port, &status)) != RT_ERR_OK)
        return retVal;

    memcpy(&reg.port, &status, sizeof(rtl8367b_port_status_t));

    /* Check the 11th bit for lpi1000 bitfield - PD25428
     * status_reg is the bitfield remap to unit16 */
    if (RTK_EEE_CHECK_BIT(reg.status_reg, 11))
        stats->lpi1000[port] = 1;

    /* Check the 10th bit for lpi100 bitfield */
    if (RTK_EEE_CHECK_BIT(reg.status_reg, 10))
        stats->lpi100[port] = 1;

    return 0;
}

static long rtk83xx_get_eth_eee_stats_clear(rtk83xx_dev_t *dev, EXTSWT_eth_eee_stats_clr_t *mode )
{
    /* No need to clear the lpi fields from Port Status. Added for the
     * sake of completion and furture development */
    return 0;
}

static long rtk83xx_ioctl(struct file* file, unsigned int cmd, unsigned long arg )                     
{  
    rtk83xx_dev_t        *dev = (rtk83xx_dev_t *)file->private_data; 
    unsigned int  __user *uarg = (unsigned int __user *)arg;
    unsigned int         priv_cmd;
    long                 ret = EIO;

    if (cmd != SIOCDEVPRIVATE)
    {
        printk ("RTK83XX IOCTL CMD not SIOCDEVPRIVATE\n");
        return (-EINVAL);
    }

    if(copy_from_user((char *)&priv_cmd, (void *)uarg, sizeof(unsigned int)))
    { 
        printk ("RTK83XX IOCTL CMD can't get userdata\n");
        return (-EINVAL); 
    }

    // Only allow ioctl if we're initialized already or for a few specific cases when not
    if( dev->api_initialized || (priv_cmd == EXTSWT_CONFIG_MODEL) || 
        (priv_cmd == EXTSWT_IS_INITIALIZED) || (priv_cmd == EXTSWT_GETPORT_STATUS) )
    {
        mutex_lock( &dev->mutex );

        switch (priv_cmd)
        {
            case EXTSWT_IS_INITIALIZED:
            {
                EXTSWT_ioctl_request_t req;
                if (copy_from_user((char *)&req, (void *)uarg, sizeof(EXTSWT_ioctl_request_t)) == 0)
                {
                    req.rsp = rtk83xx_dev.api_initialized;
    
                    if (copy_to_user((void *)uarg, (char *)&req, sizeof(EXTSWT_ioctl_request_t)) == 0)
                    {
                        ret = 0;
                    }
                }
                break;
            }
    
            case EXTSWT_CONFIG_MODEL:
            {
                EXTSWT_ioctl_config_model_t cmd;
                if (copy_from_user((char *)&cmd, (void *)uarg, sizeof(EXTSWT_ioctl_config_model_t)) == 0)
                {
                    ret = rtk83xx_config_model( dev, cmd.model, &cmd.mac);
                }
                break;
            }
    
            case EXTSWT_GETPORT_STATUS:
            {
                EXTSWT_ioctl_get_port_status_t cmd;
                if (copy_from_user((char *)&cmd, (void *)uarg, sizeof(EXTSWT_ioctl_get_port_status_t)) == 0)
                {
                    ret = rtk83xx_getport_status( dev, &cmd.ps );
    
                    if (copy_to_user((void *)uarg, (char *)&cmd, sizeof(EXTSWT_ioctl_get_port_status_t)) != 0)
                    {
                        ret = EIO;
                    }
                }   
                break;
            }
    
            case EXTSWT_GETPORT_STATS:
            {
                EXTSWT_ioctl_get_port_stats_t cmd;
                if (copy_from_user((char *)&cmd, (void *)uarg, sizeof(EXTSWT_ioctl_get_port_stats_t)) == 0)
                {
                    ret = rtk83xx_getport_stats( dev, &cmd.ps );
    
                    if (copy_to_user((void *)uarg, (char *)&cmd, sizeof(EXTSWT_ioctl_get_port_stats_t)) != 0)
                    {
                        ret = EIO;
                    }
                }
                break;
            }
            case EXTSWT_GETPORT_IFTABLE_STATS:
            {
                EXTSWT_ioctl_get_port_iftable_stats_t cmd;
                if (copy_from_user((char *)&cmd, (void *)uarg, sizeof(EXTSWT_ioctl_get_port_iftable_stats_t)) == 0)
                {
                    ret = rtk83xx_getport_iftable_stats( dev, &cmd.ps );
    
                    if (copy_to_user((void *)uarg, (char *)&cmd, sizeof(EXTSWT_ioctl_get_port_iftable_stats_t)) != 0)
                    {
                        ret = EIO;
                    }
                }
                break;
            }
    
            case EXTSWT_SETPORT_MODE:
            {
                EXTSWT_ioctl_set_port_mode_t cmd;
                if (copy_from_user((char *)&cmd, (void *)uarg, sizeof(EXTSWT_ioctl_set_port_mode_t)) == 0)
                {
                    ret = rtk83xx_setport_mode( dev, &cmd.pm );
                }
                break;
            }
    
            case EXTSWT_VLAN_ADD_PORT:
            {
                EXTSWT_ioctl_vlan_add_port_t cmd;
                if (copy_from_user((char *)&cmd, (void *)uarg, sizeof(EXTSWT_ioctl_vlan_add_port_t)) == 0)
                {
                    ret = rtk83xx_add_vlan_port( dev, &cmd.ap );
                }
                break;
            }
    
            case EXTSWT_VLAN_DELETE_PORT:
            {
                EXTSWT_ioctl_vlan_delete_port_t cmd;
                if( copy_from_user((char *)&cmd, (void *)uarg, sizeof(EXTSWT_ioctl_vlan_delete_port_t)) == 0 )
                {
                    ret = rtk83xx_delete_vlan_port( dev, &cmd.dp );
                }
                break;
            }
    
            case EXTSWT_ADD_PORT_TAG:
            {
                EXTSWT_ioctl_add_port_tag_t cmd;
                if (copy_from_user((char *)&cmd, (void *)uarg, sizeof(EXTSWT_ioctl_add_port_tag_t)) == 0)
                {
                    ret = rtk83xx_add_port_tag( dev, &cmd.apt );
                }
                break;
            }
    
            case EXTSWT_GET_VLAN_INFO:
            {
                EXTSWT_ioctl_get_vlan_info_t cmd;
                if (copy_from_user((char *)&cmd, (void *)uarg, sizeof(EXTSWT_ioctl_get_vlan_info_t)) == 0)
                {
                    ret = rtk83xx_getvlan_info( dev, &cmd.vi );
    
                    if (copy_to_user((void *)uarg, (char *)&cmd, sizeof(EXTSWT_ioctl_get_vlan_info_t)) != 0)
                    {
                        ret = EIO;
                    }
                }
                break;
            }
    
            case EXTSWT_CLEAR_PORT_STATS:
                ret = rtk83xx_clear_port_stats( dev );
                break;
    
            case EXTSWT_SET_TEST_MODE:
            {
                EXTSWT_ioctl_set_testmode_t cmd;
                if (copy_from_user((char *)&cmd, (void *)uarg, sizeof(EXTSWT_ioctl_set_testmode_t)) == 0)
                {
                    ret = rtk83xx_set_testmode( dev, &cmd.tm );
                }
                break;
            }
    
            case EXTSWT_READ_REGISTER:
            {
                EXTSWT_ioctl_rw_register_t cmd;
                if (copy_from_user((char *)&cmd, (void *)uarg, sizeof(EXTSWT_ioctl_rw_register_t)) == 0)
                {
                    ret = rtk83xx_read_register( dev, &cmd.reg );
    
                    if (copy_to_user((void *)uarg, (char *)&cmd, sizeof(EXTSWT_ioctl_rw_register_t)) != 0)
                    {
                        ret = EIO;
                    }
                }
                break;
            }
    
            case EXTSWT_WRITE_REGISTER:
            {
                EXTSWT_ioctl_rw_register_t cmd;
                if (copy_from_user((char *)&cmd, (void *)uarg, sizeof(EXTSWT_ioctl_rw_register_t)) == 0)
                {
                    ret = rtk83xx_write_register( dev, &cmd.reg );
                }
                break;
            }
    
            case EXTSWT_PORT_ENABLE:
            {
                EXTSWT_ioctl_port_enable_t cmd; 
                if (copy_from_user((char *)&cmd, (void *)uarg, sizeof(EXTSWT_ioctl_port_enable_t)) == 0)
                {
                    ret = rtk83xx_port_enable( dev, &cmd.ep );
                }
                break;
            }
    
            case EXTSWT_GET_PHY_PORT_ENABLE:
            {
                EXTSWT_ioctl_phy_port_enable_t cmd; 
                if (copy_from_user((char *)&cmd, (void *)uarg, sizeof(EXTSWT_ioctl_phy_port_enable_t)) == 0)
                {
                    ret = rtk83xx_get_phy_port_enable( dev, &cmd.pea );
                    if (copy_to_user((void *)uarg, (char *)&cmd, sizeof(EXTSWT_ioctl_phy_port_enable_t)) != 0)
                    {
                        ret = EIO;
                    }            
                }
                break;
            }
    
            case EXTSWT_PHY_PORT_ENABLE:
            {
                EXTSWT_ioctl_phy_port_enable_t cmd; 
                if (copy_from_user((char *)&cmd, (void *)uarg, sizeof(EXTSWT_ioctl_phy_port_enable_t)) == 0)
                {
                    ret = rtk83xx_phy_port_enable( dev, &cmd.pea );
                }
                break;
            }
    
            case EXTSWT_DUMP_MACS:
            {
                EXTSWT_ioctl_dump_mac_t cmd; 
    
                if( copy_from_user((char *)&cmd, (void *)uarg, sizeof(EXTSWT_ioctl_dump_mac_t)) == 0 )
                {
                    EXTSWT_mac_portmap_t *macBuffer = kmalloc( cmd.dm.macBufferSize, GFP_KERNEL );
    
                    if( macBuffer )
                    {
                        ret = rtk83xx_dump_macs( dev, &cmd.dm, macBuffer );
    
                        // Copy macBuffer results back to userspace
                        if( copy_to_user((void *)cmd.dm.macBuffer , (char *)macBuffer, (cmd.dm.macsFound * sizeof(EXTSWT_mac_portmap_t))) != 0 )
                        {
                            ret = EIO;
                        }
    
                        if( copy_to_user((void *)uarg, (char *)&cmd, sizeof(EXTSWT_ioctl_dump_mac_t)) != 0 )
                        {
                            ret = EIO;
                        }
    
                        kfree( macBuffer );
                    }
                    else
                    {
                        ret = ENOMEM;
                    }
                }
                break;
            }
    
            case EXTSWT_SEARCH_MAC:
            {
                EXTSWT_ioctl_search_mac_t cmd; 
                if (copy_from_user((char *)&cmd, (void *)uarg, sizeof(EXTSWT_ioctl_search_mac_t)) == 0)
                {
                    ret = rtk83xx_search_mac( dev, &cmd.sm);
                    
                    if (copy_to_user((void *)uarg, (char *)&cmd, sizeof(EXTSWT_ioctl_search_mac_t)) != 0)
                    {
                        ret = EIO;
                    }
                }
                break;
            }
    
            case EXTSWT_ADD_MAC:
            {
                EXTSWT_ioctl_add_mac_t cmd; 
                if (copy_from_user((char *)&cmd, (void *)uarg, sizeof(EXTSWT_ioctl_add_mac_t)) == 0)
                {
                    ret = rtk83xx_add_mac( dev, &cmd.am);
                }
                break;
            }
    
            case EXTSWT_DEL_MAC:
            {
                EXTSWT_ioctl_del_mac_t cmd; 
                if (copy_from_user((char *)&cmd, (void *)uarg, sizeof(EXTSWT_ioctl_del_mac_t)) == 0)
                {
                    ret = rtk83xx_del_mac( dev, &cmd.dm);
                }
                break;
            }
                            
    #if 0
            case EXTSWT_CERTIFICATION_CONFIG:
            {
                ret = rtk83xx_certification_config( dev );
                break;
            }
    #endif
                         
            case EXTSWT_STP_ENABLE:
            {
                EXTSWT_ioctl_stpcfg_t cmd;
                if (copy_from_user((char *)&cmd, (void *)uarg, sizeof(EXTSWT_ioctl_stpcfg_t)) == 0)
                {
                    ret = rtk83xx_stp_enable( dev, &cmd.stp );
                }
                break;
            }
    
            case EXTSWT_POWER_DOWN:
            {
                ret = rtk83xx_power_down( dev );
                break;
            }
    
            case EXTSWT_QINQ_ENABLE:
            {
                ret = rtk83xx_qinq_enable( dev );
                break;
            }
            
            case EXTSWT_QINQ_PROV:
            {
                EXTSWT_ioctl_qinq_provider_t cmd;
                if (copy_from_user((char *)&cmd, (void *)uarg, sizeof(EXTSWT_ioctl_qinq_provider_t)) == 0)
                {
                    ret = rtk83xx_qinq_provider( dev, &cmd.prov );
                }
                break;
            }
    
            case EXTSWT_QINQ_CUST:
            {
                EXTSWT_ioctl_qinq_customer_t cmd;
                if (copy_from_user((char *)&cmd, (void *)uarg, sizeof(EXTSWT_ioctl_qinq_customer_t)) == 0)
                {
                    ret = rtk83xx_qinq_customer( dev, &cmd.cust );
                }
                break;
            }
    
            case EXTSWT_LOOP_DETECT_ENABLE:
            {
                EXTSWT_ioctl_loop_detect_enable_t cmd;
                if (copy_from_user((char *)&cmd, (void *)uarg, sizeof(EXTSWT_ioctl_loop_detect_enable_t)) == 0)
                {
                    ret = rtk83xx_loop_detect_enable( dev, cmd.enable );
                }
                break;
            }
            
            case EXTSWT_REG_LINKSTATUS_FD:
            {
                EXTSWT_ioctl_reg_linkstatus_fd_t cmd;
                if (copy_from_user((char *)&cmd, (void *)uarg, sizeof(EXTSWT_ioctl_reg_linkstatus_fd_t)) == 0)
                {
                    ret = rtk83xx_register_eventfd(dev, &cmd.reg);
                }
                break;
            }            

            case EXTSWT_SET_ETH_EEE_MODE:
            {
                EXTSWT_ioctl_eth_eee_mode_t cmd;
                if( copy_from_user((char *)&cmd, (void *)uarg, sizeof(EXTSWT_ioctl_eth_eee_mode_t)) == 0)
                {
                    ret = rtk83xx_set_eth_eee_mode( dev, cmd.emode );
                }
                break;
            }

            case EXTSWT_GET_ETH_EEE_MODE:
            {
                EXTSWT_ioctl_eth_eee_mode_t cmd;
                if( copy_from_user((char *)&cmd, (void *)uarg, sizeof(EXTSWT_ioctl_eth_eee_mode_t)) == 0)
                {
                    ret = rtk83xx_get_eth_eee_mode( dev, &cmd.emode );
                    if( copy_to_user((void *)uarg, (char *)&cmd, sizeof(EXTSWT_ioctl_eth_eee_mode_t)) != 0 )
                    {
                        ret = EIO;
                    }
                }
                break;
            }

            case EXTSWT_GET_ETH_EEE_STATS:
            {
                EXTSWT_ioctl_eth_eee_stats_t cmd;
                if( copy_from_user((char *)&cmd, (void *)uarg, sizeof(EXTSWT_ioctl_eth_eee_stats_t)) == 0)
                {
                    ret = rtk83xx_get_eth_eee_stats( dev, &cmd.stats );
                    if( copy_to_user((void *)uarg, (char *)&cmd, sizeof(EXTSWT_ioctl_eth_eee_mode_t)) != 0 )
                    {
                        ret = EIO;
                    }
                }
                break;
            }

            case EXTSWT_GET_ETH_EEE_STATS_CLR:
            {
                EXTSWT_ioctl_eth_eee_stats_clr_t cmd;
                if( copy_from_user((char *)&cmd, (void *)uarg, sizeof(EXTSWT_ioctl_eth_eee_stats_clr_t)) == 0)
                {
                    ret = rtk83xx_get_eth_eee_stats_clear( dev, &cmd.clr );
                    if( copy_to_user((void *)uarg, (char *)&cmd, sizeof(EXTSWT_ioctl_eth_eee_stats_clr_t)) != 0 )
                    {
                        ret = EIO;
                    }
                }
                break;
            }
    
            default:
                ret = -EINVAL;
                break;
        }

        mutex_unlock( &dev->mutex );
    }

    return ret;
}

static void __exit rtk83xx_cleanup_module(void)
{
    rtk83xx_dev.api_initialized = FALSE;

    if ( rtk83xx_dev.irq_registered )
    {
        free_irq(AVALANCHE_INT_89, &rtk83xx_dev);
        rtk83xx_dev.irq_registered = FALSE;
    }

    unregister_chrdev_region(rtk83xx_deviceNo, EXTSWT_MINOR);
    arris_loop_detected_hook = 0;
#if defined( CONFIG_SYSTEM_MOCA ) || defined ( CONFIG_VENDOR_ARRIS_ENTROPIC_MOCA )
    if (pid_file)
    {
        kfree(pid_file->data);    
        remove_proc_entry("pid", moca_subagent_dir);
        remove_proc_entry("moca_subagent", NULL);
    }
#endif
}

#if defined( CONFIG_SYSTEM_MOCA ) || defined ( CONFIG_VENDOR_ARRIS_ENTROPIC_MOCA )
static int read_pid(char *page, char **start,
                            off_t off, int count, 
                            int *eof, void *data)
{
    int len=0;
    len += sprintf(page+len, "%d\n",pid);
    *eof = 1;
    return len;
}

static int write_pid(struct file *file,
                             const char __user *buffer,
                             unsigned long count, 
                             void *data)
{
    unsigned char local_buf[8]={0};
    if ( count < 8 )
    {
        if( copy_from_user(local_buf, buffer, count) )
        {
            return -EFAULT;
        }
        
        local_buf[count-1]='\0';

        if ( kstrtouint(local_buf, 0, &pid) )
        {
            /* Invalid input : use default */
            printk("Input error : %s\n", local_buf);
        }
    }
    return count;
}
#endif

static int __init rtk83xx_init_module(void)
{
    rtk_port_t  port;
    int         result;

    rtk83xx_dev.irq_registered = FALSE;
    rtk83xx_dev.api_initialized = FALSE;

    /* register device number. */
    rtk83xx_deviceNo = MKDEV(EXTSWT_MAJOR, EXTSWT_MINOR);
    result = register_chrdev_region(rtk83xx_deviceNo, EXTSWT_MINOR, "rtk83xx");

    if (result)
    {
        printk(KERN_ALERT "Error %d registering chrdrv rtk83xx device\n", result);

        /* will only get here on an error*/
        rtk83xx_cleanup_module();

        return result;
    }
    else
    {
        cdev_init(&rtk83xx_dev.cdev, &rtk83xx_fops);

        rtk83xx_dev.cdev.owner=THIS_MODULE;
        rtk83xx_dev.cdev.ops=&rtk83xx_fops;

        // initialize the rest of the members
        strcpy( rtk83xx_dev.name, "rtk83xx" );

        result = cdev_add(&rtk83xx_dev.cdev, rtk83xx_deviceNo, 1);

        //if failed
        if(result)
        {
            printk(KERN_NOTICE "Error adding chrdev rtk83xx %d", result );
            rtk83xx_cleanup_module();

            return result;
        }

        // Init mutex
        mutex_init( &rtk83xx_dev.mutex );

        // one-time init char dev
        rtk83xx_dev.tpid = 0;
        for (port=0; port<NUM_PORTS; port++)
        {
            rtk83xx_dev.pvid[port] = 0;
            rtk83xx_dev.provider[port] = 0;
        }
        INIT_LIST_HEAD( &rtk83xx_dev.vlans );
        INIT_LIST_HEAD( &rtk83xx_dev.reg_fds );
        arris_loop_detected_hook = rt83xx_loop_detected;
#if defined( CONFIG_SYSTEM_MOCA ) || defined ( CONFIG_VENDOR_ARRIS_ENTROPIC_MOCA )
        /* create a directory */
        moca_subagent_dir = proc_mkdir("moca_subagent", NULL);
        if(moca_subagent_dir)
        {
            pid_file = create_proc_entry("pid", 0644, moca_subagent_dir);
            if(pid_file == NULL) 
            {
                remove_proc_entry("moca_subagent", NULL);
            }
            else
            {
                pid_file->read_proc = read_pid;
                pid_file->write_proc = write_pid;
            }
        }
#endif
    }
    
    return 0;
}

module_init (rtk83xx_init_module);
module_exit (rtk83xx_cleanup_module);
