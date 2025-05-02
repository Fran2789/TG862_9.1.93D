/*
 *
 * dpp_psm_ctl.c
 * Description:
 * DOCSIS Packet Processor power save mode configuration and control implementation
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

/*! \file dpp_psm_ctl.c
    \brief Docsis Packet Processor power save mode configuration and control
*/

/**************************************************************************/
/*      INCLUDES:                                                         */
/**************************************************************************/

#include <linux/module.h>
#include <linux/kernel.h>
#include <linux/proc_fs.h>
#include <linux/uaccess.h>
#include <linux/ti_hil.h>
#include "dpp_ctl.h"
#include "dpp_psm_ctl.h"


/**************************************************************************/
/*      EXTERNS Declaration:                                              */
/**************************************************************************/
extern struct proc_dir_entry *dppctl;

/**************************************************************************/
/*      DEFINES:                                                          */
/**************************************************************************/


/**************************************************************************/
/*      LOCAL DECLARATIONS:                                               */
/**************************************************************************/
static int dppctl_write_psm_status(struct file *fp, const char * buf, unsigned long count, void * data);

/**************************************************************************/
/*      LOCAL VARIABLES:                                                  */
/**************************************************************************/
static struct proc_dir_entry *dpp_psm_ctl = NULL;

/**************************************************************************/
/*      INTERFACE FUNCTIONS Implementation:                               */
/**************************************************************************/


/**************************************************************************/
/*! \fn int DppPsmCtl_Init(void)
 **************************************************************************
 *  \brief DOCSIS Packet Processor power save mode Ctl initialization.
 *  \param[in] no input.
 *  \param[out] no output.
 *  \return 0 or error code.
 **************************************************************************/
int DppPsmCtl_Init(void)
{
    if(dppctl)
    {
        /* Create psm proc */
        dpp_psm_ctl = create_proc_entry("psmstatus", 0644, dppctl);
    }
	

	if (dpp_psm_ctl)
	{
		dpp_psm_ctl->read_proc  = NULL;
		dpp_psm_ctl->write_proc = dppctl_write_psm_status;
	}

    printk(KERN_INFO"\nDOCSIS Packet Processor psm control init done\n");

    return 0;
}

/**************************************************************************/
/*! \fn int DppPsmCtl_Exit(void)
 **************************************************************************
 *  \brief DOCSIS Packet Processor power save mode Ctl exit.
 *  \param[in] no input.
 *  \param[out] no output.
 *  \return 0 or error code.
 **************************************************************************/
int DppPsmCtl_Exit(void)
{
    printk(KERN_INFO"\nDOCSIS Packet Processor psm control exit done\n");

    return 0;
}



/**************************************************************************/
/*      LOCAL FUNCTIONS:                                                  */
/**************************************************************************/

/**************************************************************************/
/*! \fn static int dppctl_write_psm_status(struct file *fp, const char * buf, unsigned long count, void * data)                                     
 **************************************************************************
 *  \brief DOCSIS Packet Processor control write psm status.
 *  \return OK or error status.
 **************************************************************************/
static int dppctl_write_psm_status(struct file *fp, const char * buf, unsigned long count, void * data)
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
#ifdef CONFIG_MACH_PUMA5
        printk(KERN_INFO"\n%s: Call HIL Enable PSM\n", __FUNCTION__);
        if (ti_hil_enable_psm() < 0)
        {
            printk(KERN_ERR "\nti_hil_enable_psm Failed\n");
            return -EFAULT;
        }
        printk(KERN_INFO"\n%s: HIL Enable PSM done\n", __FUNCTION__);
#endif
    }
    else
    {
#ifdef CONFIG_MACH_PUMA5
        printk(KERN_INFO"\n%s: Call HIL Disable PSM\n", __FUNCTION__);
        if (ti_hil_disable_psm() < 0)
        {
            printk(KERN_ERR "\nti_hil_disable_psm Failed\n");
            return -EFAULT;
        }
        printk(KERN_INFO"\n%s: HIL Disable PSM done\n", __FUNCTION__);
#endif
    }

	return ret_val;
}
