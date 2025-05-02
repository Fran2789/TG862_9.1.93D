/*
 * File name: mmxptimer.c
 *
 * Description: MXP kernel timer module. This file is included in mmxpcore.c.
 *
 * Copyright (C) 2011 Texas Instruments, Incorporated
 * 
 * This program is free software; you can redistribute it and/or modify it
 * under the terms of the GNU General Public License as published by the
 * Free Software Foundation version 2.
 *
 * This program is distributed "as is" WITHOUT ANY WARRANTY of any kind,
 * whether express or implied; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * General Public License for more details.
 */

#include <asm-arm/arch-avalanche/generic/pal.h>
extern void avalanche_start_timer1_sync(unsigned int loadNs);

#define AVAL_MXP_TMR_REG AVALANCHE_TIMER1_BASE

#if defined(CONFIG_MACH_PUMA5)
#define AVAL_MXP_PRCR_REG AVALANCHE_RST_CTRL_PRCR
#define AVAL_MXP_PRCR_TMR_VAL (1<<AVALANCHE_TIMER1_RESET_BIT)  /*what value to put/take out of reset the timer.*/
#endif

#define AVAL_MXP_TMR_IRQ (8+AVALANCHE_TIMER_1_INT) /* **TODO Verify Primery interrupt map- Puma6.h */ /* The 8 is due to kernel considerations */
#define MXP_TIMER_PERIOD 5000 /* 5000 usec */

/* pointer to av-1 hardware registers used to configure timer interrupt  (timer 1)*/
#if defined(CONFIG_MACH_PUMA5)
unsigned int   volatile *prcr_reg = (unsigned int volatile *)AVAL_MXP_PRCR_REG;
#endif

int mmxp_timer_init(void)
{
    unsigned long cpufreq;

    /* set timer1 for 2ms clock */
    do {
        UINT32 timer_return_value;
        UINT32 ref_frequency;

#if defined(CONFIG_MACH_PUMA6)
        if(PAL_sysGetResetStatus(CRU_NUM_TIMER1) == OUT_OF_RESET)
        {
        	printk("Warning: TIMER1 may already be in use\n");
        }
#elif defined(CONFIG_MACH_PUMA5)
        if((*prcr_reg  & AVAL_MXP_PRCR_TMR_VAL))
        {
            printk("Warning: TIMER1 may already be in use\n");
        }
#endif
        printk(KERN_ERR "MXP_TMR: Calibrating MXP Timer...   Ticks/sec=%d\n", GG_TICKS_PER_SEC);

#if defined(CONFIG_MACH_PUMA6)
        PAL_sysResetCtrl(CRU_NUM_TIMER1, IN_RESET);
        PAL_sysResetCtrl(CRU_NUM_TIMER1, OUT_OF_RESET);
        ref_frequency = PAL_sysClkcGetFreq(PAL_SYS_CLKC_TIMER1);

#elif defined(CONFIG_MACH_PUMA5)
        *prcr_reg &= ~AVAL_MXP_PRCR_TMR_VAL;
        *prcr_reg |= AVAL_MXP_PRCR_TMR_VAL;
        ref_frequency = PAL_sysClkcGetFreq(CLKC_VBUS);
#endif

        timer_return_value =
            PAL_sysTimer16SetParams(AVALANCHE_TIMER1_BASE, ref_frequency, 
                    TIMER16_CNTRL_AUTOLOAD, MXP_TIMER_PERIOD, NULL);
        if(timer_return_value == -1)
        {
            printk("Error setting parameters for timer\n");
            return 1;
        }

        printk("Starting MXP timer\n");
        /* Start Timer1 in Sync with the Tick timer */
        avalanche_start_timer1_sync(MXP_TIMER_PERIOD * NSEC_PER_USEC);

    }  while (0);

    tmrobj_init();
    mxl_tmr_init();
    q_Init();

    if (request_irq(LNXINTNUM(AVALANCHE_TIMER_1_INT), mxp_timer_irq_handle, SA_INTERRUPT, "mxp_timer", NULL))
    {
        printk("Cannot register mxt_timer interrupt\n");
        return 1;
    }

    return 0;
}

int mmxp_timer_cleanup(void)
{
    free_irq(AVAL_MXP_TMR_IRQ, NULL);
    /* Turn off timer subsystem since we don't need it */
#if defined(CONFIG_MACH_PUMA6)
    PAL_sysResetCtrl(CRU_NUM_TIMER1, IN_RESET);
#elif defined(CONFIG_MACH_PUMA5)
    *prcr_reg &= ~AVAL_MXP_PRCR_TMR_VAL;
#endif
    return 0;
}
