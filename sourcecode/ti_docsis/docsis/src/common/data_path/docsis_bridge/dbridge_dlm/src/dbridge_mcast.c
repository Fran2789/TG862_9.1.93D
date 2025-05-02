/*
 *
 * dbridge_db.c
 * Description:
 * DOCSIS bridge multicase implementation
 *
 * Copyright (C) 2008 Texas Instruments Incorporated - http://www.ti.com/ 
 * 
 * 
 *  Redistribution and use in source and binary forms, with or without 
 *  modification, are permitted provided that the following conditions 
 *  are met:
 *
 *    Redistributions of source code must retain the above copyright 
 *    notice, this list of conditions and the following disclaimer.
 *
 *    Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the 
 *    documentation and/or other materials provided with the   
 *    distribution.
 *
 *    Neither the name of Texas Instruments Incorporated nor the names of
 *    its contributors may be used to endorse or promote products derived
 *    from this software without specific prior written permission.
 *
 *  THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS 
 *  "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT 
 *  LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR
 *  A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT 
 *  OWNER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, 
 *  SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT 
 *  LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
 *  DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
 *  THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT 
 *  (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE 
 *  OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 *
*/

#define _DBRIDGE_MCAST_C_

/*! \file dbridge_db.c
    \brief the docsis bridge multicast support
*/

/**************************************************************************/
/*      INCLUDES:                                                         */
/**************************************************************************/
#include "dbridge_mcast.h"
#include "dbridge_hal_filter.h"
#include <linux/kernel.h>
#include <linux/module.h>
#include <linux/ti_hil.h>
/**************************************************************************/
/*      EXTERNS Declaration:                                              */
/**************************************************************************/
 
/**************************************************************************/
/*      DEFINES:                                                          */
/**************************************************************************/



/**************************************************************************/
/*      LOCAL DECLARATIONS:                                               */
/**************************************************************************/



 
/**************************************************************************/
/*      LOCAL VARIABLES:                                                  */
/**************************************************************************/

/*  Structure which keeps track of all the information for the DOCSIS Bridge. */

 LIST_HEAD (Dbridge_Mcast_list);

/**************************************************************************/
/*      INTERFACE FUNCTIONS Implementation:                               */
/**************************************************************************/

/**************************************************************************/
/*      LOCAL FUNCTIONS:                                                  */
/**************************************************************************/


/*************************************************************************************/
/*! \fn int DbridgeDb_AddMcastMac(unsigned char* mac_address)
 *************************************************************************************
 *  \brief This function Add MAC address to Docsis bridge DB 
 *  \param[in] satic Mcast mac address.
 *  \return OK or error status.
 *************************************************************************************/
 int DbridgeDb_AddMcastMac(unsigned char* mac_address, McastFilterType_e filter_type)
 {
 
	 DbridgeMcastMac_t   *mcast_info;

	 mcast_info = DbridgeDb_GetMcastByMac(mac_address);
	 if(mcast_info)
     {
         mcast_info->refCount++;
		 printk(KERN_DEBUG "\n find match between inpute Mcast Mac %.2x-%.2x-%.2x-%.2x-%.2x-%.2x and Mcast table \n",
				mcast_info->macAddress[0],
				mcast_info->macAddress[1],
				mcast_info->macAddress[2],
				mcast_info->macAddress[3],
				mcast_info->macAddress[4],
				mcast_info->macAddress[5]);
		 return 0;
	 }
	 /* Allocate memory for Cache. */
	 mcast_info = (struct DbridgeMcastMac *) kmalloc(sizeof(struct DbridgeMcastMac), GFP_KERNEL);
	 if (mcast_info == NULL)
	 {
		 printk (KERN_ERR "Error: Failed to allocate memory for mcast info\n");
		 return -1;
	 }

	 mcast_info->hwHashIndex = 1;
     mcast_info->Type = filter_type;
	 memcpy ((void *)&mcast_info->macAddress, (void *)mac_address, MAC_ADDRESS_LEN);
	 mcast_info->refCount=1;
     
	 
	 list_add_tail((struct list_head *)&mcast_info->links, &Dbridge_Mcast_list);
	 
     (void)HAL_MacDsMulticastDataFilterAdd(mac_address);
#ifdef DBRIDGE_LOG
	 if(dbridgeDbgLevel & DBRIDGE_DBG_MCAST_CABLE)
	 
	 printk(KERN_DEBUG "\nAdd Mcast Filter for %.2x-%.2x-%.2x-%.2x-%.2x-%.2x %d \n",
			(unsigned char)mac_address[0],
			(unsigned char)mac_address[1],
			(unsigned char)mac_address[2],
			(unsigned char)mac_address[3],
			(unsigned char)mac_address[4],
			(unsigned char)mac_address[5],filter_type);
#endif	 
	 return 0;
 }

/*************************************************************************************/
/*! \fn int DbridgeDb_DelMcastMac(unsigned char* mac_address)
 *************************************************************************************
 *  \brief This function Del MAC address from Docsis bridge DB 
 *  \return OK or error status.
 *************************************************************************************/
 void DbridgeDb_DelMcastMac(unsigned char* mac_address)
 {
	 DbridgeMcastMac_t   *mcast_info;
     Uint8 *buf=NULL;

	 mcast_info = DbridgeDb_GetMcastByMac(mac_address);
	 if(mcast_info)
	 {
		 mcast_info->refCount--;
		 if(mcast_info->refCount == 0)
		 {

			 list_del((struct list_head *)&mcast_info->links);
			 /* Free allocated memory */
			 kfree (mcast_info);
		 }
	 }
	 /* delete all pp sessions */
	 ti_hil_pp_event (TI_DOCSIS_MCAST_DEL, (void *)buf);

	 return ;
 }

/*************************************************************************************/
/*! \fn DbridgeDb_DelAllMcastMac()
 *************************************************************************************
 *  \brief      This function Deletes all multicast MAC addresses from the 
 *              Docsis bridge DB.
 *  \param[in]  None.
 *  \param[out] None.
 *  \return     None.
 *************************************************************************************/
 void DbridgeDb_DelAllMcastMac(void)
 {
     DbridgeMcastMac_t  *mcast_entry;
     Uint8 *buf=NULL;

     while (!list_empty(&Dbridge_Mcast_list))    
     {
         mcast_entry = list_entry(Dbridge_Mcast_list.next, DbridgeMcastMac_t, 
                                 links);

         /* Delete next entry */
         list_del(&mcast_entry->links);

         /* Free allocated memory */
         kfree (mcast_entry);
     }
	 /* delete all pp sessions */
	 ti_hil_pp_event (TI_DOCSIS_MCAST_DEL, (void *)buf);

 }


/**************************************************************************/
/*! \fn DbridgeMcastMac_t* DbridgeDb_GetMcastByMac(unsigned char* mac_address)
 **************************************************************************
 *  \brief The function retrieves the dbridge Mcast table member for the  
 *  \brief corresponding mac address.
 *  \param[in] cpe mac address.
 *  \return Success-pointer to the Dbridge Mcast table, NULL-If no entry is found.
 **************************************************************************/
DbridgeMcastMac_t* DbridgeDb_GetMcastByMac(unsigned char* mac_address)
{
	struct list_head        *ptr_temp;
    struct DbridgeMcastMac *mcast_info;

    list_for_each(ptr_temp, &Dbridge_Mcast_list)
    {
        mcast_info = (struct DbridgeMcastMac *)ptr_temp;
		if (memcmp ((void *)mac_address, (void *)&mcast_info->macAddress, MAC_ADDRESS_LEN) == 0)
		 {
			 return mcast_info; 
		 }
	
	}
	return NULL;

}

#ifdef DSG
int DbridgeDb_AddDsgAddress(unsigned char* mac_address)
{
    DbridgeDB_AddDsgTunnel(mac_address);
    return DbridgeDb_AddMcastMac(mac_address,DSG_FILTER);
}
void DbridgeDb_DelDsgAddress(unsigned char* mac_address)
{
    DbridgeDB_DelDsgTunnel(mac_address);
    DbridgeDb_DelMcastMac(mac_address);
    (void)HAL_MacDsMulticastDataFilterDelete(mac_address);
}
#endif

/**************************************************************************/
/*! \fn int dbrctl_read_mcastmac(char* page, char **start, off_t offset, int count,int *eof, void *data)
 **************************************************************************
 *  \brief proc file to get static Mcast MAc 
 **************************************************************************/
int dbrctl_read_mcastmac(char* page, char **start, off_t offset, int count,int *eof, void *data)
{
	
	off_t pos = 0;
	off_t begin = 0;
	int len = 0;
	unsigned int index =0;
	struct list_head        *ptr_temp;
    struct DbridgeMcastMac *mcast_info;


	len+= sprintf(page+len, "\nIndex\tMcast MAC\t\tType\tHash index\tref count");
    len+= sprintf(page+len, "\n-----\t---------\t\t----\t----------\t---------");

    list_for_each(ptr_temp, &Dbridge_Mcast_list)
    {
        /* Get the WAN Bridge Information and check for the name. */
        mcast_info = (struct DbridgeMcastMac *)ptr_temp;
		len+= sprintf(page+len,"\n%d\t%.2x:%.2x:%.2x:%.2x:%.2x:%.2x\t\t%d\t%d\t%d",index,
					  mcast_info->macAddress[0],mcast_info->macAddress[1],
					  mcast_info->macAddress[2],mcast_info->macAddress[3],
					  mcast_info->macAddress[4],mcast_info->macAddress[5],
					  mcast_info->Type,mcast_info->hwHashIndex,mcast_info->refCount);
        
		pos = begin + len;
		if (pos < offset)
		{
			len = 0;
			begin = pos;
		}
		if (pos > offset + count)
			goto done;
		index++;
	}


	len+= sprintf(page+len, "\n");

	done:

	*start = page + (offset - begin);
	len -= (offset - begin);

	if (len > count)
		len = count;
	if (len < 0)
		len = 0;

	return len;
}



