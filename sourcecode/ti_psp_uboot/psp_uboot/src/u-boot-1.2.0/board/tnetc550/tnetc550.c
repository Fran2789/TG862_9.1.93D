/*
 * tnetc550.c
 * Description:
 * See below.
 *
 *
 * Copyright (C) 2008, Texas Instruments, Incorporated
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
 * This file implements board specific initilization routines.
 *
 */

#include <common.h>

#ifdef CONFIG_SPI_FLASH
#include <spi.h>
#endif

#ifndef CONFIG_PUMA5_VOLCANO_EMU
#include <puma5.h>
#endif

DECLARE_GLOBAL_DATA_PTR;

#ifdef CONFIG_TI_BBU
extern void bbu_init(int printEnableParam);
#endif

static volatile unsigned int* reg_gpioout;
static int gpio_pin = -1;
static int is_env_updated = 0;


#ifndef CONFIG_PUMA5_VOLCANO_EMU
/* Reset out specified module using PSC */
int psc_enable_module (int module_id)
{
    volatile unsigned int* module_reg = (volatile unsigned int *)REG_PSCMDCTL(module_id);

    /* Set Next state as Enabled for the module */
    *module_reg = (*module_reg & 0xffffffe0) | 0x03;

    /* Enable the Power Domain Transition Command */
    *(volatile unsigned int *)(REG_PSCPTCMD) = 1;

    /* Check for Transition Complete(PTSTAT) */
    while (*(volatile unsigned int *)(REG_PSCPTSTAT) & 1);

    return(0);
}

/* Power down specified module using PSC */
int psc_disable_module (int module_id)
{
    volatile unsigned int* module_reg = (volatile unsigned int *)REG_PSCMDCTL(module_id);

    /* Set Next state as Disabled for the module */
    *module_reg = (*module_reg & 0xffffffe0) | 0x02;

    /* Enable the Power Domain Transition Command */
    *(volatile unsigned int *)(REG_PSCPTCMD) = 1;

    /* Check for Transition Complete(PTSTAT) */
    while (*(volatile unsigned int *)(REG_PSCPTSTAT) & 1);

    return(0);
}

/* Set the module state as specified */
int psc_set_module_state (int module_id, int module_state)
{
    volatile unsigned int* module_reg = (volatile unsigned int *)REG_PSCMDCTL(module_id);

    /* Set Next state as Enabled for the module */
    *module_reg = (*module_reg & 0xffffffe0) | module_state;

    /* Enable the Power Domain Transition Command */
    *(volatile unsigned int *)(REG_PSCPTCMD) = 1;

    /* Check for Transition Complete(PTSTAT) */
    while (*(volatile unsigned int *)(REG_PSCPTSTAT) & 1);

    return(0);
}

#if (CONFIG_COMMANDS & CFG_CMD_NET)

void board_ether_init (void)
{
    typedef struct
    {
        char *boardtype;
        int eth_reset_gpio_num;

    } gmii_gpio_t;

    static const gmii_gpio_t gmii_gpio[] =
    {
        { "tnetc550",         EXTPHY_RESET_TNETC550_GPIO_NUM },  /* Default */
        { "tnetc950",         EXTPHY_RESET_TNETC950_GPIO_NUM },
        { "tnetc958",         EXTPHY_RESET_TNETC958_GPIO_NUM },
        { "tnetc958ext",      EXTPHY_RESET_TNETC958_GPIO_NUM },
        { "tnetc552",         EXTPHY_RESET_TNETC552_GPIO_NUM },
        { NULL, 0 } /* Last */
    };

    char* boardtype;
    int i = 0;

    /*
     * Put AUX GPIO pins in functional mode for GMII
     * (19 to 26)
     */
    *((volatile unsigned int *)(REG_AUX_GPIOEN))  &= ~(0xff<<19);

    /* Check if Board type is provided through ENV. */
    if ((boardtype = getenv("boardtype")) == NULL)
    {
        gpio_pin = gmii_gpio[EXTPHY_RESET_DEFAULT_INDEX].eth_reset_gpio_num;
        boardtype = gmii_gpio[EXTPHY_RESET_DEFAULT_INDEX].boardtype;

#ifndef CFG_NO_FLASH
        printf ("WARN: boardtype variable was %s, using default (%s)\n", "not set", gmii_gpio[EXTPHY_RESET_DEFAULT_INDEX].boardtype );
#endif

        is_env_updated = 1;
    }
    else
    {
        while( gmii_gpio[i].boardtype != NULL )
        {
            if( strcmp( boardtype, gmii_gpio[i].boardtype ) == 0 )
            {
                gpio_pin = gmii_gpio[i].eth_reset_gpio_num;
                break;
            }
            i++;
        }

        if(gpio_pin == -1)
        {
            gpio_pin = gmii_gpio[EXTPHY_RESET_DEFAULT_INDEX].eth_reset_gpio_num;
            boardtype = gmii_gpio[EXTPHY_RESET_DEFAULT_INDEX].boardtype;
            printf ("WARN: boardtype variable was %s, using default (%s)\n", "set incorrectly", gmii_gpio[EXTPHY_RESET_DEFAULT_INDEX].boardtype );

            is_env_updated = 1;
        }
    }

    /* for tnetc958, power up clockout0 */
    if( (gpio_pin == EXTPHY_RESET_TNETC958_GPIO_NUM) && ( strcmp( boardtype, "tnetc958" ) == 0 ) ) 
    {
        /* Toggle clock0 power control */
        *((volatile unsigned int *)(REG_IO_PDCR)) &= ~CLK_OUT0_PWRDN;
    }

    if(is_env_updated != 0)
    {
        /* Setup default boardtype */
        setenv("boardtype", boardtype);
    }


    /*
     * Set specified GPIO (default GPIO14) in out mode for PHY reset control
     */


    if (gpio_pin > 31)
    {
        //AUX
        gpio_pin -= 32;
        *((volatile unsigned int *)(REG_AUX_GPIOEN))  |= (1<<gpio_pin);     /* Enable */
        *((volatile unsigned int *)(REG_AUX_GPIODIR))  &= ~(1<<gpio_pin);   /* Out */
        reg_gpioout = (volatile unsigned int *)REG_AUX_GPIOOUT;
    }
    else
    {
        *((volatile unsigned int *)(REG_GPIOEN))  |= (1<<gpio_pin);     /* Enable */
        *((volatile unsigned int *)(REG_GPIODIR))  &= ~(1<<gpio_pin);   /* Out */
        reg_gpioout = (volatile unsigned int *)REG_GPIOOUT;
    }

    {
        int ext_switch_reset_gpio = -1;
        char * tmp;

        if (NULL != (tmp = getenv("ext_switch_reset_gpio")))
        {
            if ( 64 > (ext_switch_reset_gpio=simple_strtoul(tmp, NULL, 0)) )
            {
                if (ext_switch_reset_gpio > 31)
                {
                    //AUX
                    ext_switch_reset_gpio -= 32;
                    *((volatile unsigned int *)(REG_AUX_GPIOEN))   |=  (1<<ext_switch_reset_gpio);  /* Enable */
                    *((volatile unsigned int *)(REG_AUX_GPIODIR))  &= ~(1<<ext_switch_reset_gpio);  /* Out */
                    *((volatile unsigned int *)(REG_AUX_GPIOOUT))  |=  (1<<ext_switch_reset_gpio);  /* Take it out of reset */
                }
                else
                {
                    *((volatile unsigned int *)(REG_GPIOEN))   |=  (1<<ext_switch_reset_gpio);  /* Enable */
                    *((volatile unsigned int *)(REG_GPIODIR))  &= ~(1<<ext_switch_reset_gpio);  /* Out */
                    *((volatile unsigned int *)(REG_GPIOOUT))  |=  (1<<ext_switch_reset_gpio);  /* Take it out of reset */
                }
            }
        }
    }

    /* Set for GigEth */
    //    *(volatile unsigned int *)(REG_ETH_CR) |= (ETHCR_GIG_ENABLE | ETHCR_FD_ENABLE);
}

void reset_ether_phy (int fReset)
{
    /*
     * TODO : PSC control calls are put inside this function to isolate ethernet
     * driver from specifics, need to find proper location.
     */
    if (fReset)
    {
        /*
           * PHY in reset
           */
        *reg_gpioout     &= ~(1<<gpio_pin);

        udelay (30);    /* Minimum 20us delay */

        psc_disable_module (LPSC_CPGMAC);
        psc_disable_module (LPSC_MDIO);
    }
    else
    {
        psc_enable_module (LPSC_MDIO);
        psc_enable_module (LPSC_CPGMAC);

        /*
         * PHY out of  reset
         */
        *reg_gpioout     |= (1<<gpio_pin);
        udelay (5000);
    }
}
#endif /* CFG_CMD_NET */

/* Set watchdog disable register to specified state */
int set_watchdog_disable_reg (int state)
{
    int tmo;
    volatile unsigned int* disable_lock_reg = (volatile unsigned int *)REG_WT_DISABLE_LOCK;
    volatile unsigned int* disable_reg = (volatile unsigned int *)REG_WT_DISABLE;

    /* Unlock stage 1 */
    *disable_lock_reg = WT_DISABLE_UNLOCK_WORD1;
    for (tmo = 0xff; tmo > 0; tmo--) {
        if ((*disable_lock_reg & 3) == 1)
            break;
    }

    /* Unlock stage 2 */
    *disable_lock_reg = WT_DISABLE_UNLOCK_WORD2;
    for (tmo = 0xff; tmo > 0; tmo--) {
        if ((*disable_lock_reg & 3) == 2)
            break;
    }

    /* Unlock stage 3 */
    *disable_lock_reg = WT_DISABLE_UNLOCK_WORD3;
    for (tmo = 0xff; tmo > 0; tmo--) {
        if ((*disable_lock_reg & 3) == 3)
            break;
    }

    /* Set disable reg  */
    *disable_reg = (*disable_reg & ~1) | (state & 1);

    return(0);
}

#endif /* !CONFIG_PUMA5_VOLCANO_EMU */

/*
 * Miscellaneous platform dependent initialisations
 */

int board_init (void)
{
    gd->bd->bi_arch_number = MACH_TYPE_PUMA5;
    /* adress of boot parameters */
    gd->bd->bi_boot_params = PHYS_SDRAM_BOOT_PARAMS;


#define PRCR_REG                        (*(volatile unsigned *)(0x08611600))
#ifdef CONFIG_PUMA5_VOLCANO_EMU
#define AVALANCHE_UART0_RESET_BIT       0
#else
#define AVALANCHE_UART0_RESET_BIT       1
#endif

#define SIO0__RSTMASK                   (1<<(AVALANCHE_UART0_RESET_BIT))

    PRCR_REG|=SIO0__RSTMASK;

#ifndef CONFIG_PUMA5_VOLCANO_EMU
    psc_enable_module (LPSC_TIMER0);
    psc_enable_module (LPSC_UART0);
    set_watchdog_disable_reg (WT_STATE_DISABLED);
    psc_enable_module (LPSC_UART1);
    psc_enable_module (LPSC_CPPI41);
    psc_enable_module (LPSC_GPIO);

    /*
     * GPIO0 as Output to turn On powe LED 
     */
    *((volatile unsigned int *)(REG_GPIOEN))   |= 1;         
    *((volatile unsigned int *)(REG_GPIODIR))  &= ~(1<<0);   /* Out */

#endif /* !CONFIG_PUMA5_VOLCANO_EMU */

    icache_enable ();

#ifdef CONFIG_SPI_FLASH
        /* Initilize SPI till this point we dont access SPI BUS :-) */
    spi_init();
#endif

#ifdef CONFIG_PUMA5_VOLCANO_EMU
        extern void DispStr(char *val);
        DispStr ("<U-Boot>");

        /* Disable gigabit capabilities on Lava board PHYs */
        //lava_nogig ();
#endif
#ifdef CONFIG_TI_BBU
    /* Initialize BBU */
	bbu_init(0);
#endif

#ifdef CONFIG_EXTENDED_FLASH
    /* 
     * For Monolithic Flash Reset support, we need to pull down GPIO#6.
     * On Reset this GPIO will be pull-up by HW, causing to the FLASH to get reset 
     */
    *((volatile unsigned int *)(REG_GPIOEN))  |= (1<<6);    /* enable PIN mode (not FUNCTIONAL mode)*/
    *((volatile unsigned int *)(REG_GPIODIR)) &= ~(1<<6);   /* Set to 'output' direction */
    *((volatile unsigned int *)(REG_GPIOOUT)) &= ~(1<<6);   /* set to zero - pull to low */ 
#endif

    return 0;
}

/******************************
 Routine:
 Description:
******************************/
int dram_init (void)
{
    gd->bd->bi_dram[0].start = PHYS_SDRAM_1;
    gd->bd->bi_dram[0].size = *(unsigned int *)PHYS_SDRAM_1;
    return 0;
}

#include <command.h>
#ifdef CONFIG_EXTENDED_FLASH
#include <serial_flash.h>
#endif
static unsigned int op_to_data (char* s)
{
    unsigned int data, *ptr;

    /* if the parameter starts with a * then assume is a pointer to the value we want */
    if (s[0] == '*') {
        ptr = (unsigned int *)simple_strtoul(&s[1], NULL, 16);
#ifdef CONFIG_EXTENDED_FLASH
        ulong proc_len;
        int switch_res = process_flash_bank(NULL, ptr, sizeof(*ptr), &proc_len, 1);    
        if (switch_res == EMM_HIGH)
        {
            ptr = (unsigned int*) ((char*) ptr - FLASH_BORDER_OFFSET_16MB);
        }
        else if (switch_res == EMM_LOW)
        {
            if (proc_len != sizeof(*ptr))
                printf("Warning %s %d not implemented\n", __FUNCTION__, __LINE__); 
        }
#endif
        data = *ptr;
    } else {
        data = simple_strtoul(s, NULL, 16);
    }

    return data;
}

/* command line interface to for addition and subtraction */
int do_eval (cmd_tbl_t *cmdtp, int flag, int argc, char *argv[])
{
#define EVAL_OPS        2
#define EVAL_OP_ADD     0
#define EVAL_OP_SUB     1

    int     rc = 1, value = 0, len, i;
    unsigned int op1, op2;
    char*   op_tbl[EVAL_OPS] = {"+", "-"};
    char evalval[12];

    /* Validate arguments */
    if ((argc < 4)){
            printf("Usage:\n%s\n", cmdtp->usage);
            return rc;
    }

    len = strlen (argv[2]);

    for (i = 0; i < EVAL_OPS; i++) {
        if ((strncmp (argv[2], op_tbl[i], len) == 0) && (len == strlen (op_tbl[i]))) {
        op1 = op_to_data (argv[1]);
        op2 = op_to_data (argv[3]);

            switch (i) {
                case EVAL_OP_ADD:
                    value = op1 + op2;
                    rc = 0;
                    break;
                case EVAL_OP_SUB:
                    value = (int) (op1 - op2);
                    rc = 0;
                    break;
                default:
                    printf("Unsupported operation '%s'\n", argv[2]);
                    break;
            }
            break;
        }
        else {
#if 0
            printf ("arg: %s, len: %d\n", argv[2], len);
            printf ("op: %s, len: %d\n", op_tbl[i],  strlen (op_tbl[i]));
#endif
        }
    }

    if (i >= EVAL_OPS)
        printf("Unknown/Unsupported operator '%s'\n", argv[2]);
    else if (rc == 0) {
        sprintf (evalval, "0x%x", value);
        setenv("evalval", evalval);
    }

    return rc;
}

U_BOOT_CMD(
    eval, CFG_MAXARGS, 0, do_eval,
    "eval\t- return addition/subraction\n",
    "[*]value1 <op> [*]value2\n"
);

static int generate_env_vals (void)
{
    int ret = 0;
#ifndef CFG_NO_FLASH
    char* s;
    char hex_str[8+2+1+8/*safe*/];

    if (((s = getenv ("ubootpartsize")) == NULL)
            || (simple_strtoul (s, NULL, 16) == 0))
    {
        unsigned int ubootpartsize = CFG_UBOOT_SECT_SIZE;
        sprintf (hex_str, "0x%x", ubootpartsize);
        setenv ("ubootpartsize", hex_str);
        ret = 1;
    }

    if (((s = getenv ("envpartsize")) == NULL)
            || (simple_strtoul (s, NULL, 16) == 0))
    {
        unsigned int envpartsize = CFG_ENV_SECT_SIZE;
        sprintf (hex_str, "0x%x", envpartsize);
        setenv ("envpartsize", hex_str);
        ret = 1;
    }

    if (((s = getenv ("UBFIADDR1")) == NULL)
            || (simple_strtoul (s, NULL, 16) == 0))
    {
        unsigned int ubfiaddr1 = CFG_FLASH1_BASE + CFG_UBOOT_SECT_SIZE
                                    + (CFG_ENV_SECT_SIZE*2);    
        sprintf (hex_str, "0x%x", ubfiaddr1);
        setenv ("UBFIADDR1", hex_str);
        ret = 1;
    }

    if (((s = getenv ("UBFIADDR2")) == NULL)
            || (simple_strtoul (s, NULL, 16) == 0))
    {
        if( DETECTED_SFL_DEVICES > 1 )
        {
            /* On dual flash platform, the second partition starts in the base address of 2nd device */
            sprintf (hex_str, "0x%x", CFG_FLASH2_BASE);
        }
        else
        {
            extern flash_info_t flash_info[];
            unsigned int ubfiaddr1 = CFG_FLASH1_BASE + CFG_UBOOT_SECT_SIZE
                                    + (CFG_ENV_SECT_SIZE*2);  
            unsigned int ubfiaddr2;
#ifdef CONFIG_EXTENDED_FLASH
            /* Check for Extend Flash (larger than 16MB) */
            if (flash_info[0].size > (16*1024*1024))
            {
                /* Set the UBFI2 start address to be exactlly in the middle of the flash.
                   Which is the start offset of the second bank */
                ubfiaddr2 = CFG_FLASH1_BASE + (flash_info[0].size >> 1);
            }else
#endif
            if (CFG_UBFI_SIZE)
                ubfiaddr2 = ubfiaddr1 + CFG_UBFI_SIZE;
            else
            {
                /* Automatic calculation of UBFI size, and UBFI2 start address */
            unsigned int used_space = CFG_UBOOT_SECT_SIZE + (CFG_ENV_SECT_SIZE*2)    
                                            + ((CFG_FLASH_SECT_RESRV[0])*(CFG_FLASH_SECT_SIZE[0]));
                unsigned int ubfisize = (flash_info[0].size - used_space) >> 1;
                ubfisize -= (ubfisize & ( CFG_FLASH_SECT_SIZE[0] - 1));
                ubfiaddr2 = ubfiaddr1 + ubfisize;
            }

            sprintf (hex_str, "0x%x", ubfiaddr2);
        }
        setenv ("UBFIADDR2", hex_str);
        ret = 1;
    }
#else
    /* These variables are not relevant in flashless mode */
    setenv ("ubootpartsize", "0x0");
    setenv ("envpartsize", "0x0");
    setenv ("UBFIADDR1", "0x0");
    setenv ("UBFIADDR2", "0x0");

    ret = 1;
#endif
    return ret;
}


int board_late_init (void)
{
#if (CONFIG_COMMANDS & CFG_CMD_NET)
    board_ether_init ();
#endif

    if((generate_env_vals()) || (is_env_updated))
    {
#ifndef CFG_NO_FLASH
        saveenv ();
#endif
    }

    return 0;
}

#if 0
/******************************************************
 Routine: DispStr
 Description: Display specified string on ASCII display
*******************************************************/
void DispStr(char *val)
{
  int i;
  char *led_ptr;

  led_ptr=(char *)ASCII_DISP_BASE;
  for(i=0;i<8;i++)
  {
    if (*val)
      *led_ptr=*val++;
     else
      *led_ptr=' ';
    led_ptr+=ASCII_DISP_OFFSET;
  }
}
#endif /* CONFIG_PUMA5_VOLCANO_EMU */

