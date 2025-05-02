/*

Copyright (c) 2014 ARRIS Enterprises, LLC

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

#define EXPORT_SYMTAB

#if defined(CONFIG_MODVERSIONS) && !defined(MODVERSIONS)
#define MODEVERSIONS
#endif

#ifdef MODVERSIONS
#include <linux/modversions.h>
#endif
#include <linux/module.h>
#include <linux/init.h>
#include <linux/netdevice.h>
#include <linux/kernel.h>
#include <linux/skbuff.h>
#include <linux/in.h>
#include <linux/ip.h>
#include <linux/udp.h>
#include <linux/tcp.h>
#include <linux/moduleparam.h>
#include <linux/netfilter.h>
#include <net/netfilter/ipv4/nf_conntrack_ipv4.h>
#include <linux/inetdevice.h>
#include <net/dst.h>
#include <net/arp.h>
#include <linux/spinlock.h>

MODULE_LICENSE("Dual BSD/GPL");

/* 
 * This module is designed to handle daylightsaving proc
 * */
extern int (*daylightsaving_hook)(void);
static struct proc_dir_entry *proc_dst_diff = NULL;
static int g_dst_diff = 0;


//#define DEBUG
#ifdef DEBUG
#define PRINTK(format,argument...) printk(format,##argument)
#else
#define PRINTK(format,argument...)
#endif

static int daylightsaving_handler(void)
{
	return g_dst_diff; 
}

static int dst_diff_write(struct file *fp, const char *buf, unsigned long count,void *data)
{
	char local_buf[8];

	memset(local_buf, 0x00, sizeof(local_buf));

	if (count < 8)
	{
		copy_from_user(local_buf, buf, count);
		sscanf(local_buf, "%d", &g_dst_diff);
	}

	return count;
}

static int dst_diff_read(char *buf, char **start, off_t offset, int count, int *eof, void *data)
{
	int len = 0;

	len += sprintf(buf+len, "Daylightsaving=%d\n", g_dst_diff);

	return len;
}

static int __init sc_daylightsaving_init_module(void)
{
	proc_dst_diff = create_proc_entry("sc_dst_diff", 0666, NULL);
	if(proc_dst_diff) 
	{
		proc_dst_diff->read_proc = dst_diff_read;
		proc_dst_diff->write_proc = dst_diff_write;
	}

	daylightsaving_hook = daylightsaving_handler;
	
	PRINTK("sc_daylightsaving_init_module module init\n");

	return 0;
}

static void __exit sc_daylightsaving_cleanup_module(void)
{
	if(proc_dst_diff)
    	{
		remove_proc_entry("sc_dst_diff", NULL);
    	}

   	daylightsaving_hook = NULL;

	PRINTK("sc_daylightsaving_init_module module Remove\n");
}

module_init(sc_daylightsaving_init_module);
module_exit(sc_daylightsaving_cleanup_module);
