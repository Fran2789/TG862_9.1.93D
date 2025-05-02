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

#ifndef _PUMA6_AEP_H_
#define _PUMA6_AEP_H_

#define AEP_STATUS_OK       		0
#define AEP_STATUS_NOK      		-1
#define AEP_ACTIVE                  1
#define AEP_NOT_ACTIVE              0

/* initialize AEP driver
   Must be called at least once before any used of the function below.
   Alwasy return AEP_STATUS_OK
*/
int aep_init(void);

/* Check if AEP is active, or not.
   Can be called only after aep_init(void) had been called before.
   Return:
        AEP_ACTIVE      - AEP is ACTIVE
        AEP_NOT_ACTIVE  - AEP is NOT ACTIVE
*/
int aep_is_active(void);

/* Get AEP virtual eMMC controller base address.
   Return: Offset in RAM
*/
unsigned int aep_get_emmc_base_address(void);

/* Set Access for GPIO - For use in debug mode only, with Sepcial AEP Debug F/W.
   Set access atributes for each GPIO
   Input: GPIOs mask 1-enable access, 0-disable access
   Return:
        AEP_STATUS_OK   - Succeed
        AEP_STATUS_NOK  - Failure
*/
int aep_ipc_set_gpio_access(unsigned int gpio_0_to_31, unsigned int gpio_32_to_63, unsigned int gpio_64_to_95, unsigned int gpio_96_to_127);

/* Read from to GPIO register
   Input: Register offset and pointer to read value
   Return:
        AEP_STATUS_OK   - Succeed
        AEP_STATUS_NOK  - Failure
*/
int aep_ipc_gpio_read(unsigned int reg_offset, unsigned int* value);

/* Write to GPIO register
   The Driver will set only the valid bit in the mask according to the 'value'
   Input: Register offset and value and mask
   Return:
        AEP_STATUS_OK   - Succeed
        AEP_STATUS_NOK  - Failure
*/
int aep_ipc_gpio_write(unsigned int reg_offset, unsigned int value, unsigned int mask);


#endif /* _PUMA6_AEP_H_ */
