/*
 * File name: mmxpcore.c
 *
 * Description: MXP kernel module.
 *
 * Copyright (C) 2008 Texas Instruments, Incorporated
 *
 * This program is free software; you can redistribute it and/or modify it
 * under the terms of the GNU General Public License as published by the
 * Free Software Foundation version 2.
 *
 * This program is distributed "as is" WITHOUT ANY WARRANTY of any kind,
 * whether express or implied; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * General Public License for more details.
 */

#include <linux/init.h>
#include <linux/module.h>
#include <linux/kernel.h>
#include <linux/proc_fs.h>
#include <linux/mm.h>
#include <linux/vmalloc.h>

#include <linux/version.h>
#include <linux/unistd.h>
#include <asm/unistd.h>
#include <linux/spinlock.h>
#include <linux/interrupt.h>
#include <linux/wait.h>
#include <linux/sched.h>
#include <linux/timer.h>
#include <linux/string.h>

#define GG_TICKS_PER_SEC 200

#include <asm/irq.h>

#include "mxp_mod.h"
#include "rtxerr.h"
#include "rtxuser.h"

unsigned long mxp_mips_ticks_per_msec = -1;

#include <linux/miscdevice.h>
#include <asm/uaccess.h>

struct proc_dir_entry *mxp_proc_dir;
/***************************************************************************/
/***************************************************************************/
/* Timer objects variable and definitions                                  */
/***************************************************************************/
/***************************************************************************/
/* timer functions forward declarations */
void   tmrobj_clock(unsigned long dummy);
void   tmrobj_Delete(TMROBJ_T *this);
int    tmrobj_init(void);
void   mxl_tmr_init(void);
void   q_Init(void);
#if LINUX_VERSION_CODE < KERNEL_VERSION(2,6,0)
void
#else
irqreturn_t
#endif

#if  LINUX_VERSION_CODE < KERNEL_VERSION(2,6,19)
mxp_timer_irq_handle(int irq, void *dev_id, struct pt_regs *reg);
#else
mxp_timer_irq_handle(int irq, void *dev_id);
#endif


void tmrobj_Start(
  TMROBJ_T *      this,
  unsigned long   Delta,
  TrmEventHook_T  actionCB,
  void       *    owner
);

/* timer object local variables */
static unsigned long _inUse = 0;
static unsigned long _clock   = 0;
static unsigned long volatile mxp_tick = 0;
// ARRIS REMOVE
//static struct semaphore tmr_lock;

static TMROBJ_T *_heap[MAX_TMROBJS + 1];

/***************************************************************************/
/***************************************************************************/
/***************************************************************************/

/* variables used to lock mxp. It should disable ISR when locked ? */

// ARRIS REMOVE
//spinlock_t    mxp_lock = SPIN_LOCK_UNLOCKED;
unsigned int  mxp_irq_old;

/* system tick */
static unsigned int volatile irq_tick = 0;

/*********************************************************************/
/********** Platform dependent timer implementation ******************/
/*********************************************************************/
#include "mmxptimer.c"

/* MXP tasks control block array */
MXP_TCB_T     *mxp_tcb;
MXP_SUBTCB_T  mxp_subtcb[MXP_TASK_MAX];

/***************************************************************************/
DECLARE_TASKLET(tmrobj_tasklet, tmrobj_clock, 0);

/*********************************************************************************
* FUNCTION: tcb_by_name
*
* DESCRIPTION:
*********************************************************************************/
int tcb_by_name(char *name){

  int j;
  for (j = 1; j < MXP_TASK_MAX; j++)
    if ((mxp_tcb[j].busy != 0) && (!strcmp(mxp_tcb[j].name, name)))
      return j;

  return -1; /*  name not found */
}
EXPORT_SYMBOL(tcb_by_name);
/*********************************************************************************
* FUNCTION: mxp_tcb_identify
*
* DESCRIPTION: returns task ID for given name
*********************************************************************************/
int mxp_tcb_identify(MXP_CMD_T*  msg)
{
  unsigned long irq_st;

  local_irq_save(irq_st);

  if ((msg->cp.task.tid = tcb_by_name(msg->cp.task.name)) == -1){
    local_irq_restore(irq_st);
    return ERR_INVNAME;
  }

  local_irq_restore(irq_st);
  return ERR_NOERR;
}

/*********************************************************************************
* FUNCTION: mxp_tcb_alloc
*
* DESCRIPTION: allocates task control block
*********************************************************************************/
int mxp_tcb_alloc(MXP_CMD_T*  msg)
{
  int j;
  unsigned long irq_st;

  local_irq_save(irq_st);

  if (tcb_by_name(msg->cp.task.name) != -1){
    local_irq_restore(irq_st);
    return ERR_ASGN;
  }

  for (j = 1; j<MXP_TASK_MAX; j++){
    if (mxp_tcb[j].busy == 0){
      mxp_tcb[j].busy  = 1;
      msg->cp.task.tid = j;
      strcpy(mxp_tcb[j].name, msg->cp.task.name);
      printk(KERN_ERR "tcb_alloc: task id=%d,name=%s,busy=%d\n",j,mxp_tcb[j].name,mxp_tcb[j].busy);
      local_irq_restore(irq_st);
      return ERR_NOERR;
    }
  }
  local_irq_restore(irq_st);
  return ERR_NOTCB;
}

/*********************************************************************************
* FUNCTION: mxp_tcb_free
*
* DESCRIPTION: releases a task control block
*********************************************************************************/
int mxp_tcb_free(MXP_CMD_T*  msg)
{
  unsigned long irq_st;

  local_irq_save(irq_st);

  if ((msg->cp.task.tid < 1) ||
      (msg->cp.task.tid >= MXP_TASK_MAX) ||
      (mxp_tcb[msg->cp.task.tid].busy == 0))
  {
    local_irq_restore(irq_st);
    return ERR_TIDINV;
  }

  mxp_tcb[msg->cp.task.tid].busy = 0;
  printk(KERN_ERR "tcb_free: task id=%d, name=%s\n",msg->cp.task.tid,mxp_tcb[msg->cp.task.tid].name);
  local_irq_restore(irq_st);

  return ERR_NOERR;
}

/*********************************************************************************
* FUNCTION: mxp_task_wakeup
*
* DESCRIPTION: timer management calls the function to wakeup a sleeping task
*********************************************************************************/
void mxp_task_wakeup(struct TMROBJ_tag *this)
{
  int tid = (int)(this->owner);
  this->wait4event = 0;
  wake_up(&(mxp_subtcb[tid].gate_lock));
}

/*********************************************************************************
* FUNCTION: mxp_task_sleep
*
* DESCRIPTION: put a task to sleep
*********************************************************************************/
int mxp_task_sleep(MXP_CMD_T*  msg){
  unsigned long irq_st;

  int tid = msg->cp.task_cmd.tid;
  int ret;

  local_irq_save(irq_st);
  /*  start timer object */
  tmrobj_Start(&(mxp_subtcb[tid].tmrobj), msg->cp.task_cmd.timeout,
                                  mxp_task_wakeup, (void*)tid);
  local_irq_restore(irq_st);
  /*  and put the task to sleep */
  ret = wait_event_interruptible( (mxp_subtcb[tid].gate_lock), (mxp_subtcb[tid].tmrobj.wait4event == 0));

  /*  the task will be blocked above until mxp_task_wakeup is called  */
  /*  by tamer management or unexpected signal occures (somebody kills the task) */

  /* if task woke up because kill signal we have to delete timer object
     otherwise it is already deleted by timer manager */
  local_irq_save(irq_st);

  /* after waking up we have to decide whether it was caused by wakeup or
     other unexpected signal */
  if ( ret == -ERESTARTSYS ){
    /* it was unexpected signal */
    mxp_subtcb[tid].tmrobj.wait4event       = 0;
    // ARRIS MOD - use local_irq_restore()
    local_irq_restore(irq_st);
    //    spin_unlock_irqrestore(&mxp_lock, irq_st);
    printk( KERN_INFO "mxp_task_sleep for task %d waken up by unexpected signal\n", tid);
    return SYS_CONFIG_ERR;
  }

  if (mxp_subtcb[tid].tmrobj._index > 0){
    tmrobj_Delete(&(mxp_subtcb[tid].tmrobj));
    /* TODO: may be we have to return an error here */
  }

  local_irq_restore(irq_st);
  return ERR_NOERR;
}
/*********************************************************************/
/********** EVENTS IMPLEMENTATION ************************************/
/*********************************************************************/
#include "mmxp_ev.c"

/*********************************************************************/
/********** QUEUES IMPLEMENTATION ************************************/
/*********************************************************************/
#include "mmxp_q.c"

/*********************************************************************/
/********** TIMERS IMPLEMENTATION ************************************/
/*********************************************************************/
/* DO NOT BE CONFUSED: Timers are MXP timers, but not timers objects */
/*********************************************************************/

typedef enum {
    TMR_FREE  = 0,
    TMR_IDLE,
    TMR_FIRED,
    TMR_ACTIVE
} TMR_STATE_T;

typedef struct mxl_timer_t {
    TMR_STATE_T         state;
    unsigned long       reloadPeriod;
    int                 taskId;
    unsigned long       postEvent;
    int                 queueId;
    void*               pMsg;
    TMROBJ_T            tmrobj;
} MXL_TIMER_T;

/* array of timers */
static MXL_TIMER_T timers[MAX_TIMERS];

/*********************************************************************************
* FUNCTION: mxl_tmr_init
*
* DESCRIPTION:
*********************************************************************************/
void mxl_tmr_init(void){ memset(timers, 0, sizeof(timers)); }

/*********************************************************************************
* FUNCTION: mxl_tmr_alloc
*
* DESCRIPTION: allocate free timer
*********************************************************************************/
static int mxl_tmr_alloc(void)
{
    int j;

    for (j=0; j<MAX_TIMERS; j++){
        if (timers[j].state == TMR_FREE){
            return j;
        }
    }

    return 99999;
}
/*********************************************************************************
* FUNCTION: mxp_tmrCreate
*
* DESCRIPTION: create a timer
*********************************************************************************/
static int mxp_tmrCreate( MXP_CMD_T*  msg)
{
  unsigned long irq_st;
  int tmr_id;

  local_irq_save(irq_st);

  /* allocate a control block */
  if ((tmr_id = mxl_tmr_alloc()) == 99999){
    local_irq_restore(irq_st);
    return ERR_NOTMR;
  }

  timers[tmr_id].state     = TMR_IDLE;
  timers[tmr_id].queueId   = msg->cp.tmr.qid;
  timers[tmr_id].pMsg      = msg->cp.tmr.msg;
  timers[tmr_id].taskId    = msg->cp.tmr.tsk_id;
  timers[tmr_id].postEvent = msg->cp.tmr.ev_fl;

  msg->cp.tmr.tmr_id = tmr_id;

  local_irq_restore(irq_st);
  return ERR_NOERR;
}

/*********************************************************************************
* FUNCTION: mxp_timerTimeOut
*
* DESCRIPTION: timer expiration handler
*********************************************************************************/
static void mxp_timerTimeOut(struct TMROBJ_tag *this){
  unsigned long irq_st;
  MXP_CMD_T    msg;
  MXL_TIMER_T *timer = (MXL_TIMER_T*)(this->owner);

  local_irq_save(irq_st);
  if (timer->reloadPeriod != MX_INDEFINITE && timer->reloadPeriod != 0)
    tmrobj_Start(&(timer->tmrobj), timer->reloadPeriod, mxp_timerTimeOut, timer);
  else
    timer->state = TMR_FIRED;

  if (timer->postEvent){
    msg.cp.ev.tid       = timer->taskId;
    msg.cp.ev.events    = timer->postEvent;
    local_irq_restore(irq_st);
    mxp_ev_post( &msg);
  } else {
    msg.cp.q.qid        = timer->queueId;
    msg.cp.q.msg_ptr    = timer->pMsg;
    local_irq_restore(irq_st);
    mxp_q_post( &msg);
  }
}

/*********************************************************************************
* FUNCTION: mxp_tmrStart
*
* DESCRIPTION: start timer
*********************************************************************************/
int mxp_tmrStart(MXP_CMD_T*  msg )
{
  unsigned long irq_st;
  int tmr_id;

  local_irq_save(irq_st);

  tmr_id = msg->cp.tmr.tmr_id;
  if ((tmr_id >=MAX_TIMERS)||(timers[tmr_id].state == TMR_FREE)){
    local_irq_restore(irq_st);
    return ERR_TMRINV;
  }
  timers[tmr_id].reloadPeriod = msg->cp.tmr.reload;
  timers[tmr_id].state = TMR_ACTIVE;

  tmrobj_Start(&(timers[tmr_id].tmrobj), msg->cp.tmr.timeout,
                      mxp_timerTimeOut, &timers[tmr_id]);

  local_irq_restore(irq_st);
  return ERR_NOERR;
}

/*********************************************************************************
* FUNCTION: mxp_tmrAbort
*
* DESCRIPTION:
*********************************************************************************/
int mxp_tmrAbort(MXP_CMD_T*  msg)
{
  unsigned long irq_st;
  int          tmr_id;
  int          ret = ERR_NOERR;

  local_irq_save(irq_st);

  tmr_id = msg->cp.tmr.tmr_id;
  if ((tmr_id >=MAX_TIMERS)||(timers[tmr_id].state == TMR_FREE)){
    local_irq_restore(irq_st);
    return ERR_TMRINV;
  }
  if (timers[tmr_id].state == TMR_ACTIVE)
      tmrobj_Delete(&(timers[tmr_id].tmrobj));

  if (timers[tmr_id].state == TMR_FIRED)       ret = ERR_TMREXP;
  else if (timers[tmr_id].state == TMR_IDLE)   ret = ERR_TMRIDLE;

  timers[tmr_id].state = TMR_IDLE;
  local_irq_restore(irq_st);

  return ret;
}

/*********************************************************************************
* FUNCTION: mxp_tmrDelete
*
* DESCRIPTION:
*********************************************************************************/
int mxp_tmrDelete(MXP_CMD_T*  msg)
{
  unsigned long irq_st;
  int          tmr_id;
  int          ret = ERR_NOERR;

  local_irq_save(irq_st);

  tmr_id = msg->cp.tmr.tmr_id;
  if ((tmr_id >=MAX_TIMERS)||(timers[tmr_id].state == TMR_FREE)){
    local_irq_restore(irq_st);
    return ERR_TMRINV;
  }

  if (timers[tmr_id].state == TMR_ACTIVE)
      tmrobj_Delete(&(timers[tmr_id].tmrobj));

  timers[tmr_id].state = TMR_FREE;
  local_irq_restore(irq_st);

  return ret;
}

/*********************************************************************************
* FUNCTION: mxp_getTicks
*
* DESCRIPTION:
*********************************************************************************/
static int mxp_getTicks(MXP_CMD_T*  msg){
    msg->cp.tmr.timeout = mxp_tick;
    return ERR_NOERR;
}

/*********************************************************************************
* FUNCTION: mxp_ioctl
*
* DESCRIPTION: mxp ioctl is used to glue mxp user space lib and kernel mxp module
*********************************************************************************/
static long  mxp_ioctl(struct file *file,
                unsigned int ioctl_num,
                unsigned long ioctl_param)
{
    MXP_CMD_T  msg;
    int res;

    if (unlikely((_IOC_TYPE(ioctl_num) != MXPCORE_IOCTL_MAGIC)
                || (_IOC_NR(ioctl_num) > MXPCORE_DEV_IOC_MAXNR)))
    {
        res = -ENOTTY;
        goto error_label;
    }

    if (unlikely((!access_ok(VERIFY_WRITE, (void __user *) ioctl_param, sizeof(MXP_CMD_T) ))
                ||(__copy_from_user((void *)&msg, (void __user *) ioctl_param, (unsigned long) sizeof(MXP_CMD_T)))))
    {
        res = -EFAULT;
        goto error_label;
    }

    switch (ioctl_num)
    {
      case MXP_QUEUE_POST:   {res = mxp_q_post(&msg); break;}
      case MXP_QUEUE_WAIT:   {res = mxp_q_wait(&msg); break;}

      case MXP_EVENT_POST:   {res = mxp_ev_post(&msg); break;}
      case MXP_EVENT_WAIT:   {res = mxp_ev_wait(&msg); break;}

      case MXP_EVENT_CLEAR:  {res = mxp_ev_clear(&msg); break;}
      case MXP_EVENT_INQUIRY:{res = mxp_ev_inquiry(&msg); break;}

      case MXP_QUEUE_CREATE: {res = mxp_q_create(&msg); break;}
      case MXP_QUEUE_DELETE: {res = mxp_q_delete(&msg); break;}
      case MXP_QUEUE_IDENTIFY:{res = mxp_q_identify(&msg); break;}
      case MXP_QUEUE_INQUIRY:{res = mxp_q_inquiry(&msg); break;}
      case MXP_QUEUE_DELALL: {res = mxp_q_delete_all(); break;}
      case MXP_TMR_CREATE:   {res = mxp_tmrCreate(&msg); break;}
      case MXP_TMR_START:    {res = mxp_tmrStart(&msg); break;}
      case MXP_TMR_ABORT:    {res = mxp_tmrAbort(&msg); break;}
      case MXP_TMR_DELETE:   {res = mxp_tmrDelete(&msg); break;}
      case MXP_TMR_GETTICK:  {res = mxp_getTicks(&msg); break;}

      case MXP_TASK_ALLOC:   {res = mxp_tcb_alloc(&msg); break;}
      case MXP_TASK_IDENTIFY:{res = mxp_tcb_identify(&msg); break;}
      case MXP_TASK_FREE:    {res = mxp_tcb_free(&msg); break;}
      case MXP_TASK_SLEEP:   {res = mxp_task_sleep(&msg); break;}

      default:               {res = ERR_INV_SYS_CALL; break;}
    }
    msg.result = res;
    if (unlikely(__copy_to_user((void __user *) ioctl_param, (void *) &msg, (unsigned long) sizeof(MXP_CMD_T))))
    {
        res = -EFAULT;
        goto error_label;
    }
    return 0;

error_label:
    printk(KERN_CRIT "mxp_ioctl error: ioctl_num = %c:%d returns %d\n",
            (char)_IOC_TYPE(ioctl_num), _IOC_NR(ioctl_num), res);
    return res;
}

/*********************************************************************************
* FUNCTION: mxp_timer_irq_handle
*
* DESCRIPTION:
*********************************************************************************/
#if LINUX_VERSION_CODE < KERNEL_VERSION(2,6,0)
void
#else
irqreturn_t
#endif
#if  LINUX_VERSION_CODE < KERNEL_VERSION(2,6,19)
mxp_timer_irq_handle(int irq, void *dev_id, struct pt_regs *reg)
#else
mxp_timer_irq_handle(int irq, void *dev_id)
#endif
{
  irq_tick++;

  tasklet_schedule( &tmrobj_tasklet );
return IRQ_HANDLED;
}


/*********************************************************************************
* FUNCTION: mxp_read_proc
*
* DESCRIPTION: forms output for /proc/mxpmod file
*********************************************************************************/
static int mxp_read_proc(char *buf, char **start, off_t offset,
                   int count, int *eof, void *data)
{
  int len = 0;
  int j;

  len += sprintf(buf + len, "Linux MXP module %9d %9ld %08lx  %ld\n",
                 irq_tick, mxp_tick, _clock, _inUse);

  for (j=1; j<MXP_TASK_MAX; j++){
    if (mxp_tcb[j].busy){
      len += sprintf(buf + len, "%2d %5d %3d %-16s %d %08lX %9d\n", j,
                 mxp_tcb[j].pid, mxp_tcb[j].priority, mxp_tcb[j].name,
                 mxp_tcb[j].wait4event, mxp_tcb[j].events_posted,
                 mxp_tcb[j].event_cnt);
    }
  }

  *eof = 1;
  return len;
}

/******************************************************************************/
/******************************************************************************/
/* Character device related functions                                         */
/******************************************************************************/
/******************************************************************************/
static void mxp_vma_open(struct vm_area_struct *vma) {/* MOD_INC_USE_COUNT*/ try_module_get (THIS_MODULE);}
static void mxp_vma_close(struct vm_area_struct *vma){ /* MOD_DEC_USE_COUNT;*/ module_put (THIS_MODULE);}

/*********************************************************************************
* FUNCTION: mxp_vma_nopage
*
* DESCRIPTION:
*********************************************************************************/
static int mxp_vma_nopage( struct vm_area_struct *vma,
                                        struct vm_fault *vmf)
{
    struct page *page = NULL;
    pgd_t *pgd;
    pmd_t *pmd;
    pte_t *pte;
    unsigned long lpage = (unsigned long)(mxp_tcb);

    spin_lock(&(vma->vm_mm->page_table_lock));
    pgd = pgd_offset(vma->vm_mm, lpage);
    pmd = pmd_offset(pgd,      lpage);
    pte = pte_offset_kernel(pmd,      lpage);
    page = pte_page(*pte);
    spin_unlock(&(vma->vm_mm->page_table_lock));

    get_page(page);
    vmf->page = page;

    return 0;
}

static struct vm_operations_struct mxp_vm_ops = {
    open:   mxp_vma_open,
    close:  mxp_vma_close,
    fault:  mxp_vma_nopage,
};

/*********************************************************************************
* FUNCTION: mxp_mmap
*
* DESCRIPTION:
*********************************************************************************/
static int mxp_mmap( struct file * filp, struct vm_area_struct *vma)
{
    unsigned long offset = vma->vm_pgoff << PAGE_SHIFT;

    if ((offset >= __pa(high_memory)) || (filp->f_flags & O_SYNC))
        vma->vm_flags |= VM_IO;
    vma->vm_flags |= VM_RESERVED;

    vma->vm_ops = &mxp_vm_ops;
    mxp_vma_open(vma);

    return 0;
}

/*********************************************************************************
* FUNCTION: mxp_open
*
* DESCRIPTION:
*********************************************************************************/
static int mxp_open(struct inode * inode, struct file * filp)
{
    try_module_get (THIS_MODULE);
    return 0;
}

/*********************************************************************************
* FUNCTION: mxp_close
*
* DESCRIPTION:
*********************************************************************************/
static int mxp_close(struct inode * inode, struct file * filp)
{
    module_put (THIS_MODULE);
    return 0;
}

static ssize_t mxp_read(struct file *file, char __user *buf, size_t count, loff_t *ppos)
{
	printk(KERN_WARNING "mxp_read not supported\n");
	return 0;
}

static ssize_t mxp_write(struct file *file, const char __user *buf, size_t count, loff_t *ppos)
{
	printk(KERN_WARNING "mxp_write not supportedd\n");
	return 0;
}

static struct file_operations mxpcore_fops = {
    open:    		mxp_open,
    release: 		mxp_close,
    mmap:    		mxp_mmap,
    unlocked_ioctl:   	mxp_ioctl,
    read:    		mxp_read,
    write:   		mxp_write,
};

static struct miscdevice mxpcore_miscdev = {
	minor: MISC_DYNAMIC_MINOR,
	name:  MXP_CORE_DEVICE_NAME,
	fops:  &mxpcore_fops,
};

/*********************************************************************************/
/*********************************************************************************/
/*********************************************************************************
* FUNCTION: init_module
*
* DESCRIPTION:
*********************************************************************************/
int __init init_module(void)
{
    int j, error_num;

    if(mmxp_timer_init())
    {
        return 1;
    }

    mxp_tcb = (MXP_TCB_T*)vmalloc(sizeof(MXP_TCB_T) * MXP_TASK_MAX);
    if (!mxp_tcb){
        printk("MXP: cannot allocate memory for mxp_tcb\n");
        return 1;
    }
    memset(mxp_tcb, 0, sizeof(MXP_TCB_T) * MXP_TASK_MAX);

    memset(mxp_subtcb, 0, sizeof(mxp_subtcb));
    for (j = 1; j < MXP_TASK_MAX; j++)
    {
        init_waitqueue_head(&(mxp_subtcb[j].gate_lock));
    }

    error_num = misc_register(&mxpcore_miscdev);
    if (error_num)
    {
        printk(KERN_ERR "mxpcore: cannot register miscdev err=%d\n", error_num);
        return error_num;
    }

    /*#################added by MTSS #####################*/

    mxp_proc_dir  = proc_mkdir(MXP_PROC_DIR_NAME,NULL);

    create_proc_read_entry("core", 0, mxp_proc_dir, mxp_read_proc, NULL);
    create_proc_read_entry("queue", 0, mxp_proc_dir, mxp_queue_proc, NULL);

    printk("MXP module loaded\n");
    return 0;
}

/*********************************************************************************
 * FUNCTION: cleanup_module
 *
 * DESCRIPTION:
 *********************************************************************************/
void __exit cleanup_module(void)
{

    int err;

    if(mmxp_timer_cleanup())
    {
        err = 1;
    }
    tasklet_kill( &tmrobj_tasklet );

    remove_proc_entry("core", mxp_proc_dir);
    remove_proc_entry("queue", mxp_proc_dir);
    remove_proc_entry(MXP_PROC_DIR_NAME,NULL);

    err = misc_deregister(&mxpcore_miscdev);
    if(err)
    {
        printk(KERN_ERR "mxpcore: cannot de-register miscdev err=%d\n", err);
    }

    printk("MXP module unloaded\n");
}

/********************************************************************************/
/********************************************************************************/
/* Timer objects implementation                                                 */
/********************************************************************************/
/********************************************************************************/

/*********************************************************************************
* FUNCTION: UpTree
*
* DESCRIPTION:
*********************************************************************************/
static void UpTree(TMROBJ_T *this) {
   unsigned K = this->_index;

   _heap[0]=this;

   while (this->_wakeUpTime < _heap[K>>1]->_wakeUpTime) {
      /* Swap With "Father" */
      _heap[K] = _heap[K>>1];
      _heap[K]->_index = K;
      K>>=1;
   }
   _heap[K]=this;
   this->_index=K;
}

/*********************************************************************************
* FUNCTION: DownTree
*
* DESCRIPTION:
*********************************************************************************/
static void DownTree(TMROBJ_T *this) {
   unsigned int J, K=this->_index, Kmax=_inUse>>1;

   while (K<=Kmax) {
      J=K<<1;
       if (J<_inUse) {
         /* Choose The J With The Least Clock */
         /* Among Two Child Nodes !           */
            if (_heap[J|1]->_wakeUpTime < _heap[J]->_wakeUpTime) {
            J|=1;
         }
      }

	  if (this->_wakeUpTime <= _heap[J]->_wakeUpTime) {
            break;
      }

      _heap[K]=_heap[J];
      _heap[K]->_index=K;
      K=J;
   }

   _heap[K]=this;
   this->_index=K;
}

/*********************************************************************************
* FUNCTION: tmrobj_clock
*
* DESCRIPTION:
*********************************************************************************/
void tmrobj_clock(unsigned long dummy) {

  TMROBJ_T    **First;
  TMROBJ_T    *Act;
  unsigned long i;
  unsigned long delta_tick;
  unsigned long irq_st;

  local_irq_save(irq_st);
  delta_tick = irq_tick - mxp_tick;
  if (delta_tick == 0){
    local_irq_restore(irq_st);
    //ARRIS REMOVE
    //up(&tmr_lock);
    return;
  }

  mxp_tick += delta_tick;
  _clock   += delta_tick;
  First=_heap+1;

  while ((_inUse>0) && ((*First)->_wakeUpTime<=_clock)) {
    Act = *First;
    _heap[_inUse]->_index = 1;
    (*First)->_index      = 0;

    *First                = _heap[_inUse];
    _inUse               -= 1;

    if (0 < _inUse)  DownTree((*First));

    if(Act->actionCB){
      local_irq_restore(irq_st);
      Act->actionCB( Act );
      local_irq_save(irq_st);
    }

  }

  if (_clock>=MAX_CLOCK) {
    /* zero _clock, allign down all counters */
    for(i=1;i<=_inUse;i++) {
        _heap[i]->_wakeUpTime-=_clock;
    }
    _clock = 0;  /* Vitaly: TODO: may be "_clock -= MAX_CLOCK" is more correct */
  }

  local_irq_restore(irq_st);
}

/*********************************************************************************
* FUNCTION: tmrobj_Delete
*
* DESCRIPTION:
*********************************************************************************/
void tmrobj_Delete(TMROBJ_T *this) {
  unsigned Index;

  Index=this->_index;
  if (this->_index > 0) {
    this->_index = 0;
    _inUse-=1;
    if (Index<=_inUse) {
        _heap[Index]=_heap[_inUse+1];
        _heap[Index]->_index=Index;
        DownTree(_heap[Index]);
        UpTree(_heap[Index]);
    }
  }
}

/*********************************************************************************
* FUNCTION: tmrobj_Start
*
* DESCRIPTION:
*********************************************************************************/
void tmrobj_Start(
  TMROBJ_T   *    this,
  unsigned long   Delta,
  TrmEventHook_T  actionCB,
  void       *    owner
)
{


  if (Delta >= MAX_CLOCK)
      Delta = MAX_CLOCK-1;
  else if (Delta == 0)
      Delta = 1;


  this->_index = 0;

  this->actionCB       = actionCB;
  this->owner          = owner;

  this->_wakeUpTime = _clock + Delta;
  this->wait4event  = 1;
  /* A New Element Is Added To The Heap ! ( If Possible ) */
  if (_inUse<MAX_TMROBJS) {
    this->_index= _inUse+=1;
    _heap[_inUse]=this;
    UpTree(this);
  }
}

/*********************************************************************************
* FUNCTION: tmrobj_init
*
* DESCRIPTION:
*********************************************************************************/
int tmrobj_init(void)
{
  memset(_heap, 0, sizeof(_heap));
  _inUse = 0;

  return 0;
}

MODULE_LICENSE("GPL v2");
MODULE_AUTHOR("Texas Instruments Incorporated");
MODULE_DESCRIPTION("TI voice MXP core module.");
MODULE_SUPPORTED_DEVICE("Texas Instruments "MXP_CORE_DEVICE_NAME);


