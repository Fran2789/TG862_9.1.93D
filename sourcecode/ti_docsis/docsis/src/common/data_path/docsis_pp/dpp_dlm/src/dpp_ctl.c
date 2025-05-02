/*
 *
 * dpp_ctl.c
 * Description:
 * DOCSIS Packet Processor control implementation
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

#define _DPP_CTL_C_

/*! \file dpp_ctl.c
	\brief Docsis Packet Processor control module
*/

/**************************************************************************/
/*      INCLUDES:                                                         */
/**************************************************************************/
#include <linux/module.h>
#include <linux/kernel.h>
#include <linux/proc_fs.h>
#include <linux/netdevice.h>
#include <linux/uaccess.h>
#include "dpp_ctl.h"
#include "dpp_counters_ctl.h"
#include "dpp_psm_ctl.h"
#include "dpp_6rd_pp_ctl.h"
#include "dpp_tunnel_ctl.h"

MODULE_LICENSE("GPL");
MODULE_DESCRIPTION("Docsis Packet Processor Kernel Module");
MODULE_AUTHOR("Orit Brayer");

/**************************************************************************/
/*      EXTERNS Declaration:                                              */
/**************************************************************************/

/**************************************************************************/
/*      DEFINES:                                                          */
/**************************************************************************/

/**************************************************************************/
/*      LOCAL DECLARATIONS:                                               */
/**************************************************************************/

#ifdef DPP_LOG
static int dppctl_write_debug_level(struct file *fp, const char * buf, unsigned long count, void * data);
static int dppctl_read_debug_level(char* page, char **start, off_t offset, int count,int *eof, void *data);
#endif

/**************************************************************************/
/*      LOCAL VARIABLES:                                                  */
/**************************************************************************/
#ifdef DPP_LOG
unsigned int dppDbgLevel = 0;
#endif
struct proc_dir_entry *dppctl = NULL;
#ifdef DPP_LOG
static struct proc_dir_entry *dpp_debuge_level = NULL;
#endif

/**************************************************************************/
/*      INTERFACE FUNCTIONS Implementation:                               */
/**************************************************************************/


/**************************************************************************/
/*      LOCAL FUNCTIONS:                                                  */
/**************************************************************************/


/**************************************************************************/
/*! \fn int __init dpp_mod_init(void)                                     
 **************************************************************************
 *  \brief DOCSIS Packet Processor control module initialization.
 *  \param[in] no input.
 *  \param[out] no output.
 *  \return OK or error status.
 **************************************************************************/
static int __init dpp_mod_init(void)
{
    	/* create base directory for or DOCSIS Packet Processor */
       dppctl = proc_mkdir("ti_pp_docsis", init_net.proc_net);
       if (!dppctl) 
       {
			pr_err("Unable to proc dir entry\n");
		    remove_proc_entry("ti_pp_docsis", init_net.proc_net);
			return -ENOMEM;		        
	   }
#ifdef DPP_LOG
	/* Create debuge level proc */
	dpp_debuge_level = create_proc_entry("dbglevel", 0644, dppctl);
	if (dpp_debuge_level)
	{
		dpp_debuge_level->read_proc  = dppctl_read_debug_level;
		dpp_debuge_level->write_proc = dppctl_write_debug_level;
	}
#endif

#ifdef CONFIG_TI_PACKET_PROCESSOR_STATS
    /* DOCSIS Packet Processor counters ctl initialization */
    DppCountersCtl_Init();
#endif 

    /* DOCSIS Packet Processor 6RD (protocol 41) ctl initialization */
    Dpp6rdPPCtl_Init();

    DppTunnelCtl_Init();

    printk(KERN_INFO"\nDOCSIS Packet Processor control init done\n");

	return 0; 
}

/**************************************************************************/
/*! \fn static void __exit dpp_mod_exit(void)                                    
 **************************************************************************
 *  \brief DOCSIS Packet Processor control module exit.
 *  \param[in] no input.
 *  \param[out] no output.
 *  \return None.
 **************************************************************************/
static void __exit dpp_mod_exit(void)
{
#ifdef CONFIG_TI_PACKET_PROCESSOR_STATS
    /* DOCSIS Packet Processor counters ctl exit */
    DppCountersCtl_Exit();
#endif 

    /* DOCSIS Packet Processor 6RD (protocol 41) ctl exit */
    Dpp6rdPPCtl_Exit();

    DppTunnelCtl_Exit();

    printk(KERN_INFO"\nDOCSIS Packet Processor control exit done\n");
}

#ifdef DPP_LOG
/**************************************************************************/
/*! \fn static int dppctl_read_debug_level(char* page, char **start, off_t offset, int count,int *eof, void *data)
 **************************************************************************
 *  \brief DOCSIS Packet Processor control read debug level.
 *  \return OK or error status.
 **************************************************************************/
static int dppctl_read_debug_level(char* page, char **start, off_t offset, int count,int *eof, void *data)
{
	int len = 0;
	len+= sprintf(page+len, "\nDebuge level is %d", dppDbgLevel);
	len+= sprintf(page+len, "\nDebuge level Option:");
  	len+= sprintf(page+len, "\nDPP_DBG_CTL\t\t%d (0x%x)",DPP_DBG_CTL,DPP_DBG_CTL);
  	len+= sprintf(page+len, "\nDPP_DBG_COUNTERS\t\t%d (0x%x)",DPP_DBG_COUNTERS,DPP_DBG_COUNTERS);
	len+= sprintf(page+len, "\n");
	*eof = 1;
	return len;
}

/**************************************************************************/
/*! \fn static int dppctl_write_debug_level(struct file *fp, const char * buf, unsigned long count, void * data)                                     
 **************************************************************************
 *  \brief DOCSIS Packet Processor control write debug level.
 *  \return OK or error status.
 **************************************************************************/
static int dppctl_write_debug_level(struct file *fp, const char * buf, unsigned long count, void * data)
{
	unsigned char local_buf[10];
	int ret_val = 0;
	if (count > 10)
	{
		printk(KERN_ERR "\n%s: Buffer Overflow\n", __FUNCTION__);
		return -EFAULT;
	}

	if (copy_from_user(local_buf,buf,count))
    {
        return -EFAULT;;
    }
	local_buf[count-1]='\0'; 
	ret_val = count;
	sscanf(local_buf,"%d",&dppDbgLevel);
	return ret_val;
}

#endif


module_init(dpp_mod_init);
module_exit(dpp_mod_exit);
