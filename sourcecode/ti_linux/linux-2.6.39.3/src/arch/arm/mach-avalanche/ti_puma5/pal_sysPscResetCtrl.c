/*
 *
 * pal_sysPscResetCtrl.c 
 * Description:
 * see below
 *
 *
 * Copyright (C) 2008, Texas Instruments, Incorporated
 *
 *  This program is free software; you can distribute it and/or modify it
 *  under the terms of the GNU General Public License (Version 2) as
 *  published by the Free Software Foundation.
 *
 *  This program is distributed in the hope it will be useful, but WITHOUT
 *  ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or
 *  FITNESS FOR A PARTICULAR PURPOSE.  See the GNU General Public License
 *  for more details.
 *
 *  You should have received a copy of the GNU General Public License along
 *  with this program; if not, write to the Free Software Foundation, Inc.,
 *  59 Temple Place - Suite 330, Boston MA 02111-1307, USA.
 *
 */
/** \file   pal_sysPscResetCtrl.c
 *  \brief  PAL reset control APIs
 *  
 *  \author     PSP, TI
 *
 *  \note       Set tabstop to 4 (:se ts=4) while viewing this file in an
 *              editor
 *
 *  \version    0.1     Ajay Sing   		Created
 *              0.2     Mansoor Ahamed		For Avalanche3 architecture
 */

#include <asm-arm/arch-avalanche/generic/pal.h>
#include <asm-arm/arch-avalanche/generic/pal_sysPsc.h>
/* UNIHAN ADD START PD217906 */
#include <linux/proc_fs.h>
#include <linux/init.h>
int pal_sys_reset_status_read(char* page, char **start, off_t offset, int count,int *eof, void *data);
/* UNIHAN ADD END */
REMOTE_VLYNQ_DEV_RESET_CTRL_FN p_remote_vlynq_dev_reset_ctrl = NULL;
extern void avalanche_system_reset(PAL_SYS_SYSTEM_RST_MODE_T mode);
/*****************************************************************************
 * Reset Control Module.
 *****************************************************************************/
/*! \fn void PAL_sysResetCtrl(unsigned int module_reset_bit, PAL_SYS_RESET_CTRL_T reset_ctrl)
    \brief This API is used to assert or de-assert reset for a module
    \param module_reset_bit Unique module id to assert/de-assert reset
    \param reset_ctrl assert/de-assert reset (IN_RESET, OUT_OF_RESET)
*/
void PAL_sysResetCtrl(unsigned int module_reset_bit, PAL_SYS_RESET_CTRL_T reset_ctrl)
{
	PAL_SYS_PSC_MODULE_T module_id = (PAL_SYS_PSC_MODULE_T)(module_reset_bit);

/*HPVL*/
#if 0
    if(module_id == PSC_VLYNQ)
    {
        if(p_remote_vlynq_dev_reset_ctrl)
		{
            p_remote_vlynq_dev_reset_ctrl(module_reset_bit, reset_ctrl);
		}
		else
		{
            return;
		}
    }
#endif

    if(reset_ctrl == OUT_OF_RESET)
	{
		PAL_sysPscSetModuleState(module_id, PSC_ENABLE);
	}
    else
	{
		PAL_sysPscSetModuleState(module_id, PSC_SW_RST_DISABLE);
	}

}

/*! \fn PAL_SYS_RESET_CTRL_T PAL_sysGetResetStatus(unsigned int module_reset_bit)
    \brief This API returns the status reset status of a module
    \param module_reset_bit Unique module id whose reset status has to be read
    \return Reset assert/de-assert (IN_RESET, OUT_OF_RESET)
*/
PAL_SYS_RESET_CTRL_T PAL_sysGetResetStatus(unsigned int module_reset_bit)
{
	PAL_SYS_PSC_MODULE_T module_id = (PAL_SYS_PSC_MODULE_T)(module_reset_bit);
	PAL_SYS_PSC_MODINFO_T info;

	PAL_sysPscGetModuleInfo(module_id, &info);
	
	if(info.state == PSC_ENABLE)
			return OUT_OF_RESET;
	else
			return IN_RESET;
}

/*! \fn void PAL_sysSystemReset(PAL_SYS_SYSTEM_RST_MODE_T mode)
    \brief This API is used the reset the system
    \param mode system reset mode
*/
void PAL_sysSystemReset(PAL_SYS_SYSTEM_RST_MODE_T mode)
{
	/* This is processor specific so should be implemented in avalanche_misc.c */
	avalanche_system_reset(mode);
}

/*! \fn PAL_SYS_SYSTEM_RESET_STATUS_T PAL_sysGetSysLastResetStatus()
    \brief This API is used to retrieve the reason for last system reset
    \return last system reset status 
			HARDWARE_RESET,
			SOFTWARE_RESET0,
			WATCHDOG_RESET,
			SOFTWARE_RESET1)
*/
PAL_SYS_SYSTEM_RESET_STATUS_T PAL_sysGetSysLastResetStatus()
{
    volatile unsigned int *sys_reset_status = (unsigned int*) AVALANCHE_RST_CTRL_RSTYPE_BASE;
	PAL_SYS_SYSTEM_RESET_STATUS_T mode;

	for(mode = (PAL_SYS_SYSTEM_RESET_STATUS_T)(0); mode < RST_STAT_END; mode++){
		if((*sys_reset_status & (1 << mode)))
				break;
	}
    return mode;
}

/* UNIHAN ADD START PD217906 */
/*! \fn int __init pal_sys_init(void) 
   \brief Procfs call to be able to retrieve the reset status from user space
          Called at bootup by using the fs_initcall macro.
   \return Always 0.
*/
int __init pal_sys_init(void)
{
    create_proc_read_entry("reset_status",0,NULL,pal_sys_reset_status_read,NULL);

    return 0;

}

/*! \fn int pal_sys_reset_status_read(char* page, char **start, off_t offset, int count,int *eof, void *data)
    \brief procfs read function.  Will be called every time /proc/reset_status "file" is read.
           Based on testing in the lab,  PAL_sysGetSysLastResetStatus returns a value of WATCHDOG_RESET(2)
           for software (!reset, docsDevReset,download) and watchdog resets.  For that reason, the text
           to be displayed for a "WATCHDOG_RESET" is "SW Reset".  At this time there is no way to
           distinguish between a software and a watchdog reset. HARDWARE_RESET was confirmed to be returned
           on Power-On and push-botton resets.
    ** NOTE ** User space code expects the reset cause to be less than 30 characters.  Use caution if
    changing the string size.
    \return length of string written to /proc/reset_status
*/
int pal_sys_reset_status_read(char* page, char **start, off_t offset, int count,int *eof, void *data)
{
    int len=0;
    PAL_SYS_SYSTEM_RESET_STATUS_T resetReason = RST_STAT_END;

    resetReason = PAL_sysGetSysLastResetStatus();

    switch(resetReason)
    {
        case HARDWARE_RESET:
            /* Power cycle or Reset Button*/
            len+= sprintf(page+len, "Power-On");
            break;

        case SOFTWARE_RESET0:             
            len+= sprintf(page+len, "SW Reset SWR0");
            break;

        case SOFTWARE_RESET1:             
            len+= sprintf(page+len, "SW Reset SWR1");
            break;

        case WATCHDOG_RESET:             
            /* Software resets from CLI, mibs, downloads */
            len+= sprintf(page+len, "SW Reset");
            break;

        default:
            len+= sprintf(page+len, "Unknown Reset");
            break;
    }

    *eof = 1;
    return len;
}

fs_initcall(pal_sys_init);

/* UNIHAN ADD END */
