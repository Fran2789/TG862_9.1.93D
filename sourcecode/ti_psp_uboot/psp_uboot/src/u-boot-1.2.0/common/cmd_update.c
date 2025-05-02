/*
 * GPL LICENSE SUMMARY
 *
 *  Copyright(c) 2012 - 2013 Intel Corporation.
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
#include <command.h>
#include <environment.h>
#include <image.h>
#include <uImage.h>
#include <rtc.h>


#ifdef CONFIG_SYSTEM_PUMA6_SOC

#include <mmc_utils.h>
#include <mmc.h>
#include <docsis_ip_boot_params.h> 

#define STATUS_NOK      1
#define STATUS_OK       0

#define TYPE_IMG_NONE   		0
#define TYPE_IMG_ALL    		1
#define TYPE_IMG_ATOM   		2
#define TYPE_IMG_ARM    		3
#define TYPE_IMG_UBOOT  		4
#define TYPE_IMG_ATOM_INNER		5

#define TYPE_FLASH_NONE 0
#define TYPE_FLASH_SPI  1
#define TYPE_FLASH_MMC  2

#define ENV_BASE_DEC    10
#define ENV_BASE_HEX    16

#define CHECK_IMAGE_ENABLE   1
#define CHECK_IMAGE_DISABLE  0
#define IGNORE_ERROR_ENABLE  1
#define IGNORE_ERROR_DISABLE 0

#define VERBOSE_ENABLE  1
#define VERBOSE_DISABLE 0

#define IMAGENAME_ARMATOM  "uImageArm11Atom.img"
#define IMAGENAME_ATOM     "appcpuImage"

/* Heuristic definition of u-boot header. */
/* u-boot image does not define a header. Instead we define the first 18 assembly commands as a header */
/* Defining a header for u-boot is necessary for validating the u-boot image */ 
#define UBOOT_TEXT_BASE_OFFSET 0x01FB0000     /* The u-boot code base address in DDR (offset from Docsis DDR Base) */ 
#define UBOOT_MAGIC            0xEA000012     /* This is the first assembly command */
struct ubootHeader
{
    unsigned int resetVector[8];
    unsigned int exceptionsVector[8];
    unsigned int text_base;
    unsigned int start;
    unsigned int bss_start;
    unsigned int end;
};

static int validate_uimage_header(struct uImageHeader *hdr, int verbose, int check_image);
static int validate_ubfi_header  (image_header_t *hdr, int verbose, int check_image);
static int validate_uboot_header (struct ubootHeader *hdr, int verbose, int ignore_error);

static int checkPartitionsTypeValidity(struct uImageHeader* hdr, int checkForImage);
static int parseArmUImageHeader (void* headerAddr, unsigned int* arm_header_addr,  int verbose);
static int parseAtomUImageHeader(void* headerAddr, unsigned int* atom_header_addr, int verbose);

static void print_ubfi_hdr       (image_header_t *hdr);
static void print_uImage_hdr     (struct uImageHeader *hdr);
static void print_uboot_hdr      (struct ubootHeader *hdr);

static int parse_uimage_header(void *addr,unsigned int *arm_header_addr,unsigned int *atom_header_addr, int verbose, int check_image);
static int parse_arm_header   (void *addr,unsigned int *kernel_addr,unsigned int *kernel_size,unsigned int *rootfs_addr,unsigned int *rootfs_size,unsigned int *gwfs_addr,unsigned int *gwfs_size, int verbose, int check_image);
static int parse_atom_header  (void *addr,unsigned int *kernel_addr,unsigned int *kernel_size,unsigned int *rootfs_addr,unsigned int *rootfs_size, int verbose, int check_image);
static int parse_uboot_header (void *addr,unsigned int *uboot_addr,unsigned int *uboot_size, int verbose, int ignore_error);

static int getenv_base(char* param,int base);
static int mmc_part_info(int part, unsigned int *offset,unsigned int *size);

static int flash_write_spi(unsigned int src, unsigned int dest, unsigned int length, int verbose);

int do_update (cmd_tbl_t *cmdtp, int flag, int argc, char *argv[])
{
    int i=0;
    int img_type = TYPE_IMG_ARM;           	/* default image type is ARM */
    int flash_type = TYPE_FLASH_NONE;
    void *addr = (void *)0x40900000;       	/* default source address is 0x40900000 */
 
	int actimage = 0;
    int check_image = CHECK_IMAGE_DISABLE; 	/* Default is not to validate the image */    
    int ignore_error = IGNORE_ERROR_DISABLE;/* Default is not to ignore errors */
    int verbose = VERBOSE_DISABLE;         	/* Default is not verbose */      

    unsigned int arm_header_addr  			= 0;
    unsigned int atom_header_addr 			= 0;

    unsigned int arm_kernel_addr  			= 0;
    unsigned int arm_kernel_size  			= 0; 
    unsigned int arm_rootfs_addr  			= 0; 
    unsigned int arm_rootfs_size  			= 0;
    unsigned int arm_gwfs_addr    			= 0; 
    unsigned int arm_gwfs_size    			= 0;

    unsigned int atom_kernel_addr 			= 0;
    unsigned int atom_kernel_size 			= 0;
    unsigned int atom_rootfs_addr 			= 0; 
    unsigned int atom_rootfs_size 			= 0;

    unsigned int arm_ubfi_offset_spi    	= 0;
    unsigned int arm_kernel_offset_mmc  	= 0;
    unsigned int arm_kernel_size_mmc    	= 0;
    unsigned int arm_rootfs_offset_mmc  	= 0;
    unsigned int arm_rootfs_size_mmc    	= 0;
    unsigned int arm_gwfs_offset_mmc    	= 0;
    unsigned int arm_gwfs_size_mmc      	= 0;

    unsigned int atom_ubfi_offset_spi   	= 0;  
    unsigned int atom_kernel_offset_mmc 	= 0;
    unsigned int atom_kernel_size_mmc   	= 0;
    unsigned int atom_rootfs_offset_mmc 	= 0;
    unsigned int atom_rootfs_size_mmc   	= 0;

    unsigned int uboot_addr        			= 0;
    unsigned int uboot_size        			= 0;
    unsigned int uboot_offset_spi  			= 0;
    unsigned int uboot_offset_mmc  			= 0;
    unsigned int uboot_size_mmc    			= 0;

    /* Parse Command Line   */
    /* -------------------- */
    for (i=0;i<argc;i++)
	{
        if (strcmp(argv[i], "-t") == 0)
		{
            if (strcmp(argv[i+1], "all") == 0)
                img_type = TYPE_IMG_ALL;
            else if (strcmp(argv[i+1], "arm") == 0)
                img_type = TYPE_IMG_ARM;
            else if (strcmp(argv[i+1], "atom") == 0)
                img_type = TYPE_IMG_ATOM;
            else if (strcmp(argv[i+1], "uboot") == 0)
                img_type = TYPE_IMG_UBOOT;
            else
			{
               printf("Error: Illegal Image Type.\n");
               printf ("Usage:\n%s\n", cmdtp->usage);
               return 1;
            }
        }

        if (strcmp(argv[i], "-f") == 0)
		{
            if (strcmp(argv[i+1], "spi") == 0)
                flash_type = TYPE_FLASH_SPI;
            else if (strcmp(argv[i+1], "mmc") == 0)
                flash_type = TYPE_FLASH_MMC;
            else 
			{
                printf("Error: Illegal Flash Type.\n");
                printf ("Usage:\n%s\n", cmdtp->usage);
                return 1;
            }
        }

        if (strcmp(argv[i], "-v") == 0)
        {
            verbose = VERBOSE_ENABLE;
        }

        if (strcmp(argv[i], "-c") == 0)
        {
            check_image = CHECK_IMAGE_ENABLE;
        }

        if (strcmp(argv[i], "-a") == 0)
		{
            addr = (void*)simple_strtoul(argv[i+1], NULL, ENV_BASE_HEX);
            if ((addr < (void*)0x40900000) || (addr > (void*)0x80000000))
			{     /* check if address is in range of DDR */
                printf("Error: Illegal source address - valid address starts at 0x40900000.\n");
                printf ("Usage:\n%s\n", cmdtp->usage);
                return 1;
            }
        }

        if (strcmp(argv[i], "-i") == 0)
        {
            ignore_error = IGNORE_ERROR_ENABLE;
        }
    }

    /* Set default flash type */
    /* ---------------------- */
    if (flash_type == TYPE_FLASH_NONE)
    {
        char *cmd;

        if ((cmd = getenv ("bootdevice")) != NULL)
        {
            if (strcmp(cmd,"mmc") == 0)
            {
                flash_type = TYPE_FLASH_MMC;
            }

            if (strcmp(cmd,"spi") == 0)
            {
                flash_type = TYPE_FLASH_SPI;
            }
        }
    }
	/* Get actimage destination */
    if (img_type != TYPE_IMG_UBOOT)
	{
        actimage = simple_strtoul(argv[argc-1], NULL, 10);
        if ((actimage != 1) && (actimage != 2))
		{
            printf("Active Image was not specified (1 or 2)\n");
            //printf ("Usage:\n%s\n", cmdtp->usage);
            return 1;
        }
    }

    /* Parse Image Headers */
    /* -------------------- */
    switch (img_type)
	{
		case TYPE_IMG_ALL:
        /* Read uImage Header and get ARM Image header offset and ATOM Image offset */
			if (parse_uimage_header(addr,&arm_header_addr,&atom_header_addr,verbose,check_image) != STATUS_OK)
				return STATUS_NOK;
			if (parse_arm_header((void*)arm_header_addr,&arm_kernel_addr,&arm_kernel_size,&arm_rootfs_addr,&arm_rootfs_size,&arm_gwfs_addr,&arm_gwfs_size,verbose,CHECK_IMAGE_DISABLE) != STATUS_OK)
				return STATUS_NOK;
			if (parse_atom_header((void*)atom_header_addr,&atom_kernel_addr,&atom_kernel_size,&atom_rootfs_addr,&atom_rootfs_size,verbose,CHECK_IMAGE_DISABLE) != STATUS_OK)
				return STATUS_NOK;
		break;
		case TYPE_IMG_ARM:
			/* Check if header is old or new */
			if (parseArmUImageHeader(addr, &arm_header_addr, verbose) == STATUS_NOK)
				return STATUS_NOK;
			/* Read ARM Image header, and get ARM kernel offset, ARM RootFS offset and ARM GWFS offset */
			if (parse_arm_header((void*)arm_header_addr,&arm_kernel_addr,&arm_kernel_size,&arm_rootfs_addr,&arm_rootfs_size,&arm_gwfs_addr,&arm_gwfs_size,verbose,check_image)!= STATUS_OK)
				return STATUS_NOK;
		break;
		case TYPE_IMG_ATOM:
			/* Check if header is old or new */
			if (parseAtomUImageHeader(addr, &atom_header_addr, verbose) == STATUS_NOK)
				return STATUS_NOK;
			/* Read ATOM Image Header, and get ATOM Kernel Image and ATOM Root FS offset */
			if (parse_atom_header((void*)atom_header_addr,&atom_kernel_addr,&atom_kernel_size,&atom_rootfs_addr,&atom_rootfs_size,verbose,check_image)!= STATUS_OK)
				return STATUS_NOK;
        break;
		case TYPE_IMG_UBOOT:
			/* Uboot image does not have header. This function only does some heuristic checks to validate the image */
			if (parse_uboot_header(addr,&uboot_addr,&uboot_size,verbose,ignore_error) != STATUS_OK)
				return STATUS_NOK;
		break;
    }
    
    /* Program to flash */
    /* ----------------- */
    if (flash_type == TYPE_FLASH_MMC)
    {
        if ((img_type == TYPE_IMG_ARM) || (img_type == TYPE_IMG_ALL))
        {
            /* Read mmc partitions offset and size, for ARM11 kernel, rootfs and GW FS */
            if (actimage == 1)
            {
                if (mmc_part_info(getenv_base("mmc_part_arm11_kernel_0",ENV_BASE_DEC),&arm_kernel_offset_mmc,&arm_kernel_size_mmc) != STATUS_OK)
                    return 1;
                if (mmc_part_info(getenv_base("mmc_part_arm11_rootfs_0",ENV_BASE_DEC),&arm_rootfs_offset_mmc,&arm_rootfs_size_mmc) != STATUS_OK)
                    return 1;
                if (mmc_part_info(getenv_base("mmc_part_arm11_gw_fs_0",ENV_BASE_DEC),&arm_gwfs_offset_mmc,&arm_gwfs_size_mmc) != STATUS_OK)
                    return 1;
            }
            else
            {
                if (mmc_part_info(getenv_base("mmc_part_arm11_kernel_1",ENV_BASE_DEC),&arm_kernel_offset_mmc,&arm_kernel_size_mmc) != STATUS_OK)
                    return 1;
                if (mmc_part_info(getenv_base("mmc_part_arm11_rootfs_1",ENV_BASE_DEC),&arm_rootfs_offset_mmc,&arm_rootfs_size_mmc) != STATUS_OK)
                    return 1;
                if (mmc_part_info(getenv_base("mmc_part_arm11_gw_fs_1",ENV_BASE_DEC),&arm_gwfs_offset_mmc,&arm_gwfs_size_mmc) != STATUS_OK)
                    return 1;
            }

            /* Validate the images sizes do not exceed the mmc partiton size */ 
            if (arm_kernel_size_mmc < arm_kernel_size)
            {
                printf("Error: ARM Kernel image is too large!\n");
                return 1;
            }
            if (arm_rootfs_size_mmc < arm_rootfs_size)
            {
                printf("Error: ARM Root FS image is too large!\n");
                return 1;
            }
            if (arm_gwfs_size_mmc < arm_gwfs_size) 
            {
                printf("Error: ARM GW FS image is too large!\n");
                return 1;
            }

            /* Program ARM Kernel to eMMC */
            printf("Update 'ARM Kernel' to flash\n"); 
            if (flash_write_mmc(arm_kernel_addr,arm_kernel_offset_mmc,arm_kernel_size,verbose) != STATUS_OK)
                return 1;

            /* Program ARM Root FS to eMMC */
            printf("Update 'ARM Root FS' to flash\n"); 
            if (flash_write_mmc(arm_rootfs_addr,arm_rootfs_offset_mmc,arm_rootfs_size,verbose)!= STATUS_OK)
                return 1;

            /* Program ARM GW to eMMC */
            if (arm_gwfs_size != 0)
            {
                printf("Update 'ARM GW' to flash\n");
                if (flash_write_mmc(arm_gwfs_addr,arm_gwfs_offset_mmc,arm_gwfs_size,verbose)!= STATUS_OK)
                    return 1;
            }
        }

        if ((img_type == TYPE_IMG_ATOM) || (img_type == TYPE_IMG_ALL))
        {
            /* Read mmc partitions offset and size, for ARM11 kernel, rootfs and GW FS */
            if (actimage == 1)
            {
                if (mmc_part_info(getenv_base("mmc_part_atom_kernel_0",ENV_BASE_DEC),&atom_kernel_offset_mmc,&atom_kernel_size_mmc) != STATUS_OK)
                    return 1;
                if (mmc_part_info(getenv_base("mmc_part_atom_rootfs_0",ENV_BASE_DEC),&atom_rootfs_offset_mmc,&atom_rootfs_size_mmc) != STATUS_OK)
                    return 1;
            }
            else
            {
                if (mmc_part_info(getenv_base("mmc_part_atom_kernel_1",ENV_BASE_DEC),&atom_kernel_offset_mmc,&atom_kernel_size_mmc) != STATUS_OK)
                    return 1;
                if (mmc_part_info(getenv_base("mmc_part_atom_rootfs_1",ENV_BASE_DEC),&atom_rootfs_offset_mmc,&atom_rootfs_size_mmc) != STATUS_OK)
                    return 1;
            }

            /* Validate the images sizes do not exceed the mmc partiton size */ 
            if (atom_kernel_size_mmc < atom_kernel_size)
            {
                printf("Error: IA Kernel image is too large!\n");
                return 1;
            }
            if (atom_rootfs_size_mmc < atom_rootfs_size)
            {
                printf("Error: IA Root FS image is too large!\n");
                return 1;
            }

            /* Program ATOM Kernel to eMMC */
            printf("Update 'ATOM Kernel' to flash\n");
            if(flash_write_mmc(atom_kernel_addr,atom_kernel_offset_mmc,atom_kernel_size,verbose)!= STATUS_OK)
                return 1;

            /* Program ATOM RootFS to eMMC */
            printf("Update 'ATOM Root FS' to flash\n"); 
            if(flash_write_mmc(atom_rootfs_addr,atom_rootfs_offset_mmc,atom_rootfs_size,verbose)!= STATUS_OK)
                return 1;
        }

        if (img_type == TYPE_IMG_UBOOT) 
        {
            /* Read mmc uboot offset and size, for ARM11 kernel, rootfs and GW FS */
            uboot_offset_mmc = (unsigned int)getenv_base("ubootoffset",ENV_BASE_HEX);
            uboot_size_mmc   = (unsigned int)getenv_base("ubootsize",ENV_BASE_HEX);

            /* Validate the image size do not exceed the mmc allocated size */ 
            if (uboot_size_mmc < uboot_size)
            {
                printf("Error: IA Root FS image is too large!\n");
                return 1;
            }

            /* Program U-BOOT to eMMC */
            printf("Update 'U-BOOT' to flash\n");
            if(flash_write_mmc(uboot_addr,uboot_offset_mmc,uboot_size_mmc,verbose)!= STATUS_OK)
                return 1;
        }
    }

    if (flash_type == TYPE_FLASH_SPI)
    {
        if (actimage == 2)
        {
            printf("Error: SPI do not have second bank - \"Active Image 2\"\n");
            return 1;
        }
        
        if ((img_type == TYPE_IMG_ARM) || (img_type == TYPE_IMG_ALL))
        {
            arm_ubfi_offset_spi = (unsigned int)getenv_base("arm11ubfioffset1",ENV_BASE_HEX);

            if (arm_ubfi_offset_spi == 0)
            {
                printf("Error: 'arm11ubfioffset1=0'\n"); 
                return 1;
            }

            /* Program ARM UBFI to SPI */
            printf("Update 'ARM UBFI' to flash\n"); 
            if (flash_write_spi(arm_kernel_addr,arm_ubfi_offset_spi,arm_kernel_size+arm_rootfs_size+arm_gwfs_size,verbose) != STATUS_OK)
                return 1;
        }

        if ((img_type == TYPE_IMG_ATOM) || (img_type == TYPE_IMG_ALL))
        {
            atom_ubfi_offset_spi = (unsigned int)getenv_base("atomubfioffset1",ENV_BASE_HEX);
            
            if (atom_ubfi_offset_spi == 0) 
            {
                printf("Error: 'atomubfioffset1=0'\n");
                return 1;
            }

            /* Program ATOM uImage to SPI */
            printf("Update 'ATOM uImage' to flash\n"); 
            if (flash_write_spi(atom_header_addr-UIMAGE_HEADER_SIZE,atom_ubfi_offset_spi,UIMAGE_HEADER_SIZE+atom_kernel_size+atom_rootfs_size,verbose) != STATUS_OK)
                return 1;
        }

        if (img_type == TYPE_IMG_UBOOT) 
        {
            uboot_offset_spi = (unsigned int)getenv_base("ubootoffset",ENV_BASE_HEX);

             /* Program U-BOOT to eMMC */
            printf("Update 'U-BOOT' to flash\n");
            if (flash_write_spi(uboot_addr,uboot_offset_spi,uboot_size,verbose) != STATUS_OK)
                return 1;
        }
    }

    return 0;
}

/* Validate UBFI Image header */
static int validate_ubfi_header(image_header_t *hdr, int verbose, int check_image)
{
    unsigned long crc;

    /* Check magic number */
    if (ntohl(hdr->ih_magic) != IH_MAGIC) 
    {
        printf ("Error: Bad magic number %X\n", ntohl(hdr->ih_magic));
        return STATUS_NOK;
    }

    /* Check Header CRC */
    crc = ntohl(hdr->ih_hcrc);    /* save original CRC */
    hdr->ih_hcrc = 0;             /* Reset CRC to zero */
    if (crc32(0, (uchar *)hdr, sizeof (image_header_t)) != crc)
    {
        printf ("Error: Bad header crc (script)\n");
        return STATUS_NOK;
    }
    hdr->ih_hcrc = htonl(crc);   /* Restore original CRC */
   
    if (verbose == VERBOSE_ENABLE)
    {
        /* Print header */
        print_ubfi_hdr(hdr);
    }

    if (check_image == CHECK_IMAGE_ENABLE)
    {
        /* Check Data CRC */
        printf ("Verifying Checksum ... ");
        if (crc32 (0, ((uchar*)hdr)+sizeof(image_header_t), ntohl(hdr->ih_size)) != ntohl(hdr->ih_dcrc)) {
            printf ("Bad Data CRC\n");
            return STATUS_NOK;
        }
        printf ("OK\n");
    }

    return STATUS_OK;
}

/* Validate Unified Image header */
static int validate_uimage_header(struct uImageHeader *hdr, int verbose, int check_image)
{
    unsigned long hCrc;
    unsigned long dCrc;

    /* Validate Magic number */
    if ((hdr->magic != IMAGE_MAGIC) && (hdr->magic != IMAGE_MAGIC_VER2))
    {
        printf("Error: Bad Magic Number.\n");
        return STATUS_NOK; 
    }

    /* Validate Header CRC number */
    hCrc = ntohl(hdr->hCrc);
    hdr->hCrc = 0;
    
    if (crc32(0, (uchar *)hdr, UIMAGE_HEADER_SIZE) != hCrc)
    {
        printf("Error: Bad Header CRC\n");
        return STATUS_NOK;
    }
    hdr->hCrc = htonl(hCrc);
    
    if (verbose == VERBOSE_ENABLE)
    {
        /* Print header */
        print_uImage_hdr (hdr);
    }

    if (check_image == CHECK_IMAGE_ENABLE)
    {
        /* Validate Data CRC number */
        printf ("Verifying Checksum ... ");
        dCrc = ntohl(hdr->dCrc);
        if (crc32(0, ((uchar *)hdr)+UIMAGE_HEADER_SIZE, (hdr->imageSize)-UIMAGE_HEADER_SIZE) != dCrc)
        {
            printf("Error: Bad Data CRC\n");
            return STATUS_NOK;
        }
        printf ("OK\n");
    }

    return STATUS_OK;
}

/* Validate Unified Image header */
static int validate_uboot_header(struct ubootHeader *hdr, int verbose, int ignore_error)
{
    unsigned long rambase;
    unsigned long ramoffset;

    /* Validate Magic number - check the first assembly command */
    if (hdr->resetVector[0] != UBOOT_MAGIC)
    {
        printf("Error: Bad Magic Number.\n");
        return STATUS_NOK; 
    }
   
    if (verbose == VERBOSE_ENABLE)
    {
        /* Print header */
        print_uboot_hdr (hdr);
    }
    
    /* Validate image compilation address */
    if (verbose == VERBOSE_ENABLE)
    {
        printf ("Verifying image address ... ");
    }
    ramoffset = (unsigned int)getenv_base("ramoffset",ENV_BASE_HEX);
    rambase =   (unsigned int)getenv_base("rambase",  ENV_BASE_HEX);
    if ((hdr->text_base) != (rambase + ramoffset + UBOOT_TEXT_BASE_OFFSET))
    {
        if (ignore_error != 0) 
        {
            printf("Warning: The new U-Boot image was compiled to a different address in RAM.\n"); 
        }
        else
        {
            printf("Error:  The new U-Boot image was compiled to a different address in RAM.\n"); 
            return STATUS_NOK;
        }
    }

    if (verbose == VERBOSE_ENABLE)
    {
        printf ("OK\n");
    }

    return STATUS_OK;
}

/* Check validity of header's pType according to requested <checkForImage>*/
static int checkPartitionsTypeValidity(struct uImageHeader* hdr, int checkForImage)
{
	uint32_t firstImg  = hdr->pType[0];
	uint32_t secondImg = hdr->pType[1];
	int ret = STATUS_NOK;

	switch (checkForImage)
	{
	case TYPE_IMG_ALL:
		if ((((firstImg == PART_TYPE_APPCPU_UIMG) && (secondImg == PART_TYPE_NPCPU_UBFI  )) ||
             ((firstImg == PART_TYPE_NPCPU_UBFI)  && (secondImg == PART_TYPE_APPCPU_UIMG )) ) && hdr->partNum == 2)
			ret = STATUS_OK;
		break;
	case TYPE_IMG_ATOM:
		if ((firstImg == PART_TYPE_APPCPU_UIMG) && (hdr->partNum == 1))
			ret = STATUS_OK;
		break;
	case TYPE_IMG_ARM:
		if ((firstImg == PART_TYPE_NPCPU_UBFI) && (hdr->partNum == 1))
			ret = STATUS_OK;
		break;
    case TYPE_IMG_ATOM_INNER:
        if ((((firstImg == PART_TYPE_APPCPU_KERNEL_BIN) && (secondImg == PART_TYPE_APPCPU_ROOTFS_BIN )) || 
             ((firstImg == PART_TYPE_APPCPU_ROOTFS_BIN) && (secondImg == PART_TYPE_APPCPU_KERNEL_BIN )) ) && hdr->partNum == 2)
			ret = STATUS_OK;
        break;

	}

	return ret;
}

/* Parse image header for NEW = uImage_uImage_atomImage OR OLD = uImage_atomImage version */
static int parseAtomUImageHeader(void* headerAddr, unsigned int* atom_header_addr, int verbose)
{
	struct uImageHeader *hdr = (struct uImageHeader*)headerAddr;

	/* VER2 header = uImage_uImage */
	if (hdr->magic == IMAGE_MAGIC_VER2)
	{
		if (checkPartitionsTypeValidity(hdr, TYPE_IMG_ATOM) == STATUS_OK)
		{
            *atom_header_addr = (int)headerAddr + (int)UIMAGE_HEADER_SIZE;
		}
        else
        if (checkPartitionsTypeValidity(hdr, TYPE_IMG_ATOM_INNER) == STATUS_OK)
		{
            *atom_header_addr = (int)headerAddr;
		}
        else
        {
            printf("Error: invalid header.\n");
			return STATUS_NOK;
        }
		
		if (verbose == VERBOSE_ENABLE)
		{
			printf("Image header is of type ATOM VER2\n");
		}
		return STATUS_OK;
	}

	/* VER1 header = uImage */
	if (hdr->magic == IMAGE_MAGIC)
	{
		*atom_header_addr = (int)headerAddr;
		if (verbose == VERBOSE_ENABLE)
		{
			printf("Image header is of type ATOM VER1\n");
		}
		return STATUS_OK;
	}
	
	printf("Error: invalid header.\n");

	return STATUS_NOK;
}

/* Parse image header for NEW = uImage_UBFI_armImage OR OLD = UBFI_armImage version */
static int parseArmUImageHeader(void* headerAddr, unsigned int* arm_header_addr, int verbose)
{
	image_header_t *hdr_ubfi        = (image_header_t*)headerAddr;
	struct uImageHeader *hdr_uImage = (struct uImageHeader*)headerAddr;

	/* VER2 header = uImage_UBFI*/
	if (hdr_uImage->magic == IMAGE_MAGIC_VER2)
	{
		if (checkPartitionsTypeValidity(hdr_uImage, TYPE_IMG_ARM) == STATUS_NOK)
		{
			printf("Error: invalid header.\n");
			return STATUS_NOK;
		}
		*arm_header_addr = (int)headerAddr + (int)UIMAGE_HEADER_SIZE;
		if (verbose == VERBOSE_ENABLE)
		{
			printf("Image header is of type ARM VER2\n");
		}
		return STATUS_OK;
	}

	/* VER1 header = UBFI*/
	if (hdr_ubfi->ih_magic == IH_MAGIC)
	{
		*arm_header_addr = (unsigned int)headerAddr;
		if (verbose == VERBOSE_ENABLE)
		{
			printf("Image header is of type ARM VER1\n");
		}
		return STATUS_OK;
	}

	printf("Error: invalid header.\n");

	return STATUS_NOK;
}

/* Parse Unified ARM & ATOM Image header */
static int parse_uimage_header(void *addr,unsigned int *arm_header_addr,unsigned int *atom_header_addr, int verbose, int check_image)
{
    struct uImageHeader *hdr = (struct uImageHeader*)addr;
	int atomImageSize = 0;
	int armImageSize  = 0;

    if (verbose == VERBOSE_ENABLE)
    {
        printf("Reading ARM & ATOM Unified Image.\n");
    }

    if (validate_uimage_header(hdr,verbose,check_image) != STATUS_OK )
        return STATUS_NOK;

    /* check image name */
    if (strcmp(hdr->name,IMAGENAME_ARMATOM) != 0)
    {
        printf("Error: wrong image name '%s'  (expected '%s')\n",hdr->name,IMAGENAME_ARMATOM);
        return STATUS_NOK;
    }

	/* VER2 header */
	if (hdr->magic == IMAGE_MAGIC_VER2)
	{
		if (checkPartitionsTypeValidity(hdr, TYPE_IMG_ALL) == STATUS_NOK)
		{
			printf("Error: invalid ARM + ATOM image.\n");
			return STATUS_NOK;
		}
		if (hdr->pType[0] == PART_TYPE_NPCPU_UBFI)
		{
			*arm_header_addr  = (int)addr + (int)UIMAGE_HEADER_SIZE;
			*atom_header_addr = *arm_header_addr + hdr->size[0];
			armImageSize      = hdr->size[0];
			atomImageSize     = hdr->size[1];
		}
		else
		{
			*atom_header_addr = (int)addr + (int)UIMAGE_HEADER_SIZE;
			*arm_header_addr  = *atom_header_addr + hdr->size[0];
			atomImageSize     = hdr->size[0];
			armImageSize      = hdr->size[1];
		}
	}
	/* VER1 header */
	else
	{
		*arm_header_addr  = (int)addr + (int)UIMAGE_HEADER_SIZE;
		*atom_header_addr = *arm_header_addr + hdr->size[0];
		armImageSize      = hdr->size[0];
		atomImageSize     = hdr->size[1];
	}

	if (verbose == VERBOSE_ENABLE)
	{
		printf("ARM  Image address: 0x%X. Size: %d\n", *arm_header_addr,  armImageSize);
		printf("ATOM Image address: 0x%X. Size: %d\n", *atom_header_addr, atomImageSize);
	}
    return STATUS_OK;
}

/* Parse Unified ATOM Image header */
static int parse_atom_header  (void *addr,unsigned int *kernel_addr,unsigned int *kernel_size,unsigned int *rootfs_addr,unsigned int *rootfs_size, int verbose, int check_image)
{
    struct uImageHeader *hdr = (struct uImageHeader*)addr;
    
    if (verbose == VERBOSE_ENABLE)
    {
        printf("Reading ATOM Unified Image.\n");
    }

    if (validate_uimage_header(hdr,verbose,check_image) != STATUS_OK )
        return STATUS_NOK;

    /* check if image name has IMAGENAME_ATOM as a substring */
    if (strstr(hdr->name, IMAGENAME_ATOM) == 0)
    {
        printf("Error: wrong image name '%s'  (expected to contain '%s')\n",hdr->name,IMAGENAME_ATOM);
        return STATUS_NOK;
    }

    /* VER2 header */
	if (hdr->magic == IMAGE_MAGIC_VER2)
	{
        if (hdr->pType[0] == PART_TYPE_APPCPU_KERNEL_BIN)
		{
            *kernel_addr  = ((unsigned int)addr) + UIMAGE_HEADER_SIZE;
            *kernel_size  = hdr->size[0];
            *rootfs_addr  = *kernel_addr + *kernel_size;
            *rootfs_size  = hdr->size[1];
        }
        else
        {
            *rootfs_addr = ((unsigned int)addr) + UIMAGE_HEADER_SIZE;
            *rootfs_size = hdr->size[0];
            *kernel_addr = *rootfs_addr + *rootfs_size;
            *kernel_size = hdr->size[1];
        }
    }
    else
    {
        *kernel_addr  = ((unsigned int)addr) + UIMAGE_HEADER_SIZE;
        *kernel_size  = hdr->size[0];
        *rootfs_addr  = *kernel_addr + *kernel_size;
        *rootfs_size  = hdr->size[1];
    }

    if (verbose == VERBOSE_ENABLE)
    {
        printf("ATOM Kernel address:  0x%X. Size: %d\n", *kernel_addr, *kernel_size);
        printf("ATOM Root FS address: 0x%X. Size: %d\n", *rootfs_addr, *rootfs_size);
        printf("\n");
    }

    return STATUS_OK;
}

/* Parse ARM UBFI header */
static int parse_arm_header(void *addr,unsigned int *kernel_addr,unsigned int *kernel_size,unsigned int *rootfs_addr,unsigned int *rootfs_size,unsigned int *gwfs_addr,unsigned int *gwfs_size, int verbose, int check_image)
{
    image_header_t *hdr_script = (image_header_t*)addr;
    image_header_t *hdr_multi;
    unsigned int* img_size_array;
    unsigned int* data;

    if (verbose == VERBOSE_ENABLE)
    {
        printf("Reading ARM UBFI.\n");
    }

    /* Validate ARM Script Image */
    if (verbose == VERBOSE_ENABLE)
    {
        printf("Reading ARM Script image.\n");
    }
    if (validate_ubfi_header(hdr_script,verbose,check_image) != STATUS_OK)
        return STATUS_NOK;

    /* Set Multi Image header pointer */
    hdr_multi = (image_header_t*)((char*)hdr_script + sizeof (image_header_t) + hdr_script->ih_size) ;

    /* Validate ARM Multi image */
    if (verbose == VERBOSE_ENABLE)
    {
        printf("Reading ARM Multi-images image.\n");
    }
    if (validate_ubfi_header(hdr_multi,verbose,check_image) != STATUS_OK)
        return STATUS_NOK;

    /* Set pointer to Images Size array */
    img_size_array = (unsigned int*)((char*)hdr_multi + sizeof (image_header_t));

    /* Skip Images Size array */
    data = img_size_array;
    while (*data != 0)
        data++;
    data++; /* skip null-terminator */

    /* Set all images sizes */
    *kernel_size = img_size_array[0] + ((char*)data-(char*)addr); 
    *rootfs_size = img_size_array[1];
    if (*rootfs_size == 0)
    {
        /* Probably using wrong image, no base file system */
        printf("FileSystem size == 0, check the image\n");
        return STATUS_NOK;
    }
    *gwfs_size   = img_size_array[2];                      /* Case when image do not have GW FS, this img_size_array[2] will be zero */ 
    
    /* Set all images addresses  */
    *kernel_addr = (unsigned int)hdr_script;
    *rootfs_addr = *kernel_addr + *kernel_size;
    *gwfs_addr   = *rootfs_addr + *rootfs_size;

    if (verbose == VERBOSE_ENABLE)
    {
        printf("ARM Kernel  address: 0x%X. Size: %d\n",*kernel_addr,*kernel_size);
        printf("ARM Root FS address: 0x%X. Size: %d\n",*rootfs_addr,*rootfs_size);
        if (*gwfs_size != 0)
        {
            printf("ARM GW FS   address: 0x%X. Size: %d\n",*gwfs_addr,*gwfs_size);
        }
        printf("\n");
    }

    return STATUS_OK;
}

/* Parse U-boot header */
static int parse_uboot_header   (void *addr,unsigned int *uboot_addr,unsigned int *uboot_size, int verbose, int ignore_error)
{
    struct ubootHeader  *hdr = (struct ubootHeader *)addr;

    if (verbose == VERBOSE_ENABLE)
    {
        printf("Reading U-BOOT.\n");
    }

    if (validate_uboot_header(hdr,verbose,ignore_error) != STATUS_OK)
        return STATUS_NOK;

    *uboot_addr = (unsigned int)addr;
    *uboot_size = hdr->bss_start - hdr->start;
    
    return STATUS_OK;
}


/* Print Unified Image Header */
static void print_uImage_hdr (struct uImageHeader *hdr)
{
    int i=0;

    printf("uImage header:\n");
    printf ("   Image Name: %s\n", hdr->name);
    printf ("   Data Size:  %d\n", hdr->imageSize);
	printf ("   Date:       %d/%d/%d\n",(hdr->date>>0 )&0xFF,(hdr->date>>8)&0xFF,((hdr->date>>16)&0xFF) + 1900);
    printf ("   Time:       %02d:%02d:%02d\n",(hdr->time>>0)&0xFF,(hdr->time>>8)&0xFF,(hdr->time>>16 )&0xFF);
    for (i=0;i<MAX_PART_NUM;i++)
    {
        if (hdr->size[i] == 0)
            break;
        printf ("   Image %d: %d Bytes\n",i,hdr->size[i]);

    }
    return;
}

/* Print UBFI Header */
static void print_ubfi_hdr (image_header_t *hdr)
{

    printf("UBFI header:\n");
    printf ("   Image Name:  %.*s\n", IH_NMLEN, hdr->ih_name);
	printf ("   Data Size:   %d Bytes = ", ntohl(hdr->ih_size));
    print_size (ntohl(hdr->ih_size), "\n");
    if (hdr->ih_type == IH_TYPE_MULTI) {
		int i;
		ulong len;
		ulong *len_ptr = (ulong *)((ulong)hdr + sizeof(image_header_t));

		puts ("   Contents:\n");
		for (i=0; (len = ntohl(*len_ptr)); ++i, ++len_ptr) {
			printf ("   Image %d:    %8ld Bytes = ", i, len);
			print_size (len, "\n");
		}
	}
    return;
}

/* Print UBFI Header */
static void print_uboot_hdr (struct ubootHeader *hdr)
{

    printf("U-Boot header:\n");
    printf ("   Base Address:  0x%0.8X\n", hdr->text_base);
	printf ("   Data Size:     %d Bytes\n", hdr->bss_start - hdr->text_base);
   
    return;
}

/* Get Env value from u-boot env */
static int getenv_base(char* param,int base)
{
    char *val;

    val =  getenv(param);
    if (val != 0)
    {
        return simple_strtoul(val, NULL, base);
    }

    printf("Error: %s is not define\n",param);
    return 0; 
}


/* Convert MMC partition number to absolute offset in flash */
static int mmc_part_info(int part, unsigned int *offset,unsigned int *size)
{
    int curr_device = 0;
	struct mmc *mmc;
    block_dev_desc_t *mmc_dev;
    disk_partition_t info = {0};

    if (part <= 0)
    {
        printf("Invalid Partition number - %d\n", part);
        return STATUS_NOK;
    }

    if (get_mmc_num() > 0)
        curr_device = CONFIG_SYS_MMC_IMG_DEV;
    else {
        puts("No MMC device available\n");
        return STATUS_NOK;
    }
    
    if (part > PART_ACCESS_MASK) {
        printf("#part_num shouldn't be larger than %d\n", PART_ACCESS_MASK);
        return STATUS_NOK;
    }

	mmc = find_mmc_device(curr_device);

	if (mmc) {
		mmc_init(mmc);
        mmc_dev = mmc_get_dev(curr_device);
        if (mmc_dev != NULL)
        {
            init_part(mmc_dev);
    		if (mmc_dev->type != DEV_TYPE_UNKNOWN) {
                if (get_partition_info (mmc_dev, part, &info) == 0)
                {
                    *offset = info.start*info.blksz;
                    *size   = info.size*info.blksz;
                    return STATUS_OK;
                }
                
                printf("partition %d not found\n",part);
                return STATUS_NOK;
    		}
            printf("unknown partition type\n");
            return STATUS_NOK;
        }
		puts("get mmc type error!\n");
		return STATUS_NOK;
	} else {
		printf("no mmc device at slot %x\n", curr_device);
		return STATUS_NOK;
	}
}

/* Program image to SPI flash */
static int flash_write_spi(unsigned int src, unsigned int dest, unsigned int length, int verbose)
{
    int rc;
    unsigned long addr_first = (unsigned long) dest;
    unsigned long addr_last = addr_first + length -1;

    if (verbose == VERBOSE_ENABLE)
    {
        printf("Program to SPI from:%X to:%X len: %d\n",src, dest,length);
    }

    printf("Error: Program to SPI is not supported yet...\n",src, dest,length);
    return STATUS_NOK;

    printf("Protect off %08lX ... %08lX\n",addr_first, addr_last);
	if (flash_sect_protect (0, addr_first, addr_last))
		return STATUS_NOK;

	printf ("Erasing Flash...");
	if (flash_sect_erase (addr_first, addr_last))
		return STATUS_NOK;

	printf ("Writing to Flash... ");
	rc = flash_write((char *)src, dest, length);
	if (rc != 0) {
		flash_perror (rc);
		return STATUS_NOK;
	} else {
		printf ("done\n");
	}

	/* try to re-protect */
	(void) flash_sect_protect (1, addr_first, addr_last);
    return STATUS_OK;
}

U_BOOT_CMD(
    update, 10, 0, do_update,
    "update\t - Program Puma6 image to flash.\n",
	"[-t <all|arm|atom|uboot>] [-f <spi|mmc>] [-a <source address (hex)>] [-v] [-c] [-i] <1|2> \n"
    "'-t <all|arm|atom|uboot>'         - Source image type: 'arm', 'atom' or 'all' to unifined image (default is 'arm').\n"
    "'-f <spi|mmc>'              - Destination flash type:  'spi' or 'mmc' (defalut is according to board type).\n"
    "'-a <source address (hex)>' - Source address in RAM.\n"
    "'-v'                        - Verbose\n"
    "'-c'                        - Check crc\n"
    "'-i'                        - Force to ignore errors\n"
    "1|2                         - Destination 'Active Image' bank (must be defined as last parameter, if not uboot)\n"
);

#endif
