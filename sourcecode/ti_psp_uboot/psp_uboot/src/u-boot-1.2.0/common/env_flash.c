/*
 * (C) Copyright 2000-2002
 * Wolfgang Denk, DENX Software Engineering, wd@denx.de.
 *
 * (C) Copyright 2001 Sysgo Real-Time Solutions, GmbH <www.elinos.com>
 * Andreas Heppel <aheppel@sysgo.de>

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
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.	 See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 59 Temple Place, Suite 330, Boston,
 * MA 02111-1307 USA
 */

/* 
 * Includes Intel Corporation's changes/modifications dated: 2011. 
 * Changed/modified portions - Copyright © 2011 , Intel Corporation.   
 */ 

/* #define DEBUG */

#include <common.h>

#if defined(CFG_ENV_IS_IN_FLASH) /* Environment is in Flash */

#include <command.h>
#include <environment.h>
#include <linux/stddef.h>
#include <malloc.h>

#if defined(CONFIG_USE_HW_MUTEX)
#include <puma6_hw_mutex.h>
#endif


DECLARE_GLOBAL_DATA_PTR;

#if ((CONFIG_COMMANDS&(CFG_CMD_ENV|CFG_CMD_FLASH)) == (CFG_CMD_ENV|CFG_CMD_FLASH))
#define CMD_SAVEENV
#elif defined(CFG_ENV_ADDR_REDUND)
#error Cannot use CFG_ENV_ADDR_REDUND without CFG_CMD_ENV & CFG_CMD_FLASH
#endif

#ifdef CONFIG_INFERNO
# ifdef CFG_ENV_ADDR_REDUND
#error CFG_ENV_ADDR_REDUND is not implemented for CONFIG_INFERNO
# endif
#endif

char * env_flash_name_spec = "Flash";

#ifdef ENV_IS_EMBEDDED

#ifdef CMD_SAVEENV
/* static env_t *flash_addr = (env_t *)(&environment[0]);-broken on ARM-wd-*/
#ifndef CONFIG_SPI_FLASH
static env_t *flash_addr = (env_t *)CFG_ENV_ADDR;
#else
static env_t *flash_addr = NULL;
#endif
#endif

#else /* ! ENV_IS_EMBEDDED */

#ifdef CMD_SAVEENV
#ifndef CONFIG_SPI_FLASH
static env_t *flash_addr = (env_t *)CFG_ENV_ADDR;
#else
static env_t *flash_addr = NULL;
#endif
#endif

#endif /* ENV_IS_EMBEDDED */

extern env_t *env_ptr;

#ifdef CFG_ENV_ADDR_REDUND
#ifndef CONFIG_SPI_FLASH
static env_t *flash_addr_new = (env_t *)CFG_ENV_ADDR_REDUND;
#else
static env_t *flash_addr_new = NULL;
#endif

#ifndef CONFIG_SPI_FLASH
/* CFG_ENV_ADDR is supposed to be on sector boundary */
static ulong end_addr = CFG_ENV_ADDR + CFG_ENV_SECT_SIZE - 1;
static ulong end_addr_new = CFG_ENV_ADDR_REDUND + CFG_ENV_SECT_SIZE - 1;
#else
static ulong end_addr = 0;
static ulong end_addr_new = 0;
#endif

#define ACTIVE_FLAG   1
#define OBSOLETE_FLAG 0
#endif /* CFG_ENV_ADDR_REDUND */

extern uchar default_environment[];
//extern int default_environment_size;


uchar env_flash_get_char_spec (int index)
{
    char c;
#if defined(CONFIG_USE_HW_MUTEX)
    /* Lock the HW Mutex */
    if (hw_mutex_lock(HW_MUTEX_NOR_SPI) == 0)
    {
        printf("Error: env_get_char_spec - failed to lock HW Mutex\n");
        return 0;
    }
#endif

    c = *((uchar *)(gd->env_addr + index));

#if defined(CONFIG_USE_HW_MUTEX)
    /* Release HW Mutes */
    hw_mutex_unlock(HW_MUTEX_NOR_SPI);
#endif
	return c;
}

#ifdef CFG_ENV_ADDR_REDUND

#ifdef CONFIG_SPI_FLASH
extern int flash_addr_init(int i,int verbose);

void addr_init(void)
{
	if( flash_addr_init(0,0) < 0)
		return;

	/* Initialize flash addresses */
	env_ptr = (env_t *)CFG_ENV_ADDR;
	flash_addr = (env_t *)CFG_ENV_ADDR;
	flash_addr_new = (env_t *)CFG_ENV_ADDR_REDUND;

	end_addr = CFG_ENV_ADDR + CFG_ENV_SECT_SIZE - 1;           
	end_addr_new = CFG_ENV_ADDR_REDUND + CFG_ENV_SECT_SIZE - 1;
}
#endif

int  env_flash_init(void)
{
	int crc1_ok = 0, crc2_ok = 0;
	uchar flag1;
	uchar flag2;

	ulong addr_default;
	ulong addr1;
	ulong addr2;

#ifdef CONFIG_SPI_FLASH
	addr_init();
#endif

#if defined(CONFIG_USE_HW_MUTEX)
    /* Lock the HW Mutex */
    if (hw_mutex_lock(HW_MUTEX_NOR_SPI) == 0)
    {
        printf("Error: env_init - failed to lock HW Mutex\n");

        /* If mutex failed use default values */
        gd->env_addr  = (ulong)&default_environment[0];
		gd->env_valid = 0;
        return 0;
    }
#endif

    /* Read Flasgs from Flash - Use Mutex*/
	flag1 = flash_addr->flags;
	flag2 = flash_addr_new->flags;

	addr_default = (ulong)&default_environment[0];
	addr1 = (ulong)&(flash_addr->data);
	addr2 = (ulong)&(flash_addr_new->data);

#ifdef CONFIG_OMAP2420H4
	int flash_probe(void);

	if(flash_probe() == 0)
		goto bad_flash;
#endif

    /* Read the expected CRC from flash, and calculated the actual CRC */ 
	crc1_ok = (crc32(0, flash_addr->data, ENV_SIZE) == flash_addr->crc);
	crc2_ok = (crc32(0, flash_addr_new->data, ENV_SIZE) == flash_addr_new->crc);

#if defined(CONFIG_USE_HW_MUTEX)
    /* Release HW Mutes */
    hw_mutex_unlock(HW_MUTEX_NOR_SPI);
#endif


	if (crc1_ok && ! crc2_ok) {
		gd->env_addr  = addr1;
		gd->env_valid = 1;
	} else if (! crc1_ok && crc2_ok) {
		gd->env_addr  = addr2;
		gd->env_valid = 1;
	} else if (! crc1_ok && ! crc2_ok) {
		gd->env_addr  = addr_default;
		gd->env_valid = 0;
	} else if (flag1 == ACTIVE_FLAG && flag2 == OBSOLETE_FLAG) {
		gd->env_addr  = addr1;
		gd->env_valid = 1;
	} else if (flag1 == OBSOLETE_FLAG && flag2 == ACTIVE_FLAG) {
		gd->env_addr  = addr2;
		gd->env_valid = 1;
	} else if (flag1 == flag2) {
		gd->env_addr  = addr1;
		gd->env_valid = 2;
	} else if (flag1 == 0xFF) {
		gd->env_addr  = addr1;
		gd->env_valid = 2;
	} else if (flag2 == 0xFF) {
		gd->env_addr  = addr2;
		gd->env_valid = 2;
	}

#ifdef CONFIG_OMAP2420H4
bad_flash:
#endif
	return (0);
}

#ifdef CMD_SAVEENV
int env_flash_saveenv(void)
{
	char *saved_data = NULL;
	int rc = 1;
	char flag = OBSOLETE_FLAG, new_flag = ACTIVE_FLAG;
	ulong up_data = 0;

	debug( "CFG_ENV_ADDR = 0x%x, CFG_ENV_SECT_SIZE = 0x%x\n", CFG_ENV_ADDR, CFG_ENV_SECT_SIZE );
	debug( "end_addr = 0x%x, end_addr_new = 0x%x, flash_addr = 0x%x\n", end_addr, end_addr_new, flash_addr );

	debug ("Protect off %08lX ... %08lX\n",
		(ulong)flash_addr, end_addr);

	if (flash_sect_protect (0, (ulong)flash_addr, end_addr)) {
		goto Done;
	}

	debug ("Protect off %08lX ... %08lX\n",
		(ulong)flash_addr_new, end_addr_new);

	if (flash_sect_protect (0, (ulong)flash_addr_new, end_addr_new)) {
		goto Done;
	}

	if( CFG_ENV_SECT_SIZE > CFG_ENV_SIZE )
	{
		up_data = (end_addr_new + 1 - ((long)flash_addr_new + CFG_ENV_SIZE));
		debug ("Data to save 0x%x\n", up_data);
		if (up_data) {
			if ((saved_data = malloc(up_data)) == NULL) {
				printf("Unable to save the rest of sector (%ld)\n",
					up_data);
				goto Done;
			}
			memcpy(saved_data,
				(void *)((long)flash_addr_new + CFG_ENV_SIZE), up_data);
			debug ("Data (start 0x%x, len 0x%x) saved at 0x%x\n",
				   (long)flash_addr_new + CFG_ENV_SIZE,
					up_data, saved_data);
		}
	}

	puts ("Erasing Flash...");
	debug (" %08lX ... %08lX ...",
		(ulong)flash_addr_new, end_addr_new);

	if (flash_sect_erase ((ulong)flash_addr_new, end_addr_new)) {
		goto Done;
	}

	puts ("Writing to Flash... ");
	debug (" %08lX ... %08lX ...",
		(ulong)&(flash_addr_new->data),
		sizeof(env_ptr->data)+(ulong)&(flash_addr_new->data));

	debug( "env_ptr->data=%p, &(flash_addr_new->data)=%p, ENV_SIZE=0x%x\n", env_ptr->data, &(flash_addr_new->data), ENV_SIZE );

	if ((rc = flash_write((char *)env_ptr->data,
			(ulong)&(flash_addr_new->data),
			ENV_SIZE)) ||
	    (rc = flash_write((char *)&(env_ptr->crc),
			(ulong)&(flash_addr_new->crc),
			sizeof(env_ptr->crc))) ||
	    (rc = flash_write(&flag,
			(ulong)&(flash_addr->flags),
			sizeof(flash_addr->flags))) ||
	    (rc = flash_write(&new_flag,
			(ulong)&(flash_addr_new->flags),
			sizeof(flash_addr_new->flags))))
	{
		flash_perror (rc);
		goto Done;
	}
	puts ("done\n");

	if( CFG_ENV_SECT_SIZE > CFG_ENV_SIZE )
	{
		if (up_data) { /* restore the rest of sector */
			debug ("Restoring the rest of data to 0x%x len 0x%x\n",
				   (long)flash_addr_new + CFG_ENV_SIZE, up_data);
			if (flash_write(saved_data,
					(long)flash_addr_new + CFG_ENV_SIZE,
					up_data)) {
				flash_perror(rc);
				goto Done;
			}
		}
	}

	{
		env_t * etmp = flash_addr;
		ulong ltmp = end_addr;

		flash_addr = flash_addr_new;
		flash_addr_new = etmp;

		end_addr = end_addr_new;
		end_addr_new = ltmp;
	}

	rc = 0;
Done:

	if (saved_data)
		free (saved_data);
	/* try to re-protect */
	(void) flash_sect_protect (1, (ulong)flash_addr, end_addr);
	(void) flash_sect_protect (1, (ulong)flash_addr_new, end_addr_new);

	return rc;
}
#endif /* CMD_SAVEENV */

#else /* ! CFG_ENV_ADDR_REDUND */

int  env_flash_init(void)
{
#ifdef CONFIG_OMAP2420H4
	int flash_probe(void);

	if(flash_probe() == 0)
		goto bad_flash;
#endif
	if (crc32(0, env_ptr->data, ENV_SIZE) == env_ptr->crc) {
		gd->env_addr  = (ulong)&(env_ptr->data);
		gd->env_valid = 1;
		return(0);
	}
#ifdef CONFIG_OMAP2420H4
bad_flash:
#endif
	gd->env_addr  = (ulong)&default_environment[0];
	gd->env_valid = 0;
	return (0);
}

#ifdef CMD_SAVEENV

int env_flash_saveenv(void)
{
	int	len, rc;
	ulong	end_addr;
	ulong	flash_sect_addr;
#if defined(CFG_ENV_SECT_SIZE) && (CFG_ENV_SECT_SIZE > CFG_ENV_SIZE)
	ulong	flash_offset;
	uchar	env_buffer[CFG_ENV_SECT_SIZE];
#else
	uchar *env_buffer = (uchar *)env_ptr;
#endif	/* CFG_ENV_SECT_SIZE */
	int rcode = 0;

	if(CFG_ENV_SECT_SIZE > CFG_ENV_SIZE)
	{

		flash_offset    = ((ulong)flash_addr) & (CFG_ENV_SECT_SIZE-1);
		flash_sect_addr = ((ulong)flash_addr) & ~(CFG_ENV_SECT_SIZE-1);

		debug ( "copy old content: "
			"sect_addr: %08lX  env_addr: %08lX  offset: %08lX\n",
			flash_sect_addr, (ulong)flash_addr, flash_offset);

		/* copy old contents to temporary buffer */
		memcpy (env_buffer, (void *)flash_sect_addr, CFG_ENV_SECT_SIZE);

		/* copy current environment to temporary buffer */
		memcpy ((uchar *)((unsigned long)env_buffer + flash_offset),
			env_ptr,
			CFG_ENV_SIZE);

		len	 = CFG_ENV_SECT_SIZE;
	}
	else
	{
		flash_sect_addr = (ulong)flash_addr;
		len	 = CFG_ENV_SIZE;
	}

#ifndef CONFIG_INFERNO
	end_addr = flash_sect_addr + len - 1;
#else
	/* this is the last sector, and the size is hardcoded here */
	/* otherwise we will get stack problems on loading 128 KB environment */
	end_addr = flash_sect_addr + 0x20000 - 1;
#endif

	debug ("Protect off %08lX ... %08lX\n",
		(ulong)flash_sect_addr, end_addr);

	if (flash_sect_protect (0, flash_sect_addr, end_addr))
		return 1;

	puts ("Erasing Flash...");
	if (flash_sect_erase (flash_sect_addr, end_addr))
		return 1;

	puts ("Writing to Flash... ");
	rc = flash_write((char *)env_buffer, flash_sect_addr, len);
	if (rc != 0) {
		flash_perror (rc);
		rcode = 1;
	} else {
		puts ("done\n");
	}

	/* try to re-protect */
	(void) flash_sect_protect (1, flash_sect_addr, end_addr);
	return rcode;
}

#endif /* CMD_SAVEENV */

#endif /* CFG_ENV_ADDR_REDUND */

void env_flash_relocate_spec (void)
{
#if !defined(ENV_IS_EMBEDDED) || defined(CFG_ENV_ADDR_REDUND)

#if defined(CONFIG_USE_HW_MUTEX)
    /* Lock the HW Mutex */
    if (hw_mutex_lock(HW_MUTEX_NOR_SPI) == 0)
    {
        printf("Error: env_flash_relocate_spec - failed to lock HW Mutex\n");
        return;
    }
#endif

#ifdef CFG_ENV_ADDR_REDUND
	if (gd->env_addr != (ulong)&(flash_addr->data)) {
		env_t * etmp = flash_addr;
		ulong ltmp = end_addr;

		flash_addr = flash_addr_new;
		flash_addr_new = etmp;

		end_addr = end_addr_new;
		end_addr_new = ltmp;
	}

    /* If Env Redund is CRC valid but not obsolete, then make it obsolete */
	if (flash_addr_new->flags != OBSOLETE_FLAG &&
	    crc32(0, flash_addr_new->data, ENV_SIZE) ==
	    flash_addr_new->crc) {
		char flag = OBSOLETE_FLAG;

		gd->env_valid = 2;
		flash_sect_protect (0, (ulong)flash_addr_new, end_addr_new);
		flash_write(&flag,
			    (ulong)&(flash_addr_new->flags),
			    sizeof(flash_addr_new->flags));
		flash_sect_protect (1, (ulong)flash_addr_new, end_addr_new);
	}

    /* if Env Flag is equal Activ, but others bit are also set,
       then clear the other bits, and make it Active only */
	if (flash_addr->flags != ACTIVE_FLAG &&
	    (flash_addr->flags & ACTIVE_FLAG) == ACTIVE_FLAG) {
		char flag = ACTIVE_FLAG;

		gd->env_valid = 2;
		flash_sect_protect (0, (ulong)flash_addr, end_addr);
		flash_write(&flag,
			    (ulong)&(flash_addr->flags),
			    sizeof(flash_addr->flags));
		flash_sect_protect (1, (ulong)flash_addr, end_addr);
	}

	if (gd->env_valid == 2)
		puts ("*** Warning - some problems detected "
		      "reading environment; recovered successfully\n\n");
#endif /* CFG_ENV_ADDR_REDUND */
	memcpy (env_ptr, (void*)flash_addr, CFG_ENV_SIZE);

#if defined(CONFIG_USE_HW_MUTEX)
    /* Release HW Mutes */
    hw_mutex_unlock(HW_MUTEX_NOR_SPI);
#endif

#endif /* ! ENV_IS_EMBEDDED || CFG_ENV_ADDR_REDUND */
}

#endif /* CFG_ENV_IS_IN_FLASH */
