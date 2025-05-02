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
#ifndef XNVM_H_INCLUDED
#define XNVM_H_INCLUDED

void hal_pdi_init(void);
void hal_pdi_deinit(void);
int hal_pdi_write(const char* buffer, int size);
int hal_pdi_read(unsigned char *buffer, int size);
void hal_pdi_idle(void);
uint16_t readFlashCRC(void);




bool xnvm_init( void );
void xnvm_deinit( void );

bool nvm_chip_erase( void );

bool nvm_program_page(uint8_t const *page, uint16_t length, uint32_t address);
uint16_t nvm_set_FuseBytes(void);
uint16_t nvm_read_memory(uint8_t *data, uint16_t length, uint32_t address);
uint16_t nvm_read_memorySlow(uint8_t *data, uint16_t length, uint32_t address);
uint16_t   nvm_erase_appsec(void);
uint16_t   nvm_erase_bootloader(void);
void nvm_status_reg(void);
void hal_pdi_releaseprocessor(void);


#endif
