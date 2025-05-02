/* Copyright (c) 2008, Atmel Corporation All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions are met:
 *
 * 1. Redistributions of source code must retain the above copyright notice,
 * this list of conditions and the following disclaimer.
 *
 * 2. Redistributions in binary form must reproduce the above copyright notice,
 * this list of conditions and the following disclaimer in the documentation
 * and/or other materials provided with the distribution.
 *
 * 3. The name of ATMEL may not be used to endorse or promote products derived
 * from this software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY ATMEL "AS IS" AND ANY EXPRESS OR IMPLIED
 * WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED WARRANTIES OF
 * MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE EXPRESSLY AND
 * SPECIFICALLY DISCLAIMED. IN NO EVENT SHALL ATMEL BE LIABLE FOR ANY DIRECT,
 * INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES
 * (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES;
 * LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND
 * ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF
 * THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 ******************************************************************************/
 
//rms \#include <stdint.h>
//rms \#include <stdbool.h>

//#include <linux/config.h>
#include <linux/module.h>
#include <linux/moduleparam.h>
#include <linux/init.h>

#include <linux/kernel.h>	/* printk() */
#include <linux/slab.h>		/* kmalloc() */
#include <linux/fs.h>		/* everything... */
#include <linux/errno.h>	/* error codes */
#include <linux/types.h>	/* size_t */
#include <linux/byteorder/generic.h>
//rms 
//rms #include "compiler.h"
//rms #include "assert.h"
//rms 
//rms #include "hal_pdi.h"

#include "xnvm.h"


#define XNVM_PDI_LDS_INSTR    (0x00) //!< LDS instruction.
#define XNVM_PDI_STS_INSTR    (0x40) //!< STS instruction.
#define XNVM_PDI_LD_INSTR     (0x20) //!< LD instruction.
#define XNVM_PDI_ST_INSTR     (0x60) //!< ST instruction.
#define XNVM_PDI_LDCS_INSTR   (0x80) //!< LDCS instruction.
#define XNVM_PDI_STCS_INSTR   (0xC0) //!< STCS instruction.
#define XNVM_PDI_REPEAT_INSTR (0xA0) //!< REPEAT instruction.
#define XNVM_PDI_KEY_INSTR    (0xE0) //!< KEY instruction.

// added by rms
#define XOCD_RESET_SIGNATURE  (0x59)
                                                

/** Byte size address mask for LDS and STS instrcution */
#define XNVM_PDI_XXS_BYTE_ADDRESS_MASK (0x00)
/** Word size address mask for LDS and STS instrcution */
#define XNVM_PDI_XXS_WORD_ADDRESS_MASK (0x04)
/** 3 bytes size address mask for LDS and STS instrcution */
#define XNVM_PDI_XXS_3BYTES_ADDRESS_MASK (0x08)
/** Long size address mask for LDS and STS instrcution */
#define XNVM_PDI_XXS_LONG_ADDRESS_MASK (0x0C)
/** Byte size data mask for LDS and STS instrcution */
#define XNVM_PDI_XXS_BYTE_DATA_MASK (0x00)
/** Word size data mask for LDS and STS instrcution */
#define XNVM_PDI_XXS_WORD_DATA_MASK (0x01)
/** 3 bytes size data mask for LDS and STS instrcution */
#define XNVM_PDI_XXS_3BYTES_DATA_MASK (0x02)
/** Long size data mask for LDS and STS instrcution */
#define XNVM_PDI_XXS_LONG_DATA_MASK (0x03)

#define XNVM_CMD_NOP                         (0x00) //!< No Operation.
#define XNVM_CMD_CHIP_ERASE                  (0x40) //!< Chip Erase.
#define XNVM_CMD_READ_NVM_PDI                (0x43) //!< Read NVM PDI.
#define XNVM_CMD_LOAD_FLASH_PAGE_BUFFER      (0x23) //!< Load Flash Page Buffer.
#define XNVM_CMD_ERASE_FLASH_PAGE_BUFFER     (0x26) //!< Erase Flash Page Buffer.
#define XNVM_CMD_ERASE_FLASH_PAGE            (0x2B) //!< Erase Flash Page.
#define XNVM_CMD_WRITE_FLASH_PAGE            (0x02) //!< Flash Page Write.
#define XNVM_CMD_ERASE_AND_WRITE_FLASH_PAGE  (0x2F) //!< Erase & Write Flash Page.
#define XNVM_CMD_CALC_CRC_ON_FLASH           (0x78) //!< Flash CRC.

#define XNVM_CMD_ERASE_APP_SECTION           (0x20) //!< Erase Application Section.
#define XNVM_CMD_ERASE_APP_PAGE              (0x22) //!< Erase Application Section.
#define XNVM_CMD_WRITE_APP_SECTION           (0x24) //!< Write Application Section.
#define XNVM_CMD_ERASE_AND_WRITE_APP_SECTION (0x25) //!< Erase & Write Application Section Page.
#define XNVM_CMD_CALC_CRC_APP_SECTION        (0x38) //!< Application Section CRC.

#define XNVM_CMD_ERASE_BOOT_SECTION          (0x68) //!< Erase Boot Section.
#define XNVM_CMD_ERASE_BOOT_PAGE             (0x2A) //!< Erase Boot Loader Section Page.
#define XNVM_CMD_WRITE_BOOT_PAGE             (0x2C) //!< Write Boot Loader Section Page.
#define XNVM_CMD_ERASE_AND_WRITE_BOOT_PAGE   (0x2D) //!< Erase & Write Boot Loader Section Page.
#define XNVM_CMD_CALC_CRC_BOOT_SECTION       (0x39) //!< Boot Loader Section CRC.

#define XNVM_CMD_READ_USER_SIGN              (0x03) //!< Read User Signature Row.
#define XNVM_CMD_ERASE_USER_SIGN             (0x18) //!< Erase User Signature Row.
#define XNVM_CMD_WRITE_USER_SIGN             (0x1A) //!< Write User Signature Row.
#define XNVM_CMD_READ_CALIB_ROW              (0x02) //!< Read Calibration Row.

#define XNVM_CMD_READ_FUSE                   (0x07) //!< Read Fuse.
#define XNVM_CMD_WRITE_FUSE                  (0x4C) //!< Write Fuse.
#define XNVM_CMD_WRITE_LOCK_BITS             (0x08) //!< Write Lock Bits.

#define XNVM_CMD_LOAD_EEPROM_PAGE_BUFFER     (0x33) //!< Load EEPROM Page Buffer.
#define XNVM_CMD_ERASE_EEPROM_PAGE_BUFFER    (0x36) //!< Erase EEPROM Page Buffer.

#define XNVM_CMD_ERASE_EEPROM                (0x30) //!< Erase EEPROM.
#define XNVM_CMD_ERASE_EEPROM_PAGE           (0x32) //!< Erase EEPROM Page.
#define XNVM_CMD_WRITE_EEPROM_PAGE           (0x34) //!< Write EEPROM Page.
#define XNVM_CMD_ERASE_AND_WRITE_EEPROM      (0x35) //!< Erase & Write EEPROM Page.
#define XNVM_CMD_READ_EEPROM                 (0x06) //!< Read EEPROM.

#define XNVM_FLASH_BASE (0x0800000) //!< Adress where the flash starts.
#define XNVM_EPPROM_BASE (0x08C0000) //!< Address where eeprom starts.
#define XNVM_FUSE_BASE (0x08F0020) //!< Address where fuses start.
#define XNVM_DATA_BASE (0x1000000) //!< Address where data region starts.
#define XNVM_APPL_BASE (XNVM_FLASH_BASE) //!< Addres where application section starts.
#define XNVM_CALIBRATION_BASE (0x008E0200) //!< Address where calibration row starts.
#define XNVM_SIGNATURE_BASE (0x008E0400) //!< Address where signature bytes start.

#define SIZE_OF_FLASH_PART (0x4000)

#define XNVM_CONTROLLER_BASE (0x01C0)



#define XNVM_CONTROLLER_CMD_REG_OFFSET (0x0A)
#define XNVM_CONTROLLER_STATUS_REG_OFFSET (0x0F)

#define XNVM_CONTROLLER_CTRLA_REG_OFFSET (0x0B)
#define XNVM_CTRLA_CMDEX (1 << 0)
#define XNVM_NVMEN (1 << 1)
#define XNVM_NVM_BUSY (1 << 7)


/*! \brief Key used to enable the NVM interface. */
#define NVM_KEY_BYTE0 (0xFF)
#define NVM_KEY_BYTE1 (0x88)
#define NVM_KEY_BYTE2 (0xD8)
#define NVM_KEY_BYTE3 (0xCD)
#define NVM_KEY_BYTE4 (0x45)
#define NVM_KEY_BYTE5 (0xAB)
#define NVM_KEY_BYTE6 (0x89)
#define NVM_KEY_BYTE7 (0x12)



static uint8_t cmd_buffer[1024];
#define FUSE_BYTE_4_ADDR  (0x08f0024)
#define FUSE_BYTE_5_ADDR  (0x08f0025)
static uint8_t XNVM_CMD_FUSE_4[]    = {0x4C, 0x24, 0x00, 0x8F, 0x00, 0xF3};
static uint8_t XNVM_CMD_FUSE_5[]    = {0x4C, 0x25, 0x00, 0x8F, 0x00, 0xD4};
static uint8_t ERASE_APP_1[] =  {0x4C, 0xCA, 0x01, 0x00, 0x01, 0x20};
static uint8_t ERASE_APP_2[] =  {0x4C, 0x00, 0x00, 0x80, 0x00, 0xff};
static uint8_t ERASE_BOOT_1[] =  {0x4C, 0xCA, 0x01, 0x00, 0x01, 0x68};
static uint8_t ERASE_BOOT_2[] =  {0x4C, 0x00, 0x40, 0x80, 0x00, 0xff};


static bool volatile initialized = false;

static bool xnvm_read_status( uint8_t *status );
static bool xnvm_wait_for_nvmen( void );
static bool xnvm_write_nvm_controller_reg( uint8_t offset, uint8_t value );
static bool xnvm_read_nvm_controller_reg( uint8_t offset, uint8_t *value );
bool xnvm_set_and_verify_guard_time(void);




/*!
 *  This function is used to start the ATxmega's non volatile memory programming
 *  interface.
 *
 *  \retval true The memory interface was successfully started.
 *  \retval false Could not start the memory interface.
 */
bool xnvm_init( void )
{  
    /* Initialize the PDI. */
    hal_pdi_init();
#if 0
    /* 
        this changes the wait time between direction 
        changes on the data line. If you need to increase performance.  
    */

     do
     {
         /* this verifies that I can read and write.*/
         bActive = xnvm_set_and_verify_guard_time();
         if (bActive == false)
         {
             return bActive;
         }
     } while(0);
   
#endif
        /* Reset the device. */
        cmd_buffer[0] = XNVM_PDI_STCS_INSTR | 0x01;
        cmd_buffer[1] = XOCD_RESET_SIGNATURE;
        if (true != hal_pdi_write(cmd_buffer, 2)) {
                return false;
        }


      //  printk(" wrote reset\n");
    
        /* Load NWM key. */
        cmd_buffer[0] = XNVM_PDI_KEY_INSTR;
        cmd_buffer[1] = NVM_KEY_BYTE0;
        cmd_buffer[2] = NVM_KEY_BYTE1;
        cmd_buffer[3] = NVM_KEY_BYTE2;
        cmd_buffer[4] = NVM_KEY_BYTE3;
        cmd_buffer[5] = NVM_KEY_BYTE4;
        cmd_buffer[6] = NVM_KEY_BYTE5;
        cmd_buffer[7] = NVM_KEY_BYTE6;
        cmd_buffer[8] = NVM_KEY_BYTE7;
                
        if (true != hal_pdi_write(cmd_buffer, 9)) {
                return false;
        }

    
        /* Poll for the NVMEN in the PDI_STATUS register to be set. */
        
        initialized = xnvm_wait_for_nvmen();
        
        return initialized;
}


/*!
 *  This function releases the reset state.
 */
void xnvm_deinit( void )
{
    int ret;
    if (initialized == false) {
            return;
    }

    
    /* Load reset register with 0x00. */
    cmd_buffer[0] = XNVM_PDI_STCS_INSTR | 0x01;
    cmd_buffer[1] = 0x00;
    ret = (bool)hal_pdi_write(cmd_buffer, 2);   
    
    hal_pdi_deinit();
    
    initialized = false;
}


bool nvm_chip_erase( void )
{   
        /* Build the chip erase command and send it. */
        if (true != xnvm_write_nvm_controller_reg(XNVM_CONTROLLER_CMD_REG_OFFSET,\
                XNVM_CMD_CHIP_ERASE)) {
                return false;
        }
        
        /* Set the CMDEX bit in the NVM CTRLA register using the timed CCP sequence. */
        if (true != xnvm_write_nvm_controller_reg(XNVM_CONTROLLER_CTRLA_REG_OFFSET,\
                XNVM_CTRLA_CMDEX)) {
                return false;
        }
        
        /* Poll for the NVMEN in the PDI_STATUS register to be set. */
        return xnvm_wait_for_nvmen();
}


static bool store_pointer(uint32_t adr)
{
    unsigned int address =0;

    cmd_buffer[0] = XNVM_PDI_ST_INSTR | (0x02 << 2 | 0x03);

    address = cpu_to_le32(adr);

    memcpy((void *)(&(cmd_buffer[1])), (void *)(&address), sizeof(address));

    if (true != hal_pdi_write(cmd_buffer, 5)) {
            return false;
    }
    
    return true;
}


static bool store_to_pointer(uint8_t data)
{
        cmd_buffer[0] = XNVM_PDI_ST_INSTR | (0x01 << 2);
        cmd_buffer[1] = data;
        if (true != hal_pdi_write(cmd_buffer, 2)) {
                return false;
        }
        
        return true;
}

static bool set_repeat(uint16_t count)
{
    uint32_t number =0;
    --count;
    number = cpu_to_le32 (count);

    cmd_buffer[0] = XNVM_PDI_REPEAT_INSTR | (sizeof(number) -1);
    memcpy((void *)(&(cmd_buffer[1])), (void *)(&number), sizeof(number));
    if (true != hal_pdi_write(cmd_buffer, (sizeof(number) +1) ))  
    {
        return false;
    }



    return true;
}


void nvm_status_reg(void)
{
        uint8_t status;
        do {
                (bool)xnvm_read_nvm_controller_reg(XNVM_CONTROLLER_STATUS_REG_OFFSET, &status);
       // } while ((status & XNVM_NVM_BUSY) == XNVM_NVM_BUSY);
        }while (0);
}



static void nvm_wait_ready(void)
{
        uint8_t status;
        do {
                (bool)xnvm_read_nvm_controller_reg(XNVM_CONTROLLER_STATUS_REG_OFFSET, &status);
        } while ((status & XNVM_NVM_BUSY) == XNVM_NVM_BUSY);
}

static bool nvm_write_page_buffer(uint32_t address, uint8_t const *page, uint16_t length, uint8_t write_cmd)
{
        if (page == NULL) { return false; }
        if (length == 0) { return false; }
        if (xnvm_write_nvm_controller_reg(XNVM_CONTROLLER_CMD_REG_OFFSET, write_cmd) == false) { return false; }
        if (store_pointer(address) == false) { return false; }
        
        if (length > 1) {
                if (set_repeat(length) == false) { return false; }
        } else {
                if (store_to_pointer(*page) == false) { return false; };
                return true;
        }
        cmd_buffer[0] = XNVM_PDI_ST_INSTR | (0x01 << 2);
        memcpy((void *)(&(cmd_buffer[1])), (void *)(page), length);
        
        return hal_pdi_write(cmd_buffer, length + 1); 
}

bool nvm_program_pageBoot(uint8_t const *page, uint16_t length, uint32_t address)
{
        if (nvm_write_page_buffer(address, page, length, XNVM_CMD_LOAD_FLASH_PAGE_BUFFER) == false) { return false; }
        if (xnvm_write_nvm_controller_reg(XNVM_CONTROLLER_CMD_REG_OFFSET, \
                                XNVM_CMD_WRITE_BOOT_PAGE) == false) { return false; }
 
        /* Do dummy write. */
        if (store_pointer(address) == false) { return false; }
        if (store_to_pointer(0x55) == false) { return false; }
        
        nvm_wait_ready();
        
        return true;
}



bool nvm_program_page(uint8_t const *page, uint16_t length, uint32_t address)
{
    /* handle situation where we need to write to the boot sector*/
    if (address >= (XNVM_FLASH_BASE + SIZE_OF_FLASH_PART))
    {
        return nvm_program_pageBoot(page, length,address);

    }    

    if (nvm_write_page_buffer(address, page, length, XNVM_CMD_LOAD_FLASH_PAGE_BUFFER) == false) { return false; }
    if (xnvm_write_nvm_controller_reg(XNVM_CONTROLLER_CMD_REG_OFFSET, \
                            XNVM_CMD_WRITE_APP_SECTION ) == false) { return false; }
    
    /* Do dummy write. */
    if (store_pointer(address) == false) { return false; }
    if (store_to_pointer(0x55) == false) { return false; }
    
    nvm_wait_ready();
    
    return true;
}

uint16_t readFlashCRC(void)
{
    if (xnvm_write_nvm_controller_reg(XNVM_CONTROLLER_CMD_REG_OFFSET, XNVM_CMD_CALC_CRC_ON_FLASH) == false) {  return 0; }
    /* Set the CMDEX bit in the NVM CTRLA register using the timed CCP sequence. */
    if (true != xnvm_write_nvm_controller_reg(XNVM_CONTROLLER_CTRLA_REG_OFFSET,XNVM_CTRLA_CMDEX)) {return false;}

    /* Poll for the NVMEN in the PDI_STATUS register to be set. */
    return xnvm_wait_for_nvmen();


}


                                                                   
uint16_t nvm_read_memory(uint8_t *data, uint16_t length, uint32_t address)
{
        if (xnvm_write_nvm_controller_reg(XNVM_CONTROLLER_CMD_REG_OFFSET, XNVM_CMD_READ_NVM_PDI) == false) {  return 0; }
        if (store_pointer(address) == false) { return 0; }
        
        if (length > 1) 
        {
            if (set_repeat(length) == false) {  return 0; }
        }
                                         
        cmd_buffer[0] = XNVM_PDI_LD_INSTR | (0x01 << 2);   
        if (hal_pdi_write(cmd_buffer, 1) == false) {return 0; } 
        
        
        return hal_pdi_read(data, length); 
}

/*
3.19.2.8	Read and Set Fusebytes
1)	Use the STS instruction to load the NVM CMD register with .Read Fusebyte. command (0x07)
2)	Use the ST instruction to set the pointer to the start address of the Fusebyte 4.
3)	Use the Repeat instruction to repeat the next LD instruction 2 times.
4)	Use the LD instruction to receive 1 byte of data and increment the pointer.
5)	Receive 2 bytes of data (FB4 and FB5).
6)	If FB4=0xF3 and FB5=0xD4, then skip the following Fusebyte write operation. Otherwise, write Fusebyte values.
7)	Use the STS instruction (0x4C)to load the NVM CMD register with .Write Fusebye. command (0x4C)
8)	Use the STS instruction (0x4D)to write the fusebytes, FB4=0xF3 and FB5=0xD4
9)	Poll NVM STATUS until BUSY flag is cleared.
a.	Use the LDS instruction to read the NVM STATUS register.
b.	Repeat (a) until BUSY flag = 0.
*/
uint16_t nvm_set_FuseBytes()
{
        uint8_t data[4] ;

        if (xnvm_write_nvm_controller_reg(XNVM_CONTROLLER_CMD_REG_OFFSET, XNVM_CMD_READ_FUSE) == false) { return false; }
        if (store_pointer(FUSE_BYTE_4_ADDR) == false) { return 0; }
        if (set_repeat(2) == false) {  return 0; }       
        cmd_buffer[0] = XNVM_PDI_LD_INSTR | (0x01 << 2);   
        if (hal_pdi_write(cmd_buffer, 1) == false) { return 0; } 
        hal_pdi_read(data, 2);            
      
       if(data[0] != 0xF3) {
           if (xnvm_write_nvm_controller_reg(XNVM_CONTROLLER_CMD_REG_OFFSET, XNVM_CMD_WRITE_FUSE) == false) { return false; }
           if((hal_pdi_write(XNVM_CMD_FUSE_4, 6)) == false){ return 0;}         
           nvm_wait_ready();   
       }
       if(data[1] != 0xD4) {
           if (xnvm_write_nvm_controller_reg(XNVM_CONTROLLER_CMD_REG_OFFSET, XNVM_CMD_WRITE_FUSE) == false) { return false; } 
           if((hal_pdi_write(XNVM_CMD_FUSE_5, 6)) == false){ return 0;}         
           nvm_wait_ready();   
       }  
       return 1;        
}

/*
 *  uint16_t   nvm_erase_bootloader(void)
 *
 * Erase the boot section
 */
uint16_t   nvm_erase_bootloader(void)
{
           if((hal_pdi_write(ERASE_BOOT_1, 6)) == false){return 0;}  
           if((hal_pdi_write(ERASE_BOOT_2, 6)) == false){return 0;}  
           nvm_wait_ready();   
           return 1;
}
/*
 *  uint16_t   nvm_erase_appsec(void)
 *
 * Erase the application section
 */
uint16_t   nvm_erase_appsec(void)
{       
          if((hal_pdi_write(ERASE_APP_1, 6)) == false){ return 0;}  
          if((hal_pdi_write(ERASE_APP_2, 6)) == false){ return 0;}  
          nvm_wait_ready();   
          return 1;
}




static bool xnvm_read_status( uint8_t *status )
{
    int ret;

    cmd_buffer[0] = XNVM_PDI_LDCS_INSTR;
    if (true != hal_pdi_write(cmd_buffer,1 ))
    {
            return false;
    }
    
    ret = (uint16_t)hal_pdi_read(status, 1);
    
    return true;
}


/*!
 *  Writes the selected value to the NVM controller's register with given offset.
 *
 * \param offset Offset the register is located at.
 * \param value Value to be written.
 *
 * \retval true Value was successfully transmitted.
 * \retval false Value was not transmitted.
 */
static bool xnvm_write_nvm_controller_reg( uint8_t offset, uint8_t value )
{
    uint32_t register_address =0;

    cmd_buffer[0] = XNVM_PDI_STS_INSTR | XNVM_PDI_XXS_LONG_ADDRESS_MASK | XNVM_PDI_XXS_BYTE_DATA_MASK;

    if (cmd_buffer[0] != 0x4c)
    {
       // printk("incorrect value\n");
    }
    register_address = cpu_to_le32(XNVM_DATA_BASE + XNVM_CONTROLLER_BASE + offset);

    memcpy((void *)(&(cmd_buffer[1])), (void *)(&register_address), sizeof(register_address));
    cmd_buffer[5] = value;    
    return hal_pdi_write(cmd_buffer, 6);
}


static bool xnvm_read_nvm_controller_reg( uint8_t offset, uint8_t *value )
{
    int ret=0;

    uint32_t register_address = 0;
    cmd_buffer[0] = XNVM_PDI_LDS_INSTR | XNVM_PDI_XXS_LONG_ADDRESS_MASK | XNVM_PDI_XXS_BYTE_DATA_MASK;
    
    register_address = cpu_to_le32(XNVM_DATA_BASE + XNVM_CONTROLLER_BASE + offset);
    
    memcpy((void *)(&(cmd_buffer[1])), (void *)(&register_address), \
            sizeof(register_address));
    // arris remove cmd_buffer[5] = *value;    /* you don't need to send this.  */
    
    if (hal_pdi_write(cmd_buffer, /*6*/ 5) != true) {
            return false;
    }
    
    ret = (uint16_t)hal_pdi_read(value, 1);
    
    return true;
}


static bool xnvm_wait_for_nvmen( void )
{
        /* Poll for the NVMEN in the PDI_STATUS register to be set. */
        static uint8_t pdi_status;
        uint8_t retries = 0xFF;
        while (retries != 0)
        {
            if (true != xnvm_read_status(&pdi_status)) {
                    return false;
            }
    
            /* Check if the NVMEN bit is set in the PDI_STATUS register. */
            if ((pdi_status & XNVM_NVMEN) != 0) {
                    return true;
            }
    
           // --retries;  
        }
        return false;
}

bool xnvm_set_and_verify_guard_time(void)
{
    char value;
    int ret=0;

    value =0; /* get rid of compiler warning*/

    cmd_buffer[0] = XNVM_PDI_STCS_INSTR | 0x02;
    cmd_buffer[1] = 2;            /* write a value of 1 to the guard time register*/
    if (true != hal_pdi_write(cmd_buffer, 2))
    {
            return false;
    }


     
    /* read the  guard time regiser.*/
    cmd_buffer[0] = XNVM_PDI_LDCS_INSTR | 0x02;
    //cmd_buffer[1] = 0x00;        //ARRIS REMOVEd because you don't need to send an extra byte.
    if (true != hal_pdi_write(cmd_buffer, 1)) {
            return false;
    }

    ret = (uint16_t)hal_pdi_read(&value, 1);
    if (value !=2)
    {
      //printk(" value read from register is not correct : %d\n", value);
      return false;
    }
    return true;


}
/* EOF */
