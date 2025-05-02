/*

Copyright (c) 2013-2016 ARRIS Enterprises, LLC

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

#define DRV_NAME        "Arris Kernel Modifications"
#define DRV_VERSION     "0.0.1"

#include <linux/module.h>
#include <linux/moduleparam.h>
#include <linux/kernel.h>
#include <linux/stat.h>
#include <linux/list.h>
#include <linux/sched.h>
#include <linux/interrupt.h>
#include <linux/cdev.h>
#include <linux/fs.h>
#include <linux/sockios.h>
#include <linux/proc_fs.h>
#include <asm/uaccess.h>
#include <asm/irq.h>
#include <asm-arm/arch-avalanche/puma6/puma6.h>

#include <arris/arris_customers.h>

/* Kernel module API */
#include "arris_mod.h"
#include "arris_rip.h"
#include <linux/ip.h>
#include <linux/udp.h>
#include <linux/in6.h>
#include <net/ipv6.h>


MODULE_AUTHOR ("Arris");
MODULE_DESCRIPTION (DRV_NAME);
MODULE_LICENSE("Dual BSD/GPL");
MODULE_VERSION(DRV_VERSION);

extern unsigned long ArrisCertFlag;

int custindex = CUSTOMER_DEFAULT;
unsigned int debug_mask  = DEBUG_MOD | DEBUG_RIP;
unsigned int debug_level = DEBUG_ERROR;

module_param(custindex, int, S_IRUGO); /* this value will be passed in so we know what we are running on.*/
MODULE_PARM_DESC(custindex,"information for customer specific initialization");

/************************************************************************/
/* registered device */
static dev_t            arris_deviceNo;

/* proc root for arris */
static struct proc_dir_entry *arris_proc = NULL;
static struct proc_dir_entry *arris_proc_dbgmsk = NULL;
static struct proc_dir_entry *arris_proc_dbglvl = NULL;
static struct proc_dir_entry *arris_proc_rtptuple = NULL;
static struct proc_dir_entry *arris_proc_rtptuple_enable = NULL;
static struct proc_dir_entry *arris_proc_fw_latest_log = NULL;


/* Character device fops */
static int              arris_open     (struct inode *inode, struct file *filp);
static long             arris_ioctl    (struct file *filp, unsigned int, unsigned long);
static int              arris_release  (struct inode *inode, struct file *filp);
static int              mta_rtp_tuple_check(struct sk_buff *skb);

/* optional ( per customer ) kernel modifications */
#define MOD_RIP_VIA_CMIP    (0x00000001)
static unsigned int mods_mask = 0;
/* The one and only arris kernel modifications device */
arris_dev_t   arris_dev;

/* device file operations */
struct file_operations arris_fops = {
    .owner          = THIS_MODULE,
    .unlocked_ioctl = arris_ioctl,
    .open           = arris_open,
    .release        = arris_release
};

static unsigned int rtp_tuple_length =0;
#define MAX_RTP_TUPLE_LENGTH  (4)
#define MAX_RTP_TUPLE_LIFETIME (120 * HZ) /* 120 s */
#define RTP_TUPLE_EXPIRES(tuple, now) \
    ((now >= tuple->stamp) ? (now - tuple->stamp >= MAX_RTP_TUPLE_LIFETIME) : (ULONG_MAX - tuple->stamp + now >= MAX_RTP_TUPLE_LENGTH))

typedef struct arris_rtp_tuple_t{
    struct in6_addr saddr;
    struct in6_addr daddr;
    unsigned short sport;
    unsigned short dport;
    unsigned short l3proto;
    unsigned long  stamp;
    struct list_head    list;
}arris_rtp_tuple;
#define IS_IPV6_ADDR_EQUAL(addr1, addr2) (!(((addr1)->s6_addr32[0] ^ (addr2)->s6_addr32[0]) | \
    ((addr1)->s6_addr32[1] ^ (addr2)->s6_addr32[1]) | \
    ((addr1)->s6_addr32[2] ^ (addr2)->s6_addr32[2]) | \
    ((addr1)->s6_addr32[3] ^ (addr2)->s6_addr32[3])))

LIST_HEAD(mta_rtp_tuples);
LIST_HEAD(mta_rtp_idle_tuples);
static struct net_device* mta0_dev     = NULL;
extern void (*arris_mta_dev_free)(struct net_device *);

#ifdef INCLUDE_INTEL_GW
/* defined in xt_limit.c */
extern int (*arris_mta_rtp_tuple_check)(struct sk_buff *);

/* defined in xt_evtlimit.c */
extern int (*arris_fw_log_read)(char* page, char** start, off_t offset, int count, int* eof, void* data);
#else
/* no xt_limit for non-GW, defines here */
int (*arris_mta_rtp_tuple_check)(struct sk_buff *) = NULL;
EXPORT_SYMBOL(arris_mta_rtp_tuple_check);

/* no xt_evtlimit for non-GW, defines here */
int (*arris_fw_log_read)(char* page, char** start, off_t offset, int count, int* eof, void* data) = NULL;
EXPORT_SYMBOL(arris_fw_log_read);
#endif

static unsigned int arris_mta_rtptuple_enable = 0;

/* Externs*/
extern void arris_bridge_init( void );
extern void arris_bridge_deinit( void );

/* private fops handlers */
static int arris_open(struct inode *inode, struct file *filp)
{
    arris_dev_t *dev = container_of(inode->i_cdev, arris_dev_t, cdev);

    /* Setup filep private data to point to our device */
    filp->private_data = dev;

    return 0;
}

static int arris_release(struct inode *inode, struct file *filp)
{
    return 0;
}

static long arris_ioctl(struct file* file, unsigned int cmd, unsigned long arg )
{  
    arris_dev_t  *dev = (arris_dev_t *)file->private_data;
    unsigned int __user *uarg = (unsigned int __user *)arg;
    unsigned int priv_cmd;
    long         ret = EIO;

    if (cmd != SIOCDEVPRIVATE)
    {
        PRINT_ERROR( DEBUG_MOD, "ARRIS IOCTL CMD not SIOCDEVPRIVATE\n");
        return (-EINVAL);
    }

    if(copy_from_user((char *)&priv_cmd, (void *)uarg, sizeof(unsigned int)))
    { 
        PRINT_ERROR( DEBUG_MOD, "ARRIS IOCTL CMD can't get userdata\n");
        return (-EINVAL); 
    }

    switch (priv_cmd)
    {
        case ARRIS_ADD_RIP_SUBNET:
        {
            if(mods_mask & MOD_RIP_VIA_CMIP )
            {
            arris_ioctl_add_rip_subnet_t add_sn;

            PRINT_INFO( DEBUG_RIP, "IOCTL: ARRIS_ADD_RIP_SUBNET" );
            if (copy_from_user((char *)&add_sn, (void *)uarg, sizeof(arris_ioctl_add_rip_subnet_t)) == 0)
            {
                ret = arris_rip_add_subnet( dev, &add_sn.sn );
                }
            }
            else
            {                
                PRINT_INFO( DEBUG_RIP, "IOCTL : ARRIS_ADD_RIP_SUBNET rip module not initialized" );
                ret = 0;
            }
            break;
        }

        case ARRIS_CLEAR_RIP_SUBNETS:
        {
            if(mods_mask & MOD_RIP_VIA_CMIP)
            {
            PRINT_INFO( DEBUG_RIP, "IOCTL: ARRIS_CLAER_RIP_SUBNETS" );
            ret = arris_rip_clear_subnets( dev );
            }
            else
            {
                PRINT_INFO( DEBUG_RIP, "IOCTL : ARRIS_ADD_RIP_SUBNET rip module not initialized" );    
                ret = 0;
            }
            break;
        }

        case ARRIS_SET_CERT_FLAG:
        {
            arris_ioctl_set_cert_flag_t setCert;

            PRINT_INFO( DEBUG_RIP, "IOCTL: ARRIS_SET_CERT_FLAG" );
            if (copy_from_user((char *)&setCert, (void *)uarg, sizeof(arris_ioctl_set_cert_flag_t)) == 0)
            {
                if( setCert.setting )
                {
                    ArrisCertFlag = 1;
                    printk( KERN_ERR "ARRIS: Kernel Op Mode is now set!\n" );
                }
                else
                {
                    ArrisCertFlag = 0;
                }
            }
            break;
        }

        default:
        {
            PRINT_ERROR( DEBUG_MOD, "IOCTL: UNKNOWN" );
            ret = -EINVAL;
            break;
        }
    }

    return ret;
}

/* proc file operations for the main module */
static int arris_proc_read_dbgmsk(char* page, char** start, off_t offset, int count, int* eof, void* data)
{
    int len=0;
        
    len += sprintf(page+len, "Current Debug Mask: 0x%08x\n", debug_mask );
    len += sprintf(page+len, "\n");
    len += sprintf(page+len, " bit\tdescription\n");
    len += sprintf(page+len, "  0\tarris kernel mods\n");
    len += sprintf(page+len, "  1\trip via cm ip\n");

    *eof = 1;
    return len;
}

static int arris_proc_write_dbgmsk(struct file* fp, const char* buf, unsigned long count, void* data)
{
    unsigned char local_buf[32];
    if ( count < 32 )
    {
        if( copy_from_user(local_buf, buf, count) )
        {
            return -EFAULT;
        }
        local_buf[count-1]='\0';

        if ( kstrtouint(local_buf, 0, &debug_mask) )
        {
            /* Invalid input : use default */
            PRINT_ERROR( DEBUG_MOD, "Input error : %s\n", local_buf);
        }
    }
    
    return count;
}

static int arris_proc_read_dbglvl(char* page, char** start, off_t offset, int count, int* eof, void* data)
{
    int len=0;

    len += sprintf(page+len, "Current Debug Level: %d\n", debug_level );
    
    *eof = 1;
    return len;
}

static int arris_proc_write_dbglvl(struct file* fp, const char* buf, unsigned long count, void* data)
{
    unsigned char local_buf[32];
    if ( count < 32 )
    {
        if( copy_from_user(local_buf, buf, count) )
        {
            return -EFAULT;
        }
        local_buf[count-1]='\0';

        if ( kstrtouint(local_buf, 0, &debug_level) )
        {
            /* Invalid input : use default */
            PRINT_ERROR( DEBUG_MOD, "Input error : %s\n", local_buf);
        }
    }

    return count;
}

static int arris_proc_read_rtptuple_enable(char* page, char** start, off_t offset, int count, int* eof, void* data)
{
    int len=0;

    len += sprintf(page+len, "MTA RTP Tuple Hacker Enable: %d(%sabled)\n", arris_mta_rtptuple_enable,
            arris_mta_rtptuple_enable ? "en" : "dis");
    
    *eof = 1;
    return len;
}

static int arris_proc_write_rtptuple_enable(struct file* fp, const char* buf, unsigned long count, void* data)
{
    unsigned char local_buf[32] = {0};
    if ( count < 32 )
    {
        if( copy_from_user(local_buf, buf, count) )
        {
            return -EFAULT;
        }

        if (local_buf[count - 1] == '\n')
            local_buf[count - 1] = '\0';

        if (sscanf(local_buf, "%u", &arris_mta_rtptuple_enable) != 1)
        {
            /* Invalid input : use default */
            printk("Input error : %s\n", local_buf);
        }
    }

    return count;
}

static void free_rtp_tuple(arris_rtp_tuple *tuple)
{
    list_del(&tuple->list);

    /* add to idle list */
    INIT_LIST_HEAD(&tuple->list);
    list_add_tail(&tuple->list, &mta_rtp_idle_tuples);
    --rtp_tuple_length;
}

static arris_rtp_tuple *arris_find_rtp_tuple4(const unsigned int saddr, const unsigned int daddr, Uint16 sport, Uint16 dport)
{
    arris_rtp_tuple *ent, *next;
    unsigned long now = jiffies;

    list_for_each_entry_safe(ent, next, &mta_rtp_tuples, list)
    {
        if (RTP_TUPLE_EXPIRES(ent, now))
        {
            free_rtp_tuple(ent);
        }
        if ((ent->l3proto ^ AF_INET) ||
            (ent->saddr.s6_addr32[0] ^ saddr) || 
            (ent->daddr.s6_addr32[0] ^ daddr) ||
            (ent->sport ^ sport) ||
            (ent->dport ^ dport))
        {
            continue;
        }
        else
        {
            return ent;
        }
    }

    return 0;
}

static arris_rtp_tuple *arris_find_rtp_tuple6(const struct in6_addr *saddr, const struct in6_addr *daddr, Uint16 sport, Uint16 dport)
{
    arris_rtp_tuple *ent, *next;
    unsigned long now = jiffies;

    list_for_each_entry_safe(ent, next, &mta_rtp_tuples, list)
    {
        if (RTP_TUPLE_EXPIRES(ent, now))
        {
            free_rtp_tuple(ent);
        }
        else if ((ent->l3proto ^ AF_INET6) ||
            !IS_IPV6_ADDR_EQUAL(&ent->saddr, saddr) ||
            !IS_IPV6_ADDR_EQUAL(&ent->daddr, daddr) ||
            (ent->sport ^ sport) ||
            (ent->dport ^ dport))
        {
            continue;
        }
        else
        {
            return ent;
        }
    }

    return 0;
}

static int convert_ipv6_str_to_uncompact(char *ip6str, char *uncompactstr)
{
    int single_colon_count, double_colon;
    int i, len;
    char *p, *p1 = NULL;
    single_colon_count = 0;
    double_colon = -1;
    len = strlen(ip6str);

    p = strstr(ip6str, "::");
    if (p)
    {
        double_colon = p - ip6str;
        p1 = p;
    }    

    /* scan the string */
    for (i = 0; i < len; ++i)
    {
        switch (*(ip6str + i))
        {
            case ':':
                ++single_colon_count;
                break;
            default:
                {
                    p = ip6str + i;
                    if ((*p >= '0' && *p <= '9') || (*p >= 'a' && *p <= 'f') || (*p >= 'A' && *p <= 'F'))
                    {
                    }
                    else
                    {
                        return -1;
                    }
                }
                break;
        }
    }

    if (double_colon >= 0)
    {
        single_colon_count -= 2;
        if (single_colon_count > 5) return -1;
    }
    
    if (single_colon_count < 0 || single_colon_count > 7) return -1;

    if (double_colon < 0)
    {
        strcpy(uncompactstr, ip6str);
        return 0;
    }
    
    memcpy(uncompactstr, ip6str, double_colon);
    *(uncompactstr + double_colon) = '\0';

    /* appending 7-single_colon_count colons */
    p = uncompactstr + double_colon;
    /* leading "::" ? */
    if (double_colon == 0)
    {
        strcat(p, "0");
    }
    
    for (i = 7 - single_colon_count - 1; i > 0; --i)
    {
        strcat(p, ":0");
    }

    /* one colon left! */
    /* tailing "::" ? */
    if (double_colon == len - 2)
    {
        strcat(p, ":0");
    }
    else
    {
        strcat(p, ":");
        strcat(p, p1 + 2);
    }
    
    return 0;
}

static int convert_ipv6_from_str(char *ip6str, struct in6_addr *addr6, int *prefix_len)
{
    char *prefix;
    char buf[64];
    int ret;
 #define ARRIS_REORDER_ENDIAN_S(a) (a) = htons((a));

    prefix = strstr(ip6str, "/");
    if (prefix == NULL)
    {
        if (prefix_len)
            *prefix_len = 128;
    }
    else
    {
        *prefix ++ = '\0';
        if (prefix_len)
            sscanf(prefix, "%d", prefix_len);
    }

    if (convert_ipv6_str_to_uncompact(ip6str, buf) < 0)
    {
        return -1;
    }

    ret = sscanf(buf, "%hx:%hx:%hx:%hx:%hx:%hx:%hx:%hx", addr6->s6_addr16, addr6->s6_addr16 + 1, addr6->s6_addr16 + 2,
        addr6->s6_addr16 + 3, addr6->s6_addr16 + 4, addr6->s6_addr16 + 5, addr6->s6_addr16 +6, addr6->s6_addr16 +7);
    
    ARRIS_REORDER_ENDIAN_S(addr6->s6_addr16[0]);
    ARRIS_REORDER_ENDIAN_S(addr6->s6_addr16[1]);
    ARRIS_REORDER_ENDIAN_S(addr6->s6_addr16[2]);
    ARRIS_REORDER_ENDIAN_S(addr6->s6_addr16[3]);
    ARRIS_REORDER_ENDIAN_S(addr6->s6_addr16[4]);
    ARRIS_REORDER_ENDIAN_S(addr6->s6_addr16[5]);
    ARRIS_REORDER_ENDIAN_S(addr6->s6_addr16[6]);
    ARRIS_REORDER_ENDIAN_S(addr6->s6_addr16[7]);

    return (ret == 8 ? 0 : -1);
    
}

static arris_rtp_tuple *new_rtp_tuple()
{
    arris_rtp_tuple *ent, *next;
    int any_idle = 0;

    /* any room in idle list ? */
    list_for_each_entry_safe(ent, next, &mta_rtp_idle_tuples, list)
    {
        any_idle = 1;
        list_del(&ent->list);
        break;
    }

    if (any_idle)
    {
        ++rtp_tuple_length;        
    }
    else
    {
        /* no more idle, reuse oldest one */
        list_for_each_entry_safe(ent, next, &mta_rtp_tuples, list)
        {
            list_del(&ent->list);
            break;
        }
    }

    if (ent)
    {
        memset(ent, 0, sizeof(arris_rtp_tuple));
        ent->stamp = jiffies;
    }
    
    return ent;
}

void arris_mta_tuple_free_dev(struct net_device *dev)
{
    if (mta0_dev && (mta0_dev == dev))
    {
        dev_put(mta0_dev);
        mta0_dev = NULL;
    }
}

static int rtp_tuples_init()
{
    arris_rtp_tuple *tuple;
    int i = 0;

    arris_mta_rtp_tuple_check = mta_rtp_tuple_check;
    arris_mta_dev_free = arris_mta_tuple_free_dev;

    for (i = 0; i < MAX_RTP_TUPLE_LENGTH; ++i)
    {
        tuple = (arris_rtp_tuple*)kmalloc(sizeof(arris_rtp_tuple), GFP_KERNEL);
        if (tuple)
        {
            /* add to idle list */
            INIT_LIST_HEAD(&tuple->list);
            list_add_tail(&tuple->list, &mta_rtp_idle_tuples);
        }
        else
        {
            return -1;
        }
    }

    return 0;
}

static void rtp_tuples_deinit()
{
    arris_rtp_tuple *ent, *next;

    list_for_each_entry_safe(ent, next, &mta_rtp_tuples, list)
    {
        list_del(&ent->list);
        kfree(ent);        
    }

    list_for_each_entry_safe(ent, next, &mta_rtp_idle_tuples, list)
    {
        list_del(&ent->list);
        kfree(ent);
    }
}

static void rtp_tuples_del_all()
{
    arris_rtp_tuple *ent, *next;

    list_for_each_entry_safe(ent, next, &mta_rtp_tuples, list)
    {
        /* move to idle list */
        free_rtp_tuple(ent);
    }
}

static int arris_proc_read_rtptuple(char* page, char** start, off_t offset, int count, int* eof, void* data)
{
    int len=0;
    arris_rtp_tuple *ent, *next;
    unsigned long now = jiffies;
    
#define NIPQUAD(addr) \
        ((unsigned char *)&addr)[0], \
        ((unsigned char *)&addr)[1], \
        ((unsigned char *)&addr)[2], \
        ((unsigned char *)&addr)[3]

    len += sprintf(page+len, "Current RTP tuples: %d - %sabled\n", rtp_tuple_length, arris_mta_rtptuple_enable ? "en" : "dis");
    
    list_for_each_entry_safe(ent, next, &mta_rtp_tuples, list)
    {
        if (ent->l3proto == AF_INET)
        {
            len += sprintf(page+len, "src:%d.%d.%d.%d:%hu  dst:%d.%d.%d.%d:%hu %s\n", NIPQUAD(ent->saddr.s6_addr32[0]), 
                ntohs(ent->sport), NIPQUAD(ent->daddr.s6_addr32[0]), ntohs(ent->dport),
                RTP_TUPLE_EXPIRES(ent, now) ? "dead" : "alive");
        }
        else if(ent->l3proto == AF_INET6)
        {
            len += sprintf(page+len, "src:%pI6c.%hu dst:%pI6c.%hu %s\n", ent->saddr.s6_addr, ntohs(ent->sport),
                ent->daddr.s6_addr, ntohs(ent->dport), RTP_TUPLE_EXPIRES(ent, now) ? "dead" : "alive");
        }
    }
    
    *eof = 1;   
    return len;
}

/*
  usage:
  add -{4|6} {source_ip} {source_port} {dest_ip} {dest_port}
  del -{4|6} {source_ip} {source_port} {dest_ip} {dest_port}
  del all
*/
static int arris_proc_write_rtptuple(struct file* fp, const char* buf, unsigned long count, void* data)
{
    unsigned char local_buf[256] = {0};
    unsigned char tmpbuf[64];
    char *p;
    int ret = count;
    int adddel = 0;
    int ipv4 = 1;
    int i = 0;
    struct in_addr srcip, dstip;
    struct in6_addr srcip6, dstip6;
    unsigned short sport, dport;
    unsigned short stmp[8];
    arris_rtp_tuple *tuple;

#define MAKE_SADDR4(a, b, c, d) \
    ((((unsigned char)a) << 24) | ((unsigned char)b << 16) | ((unsigned char)c << 8) | ((unsigned char)d))
    
    if ( count < sizeof(local_buf))
    {
        if( copy_from_user(local_buf, buf, count) )
        {
            return -EFAULT;
        }

        if (local_buf[count - 1] == '\n')
            local_buf[count - 1] = '\0';

        p = local_buf;
        while (*p == ' ') ++p;

        if ((*p == 'A' || *p == 'a') && (*(p+1) == 'd' || *(p+1) == 'D') && (*(p+2) == 'd' || *(p+2) == 'D'))
        {
            /* add tuples */
            adddel = 1;
        }
        else if ((*p == 'd' || *p == 'D') && (*(p+1) == 'e' || *(p+1) == 'E') && (*(p+2) == 'l' || *(p+2) == 'L'))
        {
            /* del tuples */
            adddel = 0;
        }
        else
        {
            return ret;
        }

        /* parse tuples */
        p += 3;
        while (*p == ' ') ++p;

        /* allow add while disabled */
        //if (adddel && !arris_mta_rtptuple_enable)  return ret;

        if (!adddel)
        {
            /* del all? */
            if ((*p=='a'||*p=='A') && (*(p+1)=='l'||*(p+1)=='L') && (*(p+2)=='l'||*(p+2)=='L'))
            {
                rtp_tuples_del_all();
                return ret;
            }
        }

        /* get addr family */
        if (*p == '-' && *(p+1) == '4')
        {
            ipv4 = 1;
        }
        else if (*p == '-' && *(p+1) == '6')
        {
            ipv4 = 0;
        }
        else
        {
            return ret;
        }

        p += 2;        
        while (*p == ' ') ++p;

        if (ipv4)
        {
            if (sscanf(p, "%hu.%hu.%hu.%hu %hu %hu.%hu.%hu.%hu %hu", stmp, stmp + 1, stmp + 2, stmp +3, &sport, 
                    stmp + 4, stmp + 5, stmp + 6, stmp + 7, &dport) != 10)
            {
                return ret;
            }

            srcip.s_addr = MAKE_SADDR4(stmp[0], stmp[1], stmp[2], stmp[3]);
            dstip.s_addr = MAKE_SADDR4(stmp[4], stmp[5], stmp[6], stmp[7]);

            tuple = arris_find_rtp_tuple4(srcip.s_addr, dstip.s_addr, sport, dport);
            if (adddel)
            {
                /* add */
                if (tuple)
                {
                    return ret;
                }

                tuple = new_rtp_tuple();
                if (tuple)
                {
                    INIT_LIST_HEAD(&tuple->list);
                    tuple->saddr.s6_addr32[0] = srcip.s_addr;
                    tuple->daddr.s6_addr32[0] = dstip.s_addr;
                    tuple->sport = htons(sport);
                    tuple->dport = htons(dport);
                    tuple->l3proto = AF_INET;

                    list_add_tail(&tuple->list, &mta_rtp_tuples);
                }
            }
            else
            {
                /* del */
                if (tuple)
                {
                    free_rtp_tuple(tuple);
                }
            }
        }
        else
        {
            /* ipv6 src addr */
            for (i=0; i<sizeof(tmpbuf)-1 && *(p+i) != 0 && *(p+i) != ' '; ++i)
            {
                tmpbuf[i] = *(p + i);
            }

            if (*(p+i) == 0) return ret;
            tmpbuf[i] = 0;            
            
            convert_ipv6_from_str(tmpbuf, &srcip6, 0);

            p += i;
            while (*p == ' ') ++p;

            /* src port */
            for (i=0; i<sizeof(tmpbuf)-1 && *(p+i) != 0 && *(p+i) != ' '; ++i)
            {
                tmpbuf[i] = *(p + i);
            }

            if (*(p+i) == 0) return ret;
            tmpbuf[i] = 0;            
            if (sscanf(tmpbuf, "%hu", &sport) != 1)
            {
                return ret;
            }

            p += i;
            while (*p == ' ') ++p;

            /* ipv6 dst ip */
            for (i=0; i<sizeof(tmpbuf)-1 && *(p+i) != 0 && *(p+i) != ' '; ++i)
            {
                tmpbuf[i] = *(p + i);
            }

            if (*(p+i) == 0) return ret;
            tmpbuf[i] = 0;            
            
            convert_ipv6_from_str(tmpbuf, &dstip6, 0);

            
            p += i;
            while (*p == ' ') ++p;

            /* dst port */            
            for (i=0; i<sizeof(tmpbuf)-1 && *(p+i) != 0 && *(p+i) != ' '; ++i)
            {
                tmpbuf[i] = *(p + i);
            }

            /* last parameter, can be NULL */
            //if (*(p+i) == 0) return ret;
            tmpbuf[i] = 0;            
            if (sscanf(tmpbuf, "%hu", &dport) != 1)
            {
                return ret;
            }

            tuple = arris_find_rtp_tuple6(&srcip6, &dstip6, sport, dport);
            if (adddel)
            {
                /* add */
                if (tuple)
                {
                    return ret;
                }

                tuple = new_rtp_tuple();
                if (tuple)
                {
                    INIT_LIST_HEAD(&tuple->list);
                    memcpy(tuple->saddr.s6_addr, srcip6.s6_addr, sizeof(struct in6_addr));
                    memcpy(tuple->daddr.s6_addr, dstip6.s6_addr, sizeof(struct in6_addr));
                    tuple->sport = htons(sport);
                    tuple->dport = htons(dport);
                    tuple->l3proto = AF_INET6;

                    list_add_tail(&tuple->list, &mta_rtp_tuples);
                }
            }
            else
            {
                /* del */
                if (tuple)
                {
                    free_rtp_tuple(tuple);
                }
            }
        }
        
    }

    return ret;
}

inline static int arris_scan_ipv6(struct ipv6hdr* ptr_ipv6hdr, Uint8* ipv6HeaderLen, Uint8* nexthdr)
{
    struct ipv6_opt_hdr* hdr = NULL;
    unsigned int hdrlen;
    unsigned int nextOffset;

    *nexthdr = ptr_ipv6hdr->nexthdr;
    *ipv6HeaderLen = sizeof(struct ipv6hdr);

    /* Stop at one of the supported protocols */
    /* Iterate through well-known extenstion headers till we reach a supported protocol */
    while ((*nexthdr != IPPROTO_TCP) && (*nexthdr != IPPROTO_UDP) && (*nexthdr != IPPROTO_IPIP) && (*nexthdr != IPPROTO_GRE)
        && (*nexthdr != IPPROTO_ICMPV6))
    {
        /* If this is the last next header */
        if (*nexthdr == NEXTHDR_NONE) 
        {
            return -1;
        }

        /* Encrypted header - cannot parse it, treat as uknown header */
        if (*nexthdr == NEXTHDR_ESP)
        {
            return -1;
        }

        hdr = (struct ipv6_opt_hdr*)((unsigned char *)ptr_ipv6hdr + *ipv6HeaderLen);

        if (*nexthdr == NEXTHDR_FRAGMENT)
        {
            hdrlen = 8;
        }
        else if (*nexthdr == NEXTHDR_AUTH)
        {
            hdrlen = (hdr->hdrlen + 2) << 2;
        }
        else
        {
            hdrlen = ipv6_optlen(hdr);
        }

        nextOffset = (unsigned int)*ipv6HeaderLen + hdrlen;

        /* preventing a wrap - if the offset exceeds 255 bytes we exit the loop. */
        if (nextOffset > 0xFF)
        {
            return -1;
        }
        else
        {
            *ipv6HeaderLen = (unsigned char)nextOffset;
        }

        *nexthdr = hdr->nexthdr;
    }

    return 0;
}

inline static int arris_scan_ipv6_port(struct ipv6hdr* ptr_ipv6hdr, unsigned short *sport, unsigned short *dport, Uint8 *next_hdr, Uint8 *ip6hlen)
{
    Uint8 ipv6header_len;
    struct tcphdr *ptr_tcphdr;
    struct icmp6hdr *icmp6;
    if (arris_scan_ipv6(ptr_ipv6hdr, &ipv6header_len, next_hdr) != 0 )
    {
        return -1;
    }

    if (*next_hdr != IPPROTO_UDP && *next_hdr != IPPROTO_TCP && *next_hdr != IPPROTO_ICMPV6)
    {
        return -1;
    }

    if (ip6hlen) *ip6hlen = ipv6header_len;

    if (*next_hdr == IPPROTO_ICMPV6)
    {
        icmp6 = (struct icmp6hdr*)((unsigned char*)ptr_ipv6hdr + ipv6header_len);
        *sport = icmp6->icmp6_type;
        return 0;
    }
    
    ptr_tcphdr = (struct tcphdr *)((unsigned char *)ptr_ipv6hdr + ipv6header_len);

    *sport = ptr_tcphdr->source;
    *dport = ptr_tcphdr->dest;
    return 0;
}



static int mta_rtp_tuple_check(struct sk_buff *skb)
{
    struct iphdr *iph;
    struct udphdr *udph;
    struct ipv6hdr *ip6h;
    unsigned short sport, dport;
    Uint8 nextheader, ip6hlen;

    if (!arris_mta_rtptuple_enable)
    {
        return 0;
    }
    
    if (!mta0_dev)
    {
        mta0_dev = dev_get_by_name(&init_net, "mta0");
    }

    if (!mta0_dev || !(mta0_dev->flags & IFF_UP))
    {
        return 0;
    }
    
    if (!mta0_dev || skb->dev != mta0_dev)
    {
        return 0;
    }
    
    if(skb->protocol == ETH_P_IP)
    {
        iph = (struct iphdr *)skb_network_header(skb);
        if (iph->protocol != IPPROTO_UDP)
        {
            return 0;
        }
        
        udph = (struct udphdr*)((unsigned char*)iph + iph->ihl * 4);
        
        if (arris_find_rtp_tuple4(iph->saddr, iph->daddr, udph->source, udph->dest))
        {
            return 1;
        }
    }
    else if (skb->protocol == ETH_P_IPV6)
    {
        ip6h = (struct ipv6hdr *)skb_network_header(skb);
        if (arris_scan_ipv6_port(ip6h, &sport, &dport, &nextheader, &ip6hlen) != 0 ||
            nextheader != IPPROTO_UDP)
        {
            return 0;
        }

        if (arris_find_rtp_tuple6(&ip6h->saddr, &ip6h->daddr, sport, dport))
        {
            return 1;
        }
    }

    return 0;    
}

static void arris_mta_rtptuple_deinit(struct proc_dir_entry* root)
{
    arris_mta_rtp_tuple_check = NULL;
    arris_mta_dev_free = NULL;
    
    if (root && arris_proc_rtptuple)
    {
        remove_proc_entry("rtptuple", root);
    }

    if (root && arris_proc_rtptuple_enable)
    {
        remove_proc_entry("rtptuple_enable", root);
    }

    if (mta0_dev)
    {
        dev_put(mta0_dev);
        mta0_dev = NULL;
    }

    rtp_tuples_deinit();
}

static void arris_create_proc_entries( void )
{
    /* create base directory for arris procs : use the root dir as parent */
    arris_proc = proc_mkdir("arris", NULL);
    if (!arris_proc)
    {
        PRINT_ERROR( DEBUG_MOD, "Unable to proc dir entry\n");
        remove_proc_entry("arris", NULL);
    }
    else
    {
        /* create debug mask proc */
        arris_proc_dbgmsk = create_proc_entry("dbgmsk", 0644, arris_proc);
        if (arris_proc_dbgmsk)
        {
            arris_proc_dbgmsk->read_proc  = arris_proc_read_dbgmsk;
            arris_proc_dbgmsk->write_proc = arris_proc_write_dbgmsk;
        }

        /* create debug level proc */
        arris_proc_dbglvl = create_proc_entry("dbglvl", 0644, arris_proc);
        if (arris_proc_dbglvl)
        {
            arris_proc_dbglvl->read_proc  = arris_proc_read_dbglvl;
            arris_proc_dbglvl->write_proc = arris_proc_write_dbglvl;
        }

        /* create rtp tuple proc */
        arris_proc_rtptuple = create_proc_entry("rtptuple", 0644, arris_proc);
        if (arris_proc_rtptuple)
        {
            arris_proc_rtptuple->read_proc = arris_proc_read_rtptuple;
            arris_proc_rtptuple->write_proc= arris_proc_write_rtptuple;
        }

        arris_proc_rtptuple_enable = create_proc_entry("rtptuple_enable", 0644, arris_proc);
        if (arris_proc_rtptuple_enable)
        {
            arris_proc_rtptuple_enable->read_proc = arris_proc_read_rtptuple_enable;
            arris_proc_rtptuple_enable->write_proc = arris_proc_write_rtptuple_enable;
        }

        arris_proc_fw_latest_log = create_proc_entry("fw_log", 0644, arris_proc);
        if (arris_proc_fw_latest_log)
        {
            arris_proc_fw_latest_log->read_proc = arris_fw_log_read ? arris_fw_log_read : NULL;
            arris_proc_fw_latest_log->write_proc = NULL;
        }
    }
}

/* module load / unload operations */
static void __exit arris_cleanup_module(void)
{
    // restore each kernel modification section here
    arris_rip_deinit(arris_proc);
    arris_mta_rtptuple_deinit(arris_proc);

    arris_bridge_deinit();

    unregister_chrdev_region(arris_deviceNo, ARRIS_MINOR);
}

static int __init arris_init_module(void)
{
    int         result;

    /* register device number. */
    arris_deviceNo = MKDEV(ARRIS_MAJOR, ARRIS_MINOR);
    result = register_chrdev_region(arris_deviceNo, ARRIS_MINOR, "arris");

    if (result)
    {
        PRINT_ERROR( DEBUG_MOD, "Error %d registering chrdrv arris device\n", result);

        /* will only get here on an error*/
        arris_cleanup_module();

        return result;
    }
    else
    {
        cdev_init(&arris_dev.cdev, &arris_fops);

        arris_dev.cdev.owner=THIS_MODULE;
        arris_dev.cdev.ops=&arris_fops;

        // initialize the rest of the members
        strcpy( arris_dev.name, "arris" );

        result = cdev_add(&arris_dev.cdev, arris_deviceNo, 1);

        //if failed
        if(result)
        {
            PRINT_ERROR( DEBUG_MOD, "Error adding chrdev arris %d", result );
            arris_cleanup_module();

            return result;
        }

        // Initialize proc filesystem
        arris_create_proc_entries();

        rtp_tuples_init();
        
        // Initialize each kernel modification section here, based on customer index
        switch( custindex )
        {
            case CUSTOMER_TIMEWARNER:
                PRINT_INFO( DEBUG_MOD, "Installing TWC kernel modifications\n" );
                mods_mask = MOD_RIP_VIA_CMIP;
                break;

            case CUSTOMER_COMCAST:
                PRINT_INFO( DEBUG_MOD, "Installing CT kernel modifications\n" );
                mods_mask = MOD_RIP_VIA_CMIP;
                break;
// UNIHAN ADD START, PROD00220394
            case CUSTOMER_SUDDENLINK:
                PRINT_INFO( DEBUG_MOD, "Installing SL kernel modifications\n" );
                mods_mask = MOD_RIP_VIA_CMIP;
                break;
// UNIHAN ADD END, PROD00220394
            default:
                PRINT_ERROR( DEBUG_MOD, "unknown customer\n" );
                mods_mask = 0;
                break;
        }

        if ( mods_mask & MOD_RIP_VIA_CMIP )
        {
            arris_rip_init(arris_proc);
        }

        arris_bridge_init();
    }
    
    return 0;
}

module_init (arris_init_module);
module_exit (arris_cleanup_module);
