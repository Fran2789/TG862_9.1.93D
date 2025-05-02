/*
 *
 *  mpeg_encap_driver.c
 * Description:
 * MPEG Encapsulation system calls implementation
 *
 * Copyright (C) 2009 Texas Instruments Incorporated - http://www.ti.com/ 
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

/***************************************************************************/

/*! \file mpeg_encap_driver.c
    \brief Implementation of MPEG frames encapsulate driver. 
    \code.
*/

/****************************************************************************/

/**************************************************************************/
/*      INCLUDES                                                          */
/**************************************************************************/
#include <linux/interrupt.h>
#include <linux/proc_fs.h>
#include <linux/if_ether.h>
#include <net/ip.h>
#include <linux/udp.h>
#include <linux/inet.h>
#include <puma_autoconf.h>
#include <puma.h>
#include <hardware.h>
#include <pal.h>
#include <pal_cppi41.h>

#if PUMA6_OR_NEWER_SOC_TYPE
#include <puma6_cppi.h>
#else
#include <puma5_cppi.h>
#endif

#include "_tistdtypes.h"
#include "mpeg_encap_driver.h"
#include "mpeg_encap_prv.h"

/**************************************************************************/
/*      DEFINES                                                           */
/**************************************************************************/

static long mpegEncapDriverIoctl(struct file *file,
                                unsigned int ioctl_num,
                                unsigned long ioctl_param,
                                struct semaphore *soc_sem);
static void mpegEncapDriverInit(struct semaphore *soc_sem);
static void mpegEncapDriverCleanup(struct semaphore *soc_sem);
static Int32 mpegEncapFindFreeOutStreamEntry(Uint32 *out_stream_idx);
static Bool  mpegEncapIsOutStreamEnabled(Uint32 out_stream_idx);
static Int32 mpegEncapFrameModAddRule( SoCMPEGEncapPidOutStream_t *mpegEncapFrameModify );
static Int32 mpegEncapFrameModDelRule( SoCMPEGEncapPidOutStream_t *mpegEncapFrameModify );
static Int32 mpegEncapGetAndClearPidActivate(Uint32 chn_idx, void * user_ptr);

/************************************************************************/
/*     driver private data                                          */
/************************************************************************/

static SoCDriverOperations_t mpeg_encap_operations =  
{
   .socModuleID   = SOC_MPEG_ENCAP_MODULE_ID,
   .soc_ioctl     = &mpegEncapDriverIoctl, 
   .soc_release   = NULL,
   .soc_open      = NULL,
   .soc_cleanup   = mpegEncapDriverCleanup,
   .soc_init      = mpegEncapDriverInit
};

static mpeg_process_private* priv = NULL;

/**************************************************************************/
/*      STATIC  Functions                                                 */
/**************************************************************************/


/**************************************************************************/
/*! \fn static int   mpegEncapDriverIoctl(struct file *file,
                                          unsigned int ioctl_num, unsigned long ioctl_param)
 **************************************************************************
 *  \brief This function is called whenever a process tries to do an ioctl to our
 * SoC driver file. We get two extra parameters (additional to the inode and file
 * structures, which all device functions get): the number of the ioctl called
 * and the parameter given to the ioctl function.
 *
 * If the ioctl is write or read/write (meaning output is returned to the
 * calling process), the ioctl call returns the output of this function.
 * _IOC macros are used to decode ioctl numbers:
 *    _IOC_DIR(nr)  - get direction number 
 *    _IOC_TYPE(nr) - get type number 
 *    _IOC_NR(nr)   - get ordinal (sequential) number 
 *    _IOC_SIZE(nr) - get size number 
 *  \return 0 on success otherwise ioctl error code.
 **************************************************************************/
static long mpegEncapDriverIoctl(struct file *file,
                                unsigned int ioctl_num,
                                unsigned long ioctl_param,
                                struct semaphore *soc_sem)
{
    /*
     * extract the type and number bitfields, and don't decode
     * wrong cmds: return ENOTTY (inappropriate ioctl) before access_ok(  )
     */
    if (_IOC_TYPE(ioctl_num) != SOC_MPEG_ENCAP_MODULE_ID)
        return -ENOTTY;
    if (_IOC_NR(ioctl_num) > SOC_MPEGE_IOCTL_MAXNR)
        return -ENOTTY;

    /* */
    down(soc_sem);
    /* 
      * Switch according to the ioctl called 
      */
    switch (ioctl_num)
    {
        case SOC_MPEGE_UNIT_ENABLE:
            /* 
             * This IOCTL command should get out of reset the MPEG_ENCAP SoC unit. */
            mpegEncapUnitEnable();
            break;

        case SOC_MPEGE_UNIT_DISABLE:
            /* 
             * This IOCTL command should reset the MPEG_ENCAP SoC unit. */
            mpegEncapUnitDisable();
            break;

        case SOC_MPEGE_PORT_ENCAP_OUTSTREAM:
            {
                SoCMPEGEncapHeaderInfo_t mpegEncapHeaderInfo;

                /* 
                   * This IOCTL command should configure the MPEG Encap Header Info.
                   * 1. configure the MPEG Encap header according to ioctl_param (pointer to SoCMPEGEncapHeaderInfo_t). 
                   */
                if ( copy_from_user((void *)(&mpegEncapHeaderInfo),
                                    (void *)ioctl_param,
                                    sizeof(SoCMPEGEncapHeaderInfo_t)) )
                {
                    printk( "mpegEncapDriverIoctl:: Invalid argument for MPEG Encap header info.\n");
                    up(soc_sem);
                    return -EFAULT;
                }

				if (mpegEncapHeaderInfo.action == MPEG_ENCAP_CFG_ADD)
				{
					if (mpegEncapAddOutStream( &mpegEncapHeaderInfo ) != 0)
					{
						printk( "mpegEncapDriverIoctl:: Invalid argument for MPEG Encap header info.\n");
						up(soc_sem);
						return -EINVAL;
					}
	
					/*return assigned IP stream index to the user*/
					if (copy_to_user((void *)(&((SoCMPEGEncapHeaderInfo_t *)ioctl_param)->out_stream_idx), 
                                     (void *)(&mpegEncapHeaderInfo.out_stream_idx), 
                                     sizeof(mpegEncapHeaderInfo.out_stream_idx)))
					{
						printk("mpegEncapDriverIoctl:: failed to copy to IP stream index to user\n");
						up(soc_sem);
						return -EFAULT;
					}
				}
				else
				{
					if (mpegEncapRemOutStream( &mpegEncapHeaderInfo ) != 0)
					{
						printk( "mpegEncapDriverIoctl:: Invalid argument for MPEG Encap header info.\n");
						up(soc_sem);
						return -EINVAL;
					}
				}

            }
            break;

        case SOC_MPEGE_PORT_SET_RTP:
            {
                SoCMPEGEncapRtpTsInfo_t mpegEncapRtpInfo;

                /* 
                   * This IOCTL command should configure the MPEG Encap Header Info.
                   * 1. configure the MPEG Encap header according to ioctl_param (pointer to SoCMPEGEncapHeaderInfo_t). 
                   */
                if ( copy_from_user((void *)(&mpegEncapRtpInfo),
                                    (void *)ioctl_param,
                                    sizeof(SoCMPEGEncapRtpTsInfo_t)) )
                {
                    printk( "mpegEncapDriverIoctl:: Invalid argument for MPEG Encap rtp info.\n");
                    up(soc_sem);
                    return -EINVAL;
                }

                mpegEncapPortSetRtpTs( &mpegEncapRtpInfo );
            }
            break;

        case SOC_MPEGE_PORT_PID_DISCARD:
            /* 
               * This IOCTL command should close an MPEG Encap port.
               * 1. Open MPEG Out ports according to the ioctl_param (port number 1-4). 
               */
            if ( mpegEncapPortSetPidOp( (Uint8)(ioctl_param&0xFF), (Uint16)((ioctl_param>>8)&0xFFFF), MPEG_ENCAP_PID_DISCARD ) < 0 )
            {
                up(soc_sem);
                return -EINVAL;
            }
            break;

        case SOC_MPEGE_PORT_PID_FF:
            /* 
             * This IOCTL command should open an MPEG Encap port.
             */
            if ( mpegEncapPortSetPidOp( (Uint8)(ioctl_param&0xFF), (Uint16)((ioctl_param>>8)&0xFFFF), MPEG_ENCAP_PID_FF ) < 0 )
            {
                up(soc_sem);
                return -EINVAL;
            }
            break;

        case SOC_MPEGE_PORT_PID_HF:
            /* 
             * This IOCTL command should open an MPEG Encap port.
             */
            if ( mpegEncapPortSetPidOp( (Uint8)(ioctl_param&0xFF), (Uint16)((ioctl_param>>8)&0xFFFF), MPEG_ENCAP_PID_HF ) < 0 )
            {
                up(soc_sem);
                return -EINVAL;
            }
            break;

        case SOC_MPEGE_PORT_PID_FF_HF:
            /* 
             * This IOCTL command should open an MPEG Encap port.
             */
            if ( mpegEncapPortSetPidOp( (Uint8)(ioctl_param&0xFF), (Uint16)((ioctl_param>>8)&0xFFFF), MPEG_ENCAP_PID_FF_HF ) < 0 )
            {
                up(soc_sem);
                return -EINVAL;
            }
            break;

        case SOC_MPEGE_PORT_PID_FRAME_MODIFY:
            {
                SoCMPEGEncapPidOutStream_t mpegEncapFrameModify;

                if ( copy_from_user((void *)(&mpegEncapFrameModify),
                                    (void *)ioctl_param,
                                    sizeof(SoCMPEGEncapPidOutStream_t)) )
                {
                    printk( "mpegEncapDriverIoctl:: Invalid argument for MPEG Encap header info.\n");
                    up(soc_sem);
                    return -EINVAL;
                }

                if ( mpegEncapPortFrameModify( &mpegEncapFrameModify ) < 0 )
                {
                    up(soc_sem);
                    return -EINVAL;
                }
            }
            break;
		case SOC_MPEGE_PORT_ASSOC_PID_OUTSTREAM:
			{
				SoCMPEGEncapPidOutStream_t pidEncap;

				if ( copy_from_user((void *)(&pidEncap),
									(void *)ioctl_param,
									sizeof(SoCMPEGEncapPidOutStream_t)) )
				{
					printk( "mpegEncapDriverIoctl:: Invalid argument for MPEG PID Out stream association.\n");
					up(soc_sem);
					return -EINVAL;
				}

				if (pidEncap.action == MPEG_ENCAP_CFG_ADD)
				{
					if ( mpegEncapAddPidToOutStream( &pidEncap ) != 0 )
					{
						up(soc_sem);
						return -EINVAL;
					}
				}
				else 
				{
					if ( mpegEncapRemPidFromOutStream( &pidEncap ) != 0 )
					{
						up(soc_sem);
						return -EINVAL;
					}
				}

			}
			break;
		case SOC_MPEGE_PORT_GET_PID_FWD_OP:
			{
				SoCMPEGEncapPidOp_t pidOp;

				/*Get input parameters: port and PID*/
				if ( copy_from_user((void *)(&pidOp),
									(void *)ioctl_param,
									sizeof(SoCMPEGEncapPidOp_t)) )
				{
					printk( "mpegEncapDriverIoctl:: Invalid argument for MPEG PID FWD option get ioctl.\n");
					up(soc_sem);
					return -EINVAL;
				}

				if ( mpegEncapPortGetPidOp( pidOp.port, pidOp.pid, &pidOp.fwd_op ) != 0 )
                {
                    up(soc_sem);
					printk( "mpegEncapDriverIoctl:: Failed to get PID FWD option.\n");
                    return -EINVAL;
                }

				/*Return the PID option*/
				if (copy_to_user((void *)ioctl_param, &pidOp, sizeof(SoCMPEGEncapPidOp_t)))
				{
					printk("mpegEncapDriverIoctl:: failed to copy PID FWD option to user\n");
					up(soc_sem);
					return -EFAULT;
				}

			}
			break;
        case SOC_MPEGE_PORT_GET_PID_OUTSTREAMS:
			{
				SoCMPEGEncapPidOutStream_t pidOutStreams;
	
				/*Get input parameters: port and PID*/
				if ( copy_from_user((void *)(&pidOutStreams),
									(void *)ioctl_param,
									sizeof(SoCMPEGEncapPidOutStream_t)) )
				{
					printk( "mpegEncapDriverIoctl:: Invalid argument for MPEG PID IP streams get ioctl.\n");
					up(soc_sem);
					return -EINVAL;
				}
	
				if ( mpegEncapGetPidAssocOutStreams( &pidOutStreams ) != 0 )
				{
					up(soc_sem);
					printk( "mpegEncapDriverIoctl:: Failed to get MPEG PID associated IP streams.\n");
					return -EINVAL;
				}
	
				/*Return the PID option*/
				if (copy_to_user((void *)ioctl_param, &pidOutStreams, sizeof(SoCMPEGEncapPidOutStream_t)))
				{
					printk("mpegEncapDriverIoctl:: failed to copy PID associated IP streams to user\n");
					up(soc_sem);
					return -EFAULT;
				}
	
			}
			break;
        case SOC_MPEGE_PORT_GET_PID_ACTIVITY:
         {
            Uint32 ds_port;
          
            /*Get input parameters: DS port */
				if ( copy_from_user((void *)(&ds_port),
                                (void *)(&((SoCMPEGEncapPidActivity_t *)ioctl_param)->ds_port),
                                sizeof(ds_port)) )
				{
					printk( "mpegEncapDriverIoctl:: Invalid argument for MPEG PID Activity get ioctl.\n");
					up(soc_sem);
					return -EINVAL;
				}
            
            if (mpegEncapGetAndClearPidActivate(HAL_PORT_ID_2_CHANNEL_INDEX(ds_port), 
                                                (void *)(&((SoCMPEGEncapPidActivity_t *)ioctl_param)->is_activityDetected[0])))
            {
                printk( "mpegEncapDriverIoctl:: mpegEncapGetAndClearPidActivate Failed.\n");
                up(soc_sem);
					 return -EINVAL;
            }
         }
         break;
        default:  /* redundant, as cmd was checked against MAXNR */
            up(soc_sem);
            return -ENOTTY;
    }
    up(soc_sem);
    return 0;
}

/*******************************************************************************************/
/*! \fn static Int32 mpegEncapGetAndClearPidActivate(Uint32 chn_idx, void * user_ptr)
 *******************************************************************************************
 *  \brief Copy PID activity Indications from DS chn_idx to user data.
 *  \param[in]   chn_idx - DS channel index.
 *  \param[out]  ser_ptr - user pointer (must use copy_to_user) to copy data to.
 *  \return 0 in case of success, error code otherwise.
 **************************************************************************/
static Int32 mpegEncapGetAndClearPidActivate(Uint32 chn_idx, void * user_ptr)
{
    Uint32 idx;

    if (priv == NULL)
        return -1;

    if ((MAX_MPEG_CHNS-1)<chn_idx)
        return -1;

    /* Copy data to user */
    if (copy_to_user(user_ptr, 
                     (void *)&(priv->pid_filters[chn_idx][0]), 
                     PID_SPACE))
    {
       printk("mpegEncapGetAndClearPidActivate:: failed to copy MPEG PID Activity to user\n");
       return -1;
    }

    /* Clear Indications */
    for (idx = 0; idx < PID_SPACE; idx++)
        if( IS_PID_ACTIVITY_ON(priv->pid_filters[chn_idx][idx].filter) )
            SET_PID_ACTIVITY_OFF(priv->pid_filters[chn_idx][idx].filter);
    return 0;
}

/**************************************************************************/
/*! \fn static void socDocsisGlobalDriverInit(struct semaphore *soc_sem)
 **************************************************************************
 *  \brief Perform the required initialization of the SoC DOCSIS Global level.
 *  \      This function is envoked at the installation of the driver.
 *  \param[in]  *soc_sem - pointer to the driver's semaphore.
 *  \return none.
 **************************************************************************/
static void mpegEncapDriverInit(struct semaphore *soc_sem)
{
    down(soc_sem);

    priv = (mpeg_process_private*)kmalloc(sizeof(*priv), GFP_KERNEL);
	if (!priv)
	{
		printk("MPEG ENCAP Init failed: couldn't kmalloc private DB structure of size %d\n", sizeof(*priv));
		return;
	}
    memset(priv, 0, sizeof(*priv));

    mpegEncapUnitInit(priv);

    up(soc_sem);
    return;
}

/**************************************************************************/
/*! \fn static void socDocsisGlobalDriverInit(struct semaphore *soc_sem)
 **************************************************************************
 *  \brief Perform the required initialization of the SoC DOCSIS Global level.
 *  \      This function is envoked at the installation of the driver.
 *  \param[in]  *soc_sem - pointer to the driver's semaphore.
 *  \return none.
 **************************************************************************/
static void mpegEncapDriverCleanup(struct semaphore *soc_sem)
{
    down(soc_sem);

    mpegEncapUnitCleanup(priv);
    kfree(priv);

    up(soc_sem);
    return;
}

/**************************************************************************/
/*! \fn static Int32 mpegEncapFindFreeOutStreamEntry(Uint32 *out_stream_idx)
 **************************************************************************
 *  \brief Find free IP stream entry
 *  \param[out]  *out_stream_idx - free entry index
 *  \return 0 in case of success, error code otherwise.
 **************************************************************************/
static Int32 mpegEncapFindFreeOutStreamEntry(Uint32 *out_stream_idx)
{
	int i;

	for (i=0; i<MAX_MPEG_ENCAP_OUT_STREAMS; i++)
	{
		if (!priv->encap_out_streams[i].enabled)
		{
			*out_stream_idx = i;
			return 0;
		}
	}
	return -1;
}

/*******************************************************************************************/
/*! \fn static Int32 mpegEncapActivatePid(Uint32 chn_idx, Uint32 pid, Uint32 *active_pid_idx)
 *******************************************************************************************
 *  \brief Reset active PID entry in the active PIDs array on the channel
 *  \param[in]   chn_idx - channel index
 *  \param[out]  *active_pid_idx - free entry index
 *  \return 0 in case of success, error code otherwise.
 **************************************************************************/
static Int32 mpegEncapResetActivePidEntry(Uint32 chn_idx, Uint32 active_pid_idx)
{
	Uint32 outstream_idx;
	mpeg_modified_frame **frame_manip = priv->encap_pid_params[chn_idx][active_pid_idx].frame_manip;

	if (frame_manip)
	{
		for (outstream_idx=0; outstream_idx < MAX_MPEG_ENCAP_OUT_STREAMS; outstream_idx++)
		{
			if (frame_manip[outstream_idx])
				kfree(frame_manip[outstream_idx]);
		}
		kfree(frame_manip);
	}

	memset(&priv->encap_pid_params[chn_idx][active_pid_idx], 0, sizeof(mpeg_encap_pid_t));
	priv->encap_pid_params[chn_idx][active_pid_idx].pid = MPEG_INVALID_PID;

	return 0;
}


/*******************************************************************************************/
/*! \fn static Int32 mpegEncapActivatePid(Uint32 chn_idx, Uint32 pid, Uint32 *active_pid_idx)
 *******************************************************************************************
 *  \brief Allocate an entry for the PID in the active PIDs array on the channel
 *  \param[in]   chn_idx - channel index
 *  \param[in]   pid -     PID
 *  \param[out]  *active_pid_idx - free entry index
 *  \return 0 in case of success, error code otherwise.
 **************************************************************************/
static Int32 mpegEncapActivatePid(Uint32 chn_idx, Uint32 pid, Uint32 *active_pid_idx)
{
	Uint32 idx; 

	/*Search for free entry in channel active PIDs array*/
	for (idx = 0; idx < MAX_MPEG_ACTIVE_PIDS; idx++)
	{
		if (priv->encap_pid_params[chn_idx][idx].pid == MPEG_INVALID_PID)
		{
			*active_pid_idx = idx;
			SET_ACTIVE_PID_INDEX(priv->pid_filters[chn_idx][pid].filter, idx);
			priv->encap_pid_params[chn_idx][idx].pid = pid;
			break;
		}
	}
	if (idx == MAX_MPEG_ACTIVE_PIDS)
	{
		printk("Failed to activate PID %u on channel %u forwarding: database is full\n", pid, chn_idx);
		return -1;
	}
	return 0;
}

/*******************************************************************************************/
/*! \fn static Int32 mpegEncapDeactivatePid(Uint32 chn_idx, Uint32 pid, Uint32 active_pid_idx)
 *******************************************************************************************
 *  \brief Deactivate  PID 
 *  \param[in]   chn_idx - channel index
 *  \param[in]   pid -     PID
 *  \param[in]   active_pid_idx - active PID index
 *  \return 0 in case of success, error code otherwise.
 **************************************************************************/
static Int32 mpegEncapDeactivatePid(Uint32 chn_idx, Uint32 pid, Uint32 active_pid_idx)
{
	mpegEncapResetActivePidEntry(chn_idx, active_pid_idx);

   SET_PID_ACTIVITY_OFF(priv->pid_filters[chn_idx][pid].filter);
   SET_PID_OPERATION_FF(priv->pid_filters[chn_idx][pid].filter, 0);
   SET_ACTIVE_PID_INDEX(priv->pid_filters[chn_idx][pid].filter, INVALID_ACTIVE_PID_IDX);
    
	return 0;
}

/*******************************************************************************************/
/*! \fn static Int32 mpegEncapDisassocPidFromOutstream(Uint32 chn_idx, Uint32 active_pid_idx, Uint32 outstream_idx)
 *******************************************************************************************
 *  \brief Disassociate a PID  from an IP stream
 *  \param[in]   chn_idx - channel index
 *  \param[in]   active_pid_idx - active PID index
 *  \param[in]   outstream_idx - IP stream index
 *  \return 0 in case of success, error code otherwise.
 **************************************************************************/
static Int32 mpegEncapDisassocPidFromOutstream(Uint32 chn_idx, Uint32 active_pid_idx, Uint32 outstream_idx)
{
	/*If this PID is associated with this IP stream - perform the disassociation*/
	if (priv->encap_pid_params[chn_idx][active_pid_idx].out_stream_map[outstream_idx])
	{
		priv->encap_pid_params[chn_idx][active_pid_idx].out_stream_map[outstream_idx] = 0;
		priv->encap_pid_params[chn_idx][active_pid_idx].num_assoc_outstreams--;
		/*If there are no more associated IP streams - deactivate the PID and disable its forwarding option*/
		if (!priv->encap_pid_params[chn_idx][active_pid_idx].num_assoc_outstreams)
		{
			mpegEncapDeactivatePid(chn_idx, priv->encap_pid_params[chn_idx][active_pid_idx].pid, active_pid_idx);
		}
	}

	return 0;
}
/**************************************************************************/
/*! \fn static Bool mpegEncapIsOutStreamEnabled(Uint32 out_stream_idx)
 **************************************************************************
 *  \brief Check if IP stream is enabled
 *  \param[in]  out_stream_idx - IP stream entry index
 *  \return True/False.
 **************************************************************************/
static Bool mpegEncapIsOutStreamEnabled(Uint32 out_stream_idx)
{
	if (out_stream_idx >= MAX_MPEG_ENCAP_OUT_STREAMS)
		return False;

	if (!priv->encap_out_streams[out_stream_idx].enabled)
		return False;
	else 
		return True;
}

/**************************************************************************/
/*! \fn static Int32 mpegEncapFrameModAddRule( SoCMPEGEncapPidOutStream_t *mpegEncapFrameModify )
 **************************************************************************
 *  \brief Add frame modification rules set
 *         In case that PID is inactive (FF option is disabled) - activates the PID
 *         In case that PID is not associated with the given IP stream - associates it
 *  \param[in] SoCMPEGEncapPidOutStream_t *mpegEncapFrameModify - input parameters structure
 *  \return  0 or error code
 */
static Int32 mpegEncapFrameModAddRule( SoCMPEGEncapPidOutStream_t *mpegEncapFrameModify )
{
	Uint8 chn_idx       = HAL_PORT_ID_2_CHANNEL_INDEX(mpegEncapFrameModify->port);
	Uint32              active_pid_idx; 
	mpeg_modified_frame **frame_manip;

    /*Associate the PID with the given IP stream and activate the PID if required*/
	if (mpegEncapAddPidToOutStream(mpegEncapFrameModify))
	{
		printk("%s: Failed to associate PID %u with IP stream %u\n", __FUNCTION__, mpegEncapFrameModify->pid,
			   mpegEncapFrameModify->outstream_idx);
		return -1;
	}

	active_pid_idx = (Uint32)GET_ACTIVE_PID_INDEX(priv->pid_filters[chn_idx][mpegEncapFrameModify->pid].filter);
	if (active_pid_idx == INVALID_ACTIVE_PID_IDX)
	{
		printk("%s: Failed to define frame modification rules for PID %u on IP stream %u: PID is inactive\n",
			    __FUNCTION__, mpegEncapFrameModify->pid, mpegEncapFrameModify->outstream_idx);
		return -1;
	}


	/*if no frame modification rules at all are defined for this PID - allocate rules array */
    if (!priv->encap_pid_params[chn_idx][active_pid_idx].frame_manip)
	{
		frame_manip = (mpeg_modified_frame**)kmalloc(MAX_MPEG_ENCAP_OUT_STREAMS*sizeof(mpeg_modified_frame*), GFP_KERNEL);
		if (!frame_manip)
		{
			printk("Failed to allocate dynamic memory for frame manipulation array\n");
			return -1;
		}
		memset(frame_manip, 0, MAX_MPEG_ENCAP_OUT_STREAMS*sizeof(mpeg_modified_frame*));
	}
	else
	{
		frame_manip = priv->encap_pid_params[chn_idx][active_pid_idx].frame_manip;
	}

	/*if no frame modification rules  are defined for this PID for this particular IP stream */
	if (!frame_manip[mpegEncapFrameModify->outstream_idx])
	{
		frame_manip[mpegEncapFrameModify->outstream_idx] = (mpeg_modified_frame*)kmalloc(sizeof(mpeg_modified_frame), GFP_KERNEL);
		if (!frame_manip[mpegEncapFrameModify->outstream_idx])
		{
			if( !priv->encap_pid_params[chn_idx][active_pid_idx].frame_manip )
            {
                /* Free the preallocated frame manipulation buffer */
                kfree( frame_manip );
            }
            printk("Failed to allocate dynamic memory for frame manipulation rule\n");
			return -1;
		}
		memset(frame_manip[mpegEncapFrameModify->outstream_idx], 0, sizeof(mpeg_modified_frame));

	}

    frame_manip[mpegEncapFrameModify->outstream_idx]->frame_offset = mpegEncapFrameModify->frame_mod.frame_offset;
    frame_manip[mpegEncapFrameModify->outstream_idx]->new_frame_length = mpegEncapFrameModify->frame_mod.new_frame_length;
    memcpy(&(frame_manip[mpegEncapFrameModify->outstream_idx]->new_frame[0]), mpegEncapFrameModify->frame_mod.new_frame, MPEG2TS_LEN);

    if (!priv->encap_pid_params[chn_idx][active_pid_idx].frame_manip)
		priv->encap_pid_params[chn_idx][active_pid_idx].frame_manip = frame_manip;

    return 0;
}

/**************************************************************************/
/*! \fn static Int32 mpegEncapFrameModDelRule( SoCMPEGEncapPidOutStream_t *mpegEncapFrameModify )
 **************************************************************************
 *  \brief Delete frame modification rules set
 *         Doesn't disassociate PID from IP stream and doesn't deactivate PID
 *  \param[in] SoCMPEGEncapPidOutStream_t *mpegEncapFrameModify - input parameters structure
 *  \return  0 or error code
 */
static Int32 mpegEncapFrameModDelRule( SoCMPEGEncapPidOutStream_t *mpegEncapFrameModify )
{
	Uint8 chn_idx       = HAL_PORT_ID_2_CHANNEL_INDEX(mpegEncapFrameModify->port);
	Uint32              active_pid_idx; 

	if ((MAX_MPEG_CHNS-1)<chn_idx)
	{
		printk(" Failed to delete frame modification rule for PID %u on port %u with IP stream %u: port is invalid\n",
				mpegEncapFrameModify->pid, mpegEncapFrameModify->port, mpegEncapFrameModify->outstream_idx);
		return -1;
	}

	if (mpegEncapFrameModify->pid >= PID_SPACE)
	{
		printk("Failed to delete frame modification rule for  PID %u on port %u with IP stream %u: invalid PID\n",
				mpegEncapFrameModify->pid, mpegEncapFrameModify->port, mpegEncapFrameModify->outstream_idx);
		return -1;
	}

	if (mpegEncapFrameModify->outstream_idx >= MAX_MPEG_ENCAP_OUT_STREAMS)
	{
		printk("Failed to delete frame modification rule for  PID %u on port %u with IP stream %u: IP stream index is invalid\n",
				mpegEncapFrameModify->pid, mpegEncapFrameModify->port, mpegEncapFrameModify->outstream_idx);
		return -1;
	}

	active_pid_idx = (Uint32)GET_ACTIVE_PID_INDEX(priv->pid_filters[chn_idx][mpegEncapFrameModify->pid].filter);
	if (active_pid_idx == INVALID_ACTIVE_PID_IDX)
	{
		printk("%s: Failed to delete frame modification rule for PID %u on IP stream %u: PID is inactive\n",
			    __FUNCTION__, mpegEncapFrameModify->pid, mpegEncapFrameModify->outstream_idx);
		return -1;
	}

	return mpegEncapFrameModDelPidOutStreamRule(chn_idx, active_pid_idx, mpegEncapFrameModify->outstream_idx);
}

/**************************************************************************/
/*      INTERFACE  Functions                                              */
/**************************************************************************/

/**************************************************************************/
/*! \fn void mpegEncapRegisterSoCModule (SoCDriverOperations_t **mpeg_encap_operations_out)
 **************************************************************************
 *  \brief Register the MPEG-Encap driver function pointers object.
 *  \param[in] **mpeg_encap_operations - pointer to MPEG-Encap .
 *  \return none.
 **************************************************************************/
void mpegEncapRegisterSoCModule (SoCDriverOperations_t **mpeg_encap_operations_out)
{
   *mpeg_encap_operations_out  =  &mpeg_encap_operations;
}

/**************************************************************************/
/*! \fn Int32 mpegEncapUnitEnable( void )
 **************************************************************************
 *  \brief Enable MPEG enacapsulation driver
 *  \return error code or 0.
 **************************************************************************/
Int32 mpegEncapUnitEnable( void )
{
    enable_irq(MPEG_RXINT_NUM);
    // should replace by command to PDSP1
    return 0;
}

/**************************************************************************/
/*! \fn Int32 mpegEncapUnitDisable(Uint32 index)
 **************************************************************************
 *  \brief Disable MPEG enacapsulation driver
 *  \param[in] Uint32 index - filter index
 *  \return  0 or error code
 */
Int32 mpegEncapUnitDisable( void )
{
    disable_irq(MPEG_RXINT_NUM);
    // should replace by command to PDSP1
    return 0;
}

/**************************************************************************/
/*! \fn Int32 mpegEncapAddOutStream( SoCMPEGEncapHeaderInfo_t *mpegEncapHeaderInfo )
 **************************************************************************
 *  \brief Define new encapsulation IP stream
 *  \param[in] SoCMPEGEncapHeaderInfo_t *mpegEncapHeaderInfo - input parameters structure
 *  \return  0 or error code
 */
Int32 mpegEncapAddOutStream( SoCMPEGEncapHeaderInfo_t *mpegEncapHeaderInfo )
{

	Uint32 out_stream_idx;

	if (mpegEncapFindFreeOutStreamEntry(&out_stream_idx) !=  0)
	{
		printk("Failed to add new mpeg encapsulation IP stream - data base is full\n");
		return -1;
	}

	priv->encap_out_streams[out_stream_idx].enabled = 1;
	memset(&(priv->encap_out_streams[out_stream_idx].encap_header), 0, sizeof(priv->encap_out_streams[out_stream_idx].encap_header));

    mpegEncapSetPacketHeader(&priv->encap_out_streams[out_stream_idx],
                             &(mpegEncapHeaderInfo->mac_src_addr[0]), 
                             &(mpegEncapHeaderInfo->mac_dst_addr[0]), 
                             mpegEncapHeaderInfo->ip4_src_addr, 
                             mpegEncapHeaderInfo->ip4_dst_addr, 
                             mpegEncapHeaderInfo->udp_src_port,
                             mpegEncapHeaderInfo->udp_dst_port,
                             mpegEncapHeaderInfo->encap_type,
                             mpegEncapHeaderInfo->udp_csum,
                             mpegEncapHeaderInfo->rtp_ssrc);

	/*Return assigned IP stream index*/
	mpegEncapHeaderInfo->out_stream_idx = out_stream_idx;

    return 0;
}


/**************************************************************************/
/*! \fn Int32 mpegEncapRemOutStream( SoCMPEGEncapHeaderInfo_t *mpegEncapHeaderInfo )
 **************************************************************************
 *  \brief Remove encapsulation IP stream
 *  \param[in] SoCMPEGEncapHeaderInfo_t *mpegEncapHeaderInfo - input parameters structure
 *  \return  0 or error code
 */
Int32 mpegEncapRemOutStream( SoCMPEGEncapHeaderInfo_t *mpegEncapHeaderInfo )
{
	Uint32 chn_idx, pid_idx;
	Uint32 out_stream_idx = mpegEncapHeaderInfo->out_stream_idx;

	if (mpegEncapHeaderInfo->out_stream_idx >= MAX_MPEG_ENCAP_OUT_STREAMS)
	{
		printk("Failed to remove IP stream %u: IP stream index is invalid\n",mpegEncapHeaderInfo->out_stream_idx);
		return -1;
	}

	/*Search for PIDs associated with this IP stream and disassociated them*/
	for (chn_idx = 0; chn_idx < MAX_MPEG_CHNS; chn_idx++)
		for (pid_idx = 0; pid_idx < MAX_MPEG_ACTIVE_PIDS; pid_idx++)
		{
			if (priv->encap_pid_params[chn_idx][pid_idx].pid != MPEG_INVALID_PID)
			{
				mpegEncapDisassocPidFromOutstream(chn_idx, pid_idx, out_stream_idx);
			}
		}
		
	memset(&(priv->encap_out_streams[out_stream_idx]), 0, sizeof(priv->encap_out_streams[out_stream_idx]));

    return 0;
}

/**************************************************************************/
/*! \fn Int32 mpegEncapPortSetRtpTs( SoCMPEGEncapRtpTsInfo_t *mpegEncapRtpTs )
 **************************************************************************
 *  \brief Set RTP timestamp
 *  \param[in] SoCMPEGEncapRtpTsInfo_t *mpegEncapRtpTs - input parameters structure
 *  \return  0 or error code
 */
Int32 mpegEncapPortSetRtpTs( SoCMPEGEncapRtpTsInfo_t *mpegEncapRtpTs )
{
	if (mpegEncapRtpTs->outstream_idx >= MAX_MPEG_ENCAP_OUT_STREAMS)
	{
		printk("Failed to set RTP TS %d\n", mpegEncapRtpTs->outstream_idx);
		return -1;
	}

    mpegEncapSetRtpTsProperties(&priv->encap_out_streams[mpegEncapRtpTs->outstream_idx],
                                mpegEncapRtpTs->pcr_pid,
                                mpegEncapRtpTs->rtp_ts);
    return 0;
}

/**************************************************************************/
/*! \fn Int32 mpegEncapPortSetPidOp( Uint8 port, Uint16 pid, pidOperation_e pid_operation )
 **************************************************************************
 *  \brief Set PID forwarding operation
 *  \param[in] Uint8 port, Uint16 pid, pidOperation_e pid_operation
 *  \return  0 or error code
 */
Int32 mpegEncapPortSetPidOp( Uint8 port, Uint16 pid, pidOperation_e pid_operation )
{
    Uint8 chn_idx = HAL_PORT_ID_2_CHANNEL_INDEX(port);
    if ((MAX_MPEG_CHNS-1)<chn_idx)
        return -1;

    if (pid >= PID_SPACE)
		return -1;

    SET_PID_OPERATION(priv->pid_filters[chn_idx][pid].filter, (Uint8)pid_operation);
    return 0;
}

/**************************************************************************/
/*! \fn Int32 mpegEncapPortGetPidOp( Uint8 port, Uint16 pid, pidOperation_e *pid_op )
 **************************************************************************
 *  \brief Get PID forwarding operation
 *  \param[out] Uint8 port, Uint16 pid, pidOperation_e *pid_op
 *  \return  0 or error code
 */
Int32 mpegEncapPortGetPidOp( Uint8 port, Uint16 pid, Uint8 *pid_op)
{
    Uint8 chn_idx = HAL_PORT_ID_2_CHANNEL_INDEX(port);
    if ((MAX_MPEG_CHNS-1)<chn_idx)
        return -1;

    if (pid >= PID_SPACE)
		return -1;

    *pid_op = GET_PID_OPERATION(priv->pid_filters[chn_idx][pid].filter);

	return 0;
}

/**************************************************************************/
/*! \fn Int32 mpegEncapPortFrameModify( SoCMPEGEncapPidOutStream_t *mpegEncapFrameModify )
 **************************************************************************
 *  \brief Configure frame modification rules
 *  \param[in] SoCMPEGEncapPidOutStream_t *mpegEncapFrameModify - input parameters structure
 *  \return  0 or error code
 */
Int32 mpegEncapPortFrameModify( SoCMPEGEncapPidOutStream_t *mpegEncapFrameModify )
{
	if (mpegEncapFrameModify->action == MPEG_ENCAP_CFG_ADD)
	{
		return mpegEncapFrameModAddRule(mpegEncapFrameModify);
	}
	else
	{
		return mpegEncapFrameModDelRule(mpegEncapFrameModify);
	}
}

/**************************************************************************/
/*! \fn Int32 mpegEncapAddPidToOutStream( SoCMPEGEncapPidOutStream_t *pidEncap )
 **************************************************************************
 *  \brief Associate a PID with an IP stream
 *  \param[in] SoCMPEGEncapPidOutStream_t *pidEncap - input parameters structure
 *  \return  0 or error code
 */
Int32 mpegEncapAddPidToOutStream( SoCMPEGEncapPidOutStream_t *pidEncap )
{
	Uint32 active_pid_idx;
	Uint8 chn_idx = HAL_PORT_ID_2_CHANNEL_INDEX(pidEncap->port);

	if ((MAX_MPEG_CHNS-1)<chn_idx)
	{
		printk("Failed to associate PID %u on port %u with %s stream %u: port is invalid\n",
				pidEncap->pid, pidEncap->port, (pidEncap->mpegOutOperation) ? "MPEG-Out":"IP", pidEncap->outstream_idx);
        return -1;
	}

    if (pidEncap->pid >= PID_SPACE)
	{
		printk("Failed to associate PID %u on port %u with %s stream %u: invalid PID\n",
				pidEncap->pid, pidEncap->port, (pidEncap->mpegOutOperation) ? "MPEG-Out":"IP", pidEncap->outstream_idx);
		return -1;
	}

	if (pidEncap->outstream_idx >= MAX_MPEG_ENCAP_OUT_STREAMS)
	{
		printk("Failed to associate PID %u on port %u with %s stream %u: Stream index is invalid\n",
				pidEncap->pid, pidEncap->port, (pidEncap->mpegOutOperation) ? "MPEG-Out":"IP", pidEncap->outstream_idx);
		return -1;
	}

   active_pid_idx = (Uint32)GET_ACTIVE_PID_INDEX(priv->pid_filters[chn_idx][pidEncap->pid].filter);

	/* if the PID inactive - activate it */
	if (active_pid_idx == INVALID_ACTIVE_PID_IDX)
	{
		if (mpegEncapActivatePid(chn_idx, pidEncap->pid, &active_pid_idx))
		{
			printk("Failed to associate PID %u on port %u with %s stream %u: database is full\n",
					pidEncap->pid, pidEncap->port, (pidEncap->mpegOutOperation) ? "MPEG-Out":"IP", pidEncap->outstream_idx);
			return -1;
		}
	}

   if (pidEncap->mpegOutOperation == MPEG_OUT_STREAM_OFF)
   {
      /* This is an IP stream */
    	if (!mpegEncapIsOutStreamEnabled(pidEncap->outstream_idx))
    	{
    		printk("Failed to associate PID %u on port %u with %s stream %u: Stream doesn't exist\n",
    				pidEncap->pid, pidEncap->port, (pidEncap->mpegOutOperation) ? "MPEG-Out":"IP", pidEncap->outstream_idx);
    		return -1;
    	}
      /*If this PID is not associated with this IP stream - perform the association*/
      if (!priv->encap_pid_params[chn_idx][active_pid_idx].out_stream_map[pidEncap->outstream_idx])
      {
          priv->encap_pid_params[chn_idx][active_pid_idx].out_stream_map[pidEncap->outstream_idx] = 1;
          priv->encap_pid_params[chn_idx][active_pid_idx].num_assoc_outstreams++;
      }
   }
   else
   {   /* Mpeg-Out stream */
       priv->encap_pid_params[chn_idx][active_pid_idx].pid_map_operation = pidEncap->mpegOutOperation;
       priv->encap_pid_params[chn_idx][active_pid_idx].pid_map_value     = pidEncap->pidMapValue;
   }

	return 0;
}

/**************************************************************************/
/*! \fn Int32 mpegEncapRemPidFromOutStream( SoCMPEGEncapPidOutStream_t *pidEncap )
 **************************************************************************
 *  \brief Disassociate a PID from an IP stream
 *  \param[in] SoCMPEGEncapPidOutStream_t *pidEncap - input parameters structure
 *  \return  0 or error code
 */
Int32 mpegEncapRemPidFromOutStream( SoCMPEGEncapPidOutStream_t *pidEncap )
{
    Uint32 active_pid_idx;
    Uint8 chn_idx = HAL_PORT_ID_2_CHANNEL_INDEX(pidEncap->port);

    if ((MAX_MPEG_CHNS-1)<chn_idx)
    {
        printk("Failed to disassociate PID %u on port %u with IP stream %u: port is invalid\n",
               pidEncap->pid, pidEncap->port, pidEncap->outstream_idx);
        return -1;
    }

    if (pidEncap->pid >= PID_SPACE)
    {
        printk("Failed to disassociate PID %u on port %u with IP stream %u: invalid PID\n",
               pidEncap->pid, pidEncap->port, pidEncap->outstream_idx);
        return -1;
    }

    if (pidEncap->outstream_idx >= MAX_MPEG_ENCAP_OUT_STREAMS)
    {
        printk("Failed to disassociate PID %u on port %u with IP stream %u: IP stream index is invalid\n",
               pidEncap->pid, pidEncap->port, pidEncap->outstream_idx);
        return -1;
    }

    active_pid_idx = (Uint32)GET_ACTIVE_PID_INDEX(priv->pid_filters[chn_idx][pidEncap->pid].filter);

    if (priv->encap_pid_params[chn_idx][active_pid_idx].pid_map_operation)
    {
        /* This is an MPEG out stream */
        if (pidEncap->mpegOutOperation == MPEG_OUT_STREAM_OFF)
        {
            /* IF we stop this MPEG out stream we shoudl also drop/filter all frames with this PID */
            mpegEncapDeactivatePid(chn_idx, pidEncap->pid, active_pid_idx);
        }
        /* The assumption is that Active PID can be MPEG out OR IP stream (not both) */
        return 0;
    }

    if (!mpegEncapIsOutStreamEnabled(pidEncap->outstream_idx))
    {
        printk("Failed to disassociate PID %u on port %u with IP stream %u: IP stream doesn't exist\n",
               pidEncap->pid, pidEncap->port, pidEncap->outstream_idx);
        return -1;
    }

    if (active_pid_idx != INVALID_ACTIVE_PID_IDX)
    {
        mpegEncapDisassocPidFromOutstream(chn_idx,active_pid_idx, pidEncap->outstream_idx);
    }

    return 0;
}

/**************************************************************************/
/*! \fn Int32 mpegEncapGetPidAssocOutStreams( SoCMPEGEncapPidOutStream_t *pidEncap )
 **************************************************************************
 *  \brief Get IP streams associated with a PID
 *  \param[in] SoCMPEGEncapPidOutStream_t *pidEncap - input parameters structure
 *  \return  0 or error code
 */
Int32 mpegEncapGetPidAssocOutStreams( SoCMPEGEncapPidOutStream_t *pidEncap )
{
	Uint32 active_pid_idx;
	Uint8 chn_idx = HAL_PORT_ID_2_CHANNEL_INDEX(pidEncap->port);

	if ((MAX_MPEG_CHNS-1)<chn_idx)
	{
		printk("Failed to get PID %u on port %u associated IP streams: port is invalid\n",
				        pidEncap->pid, pidEncap->port);
        return -1;
	}

	active_pid_idx = (Uint32)GET_ACTIVE_PID_INDEX(priv->pid_filters[chn_idx][pidEncap->pid].filter);
	if (active_pid_idx == INVALID_ACTIVE_PID_IDX)
	{
		printk("No associated IP streams for PID %u on port %u\n",
						pidEncap->pid, pidEncap->port);
		return 0;
	}
	memcpy(pidEncap->outstream_map, priv->encap_pid_params[chn_idx][active_pid_idx].out_stream_map, MAX_MPEG_ENCAP_OUT_STREAMS);
	pidEncap->outstream_idx = priv->encap_pid_params[chn_idx][active_pid_idx].num_assoc_outstreams;

	return 0;
}

/**************************************************************************/
/*! \fn Int32 FrameModDelPidOutStreamRule(Uint32 chn_idx, Uint32 active_pid_idx, Uint32 outstream_idx)
 **************************************************************************
 *  \brief Delete frame modification rules for PID on IP stream. 
 *         Doesn't disassociate PID from IP stream and doesn't deactivate PID
 *  \param[in]   chn_idx - channel index
 *  \param[in]   active_pid_idx - active PID index
 *  \param[in]   outstream_idx - IP stream index
 *  \return  0 or error code
 */
Int32 mpegEncapFrameModDelPidOutStreamRule(Uint32 chn_idx, Uint32 active_pid_idx, Uint32 outstream_idx)
{
	mpeg_modified_frame **frame_manip;

	/*if no frame modification rules at all are defined for this PID - allocate rules array */
    if (!priv->encap_pid_params[chn_idx][active_pid_idx].frame_manip)
	{
		printk("%s: Failed to delete frame modification rule for PID index %u on IP stream %u: No rules defined\n",
			   __FUNCTION__, active_pid_idx, outstream_idx);
		return -1;
	}

	frame_manip = priv->encap_pid_params[chn_idx][active_pid_idx].frame_manip;

	/*if no frame modification rules  are defined for this PID for this particular IP stream */
	if (!frame_manip[outstream_idx])
	{
		printk("%s: Failed to delete frame modification rule for PID index %u on IP stream %u: No such rule\n",
			   __FUNCTION__, active_pid_idx, outstream_idx);
		return -1;
	}
	else
	{
		Ptr zero_array[MAX_MPEG_ENCAP_OUT_STREAMS]={NULL};
		mpeg_modified_frame *outstream_rule = frame_manip[outstream_idx];

		frame_manip[outstream_idx] = NULL;

		kfree(outstream_rule);
		
		/*If this has been the only rule - delete the whole array*/
		if (!memcmp(frame_manip, zero_array, MAX_MPEG_ENCAP_OUT_STREAMS*sizeof(mpeg_modified_frame*)))
		{
			priv->encap_pid_params[chn_idx][active_pid_idx].frame_manip=NULL;
			kfree(frame_manip);
		}
	}

    return 0;
}

