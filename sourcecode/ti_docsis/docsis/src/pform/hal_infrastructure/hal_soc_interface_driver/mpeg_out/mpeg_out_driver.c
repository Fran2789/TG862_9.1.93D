/*
 *
 * mpeg_out_driver.c
 * Description:
 * MPEG out driver implementation
 *
 * Copyright (C) 2008-2013 Texas Instruments Incorporated - http://www.ti.com/ 
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

/*! \file mpeg_out_driver.c
    \brief Implementation of Mpeg out Driver. 
    \code.
*/

#include <linux/module.h>   /* module */
#include <asm/uaccess.h>
#include "pal.h"
#include "soc_interface_driver.h"
#include "mpeg_out_driver.h"


/*****************************************/
/* Definitions for MPEG-Out SoC Unit     */
/*****************************************/

#if defined (CONFIG_MACH_PUMA6)
/* --------------------------------------------PUMA6------------------------------------------*/   
 #define BIT_4                               (0x10)  
 #define BIT_6                               (0x40)
 #define BIT_7                               (0x80)                                         
                                              
/* SoC MPEG Out Registers */

#define PUMA_MPEG_OUT_RESET_REG_OFFSET                   (0x1D00)
/* Bits         Name            Type        Reset               Description                                                                          */
/* 7:6     cppi_fifo_rst         r/w         11             Reset signal to cppi fifo. Active high (active low but inverted outside)                 */
/* 5:0      p2s_rst              r/w        111111          Reset signal to P2S module of the corresponded channel. Reset signals are active high.   */
#define PUMA_MPEG_OUT_CONTROL_REG_OFFSET                 (0x1D01)
/* Bits     Name            Type        Reset               Description                                                                                             */
/* 8	cppi debug select	r/w	        0	                Selects which of the CPPI channels is feeding the debug counters (registers 1D04-1D09)                  */
/*                                                           0 – CPPI 0 selected                                                                                    */
/*                                                           1 – CPPI 1 selected                                                                                    */
/*                                                           New in C0                                                                                              */
/* 7    cppi_msb_first      r/w         1                   Bit transmission order - which of the bits is sent first. Affect ONLY the mpeg frame generator:         */
/*                                                           0 - LSB                                                                                                */
/*                                                           1 - MSB                                                                                                */
/* 6    lock_rcvr_dis       r/w         0                   Lock Fall Recovery Disable. When active, extra features of the P2S modules are disabled:                */
/*                                                          "   Complete the frame when lock fall in the middle of it.                                              */
/*                                                          "   Start sending new frame only ay sinc and when lock is active.                                       */
/*                                                          1 - Lock Fall Recovery disabled                                                                         */
/*                                                          0 - Lock Fall Recovery enabled                                                                          */
/* 5    endian_swap         r/w         0                   Swap bytes in the bus going from the Async FIFO to the Frame Generator (in the CPPI path):              */
/*                                                          0 - bus directly connected                                                                              */
/*                                                          1 - bytes are swapped                                                                                   */
/* 4    clock_inv           r/w         0                   Invert the clock output:                                                                                */
/*                                                          0 - Signals change at rise edge                                                                         */
/*                                                          1 - Signals change at fall edge                                                                         */
/* 3    sync_pulse_width    r/w         0                   Sync pulse width. Affect the mpeg frame generator, P2S and S2P:                                         */
/*                                                          0 - bit                                                                                                 */
/*                                                          1 - byte                                                                                                */
/* 2    msb_first           r/w         1                   Bit transmission order - which of the bits is sent first. Affect the mpeg frame generator, P2S and S2P: */
/*                                                          0 - LSB                                                                                                 */
/*                                                          1 - MSB                                                                                                 */
/* 1    sync_inv            r/w         0                   Switch the sync output to active low mode. Used for both serial and parallel interfaces:                */
/*                                                          0 - active high                                                                                         */
/*                                                          1 - active low                                                                                          */
/* 0    valid_inv           r/w         0                   Switch the valid output to active low mode. Used for both serial and parallel interfaces:               */
/*                                                          0 - active high                                                                                         */
/*                                                          1 - active low                                                                                          */

#define PUMA_MPEG_OUT_INTERFACE_SELECT_REG_OFFSET        (0x1D02)
/* Bits     Name                Type        Reset           Description                                         */
/* 11:0  Intfc_sel[11:0]        r/w         0                 Selectors for the 6 muxes of the output.          */
/*                                                            Every 2 bits are selector for a mux selecting:    */
/*                                                            00 - all outputs are Zeros.                       */
/*                                                            01 - CPPI 1.                                      */
/*                                                            10 - CPPI 2.                                      */
/*                                                            11 - Phy MPEG.                                    */
#define PUMA_MPEG_OUT_INTERFACE_SELECT_REG_TWO_BITS_PER_CH (2) 
#define PUMA_MPEG_OUT_INTERFACE_SELECT_REGPORT_BITS_MASK  (0x0003)

#define PUMA_MPEG_OUT_STATUS_REG_OFFSET                  (0x1D03)
/* Bits         Name            Type    Reset                       Description                                                                                     */
/* 11:4         frame_count       r         0           Frame counter. Every "frame_done", from either of the 2 frame generators - the counter is increased by 1.   */
/* 3:2          smfg_busy         r         0           smfg_busy[0] for fg1. smfg_busy[1] for fg2                                                                  */
/*                                                      Serial MPEG Frame Generator busy. Active high when not in frame.                                            */
/* 1:0          smfg_error        r         0           smfg_error[0] for fg1. smfg_error[1] for fg2.                                                               */
/*                                                      Serial MPEG Frame Generator error. Set high when                                                            */
/*                                                      an error occurred (pulse was observed on error signal).                                                     */
/*                                                      Cleared when the register is written to (byten[0] = 1).                                                     */
#define PUMA_MPEG_OUT_MAX_PORT_NUM              6

#define PUMA_MPEG_OUT_RESET_REG( device )       BuildPhyAddr( PUMA_MPEG_OUT_RESET_REG_OFFSET )
#define PUMA_MPEG_OUT_CONTROL_REG( device )     BuildPhyAddr( PUMA_MPEG_OUT_CONTROL_REG_OFFSET )

/* Mapping of MPEG Out Reset Registers (PUMA_MPEG_OUT_RESET_REG) */
#define PUMA_MPEG_OUT_PORT1_EN_FIELD           (0x01)      /* bit-0 - value 1 for reset    */
#define PUMA_MPEG_OUT_PORT2_EN_FIELD           (0x02)      /* bit-1 - value 1 for reset    */
#define PUMA_MPEG_OUT_PORT3_EN_FIELD           (0x04)      /* bit-2 - value 1 for reset    */
#define PUMA_MPEG_OUT_PORT4_EN_FIELD           (0x08)      /* bit-3 - value 1 for reset    */
#define PUMA_MPEG_OUT_PORT5_EN_FIELD           (0x10)      /* bit-4 - value 1 for reset    */
#define PUMA_MPEG_OUT_PORT6_EN_FIELD           (0x20)      /* bit-5 - value 1 for reset    */

#define PUMA_MPEG_OUT_P2S1_EN_FIELD            (0x40)      /* bit-6 - value 1 for reset    */
#define PUMA_MPEG_OUT_P2S2_EN_FIELD            (0x80)      /* bit-7 - value 1 for reset    */

#define PUMA_MPEG_OUT_PORT_EN_VAL              (0) 
#define PUMA_MPEG_OUT_PORT_DIS_VAL             (1)

/* Mapping of MPEG Out Control Registers (PUMA_MPEG_OUT_CONTROL_REG) */
#define PUMA_MPEG_OUT_CPPI_MSB_FIRST           (0x80)       /* bit-7 - Controls which of the bits is sent first. Affects ONLY the MPEG frame generator:
                                                                          0 – LSB
                                                                          1 – MSB */
#define PUMA_MPEG_OUT_LOCK_RCVR_DIS            (0x40)       /* bit-6 Lock Fall Recovery Disable. When active, extra features of the P2S modules are disabled:
                                                                "   Complete the frame when lock fall in the middle of it.
                                                                "   Start sending new frame only ay sinc and when lock is active. 
                                                                1 - Lock Fall Recovery disabled
                                                                0 - Lock Fall Recovery enabled */

#define PUMA_MPEG_OUT_ENDIAN_SWAP              (0x20)       /* bit-5  - Swap bytes in the bus going from the Async FIFO to the Frame Generator: 
                                                                          0 – Bus directly connected
                                                                          1 – Bytes are swapped  */
#define PUMA_MPEG_OUT_CLK_INV_FIELD            (0x10)       /* bit-4  */
#define PUMA_MPEG_OUT_SYNC_PULSE_FIELD         (0x08)       /* bit-3  */
#define PUMA_MPEG_OUT_MSB_FIELD                (0x04)       /* bit-2  */
#define PUMA_MPEG_OUT_SYNC_INV_FIELD           (0x02)       /* bit-1  */
#define PUMA_MPEG_OUT_VALID_INV_FIELD          (0x01)       /* bit-0  */


#define PUMA_MPEG_OUT_INTFC_SELECT_DIS_VAL     (0x000)
#define PUMA_MPEG_OUT_INTFC_SELECT_EN_VAL      (0x001)       /* Selectors for the 6 muxes of the output.
                                                                Every 2 bits are selector for a mux selecting:
                                                                00 - all outputs are Zeros.
                                                                01 - CPPI 1.    
                                                                10 - CPPI 2.    
                                                                11 - Phy MPEG.  
                                                                 */             

#define PUMA_MPEG_OUT_PHY_SRC                  (0) 
#define PUMA_MPEG_OUT_CPPI01_SRC               (1) 
#define PUMA_MPEG_OUT_CPPI02_SRC               (2)
#define PUMA_MPEG_OUT_ALL_OUTPUT_ZERO          (3)


#define PUMA_MPEG_OUT_PHY_SRC_VAL              (0x0003) 
#define PUMA_MPEG_OUT_CPPI01_SRC_VAL           (0x0001) 
#define PUMA_MPEG_OUT_CPPI02_SRC_VAL           (0x0002)
#define PUMA_MPEG_OUT_ALL_OUTPUT_ZERO_VAL      (0x0000)



#define CLOSE_PORT_VAL                  (PUMA_MPEG_OUT_PORT1_EN_FIELD | \
                                         PUMA_MPEG_OUT_PORT2_EN_FIELD | \
                                         PUMA_MPEG_OUT_PORT3_EN_FIELD | \
                                         PUMA_MPEG_OUT_PORT4_EN_FIELD | \
                                         PUMA_MPEG_OUT_PORT5_EN_FIELD | \
                                         PUMA_MPEG_OUT_PORT6_EN_FIELD | \
                                         PUMA_MPEG_OUT_P2S1_EN_FIELD  | \
                                         PUMA_MPEG_OUT_P2S2_EN_FIELD    \
                                         )   



#else
/* --------------------------------------------PUMA5------------------------------------------*/
#define PUMA_MPEG_OUT_MAX_PORT_NUM          4
#define PUMA5_PINMUX0_MPEG_OUT_EN            0x4

#define PUMA5_LPSC_MPEG_OUT_MODULE_OFFSET    (0x21A0C)
#define PUMA5_LPSC_MPEG_OUT_MODULE( device ) GET_SOC_REG_ADDR( device, PUMA5_LPSC_MPEG_OUT_MODULE_OFFSET )
#define PUMA5_LPSC_MPEG_OUT_MODULE_DIS_VAL   (0x002)
#define PUMA5_LPSC_MPEG_OUT_MODULE_EN_VAL    (0x103)/* bit 8   Module local reset control
                                                      0 = assert local reset
                                                      1 = de-assert local reset
                                                    bits 0:4 
                                                      00000: SwRstDisable
                                                      00001: SyncRst
                                                      00010: Disable
                                                      00011: Enable */                                                    
/* SoC MPEG Out Registers */
#define PUMA_MPEG_OUT_RESET_REG_OFFSET          (0x0)
#define PUMA_MPEG_OUT_CONTROL_REG_OFFSET        (0x4)

#define PUMA_MPEG_OUT_RESET_REG( device )       GET_MPEG_OUT_REG_ADDR( device, PUMA_MPEG_OUT_RESET_REG_OFFSET )
#define PUMA_MPEG_OUT_CONTROL_REG( device )     GET_MPEG_OUT_REG_ADDR( device, PUMA_MPEG_OUT_CONTROL_REG_OFFSET )

/* Mapping of MPEG Out Reset Registers (PUMA_MPEG_OUT_RESET_REG). bits 31:5 reserved */
#define PUMA_MPEG_OUT_PORT1_EN_FIELD           (0x01)      /* bit-0 - value 1 for reset    */
#define PUMA_MPEG_OUT_PORT2_EN_FIELD           (0x02)      /* bit-1 - value 1 for reset    */
#define PUMA_MPEG_OUT_PORT3_EN_FIELD           (0x04)      /* bit-2 - value 1 for reset    */
#define PUMA_MPEG_OUT_PORT4_EN_FIELD           (0x08)      /* bit-3 - value 1 for reset    */
#define PUMA_MPEG_OUT_S2P_EN_FIELD             (0x10)      /* bit-4 - value 1 for reset    */
#define PUMA_MPEG_OUT_PORT_EN_VAL              (0) 
#define PUMA_MPEG_OUT_PORT_DIS_VAL             (1)

/* Mapping of MPEG Out Control Registers (PUMA_MPEG_OUT_CONTROL_REG). bits 23:12 reserved */
#define PUMA_MPEG_OUT_CPPI_MSB_FIRST           (0x800)       /* bit-11 - Controls which of the bits is sent first. Affects ONLY the MPEG frame generator:
                                                                          0 – LSB
                                                                          1 – MSB */
#define PUMA_MPEG_OUT_ENDIAN_SWAP              (0x200)       /* bit-9  - Swap bytes in the bus going from the Async FIFO to the Frame Generator: 
                                                                          0 – Bus directly connected
                                                                          1 – Bytes are swapped  */
#define PUMA_MPEG_OUT_CLK_INV_FIELD            (0x100)       /* bit-8  */
#define PUMA_MPEG_OUT_FRAME_CNT_EN             (0x080)       /* bit-7  - Frame counter enable: 1 – Enable 0 – Disable*/
#define PUMA_MPEG_OUT_SYNC_PULSE_FIELD         (0x040)       /* bit-6  */
#define PUMA_MPEG_OUT_MSB_FIELD                (0x020)       /* bit-5  */
#define PUMA_MPEG_OUT_SYNC_INV_FIELD           (0x010)       /* bit-4  */
#define PUMA_MPEG_OUT_VALID_INV_FIELD          (0x008)       /* bit-3  */
#define PUMA_MPEG_OUT_PORT1_SEL_FIELD          (0x004)       /* bit-2  */

#define PUMA_MPEG_OUT_INTFC_SELECT_OFFSET      (0x000)       /* bits-0:1     */
#define PUMA_MPEG_OUT_INTFC_SELECT_WIDTH       (0x002)       /* 2 bits width */
#define PUMA_MPEG_OUT_INTFC_SELECT_DIS_VAL     (0x000)
#define PUMA_MPEG_OUT_INTFC_SELECT_EN_VAL      (0x001)       /* 00 – All bus is zero
                                                                 01 – Serial interface
                                                                 10 – S-MODE (parallel single mode)
                                                                 11 – M-MODE (parallel multiple mode) */

#define PUMA5_MPEG_OUT_CPPI_SELECT              (0x1)




#define CLOSE_PORT_VAL                  (PUMA_MPEG_OUT_PORT1_EN_FIELD |  \
                                         PUMA_MPEG_OUT_PORT2_EN_FIELD |  \
                                         PUMA_MPEG_OUT_PORT3_EN_FIELD |  \
                                         PUMA_MPEG_OUT_PORT4_EN_FIELD |  \
                                         PUMA_MPEG_OUT_S2P_EN_FIELD      \
                                         )  


#endif
#define BIT_3                               (0x8)

static void mpegOutCloseAllPorts ( socDevice_e device );
                                                                 
static long mpegOutDriverIoctl   (struct file *file, 
                                  unsigned int ioctl_num,unsigned long ioctl_param, struct semaphore *soc_sem);

static Int32 mpegOutPortSource( socDevice_e device, Uint32 port, Uint32 source );

#if defined (CONFIG_MACH_PUMA6)
static void mpegOutInit(struct semaphore *soc_sem);
static void closePhyClockGating(Bool ClockOff);
#endif

static SoCDriverOperations_t mpeg_out_operations =  
{
   .socModuleID   = SOC_MPEG_OUT_MODULE_ID,
   .soc_ioctl     = &mpegOutDriverIoctl, 
   .soc_release   = NULL,
   .soc_open      = NULL,
   .soc_cleanup   = NULL,
#if defined (CONFIG_MACH_PUMA6)
   .soc_init      = &mpegOutInit
#else
   .soc_init      = NULL
#endif
};

/**************************************************************************/
/*! \fn void mpegOutRegisterSoCModule (SoCDriverOperations_t **mpeg_out_operations)
 **************************************************************************
 *  \brief Register the MPEG-Out driver function pointers object.
 *  \param[in] **mpeg_out_operations - pointer to MPEG-Out .
 *  \return none.
 **************************************************************************/
void mpegOutRegisterSoCModule (SoCDriverOperations_t **mpeg_out_operations_out)
{
   *mpeg_out_operations_out  =  &mpeg_out_operations;
}

#if defined (CONFIG_MACH_PUMA6)

/**************************************************************************/
/*! \fn static void closePhyClockGating(Bool ClockOff)
 **************************************************************************
 *  \brief turn off/on the clock gating bit for each phy unit.
 *  \param[in]  ClockOff - True = off, False = on.
 *  \return none.
 **************************************************************************/
static void closePhyClockGating(Bool ClockOff)
{
    Uint32 regValue;

      socReadRegister(BuildPhyAddr(PHY_GENERAL_CLOCK_GATING_REGISTER),&regValue,True);
      /*---------------------------------------------------------------------*/
      /* Bit    Name            Default     Description                        */
      /* 0  HSIF RX clocks        0          0 - Clocks On, 1- Clocks Off    */
      /* 1  HSIF TX clocks        0          0 - Clocks On, 1- Clocks Off    */
      /* 2  MCR1 clocks          0           0 - Clocks On, 1- Clocks Off    */
      /* 3  MCR2 clocks          0           0 - Clocks On, 1- Clocks Off    */
      /* 4  MPEG clocks          0           0 - Clocks On, 1- Clocks Off    */
      /* 15:5   Reserved             0                                           */
      /*---------------------------------------------------------------------*/
      if (ClockOff == True)
      {
         regValue |= (BIT_4);
      }
      else
      {
         regValue &= ~(BIT_4);
      }

      socWriteRegister(BuildPhyAddr(PHY_GENERAL_CLOCK_GATING_REGISTER), regValue, True);

}
/**************************************************************************/
/*! \fn static void cppiModuleOutOfReset()
 **************************************************************************
 *  \brief take out of reset the two CPPI modules: bits 6,7 of 0x1D00 are low
            MPEG Out Reset 
 *  \return none.
 **************************************************************************/
static void cppiModuleOutOfReset()
{
      Uint32 regValue;

      socReadRegister(BuildPhyAddr( PUMA_MPEG_OUT_RESET_REG_OFFSET ),&regValue,True);
      /* Bits         Name            Type        Reset               Description                        */
      /* 7:6     cppi_fifo_rst         r/w         11             Reset signal to cppi fifo. Active high */
      
      regValue&= ~(BIT_6);
      regValue&= ~(BIT_7);


      socWriteRegister(BuildPhyAddr( PUMA_MPEG_OUT_RESET_REG_OFFSET ), regValue, True);
}
/**************************************************************************/
/*! \fn void mpegOutInit (struct semaphore *soc_sem)
 **************************************************************************
 *  \brief init finction for mpeg out.
 *  \param[in]  *soc_sem - pointer to the driver's semaphore.
 *  \return none.
 **************************************************************************/
static void mpegOutInit(struct semaphore *soc_sem)
{
    down(soc_sem);

    closePhyClockGating(True);

    up(soc_sem);
}
#endif

/**************************************************************************/
/*! \fn static long   mpegOutDriverIoctl(struct file *file,
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
static long   mpegOutDriverIoctl(struct file *file,
                                unsigned int ioctl_num,
                                unsigned long ioctl_param,
                                struct semaphore *soc_sem)
{
   /*
    * extract the type and number bitfields, and don't decode
    * wrong cmds: return ENOTTY (inappropriate ioctl) before access_ok(  )
    */
   if (_IOC_TYPE(ioctl_num) != SOC_MPEG_OUT_MODULE_ID) 
      return -ENOTTY;
   if (_IOC_NR(ioctl_num) > SOC_MPEGO_IOCTL_MAXNR) 
      return -ENOTTY;

   /* */
   down(soc_sem);
   /* 
     * Switch according to the ioctl called 
     */
    switch (ioctl_num) {
    case SOC_MPEGO_UNIT_ENABLE:
        /* 
         * This IOCTL command should get out of reset the MPEG_OUT SoC unit. */
      mpegOutUnitEnable( SOC_DEVICE_LOCAL );
      break;
#if defined (CONFIG_MACH_PUMA6)
    case SOC_MPEGO_IO_ENABLE:
       /* 
        * This IOCTL command should enable the mpeg out pins to docsis IO. */
       enableMPEGoutIO();
       break;
#endif
    case SOC_MPEGO_UNIT_DISABLE:
      /* 
       * This IOCTL command should reset the MPEG_OUT SoC unit. */
      mpegOutUnitDisable( SOC_DEVICE_LOCAL );
      break;

    case SOC_MPEGO_PORT_ENABLE:
        /* 
         * This IOCTL command should open an MPEG Out port.
         */
      if ( mpegOutPortSet( (Uint32)ioctl_param, True, SOC_DEVICE_LOCAL ) < 0 )
      {
         up(soc_sem);
         return -EINVAL;
      }
      break;

   case SOC_MPEGO_PORT_DISABLE:
      /* 
         * This IOCTL command should close an MPEG Out port.
         * 1. Open MPEG Out ports according to the ioctl_param (port number 1-4). 
         */
      if ( mpegOutPortSet( (Uint32)ioctl_param, False, SOC_DEVICE_LOCAL ) < 0 )
      {
         up(soc_sem);
         return -EINVAL;
      }
      break;

   case SOC_MPEGO_UNIT_GLOBAL_CONFIG:
   {
      SoCMPEGOutGlobalConfig_t mpegOutConfiguration;
    
      /* 
         * This IOCTL command should configure the MPEG Out Control Register.
         * 1. configure the MPEG Out Control Register according to ioctl_param (pointer to SoCMPEGOutGlobalConfig_t). 
         */
      if ( copy_from_user((void *)(&mpegOutConfiguration),
                          (void *)ioctl_param,
                          sizeof(SoCMPEGOutGlobalConfig_t)) )
      {
         printk(KERN_ERR "mpegOutDriverIoctl:: Invalid argument for MPEG Out global config.\n");
         up(soc_sem);
         return -EINVAL;
      }
      
      mpegOutUnitGlobalConfig( &mpegOutConfiguration, SOC_DEVICE_LOCAL );
   }
   break;

   case SOC_MPEGO_PORT_SOURCE:
      /* 
         * This IOCTL command changes the MPEG Out source (CPPI or PHY receive).
         * 1. Inputs are port number and source type. 
         */
      if ( mpegOutPortSource( SOC_DEVICE_LOCAL, (((Uint32)ioctl_param)>>8)&0xff, ((Uint32)ioctl_param)&0xff ) < 0 )
      {
         up(soc_sem);
         return -EINVAL;
      }
      break;

#if defined (CONFIG_MACH_PUMA6)
    case    SOC_MPEGO_CPPI_ENABLE:
            cppiModuleOutOfReset();
      break;
#endif
 
   default:  /* redundant, as cmd was checked against MAXNR */
      up(soc_sem);
      return -ENOTTY;
    }

   up(soc_sem);
    return 0;
}


static void mpegOutCloseAllPorts( socDevice_e device )
{
   Uint32 regValue;

   /* Close all MPEG Out ports */
   printk(KERN_INFO "mpegOutCloseAllPorts:: Close all MPEG Out ports.\n");

   socReadRegister((PUMA_MPEG_OUT_RESET_REG( device )),&regValue,True);
   /* Writing value of 1 will reset the MPEG ports */
   regValue |= (CLOSE_PORT_VAL);  
   socWriteRegister((PUMA_MPEG_OUT_RESET_REG( device )),regValue,True);

}




Int32 mpegOutUnitGlobalConfig( SoCMPEGOutGlobalConfig_t *mpegOutConfiguration, socDevice_e device )
{
      Uint32 regValue;

      printk(KERN_INFO "mpegOutUnitGlobalConfig:: Configure the MPEG Out Control Registers.\n");
      /* 1. configure the MPEG Out Control Register according to ioctl_param (pointer to SoCMPEGOutGlobalConfig_t) */
      socReadRegister((PUMA_MPEG_OUT_CONTROL_REG( device )),&regValue,True);
      if (mpegOutConfiguration->clockInv == 1)
         regValue |= (PUMA_MPEG_OUT_CLK_INV_FIELD);  /* set 1 */
      else
         regValue &= ~(PUMA_MPEG_OUT_CLK_INV_FIELD); /* set 0 */

      if (mpegOutConfiguration->msbFirstTxOrder == 1)
         regValue |= (PUMA_MPEG_OUT_MSB_FIELD);
      else
         regValue &= ~(PUMA_MPEG_OUT_MSB_FIELD);

      if (mpegOutConfiguration->syncInv == 1)
         regValue |= (PUMA_MPEG_OUT_SYNC_INV_FIELD);
      else
         regValue &= ~(PUMA_MPEG_OUT_SYNC_INV_FIELD);

      if (mpegOutConfiguration->syncPulseWidth == 1)
         regValue |= (PUMA_MPEG_OUT_SYNC_PULSE_FIELD);
      else
         regValue &= ~(PUMA_MPEG_OUT_SYNC_PULSE_FIELD);
#if defined (CONFIG_MACH_PUMA6)
      if (mpegOutConfiguration->validInv == 1)
         regValue |= (PUMA_MPEG_OUT_VALID_INV_FIELD);
      else
         regValue &= ~(PUMA_MPEG_OUT_VALID_INV_FIELD);
#else

      if (mpegOutConfiguration->validInv == 1)
         regValue |= (PUMA_MPEG_OUT_SYNC_INV_FIELD);
      else
         regValue &= ~(PUMA_MPEG_OUT_SYNC_INV_FIELD);

#endif

      if (mpegOutConfiguration->cppiMsbFirst == 1)
         regValue |= (PUMA_MPEG_OUT_CPPI_MSB_FIRST);
      else
         regValue &= ~(PUMA_MPEG_OUT_CPPI_MSB_FIRST);

      if (mpegOutConfiguration->endianSwap == 1)
         regValue |= (PUMA_MPEG_OUT_ENDIAN_SWAP);
      else
         regValue &= ~(PUMA_MPEG_OUT_ENDIAN_SWAP);
#if defined (CONFIG_MACH_PUMA6)
      if (mpegOutConfiguration->lockRcvrDis == 1)
         regValue |= (PUMA_MPEG_OUT_LOCK_RCVR_DIS);
      else
         regValue &= ~(PUMA_MPEG_OUT_LOCK_RCVR_DIS);

#else

      if (mpegOutConfiguration->frameCntEn == 1)
         regValue |= (PUMA_MPEG_OUT_FRAME_CNT_EN);
      else
         regValue &= ~(PUMA_MPEG_OUT_FRAME_CNT_EN);

#endif

      socWriteRegister((PUMA_MPEG_OUT_CONTROL_REG( device )),regValue,True);
      
      return 0;
}


Int32 mpegOutPortSet( Uint32 port, Bool enable, socDevice_e device )
{
      Uint32 regValue;
      Uint32 data;

      /* 1. Open/Close MPEG Out ports according to the ioctl_param (port number 1-4). */
                
      if (port < 1 || port > PUMA_MPEG_OUT_MAX_PORT_NUM)
      {
         printk(KERN_ERR "mpegOutPortSet:: Invalid argument (MPEG Out port %d). \n",port);
         return -EINVAL;
      }

      if ( enable == False )
      {
         data = PUMA_MPEG_OUT_PORT_DIS_VAL;
      }
      else
      {
         data = PUMA_MPEG_OUT_PORT_EN_VAL;
      }
      
      /* 1. Close MPEG Out ports according to the ioctl_param (port number 1-4) */
      printk(KERN_INFO "mpegOutPortSet:: %s MPEG Out port %d. \n", enable == False ? "Close" : "Open", port);
      socReadRegister((PUMA_MPEG_OUT_RESET_REG( device )),&regValue,True);
      /* Reset signals are active high (0-port enable 1-port disable .*/
      socBitField32Set(&regValue,data,(port-1),1);
      socWriteRegister((PUMA_MPEG_OUT_RESET_REG( device )),regValue,True);
      
      return 0;
}





#if defined (CONFIG_MACH_PUMA6)

Int32 enableMPEGoutIO()
{
   // MPEG out PINs in Puma6 SoC are mux with Cable-Card and MPEG in 
   PAL_sysBootCfgCtrl_DocsisIo_MPEG_out(BOOTCFG_IO_ENABLE);
   return 0;
}
Int32 mpegOutUnitEnable( socDevice_e device )
{
      Uint32 regValue;



      /* 1. Bit 2 "MPEG Out EN" When active, MPEG Out is ported out over p-flash. */
      printk(KERN_INFO "mpegOutUnitEnable:: Enable MPEG Out unit.\n");
      



      /* 2. Set MPEG_OUT_MODULE state to Enable in general reset register and clock register*/
      closePhyClockGating(False);

      printk(KERN_INFO "mpegOutUnitEnable:: Enable in general reset register. \n");
      socReadRegister(BuildPhyAddr(PHY_GENERAL_RESET_REGISTER),&regValue,True);
      /*-------------------------------------------------------------*/ 
      /* Bit  Name          Default   Description                    */ 
      /* 0    MCR1 Reset      0       MCR1 SW reset                  */ 
      /* 1    MCR2 Reset      0       MCR2 SW reset                  */ 
      /* 2    HSIF Reset      1       HSIF SW reset                  */ 
      /* 3    MPEG OUT Reset  1       MPEG OUT SW reset              */ 
      /* 15:4 Reserved                                               */ 
      /*-------------------------------------------------------------*/ 
      regValue &= ~(BIT_3);
      socWriteRegister(BuildPhyAddr(PHY_GENERAL_RESET_REGISTER), regValue, True);

      /* 3. Close all MPEG Out ports */
      mpegOutCloseAllPorts( device );

      /* 4. Enable MPEG-Out serial interface */
/*
      socReadRegister(BuildPhyAddr(PUMA_MPEG_OUT_INTERFACE_SELECT_REG_OFFSET),&regValue,True);
      regValue = PUMA_MPEG_OUT_INTFC_SELECT_EN_VAL;
      socWriteRegister(BuildPhyAddr(PUMA_MPEG_OUT_INTERFACE_SELECT_REG_OFFSET),regValue,True);
*/
      printk(KERN_INFO "mpegOutUnitEnable:: MPEG Out is working. \n");
      /* MPEG Out Control Register is now with the default values */




      return 0;
}

Int32 mpegOutUnitDisable( socDevice_e device )
{
      Uint32 regValue;


      /* 1. Close all MPEG Out ports */
      printk(KERN_INFO "mpegOutUnitDisable:: cloase all mpeg ports. \n");
      mpegOutCloseAllPorts( device );

      /* 2. Bit 2 "MPEG Out EN" When active, MPEG Out is ported out over p-flash. */
      printk(KERN_INFO "mpegOutUnitDisable:: Disable MPEG Out unit. \n");
      PAL_sysBootCfgCtrl_DocsisIo_MPEG_out(BOOTCFG_IO_DISABLE);


      /* 3. Set MPEG_OUT_MODULE state to Disable in general reset and clock registers*/
      printk(KERN_INFO "mpegOutUnitDisable:: Disable in general reset register. \n");
      socReadRegister(BuildPhyAddr(PHY_GENERAL_RESET_REGISTER),&regValue,True);
      /*-------------------------------------------------------------*/ 
      /* Bit  Name          Default   Description                    */ 
      /* 0    MCR1 Reset      0       MCR1 SW reset                  */ 
      /* 1    MCR2 Reset      0       MCR2 SW reset                  */ 
      /* 2    HSIF Reset      1       HSIF SW reset                  */ 
      /* 3    MPEG OUT Reset  1       MPEG OUT SW reset              */ 
      /* 15:4 Reserved                                               */ 
      /*-------------------------------------------------------------*/ 
      regValue |= (BIT_3);
      socWriteRegister(BuildPhyAddr(PHY_GENERAL_RESET_REGISTER), regValue, True);
      closePhyClockGating(True);

      return 0;
}
/* This API controls the Mpeg-Out source - it can be PHY (0) or CPPI01 (1) CPPI02 (2) source */
static Int32 mpegOutPortSource( socDevice_e device, Uint32 port, Uint32 source )
{
    Uint32 regValue;
    Uint16 portShiftVal = (port-1)*PUMA_MPEG_OUT_INTERFACE_SELECT_REG_TWO_BITS_PER_CH; /* port value 1-6   */

    if (port < 1 || port > PUMA_MPEG_OUT_MAX_PORT_NUM)
    {
       printk(KERN_ERR "mpegOutPortSource:: Invalid argument (MPEG Out port %d). \n",port);
       return -EINVAL;
    }

    printk(KERN_INFO "mpegOutPortSource:: Configure the MPEG Out CPPI port %d source %s.\n",port, (source)? "CPPI":"PHY" );

    socReadRegister(BuildPhyAddr(PUMA_MPEG_OUT_INTERFACE_SELECT_REG_OFFSET),&regValue,True);
    regValue  &=  ~( PUMA_MPEG_OUT_INTERFACE_SELECT_REGPORT_BITS_MASK << portShiftVal );
    switch (source)
    {
        case PUMA_MPEG_OUT_PHY_SRC:
            regValue |= (Uint32)(PUMA_MPEG_OUT_PHY_SRC_VAL << portShiftVal);
        break;
        case PUMA_MPEG_OUT_CPPI01_SRC:
            regValue |= (Uint32)(PUMA_MPEG_OUT_CPPI01_SRC_VAL << portShiftVal);
        break;
        case PUMA_MPEG_OUT_CPPI02_SRC:
            regValue |= (Uint32)(PUMA_MPEG_OUT_CPPI02_SRC_VAL << portShiftVal);
        break;
        case PUMA_MPEG_OUT_ALL_OUTPUT_ZERO:
            regValue |= (Uint32)(PUMA_MPEG_OUT_ALL_OUTPUT_ZERO_VAL << portShiftVal);
        break;
        default:
            return -EINVAL; 
    }
    socWriteRegister(BuildPhyAddr(PUMA_MPEG_OUT_INTERFACE_SELECT_REG_OFFSET),regValue,True);

    return 0;
}



#else




Int32 mpegOutUnitEnable( socDevice_e device )
{
      Uint32 regValue;

       /* 1. Kick the bootCfg lock registers.
        * 2. Write 1 to the "MPEG Out EN" (bit 2) to enable MPEG Out unit.
        * 3. Set LPSC_MPEG_OUT_MODULE state to Enable.
        * 4. Close all MPEG Out ports.
        * 5. Enable MPEG-Out serial interface. 
         */
      /* 1. Kick the bootCfg lock registers */
      printk(KERN_INFO "mpegOutUnitEnable:: Kick the BootCfg lock. \n");
      socWriteRegister((PUMA5_BOOTCFG_KICK_0_REG( device )), PUMA5_BOOTCFG_KICK_0_VAL, True);
      socWriteRegister((PUMA5_BOOTCFG_KICK_1_REG( device )), PUMA5_BOOTCFG_KICK_1_VAL, True);

      /* 2. Bit 2 "MPEG Out EN" When active, MPEG Out is ported out over p-flash. */
      printk(KERN_INFO "mpegOutUnitEnable:: Enable MPEG Out unit. \n");
      socReadRegister((PUMA5_PINMUX0_REG( device )),&regValue,True);
      regValue |= PUMA5_PINMUX0_MPEG_OUT_EN;
      socWriteRegister((PUMA5_PINMUX0_REG( device )), regValue, True);

      /* 3. Set LPSC_MPEG_OUT_MODULE state to Enable */
      socWriteRegister((PUMA5_LPSC_MPEG_OUT_MODULE( device )), PUMA5_LPSC_MPEG_OUT_MODULE_EN_VAL,True);
      socWriteRegister((PUMA5_PSC_GO_REG( device )), PUMA5_PSC_GO_VAL  ,True); 

      /* 4. Close all MPEG Out ports */
      mpegOutCloseAllPorts( device );

      /* 5. Enable MPEG-Out serial interface */
      socReadRegister((PUMA_MPEG_OUT_CONTROL_REG( device )),&regValue,True);
      socBitField32Set(&regValue,
                       PUMA_MPEG_OUT_INTFC_SELECT_EN_VAL,
                       PUMA_MPEG_OUT_INTFC_SELECT_OFFSET,
                       PUMA_MPEG_OUT_INTFC_SELECT_WIDTH);
      socWriteRegister((PUMA_MPEG_OUT_CONTROL_REG( device )),regValue,True);

      printk(KERN_INFO "mpegOutUnitEnable:: MPEG Out is working. \n");
      /* MPEG Out Control Register is now with the default values */

      return 0;
}

Int32 mpegOutUnitDisable( socDevice_e device )
{
      Uint32 regValue;

      /* 
       * 1. Kick the bootCfg lock registers.
       * 2. Close all MPEG Out ports. 
       * 3. Disable MPEG-Out serial interface (all bus is zero). 
       * 4. Write 0 to the "MPEG Out EN" (bit 2) to disable MPEG Out unit.
       * 5. Set LPSC_MPEG_OUT_MODULE state to Disable. 
       */

      /* 1. Kick the bootCfg lock registers */
      printk(KERN_INFO "mpegOutUnitDisable:: Kick the BootCfg lock. \n");
      socWriteRegister((PUMA5_BOOTCFG_KICK_0_REG( device )), PUMA5_BOOTCFG_KICK_0_VAL, True);
      socWriteRegister((PUMA5_BOOTCFG_KICK_1_REG( device )), PUMA5_BOOTCFG_KICK_1_VAL, True);

      /* 2. Close all MPEG Out ports */
      mpegOutCloseAllPorts( device );

      /* 3. Disable MPEG-Out serial interface (all bus is zero) */
      socReadRegister((PUMA_MPEG_OUT_CONTROL_REG( device )),&regValue,True);
      socBitField32Set(&regValue,
                       PUMA_MPEG_OUT_INTFC_SELECT_DIS_VAL,
                       PUMA_MPEG_OUT_INTFC_SELECT_OFFSET,
                       PUMA_MPEG_OUT_INTFC_SELECT_WIDTH);
      socWriteRegister((PUMA_MPEG_OUT_CONTROL_REG( device )),regValue,True);

      /* 4. Bit 2 "MPEG Out EN" When active, MPEG Out is ported out over p-flash. */
      printk(KERN_INFO "mpegOutUnitDisable:: Disable MPEG Out unit. \n");
      socReadRegister((PUMA5_PINMUX0_REG( device )),&regValue,True);
      regValue &= ~(PUMA5_PINMUX0_MPEG_OUT_EN);
      socWriteRegister((PUMA5_PINMUX0_REG( device )), regValue, True);

      /* 5. Set LPSC_MPEG_OUT_MODULE state to Disable */
      socWriteRegister(PUMA5_LPSC_MPEG_OUT_MODULE( device ), PUMA5_LPSC_MPEG_OUT_MODULE_DIS_VAL,True);
      socWriteRegister((PUMA5_PSC_GO_REG( device )), PUMA5_PSC_GO_VAL  ,True); 

      return 0;
}
/* This API controls the Mpeg-Out source - it can be PHY (0) or CPPI (1) source */
static Int32 mpegOutPortSource( socDevice_e device, Uint32 port, Uint32 source )
{
    Uint32 regValue;

    if (port < 1 || port > 4)
    {
       printk(KERN_ERR "mpegOutPortSource:: Invalid argument (MPEG Out port %d). \n",port);
       return -EINVAL;
    }

    printk(KERN_INFO "mpegOutPortSource:: Configure the MPEG Out CPPI port %d source %s.\n",port, (source)? "CPPI":"PHY" );

    socReadRegister((PUMA5_BOOTCFG_CPPI_SEL_REG( device )),&regValue,True);

    if (1 == source)
       regValue |= ((PUMA5_MPEG_OUT_CPPI_SELECT) << (port-1));
    else if ( 0 == source )
       regValue &= ~((PUMA5_MPEG_OUT_CPPI_SELECT) << (port-1));
    else
       return -EINVAL; 

    socWriteRegister((PUMA5_BOOTCFG_CPPI_SEL_REG( device )),regValue,True);

    return 0;
}

#endif

EXPORT_SYMBOL( mpegOutUnitGlobalConfig );
EXPORT_SYMBOL( mpegOutPortSet );
EXPORT_SYMBOL( mpegOutUnitEnable );
EXPORT_SYMBOL( mpegOutUnitDisable );
EXPORT_SYMBOL( mpegOutPortSource );
