/*
 * File name: mmxp_ev.c
 *
 * Description: This is part of mxp module implemented event management.
 *              It must be included into mmxpcore.c and is moved to separate
 *              file to be readable only
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

/**********************************************************************
  post an event
**********************************************************************/
static int mxp_ev_post(MXP_CMD_T*  msg)
{
  unsigned long irq_st;
  int          tid  = msg->cp.ev.tid;
  int          wake = 0;

  local_irq_save(irq_st);
  if ((tid <= 0) || (tid >= MXP_TASK_MAX) || (mxp_tcb[tid].busy == 0)){
    local_irq_restore(irq_st);
    return ERR_TIDINV;
  }

  mxp_tcb[tid].events_posted |= msg->cp.ev.events;
  mxp_tcb[tid].event_cnt++;

  if (mxp_tcb[tid].wait4event != 0){
    if (mxp_tcb[tid].events_condition == MX_OR_COND){
      wake = ((mxp_tcb[tid].events_posted & mxp_tcb[tid].events_mask) != 0);
    } else if (mxp_tcb[tid].events_condition == MX_AND_COND){
      wake = ((mxp_tcb[tid].events_posted & mxp_tcb[tid].events_mask) == mxp_tcb[tid].events_mask);
    }
  }

  if (wake)
    mxp_tcb[tid].wait4event = 0;

  local_irq_restore(irq_st); /* TODO: check to move that after wake_up */
  if (wake)
    wake_up(&(mxp_subtcb[tid].gate_lock));

  return ERR_NOERR;
}


/**********************************************************************
  clear specified events
**********************************************************************/
static int mxp_ev_wait(MXP_CMD_T*  msg)
{
  unsigned long irq_st;
  int          tid  = msg->cp.ev.tid;
  unsigned long events;
  int ret;

  local_irq_save(irq_st);
  if ((tid <= 0) || (tid >= MXP_TASK_MAX) || (mxp_tcb[tid].busy == 0)){
    local_irq_restore(irq_st);
    return ERR_TIDINV;
  }

  events = msg->cp.ev.events;

  /* check whether we have events already */
  if (msg->cp.ev.condition == MX_OR_COND){
    if ((mxp_tcb[tid].events_posted & events) != 0){
      msg->cp.ev.events = events & mxp_tcb[tid].events_posted;
      mxp_tcb[tid].events_posted &= ~events;
      local_irq_restore(irq_st);
      return ERR_NOERR;
    }
  } else if (msg->cp.ev.condition == MX_AND_COND){
    if ((mxp_tcb[tid].events_posted & events) == events){
      msg->cp.ev.events = events;
      mxp_tcb[tid].events_posted &= ~events;
      local_irq_restore(irq_st);
      return ERR_NOERR;
    }
  } else {
    local_irq_restore(irq_st);
    printk("mxp_ev_wait called for task %d with illegal condition\n", tid);
    return SYS_ILLEGAL_REQUEST; /* condition illegal */
  }

  if (msg->cp.ev.timeout == MX_NO_BLOCK){
    local_irq_restore(irq_st);
    return ERR_NOEVT;
  }

  if (msg->cp.ev.timeout != MX_INDEFINITE){
    local_irq_restore(irq_st);
    printk("mxp_ev_wait called for task %d with illegal timeout\n", tid);
    return SYS_ILLEGAL_REQUEST; /* condition illegal */
  }

  mxp_tcb[tid].events_condition = msg->cp.ev.condition;
  mxp_tcb[tid].events_mask      = events;
  mxp_tcb[tid].wait4event       = 1;


  local_irq_restore(irq_st);
  ret = wait_event_interruptible( (mxp_subtcb[tid].gate_lock), mxp_tcb[tid].wait4event == 0);
/*  interruptible_sleep_on( &(mxp_subtcb[tid].gate_lock)); */
  local_irq_save(irq_st);

  /* after waking up we have to decide whether it was caused by post event or
     other unexpected signal */
  if ( ret == -ERESTARTSYS ){
    /* it was unexpected signal */
    mxp_tcb[tid].wait4event       = 0;
    local_irq_restore(irq_st);
    printk( KERN_INFO "mxp_ev_wait for task %d waken up by unexpected signal\n", tid);
    return SYS_CONFIG_ERR;
  }

  /* it was post event */
  if (mxp_tcb[tid].events_condition == MX_OR_COND){
    msg->cp.ev.events = mxp_tcb[tid].events_mask & mxp_tcb[tid].events_posted;
    mxp_tcb[tid].events_posted &= ~mxp_tcb[tid].events_mask;
  } else {
    msg->cp.ev.events = mxp_tcb[tid].events_mask;
    mxp_tcb[tid].events_posted &= ~mxp_tcb[tid].events_mask;
  }

  local_irq_restore(irq_st);
  return ERR_NOERR;
}

/**********************************************************************
  clear specified events
**********************************************************************/
static int mxp_ev_clear(MXP_CMD_T*  msg)
{
  unsigned long irq_st;
  int          tid = msg->cp.ev.tid;

  local_irq_save(irq_st);
  if ((tid <= 0) || (tid >= MXP_TASK_MAX) || (mxp_tcb[tid].busy == 0)){
    local_irq_restore(irq_st);
    return ERR_TIDINV;
  }

  mxp_tcb[tid].events_posted &= ~msg->cp.ev.events;

  local_irq_restore(irq_st);
  return ERR_NOERR;
}
/**********************************************************************
  return all posted events
**********************************************************************/
static int mxp_ev_inquiry(MXP_CMD_T*  msg)
{
  unsigned long irq_st;
  int          tid = msg->cp.ev.tid;

  local_irq_save(irq_st);
  if ((tid <= 0) || (tid >= MXP_TASK_MAX) || (mxp_tcb[tid].busy == 0)){
    local_irq_restore(irq_st);
    return ERR_TIDINV;
  }
  msg->cp.ev.events = mxp_tcb[tid].events_posted;
  local_irq_restore(irq_st);
  return ERR_NOERR;
}
/********************************************************************
This API is currently used by hw_dspmod.c to post interrupt event
* to DEX
**********************************************************************/
int mxp_ev_post_by_tid(int tid, unsigned long event)
{
    MXP_CMD_T   msg;

    msg.cp.ev.tid       = tid;
    msg.cp.ev.events    = event;

    return ( mxp_ev_post(&msg) );
}

EXPORT_SYMBOL(mxp_ev_post_by_tid);

/**********************************************************************/

