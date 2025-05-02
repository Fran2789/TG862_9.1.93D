/*
 *
 * dbridge_hal_filter.h
 * Description:
 * Declaration of functions and types used in the Filter module.
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

#ifndef __DBRIDGE_HAL_FILTER_H_
#define __DBRIDGE_HAL_FILTER_H_


/**************************************************************************/
/*      INTERFACE  Defines and Structs                                    */
/**************************************************************************/


/**************************************************************************/
/*      INTERFACE VARIABLES (prefix with EXTERN)                          */
/**************************************************************************/


/**************************************************************************/
/*      INTERFACE FUNCTIONS Prototypes:                                   */
/**************************************************************************/

/*! \fn int HAL_MacDsUnicastDataFilterAdd( unsigned char *addr )
 *  \brief this function Add Unicast MAC address filter
 *  \brif existing cpe MAC that add from config file .
 *  \param[in] mac address.
 *  \param[out] hw filter index.    // ARRIS ADD
 *  \return OK or error status.
 */
int HAL_MacDsUnicastDataFilterAdd( unsigned char *addr, unsigned int *hwIndex );    // ARRIS MOD
/*! \fn int HAL_MacDsUnicastDataFilterDelete( unsigned char *addr )
 *  \brief this function delete Unicast MAC address filter
 *  \brif existing cpe MAC that add from config file .
 *  \param[in] mac address.
 *  \return OK or error status.
 */
int HAL_MacDsUnicastDataFilterDelete( unsigned char *addr );
// ARRIS ADD
/*! \fn int HAL_MacDsUnicastDataFilterDeleteAtIndex( unsigned int index )
 *  \brief this function deletes Unicast MAC address filter given it's index
 *  \param[in] index into hw table the filter is located.
 *  \return OK or error status.
 */
int HAL_MacDsUnicastDataFilterDeleteAtIndex(unsigned int index);

/*! \fn int HAL_MacDsUnicastDataFilterGetAtIndex( unsigned int index, unsigned char* address )
 *  \brief this function retrieves Unicast MAC address filter given it's index
 *  \param[in] index into hw table the filter is located.
 *  \param[out] mac address
 *  \return OK or error status.
 */
int HAL_MacDsUnicastDataFilterGetAtIndex( unsigned int entryIdx, unsigned char* macaddress );
// END ARRIS
/*! \fn int HAL_MacDsMulticastDataFilterAdd( unsigned char *addr )
 *  \brief this function add Multicast MAC address filter
 *  \brif existing cpe MAC that add from config file .
 *  \param[in] mac address.
 *  \return OK or error status.
 */
int HAL_MacDsMulticastDataFilterAdd( unsigned char  *addr );

/*! \fn int HAL_MacDsMulticastDataFilterDelete( unsigned char *addr )
 *  \brief this function delete Multicast MAC address filter
 *  \brif existing cpe MAC that add from config file .
 *  \param[in] mac address.
 *  \return OK or error status.
 */
int HAL_MacDsMulticastDataFilterDelete( unsigned char  *addr );

/*! \fn int HAL_MacDsMulticastDataFilterSearch( unsigned char *addr )
 *  \brief this function search for Multicast MAC address filter
 *  \brif existing cpe MAC that add from config file .
 *  \param[in] mac address.
 *  \return 0 - find or -1.
 */
int HAL_MacDsMulticastDataFilterSearch( unsigned char  *addr );

/*! \fn int HAL_MacDsMulticastDeleteAll(void)
 *  \brief this function delete all Multicast MAC address filter
 *  \brif existing cpe MAC that add from config file .
 *  \return OK or error status.
 */
int HAL_MacDsMulticastDeleteAll(void);


#endif
