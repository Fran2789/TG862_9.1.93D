/***************************************************************************
**+----------------------------------------------------------------------+**
**|                                ****                                  |**
**|                                ****                                  |**
**|                                ******o***                            |**
**|                          ********_///_****                           |**
**|                           ***** /_//_/ ****                          |**
**|                            ** ** (__/ ****                           |**
**|                                *********                             |**
**|                                 ****                                 |**
**|                                  ***                                 |**
**|                                                                      |**
**|     Copyright (c) 2006-2007 Texas Instruments Incorporated           |**
**|                        ALL RIGHTS RESERVED                           |**
**|                                                                      |**
**| Permission is hereby granted to licensees of Texas Instruments       |**
**| Incorporated (TI) products to use this computer program for the sole |**
**| purpose of implementing a licensee product based on TI products.     |**
**| No other rights to reproduce, use, or disseminate this computer      |**
**| program, whether in part or in whole, are granted.                   |**
**|                                                                      |**
**| TI makes no representation or warranties with respect to the         |**
**| performance of this computer program, and specifically disclaims     |**
**| any responsibility for any damages, special or consequential,        |**
**| connected with the use of this program.                              |**
**|                                                                      |**
**+----------------------------------------------------------------------+**
***************************************************************************/
#ifndef _SYS_UTILS_H_
#define _SYS_UTILS_H_

#include <string.h>

#include "sys_ptypes.h"   /* for definition primitives types  */

/***************************************************************************/
/*! \file sys_utils.h
    \brief System general utilities and macros
****************************************************************************/


/*! \def UNALIGNED_TYPE(__type)
 *  \brief Packed structure for unaligned data of __type
 */
#define UNALIGNED_TYPE(__type) \
    typedef struct \
    { \
    	__type data; \
    } unaligned_##__type;

#pragma pack(1) /* Packed section */
UNALIGNED_TYPE(Uint16)
UNALIGNED_TYPE(Uint32)
UNALIGNED_TYPE(Int16)
UNALIGNED_TYPE(Int32)
#pragma pack() /* End of Packed section */

/*! \def GET_UNALIGNED_UINT(type, src_p)
 *  \brief Read unaligned data of type
 *  [in] unaligned_src_p - pointer to unaligned source data
 *  Return - aligned data
 */
#define GET_UNALIGNED_TYPE_FUNC(__type) \
    static inline __type GET_UNALIGNED_TYPE_FUNC_##__type(const void *unaligned_src_p) \
    { \
        __type v; \
        const unaligned_##__type *unaligned_src_ptr = unaligned_src_p; \
        \
        v = unaligned_src_ptr->data; \
        \
        return v; \
    }

/*! \def SET_UNALIGNED_UINT(type, dest_p, src_p)
 *  \brief Write unaligned data of type
 *  [out] unaligned_dest_p - pointer to unaligned destination data
 *  [in] src_p - pointer to source data
 *  Return - aligned data
 */
#define SET_UNALIGNED_TYPE_FUNC(__type) \
    static inline void SET_UNALIGNED_TYPE_FUNC_##__type(void *unaligned_dest_p, const __type *src_p) \
    { \
        unaligned_##__type *unaligned_dest_ptr = unaligned_dest_p; \
        \
        unaligned_dest_ptr->data = *src_p; \
    }

/* Funcs */
GET_UNALIGNED_TYPE_FUNC(Uint16)
GET_UNALIGNED_TYPE_FUNC(Uint32)
GET_UNALIGNED_TYPE_FUNC(Int16)
GET_UNALIGNED_TYPE_FUNC(Int32)
SET_UNALIGNED_TYPE_FUNC(Uint16)
SET_UNALIGNED_TYPE_FUNC(Uint32)
SET_UNALIGNED_TYPE_FUNC(Int16)
SET_UNALIGNED_TYPE_FUNC(Int32)

/*! \def GET_UNALIGNED_U/INT16/32
 *  \brief Read unaligned data of Ui/Int16/32 type
 */
#define GET_UNALIGNED_UINT16(p) GET_UNALIGNED_TYPE_FUNC_Uint16(p)
#define GET_UNALIGNED_UINT32(p) GET_UNALIGNED_TYPE_FUNC_Uint32(p)
#define GET_UNALIGNED_INT16(p) GET_UNALIGNED_TYPE_FUNC_Int16(p)
#define GET_UNALIGNED_INT32(p) GET_UNALIGNED_TYPE_FUNC_Int32(p)

/*! \def SET_UNALIGNED_U/INT16/32
 *  \brief Write unaligned data of Uint16 type
 */
#define SET_UNALIGNED_UINT16(pdst, psrc) SET_UNALIGNED_TYPE_FUNC_Uint16(pdst, psrc)
#define SET_UNALIGNED_UINT32(pdst, psrc) SET_UNALIGNED_TYPE_FUNC_Uint32(pdst, psrc)
#define SET_UNALIGNED_INT16(pdst, psrc) SET_UNALIGNED_TYPE_FUNC_Int16(pdst, psrc)
#define SET_UNALIGNED_INT32(pdst, psrc) SET_UNALIGNED_TYPE_FUNC_Int32(pdst, psrc)

/*! \def __CHECKPOINT__ 
 *  \brief debugging macro that prints file, function and line
 */
#define __CHECKPOINT__ printf("CHECKPOINT: %s/%s():%d\n",__FILE__,__FUNCTION__,__LINE__)

/*! \def ARR_LEN(x)
 *  \brief macro for array length
 */
#define ARR_LEN(x)  ( sizeof(x) / sizeof( x[0] ) )

/*! \def BIT_MASK(x)
 *  \brief Calculate a mask according to the bit number
 */
#define BIT_MASK(x)  ( 1 << ( x ) )

#endif /* _SYS_UTILS_H_ */
