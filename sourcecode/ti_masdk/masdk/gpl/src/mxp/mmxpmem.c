/*
 * File name: mmxpmem.c
 *
 * Description: MXP memory management kernel module.
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

#include <linux/unistd.h>
#include <asm/unistd.h>
#include <linux/spinlock.h>
#include <linux/miscdevice.h>
#include <asm/uaccess.h>
#include <linux/string.h>
#include "mxp_mod.h"
#include "rtxerr.h"

/***************************************************************************/
/*     Local types                                                         */
/***************************************************************************/

typedef struct BLOCK_tag {
  struct BLOCK_tag * next;
  unsigned char    data[1];
} BLOCK_T;

typedef struct PART_HDR_tag {
  unsigned long Part_ID;
  char          name[MAX_NAME_LEN];
  int           BlockSize;
  int           BlockCnt;
  int           FreeBlocks;
  BLOCK_T       *FreeList;
} PART_HDR_T;


typedef struct SEGMENT_DESC_tag {
  unsigned long Seg_ID;
  char          name[MAX_NAME_LEN];
  unsigned char *segPtr;
  int           SegSize;
  unsigned long PartCnt;
  int           Top;
} SEGMENT_DESC_T;

#define FREE_SEG     0xFFFFFFFF
#define BLOCK_BUSY   0xFFFFFFFF

/***************************************************************************/
/***************************************************************************/
/***************************************************************************/

static SEGMENT_DESC_T seg_descs[MAX_SEGMENTS];

/* variables used to lock mxp. It should disable ISR when locked ? */
/* since spin_lock_irqsave/restore was changed to local_irq_save/restore */
/*static spinlock_t    mxp_lock = SPIN_LOCK_UNLOCKED; */

#define SEG_INVALID(X) (((X) >= MAX_SEGMENTS)||(seg_descs[(X)].Seg_ID == FREE_SEG))

/********************************************************************************/
/* Local functions                                                              */
/*********************************************************************************
* FUNCTION: look4FreeSeg
*
* DESCRIPTION:
*********************************************************************************/
static unsigned int look4FreeSeg(void)
{
  int j;

  for (j=0; j<MAX_SEGMENTS; j++)
    if (seg_descs[j].Seg_ID == FREE_SEG)
      return j;

  return FREE_SEG;
}

/*********************************************************************************
* FUNCTION: segByName
*
* DESCRIPTION:
*********************************************************************************/
static unsigned int segByName(char *seg_name)
{
  int j;

  for (j=0; j<MAX_SEGMENTS; j++)
    if ((seg_descs[j].Seg_ID != FREE_SEG)&&(!(strcmp(seg_descs[j].name, seg_name))))
      return j;

  return FREE_SEG; /* name not found */
}

/********************************************************************************/
/* SEGMENT MANAGEMENT                                                           */
/*********************************************************************************
* FUNCTION: mxp_segInit
*
* DESCRIPTION: Segment descriptors initialization
*********************************************************************************/
static void mxp_segInit(void)
{
  int j;

  memset(seg_descs, 0, sizeof(seg_descs));

  for (j = 0; j < MAX_SEGMENTS; j++)
    seg_descs[j].Seg_ID  = FREE_SEG;
}

/*********************************************************************************
* FUNCTION: mxp_segCreate
*
* DESCRIPTION: Create a Segment
*********************************************************************************/
static int mxp_segCreate(MXP_MEM_CMD_T*  msg)
{
  unsigned long irq_st;
  unsigned int free_seg;

  local_irq_save(irq_st);

  if ((free_seg = look4FreeSeg()) == FREE_SEG ){
    local_irq_restore(irq_st);
    return ERR_NOSEG;
  }

  if (segByName(msg->name) != FREE_SEG){
    local_irq_restore(irq_st);
    return ERR_ASGN;
  }

  seg_descs[free_seg].Seg_ID  = free_seg;
  strcpy(seg_descs[free_seg].name, msg->name);
  seg_descs[free_seg].segPtr  = (unsigned char*)msg->ptr;
  seg_descs[free_seg].SegSize = msg->length;
  seg_descs[free_seg].PartCnt = 0;
  seg_descs[free_seg].Top     = msg->length;

  msg->segId = free_seg;

  local_irq_restore(irq_st);
  return ERR_NOERR;
}

/*********************************************************************************
* FUNCTION: mxp_segIdentify
*
* DESCRIPTION: Get SegID by Name
*********************************************************************************/
static int mxp_segIdentify(MXP_MEM_CMD_T*  msg)
{
  unsigned long irq_st;
  int          ret = ERR_NOERR;

  local_irq_save(irq_st);

  if ((msg->segId = segByName(msg->name)) == FREE_SEG){
    ret = ERR_INVNAME;
  }

  local_irq_restore(irq_st);

  return ret;
}

/*********************************************************************************
* FUNCTION: mxp_segGetName
*
* DESCRIPTION: Get SegID by Name
*********************************************************************************/
int mxp_segGetName(MXP_MEM_CMD_T*  msg)
{
  unsigned long irq_st;

  local_irq_save(irq_st);

  if (SEG_INVALID(msg->segId)){
    local_irq_restore(irq_st);
    return ERR_SEGINV;
  }

  strcpy(msg->name, seg_descs[msg->segId].name);

  local_irq_restore(irq_st);
  return ERR_NOERR;
}

/********************************************************************************/
/* PARTITION MANAGEMENT                                                         */
/*********************************************************************************
* FUNCTION: mxp_pCreate
*
* DESCRIPTION: Create a partition
*********************************************************************************/
static int mxp_pCreate(MXP_MEM_CMD_T*  msg)
{
  int aBlockSize = ((msg->length + 3) & 0xFFFFFFFC) + sizeof(void*);
  int reqSize;
  int freeSize;
  PART_HDR_T    *partHdr;
  unsigned int  partId;
  unsigned char *blkStart;
  int j;
  unsigned long irq_st;

  local_irq_save(irq_st);
  /* Do we have this segment? */
  if (SEG_INVALID(msg->segId)){
    local_irq_restore(irq_st);
    return ERR_SEGINV;
  }

  /* Do we have enough room? */
  reqSize  = sizeof(PART_HDR_T) + msg->length * msg->blocks;
  freeSize = seg_descs[msg->segId].Top - (sizeof(PART_HDR_T) * seg_descs[msg->segId].PartCnt);
  if (freeSize < reqSize){
    local_irq_restore(irq_st);
    return ERR_NOMEM;
  }

  /* fill header */
  partId  = seg_descs[msg->segId].PartCnt;
  seg_descs[msg->segId].PartCnt++;
  partHdr = (PART_HDR_T*) seg_descs[msg->segId].segPtr;
  partHdr[partId].Part_ID    = (msg->segId << 16) + partId;
  strcpy(partHdr[partId].name, msg->name);
  partHdr[partId].BlockSize  = aBlockSize;
  partHdr[partId].BlockCnt   = msg->blocks;
  partHdr[partId].FreeBlocks = msg->blocks;

  seg_descs[msg->segId].Top -= (aBlockSize * msg->blocks);
  blkStart = seg_descs[msg->segId].segPtr + seg_descs[msg->segId].Top;
  partHdr[partId].FreeList   = (BLOCK_T*)blkStart;

  /* link all blocks as free */
  for (j=0; j<msg->blocks; j++){
    ((BLOCK_T*)blkStart)->next = (BLOCK_T*)((j<(msg->blocks-1)) ? (blkStart + aBlockSize) : 0);
    blkStart += aBlockSize;
  }

  msg->partId = partHdr[partId].Part_ID;
  local_irq_restore(irq_st);
  return ERR_NOERR;
}

/*********************************************************************************
* FUNCTION: mxp_pIdentify
*
* DESCRIPTION: Find a partition ID by name
*********************************************************************************/
static int mxp_pIdentify(MXP_MEM_CMD_T*  msg)
{
  PART_HDR_T    *partHdr;
  unsigned int  j;
  unsigned long irq_st;

  local_irq_save(irq_st);

  /* Do we have this segment? */
  if (SEG_INVALID(msg->segId)){
    local_irq_restore(irq_st);
    return ERR_SEGINV;
  }

  partHdr = (PART_HDR_T*) seg_descs[msg->segId].segPtr;

  if (seg_descs[msg->segId].PartCnt > 0){
    for (j=0; j<seg_descs[msg->segId].PartCnt; j++)
      if (strcmp(partHdr[j].name, msg->name) == 0){
        msg->partId = partHdr[j].Part_ID;
        local_irq_restore(irq_st);
        return ERR_NOERR;
      }
  } /* if (seg... */

  local_irq_restore(irq_st);
  return ERR_INVNAME;
}

/*********************************************************************************
* FUNCTION: mxp_pInquiry
*
* DESCRIPTION: How many free blocks are left
*********************************************************************************/
static int mxp_pInquiry(MXP_MEM_CMD_T*  msg)
{
  unsigned long segID = msg->partId >> 16;
  unsigned long parID = msg->partId & 0xFFFF;
  PART_HDR_T    *partHdr;
  unsigned long irq_st;

  local_irq_save(irq_st);
  /* Do we have this segment? */
  if (SEG_INVALID(segID)){
    local_irq_restore(irq_st);
    return ERR_PARTINV;
  }

  if (parID >= seg_descs[segID].PartCnt){
    local_irq_restore(irq_st);
    return ERR_PARTINV;
  }

  partHdr = (PART_HDR_T*) seg_descs[segID].segPtr;

  msg->blocks = partHdr[parID].FreeBlocks;

  local_irq_restore(irq_st);
  return ERR_NOERR;
}

/*********************************************************************************
* FUNCTION: mxp_pBalloc
*
* DESCRIPTION: Allocate a memory block
*********************************************************************************/
static int mxp_pBalloc(MXP_MEM_CMD_T*  msg)
{
  unsigned long segID = msg->partId >> 16;
  unsigned long parID = msg->partId & 0xFFFF;
  PART_HDR_T    *partHdr;
  BLOCK_T       *blkTmpPtr;
  unsigned long irq_st;

  local_irq_save(irq_st);
  /* Do we have this segment? */
  if (SEG_INVALID(segID)){
    local_irq_restore(irq_st);
    return ERR_PARTINV;
  }

  if (parID >= seg_descs[segID].PartCnt){
    local_irq_restore(irq_st);
    return ERR_PARTINV;
  }

  partHdr = (PART_HDR_T*) seg_descs[segID].segPtr;

  if (partHdr[parID].FreeBlocks == 0){
    local_irq_restore(irq_st);
    return ERR_NOMEM;
  }

  blkTmpPtr           = partHdr[parID].FreeList; /* get ptr to the first free block*/
  partHdr[parID].FreeList = blkTmpPtr->next;     /* move free list ptr*/
  msg->ptr            = blkTmpPtr->data;         /* set ptr of req-ed block to data*/
  blkTmpPtr->next     = (BLOCK_T*)BLOCK_BUSY;    /* mark block as busy*/
  partHdr[parID].FreeBlocks--;                   /* adjust counter*/

  local_irq_restore(irq_st);
  return ERR_NOERR;
}

/*********************************************************************************
* FUNCTION: mxp_pBfree
*
* DESCRIPTION: Release memory block
*********************************************************************************/
static int mxp_pBfree(MXP_MEM_CMD_T*  msg)
{
  unsigned long segID = msg->partId >> 16;
  unsigned long parID = msg->partId & 0xFFFF;
  PART_HDR_T    *partHdr;
  BLOCK_T       *blkTmpPtr;
  unsigned long irq_st;

  local_irq_save(irq_st);
  /* Do we have this segment? */
  if (SEG_INVALID(segID)){
    local_irq_restore(irq_st);
    return ERR_PARTINV;
  }

  if (parID >= seg_descs[segID].PartCnt){
    local_irq_restore(irq_st);
    return ERR_PARTINV;
  }

  partHdr = (PART_HDR_T*) seg_descs[segID].segPtr;

  blkTmpPtr = (BLOCK_T*)((unsigned char*)(msg->ptr) - sizeof(void*));
  if (blkTmpPtr->next != ((BLOCK_T*)BLOCK_BUSY)){
    local_irq_restore(irq_st);
    return ERR_FREE;
  }

  blkTmpPtr->next = partHdr[parID].FreeList;
  partHdr[parID].FreeList = blkTmpPtr;
  partHdr[parID].FreeBlocks++;

  local_irq_restore(irq_st);
  return ERR_NOERR;
}


/*********************************************************************************
* FUNCTION: mxpmem_ioctl
*
* DESCRIPTION: mxp ioctl is used to glue mxp user space lib and kernel mxp module
*********************************************************************************/
long mxpmem_ioctl(struct file *file,
                unsigned int ioctl_num,
                unsigned long ioctl_param)
{

    MXP_MEM_CMD_T  msg;
    int res;

    if (unlikely(_IOC_TYPE(ioctl_num) != MXPMEM_IOCTL_MAGIC))
    {
        res = -ENOTTY;
        goto error_label;
    }
    if (unlikely(_IOC_NR(ioctl_num) > MXPMEM_DEV_IOC_MAXNR))
    {
        res = -ENOTTY;
        goto error_label;
    }
    if (unlikely(!access_ok(VERIFY_WRITE, (void __user *) ioctl_param, sizeof(MXP_MEM_CMD_T) )))
    {
        res = -EFAULT;
        goto error_label;
    }

    if (unlikely(__copy_from_user((void *)&msg, (void __user *) ioctl_param, (unsigned long) sizeof(MXP_MEM_CMD_T))))
    {
        res = -EFAULT;
        goto error_label;
    }

    switch (ioctl_num) 
    {
        case MXP_MEM_BLK_ALLOC:     {res = mxp_pBalloc(&msg); break;}
        case MXP_MEM_BLK_FREE:      {res = mxp_pBfree(&msg); break;}
        case MXP_MEM_PART_INQUIRY:  {res = mxp_pInquiry(&msg); break;}
        case MXP_MEM_PART_IDENTIFY: {res = mxp_pIdentify(&msg); break;}
        case MXP_MEM_PART_CREATE:   {res = mxp_pCreate(&msg); break;}
        case MXP_MEM_SEG_CREATE:    {res = mxp_segCreate(&msg); break;}
        case MXP_MEM_SEG_IDENTIFY:  {res = mxp_segIdentify(&msg); break;}
        case MXP_MEM_SEG_GETNAME:   {res = mxp_segGetName(&msg); break;}
        case MXP_MEM_SEG_INIT:      {mxp_segInit(); res = ERR_NOERR; break;}

        default:                    {res = -2; break;}
    }
    msg.result = res;
    if (unlikely(__copy_to_user((void __user *) ioctl_param, (void *) &msg, (unsigned long) sizeof(MXP_MEM_CMD_T))))
    {
        res = -EFAULT;
        goto error_label;
    }
    return 0;

error_label:
    printk(KERN_CRIT "\nmxpmem_ioctl error: ioctl_num = %c:%d returns %d\n", 
            (char)_IOC_TYPE(ioctl_num), _IOC_NR(ioctl_num), res);
    return res;    
}

/*********************************************************************************
* FUNCTION: mxp_mem_read_proc
*
* DESCRIPTION:
*********************************************************************************/
static int mxp_mem_read_proc(char *buf, char **start, off_t offset,
                   int count, int *eof, void *data)
{
  int len = 0;
  int j;
  len += sprintf(buf + len, "Linux MXP memory segments\n");
  for (j=0; j<MAX_SEGMENTS; j++){
    len += sprintf(buf + len, "%08X %16s %08X %6d %3d %08X\n",
                   (unsigned int)seg_descs[j].Seg_ID, seg_descs[j].name, (unsigned int)seg_descs[j].segPtr,
                   seg_descs[j].SegSize, (int)seg_descs[j].PartCnt, seg_descs[j].Top);
  }

  *eof = 1;
  return len;
}

/*********************************************************************************
* FUNCTION: mxpmem_open
*
* DESCRIPTION:
*********************************************************************************/
static int mxpmem_open(struct inode * inode, struct file * filp)
{
    try_module_get (THIS_MODULE);
    return 0;
}

/*********************************************************************************
* FUNCTION: mxpmem_close
*
* DESCRIPTION:
*********************************************************************************/
static int mxpmem_close(struct inode * inode, struct file * filp)
{
    module_put (THIS_MODULE);
    return 0;
}

static ssize_t mxpmem_read(struct file *file, char __user *buf, size_t count, loff_t *ppos)
{
	printk(KERN_WARNING "mxpmem_read not supported\n");
	return 0;
}

static ssize_t mxpmem_write(struct file *file, const char __user *buf, size_t count, loff_t *ppos)
{
	printk(KERN_WARNING "mxpmem_write not supportedd\n");
	return 0;
}

static struct file_operations mxpmem_fops = {
    open:    		mxpmem_open,
    release: 		mxpmem_close,
    unlocked_ioctl:   	mxpmem_ioctl,
    read:    		mxpmem_read,
    write:   		mxpmem_write,
};

static struct miscdevice mxpmem_miscdev = {
	minor: MISC_DYNAMIC_MINOR,
	name:  MXP_MEM_DEVICE_NAME,
	fops:  &mxpmem_fops,
};

/*********************************************************************************
* FUNCTION: init_module
*
* DESCRIPTION:
*********************************************************************************/
int __init init_module(void)
{
    int err;
    if ((err = misc_register(&mxpmem_miscdev)))
    {
        printk(KERN_ERR "mxpmem: cannot register miscdev err=%d\n", err);
        return err;
    }    

    mxp_segInit();

    create_proc_read_entry(MXP_PROC_DIR_NAME"/mem", 0, NULL, mxp_mem_read_proc, NULL);

    printk("MXP memory module loaded\n");
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
    if((err = misc_deregister(&mxpmem_miscdev)))
    {
        printk(KERN_ERR "mxpmem: cannot de-register miscdev err=%d\n", err);
    }

    remove_proc_entry(MXP_PROC_DIR_NAME"/mem", NULL);

    printk("MXP memory module unloaded\n");
}

MODULE_LICENSE("GPL v2");
MODULE_AUTHOR("Texas Instruments Incorporated");
MODULE_DESCRIPTION("TI voice MXP memory module.");
MODULE_SUPPORTED_DEVICE("Texas Instruments "MXP_MEM_DEVICE_NAME);
