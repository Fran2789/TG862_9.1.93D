/*
 * File: cache.s - Cache functions
 *
 * (C) Copyright 2005-2009
 *
 * Texas Instruments, <www.ti.com>
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

#define CACHE_LINE_SIZE     32	

/**
 * dcache_i 
 *      - Invalidate data cache
 *        Syntax:
 *                void dcache_i(
 *	           	void *address,		- Starting address (register r0)
 *			unsigned len )		- # of bytes to invalidate (register r1)
 *		
 * Description:
 *        This function invalidates the data cache lines containing bytes in the
 *	range of address to address+(len-1). The address is modified to start on
 *	a cache-line boundary.
 */

.align  
.globl dcache_i  
dcache_i:
    ADD			r2, r0, r1			/* r2 is one-byte past ending address */
    BIC			r0, r0, #(CACHE_LINE_SIZE-1)	/* align address to 32-byte boundary */
lbl1:	
    CMP			r0, r2			        /* while address < ending address */
    MCRLT		p15, 0, r0, c7, c6, 1	        /*   invalidate DCache single entry (MVA) */
    ADDLT		r0, r0, #CACHE_LINE_SIZE        /*  inc address to next cache line */
    BLT			lbl1
    MOV			pc, lr                          /* return */		

/**
 * dcache_wb - Write-back (clean) dirty data cache
 *
 *              Syntax:
 *                      void dcache_wb(
 *			    void *address,		- Starting address (register r0)
 *			    unsigned len )		- # of bytes to invalidate (register r1)
 *		
 * Description:
 *              This function writes back (cleans) the dirty data cache lines containing
 *		bytes in the range of address to address+(len-1). The address is modified
 *		to start on a cache-line boundary.
 */
.align
.global dcache_wb
dcache_wb:
    ADD			r2, r0, r1			/* r2 is one-byte past ending address */
    BIC			r0, r0, #(CACHE_LINE_SIZE-1)    /* align address to 32-byte boundary */
lbl2:	
    CMP			r0, r2				/* while address < ending address */
    MCRLT		p15, 0, r0, c7, c10, 1		/*   clean DCache single entry (MVA) */
    ADDLT		r0, r0, #CACHE_LINE_SIZE	/*   inc address to next cache line */
    BLT			lbl2
    MOV			pc, lr				/* return		*/

/**
 * dcache_wbi 
 *      - Write-back (clean) then invalidate dirty data cache
 *        Syntax:
 *               void dcache_wbi(
 *			void *address,		- Starting address (register r0)
 *			unsigned len )		- # of bytes to invalidate (register r1)
 *		
 * Description:
 *              This function writes back (cleans) and then invalidates the dirty data
 *		cache lines containing bytes in the range of address to address+(len-1).
 *		The address is modified to start on a cache-line boundary.
 */
.align
    .globl dcache_wbi
dcache_wbi:	
    ADD			r2, r0, r1			    /* r2 is one-byte past ending address */
    BIC			r0, r0, #(CACHE_LINE_SIZE-1)	    /* align address to 32-byte boundary */
lbl3:	CMP			r0, r2			    /* while address < ending address */
    MCRLT		p15, 0, r0, c7, c14, 1	            /*   clean and invalidate DCache entry (MVA) */
    ADDLT		r0, r0, #CACHE_LINE_SIZE            /*   inc address to next cache line */
    BLT			lbl3
    MOV			pc, lr                              /* return */		

