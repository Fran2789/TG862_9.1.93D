/*

Copyright (c) 2013-2014 ARRIS Enterprises, LLC

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

#if defined(MODVERSIONS)
#include <linux/modversions.h>
#endif
#include <linux/module.h>
#include <linux/skbuff.h>
#include <linux/netdevice.h>
#include <linux/ip.h>
#include <linux/udp.h>
#include <linux/string.h>
#include <linux/list.h>
#include <linux/proc_fs.h>
#include <linux/spinlock.h>
#include <net/dsfield.h>
#include <linux/if_vlan.h>

MODULE_LICENSE("Dual BSD/GPL");

#define XT_DSCP_MASK	0xfc	/* 11111100 */
#define XT_DSCP_SHIFT	2

#define gre_mask 0xf800

#define SC_GRE_TX_FILTER_HOOK_DEBUG
#ifdef SC_GRE_TX_FILTER_HOOK_DEBUG
#define DPRINTK(format, argument...) printk(format,##argument)
#else
#define DPRINTK(format, argument...)
#endif

typedef struct hotspot_DSCP_s {
	unsigned int index;
	unsigned int dscp;
    struct list_head list; /* kernel's list structure */
} hotspot_DSCP_t;

static hotspot_DSCP_t hotspot_dscp_listhead;
static struct proc_dir_entry *hotspot_dscp_proc;

/*for spin lock*/
//static DEFINE_SPINLOCK(hotspot_vlan_pri_lock);
//static unsigned long hotspot_vlan_pri_lock_flag;

static DEFINE_SPINLOCK(sc_gre_tx_filter_lock);
static unsigned long sc_gre_tx_filter_lock_flag;


//extern int (*sc_hotspot_vlan_pri_hook)(struct net_device *dev, struct sk_buff *skb);

extern int (*sc_gre_tx_filter_hook)(struct sk_buff *skb);


#if 0
static void DumpData(char *data, unsigned int len)
{
    char *p = (char *)data;
    int i;
    for(i=1;i<=len;i++)
    {
        printk("%02x ", *(p+(i-1)));

        if( (i%16) == 0 )
        {
            printk("\n");
        }
    }
    printk("\n");
}
#endif 
static int gre_dscp_handler_ipv4(struct sk_buff *skb)
{
        struct iphdr *iph;

    struct tcphdr *tcph;
    struct udphdr *udph;
    unsigned int oldmss, newmss;

	struct vlan_ethhdr *veth;
	int GRE_HEADER_SIZE = 4;
	int gre_index = 0;
				
     if(skb->protocol == __constant_htons(ETH_P_IP)){
	 	//printk("GRE packet LINE=%d\n", __LINE__);

		iph = ip_hdr(skb);

        if (iph->protocol == IPPROTO_GRE) {

				// Start change DSCP
				hotspot_DSCP_t* entry;
				unsigned long temp_tci;

				if(skb->mark!=0)					
				{
					//printk("[GRE]LINE=%d|skb->mark=%lx\n", __LINE__, skb->mark);
					gre_index = (unsigned int)(skb->mark ^ gre_mask);
					//printk("[GRE]LINE=%d|gre_index=%d\n", __LINE__, gre_index);
				}
				
			    spin_lock_irqsave(&sc_gre_tx_filter_lock, sc_gre_tx_filter_lock_flag);
			    list_for_each_entry(entry, &(hotspot_dscp_listhead.list), list) {
			        //if( dev != NULL && strcmp(dev->name, entry->forward_if) == 0)
			        //printk("[GRE]LINE=%d|entry->index=%d| entry->dscp=%d\n", __LINE__, entry->index, entry->dscp);
			        if(gre_index == entry->index)
			        {
			            u_int8_t dscp_old = ipv4_get_dsfield(ip_hdr(skb)) >> XT_DSCP_SHIFT;
						u_int8_t dscp_new = (u_int8_t)entry->dscp;
						//printk("[GRE]LINE=%d|dscp_old=%d\n", __LINE__, dscp_old);
		                ipv4_change_dsfield(ip_hdr(skb), (__u8)(~XT_DSCP_MASK), dscp_new << XT_DSCP_SHIFT);						

						//printk("[GRE]LINE=%d|skb->dev.name=%s\n", __LINE__, skb->dev->name);
			        }
			    }
			    spin_unlock_irqrestore(&sc_gre_tx_filter_lock, sc_gre_tx_filter_lock_flag);
				// end change DSCP

}
			 
}
	return 0;
}

static int gre_dscp_handler_ipv6(struct sk_buff *skb)
{
    struct ipv6hdr *ipv6h;
    int gre_index = 0;
    ipv6h = ipv6_hdr(skb);

     if(ipv6h->nexthdr == IPPROTO_GRE)
     {
                    // Start change DSCP
                hotspot_DSCP_t* entry;
                unsigned long temp_tci;

                if(skb->mark!=0)                    
                {
					//printk("[GRE]LINE=%d|skb->mark=%lx\n", __LINE__, skb->mark);
					gre_index = (unsigned int)(skb->mark ^ gre_mask);
					//printk("[GRE]LINE=%d|gre_index=%d\n", __LINE__, gre_index);
				}
				
			    spin_lock_irqsave(&sc_gre_tx_filter_lock, sc_gre_tx_filter_lock_flag);
			    list_for_each_entry(entry, &(hotspot_dscp_listhead.list), list) {
			        //if( dev != NULL && strcmp(dev->name, entry->forward_if) == 0)
			        //printk("[GRE]LINE=%d|entry->index=%d| entry->dscp=%d\n", __LINE__, entry->index, entry->dscp);
			        if(gre_index == entry->index)
			        {
                        u_int8_t dscp_old = ipv6_get_dsfield(ipv6_hdr(skb)) >> XT_DSCP_SHIFT;
						u_int8_t dscp_new = (u_int8_t)entry->dscp;
						//printk("[GRE]LINE=%d|dscp_old=%d\n", __LINE__, dscp_old);
                        ipv6_change_dsfield(ipv6_hdr(skb), (__u8)(~XT_DSCP_MASK), dscp_new << XT_DSCP_SHIFT);                       

			        }
			    }
			    spin_unlock_irqrestore(&sc_gre_tx_filter_lock, sc_gre_tx_filter_lock_flag);
    }               // end change DSCP

}


static int gre_dscp_handler_main(struct sk_buff *skb)
{
    if(skb->protocol == __constant_htons(ETH_P_IP)){
        gre_dscp_handler_ipv4(skb);
    }
    else if(skb->protocol == __constant_htons(ETH_P_IPV6 )){
        gre_dscp_handler_ipv6(skb);
}
	return 0;
}


static int hotspot_dscp_proc_read(char* page, char **start, off_t offset, int count, int *eof, void *data)
{
    int limit = count;
    char* buf = page + offset;
    int len=0;
    hotspot_DSCP_t* entry;
    len += snprintf(buf+len, limit-len, "Usage:\n");
    len += snprintf(buf+len, limit-len, "    Add: A <index> <dscp>\n");
    len += snprintf(buf+len, limit-len, "    Del: D <index>\n");
    len += snprintf(buf+len, limit-len, "\n");
    len += snprintf(buf+len, limit-len, "Current settings:\n");
    len += snprintf(buf+len, limit-len, " <index>  <dscp>\n");

    spin_lock_irqsave(&sc_gre_tx_filter_lock, sc_gre_tx_filter_lock_flag);
    list_for_each_entry(entry, &(hotspot_dscp_listhead.list), list) {
        len += snprintf(buf+len, limit-len, " %d  %d\n", entry->index, entry->dscp);
    }
    spin_unlock_irqrestore(&sc_gre_tx_filter_lock, sc_gre_tx_filter_lock_flag);

    return len;

}

static int hotspot_dscp_proc_write(struct file *file, const char *buffer, unsigned long count, void *data)
{
    char cmd[128];
    char action[128], index[128], dscp[128];
    hotspot_DSCP_t* entry;
    hotspot_DSCP_t* temp_entry;

    if (count > (sizeof(cmd) - 1)) {
        return -EFAULT;
    }

    memset(cmd, 0, sizeof(cmd));
    memset(action, 0, sizeof(action));
    memset(index, 0, sizeof(index));
    memset(dscp, 0, sizeof(dscp));
    
    if (copy_from_user(cmd, buffer, count)) {
        return -EFAULT;
    }
    cmd[count] = '\0';

    sscanf(cmd,"%s %s %s", action, index, dscp);

    DPRINTK("action=%s\n", action);
    DPRINTK("index=%s\n", index);
    DPRINTK("dscp=%s\n", dscp);

    if( strncmp(action, "A", sizeof(action)) == 0 ) { /*Add*/
        entry = kmalloc(sizeof(hotspot_DSCP_t), GFP_KERNEL);
		if(kstrtoint(index,10, &(entry->index))) {
            printk("Set hotspot index Error\n");
            kfree(entry);
            return -EFAULT;
        }
        //strcpy(entry->index, index);
        if(entry)
        {
	        if(kstrtoint(dscp,10, &(entry->dscp))) {
	            printk("Set hotspot dscp Error\n");
	            kfree(entry);
	            return -EFAULT;
	        }
        }
        INIT_LIST_HEAD(&(entry->list));

        spin_lock_irqsave(&sc_gre_tx_filter_lock, sc_gre_tx_filter_lock_flag); /*lock*/
        list_add_tail(&(entry->list), &(hotspot_dscp_listhead.list)); /*modify list*/
        spin_unlock_irqrestore(&sc_gre_tx_filter_lock, sc_gre_tx_filter_lock_flag); /*unlock*/
    }
    else if (strncmp( action, "D", sizeof(action)) == 0 ) { /*Del*/
        spin_lock_irqsave(&sc_gre_tx_filter_lock, sc_gre_tx_filter_lock_flag); /*lock*/
        list_for_each_entry_safe(entry, temp_entry, &(hotspot_dscp_listhead.list), list) {
//            if( strncmp(index, entry->index, sizeof(index))==0 ) { /*match, del it*/
		unsigned int cmp_index = simple_strtoul(index,NULL,10);
		if(cmp_index == entry->index){
                list_del(&entry->list);
                kfree(entry);
            }
        }
        spin_unlock_irqrestore(&sc_gre_tx_filter_lock, sc_gre_tx_filter_lock_flag); /*unlock*/
    }
    else {
        printk("command not correct\n");
        return -EFAULT;
    }

    return count;
}

static int __init init(void)
{
    INIT_LIST_HEAD(&(hotspot_dscp_listhead.list));

    /*hook function*/
    sc_gre_tx_filter_hook = gre_dscp_handler_main;

    /*proc*/
    hotspot_dscp_proc = create_proc_entry("hotspot_dscp" ,0666, NULL);
    hotspot_dscp_proc->read_proc = hotspot_dscp_proc_read;
    hotspot_dscp_proc->write_proc = hotspot_dscp_proc_write;

    return 0;
}

static void __exit fini(void)
{
    hotspot_DSCP_t* entry;
    hotspot_DSCP_t* temp_entry;

    sc_gre_tx_filter_hook = NULL;
    
    /*remove proc*/
    if(sc_gre_tx_filter_hook) {
        remove_proc_entry("hotspot_dscp", NULL);
    }

    /*free list*/
    spin_lock_irqsave(&sc_gre_tx_filter_lock, sc_gre_tx_filter_lock_flag); /*lock*/
    list_for_each_entry_safe(entry, temp_entry, &(hotspot_dscp_listhead.list), list) {
        list_del(&entry->list);
        kfree(entry);
    }
    spin_unlock_irqrestore(&sc_gre_tx_filter_lock, sc_gre_tx_filter_lock_flag); /*unlock*/
}

module_init(init);
module_exit(fini);


