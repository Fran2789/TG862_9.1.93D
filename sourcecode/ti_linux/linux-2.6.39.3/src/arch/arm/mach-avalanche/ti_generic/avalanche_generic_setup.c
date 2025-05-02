/*
 *
 * avalanche_generic_setup.c 
 * Description:
 * avalanche generic initialization
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
 *
 */
#include <linux/platform_device.h>
#include <asm/arch/update_tag.h>
#include <linux/init.h>
#include <linux/sched.h>
#include <linux/ioport.h>
#include <asm/mach/time.h>

#include <asm/cpu.h>
#include <asm/irq.h>
#include <asm/serial.h>
#include <asm/mach-types.h>

#include <linux/proc_fs.h>

#include <asm/uaccess.h>

#include <asm-arm/arch-avalanche/generic/pal.h>
#include <asm-arm/arch-avalanche/generic/release.h>

#ifdef CONFIG_SERIAL_8250
#include <linux/tty.h>
#include <linux/kgdb.h>
#include <linux/serial.h>
#include <linux/serial_core.h>
extern int early_serial_setup(struct uart_port *port);
#endif /* CONFIG_SERIAL_8250 */

#ifdef CONFIG_ARM_AVALANCHE_VLYNQ
int __init vlynq_bus_init(void);
#endif

extern void avalanche_soc_platform_init(void);
extern unsigned int cpu_freq;
extern void avalanche_proc_entries(void);

extern unsigned long long notrace avalanche_clock_vp(void);
static unsigned long long notrace sched_clock_p(void);
static unsigned long long notrace sched_clock_org(void);
static unsigned long long notrace sched_clock_ctr(void);

/* For Precise clock */
extern UINT32 sc_count_nsec;

/* For VeryPrecise clock */
extern unsigned long long sc_calibrate_jf;

#define DEFAULT_SC sched_clock_org
static unsigned long long (*selected_sc)(void) = DEFAULT_SC;
static unsigned long long (*selected_sc_ctr)(void) = DEFAULT_SC;

/* List of all sched_clock variants */
static unsigned long long (*sched_clock_variant[])(void) = { sched_clock_org, sched_clock_p, avalanche_clock_vp };
/* Description, Keep same order as sc[] above */
static const char *sc_desc[] = { "ORG", "PRECISE", "VERY_PRECISE", "COUNTER" };

static unsigned n_sc;

static int clock_p_resolution_mod = 0;
static int clock_p_resolution_bits = 0;

/* Start with 2.5 mSec */
static unsigned long long timer_01_required_phase = 2500000;

void avalanche_start_timer1_sync(unsigned int loadNs)
{
    unsigned long long timer0_phase;
    unsigned long long timer1_phase;
	unsigned long flags;
    unsigned long t1_count_nsec;

    local_irq_save(flags);  

    t1_count_nsec = PAL_sysTimer16GetNSecPerCount(AVALANCHE_TIMER1_BASE, loadNs);

    PAL_sysTimer16Ctrl(AVALANCHE_TIMER1_BASE, TIMER16_CTRL_STOP);
    /* Wait till timer0 is at the required offset */
    do
    {
        timer0_phase = PAL_sysTimer16GetNsAfterTick(AVALANCHE_TIMER0_BASE, sc_count_nsec);
    } while (abs64(timer0_phase - timer_01_required_phase) > sc_count_nsec); /* Exit close to timer_01_required_phase, there is a whole tick to achieve this */
    /* Now timer0 precedes timer1 by ~ timer_01_required_phase */
    /* Now start timer1 */
    PAL_sysTimer16Ctrl(AVALANCHE_TIMER1_BASE, TIMER16_CTRL_START);

    /* Get the curr vals for logging */
    timer0_phase = PAL_sysTimer16GetNsAfterTick(AVALANCHE_TIMER0_BASE, sc_count_nsec);
    timer1_phase = PAL_sysTimer16GetNsAfterTick(AVALANCHE_TIMER1_BASE, t1_count_nsec);

    local_irq_restore(flags);

    printk("Sync start timer1 (%llu) to timer0 (%llu) diff %llu required %llu\n",
           timer1_phase, timer0_phase, timer0_phase - timer1_phase, timer_01_required_phase);
}
EXPORT_SYMBOL(avalanche_start_timer1_sync);

/**************************************************************************/
/*! \fn static unsigned long long notrace sched_clock_p(void)
 **************************************************************************
 *  \brief Precise sched clock
 *  \return nSec
 **************************************************************************/
static unsigned long long notrace sched_clock_p(void) 
{
    unsigned long long add_ns;
    unsigned long long ns;

    add_ns = PAL_sysTimer16GetNsAfterTick(AVALANCHE_TIMER0_BASE, sc_count_nsec);
    if (clock_p_resolution_bits > 0)
    {
        /* Zero out the resolution bits */
        add_ns >>= clock_p_resolution_bits; 
        add_ns <<= clock_p_resolution_bits;
    }
    else if (clock_p_resolution_mod > 0)
    {
        unsigned long long tmp_ns = add_ns;
        unsigned long remainder;

        /* Effecively, do a modulo */
        /* Get remainder */
        remainder = do_div(tmp_ns, clock_p_resolution_mod);
        /* Subtract from add_ns (disregard rounding) */
        add_ns -= remainder;
    }
    ns = (unsigned long long)(jiffies - INITIAL_JIFFIES) * (NSEC_PER_SEC / HZ) +  add_ns; 

    return ns;  
}

/**************************************************************************/
/*! \fn static unsigned long long notrace sched_clock_p(void)
 **************************************************************************
 *  \brief Precise sched clock
 *  \return nSec
 **************************************************************************/
unsigned long long notrace avalanche_clock_p(void) 
{
    unsigned long long add_ns = PAL_sysTimer16GetNsAfterTick(AVALANCHE_TIMER0_BASE, sc_count_nsec);
    unsigned long long ns;

    ns = (unsigned long long) (jiffies - INITIAL_JIFFIES) * (NSEC_PER_SEC / HZ) +  add_ns; 

    return ns;  
}
EXPORT_SYMBOL(avalanche_clock_p);

/**************************************************************************/
/*! \fn static unsigned long long notrace sched_clock_org(void)
 **************************************************************************
 *  \brief Original (vanilla) sched clock
 *  \return nSec
 **************************************************************************/
static unsigned long long notrace sched_clock_org(void)
{
    unsigned long long ns;

    ns = (unsigned long long) (jiffies - INITIAL_JIFFIES) * (NSEC_PER_SEC / HZ); 

    return ns;  
}

/**************************************************************************/
/*! \fn static unsigned long long notrace sched_clock(void)
 **************************************************************************
 *  \brief sched clock
 *  \return nSec
 **************************************************************************/
unsigned long long notrace sched_clock(void)
{
    /* Use the selected sched_clock */
    return selected_sc();
}

/**************************************************************************/
/*! \fn static unsigned long long notrace sched_clock_ctr(void)
 **************************************************************************
 *  \brief sched clock with counter
 *  \return nSec
 **************************************************************************/
unsigned long long notrace sched_clock_ctr(void)
{
    n_sc++;

    /* Use the selected sched_clock */
    return selected_sc_ctr();
}

/**************************************************************************/
/*! \fn static const char *sc_curr_desc(void)
 **************************************************************************
 *  \brief Get description of currect schec clock
 *  \return String with description
 **************************************************************************/
static const char* sc_curr_desc(void) 
{
    int i;
    const char *desc = "unknown";
    unsigned long long (*compare_ptr)(void);
    static char str[22];

    if (selected_sc == sched_clock_ctr)
    {
        compare_ptr = selected_sc_ctr;
    }
    else
    {
        compare_ptr = selected_sc;
    }

    for (i = 0; (i < ARRAY_SIZE(sched_clock_variant)) && (sched_clock_variant[i] != compare_ptr); i++) 
        ;
    if (i < ARRAY_SIZE(sched_clock_variant))
    {
        snprintf(str, sizeof(str), "%s%s", (selected_sc == sched_clock_ctr) ? "Counter " : "", sc_desc[i]);
        desc = str;
    }

    return desc;
}

/**************************************************************************/
/*! \fn static int avalanche_p_sched_clock_test_get(char* buf, char **start, off_t offset, int count, int *eof, void *data)
 **************************************************************************
 *  \brief Read proc of sched_clock_test
 *  \param[in,out] Read proc params
 *  \return len
 **************************************************************************/
static int avalanche_p_sched_clock_test_get(char *buf, char **start, off_t offset, int count, int *eof, void *data) 
{
    int len=0;
    unsigned long long jf = (unsigned long long)(jiffies - INITIAL_JIFFIES) * (NSEC_PER_SEC / HZ);
    unsigned long long sc = sched_clock();

    len += sprintf(buf+len, "jiffies %ld, jif_ns %lld, sched_clock %lld, (sc-jf) %lld\n", jiffies, jf, sc, sc - jf);

    *eof = 1;
    return len;
}

/**************************************************************************/
/*! \fn static void avalanche_p_sched_clock_test_usage(void)
 **************************************************************************
 *  \brief Usage for proc sched_clock_test
 *  \return n/a
 **************************************************************************/
static void avalanche_p_sched_clock_test_usage(void) 
{
    printk("echo <x> > /proc/avalanche/sched_clock\n"
           "\t<x>:\n"
           "\t\thelp - print help\n"
           "\t\tstats - Statistics of sched_clock\n"
           "\t\texectime - Measure execution time of sched_clock\n"
           "\t\tget - Show time measured by curr sched_clock\n"
           "\t\tset_org - Select native (vanilla) sched_clock\n"
           "\t\tset_precise - Select precise sched_clock\n"
           "\t\tset_very_precise - Select vary precise sched_clock\n"
           "\t\ten_counter - Add event counter\n"
           "\t\tdis_counter - Remove event counter\n"
           "\t\tresolution <num> - Resolution control of precise timer\n"
           "\t\t                    0 --> Max resolution\n"
           "\t\t                    Negative --> Number of LSBits to 0 (-1..-23), -20 --> ~1mSec\n"
           "\t\t                    Positive --> Resolution (1..10000000 nSec)\n"
           "\t\ttimer01_phase <nanoSec> - Phase [nSec] between timer0 (tick) and timer1 (mxp)\n"
           "\t\tAnything else - illegal\n"
           "\tcat /proc/avalanche/sched_clock\n"
           "\t\tSample sched_clock\n"
           );
}

/**************************************************************************/
/*! \fn static int avalanche_p_sched_clock_test(struct file *fp, const char * buf, unsigned long count, void * data)
 **************************************************************************
 *  \brief Write proc of sched_clock_test
 *  \param[in,out] Write proc params
 *  \return count
 **************************************************************************/
static int avalanche_p_sched_clock_test(struct file *fp, const char * buf, unsigned long count, void * data)
{
    unsigned long flags;
    char inp[22];
    unsigned long long start_ns;
    unsigned long long end_ns;

    if (count > sizeof(inp))
    {
        printk("%s: ERR - too much input - %lu of %d\n", __func__, count, sizeof(inp));
        avalanche_p_sched_clock_test_usage();
        return -EFAULT;
    }

    if (copy_from_user(inp, buf, count) != 0)
    {
        return -EFAULT;
    }
    inp[count - 1] = '\0';
    
    if (strcmp(inp, "help") == 0)
    {
        avalanche_p_sched_clock_test_usage();
    }
    else if (strcmp(inp, "en_counter") == 0)
    {
        if (selected_sc != sched_clock_ctr)
        {
            /* Use curr sc in sched_clock_ctr */
            selected_sc_ctr = selected_sc;
            /* Use sched_clock_ctr */
            selected_sc = sched_clock_ctr;
        }
        else
        {
            printk("\nAlready using counter with %s\n", sc_curr_desc());
        }
    }
    else if (strcmp(inp, "dis_counter") == 0)
    {
        if (selected_sc == sched_clock_ctr)
        {
            selected_sc = selected_sc_ctr;
        }
        else
        {
            printk("\nNot using counter\n");
        }
    }
    else if (strcmp(inp, "stats") == 0)
    {
        printk("\ntot - %u\n", n_sc);
    }
    else if (strcmp(inp, "get") == 0)
    {
        int i;
        unsigned long long res[7];

        printk("\nCurr - %s - 7 reads 2mSec apart\n", sc_curr_desc());
        if (selected_sc == sched_clock_p)
        {
            printk("Precise sched_clock: load %d, nsec/count %d, ",
                   PAL_sysTimer16GetLoad(AVALANCHE_TIMER0_BASE), sc_count_nsec);
            if ((clock_p_resolution_bits == 0) && (clock_p_resolution_mod == 0))
            {
                printk("resolution - MAX\n");
            }
            else if (clock_p_resolution_bits > 0)
            {
                printk("resolution %d bits == %d nSec\n", 
                       clock_p_resolution_bits, 1 << clock_p_resolution_bits);
            }
            else if (clock_p_resolution_mod > 0)
            {
                printk("resolution %d nSec\n", clock_p_resolution_mod);
            }
            else
            {
                printk("illegal resolution values: clock_p_resolution_bits %d, clock_p_resolution_mod %d\n", 
                       clock_p_resolution_bits, clock_p_resolution_mod);
            }
        }
        for (i = 0; i < ARRAY_SIZE(res); i++) 
        {
            res[i] = sched_clock();
            udelay(2000);
        }
        for (i = 0; i < ARRAY_SIZE(res); i++) 
        {
            printk("%d: %lld (0x%llX)\n", i, res[i], res[i]);
        }
    }
    else if (strcmp(inp, "set_org") == 0)
    {
        selected_sc = sched_clock_org;
    }
    else if (strcmp(inp, "set_precise") == 0)
    {
        selected_sc = sched_clock_p;
    }
    else if (strcmp(inp, "set_very_precise") == 0)
    {
        selected_sc = avalanche_clock_vp;
    }
    else if (strcmp(inp, "exectime") == 0)
    {
        int i;

        printk("\n");
        for (i = 0; i < ARRAY_SIZE(sched_clock_variant); i++)
        {
            int runnum;

            local_irq_save(flags); 
            /* Save curr nSec, ignore overhead of 2*sched_clock_vp, negligible with 1024 executions */
            start_ns = avalanche_clock_vp();
            for (runnum = 0; runnum < 1024; runnum++) 
            {
                sched_clock_variant[i]();
            }
            end_ns = avalanche_clock_vp();
            local_irq_restore(flags);
            printk("sched_clock_%s() duration - %lld nSec (%lld nSec for 1024 reads)\n", sc_desc[i], (end_ns - start_ns) >> 10, end_ns - start_ns);
        }
    }
    else if (strncmp(inp, "resolution", strlen("resolution")) == 0)
    {
        int __attribute__ ((unused)) kstrtoint_retval;
        int getnum;

        kstrtoint_retval = kstrtoint(&inp[strlen("resolution") + 1], 0, &getnum);
        if ((getnum < 0) && (getnum >= -24))
        {
            clock_p_resolution_mod = 0;
            clock_p_resolution_bits = -getnum;
            printk("resolution %d bits == %d nSec\n", 
                   clock_p_resolution_bits, 1 << clock_p_resolution_bits);
        }
        else if ((getnum > 0) && (getnum <= (10 * NSEC_PER_MSEC)))
        {
            clock_p_resolution_mod = getnum;
            clock_p_resolution_bits = 0;
            printk("resolution %d nSec\n", clock_p_resolution_mod);
        }
        else if (getnum == 0)
        {
            clock_p_resolution_mod = 0;
            clock_p_resolution_bits = 0;
            printk("resolution - MAX\n");
        }
        else
        {
            clock_p_resolution_mod = 0;
            clock_p_resolution_bits = 0;
            printk("Illegal resolution values: getnum %d --> Setting to MAX\n", getnum);
        }
    }
    else if (strncmp(inp, "timer01_phase", strlen("timer01_phase")) == 0)
    {
        int __attribute__ ((unused)) kstrtoint_retval;
        int getnum;

        kstrtoint_retval = kstrtoint(&inp[strlen("timer01_phase") + 1], 0, &getnum);
        if (kstrtoint_retval == 0)
        {
            timer_01_required_phase = getnum; 
        }
        else
        {
            printk("timer01_phase %llu\n", timer_01_required_phase);
        }
    }
    else
    {
        avalanche_p_sched_clock_test_usage();
    }

    return count;
}

/**************************************************************************/
/*! \fn static int avalanche_p_sched_clock_calibrate_get(char* buf, char **start, off_t offset, int count, int *eof, void *data)
 **************************************************************************
 *  \brief Read proc of sched_clock_calibrate
 *  \param[in,out] Read proc params
 *  \return len
 **************************************************************************/
static int avalanche_p_sched_clock_calibrate_get(char* buf, char **start, off_t offset, int count, int *eof, void *data)
{
    int len=0;
    unsigned long long jf = (unsigned long long)(jiffies - INITIAL_JIFFIES) * (NSEC_PER_SEC / HZ);
    unsigned long long sc = sched_clock();

    len += sprintf(buf+len, "cal %llu, jiffies %ld, jif_ns %lld, sched_clock %lld, (sc-jf) %lld\n", sc_calibrate_jf, jiffies, jf, sc, sc - jf);

    *eof = 1;
    return len;
}

/**************************************************************************/
/*! \fn static void avalanche_p_sched_clock_calibrate_usage(void)
 **************************************************************************
 *  \brief Usage for proc sched_clock_calibrate
 *  \return n/a
 **************************************************************************/
static void avalanche_p_sched_clock_calibrate_usage(void)
{
    printk("echo <x> > /proc/avalanche/sched_clock_calibrate\n"
           "\t<x>:\n"
           "\t\t0 - Uncalibrate --> calibration == 0\n"
           "\t\t1 - Calibrate\n"
           "\t\thelp - print help\n"
           "\t\t<number> - Set calibration value\n"
           "\t\tAnything else - illegal\n"
           "cat /proc/avalanche/sched_clock_calibrate\n"
           "\tSample sched_clock\n"
           );
}

/**************************************************************************/
/*! \fn static int avalanche_p_sched_clock_calibrate(struct file *fp, const char * buf, unsigned long count, void * data)
 **************************************************************************
 *  \brief Write proc of sched_clock_calibrate
 *  \param[in,out] Write proc params
 *  \return count
 **************************************************************************/
static int avalanche_p_sched_clock_calibrate(struct file *fp, const char * buf, unsigned long count, void * data)
{
    unsigned long long jf_ns;
    unsigned long long sc;
    unsigned long long prev_cal = sc_calibrate_jf;
    unsigned long flags;
    typeof(jiffies) jf;
    typeof(sc_calibrate_jf) tmp_scc;
    char inp[22];
    int __attribute__ ((unused)) kstrtouint_retval;
    unsigned int cal;

    if (count > sizeof(inp))
    {
        printk("%s: ERR - too much input - %lu of %d\n", __func__, count, sizeof(inp));
        avalanche_p_sched_clock_calibrate_usage();
        return -EFAULT;
    }

    if (copy_from_user(inp, buf, count) != 0)
    {
        return -EFAULT;
    }
    inp[count - 1] = '\0';
    
    if (strcmp(inp, "help") == 0)
    {
        avalanche_p_sched_clock_calibrate_usage();
        return count;
    }

    if (strlen(inp) == 1)
    {
        if (inp[0] == '0')
        {
            sc_calibrate_jf = 0;
            printk("Uncalibrated sched_clock\n");
            return count;
        }
        else if (inp[0] == '1')
        {
            /* else - calibrate */
            sc_calibrate_jf = 0;
            jf = jiffies;
            /* Wait till the timer interrupt -- assume we will get enough cpu ... */
            while (jf == jiffies);
            local_irq_save(flags);
            /* Hoepfully we are immediately after a clock tick */
            sc = sched_clock();
            jf_ns = (unsigned long long)(jiffies - INITIAL_JIFFIES) * (NSEC_PER_SEC / HZ);
            if (sc >= jf_ns)
            {
                sc_calibrate_jf = sc - jf_ns;
                tmp_scc = sc_calibrate_jf;
                sc_calibrate_jf -= do_div(tmp_scc, (NSEC_PER_SEC / HZ));
                local_irq_restore(flags);
                printk("Calibrate: prev cal %llu, new cal %llu\n", prev_cal, sc_calibrate_jf);
            }
            else
            {
                /* Should not happen */
                local_irq_restore(flags);
                printk("Could not calibrate: sc %llu, jiffies %lu, prev cal %llu\n", sc, jf, prev_cal);
            }
            return count;
        }
    }

    /* Set calib val */
    kstrtouint_retval = kstrtouint(inp, 0, &cal);
    if (kstrtouint_retval == 0)
    {
        sc_calibrate_jf = cal;
        printk("Force Calibrate: prev cal %llu, new cal %llu\n", prev_cal, sc_calibrate_jf);
        return count;
    }
    else
    {
        printk("%s: ERR - illegal input \"%s\"\n", __func__, inp);
        avalanche_p_sched_clock_calibrate_usage();
        return -EFAULT;
    }

    return count;
}


static int avalanche_p_read_base_psp_version(char* buf, char **start, off_t offset, 
                                             int count, int *eof, void *data)
{
    int len   = 0;
    int limit = count - 80;
    char *cache_mode[4] = {"cached, write through", \
                           "cached, write back", \
                           "uncached"};


    int cache_index = 1; /* default is write back */

	/* write through mode */
	#if defined(CONFIG_CPU_DCACHE_WRITETHROUGH)
		cache_index = 0;
	#endif

	/* uncached mode */
	#if defined(CONFIG_CPU_DCACHE_DISABLE) && defined(CONFIG_CPU_ICACHE_DISABLE)
		cache_index = 2;
	#endif

 

    if(len<=limit)
        len+= sprintf(buf+len, "\nLinux OS version %s\n"\
			       "Avalanche SOC Version: 0x%x operating in %s mode\n"\
			       "Cpu Frequency: %u MHZ\nSystem Bus frequency: %u MHZ\n\n", 
					PSP_RELEASE_TYPE, 
					avalanche_get_chip_version_info(), cache_mode[cache_index],
					cpu_freq/1000000, 
#if defined (CONFIG_MACH_PUMA5)
                    2*avalanche_get_vbus_freq()/1000000);
#else    /* CONFIG_MACH_PUMA6  For Puma-6 SoC */
                    PAL_sysClkcGetFreq(PAL_SYS_CLKC_SSX)/1000000);
#endif

    
    return (len);
}

int avalanche_proc_init(void)
{
    struct proc_dir_entry *avalanche_proc_root;

    avalanche_proc_root = proc_mkdir("avalanche",NULL);
    if(!avalanche_proc_root)
        return -ENOMEM;

    create_proc_read_entry("avalanche/base_psp_version", 0, NULL, avalanche_p_read_base_psp_version, NULL);

    /* For VeryPrecise clock */
    {
        struct proc_dir_entry *av_timer_proc = NULL;
        av_timer_proc = create_proc_entry("sched_clock_calibrate", 0644, avalanche_proc_root);
        if (av_timer_proc)
        {
            av_timer_proc->read_proc  = avalanche_p_sched_clock_calibrate_get;
            av_timer_proc->write_proc = avalanche_p_sched_clock_calibrate;
        }
    }
    /* For all sched clocks */
    {
        struct proc_dir_entry *av_timer_proc = NULL;
        av_timer_proc = create_proc_entry("sched_clock_test", 0644, avalanche_proc_root);
        if (av_timer_proc)
        {
            av_timer_proc->read_proc  = avalanche_p_sched_clock_test_get;
            av_timer_proc->write_proc = avalanche_p_sched_clock_test;
        }
    }

	/* Create other proc entries - implemented in avalanche_intc.c */
	avalanche_proc_entries();
    return (0);
}
fs_initcall(avalanche_proc_init);

#ifdef CONFIG_ARM_AVALANCHE_VLYNQ
//arch_initcall(vlynq_bus_init);
#endif


const char *get_system_type(void)
{
	return "Texas Instruments Cable SoC";
}

/* This structure is a subset of old_serial_port in 8250.h. Redefined here
 * because old_serial_port happens to be a private structure of 8250 and 
 * his structure here has got nothing to do with the 8250 concept of 
 * old_serial_port.
 */
struct serial_port_dfns 
{
    unsigned int irq;
    unsigned int iomem_base;
};

/* from asm/serial.h */

/* Standard COM flags (except for COM4, because of the 8514 problem) */
#if 0
#ifdef CONFIG_SERIAL_DETECT_IRQ
#define STD_COM_FLAGS (ASYNC_BOOT_AUTOCONF | ASYNC_SKIP_TEST | ASYNC_AUTO_IRQ)
#define STD_COM4_FLAGS (ASYNC_BOOT_AUTOCONF | ASYNC_AUTO_IRQ)
#else
#define STD_COM_FLAGS (ASYNC_BOOT_AUTOCONF | ASYNC_SKIP_TEST)
#define STD_COM4_FLAGS ASYNC_BOOT_AUTOCONF
#endif
#endif
#define STD_COM_FLAGS UPF_BOOT_AUTOCONF

#ifdef CONFIG_CPU_BIG_ENDIAN
#define AVALANCHE_SERIAL_OFFSET 3
#else
#define AVALANCHE_SERIAL_OFFSET 0
#endif

static struct serial_port_dfns serial_port_dfns[] = {
        {.irq = AVALANCHE_UART0_INT, .iomem_base = (AVALANCHE_UART0_REGS_BASE + AVALANCHE_SERIAL_OFFSET)},
#if (CONFIG_AVALANCHE_NUM_SER_PORTS > 1)
        {.irq = AVALANCHE_UART1_INT, .iomem_base = (AVALANCHE_UART1_REGS_BASE + AVALANCHE_SERIAL_OFFSET)},
#if (CONFIG_AVALANCHE_NUM_SER_PORTS > 2)
        {.irq = AVALANCHE_UART2_INT, .iomem_base = (AVALANCHE_UART2_REGS_BASE + AVALANCHE_SERIAL_OFFSET)},
#endif
#endif
};

int  ti_avalanche_setup(void)
{
    int i, j;
    struct uart_port av_serial[CONFIG_AVALANCHE_NUM_SER_PORTS];

	/* Initialize the platform first up */
    avalanche_soc_platform_init();

#ifdef CONFIG_SERIAL_8250

	memset(&av_serial, 0, sizeof(av_serial));

    for ( i = 1, j = 0; j < CONFIG_AVALANCHE_NUM_SER_PORTS; j++)
    {
        if(j == CONFIG_AVALANCHE_CONSOLE_PORT){        
	        av_serial[j].line		= 0;
        } else {
	        av_serial[j].line		= i++;
        }
	    av_serial[j].irq		= LNXINTNUM(serial_port_dfns[j].irq);
	    av_serial[j].flags		= STD_COM_FLAGS;
#ifdef CONFIG_MACH_PUMA5 /* For Puma-5 SoC */
        av_serial[j].uartclk	= avalanche_get_vbus_freq();
#else /* CONFIG_MACH_PUMA6  For Puma-6 SoC */
        av_serial[j].uartclk	= PAL_sysClkcGetFreq(PAL_SYS_CLKC_UART0); /* UART0-2 use the same Clkc 1x */
#endif
	    av_serial[j].iotype	    = UPIO_MEM;
	    av_serial[j].mapbase 	= IO_VIRT2PHY(serial_port_dfns[j].iomem_base);
	    av_serial[j].membase  	= (char*)serial_port_dfns[j].iomem_base;
	    av_serial[j].regshift	= 2;

#ifdef CONFIG_KGDB_8250
		kgdb8250_add_port(j, &av_serial[j]);
#endif
    } 
 
    for ( i = 0; i < CONFIG_AVALANCHE_NUM_SER_PORTS; i++)
	    if (early_serial_setup(&av_serial[i]) != 0) 
	        printk(KERN_ERR "early_serial_setup on port %d failed.\n", i);		
#endif

	return 0;
}


