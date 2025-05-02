/*
 * File Name: dimhw_gw.h
 *
 * Description: The file defines types used by user space wrappers for 
 *              DIMHW functions implemented in hwdsp_mod kernel module.
 *
 * Copyright (C) 2011 Texas Instruments, Incorporated
 */

#ifndef _DIMHW_GW_H_
#define _DIMHW_GW_H_

/* Puma6 specific definitions */
#define PUMA6_DSP_MEM_BASE         	0x04000000
#define PUMA6_DSP_REGISTER_BASE    	0x05000000
#define PUMA6_EXT_ADDR_CFG_REG     	(PUMA6_DSP_REGISTER_BASE + 0x04)
#define PUMA6_INT_POLARITY_REG     	(PUMA6_DSP_REGISTER_BASE + 0x34)
#define PUMA6_DSP_EXTERNAL_MEM_SIZE	0x00200000
#define PUMA6_NMI_REG              	0x00040008

/* Puma5 specific definitions */
#define PUMAV_DSP_MEM_BASE         	0x04000000
#define PUMAV_DSP_REGISTER_BASE    	0x05000000
#define PUMAV_EXT_ADDR_CFG_REG     	(PUMAV_DSP_REGISTER_BASE + 0x04)
#define PUMAV_INT_POLARITY_REG     	(PUMAV_DSP_REGISTER_BASE + 0x34)
#define PUMAV_DSP_EXTERNAL_MEM_SIZE	0x00200000
#define PUMAV_NMI_REG              	0x08611A34

#define DSPMOD_DEVICE_NAME "tidspmod"

/*
 * GW_DSPMOD_PARAM_T defines a type of 'param' of ioctl system call
 * for hwdsp_mod kernel module
*/
typedef union  GW_DSPMOD_PARAM_tag
{
    struct DSPMOD_PARAM_tag
    {
        unsigned short     dsp_num;
        unsigned short     bool_value; /* 0 => FALSE, 1 => TRUE */
    } u;
    unsigned long v;
} GW_DSPMOD_PARAM_T;

/* dspmod ioctl definitions for puma-5 */

#define DSPMOD_IOCTL_MAGIC 'D'

#define DSPMOD_IOCTL_QUERY_DSP_TYPE   _IOR(DSPMOD_IOCTL_MAGIC, 0, unsigned long)
#define DSPMOD_IOCTL_DSP_HALT         _IOW(DSPMOD_IOCTL_MAGIC, 1, unsigned short)
#define DSPMOD_IOCTL_QUERY_DSP_NUM    _IOR(DSPMOD_IOCTL_MAGIC, 2, unsigned long)
#define DSPMOD_IOCTL_QUERY_DSP_ADDR   _IOR(DSPMOD_IOCTL_MAGIC, 3, unsigned long)
#define DSPMOD_IOCTL_DSP_RESET        _IOW(DSPMOD_IOCTL_MAGIC, 4, unsigned long)
#define DSPMOD_IOCTL_DSP_NMI          _IOW(DSPMOD_IOCTL_MAGIC, 5, unsigned long)
#define DSPMOD_IOCTL_DSP_PWR          _IOW(DSPMOD_IOCTL_MAGIC, 6, unsigned long)
#define DSPMOD_IOCTL_RESET_TDM        _IOW(DSPMOD_IOCTL_MAGIC, 7, unsigned long)
#define DSPMOD_IOCTL_REG_READ	      _IOR(DSPMOD_IOCTL_MAGIC, 8, unsigned long)

/* Following ioctl's are not supported for Puma-5. */
#define DSPMOD_IOCTL_DSP_INT          _IO(DSPMOD_IOCTL_MAGIC, 7)
#define DSPMOD_IOCTL_DSP_LOOP         _IO(DSPMOD_IOCTL_MAGIC, 8)

#define DSPMOD_DEV_IOC_MAXNR 8

#endif /* _DIMHW_GW_H_ */
