/*
 * GPL LICENSE SUMMARY
 *
 *  Copyright(c) 2012 Intel Corporation.
 *
 *  This program is free software; you can redistribute it and/or modify 
 *  it under the terms of version 2 of the GNU General Public License as
 *  published by the Free Software Foundation.
 *
 *  This program is distributed in the hope that it will be useful, but 
 *  WITHOUT ANY WARRANTY; without even the implied warranty of 
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU 
 *  General Public License for more details.
 *
 *
 *  You should have received a copy of the GNU General Public License 
 *  along with this program; if not, write to the Free Software 
 *  Foundation, Inc., 51 Franklin St - Fifth Floor, Boston, MA 02110-1301 USA.
 *  The full GNU General Public License is included in this distribution 
 *  in the file called LICENSE.GPL.
 *
 *  Contact Information:
 *  Intel Corporation
 *  2200 Mission College Blvd.
 *  Santa Clara, CA  97052
 */

#include <common.h>
#include <environment.h>

#ifdef CONFIG_SYSTEM_PUMA6_SOC
#include <docsis_ip_boot_params.h> 
#endif

DECLARE_GLOBAL_DATA_PTR;

/* references to names in env_common.c */
extern uchar default_environment[];
extern int default_environment_size;

/* 
   Note:
   env_ptr is define here for external usage.
   In Puma5/6 ENV_IS_EMBEDDED is not define.
*/
#ifdef ENV_IS_EMBEDDED
extern uchar environment[];
env_t *env_ptr = (env_t *)(&environment[0]);
#else /* ! ENV_IS_EMBEDDED */
env_t *env_ptr = NULL;
#endif /* ENV_IS_EMBEDDED */



/* SPI Flash call back function declaration */
extern int  env_flash_init(void);
extern int  env_flash_get_char_spec(int index);
extern int  env_flash_saveenv(void);
extern void env_flash_relocate_spec(void);
extern char * env_flash_name_spec;

#if defined(CONFIG_GENERIC_MMC) && defined(CONFIG_ENV_IS_IN_MMC)
/* SPI Flash call back function declaration */
extern int  env_mmc_init(void);
extern int  env_mmc_get_char_spec(int index);
extern int  env_mmc_saveenv(void);
extern void env_mmc_relocate_spec(void);
extern char * env_mmc_name_spec;
#endif

/* Default call back function declaration */
char *env_default_name_spec = "Nowhere";
void  env_default_relocate_spec (void);
int   env_default_get_char_spec (int index);
int   env_default_init(void);
int   env_default_saveenv(void);


/* file global call back function declaration */
int (*cb_env_init)(void);
int (*cb_env_get_char_spec)(int index);
int (*cb_saveenv)(void);
void(*cb_env_relocate_spec)(void);
char * env_name_spec;



int env_init(void)
{
#ifndef CONFIG_SYSTEM_PUMA6_SOC
    return env_flash_init();
#else
    
#if defined (CFG_ENV_IS_IN_FLASH)
    /* check if boot from SPI/Nor */
    if (BOOT_PARAM_LONG_READ( BOOT_MODE ) == BOOT_MODE_SPI)
    {
        cb_env_init             = env_flash_init;
        cb_env_get_char_spec    = env_flash_get_char_spec;
        cb_saveenv              = env_flash_saveenv;
        cb_env_relocate_spec    = env_flash_relocate_spec;

        env_name_spec = env_flash_name_spec;
    }
#endif

#if defined(CONFIG_GENERIC_MMC) && defined(CONFIG_ENV_IS_IN_MMC)
    /* check if boot from eMMC/Nand */
    if (BOOT_PARAM_LONG_READ( BOOT_MODE ) == BOOT_MODE_eMMC) 
    {
        cb_env_init             = env_mmc_init;
        cb_env_get_char_spec    = env_mmc_get_char_spec;
        cb_saveenv              = env_mmc_saveenv;
        cb_env_relocate_spec    = env_mmc_relocate_spec;

        env_name_spec = env_mmc_name_spec;
    }
#endif
    
    if ((BOOT_PARAM_LONG_READ( BOOT_MODE ) != BOOT_MODE_SPI) &&
        (BOOT_PARAM_LONG_READ( BOOT_MODE ) != BOOT_MODE_eMMC) )
    {
        cb_env_init             = env_default_init;
        cb_env_get_char_spec    = env_default_get_char_spec;
        cb_saveenv              = env_default_saveenv;
        cb_env_relocate_spec    = env_default_relocate_spec;

        env_name_spec = env_default_name_spec;
    }
    
    /* TODO: enable fallback option to ENV NOWHERE */
    return cb_env_init();
#endif
}


uchar env_get_char_spec(int index)
{
#ifndef CONFIG_SYSTEM_PUMA6_SOC
    return env_flash_get_char_spec(index);
#else
    return cb_env_get_char_spec(index);
#endif
	
}

int saveenv(void)
{
#ifndef CONFIG_SYSTEM_PUMA6_SOC
    return env_flash_saveenv();
#else
    return cb_saveenv();
#endif
}



void env_relocate_spec(void)
{
#ifndef CONFIG_SYSTEM_PUMA6_SOC
    return env_flash_relocate_spec();
#else
    return cb_env_relocate_spec();
#endif
}

#ifdef CONFIG_SYSTEM_PUMA6_SOC

void env_default_relocate_spec (void)
{
    puts ("Warning: No vaild flash for saving environment variables\n");
    puts ("Using embedded default environment\n\n");

    if (default_environment_size > ENV_SIZE)
    {
        puts ("*** Error - default environment is too large\n\n");
        return;
    }

    memset (env_ptr, 0, CFG_ENV_SIZE);
    memcpy (env_ptr->data,default_environment,default_environment_size);

#ifdef CFG_REDUNDAND_ENVIRONMENT
	env_ptr->flags = 0xFF;
#endif

	env_crc_update ();

    gd->env_valid = 1;
}

int env_default_get_char_spec (int index)
{
	return ( *((uchar *)(gd->env_addr + index)) );
}

int  env_default_init(void)
{
    flash_addr_init(0,0); /* TODO: should be moved to board_init() */

	gd->env_addr  = (ulong)&default_environment[0];
	gd->env_valid = 1;

	return (0);
}

int env_default_saveenv(void)
{
    puts ("Warning: No vaild flash for saving environment variables\n");
    return 0;
}

#endif




