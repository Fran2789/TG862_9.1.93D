/*
 *
 * dpp_6rd_pp_ctl.c
 * Description:
 * DOCSIS Packet Processor 6RD (protocol 41) configuration and control implementation
 *
 * DESCRIPTION:   Provides DOCSIS Packet Processor 6RD support(protocol 41) 
 *                configuration and control function. 6RD PP support might bring some
 *                problematic effects, the feature need to be configurable.
 *
 *                Author: Brian Liu
 *
 * Copyright 2013, ARRIS Group, Inc., All rights reserved
*/

/*! \file dpp_6rd_pp_ctl.c
    \brief Docsis Packet Processor 6RD (protocol 41) configuration and control
*/

/**************************************************************************/
/*      INCLUDES:                                                         */
/**************************************************************************/
#include <linux/module.h>
#include <linux/kernel.h>
#include <linux/proc_fs.h>
#include <linux/uaccess.h>
#include <linux/ti_hil.h>
#include <net/net_namespace.h> // ARRIS ADD
#include "dpp_ctl.h"
#include "dpp_6rd_pp_ctl.h"

/**************************************************************************/
/*      EXTERNS Declaration:                                              */
/**************************************************************************/


/**************************************************************************/
/*      DEFINES:                                                          */
/**************************************************************************/


/**************************************************************************/
/*      LOCAL DECLARATIONS:                                               */
/**************************************************************************/
static int dppctl_write_6rd_pp_status(struct file *fp, const char * buf, unsigned long count, void * data);

/**************************************************************************/
/*      LOCAL VARIABLES:                                                  */
/**************************************************************************/
static struct proc_dir_entry *dpp_6rd_pp_ctl = NULL;

/**************************************************************************/
/*      INTERFACE FUNCTIONS Implementation:                               */
/**************************************************************************/


/**************************************************************************/
/*! \fn int Dpp6rdPPCtl_Init(void)
 **************************************************************************
 *  \brief DOCSIS Packet Processor 6RD (protocol 41) Ctl initialization.
 *  \param[in] no input.
 *  \param[out] no output.
 *  \return 0 or error code.
 **************************************************************************/
int Dpp6rdPPCtl_Init(void)
{
	/* Create 6RD PP proc */
	dpp_6rd_pp_ctl = create_proc_entry("ti_pp_docsis/6rd_pp", 0644, init_net.proc_net); // ARRIS MOD

	if (dpp_6rd_pp_ctl)
	{
		dpp_6rd_pp_ctl->read_proc  = NULL;
		dpp_6rd_pp_ctl->write_proc = dppctl_write_6rd_pp_status;
	}

    printk(KERN_INFO"\nDOCSIS Packet Processor 6RD (protocol 41) control init done\n");

    return 0;
}

/**************************************************************************/
/*! \fn int Dpp6rdPPCtl_Exit(void)
 **************************************************************************
 *  \brief DOCSIS Packet Processor 6RD (protocol 41) Ctl exit.
 *  \param[in] no input.
 *  \param[out] no output.
 *  \return 0 or error code.
 **************************************************************************/
int Dpp6rdPPCtl_Exit(void)
{
    printk(KERN_INFO"\nDOCSIS Packet Processor 6RD (protocol 41) control exit done\n");

    return 0;
}



/**************************************************************************/
/*      LOCAL FUNCTIONS:                                                  */
/**************************************************************************/

/**************************************************************************/
/*! \fn static int dppctl_write_6rd_pp_status(struct file *fp, const char * buf, unsigned long count, void * data)                                     
 **************************************************************************
 *  \brief DOCSIS Packet Processor control write 6RD PP (protocol 41) status.
 *  \return OK or error status.
 **************************************************************************/
static int dppctl_write_6rd_pp_status(struct file *fp, const char * buf, unsigned long count, void * data)
{
	unsigned char local_buf[10];
	int ret_val = 0;

	if (count > 10)
	{
		printk(KERN_ERR "\nBuffer Overflow\n");
		return -EFAULT;
	}

	if (copy_from_user(local_buf, buf, count))
    {
        return -EFAULT;;
    }

	local_buf[count-1]='\0'; 
	ret_val = count;

    if (!strcmp(local_buf, "enable"))
    {
        printk(KERN_INFO"\n%s: Call HIL Enable 6RD PP Support\n", __FUNCTION__);
        if (ti_hil_enable_6rd_pp() < 0)
        {
            printk(KERN_ERR "\nti_hil_enable_6rd_pp Failed\n");
            return -EFAULT;
        }
        printk(KERN_INFO"\n%s: HIL Enable 6RD PP Support done\n", __FUNCTION__);
    }
    else
    {
        printk(KERN_INFO"\n%s: Call HIL Disable 6RD PP Support\n", __FUNCTION__);
        if (ti_hil_disable_6rd_pp() < 0)
        {
            printk(KERN_ERR "\nti_hil_disable_6rd_pp Failed\n");
            return -EFAULT;
        }
        printk(KERN_INFO"\n%s: HIL Disable 6RD PP Support done\n", __FUNCTION__);
    }

	return ret_val;
}

