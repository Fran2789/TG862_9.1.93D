/*
 * (C) Copyright 2004
 * Texas Instruments
 * Richard Woodruff <r-woodruff2@ti.com>
 *
 * (C) Copyright 2002
 * Sysgo Real-Time Solutions, GmbH <www.elinos.com>
 * Marius Groeger <mgroeger@sysgo.de>
 * Alex Zuepke <azu@sysgo.de>
 *
 * (C) Copyright 2002
 * Gary Jennejohn, DENX Software Engineering, <gj@denx.de>
 *
 * See file CREDITS for list of people who contributed to this
 * project.
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License as
 * published by the Free Software Foundation; either version 2 of
 * the License, or (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 59 Temple Place, Suite 330, Boston,
 * MA 02111-1307 USA
 */

/* Copyright 2008, Texas Instruments Incorporated
 *
 * This program has been modified from its original operation by Texas Instruments
 * to do the following:
 * 1) Separatees out Timer specific functionality. It uses on-chip timer and
 * configures it for the timer specification as per CFG_HZ and the frequency.
 * 
 * Derived from : cpu/arm1136/interrupts.c
 *
 *
 * THIS MODIFIED SOFTWARE AND DOCUMENTATION ARE PROVIDED
 * "AS IS," AND TEXAS INSTRUMENTS MAKES NO REPRESENTATIONS
 * OR WARRENTIES, EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED
 * TO, WARRANTIES OF MERCHANTABILITY OR FITNESS FOR ANY
 * PARTICULAR PURPOSE OR THAT THE USE OF THE SOFTWARE OR
 * DOCUMENTATION WILL NOT INFRINGE ANY THIRD PARTY PATENTS,
 * COPYRIGHTS, TRADEMARKS OR OTHER RIGHTS.
 * See The GNU General Public License for more details.
 *
 * These changes are covered under version 2 of the GNU General Public License,
 * dated June 1991.
 */


/* 
 * Includes Intel Corporation's changes/modifications dated: 2011. 
 * Changed/modified portions - Copyright © 2011 , Intel Corporation.   
 */ 


#include <common.h>
#include <puma6.h>


/* Use hard delay (nops) */
/*#define CONFIG_HARD_DELAY*/

#if !defined ABS_TIMER && (CFG_HZ != 1000)
#error This driver only supports CFG_HZ @1000
#endif

#define TIMER0_BASE           0x00030000

#define TIMER16_CNTRL_PRESCALE_ENABLE       0x8000
#define TIMER16_CNTRL_PRESCALE              0x003C
#define TIMER16_CNTRL_MODE                  0x0002

#define TIMER16_MINPRESCALE                 2
#define TIMER16_MAXPRESCALE                 8192
#define TIMER16_MIN_LOAD_VALUE              1
#define TIMER16_MAX_LOAD_VALUE              0xFFFF

#define MHZ                                 1000000

/* set min clock divisor to a little higher value
 * so that we are not close to the edge.
 * so multiply by factor 2
 */
#define TIMER16_MAX_CLK_DIVISOR (TIMER16_MAX_LOAD_VALUE * TIMER16_MAXPRESCALE)
#define TIMER16_MIN_CLK_DIVISOR (TIMER16_MIN_LOAD_VALUE * TIMER16_MINPRESCALE * 2)

unsigned int refclk_mhz;    /* Value determined by PLL settings */

#define TIMER16_CNTRL_AUTOLOAD  2

/* macro to read the 32 bit timer */
#define READ_TIMER (*((volatile unsigned int*) TIMER0_BASE+2))

static ulong timestamp;
static ulong lastdec;
#ifndef ABS_TIMER
static ulong prevdec;
#endif
int timer_load_val = 0;

/*=|=|=||=|=| TODO : Find a better place for these functions |=|=|=|=|=|= */
#ifndef CFG_EXTERNAL_CLK

/********************************************
 * get_bus_freq
 * return system bus freq in Hz
 *********************************************/
ulong get_bus_freq (ulong dummy)
{
    return FPLL_1xCLK_FREQ;
}
#endif /* !CFG_EXTERNAL_CLK */


/* nothing really to do with interrupts, just starts up a counter. */
int interrupt_init (void)
{
        volatile unsigned int *timer_base = (unsigned int*) TIMER0_BASE;

        unsigned int usec =  (unsigned int)((1.0/(float)(CFG_HZ)) * 1000000.0);
        unsigned int prescale;
        unsigned int count;
    
#ifdef CFG_EXTERNAL_CLK
        refclk_mhz = AVALANCHE_SYS_CLKC_FREQ/2;
#else
        refclk_mhz = get_bus_freq(0) / MHZ ; /* TODO: need to be float */
        /* BRINGUP - Added */refclk_mhz *=2;
#endif /* !CFG_EXTERNAL_CLK */

        
       
        /* The min time period is 1 usec and since the reference clock freq
         * is always going to be more than "min" divider value, minimum
         * value is not checked.  Check the max time period that can be
         * derived from the timer in micro-seconds 
         */
        if (usec > ( (TIMER16_MAX_CLK_DIVISOR) / refclk_mhz ))
        {
            return -1; /* input argument speed, out of range */
        }
        count = refclk_mhz * usec;
        for(prescale = 0; prescale < 12; prescale++)
        {
           count = count >> 1;
           if( count <= TIMER16_MAX_LOAD_VALUE )
           {
               break;
           }
        }

        /*write the load counter value*/
        *(timer_base + 1) = count;
        timer_load_val  = count;

        /* write prescalar and mode to control reg */
        *timer_base = TIMER16_CNTRL_AUTOLOAD | TIMER16_CNTRL_PRESCALE_ENABLE | (prescale << 2 );
                        
        reset_timer_masked(); /* init the timestamp and lastinc value */

        /* Start the timer */
        *timer_base |= 1;
        
        return(0);
}
/*
 * timer without interrupts
 */
void reset_timer (void)
{
	reset_timer_masked ();
}

ulong get_timer (ulong base)
{
    return get_timer_masked () - base;
}

void set_timer (ulong t)
{
	timestamp = t;
}

void delay10sec(void)
{
        printf ("Delay 10sec: ");
        udelay (10000000);
        printf ("done.\n");
}

void delay1min(void)
{
        printf ("Delay 1min: ");
        udelay (60000000);
        printf ("done.\n");
}


/* delay x useconds AND perserve advance timstamp value */
void udelay (unsigned long usec)
{
#ifdef CONFIG_HARD_DELAY
    while (usec--)
    {
        asm volatile (" NOP");
        asm volatile (" NOP");
        asm volatile (" NOP");
        asm volatile (" NOP");
        asm volatile (" NOP");
        asm volatile (" NOP");
        asm volatile (" NOP");
        asm volatile (" NOP");
        asm volatile (" NOP");
        asm volatile (" NOP");
        asm volatile (" NOP");
        asm volatile (" NOP");
        asm volatile (" NOP");
    }
#else
        ulong tmo;
        ulong start = get_timer(0);

        tmo = usec / 1000;
        tmo = (tmo ? tmo : 1);
#ifdef ABS_TIMER
        tmo *= (timer_load_val * 100);
        tmo /= 1000;
#endif

        while ((ulong)(get_timer_masked () - start) < tmo)
        /*NOP*/;
    
#endif
}

void reset_timer_masked (void)
{
	/* reset time */
    lastdec = READ_TIMER;		/* capture current incrementer value time */
#ifndef ABS_TIMER
    prevdec = lastdec/2;
#endif
	timestamp = 0;				/* start "advancing" time stamp from 0 */
}

#if 0
ulong get_timer_masked (void)
{
	ulong now = READ_TIMER;		/* current tick value */

	if (now >= lastinc)			/* normal mode (non roll) */
		timestamp += (now - lastinc); /* move stamp fordward with absoulte diff ticks */
	else						/* we have rollover of incrementer */
		timestamp += (0xFFFF - lastinc) + now;
	lastinc = now;
	return timestamp;
}
#else

ulong get_timer_masked (void)
{
    ulong now = READ_TIMER;
    if (lastdec >= now) {
    /* normal mode */
#ifdef ABS_TIMER
        timestamp += lastdec - now; 
#else
        if (prevdec < now) {
            timestamp++;
            lastdec = now;
        }
        prevdec = now;
            
#endif
    } else {
        /* we have an overflow ... */
#ifdef ABS_TIMER
        timestamp += lastdec + timer_load_val - now; 
#else
        timestamp++; 
    lastdec = now;

#endif
    }
#ifdef ABS_TIMER
    lastdec = now;
#endif
    return timestamp; 
}
#endif


/* waits specified delay value and resets timestamp */
void udelay_masked (unsigned long usec)
{
    udelay (usec);
    reset_timer ();
}

/*
 * This function is derived from PowerPC code (read timebase as long long).
 * On ARM it just returns the timer value.
 */
unsigned long long get_ticks(void)
{
	return get_timer(0);
}
/*
 * This function is derived from PowerPC code (timebase clock frequency).
 * On ARM it returns the number of timer ticks per second.
 */
ulong get_tbclk (void)
{
	ulong tbclk;
	tbclk = CFG_HZ;
	return tbclk;
}

