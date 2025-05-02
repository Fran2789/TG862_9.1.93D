/*
 * File name: mmxp_q.c
 *
 * Description: This is part of mxp module implemented queue management.
 *              It must be included into mmxpcore.c and is moved to separate
 *              file to be readable only.
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

/*
   Queue management operates with pull of message containers and pull of queues.
   At the beginning all containers linked into the dual linked list. The list refered
   by two pinters freeHead and freeTail.
   Each queue also has two pointers. When queue needs a container it remove the first
   free pinted by freeHead and add it to the tail of the queue. When the queue release
   the message it return the container to the tail of the free list

*/
static MSG_CONTAINER_T     msgs[MAX_MASSAGES];
static MSG_CONTAINER_T     *freeHead;
static MSG_CONTAINER_T     *freeTail;
static MSG_QUEUE_T         mqueue[MAX_QUEUES];

/* message list management *************************************************************/
/* Insert container to the list head                                                   */
/*
static void q_putFirst(MSG_CONTAINER_T **head, MSG_CONTAINER_T **tail, MSG_CONTAINER_T *this)
{
  MSG_CONTAINER_T *tmp;

  this->p  = NULL;
  if (!(*head)){
    this->n = NULL;
    *head   = this;
    *tail   = this;
  } else {
    tmp     = *head;
    *head   = this;
    this->n = tmp;
    tmp->p  = this;
  }
}*/
/****************************************************************************************/
/* Insert container to the list tail                                                   */
static void q_putLast(MSG_CONTAINER_T **head, MSG_CONTAINER_T **tail, MSG_CONTAINER_T *this)
{
  MSG_CONTAINER_T *tmp;

  this->n  = NULL;
  if (!(*tail)){
    this->p = NULL;
    *head   = this;
    *tail   = this;
  } else {
    tmp     = *tail;
    *tail   = this;
    this->p = tmp;
    tmp->n  = this;
  }
}

/****************************************************************************************/
/* delete container from the list                                                       */
static void q_msgDelete(MSG_CONTAINER_T **head, MSG_CONTAINER_T **tail, MSG_CONTAINER_T *this)
{
  if (this->n) (this->n)->p = this->p;
  else         *tail        = this->p;

  if (this->p) (this->p)->n = this->n;
  else         *head        = this->n;
}

/*********************************************************************************
* FUNCTION: q_Init
*
* DESCRIPTION: Initialize the list of free containers, and queue pull
*********************************************************************************/
void q_Init(void)
{
  int j;

  freeHead = NULL;
  freeTail = NULL;
  for (j = 0; j < MAX_MASSAGES; j++)
    q_putLast(&freeHead, &freeTail, &msgs[j]);

  memset(mqueue, 0, sizeof(mqueue));
  mqueue[0].state = 1; /* we don't use queue #0 */
  for(j=1; j<MAX_QUEUES; j++){
    init_waitqueue_head(&(mqueue[j].queue_lock));
  }
}

/*********************************************************************************
* FUNCTION: msg_queue_alloc
*
* DESCRIPTION:
*********************************************************************************/
static int msg_queue_alloc(void)
{
    int j;
    for (j = 1; j < MAX_QUEUES; j++){
        if (!(mqueue[j].state)){
            mqueue[j].state = 1;
            return j;
        }
    }
    return 0;
}

/*********************************************************************************
* FUNCTION: qcb_by_name
*
* DESCRIPTION:
*********************************************************************************/
static int qcb_by_name(char *name){

  int j;
  for (j = 1; j < MAX_QUEUES; j++)
    if ((mqueue[j].state != 0) && (!strcmp(mqueue[j].name, name)))
      return j;

  return -1; /* name not found */
}

/*********************************************************************************
* FUNCTION: mxp_q_create
*
* DESCRIPTION:
*********************************************************************************/
static int mxp_q_create(MXP_CMD_T*  msg)
{
  unsigned long irq_st;
  int qid;

  local_irq_save(irq_st);

  /* check whether the named queue already exists */
  if ( qcb_by_name(msg->cp.q.name) > 0 ){
    local_irq_restore(irq_st);
    return ERR_ASGN;
  }

  /* try to allocate qcb */
  if ((qid = msg_queue_alloc()) == 0){
    local_irq_restore(irq_st);
    return ERR_NOQCB;
  }

  strcpy(mqueue[qid].name,   msg->cp.q.name);
  mqueue[qid].msgcnt   = 0;
  mqueue[qid].depth    = msg->cp.q.depth;
  mqueue[qid].taskId   = msg->cp.q.tid;
  mqueue[qid].events   = msg->cp.q.events;
  mqueue[qid].head     = NULL;
  mqueue[qid].tail     = NULL;
  mqueue[qid].wait4msg = 0;
  //ARRIS ADD Start
  mqueue[qid].hiwater_cnt = 0;
  //ARRIS ADD End
  msg->cp.q.qid = qid;

  local_irq_restore(irq_st);
  return ERR_NOERR;
}

/*********************************************************************************
* FUNCTION: mxp_q_delete
*
* DESCRIPTION:
*********************************************************************************/
static int mxp_q_delete(MXP_CMD_T*  msg)
{
  unsigned long irq_st;
  MSG_CONTAINER_T *cont;
  int qid = msg->cp.q.qid;

  local_irq_save(irq_st);
  if ((qid <= 0) || (qid >= MAX_QUEUES) || (mqueue[qid].state == 0)){
    local_irq_restore(irq_st);
    return ERR_QIDINV;
  }

  /* remove all messages from the queue */
  while (mqueue[qid].head != NULL){
    cont = mqueue[qid].head;
    q_msgDelete( &(mqueue[qid].head), &(mqueue[qid].tail), cont);
    q_putLast(&freeHead, &freeTail, cont);
  }

  mqueue[qid].state = 0;

  local_irq_restore(irq_st);
  return ERR_NOERR;
}

/*********************************************************************************
* FUNCTION: mxp_q_delete_all
*
* DESCRIPTION:
*********************************************************************************/
static int mxp_q_delete_all(void)
{
  MXP_CMD_T  msg;
  int        j;

  for (j=1; j<MAX_QUEUES; j++){
    if (mqueue[j].state > 0){
      msg.cp.q.qid = j;
      mxp_q_delete(&msg);
    }
  }
  return ERR_NOERR;
}

/*********************************************************************************
* FUNCTION: mxp_q_post
*
* DESCRIPTION:
*********************************************************************************/
static int mxp_q_post(MXP_CMD_T*  msg)
{
  unsigned long irq_st;
  MSG_CONTAINER_T *cont;
  int qid = msg->cp.q.qid;
  int wakeup_q = 0;
  int wakeup_t = 0;
  MXP_CMD_T  msg_ev;

  local_irq_save(irq_st);
  if ((qid <= 0) || (qid >= MAX_QUEUES) || (mqueue[qid].state == 0)){
    local_irq_restore(irq_st);
    return ERR_QIDINV;
  }

  if (mqueue[qid].msgcnt >= mqueue[qid].depth){
    local_irq_restore(irq_st);
    return ERR_QFULL;
  }
  /* prepare the message */
  cont = freeHead;
  q_msgDelete(&freeHead, &freeTail, cont);
  cont->msg = msg->cp.q.msg_ptr;
  q_putLast( &(mqueue[qid].head), &(mqueue[qid].tail), cont);
  mqueue[qid].msgcnt++;
  
  //ARRIS ADD Start
  if(mqueue[qid].msgcnt > mqueue[qid].hiwater_cnt)
  {
      mqueue[qid].hiwater_cnt = mqueue[qid].msgcnt;
  }
  //ARRIS ADD End
  
  if (mqueue[qid].wait4msg > 0){
    mqueue[qid].wait4msg = 0;
    wakeup_q = 1;
  }

  if (mqueue[qid].taskId != 0){
    wakeup_t = mqueue[qid].taskId;
    msg_ev.cp.ev.tid    = wakeup_t;
    msg_ev.cp.ev.events = mqueue[qid].events;
  }

  local_irq_restore(irq_st);
  if (wakeup_q)
    wake_up(&(mqueue[qid].queue_lock));

  if (wakeup_t)
    return mxp_ev_post(&msg_ev);

  return ERR_NOERR;
}

/*********************************************************************************
* FUNCTION: mxp_q_wait
*
* DESCRIPTION:
*********************************************************************************/
static int mxp_q_wait(MXP_CMD_T*  msg)
{
  unsigned long irq_st;
  MSG_CONTAINER_T *cont;
  int qid = msg->cp.q.qid;
  int ret;

  local_irq_save(irq_st);
  if ((qid <= 0) || (qid >= MAX_QUEUES) || (mqueue[qid].state == 0)){
    local_irq_restore(irq_st);
    return ERR_QIDINV;
  }

  while(1){
    if (mqueue[qid].msgcnt){
      /* we have a message in the queue */
      cont = mqueue[qid].head;
      q_msgDelete(&(mqueue[qid].head), &(mqueue[qid].tail), cont);
      q_putLast(&freeHead, &freeTail, cont);
      msg->cp.q.msg_ptr = cont->msg;
      mqueue[qid].msgcnt--;
      local_irq_restore(irq_st);
      return ERR_NOERR;
    }

    if (msg->cp.q.timeout == MX_NO_BLOCK){
      local_irq_restore(irq_st);
      return ERR_QEMPTY;
    }

    /* if timeout is not MX_NO_BLOCK, consider it as MX_INDEFINITE */
    mqueue[qid].wait4msg = 1; /* flag that we wait a message */
    local_irq_restore(irq_st);
    ret = wait_event_interruptible( (mqueue[qid].queue_lock), mqueue[qid].wait4msg == 0);
    local_irq_save(irq_st);

    /* after waking up we have to decide whether it was caused by post message or
      other unexpected signal */
    if ( ret == -ERESTARTSYS ){
      /* it was unexpected signal */
      mqueue[qid].wait4msg       = 0;
      local_irq_restore(irq_st);
      printk( KERN_INFO "mxp_q_wait for queueId %d waken up by unexpected signal\n", qid);
      return SYS_CONFIG_ERR;
    }

    /* here we must have a message; if we don't somebody tool it */
    if (mqueue[qid].msgcnt == 0){
      local_irq_restore(irq_st);
      printk( KERN_INFO "mxp_q_wait for queueId %d found no message\n", qid);
      return SYS_CONFIG_ERR;
    }
  }
}

/*********************************************************************************
* FUNCTION: mxp_q_identify
*
* DESCRIPTION: find qid by name
*********************************************************************************/
static int mxp_q_identify(MXP_CMD_T*  msg)
{
  unsigned long irq_st;
  int ret = ERR_NOERR;

  local_irq_save(irq_st);

  if ((msg->cp.q.qid = qcb_by_name(msg->cp.q.name)) == 0)
    ret = ERR_INVNAME;

  local_irq_restore(irq_st);
  return ret;
}

/*********************************************************************************
* FUNCTION: mxp_q_inquiry
*
* DESCRIPTION: get number of messages
*********************************************************************************/
static int mxp_q_inquiry(MXP_CMD_T*  msg)
{
  unsigned long irq_st;
  int qid = msg->cp.q.qid;

  local_irq_save(irq_st);
  if ((qid <= 0) || (qid >= MAX_QUEUES) || (mqueue[qid].state == 0)){
    local_irq_restore(irq_st);
    return ERR_QIDINV;
  }

  msg->cp.q.depth = mqueue[qid].msgcnt;

  local_irq_restore(irq_st);
  return ERR_NOERR;
}

/*********************************************************************************
* FUNCTION: mxp_queue_proc
*
* DESCRIPTION: form the output for /proc/mxpqueue file
*********************************************************************************/
static int mxp_queue_proc(char *buf, char **start, off_t offset,
                   int count, int *eof, void *data)
{
  int len = 0;
  int j;
  len += sprintf(buf + len, "Linux MXP queues\n");

  for (j=1; j<MAX_QUEUES; j++){
    if (mqueue[j].state){
      //ARRIS Modify Start
      len += sprintf(buf + len, "%2d %3d (%3d/%3d) %3d %s\n", j,
                 mqueue[j].taskId, mqueue[j].hiwater_cnt,mqueue[j].depth, mqueue[j].msgcnt, mqueue[j].name);
      //ARRIS Modify End
    }
  }

  *eof = 1;
  return len;
}
