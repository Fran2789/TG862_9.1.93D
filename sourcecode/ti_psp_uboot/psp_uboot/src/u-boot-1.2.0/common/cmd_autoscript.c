/*
 * (C) Copyright 2001
 * Kyle Harris, kharris@nexus-tech.net
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
 * autoscript allows a remote host to download a command file and,
 * optionally, binary data for automatically updating the target. For
 * example, you create a new kernel image and want the user to be
 * able to simply download the image and the machine does the rest.
 * The kernel image is postprocessed with mkimage, which creates an
 * image with a script file prepended. If enabled, autoscript will
 * verify the script and contents of the download and execute the
 * script portion. This would be responsible for erasing flash,
 * copying the new image, and rebooting the machine.
 */

/* #define DEBUG */

#include <common.h>
#include <command.h>
#include <image.h>
#include <malloc.h>
#include <asm/byteorder.h>
#if defined(CONFIG_8xx)
#include <mpc8xx.h>
#endif
#ifdef CFG_HUSH_PARSER
#include <hush.h>
#endif

#ifdef CONFIG_EXTENDED_FLASH
#include <serial_flash.h>
#endif

#if defined(CONFIG_USE_HW_MUTEX)
#include <puma6_hw_mutex.h>
#endif

#ifdef CONFIG_GENERIC_MMC
#include <mmc.h>
#endif

#ifdef CONFIG_DYNAMIC_BASE_ADDRESS 
#include <docsis_ip_boot_params.h>
#endif

#if defined(CONFIG_AUTOSCRIPT) || \
	 (CONFIG_COMMANDS & CFG_CMD_AUTOSCRIPT )

extern image_header_t header;		/* from cmd_bootm.c */
int
autoscript (ulong addr)
{
	ulong crc, data, len;
	image_header_t *hdr = &header;
	ulong *len_ptr;
	char *cmd;
	int rcode = 0;
	int verify;
    int mmc_boot;
#if defined(CONFIG_USE_HW_MUTEX)
    int mutex_on = 0;
#endif
#ifdef CONFIG_GENERIC_MMC
    char  blk_buffer[CONFIG_MMC_BLOCK_SIZE*2];
    ulong addr_align;
    int   addr_gap;
#endif

    cmd = getenv ("verify");
    verify = (cmd && (*cmd == 'n')) ? 0 : 1;
    
    cmd = getenv ("bootdevice");
    mmc_boot = (cmd && (strcmp(cmd,"mmc") == 0)) ? 1 : 0;

#ifdef CONFIG_GENERIC_MMC
    if (mmc_boot == 1)
    {
        /* copy Image Header from emmc to &header */
        addr_align = addr & 0xFFFFFE00; /* allign to mmc block size*/
        addr_gap   = addr & 0x1FF;      /* gap after alignment*/
        /*printf("[DEBUG] addr:0x%X, addr_align:0x%X, addr_gap:0x%X \n",addr,addr_align,addr_gap); */
        /*printf("[DEBUG] mmc_read (header) from:0x%X to:0x%X len:0x%X\n",addr_align,(uchar*)blk_buffer, CONFIG_MMC_BLOCK_SIZE*2);*/
        mmc_read(CONFIG_SYS_MMC_IMG_DEV, addr_align, (uchar*)blk_buffer, CONFIG_MMC_BLOCK_SIZE*2);
        /*printf("[DEBUG] memcpy (header) from:0x%X to:0x%X len:0x%X\n",blk_buffer+addr_gap,hdr,sizeof(image_header_t));*/
        memcpy(hdr,blk_buffer+addr_gap,sizeof(image_header_t));
    }
    else
#endif
	memmove (hdr, (char *)addr, sizeof(image_header_t));

	if (ntohl(hdr->ih_magic) != IH_MAGIC) {
		puts ("Bad magic number\n");
		return 1;
	}

	crc = ntohl(hdr->ih_hcrc);
	hdr->ih_hcrc = 0;
	len = sizeof (image_header_t);
	data = (ulong)hdr;
	if (crc32(0, (uchar *)data, len) != crc) {
		puts ("Bad header crc\n");
		return 1;
	}

#ifdef CONFIG_GENERIC_MMC
    if (mmc_boot == 1)
    {
        char hex_str[12];

        /* copy image (Header(script)+ data+ header(kernel image)) to RAM from emmc */
        addr_align = addr & 0xFFFFFE00; /* allign to mmc block size*/
        addr_gap   = addr & 0x1FF;      /* gap after alignment*/
        /* printf("[DEBUG] addr:0x%X, addr_align:0x%X, addr_gap:0x%X \n",addr,addr_align,addr_gap); */
        len  = ntohl(hdr->ih_size) + sizeof(image_header_t) + addr_gap + sizeof(image_header_t) + 40 ; /* Gap + Image Data Size + Header Size + 40 byte extra for image list*/
        /* printf("[DEBUG] mmc_read (header) from:0x%X to:0x%X len:0x%X\n",addr_align,(uchar*)CFG_EMMC_LOAD_ADDR, len); */
        /*printf ("[Debug-Uboot] Copy Script image from emmc 0x%0.8X to RAM 0x%0.8X ,length=%d, gap=%d \n",addr_align,(uchar*)CFG_EMMC_LOAD_ADDR, len , addr_gap);*/
        mmc_read(CONFIG_SYS_MMC_IMG_DEV, addr_align, (uchar*)CFG_EMMC_LOAD_ADDR, len);
        addr = CFG_EMMC_LOAD_ADDR+addr_gap; /* set addr to new position in DDR */
        /* printf("[DEBUG] re-set addr to 0x%X\n",addr); */
        sprintf (hex_str, "0x%0.8X", addr);
        setenv ("UBFIADDR_MMC", hex_str);
        /*printf ("[Debug-Uboot] set UBFIADDR_MMC to 0x%0.8X\n",addr);*/
    }
#endif

	/* set pointer to start of data part (script) */
	data = addr + sizeof(image_header_t);
	len = ntohl(hdr->ih_size);

	if (verify) {
		if (crc32(0, (uchar *)data, len) != ntohl(hdr->ih_dcrc)) {
			puts ("Bad data crc\n");
			return 1;
		}
	}

	if (hdr->ih_type != IH_TYPE_SCRIPT) {
		puts ("Bad image type\n");
		return 1;
	}
#if defined(CONFIG_USE_HW_MUTEX)
    /* Lock the HW Mutex */
    if ((data >= CFG_FLASH_BASE) && (data < (CFG_FLASH_BASE + CFG_FLASH_SIZE)))
    {
        if (hw_mutex_lock(HW_MUTEX_NOR_SPI) == 0)
        {
            puts("autoscript: Failed to lock HW Mutex\n");
            return 1;
        }
        mutex_on = 1;
    }
#endif

#ifdef CONFIG_EXTENDED_FLASH
    ulong retlen;
    int switch_res = process_flash_bank(NULL, data, sizeof(ulong), &retlen, 1);
    debug("%s %d data=0x%x switch_res=%d\n",__FUNCTION__, __LINE__, data, switch_res);
    if (switch_res == EMM_HIGH)
    {
	    /* get length of script */
        len_ptr = (ulong *)((uchar*)data - FLASH_BORDER_OFFSET_16MB);
    }
    else if (switch_res == EMM_LOW)
    {
        if  (sizeof(ulong) != retlen)
        {
            /* Since data is aligned to 4 bytes offset, this should never happen */
            if (retlen != sizeof(ulong))
                printf("error: %s:%d read %d bytes from %d in low bank \n", 
                __FUNCTION__, __LINE__, retlen, sizeof(ulong));
        }
        /* get length of script */
	    len_ptr = (ulong *)data;
    }
#else
	/* get length of script */
	len_ptr = (ulong *)data;
#endif

#if defined(CONFIG_USE_HW_MUTEX)
    /* Release HW Mutes */
    if (mutex_on == 1)
    {
        hw_mutex_unlock(HW_MUTEX_NOR_SPI);
        mutex_on = 0;
    }
#endif

    len = ntohl(*len_ptr);
	if (len == 0) {
		puts ("Empty Script\n");
		return 1; 
	}

	debug ("** Script length: %ld\n", len);

	if ((cmd = malloc (len + 1)) == NULL) {
		return 1; 
	}
#if defined(CONFIG_USE_HW_MUTEX)
    if ((len_ptr >= CFG_FLASH_BASE) && (len_ptr < (CFG_FLASH_BASE + CFG_FLASH_SIZE)))
    {
        /* Lock the HW Mutex */
        if (hw_mutex_lock(HW_MUTEX_NOR_SPI) == 0)
        {
            puts("autoscript: Failed to lock HW Mutex\n");
            return 1;
        }
        mutex_on = 1;
    }
#endif

	while (*len_ptr++);

#if defined(CONFIG_USE_HW_MUTEX)
    /* Release HW Mutes */
    if (mutex_on == 1)
    {
        hw_mutex_unlock(HW_MUTEX_NOR_SPI);
        mutex_on = 0;
    }
#endif

#ifdef CONFIG_EXTENDED_FLASH
    /* restore real address */
    if (switch_res == EMM_HIGH)
        len_ptr = (ulong*)((uchar*) len_ptr + FLASH_BORDER_OFFSET_16MB); 
#endif

	/* make sure cmd is null terminated */
	memmove (cmd, (char *)len_ptr, len);
	*(cmd + len) = 0;

#ifdef CFG_HUSH_PARSER /*?? */
#if defined(CONFIG_USE_HW_MUTEX)
    if ((len_ptr >= CFG_FLASH_BASE) && (len_ptr < (CFG_FLASH_BASE + CFG_FLASH_SIZE)))
    {
        /* Lock the HW Mutex */
        if (hw_mutex_lock(HW_MUTEX_NOR_SPI) == 0)
        {
            puts("autoscript: Failed to lock HW Mutex\n");
            return 1;
        }
        mutex_on = 1;
    }
#endif
    rcode = parse_string_outer (cmd, FLAG_PARSE_SEMICOLON);

#if defined(CONFIG_USE_HW_MUTEX)
    /* Release HW Mutes */
    if (mutex_on == 1)
    {
        hw_mutex_unlock(HW_MUTEX_NOR_SPI);
        mutex_on = 0;
    }
#endif

	
#else
    {
		char *line = cmd;
		char *next = cmd;

		/*
		 * break into individual lines,
		 * and execute each line;
		 * terminate on error.
		 */
		while (*next) {
			if (*next == '\n') {
				*next = '\0';
				/* run only non-empty commands */
				if ((next - line) > 1) {
					debug ("** exec: \"%s\"\n",
						line);
					if (run_command (line, 0) < 0) {
						rcode = 1;
						break;
					}
				}
				line = next + 1;
			}
			++next;
		}
	}
#endif
	free (cmd);
	return rcode;
}

#endif	/* CONFIG_AUTOSCRIPT || CFG_CMD_AUTOSCRIPT */
/**************************************************/
#if (CONFIG_COMMANDS & CFG_CMD_AUTOSCRIPT)
int
do_autoscript (cmd_tbl_t *cmdtp, int flag, int argc, char *argv[])
{
	ulong addr;
	int rcode;

	if (argc < 2) {
		addr = CFG_LOAD_ADDR;
	} else {
		addr = simple_strtoul (argv[1],0,16);
	}

	printf ("## Executing script at %08lx\n",addr);
	rcode = autoscript (addr);
	return rcode;
}

#if (CONFIG_COMMANDS & CFG_CMD_AUTOSCRIPT)
U_BOOT_CMD(
	autoscr, 2, 0,	do_autoscript,
	"autoscr - run script from memory\n",
	"[addr] - run script starting at addr"
	" - A valid autoscr header must be present\n"
);
#endif /* CFG_CMD_AUTOSCRIPT */

#endif /* CONFIG_AUTOSCRIPT || CFG_CMD_AUTOSCRIPT */
