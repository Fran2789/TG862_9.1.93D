/*
 *  puma6_aep
 *
 *  GPL LICENSE SUMMARY
 *
 *  Copyright(c) 2013 Intel Corporation. All rights reserved.
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
 *  You should have received a copy of the GNU General Public License
 *  along with this program; if not, write to the Free Software
 *  Foundation, Inc., 51 Franklin St - Fifth Floor, Boston, MA 02110-1301 USA.
 *  The full GNU General Public License is included in this distribution
 *  in the file called LICENSE.GPL.
 *
 *  Contact Information:
 *    Intel Corporation
 *    2200 Mission College Blvd.
 *    Santa Clara, CA  97052
 *
 * The file contains the AEP driver for u-boot
 *
 */

#include <common.h>
#include <docsis_ip_boot_params.h>
#include "puma6_aep.h"

//#define AEP_DEBUG 

#ifdef  AEP_DEBUG
#define DEBUGF(fmt,args...) printf(fmt ,##args)
#else
#define DEBUGF(fmt,args...)
#endif


/* AEP Base Address */
#define AEP_BASE_ADDRESS                             (0x0F8C0000)

/* AEP Registers */
#define AEP_REG_OFFSET_PV_CONTROL                    (AEP_BASE_ADDRESS + 0x0000)
#define AEP_REG_OFFSET_ICACHE_BASE_ADDRESS           (AEP_BASE_ADDRESS + 0x0004)
#define AEP_REG_OFFSET_WATCHDOG_TIMER                (AEP_BASE_ADDRESS + 0x0008)
#define AEP_REG_OFFSET_CACHELINE_INVALIDATE          (AEP_BASE_ADDRESS + 0x0010)
#define AEP_REG_OFFSET_PV_SLAVE_SECURITY_ATTR        (AEP_BASE_ADDRESS + 0x0020)
#define AEP_REG_OFFSET_PV_MASTER_SYS_SECURITY        (AEP_BASE_ADDRESS + 0x0024)
#define AEP_REG_OFFSET_PV_MASTER_ATOM_SECURITY       (AEP_BASE_ADDRESS + 0x0028)
#define AEP_REG_OFFSET_PV_MASTER_NP_SECURITY         (AEP_BASE_ADDRESS + 0x002C)
#define AEP_REG_OFFSET_PV_TMR_VALUE0                 (AEP_BASE_ADDRESS + 0x0030)
#define AEP_REG_OFFSET_PV_TMR_LOAD0                  (AEP_BASE_ADDRESS + 0x0034)
#define AEP_REG_OFFSET_PV_TMR_CONTROL0               (AEP_BASE_ADDRESS + 0x0038)
#define AEP_REG_OFFSET_PV_TMR_VALUE1                 (AEP_BASE_ADDRESS + 0x003C)
#define AEP_REG_OFFSET_PV_TMR_LOAD1                  (AEP_BASE_ADDRESS + 0x0040)
#define AEP_REG_OFFSET_PV_TMR_CONTROL1               (AEP_BASE_ADDRESS + 0x0044)
#define AEP_REG_OFFSET_PV_TMR_SCALAR                 (AEP_BASE_ADDRESS + 0x0048)
#define AEP_REG_OFFSET_PV_TMR_SCALAR_PRESET          (AEP_BASE_ADDRESS + 0x004C)
#define AEP_REG_OFFSET_SYS_FUSE_DISABLE              (AEP_BASE_ADDRESS + 0x0068)
#define AEP_REG_OFFSET_PV_MASTERS_ATTR_SEL           (AEP_BASE_ADDRESS + 0x006C)
#define AEP_REG_OFFSET_EXT_EVENT_ENABLE              (AEP_BASE_ADDRESS + 0x0070)
#define AEP_REG_OFFSET_EXT_EVENT_STATUS              (AEP_BASE_ADDRESS + 0x0074)
#define AEP_REG_OFFSET_PROXY_MODE                    (AEP_BASE_ADDRESS + 0x0078)
#define AEP_REG_OFFSET_EXT_INPUT_EVENT_STATUS        (AEP_BASE_ADDRESS + 0x0084)
#define AEP_REG_OFFSET_PV_SEC_STATUS                 (AEP_BASE_ADDRESS + 0x00A0)
#define AEP_REG_OFFSET_ATOM_EMMC_BASE1               (AEP_BASE_ADDRESS + 0x1000)
#define AEP_REG_OFFSET_ATOM_MAILBOX                  (AEP_BASE_ADDRESS + 0x1280)
#define AEP_REG_OFFSET_ATOM_PV_MAILBOX               (AEP_BASE_ADDRESS + 0x12C0)
#define AEP_REG_OFFSET_ATOM_DOORBELL                 (AEP_BASE_ADDRESS + 0x1400)
#define AEP_REG_OFFSET_ATOM_IPC_STATUS               (AEP_BASE_ADDRESS + 0x1404)
#define AEP_REG_OFFSET_ATOM_PV_DOORBELL              (AEP_BASE_ADDRESS + 0x1408)
#define AEP_REG_OFFSET_ATOM_PV_IPC_STATUS            (AEP_BASE_ADDRESS + 0x140C)
#define AEP_REG_OFFSET_PV2ATOM_SW_INT                (AEP_BASE_ADDRESS + 0x1410)
#define AEP_REG_OFFSET_NP_EMMC_BASE2                 (AEP_BASE_ADDRESS + 0x1800)
#define AEP_REG_OFFSET_NP_MAILBOX                    (AEP_BASE_ADDRESS + 0x1A80)  
#define AEP_REG_OFFSET_NP_PV_MAILBOX                 (AEP_BASE_ADDRESS + 0x1AC0)  /* On Send, write by NP, read by AEP. */
#define AEP_REG_OFFSET_NP_DOORBELL                   (AEP_BASE_ADDRESS + 0x1C00)  /* write by AEP, read by NP, */
#define AEP_REG_OFFSET_NP_IPC_STATUS                 (AEP_BASE_ADDRESS + 0x1C04)  /* read by AEP, (NP write 'done') */
#define AEP_REG_OFFSET_NP_PV_DOORBELL                (AEP_BASE_ADDRESS + 0x1C08)  /* write by NP (auto clear 'ready' and 'done' in NP_PV_IPC_STATUS) , read by AEP (auto set ready in NP_PV_IPC_STATUS) */
#define AEP_REG_OFFSET_NP_PV_IPC_STATUS              (AEP_BASE_ADDRESS + 0x1C0C)  /* read by NP  (AEP write 1 to 'done') */
#define AEP_REG_OFFSET_PV2NP_SW_INT                  (AEP_BASE_ADDRESS + 0x1C10)


#define AEP_NP_MAILBOX_SIZE              0x40
#define AEP_IPC_STATUS_READY_MASK        0x00000001
#define AEP_IPC_STATUS_DONE_MASK         0x00000002


#define AEP_IPC_GPIO_WRITE    0
#define AEP_IPC_GPIO_READ     1


/* AEP commands ID */
#define AEP_IPC_CMD_ID_BIST                      0
#define AEP_IPC_CMD_ID_SEC_BOOT_NOTIFY           1
#define AEP_IPC_CMD_ID_GPIO_ACCESS               2
#define AEP_IPC_CMD_ID_PUINT_ACCESS              3
#define AEP_IPC_CMD_ID_AEP_FW_VERSION            4
#define AEP_IPC_CMD_ID_NP_ATOM_COMM              5
#define AEP_IPC_CMD_ID_SET_ATTRIB                6
#define AEP_IPC_CMD_ID_SET_EMMC_FLASH_PARTITION  7
#define AEP_IPC_CMD_ID_ARM_BOOT_COMLETE          8
#define AEP_IPC_CMD_ID_SET_GPIO_ACCESS           9
#define AEP_IPC_CMD_ID_IOSF_SB_ACCESS            10
#define AEP_IPC_CMD_ID_GET_PUNIT_FW_VERSION      11 /* to delete */
#define AEP_IPC_CMD_ID_LAST                      12 
#define AEP_IPC_CMD_ID_COMPLETE_FLAG             0x80000000

#define MAX_TIMEOUT 10000 /* Max time-out for IPC-mailbox in ms */

/* Read and Write resgister macros */
#define reg_write_32(addr, data) (( *(volatile unsigned int *) (addr) ) = (data))
#define reg_read_32(addr)        ( *(volatile unsigned int *) (addr) )


unsigned int g_buffer_in[AEP_NP_MAILBOX_SIZE];    // 4 byte alignment input buffer
unsigned int g_buffer_out[AEP_NP_MAILBOX_SIZE];   // 4 byte alignment input buffer

typedef struct _aep_set_gpio_access_input_t
{
  unsigned int gpio_96_to_127;
  unsigned int gpio_64_to_95;
  unsigned int gpio_32_to_63;
  unsigned int gpio_0_to_31;
} aep_set_gpio_access_input_t;

typedef struct _aep_set_gpio_access_output_t
{
  unsigned int status;
} aep_set_gpio_access_output_t;

typedef struct _aep_gpio_access_input_t
{
  unsigned int offset;
  unsigned int is_read;
  unsigned int write_data;
  unsigned int write_mask;
} aep_gpio_access_input_t;

typedef struct _aep_gpio_access_output_t
{
  unsigned int status;
  unsigned int read_data;
} aep_gpio_access_output_t;

/* Local functions */
static int aep_ipc_send_message(unsigned int cmd_id, unsigned int* message, int msg_length, unsigned int* response, int res_length);
inline unsigned int aep_swap_dword(unsigned int x);

/* Global parameters */
unsigned int g_aep_active = 0;



/* initialize AEP driver
   Must be called at least once before any used of the function below.
   Alwasy return AEP_STATUS_OK
*/
int aep_init(void)
{

    /* Check AEP boot Mode in Boot-Params */
    if (GET_BOOT_PARAM_REG(AEP_MODE) == AEP_MODE_ACTIVE)
    {
        g_aep_active = AEP_ACTIVE;
    }
    else
    {
        g_aep_active = AEP_NOT_ACTIVE;
    }

    /* Check Sillicon stepping in Boot-Params*/
    if ((g_aep_active == 1) && (GET_BOOT_PARAM_REG( SILICON_STEPPING) < SILICON_STEPPING_ID_C_0) )
    {
        printf("Warning: AEP Enabled, but Silicon settings is less than 'C' \n");
        
    }

    /* Check if VSPARC is out of reset */
    if ((g_aep_active == 1) && ( aep_swap_dword(reg_read_32(AEP_REG_OFFSET_PV_CONTROL)) == 0 ) )
    {
        printf("Warning: AEP Enabled, but VSPARC is not out of reset. \n");
        
    }

    return AEP_STATUS_OK;
}

/* Check if AEP is active, or not.
   Can be called only after aep_init(void) had been called before.
   Return:
        AEP_ACTIVE      - AEP is ACTIVE
        AEP_NOT_ACTIVE  - AEP is NOT ACTIVE
*/
int aep_is_active(void)
{
    return g_aep_active;
}

/* Get AEP virtual eMMC controller base address.
   Return: Offset in RAM
*/
unsigned int aep_get_emmc_base_address(void)
{
    return AEP_REG_OFFSET_NP_EMMC_BASE2;
}


/* Set Access for GPIO - For use in debug mode only, with Sepcial AEP Debug F/W.
   Set access atributes for each GPIO
   Input: GPIOs mask 1-enable access, 0-disable access
   Return:
        AEP_STATUS_OK   - Succeed
        AEP_STATUS_NOK  - Failure
*/
int aep_ipc_set_gpio_access(unsigned int gpio_0_to_31, unsigned int gpio_32_to_63, unsigned int gpio_64_to_95, unsigned int gpio_96_to_127)
{
    aep_set_gpio_access_input_t  *p_msg_input;
    aep_set_gpio_access_output_t *p_msg_output;

    p_msg_input  = (aep_set_gpio_access_input_t*)g_buffer_in;
    p_msg_output = (aep_set_gpio_access_output_t*)g_buffer_out;

   
    /* initialize input payload message */
    p_msg_input->gpio_96_to_127 = aep_swap_dword(gpio_96_to_127);
    p_msg_input->gpio_64_to_95  = aep_swap_dword(gpio_64_to_95);
    p_msg_input->gpio_32_to_63  = aep_swap_dword(gpio_32_to_63);
    p_msg_input->gpio_0_to_31   = aep_swap_dword(gpio_0_to_31);

    /* Send AEP IPC Message */
    aep_ipc_send_message(AEP_IPC_CMD_ID_SET_GPIO_ACCESS,g_buffer_in,sizeof(aep_set_gpio_access_input_t),p_msg_output,sizeof(aep_set_gpio_access_output_t));

    /* Check response status */
    if (p_msg_output->status != 0) 
    {
        return AEP_STATUS_NOK;
    }

    return AEP_STATUS_OK;
}


/* Read from to GPIO register
   Input: Register offset and pointer to read value
   Return:
        AEP_STATUS_OK   - Succeed
        AEP_STATUS_NOK  - Failure
   */
int aep_ipc_gpio_read(unsigned int reg_offset, unsigned int* value)
{
    aep_gpio_access_input_t  *p_msg_input;
    aep_gpio_access_output_t *p_msg_output;

    p_msg_input  = (aep_gpio_access_input_t*)g_buffer_in;
    p_msg_output = (aep_gpio_access_output_t*)g_buffer_out;

   
    /* initialize input payload message */
    p_msg_input->offset     = aep_swap_dword(reg_offset);
    p_msg_input->is_read    = aep_swap_dword(AEP_IPC_GPIO_READ);
    p_msg_input->write_data = 0;
    p_msg_input->write_mask = 0;

    /* Send AEP IPC Message */
    aep_ipc_send_message(AEP_IPC_CMD_ID_GPIO_ACCESS,g_buffer_in,sizeof(aep_gpio_access_input_t),p_msg_output,sizeof(aep_gpio_access_output_t));

    /* Check response status */
    if (p_msg_output->status != 0) 
    {
        return AEP_STATUS_NOK;
    }

    /* update output response */
    if (value != NULL)
    {
        *value = p_msg_output->read_data;
    }
    return AEP_STATUS_OK;
}


/* Write to GPIO register
   The Driver will set only the valid bit in the mask according to the 'value'
   Input: Register offset and value and mask
   Return:
        AEP_STATUS_OK   - Succeed
        AEP_STATUS_NOK  - Failure
*/
int aep_ipc_gpio_write(unsigned int reg_offset, unsigned int value, unsigned int mask)
{
    aep_gpio_access_input_t  *p_msg_input;
    aep_gpio_access_output_t *p_msg_output;

    p_msg_input  = (aep_gpio_access_input_t*)g_buffer_in;
    p_msg_output = (aep_gpio_access_output_t*)g_buffer_out;
    
    /* initialize input payload message */
    p_msg_input->offset     = aep_swap_dword(reg_offset);
    p_msg_input->is_read    = aep_swap_dword(AEP_IPC_GPIO_WRITE);
    p_msg_input->write_data = value;
    p_msg_input->write_mask = mask;

    /* Send AEP IPC Message */
    aep_ipc_send_message(AEP_IPC_CMD_ID_GPIO_ACCESS,g_buffer_in,sizeof(aep_gpio_access_input_t),g_buffer_out,sizeof(aep_gpio_access_output_t));

    /* Check response status */
    if (p_msg_output->status != 0) 
    {
        return AEP_STATUS_NOK;
    }

    return AEP_STATUS_OK;
}

static int aep_ipc_send_message(unsigned int cmd_id, unsigned int* message, int msg_length, unsigned int* response, int res_length)
{
    unsigned int *offset_start;
    unsigned int *offset_end;
    unsigned int mask = (AEP_IPC_STATUS_READY_MASK | AEP_IPC_STATUS_DONE_MASK); 
    unsigned int rcev_cmd_id;
    unsigned int timeout = MAX_TIMEOUT;  /* 10 seconds*/ 
   
    if (g_aep_active != 1) 
    {
        printf("AEP: AEP is not loaded\n");
        return AEP_STATUS_NOK;
    }

    //printf("[debug]: aep_ipc_send_message(id=%d,  message=%X, msg_length=%d, response=%X, res_length=%d\n",cmd_id, message, msg_length, response, res_length);

    /* validate input params */
    if (msg_length > AEP_NP_MAILBOX_SIZE)
    {
        printf("AEP: Message length is too long\n");
        return -1;
    }

    /* validate input params */
    if (res_length > AEP_NP_MAILBOX_SIZE)
    {
        printf("AEP: Output message length is too long\n");
        return AEP_STATUS_NOK;
    }

    mask = aep_swap_dword(mask); // Convert to Little Endian

    /* wait for AEP to be 'ready' and 'done' */
    //printf("[debug]: aep_ipc_send_message: wait for AEP to be 'ready' and 'done'\n");
    timeout = MAX_TIMEOUT;
    while(( ( reg_read_32(AEP_REG_OFFSET_NP_PV_IPC_STATUS) & mask ) != mask ) && (timeout != 0) )
    {
        DEBUGF("[aep debug]: reg = 0x%0.8X mask=0x%0.8X\n",reg_read_32(AEP_REG_OFFSET_NP_PV_IPC_STATUS),mask);
        udelay(1);
        timeout--;
    }

    /* Check exit condition */
    if (timeout == 0)
    {
        printf("AEP: Error - send message fail on time-out. AEP is not ready\n");
        return AEP_STATUS_NOK;
    }

    /* Copy the message payload to mailbox*/
    if ((message != NULL) && (msg_length > 0))
    {
        msg_length = (msg_length + 3) /4; 
        offset_start = (unsigned int *)AEP_REG_OFFSET_NP_PV_MAILBOX;
        offset_end = offset_start + msg_length;
        //printf("[debug]: Copy the message payload to mailbox (offset_start=0x%X, offset_end=0x%X, msg_length=%d\n",offset_start,offset_end,msg_length);
        while(offset_start < offset_end)
        {
            //printf("[debug]: write 0x%X to 0x%X\n", *message, offset_start);
            reg_write_32(offset_start, *message);
            offset_start++;
            message++;
        }
    }

    /* Send the command */
    //printf("[debug]: aep_ipc_send_message: Send the command (cmd id = %d)\n", cmd_id);
    reg_write_32(AEP_REG_OFFSET_NP_PV_DOORBELL, aep_swap_dword(cmd_id));   /* Clear the 'Ready' and 'Done' bits, and trigger interrupt on other AEP F/W, */

  
    /* wait for ARM to be 'not ready' and 'not done', indicate that a new command is waiting to be read in the doorbell */
    //printf("[debug]: aep_ipc_send_message: wait for ARM to be 'not ready' and 'not done'\n", cmd_id);
    timeout = MAX_TIMEOUT;
    while( ( ( reg_read_32(AEP_REG_OFFSET_NP_IPC_STATUS) & mask ) != 0 )  && (timeout != 0) )
    {
        udelay(1);
        timeout--;
    }

    /* Check exit condition */
    if (timeout == 0)
    {
        printf("AEP: Error - send message fail on time-out. Never got response\n");
        return AEP_STATUS_NOK;
    }

    /* Read command from doorbell */
    rcev_cmd_id = reg_read_32(AEP_REG_OFFSET_NP_DOORBELL);  /* This set the ARM 'ready' to '1' */
    rcev_cmd_id = aep_swap_dword(rcev_cmd_id); // Convert to Big Endian
    //printf("[debug]: aep_ipc_send_message: Read command from doorbell %X", rcev_cmd_id);
 
 
    if ((rcev_cmd_id & AEP_IPC_CMD_ID_COMPLETE_FLAG) != AEP_IPC_CMD_ID_COMPLETE_FLAG) 
    {
        printf("Error: did not get 'Complete' message");
        /* perform dummy process - and return error status, and wait to 'complete' response again*/
    }

    if ((rcev_cmd_id & (~AEP_IPC_CMD_ID_COMPLETE_FLAG)) != cmd_id) 
    {
        printf("Error: Error got unexpected response type (%d) - expected to (%d)\n",rcev_cmd_id,cmd_id);
    }

    /* Read the response payload from the mailbox. and set 'Done' bit */
    if ((response != NULL) && (res_length > 0))
    {
        /* Copy the message */
        res_length = (res_length + 3) /4;
        offset_start = (unsigned int *)AEP_REG_OFFSET_NP_MAILBOX;
        offset_end = offset_start + res_length;
        DEBUGF("[debug]: Copy the message from mailbox to payload (offset_start=0x%X, offset_end=0x%X, res_length=%d\n",offset_start,offset_end,res_length);
        while(offset_start < offset_end)
        {
            *response = reg_read_32(offset_start);
            DEBUGF("[debug]: read 0x%X from 0x%X\n", *response, offset_start);
            offset_start++;
            response++;
        }
    }

    /* Set 'Done' bit */
    //printf("[debug]: aep_ipc_send_message:Set 'Done' bit.\n");
    reg_write_32(AEP_REG_OFFSET_NP_IPC_STATUS, aep_swap_dword(AEP_IPC_STATUS_DONE_MASK));  /* This set the ARM 'Done' to '1' */

    return AEP_STATUS_OK;
}

/* Endian swap for 32bits double word */
inline unsigned int aep_swap_dword(unsigned int x) 
{ 
    int swp = x;
    return ( ((swp&0x000000FF)<<24) + ((swp&0x0000FF00)<<8 ) +
             ((swp&0x00FF0000)>>8 ) + ((swp&0xFF000000)>>24) );
}

