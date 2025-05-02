/*
 *
 * dbridge_ctl.c
 * Description:
 * DOCSIS bridge control implementation
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

#define _DBRIDGE_CTL_C_

/*! \file dbridge_ctl.c
    \brief the docsis bridge control module
*/

/**************************************************************************/
/*      INCLUDES:                                                         */
/**************************************************************************/

#include "dbridge_main.h"
#include "dbridge_db.h"
#include "dbridge_esafe.h"
#include "dbridge_netdev.h"
#include "dbridge_mcast.h"
#include "dbridge_igmp.h"
#include "dbridge_mdf_ctl.h"
#include "dbridge_mdf_db.h"
#include "dbridge_hal_filter.h"
#include "dbridge_l2vpn_ds_ctl.h"
#include "dbridge_l2vpn_ds_db.h"
#include "dbridge_mac_ageing.h"
#include "dbridge_common.h"
#include "dbridge_hal_filter.h"		// ARRIS ADD

#include <linux/kernel.h>
#include <linux/proc_fs.h>
#include <linux/module.h>
#include <linux/ctype.h>

/**************************************************************************/
/*      EXTERNS Declaration:                                              */
/**************************************************************************/

extern int Dbridge_interconnectReceive(struct sk_buff *skb, unsigned int fc_flags);

extern struct net_device * lbr0Dev;

/**************************************************************************/
/*      DEFINES:                                                          */
/**************************************************************************/

/**************************************************************************/
/*! \fn #define TOKENIZE(__sep, __token, __next_token, __err_msg)
 **************************************************************************
 *  \brief Macro to Tokenize a string
 *  \param[in] __token: Name of token var, string
 *  \param[in] __sep: String with separator chars
 *  \param[in] __next_token: Name of var for processing of next token, string
 *  \return Nothing
 **************************************************************************/
#define TOKENIZE(__token, __sep, __next_token) \
    do \
    { \
        /* Beginning of token  */ \
        __token = __next_token; \
        /* Skip sep */ \
        __token += strspn(__token, __sep); \
        /* Find end of token */ \
        __next_token = __token + strcspn(__token, __sep); \
        if (__token == __next_token) \
        { \
            __token = NULL; \
        } \
        /* Terminate token */ \
        *__next_token = '\0'; \
        /* Next token */ \
        __next_token++; \
    } while (0)

/* Same as before, except on ERR, prints _err_msg and returns -EFAULT */
#define TOKENIZE_ERR(__token, __sep, __next_token, __err_msg) \
    do \
    { \
        TOKENIZE(__token, __sep, __next_token); \
        if (__token == NULL) \
        { \
            printk(KERN_ERR __err_msg "\n"); \
            return -EFAULT; \
        } \
    } while (0)

/**************************************************************************/
/*! \fn #define CMPISTR(__s1, __s2)
 **************************************************************************
 *  \brief Macro to Compare (insensitive) strings
 *  \param[in] __s1, __s2: Strings
 *  \return True/False
 **************************************************************************/
#define CMPISTR(__s1, __s2) ((__s1 != NULL) && (__s2 != NULL) && (strlen(__s1) == strlen(__s2)) && (strnicmp(__s1, __s2, strlen(__s1)) == 0))

struct net_device * mtaDev =0;

unsigned int ipv4WanInterface_enable =1;
unsigned int ipv6WanInterface_enable =1;
unsigned int ipv4LanInterface_enable =1;
unsigned int ipv6LanInterface_enable =1;

#ifdef DBRIDGE_LOG
unsigned int dbridgeDbgLevel = 0;
#endif
/**************************************************************************/
/*      LOCAL DECLARATIONS:                                               */
/**************************************************************************/

/**************************************************************************/
/*      LOCAL VARIABLES:                                                  */
/**************************************************************************/
struct proc_dir_entry *dbrctl = NULL;
static struct proc_dir_entry *dbrctl_init = NULL;
static struct proc_dir_entry *dbrctl_addcpe = NULL;
static struct proc_dir_entry *dbrctl_delalt = NULL;
static struct proc_dir_entry *dbrctl_addesafemac2alt = NULL;
static struct proc_dir_entry *dbrctl_learncmmac = NULL;
static struct proc_dir_entry *dbrctl_maxcpe = NULL;
static struct proc_dir_entry *dbrctl_maxcpe_bypass = NULL; /* ARRIS ADD */
static struct proc_dir_entry *dbrctl_unused_hwfilters = NULL; /* ARRIS ADD */
static struct proc_dir_entry *dbrctl_l2vpnprommode = NULL; /* ARRIS ADD */
static struct proc_dir_entry *dbrctl_cmtsmac = NULL;    // ARRIS ADD 
#ifdef CONFIG_MACH_PUMA6
static struct proc_dir_entry *dbrctl_atommac = NULL;    // ARRIS ADD 
#endif
#ifdef CONFIG_VENDOR_ARRIS_GATEWAY
static struct proc_dir_entry *dbrctl_deletecpe = NULL; /* ARRIS ADD */
static struct proc_dir_entry *dbrctl_deleteinterfacecpe = NULL; /* ARRIS ADD */
static struct proc_dir_entry *dbrctl_erouter_selective_fwd = NULL; /* ARRIS ADD */
#endif
static struct proc_dir_entry *dbrctl_tftp = NULL;	/* ARRIS ADD */
static struct proc_dir_entry *dbrctl_usb = NULL;	/* ARRIS ADD */
static struct proc_dir_entry *dbrctl_mode = NULL;
static struct proc_dir_entry *dbrctl_mcastmac = NULL;
static struct proc_dir_entry *dbrctl_ipv4WanInterface_enable = NULL;
static struct proc_dir_entry *dbrctl_ipv6WanInterface_enable = NULL;
static struct proc_dir_entry *dbrctl_ipv4LanInterface_enable = NULL;
static struct proc_dir_entry *dbrctl_ipv6LanInterface_enable = NULL;
static struct proc_dir_entry *dbrctl_erouter_mode = NULL;
static struct proc_dir_entry *dbrctl_setLbrIf_mask = NULL;


#ifdef DBRIDGE_LOG
static struct proc_dir_entry *dbrctl_debuge_level = NULL;
static struct proc_dir_entry *dbrctl_test = NULL;
#endif
#ifdef DSG
static struct proc_dir_entry *dbrctl_dsg_tunnel = NULL;
static struct proc_dir_entry *dbrctl_dsg_estbip = NULL;
static struct proc_dir_entry *dbrctl_dsg_mode = NULL;
static struct proc_dir_entry *dbrctl_dsg_device = NULL;
#endif

static Bool tftpAllowed;	/* ARRIS ADD */
static Bool usbEnabled;		/* ARRIS ADD */

/**************************************************************************/
/*      INTERFACE FUNCTIONS Implementation:                               */
/**************************************************************************/

/**************************************************************************/
/*      LOCAL FUNCTIONS:                                                  */
/**************************************************************************/


/* write proc function */
// ARRIS ADD Start
int dbrctl_write_cmtsmac(struct file *fp, const char * buf, unsigned long count, void * data);
int dbrctl_read_cmtsmac(char* page, char **start, off_t offset, int count,int *eof, void *data);
#ifdef CONFIG_MACH_PUMA6
int dbrctl_write_atommac(struct file *fp, const char * buf, unsigned long count, void * data);
int dbrctl_read_atommac(char* page, char **start, off_t offset, int count,int *eof, void *data);
#endif
// ARRIS ADD End

int dbrctl_write_init(struct file *fp, const char * buf, unsigned long count, void * data);
int dbrctl_write_addcpe(struct file *fp, const char * buf, unsigned long count, void * data);
int dbrctl_write_delalt(struct file *fp, const char * buf, unsigned long count, void * data);
int dbrctl_write_addesafemac2alt(struct file *fp, const char * buf, unsigned long count, void * data);
int dbrctl_write_learncmmac(struct file *fp, const char * buf, unsigned long count, void * data);
int dbrctl_write_cmcioperntfy(struct file *fp, const char * buf, unsigned long count, void * data);
int dbrctl_write_macageingconf(struct file *fp, const char * buf, unsigned long count, void * data);
int dbrctl_write_maxcpe(struct file *fp, const char * buf, unsigned long count, void * data);
int dbrctl_write_maxcpe_bypass(struct file *fp, const char * buf, unsigned long count, void * data); /* ARRIS ADD */
int dbrctl_write_l2vpnprommode(struct file *fp, const char * buf, unsigned long count, void * data); /* ARRIS ADD */
int dbrctl_write_tftp(struct file *fp, const char * buf, unsigned long count, void * data); /* ARRIS ADD */
int dbrctl_write_usb(struct file *fp, const char * buf, unsigned long count, void * data); /* ARRIS ADD */
#ifdef CONFIG_VENDOR_ARRIS_GATEWAY
int dbrctl_write_deletecpe(struct file *fp, const char * buf, unsigned long count, void * data); /* ARRIS ADD */
int dbrctl_write_deleteinterfacecpe(struct file *fp, const char * buf, unsigned long count, void * data); /* ARRIS ADD */
int dbrctl_read_erouter_selective_fwd(char* page, char **start, off_t offset, int count,int *eof, void *data); /* ARRIS ADD */
int dbrctl_write_erouter_selective_fwd(struct file *fp, const char * buf, unsigned long count, void * data); /* ARRIS ADD */
#endif

int dbrctl_write_mode(struct file *fp, const char * buf, unsigned long count, void * data);
int dbrctl_write_mcastmac(struct file *fp, const char * buf, unsigned long count, void * data);
int dbrctl_write_ipv4WanInterface_enable(struct file *fp, const char * buf, unsigned long count, void * data);
int dbrctl_write_ipv6WanInterface_enable(struct file *fp, const char * buf, unsigned long count, void * data);
int dbrctl_write_ipv4LanInterface_enable(struct file *fp, const char * buf, unsigned long count, void * data);
int dbrctl_write_ipv6LanInterface_enable(struct file *fp, const char * buf, unsigned long count, void * data);
int dbrctl_write_erouter_mode(struct file *fp, const char * buf, unsigned long count, void * data);
int dbrctl_write_setLbrIf_mask(struct file *fp, const char * buf, unsigned long count, void * data);

#ifdef DBRIDGE_LOG
int dbrctl_write_debuge_level(struct file *fp, const char * buf, unsigned long count, void * data);
int dbrctl_write_test(struct file *fp, const char * buf, unsigned long count, void * data);
#endif
#ifdef DSG
int dbrctl_write_dsgaddress(struct file *fp, const char * buf, unsigned long count, void * data);
int dbrctl_read_dsgaddress(char* page, char **start, off_t offset, int count,int *eof, void *data);
int dbrctl_write_dsgestbip(struct file *fp, const char * buf, unsigned long count, void * data);
int dbrctl_read_dsgestbip(char* page, char **start, off_t offset, int count,int *eof, void *data);
int dbrctl_read_dsgmode(char* page, char **start, off_t offset, int count,int *eof, void *data);
int dbrctl_write_dsgmode(struct file *fp, const char * buf, unsigned long count, void * data);
int dbrctl_read_dsgdevice(char* page, char **start, off_t offset, int count,int *eof, void *data);
int dbrctl_write_dsgdevice(struct file *fp, const char * buf, unsigned long count, void * data);
int dbrctl_read_esafe(char* page, char **start, off_t offset, int count,int *eof, void *data);
#endif
/* read proc function */
int dbrctl_read_show(char* page, char **start, off_t offset, int count,int *eof, void *data);
int dbrctl_read_alt(char* page, char **start, off_t offset, int count,int *eof, void *data);
int dbrctl_read_cpe(char* page, char **start, off_t offset, int count,int *eof, void *data);
int dbrctl_read_counters(char* page, char **start, off_t offset, int count,int *eof, void *data);
int dbrctl_read_maxcpe(char* page, char **start, off_t offset, int count,int *eof, void *data);
int dbrctl_read_maxcpe_bypass(char* page, char **start, off_t offset, int count,int *eof, void *data); /* ARRIS ADD */
int dbrctl_read_unused_hwfilters(char* page, char **start, off_t offset, int count,int *eof, void *data); /* ARRIS ADD */
int dbrctl_read_l2vpnprommode(char* page, char **start, off_t offset, int count,int *eof, void *data); /* ARRIS ADD */
int dbrctl_read_tftp(char* page, char **start, off_t offset, int count,int *eof, void *data); /* ARRIS ADD */
int dbrctl_read_usb(char* page, char **start, off_t offset, int count,int *eof, void *data); /* ARRIS ADD */
int dbrctl_read_mode(char* page, char **start, off_t offset, int count,int *eof, void *data);
int dbrctl_read_help(char* page, char **start, off_t offset, int count,int *eof, void *data);
int dbrctl_read_num_of_esafe(char* page, char **start, off_t offset, int count,int *eof, void *data);
int dbrctl_read_mdfmode(char* page, char **start, off_t offset, int count,int *eof, void *data);
int dbrctl_read_ipv4WanInterface_enable(char* page, char **start, off_t offset, int count,int *eof, void *data);
int dbrctl_read_ipv6WanInterface_enable(char* page, char **start, off_t offset, int count,int *eof, void *data);
int dbrctl_read_ipv4LanInterface_enable(char* page, char **start, off_t offset, int count,int *eof, void *data);
int dbrctl_read_ipv6LanInterface_enable(char* page, char **start, off_t offset, int count,int *eof, void *data);
int dbrctl_read_erouter_mode(char* page, char **start, off_t offset, int count,int *eof, void *data);
int dbrctl_read_setLbrIf_mask(char* page, char **start, off_t offset, int count,int *eof, void *data);

#ifdef DBRIDGE_LOG
int dbrctl_read_debuge_level(char* page, char **start, off_t offset, int count,int *eof, void *data);
int dbrctl_read_test(char* page, char **start, off_t offset, int count,int *eof, void *data);
#endif

static int stringToMacArray(unsigned char* str, unsigned char* mac_array);
static unsigned char HexToDec(unsigned char c);

/**************************************************************************/
/*! \fn int __init dbridgeCtl_init(void)
 **************************************************************************
 *  \brief DOCSIS bridge control module initialization.
 *  \param[in] no input.
 *  \param[out] no output.
 *  \return OK or error status.
 **************************************************************************/
int __init dbridgeCtl_init(void)
{

    /* create base directory for dbrctl */
    dbrctl = proc_mkdir("dbrctl", init_net.proc_net);
       if (!dbrctl) {
            pr_err("Unable to proc dir entry\n");
            remove_proc_entry("dbrctl", init_net.proc_net);
            return -ENOMEM;
       }

    /* create init proc */
    dbrctl_init = create_proc_entry("init", 0644,dbrctl);
    if (dbrctl_init)
    {
        dbrctl_init->read_proc  = NULL;
        dbrctl_init->write_proc = dbrctl_write_init;
    }

    /* create addcpe proc */
    dbrctl_addcpe = create_proc_entry("addcpe", 0644,dbrctl);
    if (dbrctl_addcpe)
    {
        dbrctl_addcpe->read_proc  = NULL;
        dbrctl_addcpe->write_proc = dbrctl_write_addcpe;
    }

    /* create delcpe proc */
    dbrctl_delalt = create_proc_entry("delalt", 0644,dbrctl);
    if (dbrctl_delalt)
    {
        dbrctl_delalt->read_proc  = NULL;
        dbrctl_delalt->write_proc = dbrctl_write_delalt;
    }

    // ARRIS ADD BEGIN
#ifdef CONFIG_VENDOR_ARRIS_GATEWAY
	/* create deletecpe proc */
	dbrctl_deletecpe = create_proc_entry("deletecpe", 0644,dbrctl);
	if (dbrctl_deletecpe)
	{
		dbrctl_deletecpe->read_proc  = NULL;
		dbrctl_deletecpe->write_proc = dbrctl_write_deletecpe;
	}

	/* create deletecpe proc */
	dbrctl_deleteinterfacecpe = create_proc_entry("deleteinterfacecpe", 0644,dbrctl);
	if (dbrctl_deleteinterfacecpe)
	{
		dbrctl_deleteinterfacecpe->read_proc  = NULL;
		dbrctl_deleteinterfacecpe->write_proc = dbrctl_write_deleteinterfacecpe;
	}

    /* create erouer_selective_fwd proc entry */
    dbrctl_erouter_selective_fwd = create_proc_entry("erouter_selective_fwd", 0644, dbrctl);
    if (dbrctl_erouter_selective_fwd)
    {
        dbrctl_erouter_selective_fwd->read_proc = dbrctl_read_erouter_selective_fwd;
        dbrctl_erouter_selective_fwd->write_proc = dbrctl_write_erouter_selective_fwd;
    }
#endif    

	/* ARRIS create tftp/usb proc */
	dbrctl_tftp = create_proc_entry("tftp", 0644,dbrctl);
	if (dbrctl_tftp)
	{
		dbrctl_tftp->read_proc  = dbrctl_read_tftp;
		dbrctl_tftp->write_proc = dbrctl_write_tftp;
	}
	tftpAllowed = 0;

	dbrctl_usb = create_proc_entry("usb", 0644,dbrctl);
	if (dbrctl_usb)
	{
		dbrctl_usb->read_proc  = dbrctl_read_usb;
		dbrctl_usb->write_proc = dbrctl_write_usb;
	}
	usbEnabled = 1;
	// END ARRIS
    
    /* create lbrctl proc */
    dbrctl_addesafemac2alt = create_proc_entry("addesafemac2alt", 0644,dbrctl);
    if (dbrctl_addesafemac2alt)
    {
        dbrctl_addesafemac2alt->read_proc  = NULL;
        dbrctl_addesafemac2alt->write_proc = dbrctl_write_addesafemac2alt;
    }

    /* create learncmmac proc */
    dbrctl_learncmmac = create_proc_entry("learncmmac", 0644,dbrctl);
    if (dbrctl_learncmmac)
    {
        dbrctl_learncmmac->read_proc  = NULL;
        dbrctl_learncmmac->write_proc = dbrctl_write_learncmmac;
    }

    /* create cmcioperntfy proc */
    dbrctl_learncmmac = create_proc_entry("cmcioperntfy", 0644,dbrctl);
    if (dbrctl_learncmmac)
    {
        dbrctl_learncmmac->read_proc  = NULL;
        dbrctl_learncmmac->write_proc = dbrctl_write_cmcioperntfy;
    }

    /* create macageingconf proc */
    dbrctl_learncmmac = create_proc_entry("macageingconf", 0644,dbrctl);
    if (dbrctl_learncmmac)
    {
        dbrctl_learncmmac->read_proc  = NULL;
        dbrctl_learncmmac->write_proc = dbrctl_write_macageingconf;
    }

    /* create maxcpe proc */
    dbrctl_maxcpe = create_proc_entry("maxcpe", 0644,dbrctl);
    if (dbrctl_maxcpe)
    {
        dbrctl_maxcpe->read_proc  = dbrctl_read_maxcpe;
        dbrctl_maxcpe->write_proc = dbrctl_write_maxcpe;
    }

    /* ARRIS ADD START */
	/* create maxcpe_bypass proc */
	dbrctl_maxcpe_bypass = create_proc_entry("maxcpe_bypass", 0644,dbrctl);
	if (dbrctl_maxcpe_bypass)
	{
		dbrctl_maxcpe_bypass->read_proc  = dbrctl_read_maxcpe_bypass;
		dbrctl_maxcpe_bypass->write_proc = dbrctl_write_maxcpe_bypass;
	}
    /* ARRIS ADD END */

	/* ARRIS ADD START */
	/* create unused_hwfilters proc */
	dbrctl_unused_hwfilters = create_proc_entry("unused_hwfilters", 0644,dbrctl);
	if (dbrctl_unused_hwfilters)
	{
		dbrctl_unused_hwfilters->read_proc  = dbrctl_read_unused_hwfilters;
		dbrctl_unused_hwfilters->write_proc = NULL;
	}

	/* create l2vpnprommode proc */
	dbrctl_l2vpnprommode = create_proc_entry("l2vpnprommode", 0644,dbrctl);
	if (dbrctl_l2vpnprommode)
	{
		dbrctl_l2vpnprommode->read_proc  = dbrctl_read_l2vpnprommode;
		dbrctl_l2vpnprommode->write_proc = dbrctl_write_l2vpnprommode;
	}
    /* create cmtsmac proc */
    dbrctl_cmtsmac = create_proc_entry("cmtsmac", 0644,dbrctl);
    if (dbrctl_cmtsmac)
    {
        dbrctl_cmtsmac->read_proc  = dbrctl_read_cmtsmac;
        dbrctl_cmtsmac->write_proc = dbrctl_write_cmtsmac;
    }

#ifdef CONFIG_MACH_PUMA6
    /* create atommac proc */
    dbrctl_atommac = create_proc_entry("atommac", 0644,dbrctl);
    if (dbrctl_atommac)
    {
        dbrctl_atommac->read_proc  = dbrctl_read_atommac;
        dbrctl_atommac->write_proc = dbrctl_write_atommac;
    }
#endif    
	/* ARRIS ADD END */

    /* create setmode proc */
    dbrctl_mode = create_proc_entry("mode", 0644,dbrctl);
    if (dbrctl_mode)
    {
        dbrctl_mode->read_proc  = dbrctl_read_mode;
        dbrctl_mode->write_proc = dbrctl_write_mode;
    }

    /* create static mcast mac address proc */
    dbrctl_mcastmac = create_proc_entry("mcastmac", 0644,dbrctl);
    if (dbrctl_mcastmac)
    {
        dbrctl_mcastmac->read_proc  = dbrctl_read_mcastmac;
        dbrctl_mcastmac->write_proc = dbrctl_write_mcastmac;
    }


    /* create ipv4waninterface_enable proc */
    dbrctl_ipv4WanInterface_enable = create_proc_entry("ipv4waninterface_enable", 0644,dbrctl);
    if (dbrctl_ipv4WanInterface_enable)
    {
        dbrctl_ipv4WanInterface_enable->read_proc  = dbrctl_read_ipv4WanInterface_enable;
        dbrctl_ipv4WanInterface_enable->write_proc = dbrctl_write_ipv4WanInterface_enable;
    }

        /* create ipv6waninterface_enable proc */
    dbrctl_ipv6WanInterface_enable = create_proc_entry("ipv6waninterface_enable", 0644,dbrctl);
    if (dbrctl_ipv6WanInterface_enable)
    {
        dbrctl_ipv6WanInterface_enable->read_proc  = dbrctl_read_ipv6WanInterface_enable;
        dbrctl_ipv6WanInterface_enable->write_proc = dbrctl_write_ipv6WanInterface_enable;
    }

    /* create ipv4laninterface_enable proc */
    dbrctl_ipv4LanInterface_enable = create_proc_entry("ipv4laninterface_enable", 0644,dbrctl);
    if (dbrctl_ipv4LanInterface_enable)
    {
        dbrctl_ipv4LanInterface_enable->read_proc  = dbrctl_read_ipv4LanInterface_enable;
        dbrctl_ipv4LanInterface_enable->write_proc = dbrctl_write_ipv4LanInterface_enable;
    }

    /* create ipv6laninterface_enable proc */
    dbrctl_ipv6LanInterface_enable = create_proc_entry("ipv6laninterface_enable", 0644,dbrctl);
    if (dbrctl_ipv6LanInterface_enable)
    {
        dbrctl_ipv6LanInterface_enable->read_proc  = dbrctl_read_ipv6LanInterface_enable;
        dbrctl_ipv6LanInterface_enable->write_proc = dbrctl_write_ipv6LanInterface_enable;
    }


    /* create dbrctl_erouter_mode proc */
    dbrctl_erouter_mode = create_proc_entry("erouter_mode", 0644,dbrctl);
    if (dbrctl_erouter_mode)
    {
        dbrctl_erouter_mode->read_proc  = dbrctl_read_erouter_mode;
        dbrctl_erouter_mode->write_proc = dbrctl_write_erouter_mode;
    }

    /* create setlbrIf_mask proc */
    dbrctl_setLbrIf_mask = create_proc_entry("setlbrIf_mask", 0644,dbrctl);
    if (dbrctl_setLbrIf_mask)
    {
        dbrctl_setLbrIf_mask->read_proc  = dbrctl_read_setLbrIf_mask;
        dbrctl_setLbrIf_mask->write_proc = dbrctl_write_setLbrIf_mask;
    }

#ifdef DBRIDGE_LOG
    /* create debuge level proc */
    dbrctl_debuge_level = create_proc_entry("dbglevel", 0644,dbrctl);
    if (dbrctl_debuge_level)
    {
        dbrctl_debuge_level->read_proc  = dbrctl_read_debuge_level;
        dbrctl_debuge_level->write_proc = dbrctl_write_debuge_level;
    }
    dbrctl_test = create_proc_entry("test", 0644,dbrctl);
    if (dbrctl_test)
    {
        dbrctl_test->read_proc  = dbrctl_read_test;
        dbrctl_test->write_proc = dbrctl_write_test;
    }
#endif
#ifdef DSG
    /* create dsg tunnel proc */
    dbrctl_dsg_tunnel = create_proc_entry("dsgtunnel", 0644,dbrctl);
    if (dbrctl_dsg_tunnel)
    {
        dbrctl_dsg_tunnel->read_proc  = dbrctl_read_dsgaddress;
        dbrctl_dsg_tunnel->write_proc = dbrctl_write_dsgaddress;
    }

    /* create dsg estb ip proc */
    dbrctl_dsg_estbip = create_proc_entry("dsgestbip", 0644,dbrctl);
    if (dbrctl_dsg_estbip)
    {
        dbrctl_dsg_estbip->read_proc  = dbrctl_read_dsgestbip;
        dbrctl_dsg_estbip->write_proc = dbrctl_write_dsgestbip;
    }

    /* create dsg loob mode proc */
    dbrctl_dsg_mode = create_proc_entry("dsgmode", 0644,dbrctl);
    if (dbrctl_dsg_mode)
    {
        dbrctl_dsg_mode->read_proc  = dbrctl_read_dsgmode;
        dbrctl_dsg_mode->write_proc = dbrctl_write_dsgmode;
    }
	/* create dsg device proc */
	dbrctl_dsg_device = create_proc_entry("dsgdevice", 0644,dbrctl);
    if (dbrctl_dsg_device)
    {
        dbrctl_dsg_device->read_proc  = dbrctl_read_dsgdevice;
        dbrctl_dsg_device->write_proc = dbrctl_write_dsgdevice;
    }
    create_proc_read_entry("esafe",0,dbrctl,dbrctl_read_esafe,dbrctl);
#endif

    /* create read only proc */
    create_proc_read_entry("show",0,dbrctl,dbrctl_read_show,dbrctl);
    create_proc_read_entry("alt",0,dbrctl,dbrctl_read_alt,dbrctl);
    create_proc_read_entry("cpe",0,dbrctl,dbrctl_read_cpe,dbrctl);
    create_proc_read_entry("counters",0,dbrctl,dbrctl_read_counters,dbrctl);
    create_proc_read_entry("help",0,dbrctl,dbrctl_read_help,dbrctl);
    create_proc_read_entry("igmpdbg",0,dbrctl,dbrctl_read_igmp_dbg,dbrctl);
    create_proc_read_entry("igmpif",0,dbrctl,dbrctl_read_igmp_if_mib,dbrctl);
    create_proc_read_entry("igmpcache",0,dbrctl,dbrctl_read_igmp_cache_mib,dbrctl);
    create_proc_read_entry("numofesafe",0,dbrctl,dbrctl_read_num_of_esafe,dbrctl);
    create_proc_read_entry("mdfmode",0,dbrctl,dbrctl_read_mdfmode,dbrctl);







    /* DOCSIS bridge MDF ctl initialization */
    DbridgeMdfCtl_Init();

    /* DOCSIS bridge L2VPN DS ctl initialization */
    DbridgeL2vpnDsCtl_Init();

    return 0;
}

/**************************************************************************/
/*! \fn static void  __exit dbridgeCtl_exit(void)
 **************************************************************************
 *  \brief DOCSIS bridge control module exit.
 *  \param[in] no input.
 *  \param[out] no output.
 *  \return None.
 **************************************************************************/
static void  __exit dbridgeCtl_exit(void)
{


       if (dbrctl != NULL) {
            remove_proc_entry("mdfmode", dbrctl);
            remove_proc_entry("numofesafe", dbrctl);
            remove_proc_entry("igmpcache", dbrctl);
            remove_proc_entry("igmpif", dbrctl);
            remove_proc_entry("igmpdbg", dbrctl);
            remove_proc_entry("help", dbrctl);
            remove_proc_entry("counters", dbrctl);
            remove_proc_entry("cpe", dbrctl);
            remove_proc_entry("alt", dbrctl);
            remove_proc_entry("show", dbrctl);
#ifdef DBRIDGE_LOG
            if (dbrctl_test)
            {
                dbrctl_test = NULL;
                remove_proc_entry("test", dbrctl);
            }
            if (dbrctl_debuge_level)
            {
                dbrctl_debuge_level = NULL;
                remove_proc_entry("dbglevel", dbrctl);
            }
#endif
            if (dbrctl_erouter_mode)
            {
                dbrctl_erouter_mode = NULL;
                remove_proc_entry("erouter_mode", dbrctl);
            }

            if (dbrctl_setLbrIf_mask)
            {
                dbrctl_setLbrIf_mask = NULL;
                remove_proc_entry("setlbrIf_mask", dbrctl);
            }

            if (dbrctl_ipv6LanInterface_enable)
            {
                dbrctl_ipv6LanInterface_enable=NULL;
                remove_proc_entry("ipv6laninterface_enable", dbrctl);
            }
            if (dbrctl_ipv4LanInterface_enable)
            {
                dbrctl_ipv4LanInterface_enable = NULL;
                remove_proc_entry("ipv4laninterface_enable", dbrctl);
            }
            if (dbrctl_ipv6WanInterface_enable)
            {
                dbrctl_ipv6WanInterface_enable = NULL;
                remove_proc_entry("ipv6waninterface_enable", dbrctl);
            }
            if (dbrctl_ipv4WanInterface_enable)
            {
                dbrctl_ipv4WanInterface_enable = NULL;
                remove_proc_entry("ipv4waninterface_enable", dbrctl);
            }
            if (dbrctl_mcastmac)
            {
                dbrctl_mcastmac = NULL;
                remove_proc_entry("mcastmac", dbrctl);
            }
            if (dbrctl_mode)
            {
                dbrctl_mode = NULL;
                remove_proc_entry("mode", dbrctl);
            }
            if (dbrctl_maxcpe)
            {
                dbrctl_maxcpe = NULL;
                remove_proc_entry("maxcpe", dbrctl);
            }
            if (dbrctl_learncmmac)
            {
                dbrctl_learncmmac = NULL;
                remove_proc_entry("learncmmac", dbrctl);
            }
            if (dbrctl_addesafemac2alt)
            {
                dbrctl_addesafemac2alt = NULL;
                remove_proc_entry("addesafemac2alt", dbrctl);
            }
            if (dbrctl_delalt)
            {
                dbrctl_delalt = NULL;
                remove_proc_entry("dbrctl_delalt", dbrctl);
            }
            if (dbrctl_addcpe)
            {
                dbrctl_addcpe = NULL;
                remove_proc_entry("addcpe", dbrctl);
            }
            if (dbrctl_init)
            {
                dbrctl_init = NULL;
                remove_proc_entry("init", dbrctl);
            }
            dbrctl = NULL;
            remove_proc_entry("dbrctl", init_net.proc_net);
        }

    /* DOCSIS bridge MDF ctl exit */
    DbridgeMdfCtl_Exit();

    /* DOCSIS bridge L2VPN DS ctl exit */
    DbridgeL2vpnDsCtl_Exit();
}


/**************************************************************************/
/*! \fn int dbrctl_read_show(char* page, char **start, off_t offset, int count,int *eof, void *data)
 **************************************************************************
 *  \brief proc file to get the docsis bridge configuration
 **************************************************************************/
int dbrctl_read_show(char* page, char **start, off_t offset, int count,int *eof, void *data)
{
    int len = 0;
    int index = 0;
    int num_of_device = 0;
    DbridgeDevice_t*   ptr_dbridge_dev;
    int dbridge_mode;
    int erouter_mode;

    dbridge_mode= DbridgeDb_GetMode();
    num_of_device=DbridgeDb_GetNumOfDevice();
    erouter_mode=DbridgeDb_Erouter_GetMode();

    len+= sprintf(page+len, "\n Docsis bridge mode is: %d  (0=off; 1=reg; 2=pre reg; 3=standby)",dbridge_mode );
    len+= sprintf(page+len, "\n --------------------------------------------------------------");
    len+= sprintf(page+len, "\n Docsis bridge erouter mode is: %d  (0=disable; 1=ebable)",erouter_mode );
    len+= sprintf(page+len, "\n Docsis bridge lbridge_interface_mask: 0x%llX",DbridgeDB_GetlbridgeInterfaceMask() );
    len+= sprintf(page+len, "\n --------------------------------------------------------------");
    if (num_of_device)
    {
        len+= sprintf(page+len, "\n index\tInterface\tType");
        for (index = 0; index < num_of_device; index++)
        {
            ptr_dbridge_dev = DbridgeDb_DevGetByIndex(index);
            if (ptr_dbridge_dev)
            {
                switch (ptr_dbridge_dev->net_dev_type)
                {
                case DBR_CMCI_NET_DEV_TYPE:
                    len+= sprintf(page+len, "\n %d\t%s\t\tCMCI",index,ptr_dbridge_dev->ptr_interface->name);
                    break;

                case DBR_CABLE_NET_DEV_TYPE:
                    len+= sprintf(page+len, "\n %d\t%s\t\tCABLE",index,ptr_dbridge_dev->ptr_interface->name);
                    break;
                case DBR_ESAFE_NET_DEV_TYPE:
                    len+= sprintf(page+len, "\n %d\t%s\t\tESAFE",index,ptr_dbridge_dev->ptr_interface->name);
                    break;
                case DBR_WAN_IP_NET_DEV_TYPE:
                    len+= sprintf(page+len, "\n %d\t%s\t\tWAN IP",index,ptr_dbridge_dev->ptr_interface->name);
                    break;
                case DBR_LAN_IP_NET_DEV_TYPE:
                    len+= sprintf(page+len, "\n %d\t%s\t\tLAN IP",index,ptr_dbridge_dev->ptr_interface->name);
                    break;
                default:
                    len+= sprintf(page+len, "\n %d\t%s\t\tUNKNOWN",index,ptr_dbridge_dev->ptr_interface->name);

                }
            }
        }
    }

    len+= sprintf(page+len, "\n --------------------------------------------------------------");
    len+= sprintf(page+len, "\n");
    len+= sprintf(page+len, "\n Esafe configuration");
    len+= sprintf(page+len, "\n -------------------");

    len+= DbridgeEsafe_PrintEsafeCfg(page+len);

    len+= sprintf(page+len, "\n");
    *eof = 1;
    return len ;

}

/**************************************************************************/
/*! \fn int dbrctl_read_alt(char* page, char **start, off_t offset, int count,int *eof, void *data)
 **************************************************************************
 *  \brief proc file to get the adress lookup table
 **************************************************************************/

int dbrctl_read_alt(char* page, char **start, off_t offset, int count,int *eof, void *data)
{
    off_t pos = 0;
    off_t begin = 0;
    int len = 0;
    DbridgeAltEntry_t* ptr_dbridge_alt;
    unsigned int max_cpe = DbridgeDB_GetMaxCpe();
    int firstAltEntry = 1;
    int index = 0;
        unsigned int max_cpe_bypass = DbridgeDB_GetMaxCpeBypass(); /* ARRIS ADD */

    len+= sprintf(page+len, "\nAddress Lookup Table");
    len+= sprintf(page+len, "\n--------------------");
    len+= sprintf(page+len, "\nMax CPEs\t%d",max_cpe);
	len+= sprintf(page+len, "\nCPE Bypass\t%d",max_cpe_bypass); /* ARRIS ADD */
    len+= sprintf(page+len, "\n--------------------");

    DBRALT_FOREACH_ALT_ENTRY(ptr_dbridge_alt)
    {
        if (firstAltEntry)
        {
            firstAltEntry = 0;
            len+= sprintf(page+len, "\nIndex\tMac\t\t\tName\tLearning\tSource\tCpe num");
            len+= sprintf(page+len, "\n-----\t---\t\t\t----\t--------\t------\t-------");
        }

        len+= sprintf(page+len, "\n%d\t%.2x:%.2x:%.2x:%.2x:%.2x:%.2x",index,
                      ptr_dbridge_alt->macAddress[0],ptr_dbridge_alt->macAddress[1],
                      ptr_dbridge_alt->macAddress[2],ptr_dbridge_alt->macAddress[3],
                      ptr_dbridge_alt->macAddress[4],ptr_dbridge_alt->macAddress[5]);

        if(ptr_dbridge_alt->dbridgeDevice)
            len+= sprintf(page+len, "\t%s",ptr_dbridge_alt->dbridgeDevice->ptr_interface->name);
        else
            len+= sprintf(page+len, "\tNULL");


        switch(ptr_dbridge_alt->macLearningFrom)
        {
        case DBRIDGE_INIT:
            len+= sprintf(page+len, "\tInit");
            break;
        case CPE_STATIC:
            len+= sprintf(page+len, "\tStatic");
            break;
        case CPE_DYNAMIC:
            len+= sprintf(page+len, "\tDynamic");
            break;
        default:
            len+= sprintf(page+len, "\tUnknown");
        }

        if(ptr_dbridge_alt->sourceInterface)
            len+= sprintf(page+len, "\t\t%s",ptr_dbridge_alt->sourceInterface->name);
        else
            len+= sprintf(page+len, "\t\tNULL");
        len+= sprintf(page+len, "\t%d",ptr_dbridge_alt->cpe_number);

        pos = begin + len;
        if (pos < offset) {
            len = 0;
            begin = pos;
        }
        if(pos > offset + count)
            goto done;

        index++;
    }


    len+= sprintf(page+len, "\n");

done:

    *start = page + (offset - begin);
    len -= (offset - begin);

    if (len > count)
        len = count;
    if (len < 0)
        len = 0;

    return len;

}


/**************************************************************************/
/*! \fn int dbrctl_read_cpe(char* page, char **start, off_t offset, int count,int *eof, void *data)
 **************************************************************************
 *  \brief proc file to get the CPEs mac adress list
 **************************************************************************/

int dbrctl_read_cpe(char* page, char **start, off_t offset, int count,int *eof, void *data)
{
    off_t pos = 0;
    off_t begin = 0;
    int len = 0;
    DbridgeAltEntry_t* ptr_dbridge_alt;
    unsigned int max_cpe = DbridgeDB_GetMaxCpe();
        unsigned int max_cpe_bypass = DbridgeDB_GetMaxCpeBypass(); /* ARRIS ADD */
    int firstAltEntry = 1;

    len+= sprintf(page+len, "\nCPEs List           ");
    len+= sprintf(page+len, "\n--------------------");
    len+= sprintf(page+len, "\nMax CPEs\t%d",max_cpe);
	len+= sprintf(page+len, "\nCPE Bypass\t%d",max_cpe_bypass); /* ARRIS ADD */
    len+= sprintf(page+len, "\n--------------------");

    DBRALT_FOREACH_ALT_ENTRY(ptr_dbridge_alt)
    {
        if (firstAltEntry)
        {
            firstAltEntry = 0;
#ifdef CONFIG_VENDOR_ARRIS_GATEWAY  // ARRIS ADD
		len+= sprintf(page+len, "\n\tMac\t\t\tName\tLearning\tSource\tCpe Num\tGwIf");
		len+= sprintf(page+len, "\n\t---\t\t\t----\t--------\t------\t-------\t-------");
#else        
            len+= sprintf(page+len, "\n\tMac\t\t\tName\tLearning\tSource\tCpe Num");
            len+= sprintf(page+len, "\n\t---\t\t\t----\t--------\t------\t-------");
#endif  // ARRIS ADD        
        }

        if ((ptr_dbridge_alt->macLearningFrom == CPE_STATIC)||(ptr_dbridge_alt->macLearningFrom == CPE_DYNAMIC))
        {

            len+= sprintf(page+len, "\n->\t%.2x:%.2x:%.2x:%.2x:%.2x:%.2x",
                          ptr_dbridge_alt->macAddress[0],ptr_dbridge_alt->macAddress[1],
                          ptr_dbridge_alt->macAddress[2],ptr_dbridge_alt->macAddress[3],
                          ptr_dbridge_alt->macAddress[4],ptr_dbridge_alt->macAddress[5]);

            if (ptr_dbridge_alt->dbridgeDevice)
                len+= sprintf(page+len, "\t%s",ptr_dbridge_alt->dbridgeDevice->ptr_interface->name);
            else
                len+= sprintf(page+len, "\tNULL");


            switch (ptr_dbridge_alt->macLearningFrom)
            {
            case CPE_STATIC:
                len+= sprintf(page+len, "\tStatic");
                break;
            case CPE_DYNAMIC:
                len+= sprintf(page+len, "\tDynamic");
                break;
            default:
                len+= sprintf(page+len, "\tUnknown");
            }

            if (ptr_dbridge_alt->sourceInterface)
                len+= sprintf(page+len, "\t\t%s",ptr_dbridge_alt->sourceInterface->name);
            else
                len+= sprintf(page+len, "\t\tNULL");
            len+= sprintf(page+len, "\t%d",ptr_dbridge_alt->cpe_number);

#ifdef CONFIG_VENDOR_ARRIS_GATEWAY  // ARRIS ADD
                len+= sprintf(page+len, "\t0x%x",ptr_dbridge_alt->gwInterfaceInfo);
#endif
        }

        pos = begin + len;
        if (pos < offset)
        {
            len = 0;
            begin = pos;
        }
        if (pos > offset + count)
            goto done;
    }


    len+= sprintf(page+len, "\n");

    done:

    *start = page + (offset - begin);
    len -= (offset - begin);

    if (len > count)
        len = count;
    if (len < 0)
        len = 0;

    return len;

}

/**************************************************************************/
/*! \fn int dbrctl_read_counters(char* page, char **start, off_t offset, int count,int *eof, void *data)
 **************************************************************************
 *  \brief proc file to get docsis bridge counters
 **************************************************************************/

int dbrctl_read_counters(char* page, char **start, off_t offset, int count,int *eof, void *data)
{
    int len=0;
    int index=0;
    DbridgeCounters_t*   ptr_dbridge_counters;
    struct net_device*  ptr_interface;
    ptr_dbridge_counters= Dbridge_GetCounters();

    len+= sprintf(page+len, "\nDocsis bridge counters");
    len+= sprintf(page+len, "\n----------------------");
    len+= sprintf(page+len, "\nStatic Cpe\t%u",ptr_dbridge_counters->StaticCpe);
    len+= sprintf(page+len, "\nDynamic Cpe\t%u",ptr_dbridge_counters->DynamicCpe);
    len+= sprintf(page+len, "\n----------------------");
    len+= sprintf(page+len, "\nIndex\tName\tIn\tOut\tDiscard\tUnSup\tFltDrop\tFwrdDrop\tInUni\tOutUni\tInMcast\tOutMcast\tInBcast\tOutBcast");
    len+= sprintf(page+len, "\n-----\t----\t--\t---\t-------\t-----\t-------\t--------\t-----\t------\t-------\t--------\t-------\t--------");

    for (index=1; index <= DBR_MAX_ALL_INTERFACE ; index++ )
    {
        ptr_interface = dev_get_by_index(&init_net, index);
        if (ptr_interface)
        {
            len+= sprintf(page+len, "\n%d\t%s\t%u\t%u\t%u\t%u\t%u\t%u\t\t%llu\t%llu\t%llu\t%llu\t\t%llu\t%llu",
                          index,
                          ptr_interface->name,
                          ptr_dbridge_counters->DevCounters[index - 1].In,
                          ptr_dbridge_counters->DevCounters[index - 1].Out,
                          ptr_dbridge_counters->DevCounters[index - 1].Discard,
                          ptr_dbridge_counters->DevCounters[index - 1].UnSupport,
                          ptr_dbridge_counters->DevCounters[index - 1].FilterDrop,
                          ptr_dbridge_counters->DevCounters[index - 1].FwrdRuleDrop,
                          ptr_dbridge_counters->DevCounters[index - 1].inUcast,
                          ptr_dbridge_counters->DevCounters[index - 1].outUcast,
                          ptr_dbridge_counters->DevCounters[index - 1].inMcast,
                          ptr_dbridge_counters->DevCounters[index - 1].outMcast,
                          ptr_dbridge_counters->DevCounters[index - 1].inBcast,
                          ptr_dbridge_counters->DevCounters[index - 1].outBcast
                          );

            /* Release the device */
            dev_put(ptr_interface);
        }

    }

    len+= sprintf(page+len, "\n");
    *eof = 1;
    return len;
}

/**************************************************************************/
/*! \fn int dbrctl_read_help(char* page, char **start, off_t offset, int count,int *eof, void *data)
 **************************************************************************
 *  \brief help for all docsis bridge proc
 **************************************************************************/
int dbrctl_read_help(char* page, char **start, off_t offset, int count,int *eof, void *data)
{
    int len=0;

    len+= sprintf(page+len,"\n =======================================================================");
    len+= sprintf(page+len,"\n * init: initialize docsis bridge (Databese and interfaces             *");
    len+= sprintf(page+len,"\n * / # echo 1 > /proc/net/dbrctl/init                                  *");
    len+= sprintf(page+len,"\n =======================================================================");
    len+= sprintf(page+len,"\n * maxcpe: set maximum supported CPEs(from 0 to 64)                    *");
    len+= sprintf(page+len,"\n * / # echo <num of cpe> > /proc/net/dbrctl/maxcpe                     *");
    len+= sprintf(page+len,"\n =======================================================================");
    len+= sprintf(page+len,"\n * addcpe: add cpe mac address to ALT table                            *");
    len+= sprintf(page+len,"\n *         MAC address is mandatory                                    *");
    len+= sprintf(page+len,"\n *         Other parameters are OPTIONAL                               *");
    len+= sprintf(page+len,"\n *         Other parameters are FOR DEBUG ONLY !!!                     *");
    len+= sprintf(page+len,"\n *         static|dynamic|init, if absent - static                     *");
    len+= sprintf(page+len,"\n *         CMCI|CABLE|ESAFE|WAN_IP|LAN_IP, if absent - NULL            *");
    len+= sprintf(page+len,"\n * / # echo  <MAC address> \\                                           *");
    len+= sprintf(page+len,"\n *           [static|dynamic  \\                                        *");
    len+= sprintf(page+len,"\n *             [CMCI|CABLE|ESAFE|WAN_IP|LAN_IP]] \\                     *");
    len+= sprintf(page+len,"\n *           > /proc/net/dbrctl/addcpe                                 *");
    len+= sprintf(page+len,"\n =======================================================================");
    len+= sprintf(page+len,"\n * addesafemac2alt: add eSafe mac address to ALT table                 *");
    len+= sprintf(page+len,"\n * / # echo  <device name> <MAC address> > /proc/net/dbrctl/addcpe     *");
    len+= sprintf(page+len,"\n =======================================================================");
    len+= sprintf(page+len,"\n * delalt: delete an entry from the ALT                                *");
    len+= sprintf(page+len,"\n * / # echo  match=ALL|ANY \\                                           *");
    len+= sprintf(page+len,"\n *           mac=<MAC address> \\                                       *");
    len+= sprintf(page+len,"\n *           learnfrom=DBRIDGE_INIT | CPE_STATIC | CPE_DYNAMIC         *");
    len+= sprintf(page+len,"\n *           destdev=<dest devname>\\                                   *");
    len+= sprintf(page+len,"\n *           src=<src devname>\\                                        *");
    len+= sprintf(page+len,"\n *           cpenum=<decimal number of cpe>\\                           *");
    len+= sprintf(page+len,"\n *           devtype=CMCI | CABLE | ESAFE | WAN_IP | LAN_IP\\           *");
    len+= sprintf(page+len,"\n *           > \\/proc/net/dbrctl/delalt                                *");
    len+= sprintf(page+len,"\n =======================================================================");
    len+= sprintf(page+len,"\n * learncmmac: dbride data base learn cm MAC address (wan0 or lan0)    *");
    len+= sprintf(page+len,"\n * / # echo  <device name> <mac address> > /proc/net/dbrctl/learncmmac *");
    len+= sprintf(page+len,"\n =======================================================================");
    len+= sprintf(page+len,"\n * setmode: set docsis bridge mode - off, registered pre_registration  *");
    len+= sprintf(page+len,"\n *          or naco_off                                                *");
    len+= sprintf(page+len,"\n * / # echo  <mode> > /proc/net/dbrctl/mode                            *");
    len+= sprintf(page+len,"\n =======================================================================\n");
    len+= sprintf(page+len,"\n * mcastmac: Add Static Multicast mac address                          *");
    len+= sprintf(page+len,"\n * / # echo  <MAC address> > /proc/net/dbrctl/mcastmac                 *");
    len+= sprintf(page+len,"\n =======================================================================\n");
    len+= sprintf(page+len,"\n * dbglevel: Set Docsis Bridge debug level                             *");
    len+= sprintf(page+len,"\n * / # echo  <MAC address> > /proc/net/dbrctl/mcastmac                 *");
    len+= sprintf(page+len,"\n =======================================================================\n");
    len+= sprintf(page+len,"\n * show: get the docsis bridge configuration                           *");
    len+= sprintf(page+len,"\n * / # cat /proc/net/dbrctl/show                                       *");
    len+= sprintf(page+len,"\n =======================================================================\n");
    len+= sprintf(page+len,"\n * alt: get the adress lookup table                                    *");
    len+= sprintf(page+len,"\n * / # cat /proc/net/dbrctl/alt                                        *");
    len+= sprintf(page+len,"\n =======================================================================\n");
    len+= sprintf(page+len,"\n * cpe: get the CPEs mac adress list                                   *");
    len+= sprintf(page+len,"\n * / # cat /proc/net/dbrctl/cpe                                        *");
    len+= sprintf(page+len,"\n =======================================================================\n");
    len+= sprintf(page+len,"\n * counters: get docsis bridge counters                                *");
    len+= sprintf(page+len,"\n * / # cat /proc/net/dbrctl/counters                                   *");
    len+= sprintf(page+len,"\n =======================================================================\n");
    len+= sprintf(page+len,"\n * mcastmac: get static multicast table                                *");
    len+= sprintf(page+len,"\n * / # cat /proc/net/dbrctl/mcastmac                                   *");
    len+= sprintf(page+len,"\n =======================================================================\n");
    len+= sprintf(page+len,"\n * dbglevel: get docsis bridge debug level and options                 *");
    len+= sprintf(page+len,"\n * / # cat /proc/net/dbrctl/dbglevel                                   *");
    len+= sprintf(page+len,"\n =======================================================================\n");
    len+= sprintf(page+len,"\n * mdfmode: get docsis bridge MDF mode                                 *");
    len+= sprintf(page+len,"\n * / # cat /proc/net/dbrctl/mdfmode                                    *");
    len+= sprintf(page+len,"\n =======================================================================\n");
    *eof = 1;
    return len;
}


/**************************************************************************/
/*! \fn int dbrctl_write_init(struct file *fp, const char * buf, unsigned long count, void * data)
 **************************************************************************
 *  \brief proc file for docsis bridge initialization
 **************************************************************************/

int dbrctl_write_init(struct file *fp, const char * buf, unsigned long count, void * data)
{
    unsigned char local_buf[10];
    int  ret_val = 0;


    if (count > 10)
    {
        printk(KERN_ERR "Buffer Overflow\n");
        return -EFAULT;
    }

    if(copy_from_user(local_buf,buf,count))
        return -EFAULT;;
    local_buf[count-1]='\0'; /* Ignoring last \n char */
    ret_val = count;


    if (strcmp("1",local_buf)==0)
    {

        if(Dbridge_Init())
			printk(KERN_ERR "Dbridge Init FAILED\n"); // ARRIS
        else
			printk(KERN_ERR "Dbridge Init SUCCESS\n" ); // ARRIS
    }

    return ret_val;
}

/**************************************************************************/
/*! \fn int dbrctl_write_addcpe(struct file *fp, const char * buf, unsigned long count, void * data)
 **************************************************************************
 *  \brief proc file to add static cpe to adress lookup table
 *  All parameters are case INSENSITIVE
 *  -   <mac address>
 *          MAC address is specified as 6 hex pairs separated by : or -.
 *          Mandatory
 *
 *  Following parameters are OPTIONAL and to be used
 *
 *          ONLY FOR DEBUG
 *          ONLY FOR DEBUG
 *          ONLY FOR DEBUG
 *
 *  -   static | dynamic | init
 *          Entry is marked as CPE_STATIC | CPE_DYNAMIC | DBRIDGE_INIT
 *          Optional, if absent - static
 *  -   CMCI | CABLE | ESAFE | WAN_IP | LAN_IP
 *      Select a Dbridge entry that matches the dev type
 *      Optional, if absent - NULL
 **************************************************************************/
int dbrctl_write_addcpe(struct file *fp, const char * buf, unsigned long count, void * data)
{
    unsigned char local_buf[40]; /* 17 MAC, 1 space, 7 static|dynamic, 1 space, 6 dev type, some spare (extra spaces, etc) */
    int  ret_val = 0;
    char *param;
    char *end_param;
    char *last_char;
    static const char *param_sep = " \t,\n";
    unsigned char macAddress[MAC_ADDRESS_LEN];
    Maclearning_e mac_learn_from = CPE_STATIC;
    DbridgeDevice_t *dbridge_Device = NULL;
    int net_dev_type;

    /* Leave room for a string terminator */
    if (count > (sizeof(local_buf) - 1))
    {
        printk(KERN_ERR "Buffer Overflow\n");
        return count;
    }
    else if (count <= 0)
    {
        printk(KERN_ERR "No input\n");
        return count;
    }

    if(copy_from_user(local_buf,buf,count))
        return count;

    ret_val = count;

    /* Terminate string */
    if (local_buf[count-1] == '\n')
    {
        /* Ignore last \n char, terminate over \n */
    }
    else
    {
        /* Terminate after last char. Checked above for enough space. */
        count++;
    }
    local_buf[count-1] = '\0'; /* Ignoring last \n char */

    last_char = &local_buf[count-1];

    end_param = local_buf;

    /* Get MAC param name */
    TOKENIZE(param, param_sep, end_param);
    if (param == NULL)
    {
        /* No MAC param */
        printk(KERN_ERR "Missing mandatory MAC parameter");
        return ret_val;
    }

    if (stringToMacArray(param, macAddress) != 0)
    {
        printk(KERN_ERR "Illegal MAC address: %s", param);
        return ret_val;
    }

    if (end_param < last_char)
    {
        /* Get STATIC/DYNAMIC */
        TOKENIZE(param, param_sep, end_param);
        if (param == NULL)
        {
            /* No param */
            mac_learn_from = CPE_STATIC;
        }
        else
        {
            if (CMPISTR("static", param))
            {
                mac_learn_from = CPE_STATIC;
            }
            else if (CMPISTR("dynamic", param))
            {
                mac_learn_from = CPE_DYNAMIC;
            }
            else if (CMPISTR("INIT", param))
            {
                mac_learn_from = DBRIDGE_INIT;
            }
            else
            {
                printk(KERN_ERR "Illegal LearnFrom: %s", param);
                return ret_val;
            }
        }
    }


    if (end_param < last_char)
    {
        /* dev type */
        TOKENIZE(param, param_sep, end_param);
        if (param == NULL)
        {
            /* No param */
            dbridge_Device = NULL;
        }
        else
        {
            if (CMPISTR("CMCI", param))
            {
                net_dev_type = DBR_CMCI_NET_DEV_TYPE;
            }
            else if (CMPISTR("CABLE", param))
            {
                net_dev_type = DBR_CABLE_NET_DEV_TYPE;
            }
            else if (CMPISTR("ESAFE", param))
            {
                net_dev_type = DBR_ESAFE_NET_DEV_TYPE;
            }
            else if (CMPISTR("WAN_IP", param))
            {
                net_dev_type = DBR_WAN_IP_NET_DEV_TYPE;
            }
            else if (CMPISTR("LAN_IP", param))
            {
                net_dev_type = DBR_LAN_IP_NET_DEV_TYPE;
            }
            else
            {
                printk(KERN_ERR "Illegal net dev type: \"%s\"\n", param);
                return count;
            }
            dbridge_Device = DbridgeDb_DevGetByType(net_dev_type);
        }
    }

#ifdef CONFIG_VENDOR_ARRIS_GATEWAY  // ARRIS ADD
        DbridgeDb_AltAddMac(macAddress, mac_learn_from, dbridge_Device, NULL,0);
#else        
    DbridgeDb_AltAddMac(macAddress, mac_learn_from, dbridge_Device, NULL);
#endif // ARRIS ADD END          

    return ret_val;
}

/**************************************************************************/
/*! \fn int dbrctl_write_delalt(struct file *fp, const char * buf, unsigned long count, void * data)
 **************************************************************************
 *  \brief proc file to delete an entry from the address lookup table
 *  Parameters provide a match criterion, to determine which CPE to delete.
 *  All CPEs that match will be deleted.
 *  Parameters have a fixed structure, according to the following.
 *  All parameters are case INSENSITIVE
 *  All following parameters have a "name=val" structure, no spaces are allowed around the =.
 *  The value may be missing. In this case, the parameter will not be considered for matching.
 *  Name/Val pairs are separated by space/tab/,
 *  -   match=ALL | ANY ::: If missing, the default behavior is ALL
 *          ALL: all supplied params will be used to determine which CPEs to delete
 *          ANY: Any single matched field will cause the matched CPE to be deleted
 *  -   mac=<mac address>
 *          MAC address is specified as 6 hex pairs separated by : or -.
 *  -   learnfrom=DBRIDGE_INIT | CPE_STATIC | CPE_DYNAMIC
 *          Initiator of the ALT entry
 *  -   detsdev=<devname>
 *          Name of destination device
 *  -   srcsdev=<devname>
 *          Name of source device
 *  -   cpenum=<decimal num>
 *          Number of CPE in the ALT
 *  -   devtype=CMCI | CABLE | ESAFE | WAN_IP | LAN_IP
 *          Device type
 *
 **************************************************************************/
int dbrctl_write_delalt(struct file *fp, const char * buf, unsigned long count, void * data)
{
    unsigned char local_buf[200];
    int  ret_val = 0;
    char *last_byte;
    char *param;
    char *end_param;
    char *param_name;
    char *end_param_name;
    char *param_val;
    static const char *param_sep = " \t,\n";
    static const char *val_sep = "=";
    int val_exist;
    unsigned char mac_address[MAC_ADDRESS_LEN] = {0};
    Maclearning_e mac_learn_from = 0;
    DbridgeDb_FieldMask_e field_mask;
    char *dest_dev = NULL;
    char *src_dev = NULL;
    int cpe_num = 0;
    int net_dev_type = 0;

#define MATCH_PARAM(__param_str, __param_mask) \
    if (CMPISTR(__param_str, param_name)) \
    { \
        field_mask |= __param_mask; \
        val_exist = 1; \
    } \
    else \
    { \
        val_exist = 0; \
    } \

#define PARSE_PARAM(__param_str, __param_mask) \
    do \
    { \
        val_exist = 0; \
        end_param_name = end_param; \
        if (last_byte < end_param) \
        { \
            TOKENIZE(param, param_sep, end_param); \
            if ((param != NULL) && (last_byte < end_param)) \
            { \
                TOKENIZE(param_name, val_sep, end_param_name); \
                if ((param_name != NULL) && (last_byte < end_param)) \
                { \
                    if (CMPISTR(__param_str, param_name)) \
                    { \
                        TOKENIZE(param_val, val_sep, end_param_name); \
                        if (param_val != NULL) \
                        { \
                            field_mask |= __param_mask; \
                            val_exist = 1; \
                        } \
                    } \
                    else \
                    { \
                        printk(KERN_ERR "Illegal param: \"%s\", expected \"" __param_str "\"\n", param_name); \
                        return count; \
                    } \
                } \
                else \
                { \
                    printk(KERN_ERR "Missing param value for param: \"%s\", expected \"" __param_str "\"\n", param_name); \
                    return count; \
                } \
            } \
        } \
    } while (0)

    /* Leave room for a string terminator */
    if (count > (sizeof(local_buf) - 1))
    {
        printk(KERN_ERR "Buffer Overflow\n");
        return count;
    }
    else if (count <= 0)
    {
        printk(KERN_ERR "No input\n");
        return count;
    }

    if(copy_from_user(local_buf,buf,count))
        return count;

    ret_val = count;

    /* Terminate string */
    if (local_buf[count-1] == '\n')
    {
        /* Ignore last \n char, terminate over \n */
    }
    else
    {
        /* Terminate after last char. Checked above for enough space. */
        count++;
    }
    local_buf[count-1] = '\0'; /* Ignoring last \n char */

    last_byte = &local_buf[count-1];

    /* Parse */
    end_param = local_buf;

    /* Default */
    field_mask = DBRALT_MASK_ALL;
    while (last_byte >= end_param)
    {
        /* Get param name */
        TOKENIZE(param, param_sep, end_param);
        if (param == NULL)
        {
            /* No more tokens */
            break;
        }

        /* Get param name */
        end_param_name = param;
        TOKENIZE(param_name, val_sep, end_param_name);
        if (param_name == NULL)
        {
            /* Shouldn't happen */
            printk(KERN_ERR "Could not find param name in %s\n", param);
            return count;
        }

        /* Get param val */
        TOKENIZE(param_val, val_sep, end_param_name);
        if (param_val == NULL)
        {
            printk(KERN_ERR "Missing param value for param: \"%s\"\n", param_name);
            return count;
        }

        /* Match */
        if (CMPISTR("match", param_name))
        {
            if (CMPISTR("ALL", param_val))
            {
                field_mask |= DBRALT_MASK_ALL;
                /* Remove other */
                field_mask &= ~DBRALT_MASK_ANY;
            }
            else if (CMPISTR("ANY", param_val))
            {
                field_mask |= DBRALT_MASK_ANY;
                /* Remove other */
                field_mask &= ~DBRALT_MASK_ALL;
            }
            else
            {
                printk(KERN_ERR "Illegal mask type: \"%s\", expected ALL | ANY\n", param_val);
                return count;
            }
            continue;
        }
        /* MAC */
        MATCH_PARAM("MAC", DBRALT_MASK_MAC);
        if (val_exist)
        {
            if (stringToMacArray(param_val, mac_address) == 0)
            {
                printk(KERN_ERR "Illegal MAC address %s\n", param_val);
                return count;
            }
            continue;
        }
        /* LearnFrom */
        MATCH_PARAM("LearnFrom", DBRALT_MASK_LEARNFROM);
        if (val_exist)
        {
            if (CMPISTR("DBRIDGE_INIT", param_val))
            {
                mac_learn_from = DBRIDGE_INIT;
            }
            else if (CMPISTR("CPE_STATIC", param_val))
            {
                mac_learn_from = CPE_STATIC;
            }
            else if (CMPISTR("CPE_DYNAMIC", param_val))
            {
                mac_learn_from = CPE_DYNAMIC;
            }
            else
            {
                printk(KERN_ERR "Illegal LearnFrom: \"%s\"\n", param_val);
                return count;
            }
            continue;
        }
        /* Dest dev */
        MATCH_PARAM("DestDev", DBRALT_MASK_DESTDEV);
        if (val_exist)
        {
            dest_dev = param_val;
            continue;
        }
        /* Src dev */
        MATCH_PARAM("SrcDev", DBRALT_MASK_SRCDEV);
        if (val_exist)
        {
            src_dev = param_val;
            continue;
        }
        /* CPE NUM */
        MATCH_PARAM("CPENUM", DBRALT_MASK_CPENUM);
        if (val_exist)
        {
            sscanf(param_val, "%d", &cpe_num);
            continue;
        }
        /* CPE NUM */
        MATCH_PARAM("DEVTYPE", DBRALT_MASK_DEVTYPE);
        if (val_exist)
        {
            if (CMPISTR("CMCI", param_val))
            {
                net_dev_type = DBR_CMCI_NET_DEV_TYPE;
            }
            else if (CMPISTR("CABLE", param_val))
            {
                net_dev_type = DBR_CABLE_NET_DEV_TYPE;
            }
            else if (CMPISTR("ESAFE", param_val))
            {
                net_dev_type = DBR_ESAFE_NET_DEV_TYPE;
            }
            else if (CMPISTR("WAN_IP", param_val))
            {
                net_dev_type = DBR_WAN_IP_NET_DEV_TYPE;
            }
            else if (CMPISTR("LAN_IP", param_val))
            {
                net_dev_type = DBR_LAN_IP_NET_DEV_TYPE;
            }
            else
            {
                printk(KERN_ERR "Illegal net dev type: \"%s\"\n", param_val);
                return count;
            }
            continue;
        }

        /* Unrecognized */
        printk(KERN_ERR "Illegal param_name: \"%s\"\n", param_name);
        return count;
    }

    /* It is not necessary to initialize unused vals since the mask marks them as unused */
    DbridgeDb_AltDelMac(mac_address, mac_learn_from, dest_dev, src_dev, cpe_num, net_dev_type, field_mask, NULL);

    return ret_val;

#undef MATCH_PARAM
#undef PARSE_PARAM
}

// ARRIS ADD
#ifdef CONFIG_VENDOR_ARRIS_GATEWAY
/*! \fn int dbrctl_write_deletecpe(struct file *fp, const char * buf, unsigned long count, void * data)
 **************************************************************************
 *  \brief proc file to delete a cpe from the table
 **************************************************************************/
int dbrctl_write_deletecpe(struct file *fp, const char * buf, unsigned long count, void * data)
{
    unsigned char local_buf[20];
    int  ret_val = 0;
    unsigned char macAddress[MAC_ADDRESS_LEN];

    if (count > 20)
    {
        printk(KERN_ERR "Buffer Overflow\n");
        return -EFAULT;
    }

    if(copy_from_user(local_buf,buf,count))
        return -EFAULT;;
    local_buf[count-1]='\0'; /* Ignoring last \n char */
    ret_val = count;
    //ARRIS ADD BEGIN
    stringToMacArray((unsigned char*)&local_buf,(unsigned char*)&macAddress);
    DbridgeDb_AltDelMac(macAddress, CPE_DYNAMIC,NULL,NULL,0,DBR_CMCI_NET_DEV_TYPE,(DBRALT_MASK_ANY | DBRALT_MASK_MAC), NULL );
    //ARRIS ADD END
    return ret_val;
}

/*! \fn int dbrctl_write_deleteinterfacecpe(struct file *fp, const char * buf, unsigned long count, void * data)
 **************************************************************************
 *  \brief proc file to delete all cpe from the table by interface
 **************************************************************************/
int dbrctl_write_deleteinterfacecpe(struct file *fp, const char * buf, unsigned long count, void * data)
{
    unsigned char local_buf[20];
    int  ret_val = 0;
    int interfaceNumber=0;

    if (count > 20)
    {
        printk(KERN_ERR "Buffer Overflow\n");
        return -EFAULT;
    }

    if(copy_from_user(local_buf,buf,count))
        return -EFAULT;;
    local_buf[count-1]='\0'; /* Ignoring last \n char */
    ret_val = count;

    sscanf(local_buf,"%x",&interfaceNumber);
    //printk(KERN_ERR "dbrctl_write_deleteinterfacecpe is called:0x%x\n", interfaceNumber);
    DbridgeDb_AltDelCpeByInterface(interfaceNumber);

    return ret_val;
}

/**************************************************************************/
/*! \fn int dbrctl_write_erouter_selective_fwd(struct file *fp, const char * buf, unsigned long count, void * data)
 **************************************************************************
 *  \brief proc file to set the erouter_selective_fwd
 **************************************************************************/
int dbrctl_write_erouter_selective_fwd(struct file *fp, const char * buf, unsigned long count, void * data)
{
    unsigned char local_buf[8] = {0};
    int  ret_val = 0;
    unsigned int mode;

    if (count > sizeof(local_buf))
    {
        printk(KERN_ERR "Buffer Overflow\n");
        return -EFAULT;
    }

    if(copy_from_user(local_buf, buf, count))
    {
        return -EFAULT;;
    }
    
    if (local_buf[count - 1] == '\n')
    {
        local_buf[count - 1] = '\0'; /* Ignoring last \n char */
    }
    
    ret_val = count;
    sscanf(local_buf,"%d",&mode);

    DbridgeDb_SetErouterSelectiveFwd(mode);
    return ret_val;

}

/**************************************************************************/
/*! \fn int dbrctl_read_erouter_selective_fwd(char* page, char **start, off_t offset, int count,int *eof, void *data)
 **************************************************************************
 *  \brief read erouter_selective
 **************************************************************************/
int dbrctl_read_erouter_selective_fwd(char* page, char **start, off_t offset, int count,int *eof, void *data)
{
    int len=0;
    unsigned int mode = DbridgeDb_GetErouterSelectiveFwd();
    len += sprintf(page + len, "\neRouter LBridge Selective Packet Handler Switch");
    len += sprintf(page + len, "\nChanging it is pointless while eRouter is disabled");
    len += sprintf(page + len, "\n");
    len += sprintf(page + len, "\n--------------------------------------------------");
    len += sprintf(page + len, "\n%s", mode == (unsigned int)-1 ?
        "N/A": (mode?"Enabled":"Disabled"));
    len += sprintf(page + len, "\n--------------------------------------------------\n\n");
    *eof = 1;
    return len;
}


#endif

// ARRIS ADD handling of lan tftp setting
int dbrctl_read_tftp(char* page, char **start, off_t offset, int count,int *eof, void *data)
{
    int len=0;

    if( tftpAllowed == 1 )
    {
        len += sprintf( page+len, "not blocked\n" );
    }
    else
    {
        len += sprintf( page+len, "blocked\n" );
    }
    *eof = 1;
    return len;
}

int dbrctl_write_tftp(struct file *fp, const char * buf, unsigned long count, void * data)
{
    unsigned char local_buf[5];

    if( count > 5 )
    {
        printk( KERN_ERR "buffer overflow\n" );
	    return -EFAULT;
    }

    if( copy_from_user(local_buf, buf, count) )
    {
        printk( KERN_ERR "copy from user failure\n" );
	    return -EFAULT;
    }
    
    if( local_buf[0] == '0' )
    {
        tftpAllowed = 0;
    }
    else if( local_buf[0] == '1' )
    {
        tftpAllowed = 1;
    }
    else
    {
        printk( KERN_ERR "invalid tftp value\n" );
	    return -EINVAL;
    }
    return count;
}

int dbrctl_read_usb(char* page, char **start, off_t offset, int count,int *eof, void *data)
{
    int len=0;

    if( usbEnabled == 1 )
    {
        len += sprintf( page+len, "enabled\n" );
    }
    else
    {
        len += sprintf( page+len, "disabled\n" );
    }
    *eof = 1;
    return len;
}

int dbrctl_write_usb(struct file *fp, const char * buf, unsigned long count, void * data)
{
    unsigned char local_buf[5];

    if( count > 5 )
    {
        printk( KERN_ERR "buffer overflow\n" );
	    return -EFAULT;
    }

    if( copy_from_user(local_buf, buf, count) )
    {
        printk( KERN_ERR "copy from user failure\n" );
	    return -EFAULT;
    }
    
    if( local_buf[0] == '0' )
    {
        usbEnabled = 0;
    }
    else if( local_buf[0] == '1' )
    {
        usbEnabled = 1;
    }
    else
    {
        printk( KERN_ERR "invalid usb value\n" );
	    return -EINVAL;
    }
    return count;
}

int DBridge_UsbEnabled( void )
{
    return usbEnabled;
}

int DBridge_TftpAllowed( void )
{
    return tftpAllowed;
}
// ARRIS ADD END

/**************************************************************************/
/*! \fn int dbrctl_write_learncmmac(struct file *fp, const char * buf, unsigned long count, void * data)
 **************************************************************************
 *  \brief proc file to add mac adress  to adress lookup table
 **************************************************************************/
int dbrctl_write_learncmmac(struct file *fp, const char * buf, unsigned long count, void * data)
{
    unsigned char local_buf[40];
    int  ret_val = 0;
    unsigned char macAddress[MAC_ADDRESS_LEN];
    unsigned char cmd0[20];
    unsigned char cmd1[20];

    if (count > 40)
    {
        printk(KERN_ERR "Buffer Overflow\n");
        return -EFAULT;
    }
    if(copy_from_user(local_buf,buf,count))
        return -EFAULT;;
    local_buf[count-1]='\0'; /* Ignoring last \n char */
    ret_val = count;

    sscanf(local_buf,"%s %s",cmd0,cmd1);

    if (stringToMacArray((unsigned char*)&cmd1,(unsigned char*)&macAddress)==0)
    {
       if(DbridgeDb_AltAddMacByDevName((unsigned char*)&macAddress,(unsigned char*)&cmd0)==0)
          printk(KERN_INFO "LEARN CM MAC: %.2x:%.2x:%.2x:%.2x:%.2x:%.2x for %s\n",(unsigned char)macAddress[0], (unsigned char)macAddress[1], (unsigned char)macAddress[2],
           (unsigned char)macAddress[3], (unsigned char)macAddress[4], (unsigned char)macAddress[5],cmd0);
       else
           printk(KERN_INFO "UNABLE TO LEARN CM MAC: %.2x:%.2x:%.2x:%.2x:%.2x:%.2x for %s\n",(unsigned char)macAddress[0], (unsigned char)macAddress[1], (unsigned char)macAddress[2],
            (unsigned char)macAddress[3], (unsigned char)macAddress[4], (unsigned char)macAddress[5],cmd0);
    }
    return ret_val;

}

/**************************************************************************/
/*! \fn int dbrctl_write_cmcioperntfy(struct file *fp, const char * buf, unsigned long count, void * data)
 **************************************************************************
 *  \brief proc file to notify on CMCI NI operation status change
 **************************************************************************/
int dbrctl_write_cmcioperntfy(struct file *fp, const char * buf, unsigned long count, void * data)
{
    unsigned char local_buf[40];
    int  ret_val = 0;
    unsigned char status[20];
    unsigned int docsisIfIndex;
    unsigned int sysIfIndex;

    if (count > sizeof(local_buf))
    {
        printk(KERN_ERR "Buffer Overflow\n");
        return -EFAULT;
    }
    if(copy_from_user(local_buf,buf,count))
        return -EFAULT;
    local_buf[count-1]='\0'; /* Ignoring last \n char */
    ret_val = count;

    sscanf(local_buf,"%d %s",&docsisIfIndex,status);

    if (DbridgeCommon_GetSysIfIndexByDocsisCB)
    {
        DbridgeCommon_GetSysIfIndexByDocsisCB(docsisIfIndex, &sysIfIndex);
        DbridgeMacAgeing_HandleEvent(strcmp("DOWN", status) ? True : False, sysIfIndex);
    }

    return ret_val;
}

/**************************************************************************/
/*! \fn int dbrctl_write_macageingconf(struct file *fp, const char * buf, unsigned long count, void * data)
 **************************************************************************
 *  \brief proc file to config mac ageing status
 **************************************************************************/
int dbrctl_write_macageingconf(struct file *fp, const char * buf, unsigned long count, void * data)
{
    unsigned char local_buf[40];
    int  ret_val = 0;
    unsigned char status[20];
    unsigned int timer_Value;

    if (count > sizeof(local_buf))
    {
        printk(KERN_ERR "Buffer Overflow\n");
        return -EFAULT;
    }
    if(copy_from_user(local_buf,buf,count))
        return -EFAULT;

    local_buf[count-1]='\0'; /* Ignoring last \n char */
    ret_val = count;

    sscanf(local_buf,"%s %d",status, &timer_Value);

    DbridgeMacAgeing_SetStatusAndTimer(strcmp("ENABLE", status) ? False : True, timer_Value);

    return ret_val;
}

/**************************************************************************/
/*! \fn int dbrctl_write_learncmmac(struct file *fp, const char * buf, unsigned long count, void * data)
 **************************************************************************
 *  \brief proc file to set the maximum  cpe allow
 **************************************************************************/
int dbrctl_write_maxcpe(struct file *fp, const char * buf, unsigned long count, void * data)
{
    unsigned char local_buf[10];
    int  ret_val = 0;
    unsigned int max_cpe;
        unsigned int max_cpe_bypass = DbridgeDB_GetMaxCpeBypass(); /* ARRIS ADD */

    if (count > 10)
    {
        printk(KERN_ERR "Buffer Overflow\n");
        return -EFAULT;
    }

    if(copy_from_user(local_buf,buf,count))
        return -EFAULT;;
    local_buf[count-1]='\0'; /* Ignoring last \n char */
    ret_val = count;
    sscanf(local_buf,"%d",&max_cpe);
        /* ARRIS CHANGE START */
        /* account for maxCpeBypass as well */
	if( (max_cpe + max_cpe_bypass) > DBR_MAX_CPE_ENTRY )
	{
            /* Set bypass to 0, since we are exceeding the limit */
            DbridgeDB_SetMaxCpeBypass(0);

    if (max_cpe > DBR_MAX_CPE_ENTRY)
    {
        max_cpe= 0; /* error code */
    }
	}
        /* ARRIS CHANGE END */
    DbridgeDB_SetMaxCpe(max_cpe);
    return ret_val;

}

/* ARRIS ADD START */
/**************************************************************************/
/*! \fn int dbrctl_write_maxcpe_bypass(struct file *fp, const char * buf, unsigned long count, void * data)
 **************************************************************************
 *  \brief proc file to set the maximum bypass cpe allowed
 **************************************************************************/
int dbrctl_write_maxcpe_bypass(struct file *fp, const char * buf, unsigned long count, void * data)
{
	unsigned char local_buf[10];
	int  ret_val = 0;
        unsigned int max_cpe_bypass;
        unsigned int max_cpe = DbridgeDB_GetMaxCpe();

	if (count > 10)
	{
		printk(KERN_ERR "Buffer Overflow\n");
		return -EFAULT;
	}

	if(copy_from_user(local_buf,buf,count))
		return -EFAULT;;
	local_buf[count-1]='\0'; /* Ignoring last \n char */
	ret_val = count;
	sscanf(local_buf,"%d",&max_cpe_bypass);

        /* make sure that the cpe bypass does not put us over the maximum number of CPEs allowed */
        if ( (max_cpe + max_cpe_bypass) <= DBR_MAX_CPE_ENTRY )
        {
            DbridgeDB_SetMaxCpeBypass(max_cpe_bypass);
        }

	return ret_val;

}

/**************************************************************************/
/*! \fn int dbrctl_write_l2vpnprommode(struct file *fp, const char * buf, unsigned long count, void * data)
 **************************************************************************
 *  \brief proc file to set the L2VPN Promiscuous Mode 
 **************************************************************************/
int dbrctl_write_l2vpnprommode(struct file *fp, const char * buf, unsigned long count, void * data)
{
    unsigned char local_buf[10];
    int  ret_val = 0;
    unsigned int l2vpn_prom_mode;

    if (count > 10)
    {
        printk(KERN_ERR "Buffer Overflow\n");
        return -EFAULT;
    }

    if(copy_from_user(local_buf,buf,count))
    {
        return -EFAULT;;
    }

    local_buf[count-1]='\0'; /* Ignoring last \n char */
    ret_val = count;
    sscanf(local_buf,"%d",&l2vpn_prom_mode);
    DbridgeDB_SetL2vpnPromMode(l2vpn_prom_mode);
    return ret_val;
}

/**************************************************************************/
/*! \fn int dbrctl_write_cmtsmac(struct file *fp, const char * buf, unsigned long count, void * data)
 **************************************************************************
 *  \brief proc file to set the cmtsmac
 **************************************************************************/
int dbrctl_write_cmtsmac(struct file *fp, const char * buf, unsigned long count, void * data)
{
    unsigned char local_buf[50];
    unsigned char macAddr[6];
    int  ret_val = 0;

    if (count > 50 )
    {
        printk(KERN_ERR "Buffer Overflow\n");
        return -EFAULT;
    }

    if(copy_from_user(local_buf,buf,count))
    {
        return -EFAULT;;
    }

    local_buf[count-1]='\0'; /* Ignoring last \n char */
    ret_val = count;

    sscanf( local_buf, "%hhx-%hhx-%hhx-%hhx-%hhx-%hhx", macAddr, macAddr+1, macAddr+2, macAddr+3, macAddr+4, macAddr+5 );

    /*
    printk( KERN_ERR "CMTSMAC: %.2x-%.2x-%.2x-%.2x-%.2x-%.2x", 
            (unsigned char)macAddr[0], (unsigned char)macAddr[1], (unsigned char)macAddr[2], 
            (unsigned char)macAddr[3], (unsigned char)macAddr[4], (unsigned char)macAddr[5] );
    */

    DbridgeDb_SetCmtsMacAddr(macAddr);
    return ret_val;
}

#ifdef CONFIG_MACH_PUMA6
/**************************************************************************/
/*! \fn int dbrctl_write_atommac(struct file *fp, const char * buf, unsigned long count, void * data)
 **************************************************************************
 *  \brief proc file to set the atommac
 **************************************************************************/
int dbrctl_write_atommac(struct file *fp, const char * buf, unsigned long count, void * data)
{
    unsigned char local_buf[50];
    unsigned char macAddr[6];
    int  ret_val = 0;

    if (count > 50 )
    {
        printk(KERN_ERR "Buffer Overflow\n");
        return -EFAULT;
    }

    if(copy_from_user(local_buf,buf,count))
    {
        return -EFAULT;;
    }

    local_buf[count-1]='\0'; /* Ignoring last \n char */
    ret_val = count;

    sscanf( local_buf, "%hhx-%hhx-%hhx-%hhx-%hhx-%hhx", macAddr, macAddr+1, macAddr+2, macAddr+3, macAddr+4, macAddr+5 );

    DbridgeDb_SetAtomMacAddr(macAddr);
    return ret_val;
}
#endif
/* ARRIS ADD END */

/**************************************************************************/
/**************************************************************************/
/*! \fn int dbrctl_write_learncmmac(struct file *fp, const char * buf, unsigned long count, void * data)
 **************************************************************************
 *  \brief proc file to set the docsis bridge mode
 **************************************************************************/
int dbrctl_write_mode(struct file *fp, const char * buf, unsigned long count, void * data)
{
    unsigned char local_buf[30];
    int  ret_val = 0;
    unsigned int sucsess=0;

    if (count > 30)
    {
        printk(KERN_ERR "Buffer Overflow\n");
        return -EFAULT;
    }
    if(copy_from_user(local_buf,buf,count))
        return -EFAULT;
    local_buf[count-1]='\0'; /* Ignoring last \n char */
    ret_val = count;
        sucsess = 1;

    if(strcmp("off",local_buf)==0)
    {
        DbridgeDb_SetMode(DBR_OFF_MODE);
    }
    else if(strcmp("registered",local_buf)==0)
    {
        DbridgeDb_SetMode(DBR_REGISTERED_MODE);
    }
    else if(strcmp("pre_registration",local_buf)==0)
    {
        DbridgeDb_SetMode(DBR_PRE_REGISTRATION_MODE);
    }
    else if(strcmp("naco_off",local_buf)==0)
    {
        DbridgeDb_SetMode(DBR_NACO_OFF_MODE);
    }
    else if(strcmp("standbay",local_buf)==0)
        {
       DbridgeDb_SetMode(DBR_STANDBY_MODE);
        }
        else
        sucsess=0;

    if(sucsess)
        printk(KERN_INFO "Docsis bridge mode set to %s\n",local_buf);
    else
        printk(KERN_INFO "Docsis bridge mode %s illegal\n",local_buf);
    return ret_val;
}

/**************************************************************************/
/*! \fn int dbrctl_read_maxcpe(char* page, char **start, off_t offset, int count,int *eof, void *data)
 **************************************************************************
 *  \brief read max cpe allow
 **************************************************************************/
int dbrctl_read_maxcpe(char* page, char **start, off_t offset, int count,int *eof, void *data)
{
    int len=0;
    unsigned int max_cpe = DbridgeDB_GetMaxCpe();

    len+= sprintf(page+len, "%d\n",max_cpe);
    *eof = 1;
    return len;

}

/**************************************************************************/
/*! \fn int dbrctl_read_mode(char* page, char **start, off_t offset, int count,int *eof, void *data)
 **************************************************************************
 *  \brief read docsis bridge mode
 **************************************************************************/

int dbrctl_read_mode(char* page, char **start, off_t offset, int count,int *eof, void *data)
{
    int len=0;
    unsigned int mode = DbridgeDb_GetMode();
    switch(mode)
        {
    case DBR_OFF_MODE:
        len+= sprintf(page+len, "off \n");
        break;
    case DBR_REGISTERED_MODE:
        len+= sprintf(page+len, "registered \n");
        break;
    case DBR_PRE_REGISTRATION_MODE:
        len+= sprintf(page+len, "pre_registration \n");
        break;
    case DBR_NACO_OFF_MODE:
        len+= sprintf(page+len, "naco_off \n");
        break;
    case DBR_STANDBY_MODE:
        len+= sprintf(page+len, "standbay \n");
        break;
    default:
        len+= sprintf(page+len, "Unknown mode\n");
    }
    *eof = 1;
    return len;

}
/* ARRIS ADD START */
/**************************************************************************/
/*! \fn int dbrctl_read_maxcpe_bypass(char* page, char **start, off_t offset, int count,int *eof, void *data)
 **************************************************************************
 *  \brief read max cpe bypass allowed
 **************************************************************************/
int dbrctl_read_maxcpe_bypass(char* page, char **start, off_t offset, int count,int *eof, void *data)
{
        int len=0;
        unsigned int max_cpe_bypass = DbridgeDB_GetMaxCpeBypass(); 
    
        len+= sprintf(page+len, "%d\n",max_cpe_bypass);
        *eof = 1;
        return len;
}

/**************************************************************************/
/*! \fn int dbrctl_read_unused_hwfilters(char* page, char **start, off_t offset, int count,int *eof, void *data)
 **************************************************************************
 *  \brief read unused hwfilters array
 **************************************************************************/
int dbrctl_read_unused_hwfilters(char* page, char **start, off_t offset, int count,int *eof, void *data)
{
    int len=0;
    unsigned int* unused_filters;
    unsigned int  num_unused_filters;
    unsigned int  index;
    unsigned int  hwFilter;
    unsigned char address[6];

    DbridgeDB_getHwFilters(&unused_filters, &num_unused_filters); 

    len+= sprintf(page+len, "Unused HW filters from oldest to newest\n\n");
    len+= sprintf(page+len, "Index | Address\n");
    len+= sprintf(page+len, "------| -------\n");

    for (index=0; index<num_unused_filters; index++)
    {
        hwFilter = *(unused_filters+index);
        HAL_MacDsUnicastDataFilterGetAtIndex(hwFilter, address);

        len+= sprintf(page+len, "  %02d  | %02x-%02x-%02x-%02x-%02x-%02x\n",hwFilter,
                      address[0], address[1], address[2],
                      address[3], address[4], address[5]);
    }

    *eof = 1;
    return len;
}

/**************************************************************************/
/*! \fn int dbrctl_read_l2vpnprommode(char* page, char **start, off_t offset, int count,int *eof, void *data)
 **************************************************************************
 *  \brief read the L2VPN Promiscuous Mode
 **************************************************************************/
int dbrctl_read_l2vpnprommode(char* page, char **start, off_t offset, int count,int *eof, void *data)
{
    int len=0;
    unsigned int l2vpn_prom_mode = DbridgeDB_GetL2vpnPromMode(); 
    
    len+= sprintf(page+len, "%d\n", l2vpn_prom_mode);
    *eof = 1;
    return len;
}

/**************************************************************************/
/*! \fn int dbrctl_read_cmtsmac(char* page, char **start, off_t offset, int count,int *eof, void *data)
 **************************************************************************
 *  \brief read the cmtsmac
 **************************************************************************/
int dbrctl_read_cmtsmac(char* page, char **start, off_t offset, int count,int *eof, void *data)
{
    int len=0;
    unsigned char mac_addr[6];
    DbridgeDb_GetCmtsMacAddr(mac_addr);

    len+= sprintf(page+len, "%.2x-%.2x-%.2x-%.2x-%.2x-%.2x\n", 
                  (unsigned char)mac_addr[0], (unsigned char)mac_addr[1], (unsigned char)mac_addr[2], 
                  (unsigned char)mac_addr[3], (unsigned char)mac_addr[4], (unsigned char)mac_addr[5] );
    *eof = 1;
    return len;
}

#ifdef CONFIG_MACH_PUMA6
/**************************************************************************/
/*! \fn int dbrctl_read_atommac(char* page, char **start, off_t offset, int count,int *eof, void *data)
 **************************************************************************
 *  \brief read the atommac
 **************************************************************************/
int dbrctl_read_atommac(char* page, char **start, off_t offset, int count,int *eof, void *data)
{
    int len=0;
    unsigned char mac_addr[6];
    DbridgeDb_GetAtomMacAddr(mac_addr);

    len+= sprintf(page+len, "%.2x-%.2x-%.2x-%.2x-%.2x-%.2x\n", 
                  (unsigned char)mac_addr[0], (unsigned char)mac_addr[1], (unsigned char)mac_addr[2], 
                  (unsigned char)mac_addr[3], (unsigned char)mac_addr[4], (unsigned char)mac_addr[5] );
    *eof = 1;
    return len;
}
#endif

/* ARRIS ADD END */
/**************************************************************************/
/*! \fn int dbrctl_write_mcastmac(struct file *fp, const char * buf, unsigned long count, void * data)
 **************************************************************************
 *  \brief proc file to add static Mcast MAC address
 **************************************************************************/
int dbrctl_write_mcastmac(struct file *fp, const char * buf, unsigned long count, void * data)
{
    unsigned char local_buf[40];
    int  ret_val = 0;
    unsigned char macAddress[MAC_ADDRESS_LEN];
    unsigned char cmd0[20];
    unsigned int filter_type;

    if (count > 40)
    {
        printk(KERN_ERR "Buffer Overflow\n");
        return -EFAULT;
    }
    if(copy_from_user(local_buf,buf,count))
        return -EFAULT;;
    local_buf[count-1]='\0'; /* Ignoring last \n char */
    ret_val = count;

    sscanf(local_buf,"%s %d",cmd0 ,&filter_type);
    if (stringToMacArray((unsigned char*)&cmd0,(unsigned char*)&macAddress)==0)
    /* Add test that this is Mcast Mac address */
    (void)DbridgeDb_AddMcastMac((unsigned char*)&macAddress,filter_type);

    return ret_val;
}

/**************************************************************************/
/*! \fn int dbrctl_read_num_of_esafe(char* page, char **start, off_t offset, int count,int *eof, void *data)
 **************************************************************************
 *  \brief read num of eSafe connect to the bridge
 **************************************************************************/
int dbrctl_read_num_of_esafe(char* page, char **start, off_t offset, int count,int *eof, void *data)
{
    int len=0;
    unsigned int num_of_esafe = DbridgeEsafe_GetNumOfEsaf();

    len+= sprintf(page+len, "%d\n",num_of_esafe);
    *eof = 1;
    return len;

}

#ifdef DSG
/**************************************************************************/
/*! \fn int dbrctl_write_dsgaddress(struct file *fp, const char * buf, unsigned long count, void * data)
 **************************************************************************
 *  \brief proc file to add dsg tunnle address
 **************************************************************************/

int dbrctl_write_dsgaddress(struct file *fp, const char * buf, unsigned long count, void * data)
{
    unsigned char local_buf[40];
    int  ret_val = 0;
    unsigned char macAddress[MAC_ADDRESS_LEN];
    unsigned char cmd0[20];
    int tkn_number;
    unsigned int tunl_action_type;

    if (count > 40)
    {
        printk(KERN_ERR "Buffer Overflow\n");
        return -EFAULT;
    }
    if(copy_from_user(local_buf,buf,count))
        return -EFAULT;;
    local_buf[count-1]='\0'; /* Ignoring last \n char */
    ret_val = count;

    tkn_number = sscanf(local_buf,"%s %d",cmd0 ,&tunl_action_type);
    if (stringToMacArray((unsigned char*)&cmd0,(unsigned char*)&macAddress)==0)
    {
        if(tunl_action_type == ADD_TUNNEL)
            (void)DbridgeDb_AddDsgAddress((unsigned char*)&macAddress);
        else if(tunl_action_type == DEL_TUNNEL)
            (void)DbridgeDb_DelDsgAddress((unsigned char*)&macAddress);
        else
        {
            printk(KERN_ERR "Unknown action\n");
            return -EFAULT;
        }
    }
    return ret_val;
}

int dbrctl_read_dsgaddress(char* page, char **start, off_t offset, int count,int *eof, void *data)
{

    off_t pos = 0;
    off_t begin = 0;
    int len = 0;
    int index =0;
    unsigned char macAddress[MAC_ADDRESS_LEN];

    len+= sprintf(page+len, "\nIndex\tMcast MAC\t");
    len+= sprintf(page+len, "\n-----\t---------\t");

    for(index = 0; index< DbridgeDB_GetDsgTunnelNums(); index++)
    {
        if(DbridgeDB_GetDsgTunnelMacAddress(index,(unsigned char*)&macAddress) != 0)
            goto done;
        len+= sprintf(page+len,"\n%d\t%.2x:%.2x:%.2x:%.2x:%.2x:%.2x",index,
                      macAddress[0],macAddress[1],macAddress[2],
                      macAddress[3],macAddress[4],macAddress[5]);
        pos = begin + len;
        if (pos < offset)
        {
            len = 0;
            begin = pos;
        }
        if (pos > offset + count)
            goto done;
    }

    len+= sprintf(page+len, "\n");

    done:

    *start = page + (offset - begin);
    len -= (offset - begin);

    if (len > count)
        len = count;
    if (len < 0)
        len = 0;

    return len;
}

/**************************************************************************/
/*! \fn int dbrctl_write_dsgestbip(struct file *fp, const char * buf, unsigned long count, void * data)
 **************************************************************************
 *  \brief proc file to set dsg estb ip address
 **************************************************************************/

int dbrctl_write_dsgestbip(struct file *fp, const char * buf, unsigned long count, void * data)
{
    unsigned char local_buf[40];
    int  ret_val = 0;
    unsigned long ip;
    int tkn_number;

    if (count > 40)
    {
        printk(KERN_ERR "Buffer Overflow\n");
        return -EFAULT;
    }
    if(copy_from_user(local_buf,buf,count))
        return -EFAULT;;
    local_buf[count-1]='\0'; /* Ignoring last \n char */
    ret_val = count;

    tkn_number = sscanf(local_buf,"%d",&ip);
    (void)DbridgeDB_SetDsgEstbIPAddress(ip);

    return ret_val;
}

int dbrctl_read_dsgestbip(char* page, char **start, off_t offset, int count,int *eof, void *data)
{

    off_t pos = 0;
    off_t begin = 0;
    int len = 0;
    int index =0;
    unsigned long ip;

    len+= sprintf(page+len, "\nESTB IP address\t");

    if(DbridgeDB_GetDsgEstbIPAddress(&ip) != 0)
        goto done;
    len+= sprintf(page+len,"\n%d.%d.%d.%d",
                  (unsigned long)(ip >> 24) & 0xFF,
                  (unsigned long)(ip >> 16) & 0xFF,
                  (unsigned long)(ip >>  8) & 0xFF,
                  (unsigned long)(ip      ) & 0xFF);
    pos = begin + len;
    if (pos < offset)
    {
        len = 0;
        begin = pos;
    }
    if (pos > offset + count)
        goto done;

    len+= sprintf(page+len, "\n");

    done:

    *start = page + (offset - begin);
    len -= (offset - begin);

    if (len > count)
        len = count;
    if (len < 0)
        len = 0;

    return len;
}


int dbrctl_write_dsgmode(struct file *fp, const char * buf, unsigned long count, void * data)
{
    unsigned char local_buf[10];
    int  ret_val = 0;
       unsigned int mode;

    if (count > 10)
    {
        printk(KERN_ERR "Buffer Overflow\n");
        return -EFAULT;
    }

    if(copy_from_user(local_buf,buf,count))
        return -EFAULT;;
    local_buf[count-1]='\0'; /* Ignoring last \n char */
    ret_val = count;
    sscanf(local_buf,"%d",&mode);
    if (mode > DBR_DSGLOOB_LOOB_ENABLE)
    {
        mode= 0; /* error code */
    }
    DbridgeDB_SetDsgMode(mode);
    return ret_val;
}

int dbrctl_read_dsgmode(char* page, char **start, off_t offset, int count,int *eof, void *data)
{
    int len=0;
    unsigned char mode = DbridgeDB_GetDsgMode();
    switch(mode)
    {
        case DBR_DSGLOOB_DOCSIS_STANDARD:
            len+= sprintf(page+len, "Docsis Standard Mode \n");
            break;
        case DBR_DSGLOOB_DSG_ENABLE:
            len+= sprintf(page+len, "DSG Mode \n");
            break;
        case DBR_DSGLOOB_LOOB_ENABLE:
            len+= sprintf(page+len, "LOOB Mode \n");
            break;
        default:
            len+= sprintf(page+len, "Unknown mode\n");
    }
    *eof = 1;
    return len;
}

int dbrctl_read_dsgdevice(char* page, char **start, off_t offset, int count,int *eof, void *data)
{
	int len=0;
	int index;
	struct net_device *dev;
	
	DbridgeDB_GetDsgDeviceIndex(&index);
	
	dev = dev_get_by_index(&init_net, index);

	if (dev)
	{
		len+= sprintf(page+len, "name: %s index: %d\n", dev->name, index);
        /* Release the device */
        dev_put(dev);
	}

    *eof = 1;
    return len;
}

int dbrctl_write_dsgdevice(struct file *fp, const char * buf, unsigned long count, void * data)
{
	char devname[IFNAMSIZ+1];
    int ret_val = 0;
	int dev_index;
	struct net_device *dev;
    if (count > sizeof(devname))
    {
        printk(KERN_ERR "Buffer Overflow\n");
        return -EFAULT;
    }

    if(copy_from_user(devname,buf,count))
        return -EFAULT;
	devname[count]='\0';
    ret_val = count;
	dev = dev_get_by_name(&init_net, devname);
	if (dev == NULL)
    {
        printk(KERN_ERR "No such device: %s\n", devname);
        return -EFAULT;
    }
	dev_index = dev->ifindex;
	dev_put(dev);
  
	DbridgeDB_SetDsgDeviceIndex(&dev_index);
    return ret_val;
}

/**************************************************************************/
/*! \fn int dbrctl_read_esafe(char* page, char **start, off_t offset, int count,int *eof, void *data)
 **************************************************************************
 *  \brief proc file to get the esafe mac adress list
 **************************************************************************/

int dbrctl_read_esafe(char* page, char **start, off_t offset, int count,int *eof, void *data)
{
	off_t pos = 0;
	off_t begin = 0;
	int len = 0;
	unsigned int index =0;
	unsigned int num_of_alt_member = 0;
	DbridgeAltEntry_t* ptr_dbridge_alt;
	unsigned int max_cpe = DbridgeDB_GetMaxCpe();


	len+= sprintf(page+len, "\nesafe List           ");
	len+= sprintf(page+len, "\n--------------------");
	len+= sprintf(page+len, "\nMax CPEs\t%d",max_cpe);
	len+= sprintf(page+len, "\n--------------------");

	num_of_alt_member = DbridgeDb_AltGetNumMember();
	if (num_of_alt_member)
	{
		len+= sprintf(page+len, "\n\tMac\t\t\tName\tLearning\tSource\tCpe Num");
		len+= sprintf(page+len, "\n\t---\t\t\t----\t--------\t------\t-------");
	}
	for (index=0; index< num_of_alt_member; index++)
	{
		ptr_dbridge_alt = DbridgeDb_AltGetMemberByIndex(index);
		if (ptr_dbridge_alt)
		{
			if (ptr_dbridge_alt->macLearningFrom == CPE_ESAFE)
			{
				len+= sprintf(page+len, "\n->\t%.2x:%.2x:%.2x:%.2x:%.2x:%.2x",
							  ptr_dbridge_alt->macAddress[0],ptr_dbridge_alt->macAddress[1],
							  ptr_dbridge_alt->macAddress[2],ptr_dbridge_alt->macAddress[3],
							  ptr_dbridge_alt->macAddress[4],ptr_dbridge_alt->macAddress[5]);

				if (ptr_dbridge_alt->dbridgeDevice)
					len+= sprintf(page+len, "\t%s",ptr_dbridge_alt->dbridgeDevice->ptr_interface->name);
				else
					len+= sprintf(page+len, "\tNULL");

				len+= sprintf(page+len, "\teSafe");

				if (ptr_dbridge_alt->sourceInterface)
					len+= sprintf(page+len, "\t\t%s",ptr_dbridge_alt->sourceInterface->name);
				else
					len+= sprintf(page+len, "\t\tNULL");
				len+= sprintf(page+len, "\t%d",ptr_dbridge_alt->cpe_number);

			}
		}

		pos = begin + len;
		if (pos < offset)
		{
			len = 0;
			begin = pos;
		}
		if (pos > offset + count)
			goto done;
	}


	len+= sprintf(page+len, "\n");

	done:

	*start = page + (offset - begin);
	len -= (offset - begin);

	if (len > count)
		len = count;
	if (len < 0)
		len = 0;

	return len;

}

#endif //DSG

/**************************************************************************/
/*! \fn int dbrctl_read_mdfmode(char* page, char **start, off_t offset, int count,int *eof, void *data)
 **************************************************************************
 *  \brief read DOCSIS bridge MDF mode
 **************************************************************************/
int dbrctl_read_mdfmode(char* page, char **start, off_t offset, int count,int *eof, void *data)
{
    int len=0;
    unsigned int mdfMode = 0;

    DbridgeMdfDb_GetMdfMode(&mdfMode);

    switch(mdfMode)
    {
        case DBR_MDF_DISABLE_MODE:
            len+= sprintf(page+len, "Disabled \n");
            break;
        case DBR_MDF_ENABLE_MODE:
            len+= sprintf(page+len, "Enabled \n");
            break;
        default:
            len+= sprintf(page+len, "Unknown mode\n");
    }

    *eof = 1;
    return len;
}


/**************************************************************************/
/*! \fn int dbrctl_write_ipv4WanInterface_enable(struct file *fp, const char * buf, unsigned long count, void * data)
 **************************************************************************
 *  \brief proc file to set the ipv4 wan Interface enable
 **************************************************************************/
int dbrctl_write_ipv4WanInterface_enable(struct file *fp, const char * buf, unsigned long count, void * data)
{
    unsigned char local_buf[10];
    int  ret_val = 0;
    if (count > 10)
    {
        printk(KERN_ERR "Buffer Overflow\n");
        return -EFAULT;
    }

    if(copy_from_user(local_buf,buf,count))
        return -EFAULT;;
    local_buf[count-1]='\0';
    ret_val = count;
    sscanf(local_buf,"%d",&ipv4WanInterface_enable);
    return ret_val;
}


/**************************************************************************/
/*! \fn int dbrctl_read_ipv4WanInterface_enable(char* page, char **start, off_t offset, int count,int *eof, void *data)
 **************************************************************************
 *  \brief read ipv4 waN interface enable
 **************************************************************************/
int dbrctl_read_ipv4WanInterface_enable(char* page, char **start, off_t offset, int count,int *eof, void *data)
{
    int len=0;

    len+= sprintf(page+len, "%d\n",ipv4WanInterface_enable);
    *eof = 1;
    return len;

}

/**************************************************************************/
/*! \fn int dbrctl_write_ipv6WanInterface_enable(struct file *fp, const char * buf, unsigned long count, void * data)
 **************************************************************************
 *  \brief proc file to set the ipv6 wan interface enable
 **************************************************************************/
int dbrctl_write_ipv6WanInterface_enable(struct file *fp, const char * buf, unsigned long count, void * data)
{
    unsigned char local_buf[10];
    int  ret_val = 0;
    if (count > 10)
    {
        printk(KERN_ERR "Buffer Overflow\n");
        return -EFAULT;
    }

    if(copy_from_user(local_buf,buf,count))
        return -EFAULT;;
    local_buf[count-1]='\0';
    ret_val = count;
    sscanf(local_buf,"%d",&ipv6WanInterface_enable);
    return ret_val;
}



/**************************************************************************/
/*! \fn int dbrctl_read_ipv6WanInterface_enable(char* page, char **start, off_t offset, int count,int *eof, void *data)
 **************************************************************************
 *  \brief read ipv6 WAN interface enable
 **************************************************************************/
int dbrctl_read_ipv6WanInterface_enable(char* page, char **start, off_t offset, int count,int *eof, void *data)
{
    int len=0;

    len+= sprintf(page+len, "%d\n",ipv6WanInterface_enable);
    *eof = 1;
    return len;

}

/**************************************************************************/
/*! \fn int dbrctl_write_ipv4LanInterface_enable(struct file *fp, const char * buf, unsigned long count, void * data)
 **************************************************************************
 *  \brief proc file to set the ipv4 lan Interface enable
 **************************************************************************/
int dbrctl_write_ipv4LanInterface_enable(struct file *fp, const char * buf, unsigned long count, void * data)
{
    unsigned char local_buf[10];
    int  ret_val = 0;
    if (count > 10)
    {
        printk(KERN_ERR "Buffer Overflow\n");
        return -EFAULT;
    }

    if(copy_from_user(local_buf,buf,count))
        return -EFAULT;;
    local_buf[count-1]='\0';
    ret_val = count;
    sscanf(local_buf,"%d",&ipv4LanInterface_enable);
    return ret_val;
}


/**************************************************************************/
/*! \fn int dbrctl_read_ipv4LanInterface_enable(char* page, char **start, off_t offset, int count,int *eof, void *data)
 **************************************************************************
 *  \brief read ipv4 Laninterface enable
 **************************************************************************/
int dbrctl_read_ipv4LanInterface_enable(char* page, char **start, off_t offset, int count,int *eof, void *data)
{
    int len=0;

    len+= sprintf(page+len, "%d\n",ipv4LanInterface_enable);
    *eof = 1;
    return len;

}

/**************************************************************************/
/*! \fn int dbrctl_write_ipv6LanInterface_enable(struct file *fp, const char * buf, unsigned long count, void * data)
 **************************************************************************
 *  \brief proc file to set the ipv6 Lan interface enable
 **************************************************************************/
int dbrctl_write_ipv6LanInterface_enable(struct file *fp, const char * buf, unsigned long count, void * data)
{
    unsigned char local_buf[10];
    int  ret_val = 0;
    if (count > 10)
    {
        printk(KERN_ERR "Buffer Overflow\n");
        return -EFAULT;
    }

    if(copy_from_user(local_buf,buf,count))
        return -EFAULT;;
    local_buf[count-1]='\0';
    ret_val = count;
    sscanf(local_buf,"%d",&ipv6LanInterface_enable);
    return ret_val;
}



/**************************************************************************/
/*! \fn int dbrctl_read_ipv6LanInterface_enable(char* page, char **start, off_t offset, int count,int *eof, void *data)
 **************************************************************************
 *  \brief read ipv6 LAN interface enable
 **************************************************************************/
int dbrctl_read_ipv6LanInterface_enable(char* page, char **start, off_t offset, int count,int *eof, void *data)
{
    int len=0;

    len+= sprintf(page+len, "%d\n",ipv6LanInterface_enable);
    *eof = 1;
    return len;

}

/**************************************************************************/
/*! \fn static int stringToMacArray
 **************************************************************************
 *  \brief Converts mac address string in to array .
 *  \param[in] str mac addres string
 *  \param[out] mac_array mac addres array
 *  \return The decimal value of an hexadecimal character.
 **************************************************************************/
static int stringToMacArray(unsigned char* str, unsigned char* mac_array)
{
    int i;
    unsigned int uint32Val;
    unsigned int pDigitPos;
    unsigned int pLength;
    unsigned int pPos;

    for (i = 0; i < MAC_ADDRESS_LEN; i++)
    {
        mac_array[i] = 0;
    }
    i = 0;
    uint32Val = 0;
    pDigitPos = 0;

    pLength = strlen(str);
    if (pLength > 17)
    {
        return -1;
    }

    for (pPos = 0; pPos < pLength; pPos++)
    {
        if (str[pPos] == '-' || str[pPos] ==':')
        {
            if (i >= 5)
            {
                /* its the sixth '-' */
                return -1;
            }
            if ((uint32Val == 0) && (pDigitPos == 0))
            {
                return -1;
            }
            mac_array[i] = uint32Val;
            i++;
            uint32Val = 0;
            pDigitPos = 0;
        } else if (isxdigit(str[pPos]))
        {
            pDigitPos++;
            if (pDigitPos > 2)
            {
                return -1;
            }
            uint32Val = uint32Val * 16;
            if (isdigit(str[pPos]))
            {
                uint32Val += (str[pPos] - '0');
            } else
            {
                uint32Val += HexToDec(str[pPos]);
            }
        } else
        {
            return -1;
        }
    }

    if (i < 5)
    {
        return -1;
    }

    /* its the last octet */
    if ((uint32Val == 0) && (pDigitPos == 0))
    {
        return -1;
    }
    mac_array[i] = uint32Val;

    return 0;

}


/**************************************************************************/
/*! \fn static unsigned char HexToDec(unsigned char c)
 **************************************************************************
 *! \fn static unsigned unsigned char HexToDec(unsigned char c)
 *  \brief Converts hexadecimal character into decimal value.
 *  \param[in] c Hexadecimal character
 *  \return The decimal value of an hexadecimal character.
 **************************************************************************/
static unsigned char HexToDec(unsigned char c)
{
    if (c >= 'A' && c <= 'F')
    {
        return(c - 'A' + 10);
    } else if (c >= 'a' && c <= 'f')
    {
        return(c - 'a' + 10);
    }
    return c - '0';
}

#ifdef DBRIDGE_LOG
 /***************************************************************************
 *  \brief proc file to get static Mcast MAc
 **************************************************************************/
int dbrctl_read_debuge_level(char* page, char **start, off_t offset, int count,int *eof, void *data)
{
#define dbgline(__e) \
  	len+= sprintf(page+len, "\n%-.29s0x%08X - %d", #__e "                             ", __e, __e);

    int len=0;
	len+= sprintf(page+len, "\nDebuge level is              0x%08X - %d", dbridgeDbgLevel, dbridgeDbgLevel);
    len+= sprintf(page+len, "\nDebuge level Option:");

#define DBRIDGE_DBG_TYPE(__name, __mask) dbgline(__name);
    DBRIDGE_DBG_TYPES
#undef DBRIDGE_DBG_TYPE

    len+= sprintf(page+len, "\n");
    *eof = 1;
    return len;
}
int dbrctl_read_test(char* page, char **start, off_t offset, int count,int *eof, void *data)
{
    int len=0;
    len+= sprintf(page+len, "\nxxxxDebuge level is %d", dbridgeDbgLevel);
    len+= sprintf(page+len, "\nDebuge level Option:");
    len+= sprintf(page+len, "\nDBRIDGE_DBG_REG_TX\t\t%d (0x%x)",DBRIDGE_DBG_REG_TX,DBRIDGE_DBG_REG_TX);
    len+= sprintf(page+len, "\nDBRIDGE_DBG_REG_RECIVE\t\t%d (0x%x)",DBRIDGE_DBG_REG_RECIVE,DBRIDGE_DBG_REG_RECIVE);
    len+= sprintf(page+len, "\nDBRIDGE_DBG_REG_RX_CMCI\t\t%d (0x%x)",DBRIDGE_DBG_REG_RX_CMCI,DBRIDGE_DBG_REG_RX_CMCI);
    len+= sprintf(page+len, "\nDBRIDGE_DBG_REG_RX_LANIP\t\t%d (0x%x)",DBRIDGE_DBG_REG_RX_LANIP,DBRIDGE_DBG_REG_RX_LANIP);
    len+= sprintf(page+len, "\nDBRIDGE_DBG_REG_RX_WANIP\t\t%d (0x%x)",DBRIDGE_DBG_REG_RX_WANIP,DBRIDGE_DBG_REG_RX_WANIP);
    len+= sprintf(page+len, "\nDBRIDGE_DBG_REG_RX_CABLE\t\t%d (0x%x)",DBRIDGE_DBG_REG_RX_CABLE,DBRIDGE_DBG_REG_RX_CABLE);
    len+= sprintf(page+len, "\nDBRIDGE_DBG_REG_TX_FLOOD\t\t%d (0x%x)",DBRIDGE_DBG_REG_TX_FLOOD,DBRIDGE_DBG_REG_TX_FLOOD);
    len+= sprintf(page+len, "\nDBRIDGE_DBG_REG_TX_IF\t\t%d (0x%x)",DBRIDGE_DBG_REG_TX_IF,DBRIDGE_DBG_REG_TX_IF);
    len+= sprintf(page+len, "\nDBRIDGE_DBG_MCAST_CABLE\t\t%d (0x%x)",DBRIDGE_DBG_MCAST_CABLE,DBRIDGE_DBG_MCAST_CABLE);
    len+= sprintf(page+len, "\nDBRIDGE_DBG_REG_IGMP\t\t%d (0x%x)",DBRIDGE_DBG_REG_IGMP,DBRIDGE_DBG_REG_IGMP);
    len+= sprintf(page+len, "\nDBRIDGE_DBG_PREREG_RX_CABLE\t\t%d (0x%x)",DBRIDGE_DBG_PREREG_RX_CABLE,DBRIDGE_DBG_PREREG_RX_CABLE);
    len+= sprintf(page+len, "\nDBRIDGE_DBG_CLASSIFIER\t\t%d (0x%x)",DBRIDGE_DBG_CLASSIFIER,DBRIDGE_DBG_CLASSIFIER);
    len+= sprintf(page+len, "\nDBRIDGE_DBG_FILTER\t\t%d (0x%x)",DBRIDGE_DBG_FILTER,DBRIDGE_DBG_FILTER);
    len+= sprintf(page+len, "\nDBRIDGE_DBG_MDF\t\t%d (0x%x)",DBRIDGE_DBG_MDF,DBRIDGE_DBG_MDF);
    len+= sprintf(page+len, "\nDBRIDGE_DBG_L2VPN_DS\t\t%d (0x%x)",DBRIDGE_DBG_L2VPN_DS,DBRIDGE_DBG_L2VPN_DS);
    len+= sprintf(page+len, "\n");
    *eof = 1;
    return len;
}

/**************************************************************************/
/*! \fn int dbrctl_write_debuge_level(struct file *fp, const char * buf, unsigned long count, void * data)
 **************************************************************************
 *  \brief proc file to add static Mcast MAC address
 **************************************************************************/
int dbrctl_write_debuge_level(struct file *fp, const char * buf, unsigned long count, void * data)
{
// ARRIS MOD BEGIN : I don't think anyone ever tested this.
// No way it would ever work.
#if 0
    unsigned char local_buf[sizeof("0x12345678\n") + 11 /* spare or if someone is crazy enough to use decimal... */]; 
    int  ret_val = 0;
    int __attribute__ ((unused)) kstrtouint_retval;

    if (count > sizeof(local_buf))
    kstrtouint_retval = kstrtouint(local_buf, 0, &dbridgeDbgLevel);
    return ret_val;
#else // I'll re-implement it
    unsigned char local_buf[32];
    if ( count < 32 )
    {
        if( copy_from_user(local_buf, buf, count) )
        {
            return -EFAULT;
        }
        local_buf[count-1]='\0';
        kstrtouint(local_buf, 0, &dbridgeDbgLevel);
    }    
    return count;
#endif
    // ARRIS MOD END
}


int dbrctl_write_test(struct file *fp, const char * buf, unsigned long count, void * data)
{
    unsigned char local_buf[10];
    int  ret_val = 0;
    if (count > 10)
    {
        printk(KERN_ERR "Buffer Overflow\n");
        return -EFAULT;
    }

    if(copy_from_user(local_buf,buf,count))
        return -EFAULT;;
    local_buf[count-1]='\0';
    ret_val = count;
    sscanf(local_buf,"xxx %d",&dbridgeDbgLevel);
    return ret_val;
}


#endif

/**************************************************************************/
/*! \fn int dbrctl_write_addesafemac2alt(struct file *fp, const char * buf, unsigned long count, void * data)
 **************************************************************************
 *  \brief proc file to add an eSafe device to the ALT
 *          param1 - Device name
 *          param2 - MAC address : xx:xx:xx:xx:xx:xx or xx-xx-xx-xx-xx-xx
 **************************************************************************/
int dbrctl_write_addesafemac2alt(struct file *fp, const char * buf, unsigned long count, void * data)
{
    char local_buf[50]; /* 16 for device, 1 for space, 17 for MAC, 1 for \0, some spare (whitespaces) */
    int  ret_val = 0;
    unsigned char macAddress[MAC_ADDRESS_LEN];
    DbridgeDevice_t *dbDev;
    struct net_device * dev =0;
    char *devname;
    char *macstr;
    char *endtok;
    static const char *sep = " \t,\n";
    DbridgeEsafeDevice_t*  ptr_esafe_table;

    if (count > sizeof(local_buf))
    {
        printk(KERN_ERR "Buffer Overflow\n");
        return -EFAULT;
    }

    if(copy_from_user(local_buf,buf,count))
        return -EFAULT;;
    local_buf[count-1]='\0'; /* Ignoring last \n char */
    ret_val = count;

    /* Parse line */
    endtok = local_buf;
    TOKENIZE_ERR(devname, sep, endtok, "No device name");
    TOKENIZE_ERR(macstr, sep, endtok, "No MAC address");

    dev = dev_get_by_name(&init_net, devname);
    if (dev == NULL)
    {
        printk(KERN_ERR "No such device: %s\n", devname);
        return -EFAULT;
    }

    ptr_esafe_table = DbridgeEsafe_GetMemberFromTargetDev(dev);
    if (ptr_esafe_table == NULL)
    {
        printk(KERN_ERR "No entry in eSafe Dbridge DB for device: %s\n", devname);
		dev_put(dev);
        return -EFAULT;
    }

    dbDev = DbridgeDb_DevGetByInterface (ptr_esafe_table->ptr_esafeInterface);

    if (stringToMacArray(macstr, macAddress) != 0)
    {
        printk(KERN_ERR "Illegal MAC address: %s\n", macstr);
        dev_put(dev);
        return -EFAULT;
    }

#ifdef CONFIG_VENDOR_ARRIS_GATEWAY  // ARRIS ADD
        DbridgeDb_AltAddMac(macAddress, CPE_STATIC, dbDev, dev,0);
#else        
    DbridgeDb_AltAddMac(macAddress, CPE_STATIC, dbDev, dev);
#endif // ARRIS ADD END          


    if (strcmp(devname, "mta0") == 0)
    {
        /* Required in cni */
        mtaDev = dev;
    }

    dev_put(dev);

    return ret_val;
}


/**************************************************************************/
/*! \fn int dbrctl_write_erouter_mode(struct file *fp, const char * buf, unsigned long count, void * data)
 **************************************************************************
 *  \brief proc file to set the erouter_mode
 **************************************************************************/
int dbrctl_write_erouter_mode(struct file *fp, const char * buf, unsigned long count, void * data)
{
    unsigned char local_buf[10];
    int  ret_val = 0;
    unsigned int erouter_mode;

    if (count > 10)
    {
        printk(KERN_ERR "Buffer Overflow\n");
        return -EFAULT;
    }

    if(copy_from_user(local_buf,buf,count))
        return -EFAULT;;
    local_buf[count-1]='\0'; /* Ignoring last \n char */
    ret_val = count;
    sscanf(local_buf,"%d",&erouter_mode);

    DbridgeDb_Erouter_SetMode(erouter_mode);
    return ret_val;

}


/**************************************************************************/
/*! \fn int dbrctl_read_erouter_mode(char* page, char **start, off_t offset, int count,int *eof, void *data)
 **************************************************************************
 *  \brief read erouter_mode
 **************************************************************************/
int dbrctl_read_erouter_mode(char* page, char **start, off_t offset, int count,int *eof, void *data)
{
    int len=0;
    unsigned int erouter_mode = DbridgeDb_Erouter_GetMode();

    len+= sprintf(page+len, "%d\n",erouter_mode);
    *eof = 1;
    return len;

}


/**************************************************************************/
/*! \fn int dbrctl_write_setLbrIf_mask(struct file *fp, const char * buf, unsigned long count, void * data)
 **************************************************************************
 *  \brief proc file to setLbrIf_mask
 **************************************************************************/
int dbrctl_write_setLbrIf_mask(struct file *fp, const char * buf, unsigned long count, void * data)
{
    unsigned char local_buf[30];
    int  ret_val = 0;
    struct net_device * dev =0;


    if (count > 30)
    {
        printk(KERN_ERR "Buffer Overflow\n");
        return -EFAULT;
    }
    if(copy_from_user(local_buf,buf,count))
        return -EFAULT;
    local_buf[count-1]='\0'; /* Ignoring last \n char */
    ret_val = count;
	    
	dev = dev_get_by_name(&init_net, local_buf);
	if (dev == NULL)
	{
		printk(KERN_ERR "No such device: %s\n", local_buf);
		return -EFAULT;
	}

	DbridgeDB_SetlbridgeInterfaceMask(dev->ifindex);
    
	dev_put(dev);
    return ret_val;

}


/**************************************************************************/
/*! \fn int dbrctl_read_setLbrIf_mask(char* page, char **start, off_t offset, int count,int *eof, void *data)
 **************************************************************************
 *  \brief read dbrctl_read_setLbrIf_mask
 **************************************************************************/
int dbrctl_read_setLbrIf_mask(char* page, char **start, off_t offset, int count,int *eof, void *data)
{
    int len=0;

    len+= sprintf(page+len, "0x%llX\n",DbridgeDB_GetlbridgeInterfaceMask());  
    *eof = 1;                
    return len;

}


EXPORT_SYMBOL(mtaDev);

module_init(dbridgeCtl_init)
module_exit(dbridgeCtl_exit)
MODULE_LICENSE("GPL");

