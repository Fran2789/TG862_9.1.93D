/*

Copyright (c) 2013 ARRIS Enterprises, LLC

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

MODULE_LICENSE("Dual BSD/GPL");

//#define SC_HOTSPOT_VLAN_PRI_HOOK_DEBUG
#ifdef SC_HOTSPOT_VLAN_PRI_HOOK_DEBUG
#define DPRINTK(format, argument...) printk(format,##argument)
#else
#define DPRINTK(format, argument...)
#endif

typedef struct hotspot_vlan_pri_s {
    char forward_if[IFNAMSIZ];
    unsigned int pri;
    struct list_head list; /* kernel's list structure */
} hotspot_vlan_pri_t;

static hotspot_vlan_pri_t hotspot_vlan_pri_listhead;
static struct proc_dir_entry *hotspot_vlan_pri_proc;

/*for spin lock*/
static DEFINE_SPINLOCK(hotspot_vlan_pri_lock);
static unsigned long hotspot_vlan_pri_lock_flag;

extern int (*sc_hotspot_vlan_pri_hook)(struct net_device *dev, struct sk_buff *skb);

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

static void gre_vlan_pri_handler(struct net_device *dev, struct sk_buff *skb)
{
    hotspot_vlan_pri_t* entry;
    
    spin_lock_irqsave(&hotspot_vlan_pri_lock, hotspot_vlan_pri_lock_flag);
    list_for_each_entry(entry, &(hotspot_vlan_pri_listhead.list), list) {
        if( dev != NULL && strcmp(dev->name, entry->forward_if) == 0)
        {
            skb->ti_meta_info = entry->pri;
        }
    }
    spin_unlock_irqrestore(&hotspot_vlan_pri_lock, hotspot_vlan_pri_lock_flag);
}

static int hotspot_vlan_pri_proc_read(char* page, char **start, off_t offset, int count, int *eof, void *data)
{
    int limit = count;
    char* buf = page + offset;
    int len=0;
    hotspot_vlan_pri_t* entry;
    len += snprintf(buf+len, limit-len, "Usage:\n");
    len += snprintf(buf+len, limit-len, "    Add: A <forward interface> <priority>\n");
    len += snprintf(buf+len, limit-len, "    Del: D <forward interface>\n");
    len += snprintf(buf+len, limit-len, "\n");
    len += snprintf(buf+len, limit-len, "Current settings:\n");
    len += snprintf(buf+len, limit-len, " <forward interface>  <priority>\n");

    spin_lock_irqsave(&hotspot_vlan_pri_lock, hotspot_vlan_pri_lock_flag);
    list_for_each_entry(entry, &(hotspot_vlan_pri_listhead.list), list) {
        len += snprintf(buf+len, limit-len, " %18.18s  %d\n", entry->forward_if, entry->pri);
    }
    spin_unlock_irqrestore(&hotspot_vlan_pri_lock, hotspot_vlan_pri_lock_flag);

    return len;

}

static int hotspot_vlan_pri_proc_write(struct file *file, const char *buffer, unsigned long count, void *data)
{
    char cmd[128];
    char action[128], forward_if[128], pri[128];
    hotspot_vlan_pri_t* entry;
    hotspot_vlan_pri_t* temp_entry;

    if (count > (sizeof(cmd) - 1)) {
        return -EFAULT;
    }

    memset(cmd, 0, sizeof(cmd));
    memset(action, 0, sizeof(action));
    memset(forward_if, 0, sizeof(forward_if));
    memset(pri, 0, sizeof(pri));
    
    if (copy_from_user(cmd, buffer, count)) {
        return -EFAULT;
    }
    cmd[count] = '\0';

    sscanf(cmd,"%s %s %s", action, forward_if, pri);

    DPRINTK("action=%s\n", action);
    DPRINTK("forward_if=%s\n", forward_if);
    DPRINTK("pri=%s\n", pri);

    if( strncmp(action, "A", sizeof(action)) == 0 ) { /*Add*/
        entry = kmalloc(sizeof(hotspot_vlan_pri_t), GFP_KERNEL);
        strcpy(entry->forward_if, forward_if);
        if(kstrtoint(pri,10, &(entry->pri))) {
            printk("Set hotspot vlan pri Error\n");
            kfree(entry);
            return -EFAULT;
        }
        INIT_LIST_HEAD(&(entry->list));

        spin_lock_irqsave(&hotspot_vlan_pri_lock, hotspot_vlan_pri_lock_flag); /*lock*/
        list_add_tail(&(entry->list), &(hotspot_vlan_pri_listhead.list)); /*modify list*/
        spin_unlock_irqrestore(&hotspot_vlan_pri_lock, hotspot_vlan_pri_lock_flag); /*unlock*/
    }
    else if (strncmp( action, "D", sizeof(action)) == 0 ) { /*Del*/
        spin_lock_irqsave(&hotspot_vlan_pri_lock, hotspot_vlan_pri_lock_flag); /*lock*/
        list_for_each_entry_safe(entry, temp_entry, &(hotspot_vlan_pri_listhead.list), list) {
            if( strncmp(forward_if, entry->forward_if, sizeof(forward_if))==0 ) { /*match, del it*/
                list_del(&entry->list);
                kfree(entry);
            }
        }
        spin_unlock_irqrestore(&hotspot_vlan_pri_lock, hotspot_vlan_pri_lock_flag); /*unlock*/
    }
    else {
        printk("command not correct\n");
        return -EFAULT;
    }

    return count;
}

static int __init init(void)
{
    /*hook function*/
    sc_hotspot_vlan_pri_hook = gre_vlan_pri_handler;

    /*proc*/
    INIT_LIST_HEAD(&(hotspot_vlan_pri_listhead.list));    
    hotspot_vlan_pri_proc = create_proc_entry("hotspot_vlan_pri" ,0666, NULL);
    hotspot_vlan_pri_proc->read_proc = hotspot_vlan_pri_proc_read;
    hotspot_vlan_pri_proc->write_proc = hotspot_vlan_pri_proc_write;

    return 0;
}

static void __exit fini(void)
{
    hotspot_vlan_pri_t* entry;
    hotspot_vlan_pri_t* temp_entry;

    sc_hotspot_vlan_pri_hook = NULL;
    
    /*remove proc*/
    if(hotspot_vlan_pri_proc) {
        remove_proc_entry("hotspot_vlan_pri", NULL);
    }

    /*free list*/
    spin_lock_irqsave(&hotspot_vlan_pri_lock, hotspot_vlan_pri_lock_flag); /*lock*/
    list_for_each_entry_safe(entry, temp_entry, &(hotspot_vlan_pri_listhead.list), list) {
        list_del(&entry->list);
        kfree(entry);
    }
    spin_unlock_irqrestore(&hotspot_vlan_pri_lock, hotspot_vlan_pri_lock_flag); /*unlock*/
}

module_init(init);
module_exit(fini);

