/*

Copyright (c) 2008-2016 ARRIS Enterprises, LLC

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

/******************************************************************************
 * FILE PURPOSE:     - Battery charger manager module Source
 ******************************************************************************
 * FILE NAME:     ups_manager.c
 *
 * DESCRIPTION:   Linux Battery Charger Telemetry Module
 *                Provides point of contact in kernel for user space charger app.
 *                Interfaces to kernel drivers that communicate with charger.
 *
 *                Author: Bill Mohr, ARRIS Group, Inc.
 *
 *******************************************************************************/

#include <linux/module.h>
#include <linux/moduleparam.h>
#include <linux/fs.h>
#include <linux/netdevice.h>
#include <linux/etherdevice.h>

#include <linux/sched.h>
#include <linux/wait.h>
//#include <asm/irq.h>
#include <linux/irq.h>
#include <asm-arm/arch-avalanche/puma6/puma6.h>
#include "autoconf.h"
#include "tlm_arris.h"


MODULE_LICENSE("Dual BSD/GPL");
#define DEVICE_NAME "tlm"
// externs

extern void TLM_init (void);
extern boolean TLM_testMode;
extern boolean TLM_pollUPS (uint16);
extern void TLM_mk_req (uint8, uint8*, uint16);
extern void TLM_uart_cmdDump (TLM_msg6_t*);
extern void TLM_data2userMsg (TLM_msg2_t*);
extern void CHRGR_kill (void);
extern void TLM_printTlm_todo (void);
extern boolean CHRGR_download (uint8);
extern boolean CHRGR_downloadFactory(uint8);
extern void CHRGR_fwVerGet (TLM_msg4_t*);
extern void TLM_setDataDumpCnt (uint16);
extern boolean TLM_epromUpdateInfo (TLM_eprom_t*);
extern boolean CHRGR_erase(void);
extern boolean AC_ok;
extern uint8 TLM_DebugVal;
extern uint32 TLM_configSclkHz;
extern void TLM_uartInit (void);

// globs
int platform = PLAT_UNKNOWN;
int charger  = CHARGER_UNKNOWN;

module_param(platform, int, S_IRUGO); /* this value will be passed in so we know what we are running on.*/
MODULE_PARM_DESC(platform,"information on which platform we're on");
module_param(charger, int, S_IRUGO); /* this value will be passed in so we know what we are running on.*/
MODULE_PARM_DESC(charger,"information on which charger HW we have");

boolean TLM_initComplete = FALSE;

static  int extIrq0 = 1;
static  int peOccurred = 0;
 
DECLARE_WAIT_QUEUE_HEAD(usWaitQ);

/**************************************************************************/
/*      LOCAL FUNCTION DECLERATIONS:                                     */
/**************************************************************************/
static int ups_open (struct inode *, struct file *);
static long ups_ioctl (struct file *, unsigned int, unsigned long);
void TLM_DebugHidden(uint8 cmd, uint8 value);

extern boolean TLM_check_uart(void);

static struct file_operations tlm_fops = {
    .owner          = THIS_MODULE,
    .open           = ups_open,
    .unlocked_ioctl = ups_ioctl,
};

/**************************************************************************/
/*      LOCAL FUNCTIONS:                                                  */
/**************************************************************************/


static int ups_open (struct inode *inp, struct file *fp)
{
    printk ("TLM: Driver Opened\n");
    return 0;
}

static long ups_ioctl (struct file *file, unsigned int cmd, unsigned long arg)
{
    uint16 i;
    int ret = 0;
    char *cp = 0;
    TLM_privIoctl privIoctl;
    void __user *uarg = (void __user *)arg;

    if (cmd != SIOCDEVPRIVATE)
    {
        printk ("TLM: ups_ioctl error. Cmd not SIOCDEVPRIVATE\n");
        return (-EINVAL);
    }

    if(copy_from_user((char *)&privIoctl, uarg, sizeof(TLM_privIoctl)))
        { return (-EINVAL); }

    switch (privIoctl.cmd)
    {
        case UPS_POLL:
        {
            TLM_msg2_t msg2;

            if (copy_from_user ((char *)&msg2, (char *)privIoctl.data, sizeof(msg2)))
            {
                cp = "UPS_POLL";
                ret =  -EFAULT;
                break;
            }

            AC_ok = msg2.acOK;              // from TLM application
            msg2.testMode   = TLM_testMode; // to TLM application

            for (i = 0; i < (TLM_testMode ? 1 : 2); i++)  // poll both logical batteries
            {
                msg2.result = (uint8)(TLM_pollUPS (i));
                if (!msg2.result)   { break; }
            }

            TLM_data2userMsg (&msg2);

            if (copy_to_user ((char *)privIoctl.data, (char *)&msg2, sizeof(msg2)))
            {
                cp = "UPS_POLL";
                ret =  -EFAULT;
                break;
            }

        }
        break;

        case UPS_DOWNLOAD:  // download charger load - up to 2 trys
        {
            TLM_msg1_t msg1;
            uint8 plat;

            if (copy_from_user ((char *)&msg1, (char *)privIoctl.data, sizeof(msg1)))
            {
                cp = "UPS_DOWNLOAD";
                ret =  -EFAULT;
                break;
            }

            plat = msg1.data;

            msg1.data = CHRGR_download (plat);

            if (!msg1.data)
            {
                if (!(msg1.data = CHRGR_download (plat)))   { CHRGR_kill(); }
            }

            if (copy_to_user ((char*)privIoctl.data, (char *)&msg1, sizeof(msg1)))
            {
                cp = "UPS_DOWNLOAD";
                ret =  -EFAULT;
                break;
            }
        }
        break;

        case UPS_RESET:  // reset charger
        {
            CHRGR_UPSreset();
            printk ("TLM Kernel: Charger is Reset\n");
        }
        break;

        case UPS_CHRGR_FW_VER:
        {
            TLM_msg4_t msg4;

            if (copy_from_user ((char *)&msg4, (char *)privIoctl.data, sizeof(msg4)))
            {
                cp = "UPS_CHRGR_FW_VER";
                ret =  -EFAULT;
                break;
            }

            CHRGR_fwVerGet (&msg4);

            if (copy_to_user ((char *)privIoctl.data, (char *)&msg4, sizeof(msg4)))
            {
                cp = "UPS_CHRGR_FW_VER";
                ret =  -EFAULT;
                break;
            }
        }
        break;

        case UPS_KILL:
        {
            CHRGR_kill();
            printk ("TLM Kernel: Charger is Killed\n");
        }
        break;

        case UPS_CMD_OR_DATA:
        {
            TLM_msg4_t msg4;

            if (copy_from_user ((char *)&msg4, (char *)privIoctl.data, sizeof(msg4)))
            {
                cp = "UPS_CMD_OR_DATA";
                ret =  -EFAULT;
                break;
            }

            TLM_mk_req (msg4.data[tlm_request], &(msg4.data[tlm_data]),
                                                             msg4.data[tlm_battery]);
        }
        break;

        case UPS_TEST_MODE:
        {
            TLM_testMode = TRUE;
            if (!TLM_check_uart())
            {
                cp = "UPS_TEST_MODE";
                ret = -EIO;
            }
        }
        break;

        case UPS_FAC0:    // download factory charger load
        {
            TLM_msg1_t msg1;
            uint8 plat;

            if (copy_from_user ((char *)&msg1, (char *)privIoctl.data, sizeof(msg1)))
            {
                cp = "UPS_FAC0";
                ret =  -EFAULT;
                break;
            }

            plat = msg1.data;

            TLM_testMode = TRUE;

            if (charger == CHARGER_ZILOG)
            {
                msg1.data = CHRGR_downloadFactory (plat);

                if (!msg1.data)   // up to 2 trys to download
                {
                    if (!(msg1.data = CHRGR_downloadFactory (plat)))   { CHRGR_kill(); }
                }

            }
            if (copy_to_user ((char*)privIoctl.data, (char *)&msg1, sizeof(msg1)))
            {
                cp = "UPS_FAC0";
                ret =  -EFAULT;
                break;
            }
        }
        break;

        case UPS_FAC4:    // erase factory charger load
        {
            boolean i = CHRGR_erase();

            if (!i) { i = CHRGR_erase(); }  // up to 2 trys

            if (!i) { cp = "UPS_FAC4"; }

            CHRGR_kill();
        }
        break;

        case UPS_DATA_DUMP:
        {
            static TLM_msg6_t msg6;

            TLM_uart_cmdDump (&msg6);

            if (copy_to_user ((char*)privIoctl.data, (char *)&msg6, sizeof(msg6)))
            {
                cp = "UPS_DATA_DUMP";
                ret =  -EFAULT;
                break;
            }
        }
        break;

        case UPS_DATA_DUMP_CNT:
        {
            TLM_msg4_t msg4;

            if (copy_from_user ((char *)&msg4, (char *)privIoctl.data, sizeof(msg4)))
            {
                cp = "UPS_DATA_DUMP_CNT";
                ret =  -EFAULT;
                break;
            }

            TLM_setDataDumpCnt ((((uint16) msg4.data[1]) << 8) | ((uint16) msg4.data[0]));
        }
        break;

        case UPS_READ_EPROM:
        {
            static TLM_msg5_t msg5;

            msg5.data_valid = TLM_epromUpdateInfo(&(msg5.epromPage));

            if (copy_to_user ((char*)privIoctl.data, (char *)&msg5, sizeof(msg5)))
            {
                cp = "UPS_READ_EPROM";
                ret =  -EFAULT;
                break;
            }
        }
        break;

        case UPS_DEBUG:
        {
            TLM_msg1_t msg1;

            if (copy_from_user ((char *)&msg1, (char *)privIoctl.data, sizeof(msg1)))
            {
                cp = "UPS_DEBUG";
                ret =  -EFAULT;
                break;
            }
            if (msg1.data >= 125)
            {
                TLM_DebugHidden(msg1.data, msg1.spare);
                break;
            }
       
            TLM_DebugVal = msg1.data;            
        }
        break;

        case UPS_INIT_STATUS:  // returns tlm driver init status
        {
            TLM_msg1_t msg1;

            msg1.data = TLM_initComplete;

            if (copy_to_user ((char*)privIoctl.data, (char *)&msg1, sizeof(msg1)))
            {
                cp = "UPS_INIT_STATUS";
                ret =  -EFAULT;
                break;
            }
        }
        break;

        case UPS_WAIT_POWER_EVENT:
        {
            TLM_msg1_t msg1;

            wait_event(usWaitQ, peOccurred == 1);

            /* Reset power event */
            peOccurred = 0;
            msg1.data = extIrq0;

            if (copy_to_user ((char*)privIoctl.data, (char *)&msg1, sizeof(msg1)))
            {
                cp = "UPS_WAIT_POWER";
                ret =  -EFAULT;
                break;
            }
        }
        break;

        default:
        {
            cp = "default";
            ret = -EINVAL;
        }
        break;
    }
    if (cp)  { printk ("TLM: ups_ioctl error on %s\n", cp); }

    return ret;
}

#define TLM_DebugCmdSetClock 125
#define TLM_DebugCmdInitUart 126
void TLM_DebugHidden(uint8 cmd, uint8 value)
{
    switch (cmd)
    {
        case TLM_DebugCmdSetClock:
            TLM_configSclkHz = (value * 1000000) / 2;
            printk("TLM_configSclkHz is set to %d\n", TLM_configSclkHz);            
            break;
        
        case TLM_DebugCmdInitUart:
            TLM_uartInit();
            printk("TLM_uartInit with TLM_configSclkHz value %d\n", TLM_configSclkHz);            
            break;
                 
        default:
            break;
    }
}

/* On interrupt, ublock userspce thread */
static irqreturn_t test_irq(int irq, void *dev_instance)
{
    if ( extIrq0 ) {
        printk( KERN_ERR " AVALANCHE_INT_84 TRIGGERED on level LOW!\n" );

        extIrq0 = 0;

        /* Reconfigure ExtIrq0 to be level HIGH */
        irq_set_irq_type( AVALANCHE_INT_84, IRQF_TRIGGER_HIGH );
    } else {
        printk( KERN_ERR " AVALANCHE_INT_84 TRIGGERED on level HIGH!\n" );

        extIrq0 = 1;

        /* Reconfigure ExtIrq0 to be level LOW */
        irq_set_irq_type( AVALANCHE_INT_84, IRQF_TRIGGER_LOW );
    }
    peOccurred = 1;

    /* Wake any userspace thread */
    wake_up(&usWaitQ);

    /* Puma6 - ack the IRQ */
    ack_irq(AVALANCHE_INT_84);

    return IRQ_HANDLED;
}

int __init ups_init_module (void)
{
    int rc;

    rc = register_chrdev (TLM_MAJOR, DEVICE_NAME, &tlm_fops);

    if (rc < 0)
    {
        printk ("TLM: Error %d registering TLM driver\n", rc);
        return rc;
    }

    TLM_init();

    TLM_initComplete = TRUE;

#if defined (CONFIG_VENDOR_ARRIS_P6MG) || defined (CONFIG_VENDOR_ARRIS_TG1682)
    /* First clear the IRQ in order not to get a false interrupt since INTD is level */
    ack_irq(AVALANCHE_INT_84); 
    if( request_threaded_irq(AVALANCHE_INT_84, NULL, test_irq, 
                             IRQF_TRIGGER_LOW | IRQF_ONESHOT, 
                             "TLM_EXTIRQ0", NULL) )
    {
        printk( KERN_ERR " unable to get IRQ #%d for TLM!\n", AVALANCHE_INT_84);
    }
#endif

    printk("TLM: driver registered. platform is %d, Charger is %s\n", 
                    platform, (charger == 0) ? "ZILOG" : ((charger == 1)? "ATMEL" : "TI" ));

    
	return 0;
}


void __exit ups_cleanup (void)
{   
    unregister_chrdev(TLM_MAJOR,DEVICE_NAME);
}

module_init (ups_init_module);
module_exit (ups_cleanup);

