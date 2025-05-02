/*
 * (C) Copyright 2000-2003
 * Wolfgang Denk, DENX Software Engineering, wd@denx.de.
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

/* 
 * Includes Intel Corporation's changes/modifications dated: 2012. 
 * Changed/modified portions - Copyright © 2012 , Intel Corporation.   
 */ 
/* This program has been modified from its original operation by Intel
 * to do the following:
 *
 * 1. Find the right mtd device which stores the environment variables automatically. (2010)
 * 2. Support eMMC for Puma6 SoC. (2012)
*/
 

#include <errno.h>
#include <fcntl.h>
#include <stdio.h>
#include <stdlib.h>
#include <stddef.h>
#include <string.h>
#include <sys/types.h>
#include <sys/ioctl.h>
#include <sys/stat.h>
#include <unistd.h>
#include <mtd/mtd-user.h>
#include <puma_autoconf.h>

#include "fw_env.h"
#include "crc32.h"


#if PUMA6_OR_NEWER_SOC_TYPE   
#include "sram_api.h"
#endif


//#define dbg_printf(fmt, args...) printf("Env-Tool-Debug: %s\n" fmt, __FUNCTION__ , ## args)
#define dbg_printf(fmt, args...)

#define MTDFILENAME "/proc/mtd"
#define ARR_LEN(x)  ( sizeof(x) / sizeof( x[0] ) )

typedef unsigned char uchar;

typedef struct envdev_s {
	char devname[16];		/* Device name */
	ulong devoff;			/* Device offset */
	ulong env_size;			/* environment size */
	ulong erase_size;		/* device erase size */
} envdev_t;

static envdev_t envdevices[2];
static int curdev;
static uchar *addr1 = NULL;
static uchar *addr2 = NULL;

#define DEVNAME(i)    envdevices[(i)].devname
#define DEVOFFSET(i)  envdevices[(i)].devoff
#define ENVSIZE(i)    envdevices[(i)].env_size
#define DEVESIZE(i)   envdevices[(i)].erase_size

#define CFG_ENV_SIZE ENVSIZE(curdev)

#define ENV_SIZE      getenvsize()

typedef struct environment_s {
	ulong crc;			/* CRC32 over data bytes    */
	uchar flags;			/* active or obsolete */
	uchar *data;
} env_t;

static env_t environment;
static int HaveRedundEnv = 0;
static uchar active_flag = 1;
static uchar obsolete_flag = 0;
static int emmc_env = 0;        /* 0 - SPI-NOR  ;  1 - eMMC-NAND  */

static int flash_io (int mode);
static uchar *envmatch (uchar * s1, uchar * s2);
static int env_init (void);
static int env_deinit(void);
static int parse_config (void);
static int dynamic_config( void );

#if defined(CONFIG_FILE)
static int get_config (char *);
#endif

#if PUMA6_OR_NEWER_SOC_TYPE   
static int emmc_io (int mode);
static int get_BootParam(BootParamId_e param, unsigned int *val);
static int dynamic_config_emmc( void );
#endif



static inline ulong getenvsize (void)
{
	ulong rc = CFG_ENV_SIZE - sizeof (long);

	if (HaveRedundEnv)
		rc -= sizeof (char);
	return rc;
}

/*
 * Search the environment for a variable.
 * Return the value, if found, or NULL, if not found.
 */
unsigned char *fw_getenv (unsigned char *name)
{
	uchar *env, *nxt;
	static uchar *value = NULL;

	if(!name) {
		if(value) {
			free(value);
			value = NULL;
		}
		return (NULL);
	}

	if (env_init ())
		return (NULL);

	for (env = environment.data; *env; env = nxt + 1) {
		uchar *val;
		uchar *ptr;

		for (nxt = env; *nxt; ++nxt) {
			if (nxt >= &environment.data[ENV_SIZE]) {
				fprintf (stderr, "## Error: "
					"environment not terminated\n");
				env_deinit();
				return (NULL);
			}
		}
		val = envmatch (name, env);
		if (!val)
			continue;

		ptr = realloc( value, strlen( (const char *)val ) +1 );

		if(!ptr) {
			if( value ) {
				free(value);
				value = NULL;
			}
		}
		else {
			value = ptr;
			strcpy( (char *)value, (const char *)val );
		}

		env_deinit();
		return (value);
	}

	env_deinit();
	return (NULL);
}

/*
 * Print the current definition of one, or more, or all
 * environment variables
 */
void fw_printenv (int argc, char *argv[])
{
	uchar *env, *nxt;
	int i, n_flag;

	if (env_init ())
		return;

	if (argc == 1) {		/* Print all env variables  */
		for (env = environment.data; *env; env = nxt + 1) {
			for (nxt = env; *nxt; ++nxt) {
				if (nxt >= &environment.data[ENV_SIZE]) {
					fprintf (stderr, "## Error: "
						"environment not terminated\n");
					env_deinit();
					return;
				}
			}

			printf ("%s\n", env);
		}
		env_deinit();
		return;
	}

	if (strcmp (argv[1], "-n") == 0) {
		n_flag = 1;
		++argv;
		--argc;
		if (argc != 2) {
			fprintf (stderr, "## Error: "
				"`-n' option requires exactly one argument\n");
			env_deinit();
			return;
		}
	} else {
		n_flag = 0;
	}

	for (i = 1; i < argc; ++i) {	/* print single env variables   */
		char *name = argv[i];
		char *val = NULL;

		for (env = environment.data; *env; env = nxt + 1) {

			for (nxt = env; *nxt; ++nxt) {
				if (nxt >= &environment.data[ENV_SIZE]) {
					fprintf (stderr, "## Error: "
						"environment not terminated\n");
					env_deinit();
					return;
				}
			}
			val = (char*)envmatch ((uchar*)name, env);
			if (val) {
				if (!n_flag) {
					fputs (name, stdout);
					putc ('=', stdout);
				}
				puts (val);
				break;
			}
		}
		if (!val)
			fprintf (stderr, "## Error: \"%s\" not defined\n", name);
	}
	env_deinit();
}

/*
 * Deletes or sets environment variables. Returns errno style error codes:
 * 0	  - OK
 * EINVAL - need at least 1 argument
 * EROFS  - certain variables ("ethaddr", "serial#") cannot be
 *	    modified or deleted
 *
 */
int fw_setenv (int argc, char *argv[])
{
	int i, len;
	uchar *env, *nxt;
	uchar *oldval = NULL;
	char *name;

	if (argc < 2) {
		return (EINVAL);
	}

	if (env_init ())
		return (errno);

	name = argv[1];

	/*
	 * search if variable with this name already exists
	 */
	for (nxt = env = environment.data; *env; env = nxt + 1) {
		for (nxt = env; *nxt; ++nxt) {
			if (nxt >= &environment.data[ENV_SIZE]) {
				fprintf (stderr, "## Error: "
					"environment not terminated\n");
				env_deinit();
				return (EINVAL);
			}
		}
		if ((oldval = envmatch ((uchar *)name, env)) != NULL)
			break;
	}

	/*
	 * Delete any existing definition
	 */
	if (oldval) {
		/*
		 * Ethernet Address and serial# can be set only once
		 */
		if ((strcmp (name, "ethaddr") == 0) ||
			(strcmp (name, "serial#") == 0)) {
			fprintf (stderr, "Can't overwrite \"%s\"\n", name);
			env_deinit();
			return (EROFS);
		}

		if (*++nxt == '\0') {
			*env = '\0';
		} else {
			for (;;) {
				*env = *nxt++;
				if ((*env == '\0') && (*nxt == '\0'))
					break;
				++env;
			}
		}
		*++env = '\0';
	}

	/* Delete only ? */
	if (argc < 3)
		goto WRITE_FLASH;

	/*
	 * Append new definition at the end
	 */
	for (env = environment.data; *env || *(env + 1); ++env);
	if (env > environment.data)
		++env;
	/*
	 * Overflow when:
	 * "name" + "=" + "val" +"\0\0"  > CFG_ENV_SIZE - (env-environment)
	 */
	len = strlen (name) + 2;
	/* add '=' for first arg, ' ' for all others */
	for (i = 2; i < argc; ++i) {
		len += strlen (argv[i]) + 1;
	}
	if (len > (&environment.data[ENV_SIZE] - env)) {
		fprintf (stderr,
			"Error: environment overflow, \"%s\" deleted\n",
			name);
		env_deinit();
		return (-1);
	}
	while ((*env = *name++) != '\0')
		env++;
	for (i = 2; i < argc; ++i) {
		char *val = argv[i];

		*env = (i == 2) ? '=' : ' ';
		while ((*++env = *val++) != '\0');
	}

	/* end is marked with double '\0' */
	*++env = '\0';

  WRITE_FLASH:

	/* Update CRC */
	environment.crc = crc32 (0, environment.data, ENV_SIZE);

    if (emmc_env == 0)
    {
        /* write environment back to flash */
        if (flash_io (O_RDWR)) {
            fprintf (stderr, "Error: can't write fw_env to flash\n");
            env_deinit();
            return (-1);
        }
    }
#if PUMA6_OR_NEWER_SOC_TYPE
    if (emmc_env == 1)
    {
        /* write environment back to flash */
        if (emmc_io (O_RDWR)) {
            fprintf (stderr, "Error: can't write fw_env to eMMC\n");
            env_deinit();
            return (-1);
        }
    }
#endif

	env_deinit();
	return (0);
}

static int flash_io (int mode)
{
	int fd, fdr, rc, otherdev, len, resid;
	erase_info_t erase;
	char *data = NULL;

	if ((fd = open (DEVNAME (curdev), mode)) < 0) {
		fprintf (stderr,
			"Can't open %s: %s\n",
			DEVNAME (curdev), strerror (errno));
		return (-1);
	}

	len = sizeof (environment.crc);
	if (HaveRedundEnv) {
		len += sizeof (environment.flags);
	}

	if (mode == O_RDWR) {
		if (HaveRedundEnv) {
			/* switch to next partition for writing */
			otherdev = !curdev;
			if ((fdr = open (DEVNAME (otherdev), mode)) < 0) {
				fprintf (stderr,
					"Can't open %s: %s\n",
					DEVNAME (otherdev),
					strerror (errno));
				return (-1);
			}
		} else {
			otherdev = curdev;
			fdr = fd;
		}
		dbg_printf ("Unlocking flash...\n");
		erase.length = DEVESIZE (otherdev);
		erase.start = DEVOFFSET (otherdev);
		ioctl (fdr, MEMUNLOCK, &erase);

		if (HaveRedundEnv) {
			erase.length = DEVESIZE (curdev);
			erase.start = DEVOFFSET (curdev);
			ioctl (fd, MEMUNLOCK, &erase);
			environment.flags = active_flag;
		}

		dbg_printf ("Done\n");
		resid = DEVESIZE (otherdev) - CFG_ENV_SIZE;
		if (resid) {
			if ((data = malloc (resid)) == NULL) {
				fprintf (stderr,
					"Cannot malloc %d bytes: %s\n",
					resid,
					strerror (errno));
				return (-1);
			}
			if (lseek (fdr, DEVOFFSET (otherdev) + CFG_ENV_SIZE, SEEK_SET)
				== -1) {
				fprintf (stderr, "seek error on %s: %s\n",
					DEVNAME (otherdev),
					strerror (errno));
				free(data);
				return (-1);
			}
			if ((rc = read (fdr, data, resid)) != resid) {
				fprintf (stderr,
					"read error on %s: %s\n",
					DEVNAME (otherdev),
					strerror (errno));
				free(data);
				return (-1);
			}
		}

		dbg_printf ("Erasing old environment...\n");

		erase.length = DEVESIZE (otherdev);
		erase.start = DEVOFFSET (otherdev);
		if (ioctl (fdr, MEMERASE, &erase) != 0) {
			fprintf (stderr, "MTD erase error on %s: %s\n",
				DEVNAME (otherdev),
				strerror (errno));
			free(data);
			return (-1);
		}

		dbg_printf ("Done\n");

		dbg_printf ("Writing environment to %s...\n", DEVNAME (otherdev));
		if (lseek (fdr, DEVOFFSET (otherdev), SEEK_SET) == -1) {
			fprintf (stderr,
				"seek error on %s: %s\n",
				DEVNAME (otherdev), strerror (errno));
			free(data);
			return (-1);
		}
		if (write (fdr, &environment, len) != len) {
			fprintf (stderr,
				"CRC write error on %s: %s\n",
				DEVNAME (otherdev), strerror (errno));
			free(data);
			return (-1);
		}
		if (write (fdr, environment.data, ENV_SIZE) != ENV_SIZE) {
			fprintf (stderr,
				"Write error on %s: %s\n",
				DEVNAME (otherdev), strerror (errno));
			free(data);
			return (-1);
		}
		if (resid) {
			if (write (fdr, data, resid) != resid) {
				fprintf (stderr,
					"write error on %s: %s\n",
					DEVNAME (curdev), strerror (errno));
				free(data);
				return (-1);
			}
			free (data);
		}
		if (HaveRedundEnv) {
			/* change flag on current active env partition */
			if (lseek (fd, DEVOFFSET (curdev) + sizeof (ulong), SEEK_SET)
				== -1) {
				fprintf (stderr, "seek error on %s: %s\n",
					DEVNAME (curdev), strerror (errno));
				return (-1);
			}
			if (write (fd, &obsolete_flag, sizeof (obsolete_flag)) !=
				sizeof (obsolete_flag)) {
				fprintf (stderr,
					"Write error on %s: %s\n",
					DEVNAME (curdev), strerror (errno));
				return (-1);
			}
		}
		dbg_printf ("Done\n");
		dbg_printf ("Locking ...\n");
		erase.length = DEVESIZE (otherdev);
		erase.start = DEVOFFSET (otherdev);
		ioctl (fdr, MEMLOCK, &erase);
		if (HaveRedundEnv) {
			erase.length = DEVESIZE (curdev);
			erase.start = DEVOFFSET (curdev);
			ioctl (fd, MEMLOCK, &erase);
			if (close (fdr)) {
				fprintf (stderr,
					"I/O error on %s: %s\n",
					DEVNAME (otherdev),
					strerror (errno));
				return (-1);
			}
		}
		dbg_printf ("Done\n");
	} else {

		if (lseek (fd, DEVOFFSET (curdev), SEEK_SET) == -1) {
			fprintf (stderr,
				"seek error on %s: %s\n",
				DEVNAME (curdev), strerror (errno));
			return (-1);
		}
		if (read (fd, &environment, len) != len) {
			fprintf (stderr,
				"CRC read error on %s: %s\n",
				DEVNAME (curdev), strerror (errno));
			return (-1);
		}
		if ((rc = read (fd, environment.data, ENV_SIZE)) != ENV_SIZE) {
			fprintf (stderr,
				"Read error on %s: %s\n",
				DEVNAME (curdev), strerror (errno));
			return (-1);
		}
	}

	if (close (fd)) {
		fprintf (stderr,
			"I/O error on %s: %s\n",
			DEVNAME (curdev), strerror (errno));
		return (-1);
	}

	/* everything ok */
	return (0);
}

/*
 * s1 is either a simple 'name', or a 'name=value' pair.
 * s2 is a 'name=value' pair.
 * If the names match, return the value of s2, else NULL.
 */

static uchar *envmatch (uchar * s1, uchar * s2)
{

	while (*s1 == *s2++)
		if (*s1++ == '=')
			return (s2);
	if (*s1 == '\0' && *(s2 - 1) == '=')
		return (s2);
	return (NULL);
}


static int env_deinit(void)
{
	if(addr1) {
		free(addr1);
		addr1 = NULL;
	}
	if(addr2) {
		free(addr2);
		addr2 = NULL;
	}
	return 0;
}

/*
 * Prevent confusion if running from erased flash memory
 */
static int env_init (void)
{
	int crc1, crc1_ok;

	int crc2, crc2_ok;
	uchar flag1, flag2;

	if (parse_config ())		/* should fill envdevices */
		return 1;

	if( !addr1 ) {
		if ((addr1 = calloc (1, ENV_SIZE)) == NULL) {
			fprintf (stderr,
				"Not enough memory for environment (%ld bytes)\n",
				ENV_SIZE);
			return (errno);
		}
	}

	/* read environment from FLASH to local buffer */
	environment.data = addr1;
	curdev = 0;
    if (emmc_env == 0)
    {
        if (flash_io (O_RDONLY)) {
            return (errno);
        }
    }
#if PUMA6_OR_NEWER_SOC_TYPE
    if (emmc_env == 1)
    {
        if (emmc_io (O_RDONLY)) {
            return (errno);
        }
    }
#endif

	crc1_ok = ((crc1 = crc32 (0, environment.data, ENV_SIZE)) == environment.crc);

#if PUMA6_OR_NEWER_SOC_TYPE
    if (emmc_env == 1){
    	if (!crc1_ok) {
            printf ("%s offset 0x%x crc1 = 0x%x environment.crc = 0x%x\n", DEVNAME (curdev), DEVOFFSET (curdev), crc1, environment.crc);
			fprintf (stderr,
				"Error: Bad environment CRC, aborting!\n");
			return EBADF;
		}
    }

    return (0);

#endif

	if (!HaveRedundEnv) {
		if (!crc1_ok) {
			fprintf (stderr,
				"Error: Bad environment CRC, aborting!\n");
			return EBADF;
		}
	} else {
		flag1 = environment.flags;

		curdev = 1;
		if( !addr2 ) {
			if ((addr2 = calloc (1, ENV_SIZE)) == NULL) {
				fprintf (stderr,
					"Not enough memory for environment (%ld bytes)\n",
					ENV_SIZE);
				return (errno);
			}
		}
		environment.data = addr2;

		if (flash_io (O_RDONLY)) {
			return (errno);
		}

		crc2_ok = ((crc2 = crc32 (0, environment.data, ENV_SIZE))
				   == environment.crc);
		flag2 = environment.flags;

		if (crc1_ok && !crc2_ok) {
			environment.data = addr1;
			environment.flags = flag1;
			environment.crc = crc1;
			curdev = 0;
			free (addr2);
			addr2 = NULL;
		} else if (!crc1_ok && crc2_ok) {
			environment.data = addr2;
			environment.flags = flag2;
			environment.crc = crc2;
			curdev = 1;
			free (addr1);
			addr1 = NULL;
		} else if (!crc1_ok && !crc2_ok) {
			free (addr1);
			free (addr2);
			fprintf (stderr,
				"Error: Bad environment CRC, aborting!\n");
			return EBADF;
		} else if (flag1 == active_flag && flag2 == obsolete_flag) {
			environment.data = addr1;
			environment.flags = flag1;
			environment.crc = crc1;
			curdev = 0;
			free (addr2);
			addr2 = NULL;
		} else if (flag1 == obsolete_flag && flag2 == active_flag) {
			environment.data = addr2;
			environment.flags = flag2;
			environment.crc = crc2;
			curdev = 1;
			free (addr1);
			addr1 = NULL;
		} else if (flag1 == flag2) {
			environment.data = addr1;
			environment.flags = flag1;
			environment.crc = crc1;
			curdev = 0;
			free (addr2);
			addr2 = NULL;
		} else if (flag1 == 0xFF) {
			environment.data = addr1;
			environment.flags = flag1;
			environment.crc = crc1;
			curdev = 0;
			free (addr2);
			addr2 = NULL;
		} else if (flag2 == 0xFF) {
			environment.data = addr2;
			environment.flags = flag2;
			environment.crc = crc2;
			curdev = 1;
			free (addr1);
			addr1 = NULL;
		}
	}
	return (0);
}

static int dynamic_config( void )
{
    /**********************************************************************************
    # cat /proc/mtd
        dev:    size   erasesize  name
        mtd0: 00800000 00001000 "RAM0"
        mtd1: 000e21fc 00001000 "Kernel"
        mtd2: 0021dc00 00001000 "RootFileSystem"
        mtd3: 00020000 00010000 "U-Boot"
        mtd4: 00010000 00010000 "env1"
        mtd5: 00010000 00010000 "env2"
        mtd6: 007b0000 00010000 "UBFI1"
        mtd7: 007b0000 00010000 "UBFI2"
        mtd8: 00050000 00010000 "nvram"
    **********************************************************************************/

    int fd, i;
    char *buffer, *newbuffer;
    int buflen;
    int readlen;
    static const char *names[] = { "\"env1\"", "\"env2\"" };
    char *line = NULL;
    char *fileline;
    char *colon;
    char *sizebeg, *sizeend;
    char *lasts;

    buflen = 0;
    buffer = NULL;
    do
    {
        /* Open and read the MTD assignments */
        fd = open( MTDFILENAME, O_RDONLY );
        if( fd < 0 ) {
            goto use_defaults;
        }

        buflen += 1000;
        /* Alloc mem for file + space for \0 + set to 0 */
        newbuffer = realloc(buffer, buflen);
        if (newbuffer == NULL)
        {
            printf("Could not alloc %d bytes for " MTDFILENAME "\n", buflen);
            goto use_defaults;
        }
        buffer = newbuffer;

        readlen = read(fd, buffer, buflen);
        if (readlen <= 0)
        {
            printf("Could not read %d bytes from " MTDFILENAME "\n", buflen);
            goto use_defaults;
        }

        close( fd );

        /* Done when read less than buflen */
    } while (readlen >= buflen);

    /* Go over each line. In each line search for the "env"-s. */
    for (fileline = strtok_r(buffer, "\r\n", &lasts); fileline != NULL; fileline = strtok_r(NULL, "\r\n", &lasts))
    {
        for( i = 0; i < ARR_LEN(names); i++)
        {
            line = strdup(fileline);
            /* Find the name string */
            if( strstr( line, names[i] ) != NULL )
            {
                /* mtd is at beginning of line, before ":" */
                if( ( colon = strchr( line, ':' ) ) != NULL )
                {
                    /* devname is "/dev/mtd..."*/
                    *colon = '\0';
                    strcpy (DEVNAME (i), "/dev/");
                    strcat (DEVNAME (i), line);
                }
                else
                {
                    printf("Could not find \":\" on line with %s in " MTDFILENAME "\n", names[i]);
                    goto use_defaults;
                }

                /* Next comes a space, then the size */
                sizebeg = colon + 2;

                /* Find the space after the size */
                if( ( sizeend = strchr( sizebeg, ' ' ) ) != NULL )
                {
                    *sizeend = '\0';
                    ENVSIZE(i) = strtol(sizebeg, (char **)NULL, 16);
                    DEVESIZE(i) = ENVSIZE(i);
                    DEVOFFSET(i) = 0;
                }
                else
                {
                    goto use_defaults;
                }
            }
            free(line);
            line = NULL;
        }
    }

#ifdef HAVE_REDUND
    HaveRedundEnv = 1;
#else
    /* Remove name from 2nd dev */
    DEVNAME(1) = '\0';
#endif

    if (buffer != NULL) free(buffer); 
    if (line != NULL) free(line); 

    return 0;

	use_defaults:
    if (buffer != NULL)
    {
        free(buffer);
    }

	for( i=1; i>=0; i--)
	{
		/* Fall to default sector size */
		DEVESIZE(i) = ENVSIZE(i) = CONFIG_TI_ENVUTILS_DEFAULT_SECTOR_SIZE;
		DEVOFFSET(i) = 0;
	}

	return 0;
}

static int parse_config ()
{
	struct stat st;
#if PUMA6_OR_NEWER_SOC_TYPE
    unsigned int boot_mode;
#endif

    emmc_env = 0;  /* SPI (we can use the MTD) */
#if PUMA6_OR_NEWER_SOC_TYPE
    boot_mode = BOOT_MODE_SPI;
    if (get_BootParam(BOOT_MODE_ID,&boot_mode))
    {
        printf("Can't get Boot Mode (SPI or eMMC) from Boot-Param. will try SPI (MTD)\n\n");
    }
    if (boot_mode == BOOT_MODE_eMMC) 
    {
        emmc_env = 1; /* eMMC (we can't use the MTD - we must read the boot-param) */
    }
#endif


#if defined(CONFIG_FILE)
	/* Fills in DEVNAME(), ENVSIZE(), DEVESIZE(). Or don't. */
	if (get_config (CONFIG_FILE)) {
		fprintf (stderr,
			"Cannot parse config file: %s\n", strerror (errno));
		return 1;
	}
#else
#if PUMA6_OR_NEWER_SOC_TYPE
    if (emmc_env == 1)
    {
        dynamic_config_emmc();
    }
    else
#endif
    {
        dynamic_config();
    }
#endif
	if (stat (DEVNAME (0), &st)) { 
		fprintf (stderr,
			"Cannot access %s device %s: %s\n", (emmc_env == 1)?"eMMC":"MTD", 
			DEVNAME (0), strerror (errno));
		return 1;
	}

    if (emmc_env == 0) /* In eMMC we only have 1 ENV. */
    {
        if (HaveRedundEnv && stat (DEVNAME (1), &st)) {
            fprintf (stderr,
                "Cannot access MTD device %s: %s\n",
                DEVNAME (1), strerror (errno));
            return 1;
        }
    }
	return 0;
}

#if defined(CONFIG_FILE)
static int get_config (char *fname)
{
	FILE *fp;
	int i = 0;
	int rc;
	char dump[128];

	if ((fp = fopen (fname, "r")) == NULL) {
		return 1;
	}

	while ((i < 2) && ((rc = fscanf (fp, "%s %lx %lx %lx",
				  DEVNAME (i),
				  &DEVOFFSET (i),
				  &ENVSIZE (i),
				  &DEVESIZE (i)  )) != EOF)) {

		/* Skip incomplete conversions and comment strings */
		if ((rc < 3) || (*DEVNAME (i) == '#')) {
			fgets (dump, sizeof (dump), fp);	/* Consume till end */
			continue;
		}

		i++;
	}
	fclose (fp);

	HaveRedundEnv = i - 1;
	if (!i) {			/* No valid entries found */
		errno = EINVAL;
		return 1;
	} else
		return 0;
}
#endif


#if PUMA6_OR_NEWER_SOC_TYPE   

static int emmc_io (int mode)
{
    int fd, rc, len;

    if ( (fd = open (DEVNAME (curdev), mode)) < 0 )
    {
        fprintf (stderr,
                 "Can't open %s: %s\n",
                 DEVNAME (curdev), strerror (errno));
        return(-1);
    }

	if ( lseek (fd, DEVOFFSET (curdev), SEEK_SET) == -1 )
    {
		fprintf (stderr,"seek error on %s: %s\n", DEVNAME (curdev), strerror (errno));
        close (fd);
        return(-1);
    }
	
    len = sizeof (environment.crc);
	if (HaveRedundEnv) {
		len += sizeof (environment.flags);
	}
	
    if ( mode == O_RDWR )
    {
        dbg_printf ("Writing environment to %s [offset 0x%x]\n", DEVNAME (curdev), DEVOFFSET (curdev));
        
        /* write env header (CRC + flags) */
        if ( write (fd, &environment, len) != len )
        {
            fprintf (stderr,
                     "CRC write error on %s: %s\n",
                     DEVNAME (curdev), strerror (errno));
            close (fd);
            return(-1);
        }

        /* write env data */
        if ( write (fd, environment.data, ENV_SIZE) != ENV_SIZE )
        {
            fprintf (stderr,
                     "Write error on %s: %s\n",
                     DEVNAME (curdev), strerror (errno));
            close (fd);
            return(-1);
        }
        dbg_printf ("Done\n");
    }
    else
    {
        dbg_printf ("Reading environment from %s [offset 0x%x]\n", DEVNAME (curdev), DEVOFFSET (curdev));

        /* read env header (CRC + flags) */
        if ( read (fd, &environment, len) != len )
        {
            fprintf (stderr,
                     "CRC read error on %s: %s\n",
                     DEVNAME (curdev), strerror (errno));
            close (fd);
            return(-1);
        }

        /* read env data */
        if ( (rc = read (fd, environment.data, ENV_SIZE)) != ENV_SIZE )
        {
            fprintf (stderr,
                     "Read error on %s: %s\n",
                     DEVNAME (curdev), strerror (errno));
            close (fd);
            return(-1);
        }
    }

    if ( close (fd) )
    {
        fprintf (stderr,
                 "I/O error on %s: %s\n",
                 DEVNAME (curdev), strerror (errno));
        return(-1);
    }

    /* This will commit the data into the eMMC */
    sync();

    /* everything ok */
    return(0);
}

/**************************************************************************/
/*! \fn int get_BootParam(BootParamId_e paramm, unsigned int *val)
 **************************************************************************
 *  \brief Get a Boot Parameter
 *  \param[in] param - parametr name
 *  \param[out] val - parametr value
 *  \return 0-OK or (-1) for error
 **************************************************************************/
static int get_BootParam(BootParamId_e param, unsigned int *val)
{

    Int32 sramFd;
    sram_boot_params_args_t bootParam;

    /* open the character device, for the ioctl use.*/
    sramFd = open(SRAM_DEV_NAME, O_RDWR | O_CREAT );
    if( sramFd < 0 )
    {
        char msg[50];
        sprintf(msg,"Failed to open '%s'",SRAM_DEV_NAME);
        perror(msg);
        return (-1);
    }

    /* Read The board type from the Sram driver */
    bootParam.bootParamId = param;
    
    if (ioctl(sramFd, SRAM_GET_BOOT_PARAM, &bootParam) == -1) 
    {
        printf("ioctl return with -1\n");
        close( sramFd );
        return (-1); 
    }

    /* save the value */
    *val = bootParam.bootParamVal;
    
    close( sramFd );

    return 0;
}

static int dynamic_config_emmc( void )
{
    /**********************************************************************************
     
      read boot param to get the ENV1 offset and size (we have only 1 ENV in emmc) 
     
    **********************************************************************************/
    unsigned int env1_emmc_offset, env1_emmc_size;

#ifdef HAVE_REDUND
    HaveRedundEnv = 1;
#endif

    if ( get_BootParam(UBOOT_ENV1_OFFSET_ID, &env1_emmc_offset) )
    {
        fprintf (stderr, "Failed to get ENV-1 eMMC offset from boot-param \n");
        return 1;
    }

    if ( get_BootParam(UBOOT_ENV_SIZE_ID, &env1_emmc_size) )
    {
        fprintf (stderr, "Failed to get ENV-1 eMMC size from boot-param \n");
        return 1;
    }

    strcpy (DEVNAME (0), "/dev/mmcblk0");
    DEVESIZE(0) = ENVSIZE(0) = env1_emmc_size;
    DEVOFFSET(0) = env1_emmc_offset;

    dbg_printf ("ENV1: eMMC dev %s offset in dev 0x%x size of ENV1 %d\n", DEVNAME (0), DEVOFFSET (0), ENVSIZE(0));

    /* Remove name from 2nd dev */
    strcpy (DEVNAME (1), "");
    DEVESIZE(1) = 0;
    DEVOFFSET(1) = 0;
	ENVSIZE(1) = 0;

    return 0;
}

#endif


