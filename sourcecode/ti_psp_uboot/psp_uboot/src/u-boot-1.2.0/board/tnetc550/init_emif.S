/*
 * init_emif.S
 * Description:
 * This file contains functions to initialize the EMIF and RAM related
 * functions. The code in this file assumes that data-cache is disabled.
 *
 * (C) Copyright 2007-2009 Texas Instruments, <www.ti.com>
 *
 *      25Oct06 BEGR
 *	- Platform-specific EMIF constants are now provided externally via the
 *	hardware.inc file. The <platform_name>.inc file is copied to hardware.inc
 *	by the clittle/cbig compile script.
 *
 *      August 2009: Hai Shalom
 *      - Added support for 1Gb DDR2 device
 *      - Fix all timing registers values per each device
 *      - Code cleanup
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
 */


/*
 * InitEmif
 *     Initializes the device base memory and memory controller
 *     Returns r12=0 for OK, and r11 as amount of SDRAM found
 */
       
#include <config.h>

#define EMSU                    1
#define EMSTROBE                26
#define EMHD                    3
#define RCS5_SIZE               2
#define CFG_SDRAM_TIMING1       0x289242C9   /* ARRIS change timing to support more RAM vendors */
#define CFG_SDRAM_TIMING2       0x3d16c722   /* ARRIS ODT/CAS change */

#define ARRIS_DDR_PATCH		/* ARRIS workaround for improper DDR size detection */

#define EMTA                    1
#define EM64		        3
#define EM32                    2
#define EM16                    1
#define EM08                    0

#define CLK_CTRL_BASE           0x08610A00

#define DDR2_TIMING_LOCK        (1<<15)
#define DDR2_CAS_MASK           0x001b8400
#define DDR2_CAS_LOCK           0x001b0400
#define MASK1                   0x0000ffff
#define MASK2                   0xffff0000
#define CONFIG1                 ((EMSU<<26)|(EMSTROBE<<20)|(EMHD<<17)|(EMSU<<13)|(EMSTROBE<<7)|(EMHD<<4)|(EMTA<<2))
#if defined CONFIG_PUMA5_VOLCANO_EMU || defined CONFIG_PUMA5_QT_EMU
#define CONFIG2                 ((EMSU<<24)|(EMSTROBE<<18)|(EMHD<<15)|(EMSU<<11)|(EMSTROBE<<5)|(EMHD<<2)|(1<<29))
#else
#define CONFIG2                 (0x0030E100)
#endif
#define IS_EMIF_3X              (1<<0)
#define IS_SDRAM		(1<<1)
#define IS_DDR			(1<<2)
#define IS_DDR2			(1<<3)

#define ASYNC_EMIF3X    IS_EMIF_3X
#define SDRAM_EMIF3X    (IS_EMIF_3X<<4)
#define SDRAM_SDRAM     (IS_SDRAM<<4)
#define SDRAM_DDR       (IS_DDR<<4)
#define SDRAM_DDR2      (IS_DDR2<<4)

/**
 * Read Latency configuration 
 */
#define CONFIG_CAS_LATENCY                  (5) /* ARRIS - use 5 since we have ODT turned on, per TI */
#define CONFIG_READ_LATENCY                 (CONFIG_CAS_LATENCY+1) 

/**
 * DDR PHY Control register bits 
 */
#define CONFIG_EXT_STRBEN                   (1<<7)
#define CONFIG_PWRDNEN                      (1<<6)
#define CONFIG_PAD_TERMINATION_HALF         (1<<5) /*ARRIS*/
#define DDR_PHY_CTL1_CONFIG                 ( CONFIG_EXT_STRBEN | CONFIG_PWRDNEN | CONFIG_READ_LATENCY | CONFIG_PAD_TERMINATION_HALF) /*ARRIS*/

/** 
 * Timing registers configuration 
 */
#if defined CONFIG_PUMA5_VOLCANO_EMU || defined CONFIG_PUMA5_QT_EMU

/** 
 * Refresh rate for 20MHz initial value */
#define REFRESHR                (78*20)/10
#else

/** 
 * Refresh rate for 200MHz 
 */
#define REFRESHR                (78*200)/10  

/** 
 * 1Gb DDR2 device 
 */
#define CFG_SDRAM_1G_TIMING1	0x32923A89	/* Timing1 for 1Gb */
#define CFG_SDRAM_1G_TIMING2	0x3D1BC722	/* Timing2 for 1Gb */

/** 
 * 512Mb DDR2 device 
 */
#define CFG_SDRAM_512M_TIMING1	0x28923A89	/* Timing1 for 512Mb */
#define CFG_SDRAM_512M_TIMING2	0x3D16C722	/* Timing2 for 512Mb */

/** 
 * 256Mb DDR2 device 
 */
#define CFG_SDRAM_256M_TIMING1  0x1C9242C9      /* Timing1 for 256Mb */
#define CFG_SDRAM_256M_TIMING2  0x3D10C722      /* Timing2 for 256Mb */

#endif

/**
 * SRAM EMIF Control Registers
 */
#define SDRAM_EMIF_CTL_BASE     0x20000000                      /* !@T OLDMM 0x08613000 */
#define ADDR_EMIF_STATUS        (SDRAM_EMIF_CTL_BASE+0x04)      /* ARRIS */
#define ADDR_EMIF_DRAMCTL       (SDRAM_EMIF_CTL_BASE+0x08)
#define ADDR_EMIF_REFRESH       (SDRAM_EMIF_CTL_BASE+0x0c)
#define ADDR_EMIF_SDRAM_TMING1  (SDRAM_EMIF_CTL_BASE+0x10)
#define ADDR_EMIF_SDRAM_TMING2  (SDRAM_EMIF_CTL_BASE+0x14)

/**
 * DDR PHY Control Registers
 */
#define DDR_PHY_CTL_BASE        0x200000E0
#define DDR_PHY_ID              (DDR_PHY_CTL_BASE+0x00)
#define DDR_PHY_CTL1            (DDR_PHY_CTL_BASE+0x04)
#define DDR_PHY_CTL2            (DDR_PHY_CTL_BASE+0x08)
#define DDR_PHY_CTL3            (DDR_PHY_CTL_BASE+0x0C)
#define DDR_PHY_VTP_IO_CTL      (DDR_PHY_CTL_BASE+0x10)

/** 
 * Async EMIF Control Registers
 */
#define     ASYNC_EMIF_CTL_BASE     0x08610800
#define     ADDR_EMIF_REV           (ASYNC_EMIF_CTL_BASE+0x00)
#define     ADDR_EMIF_GASYNC        (ASYNC_EMIF_CTL_BASE+0x04)
#define     ADDR_EMIF_ASYNC_CS0     (ASYNC_EMIF_CTL_BASE+0x80)
#define     ADDR_EMIF_ASYNC_CS3     (ASYNC_EMIF_CTL_BASE+0x84)
#define     ADDR_EMIF_ASYNC_CS4     (ASYNC_EMIF_CTL_BASE+0x88)
#define     ADDR_EMIF_ASYNC_CS5     (ASYNC_EMIF_CTL_BASE+0x8c)
    

patt:   .long   0x55555555, 0xaaaaaaaa, 0x33333333, 0xcccccccc, 0x0f0f0f0f, 0xf0f0f0f0, 0x00ff00ff, 0xff00ff00, 0x0000ffff, 0xffff0000, 0x00000000, 0xffffffff,0x00000000, 0x55555555, 0xaaaaaaaa, 0x33333333,0xcccccccc, 0x0f0f0f0f, 0xf0f0f0f0, 0x00ff00ff,0xff00ff00, 0x0000ffff, 0xffff0000, 0x00000000,0xffffffff


/**
 * DetermineEmifType
 * 	Entry:
 * 	        r0 = Base address of EMIF memory controller
 * 	Exit:
 * 		r0 = Bit-mask containing EMIF type information
 * 		        r0[0] = EMIF3.x? 	(0=no, 1=yes)
 * 		        r0[1] = SDRAM?	 	(0=async only, 1=SDRAM(SDR, DDR1, or DDR2))
 * 		        r0[2] = DDR		(0=SDR, 1=DDR(DDR1 or DDR2))
 * 		        r0[3] = DDR2		(0=DDR1, 1=DDR2)
 */
DetermineEmifType: 
    MOV	  r2, #0
    LDR   r1, [r0]
    MOV   r1, r1, LSR #20
    AND   r1, r1, #0x3
    CMP	  r1, #3
    BNE   done
    ORR	  r2, #IS_EMIF_3X
    ADD	  r0, r0, #8
    LDR   r1, [r0]				/* Load EMIF DRAM control*/
    MOV	  r1, r1, LSR #16			/* Move SDRAM bit into LSB*/
    TST	  r1, #1
    BEQ	  done
    ORR	  r2, #IS_SDRAM
    MOV	  r1, r1, LSR #1			/* Move DDR bit into LSB*/
    TST	  r1, #1
    BEQ	  done
    ORR	  r2, #IS_DDR
    MOV	  r1, r1, LSR #3			/* Move DDR2 bit into LSB*/
    TST	  r1, #1
    BEQ	  done
    ORR	  r2, #IS_DDR2
done:
    MOV	  r0, r2
    BX	  lr

/* InitEmif
 *
 * Initialize EMIF memory controller
 */

.globl InitEmif
InitEmif: 
      MOV   sp, lr					/* save link register */

      /* Determine EMIF revisions */
      LDR   r0, =ASYNC_EMIF_CTL_BASE
      BL    DetermineEmifType
      MOV   r5, r0
      LDR   r0, =SDRAM_EMIF_CTL_BASE
      BL    DetermineEmifType
      ORR   r5, r5, r0, LSL #4

      /* Speed up flash accesses (initialize value in EMIF_ASYNC_CS0 register) */
      LDR   r8, =CONFIG1				/* Load default async control value*/
      TST   r5, #ASYNC_EMIF3X
      LDRNE r8, =CONFIG2				/* Statement will execute for EMIF 3.x only*/
      LDR   r3, =ADDR_EMIF_ASYNC_CS0
      LDR   r6, [r3]						/* Get current boot width*/
      MOV   r4, r8							/* Get async timing*/
      AND   r6, r6, #3					/* Isolate boot width*/
      ORR   r4, r4, r6					/* Add in boot width*/
      STR   r4, [r3]						/* Set new timing*/

      /* Initialize memory refresh, will be slowed down later*/
init_mem_refresh:
      LDR   r3, =ADDR_EMIF_REFRESH
      LDR   r4, =REFRESHR /*; Refresh = clk freq in MHz * 7.8*/
      STR   r4, [r3]

#if !defined CONFIG_PUMA5_VOLCANO_EMU && !defined CONFIG_PUMA5_QT_EMU
      LDR   r0, =ADDR_EMIF_DRAMCTL
      LDR   r2, [r0]

      /* ARRIS ODT/CAS change -- set bit27 */
      ORR r2, r2, #0x00800000
      STR r2, [r0]
      BIC r2,     #0x00800000
      ORR r2, r2, #0x08000000
      STR r2, [r0]
      /* END ARRIS ODT/CAS change */

      /* timing_unlock=1 */
      ORR   r2, r2, #DDR2_TIMING_LOCK

      /* Configure EMIF to use a CAS Latency of 3 intead of the default CL of 5. 
       * This is required because the DDR2 device used on the Puma-5 DVDB does
       * not support a CL of 5, and 3 is recommended for 400Mz DDR clock.
       */
      MOV   r4, #0x7
      BIC   r2, r2, r4, LSL #9     /* Clear cl field [11:9] */
      MOV   r4, #CONFIG_CAS_LATENCY
      ORR   r2, r2, r4, LSL #9     /* Set cl field to 5 ARRIS */      
      STR   r2, [r0]
      
      /* Configure t_xp for DDR2 devices, stay with EMIF defaults for now */
      LDR r3, =ADDR_EMIF_SDRAM_TMING2	
      LDR r4, [r3]
      ORR r4, r4, #(1<<26)
      BIC r4, r4, #(1<<25)
      STR r4, [r3]

      /* timing_unlock=0 */
      BIC   r2, r2, #DDR2_TIMING_LOCK
      STR   r2, [r0]

      /* Configure the DDR PHY to use a read latency of CAS latency + 1. */
      LDR   r3, =DDR_PHY_CTL1
      LDR   r2, [r3]
      MOV   r4, #0x7
      BIC   r2, r2, r4      			  /* Clear read_latency field [2:0] */
      ORR   r2, r2, #DDR_PHY_CTL1_CONFIG  /* Set read_latency field to CAS+1= 5+1= 6 ARRIS */
      STR   r2, [r3]        			   /* Write updated DDR PHY Control 1 Register */

#endif

      LDR   r0, =ADDR_EMIF_DRAMCTL
      LDR   r2, [r0]		/* Save EMIF_DRAMCTL register value for later (in r2)*/
      BIC   r2, r2, #(1<<14)    /* Clear narrow mode bit */
      BIC   r2, r2, #0xF        /* Clear ebank and pagesize fields */

      /* Configure the rest of the EMIF*/
      LDR   r6, =PHYS_SDRAM_1
      MOV   r3, r2
      BL    Mode16
      MOV   r0, r12

      /* Determine the number of columns in the SDRAM*/
      MOV   r3, r2   		/* test for 8 bits  ( 256w) r3 = 0*/
      BL    CheckForWrap
      CMP   r12, #0
      BNE   badmem

      ADD   r3, r3, #1   /* test for 9 bits  ( 512w) r3 = 1*/
      BL    CheckForWrap
      CMP   r12, #0
      BNE   numcol

      ADD   r3, r3, #1   /* test for 10 bits (1024w) r3 = 2*/
      BL    CheckForWrap
      CMP   r12, #0
      BNE   numcol

      ADD   r3, r3, #1   /* test for 11 bits (2056w) r3 = 3*/
      BL    CheckForWrap
      CMP   r12, #0
      BNE   numcol
badmem:
#ifdef ARRIS_DDR_PATCH
      ADD   r3, r3, #3	/* ARRIS default to 64MB rather than 16 */
#else
      ADD   r3, r3, #1
#endif
numcol:
      SUB   r10, r3, #1
      AND   r10, r10, #0x7  /* Save the column size */

      /* Determine if there are 8 internal DRAM banks (ibank) */    
      ORR   r3, r2, #(1<<4)         /* set #ibanks to 8 */
      BL    CheckForWrap
      ORR   r10, r10, #(1<<4)       /* set #ibanks to 8 */
      CMP   r12, #0
      BEQ   ibank8
ibank4:
      BIC   r10, r10, #(1<<4)      /* set #ibanks to 4 */
ibank8:

      /* Determine if there are 2 external DRAM banks*/
      MOV   r3, #(1<<3)				/*	; set #ebanks to 2*/
      BL    CheckForWrap
      CMP   r12, #0
      BEQ   bank2

      /* Software workaround for ASIC DDR PHY issue where FIFO pointers can get
      out of sync. This workaround is required only for EMIF 3.x and DDR. */

      TST	r5, #SDRAM_DDR
      BEQ	bank1
      LDR	r4, =SDRAM_EMIF_CTL_BASE
      ADD	r4, r4, #0x60			/* r4 = address of IODFT Test Logic Global Control Register */
      MOV	r3, #1
      MOV	r3, r3, LSL #10			/* r3 = PHY reset bit mask (bit 10)*/
      LDR	r5, [r4]			/* Read IODFT Test Logic Global Control Register*/
      ORR	r5, r5, r3			/* Set PHY reset bit... */
      STR	r5, [r4]			/* ... and write it out*/
WaitForSelfClear:
      LDR	r5, [r4]
      ANDS	r5, r5, r3
      BNE	WaitForSelfClear
bank1:
      MOV   r3, #0							/*; set #ebanks to 0*/
bank2:
      ORR   r4, r3, r10         /* OR in columns (r10) and #ebanks (r3)*/
      ORR   r4, r4, r0          /* OR in result of narrow mode check (r0)*/
      ORR   r3, r2, r4          /* OR in with saved DRAMCTL register contents*/
      LDR   r6, =ADDR_EMIF_DRAMCTL
      STR   r3, [r6]

      /* Bank 4 - SRAM, ASCII, LED, DIP-SW, UARTs CS3*/
      LDR   r3, =ADDR_EMIF_ASYNC_CS3
      ORR   r4, r8, #EM32
      STR   r4, [r3]

      /* Bank 5 - EEPROM CS4*/
      LDR   r3, =ADDR_EMIF_ASYNC_CS4
      ORR   r4, r8, #EM08
      STR   r4, [r3]

      /* Bank 6 - PHYs CS5*/
      LDR   r3, =ADDR_EMIF_ASYNC_CS5
      ORR   r4, r8, #RCS5_SIZE
      STR   r4, [r3]

      BX    sp  /* return to saved link register*/

/* CheckForWrap
 *	On entry:
 *		r3 = mode (0=check for 8-bits, 1=9-bits, 2=10-bits, 3=11-bits)
 *		r6 = base address
 *
 *	On exit:
 *     	r12 = wrap status (0=no wrap, 1=wrap)
 *
 *	Destroyed: r4, r5, r9, r12
 */

CheckForWrap:
      LDR   r4, =ADDR_EMIF_DRAMCTL
      ORR   r1, r3, r0
      STR   r1, [r4]         /* ; Configure SDRAM controller*/

      /* ARRIS patch for DDR issue; delay after configuring controller */
#ifdef ARRIS_DDR_PATCH
      MOV r12, #0x20000 
DramDelay:
      SUB r12, r12, #0x1
      CMP r12, #0x0
      BNE DramDelay  
      
      /* now make sure DDR PHY is ready */
      LDR r12, =ADDR_EMIF_STATUS 
DramReadyDelay:
      LDR r9, [r12]
      AND r9, r9, #0x4
      CMP r9, #0x4
      BNE DramReadyDelay  
#endif
      /* END ARRIS */

      MOV   r4, #8
      MOV   r1, #256
      STR   r6, [r6]          /*; Set wrap test value*/
CFW1:
      ADD   r12, r1, r6
      STR   r12, [r12]        /*; Write to index 256, 512, etc*/
      ADD   r1, r1, r1
      SUBS  r4, r4, #1
      BNE   CFW1
      /* ARRIS add NOPs between critical instructions */
#ifdef ARRIS_DDR_PATCH
      ORR		r1, r1, #0
      ORR		r1, r1, #0
      ORR		r1, r1, #0
      ORR		r1, r1, #0
      ORR		r1, r1, #0
      ORR		r1, r1, #0
      ORR		r1, r1, #0
      ORR		r1, r1, #0
#endif
      LDR   r9, =SDRAM_EMIF_CTL_BASE		/*; Interlock:*/
#ifdef ARRIS_DDR_PATCH
      ORR		r1, r1, #0
      ORR		r1, r1, #0
      ORR		r1, r1, #0
      ORR		r1, r1, #0
      ORR		r1, r1, #0
      ORR		r1, r1, #0
      ORR		r1, r1, #0
      ORR		r1, r1, #0
#endif
      STR   r9, [r9]				/*;   Insure prior aliased writes have*/
#ifdef ARRIS_DDR_PATCH
      ORR		r1, r1, #0
      ORR		r1, r1, #0
      ORR		r1, r1, #0
      ORR		r1, r1, #0
      ORR		r1, r1, #0
      ORR		r1, r1, #0
      ORR		r1, r1, #0
      ORR		r1, r1, #0
#endif
      LDR   r9, [r9]				/*;   completed prior to next read*/
#ifdef ARRIS_DDR_PATCH
      ORR		r1, r1, #0
      ORR		r1, r1, #0
      ORR		r1, r1, #0
      ORR		r1, r1, #0
      ORR		r1, r1, #0
      ORR		r1, r1, #0
      ORR		r1, r1, #0
      ORR		r1, r1, #0
#endif
      /* END ARRIS add NOPs between critical instructions */
      
      LDR   r9, [r6]          /*; Get wrap test value*/
      CMP   r6, r9
      BNE   s1_wrap           /*; Jump if memory wrapped*/
      MOV   r4, #8
      MOV   r1, #256
CFW2:
      ADD   r12, r1, r6
      LDR   r9, [r12]         /*; Verify to index 256, 512, etc*/
      CMP   r9, r12
      BNE   s1_wrap
      ADD   r1, r1, r1
      SUBS  r4, r4, #1
      BNE   CFW2
      MOV   r12, #0           /*; Indicate no wrap error*/
      BX    lr
s1_wrap:
      MOV   r12, #1
      BX    lr


/* Mode16
 *
 * Uses r3 = mode, r6 = address
 * Destroys r0, r4, r1, r9, r12  
 */
Mode16:
      LDR   r4, =ADDR_EMIF_DRAMCTL
      STR   r3, [r4]    /* Configure SDRAM Controller*/
      LDR   r4, =patt
      MOV   r1, #13
      MOV   r12, #0
m16lp:
      LDR   r9, [r4], #4
      STR   r9, [r6]
      STR   r1, [r6, #4]
      LDR   r0, [r6]
      EOR   r0, r0, r9
      ORR   r12, r12, r0
      SUBS  r1, r1, #1
      BNE   m16lp
      LDR   r1, =MASK1
      ANDS  r4, r12, r1
      BNE   m32s
      LDR   r1, =MASK2
      CMP   r12, r1
      BNE   m32s
      MOV   r12, #0x4000
      BX    lr
m32s:
      MOV   r12, #0
      BX    lr

/* GetSdramSize
 *
 * Find out how much SDRAM we have. Assume we have at least 64KB.
 * Configure the timing registers accordingly.
 * 
 */
.globl GetSdramSize
GetSdramSize: 
      LDR	 r0, =EMIF_SDRAM_MAXSZ_DIV2
      LDR	 r1, =SDRAM_BASE_VIRT_NC		/* Uncached accesses*/
      LDR	 r3, [r1]					/* Save 1st word of SDRAM into r3*/
gss1:	ADD	 r2, r0, r1
      STR	 r0, [r2]
      MOV	 r0, r0, LSR #1
      CMP	 r0, #0x10000
      BGE	 gss1
      LDR	 r0, [r1]
      CMP	 r0, r3
      LDREQ  r0, =EMIF_SDRAM_MAXSZ	/* If no wrap occurred, we have maximum SDRAM size*/
      STRNE	 r3, [r1]

#if !defined( CONFIG_PUMA5_VOLCANO_EMU ) && !defined( CONFIG_PUMA5_QT_EMU )

/* Configure SDRAM timing registers according to the detected platform.
   Use defined values for 1Gb platforms. For others, use the 512Mb values (good for 512/256Mb).
   The routine expects the amount of memory in r0 */

      LDR   r5, =ADDR_EMIF_DRAMCTL
      LDR   r2, [r5]

      /* Check if 256Mb device was detected */
      CMP   r0, #0x02000000
      BNE   cfgTry512
      LDR   r6, =CFG_SDRAM_256M_TIMING1
      LDR   r4, =CFG_SDRAM_256M_TIMING2
      B     cfgWriteTimings
cfgTry512:
      /* Check if 512Mb device was detected */
      CMP   r0, #0x04000000
      BNE   defaultTimings
      LDR   r6, =CFG_SDRAM_512M_TIMING1
      LDR   r4, =CFG_SDRAM_512M_TIMING2
      B     cfgWriteTimings
defaultTimings:
      /* Use default (slowest) timing configuration.
         This configuration is good for 1Gb device */
      LDR   r6, =CFG_SDRAM_1G_TIMING1
      LDR   r4, =CFG_SDRAM_1G_TIMING2

cfgWriteTimings:
      /* timing_unlock=1 */
      ORR   r2, r2, #DDR2_TIMING_LOCK
      STR   r2, [r5]

      LDR   r3, =ADDR_EMIF_SDRAM_TMING1
      STR   r6, [r3]

      LDR   r3, =ADDR_EMIF_SDRAM_TMING2
      STR   r4, [r3]

      /* timing_unlock=0 */
      BIC   r2, r2, #DDR2_TIMING_LOCK
      STR   r2, [r5]
#endif
      BX    lr

