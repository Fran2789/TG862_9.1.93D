/*
 
Copyright (c) 2013-2017 ARRIS Enterprises, LLC

All rights reserved.

Redistribution and use in source and binary forms, with or without 
modification, are permitted provided that the following conditions are met:

1. Redistributions of source code must retain the above copyright notice, 
   this list of conditions and the following disclaimer.
2. Redistributions in binary form must reproduce the above copyright notice, 
   this list of conditions and the following disclaimer in the documentation
   and/or other materials provided with the distribution.
3. The name of the author may not be used to endorse or promote products 
   derived from this software without specific prior written permission.


Alternatively, this software may be distributed under the terms of the
GNU General Public License ("GPL") version 2 as published by the Free
Software Foundation. 
 

THIS SOFTWARE IS PROVIDED BY THE AUTHOR ``AS IS'' AND ANY EXPRESS OR IMPLIED 
WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED WARRANTIES OF 
MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE DISCLAIMED. 
IN NO EVENT SHALL THE AUTHOR BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, 
SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, 
PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; 
OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, 
WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR 
OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF 
ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.

GPLv2 license:

This program is free software; you can redistribute it and/or modify it 
under the terms of the GNU General Public License as published by the 
Free Software Foundation; either version 2 of the License, or 
(at your option) any later version.

This program is distributed in the hope that it will be useful, but 
WITHOUT ANY WARRANTY; without even the implied warranty of MERCHANTABILITY 
or FITNESS FOR A PARTICULAR PURPOSE. See the GNU General Public License 
for more details.

You should have received a copy of the GNU General Public License along with 
this program; if not, write to the Free Software Foundation, Inc., 
51 Franklin Street, Fifth Floor, Boston, MA 02110-1301 USA.

////////////////////////////////////////////////////////////////////////////////
//
// arris_models.h     Definitions for the Arris HW types
//
////////////////////////////////////////////////////////////////////////////////
*/

#ifndef __ARRIS_MODELS_H__
#define __ARRIS_MODELS_H__

// WARNING -- This enum must match model_str array in arris_init.c!!
typedef enum
{
    WBM750A_HW_MODEL,
    WBM750B_HW_MODEL,
    WBM750C_HW_MODEL,
    WBM760A_HW_MODEL,
    WBM760B_HW_MODEL,
    WBM760C_HW_MODEL,
    TM702A_HW_MODEL,
    TM702B_HW_MODEL,
    TM702G_HW_MODEL,
    TM722A_HW_MODEL,
    TM722B_HW_MODEL,
    TM722G_HW_MODEL,
    TM722S_HW_MODEL,
    HW_MODEL8,      /* demarkation item; everything after this is model8 */
    TM802G_HW_MODEL,
    TM804G_HW_MODEL,
    CM820A_HW_MODEL,
    CM820B_HW_MODEL,
    CM820C_HW_MODEL,
    CM820S_HW_MODEL,
    CM820B_AU_HW_MODEL,
    TM822A_HW_MODEL,
    TM822B_HW_MODEL,
    TM822G_HW_MODEL,
    TM822R_HW_MODEL,
    TM822S_HW_MODEL,
    TG852G_HW_MODEL,
    DG860A_HW_MODEL,
    DG860B_HW_MODEL,
    DG860S_HW_MODEL,
    DG860P2_HW_MODEL,
    TG861A_HW_MODEL,
    TG862A_HW_MODEL,
    TG862B_HW_MODEL,
    TG862G_HW_MODEL,
    TG862R_HW_MODEL,
    TG862S_HW_MODEL,
    TG872G_HW_MODEL,
    HW_MODEL16,
    DG1660A_HW_MODEL,
    DG1670A_HW_MODEL,
    TG1642G_HW_MODEL,
    TG1652A_HW_MODEL,
    TG1652G_HW_MODEL,
    TG1652S_HW_MODEL,
    TG1662A_HW_MODEL,
    TG1662G_HW_MODEL,
    TG1662S_HW_MODEL,
    TG1672G_HW_MODEL,
    // below this line, all models use the new OrgName except for MG2402G
    TG1682G_HW_MODEL,
    TG1682GP2_HW_MODEL,
    TM1602A_HW_MODEL,
    TM1602G_HW_MODEL,
    TM1602B_HW_MODEL,
    TM1602AP2_HW_MODEL,
    TM1602GP2_HW_MODEL,
    OG1600A_HW_MODEL,
    DG1680A_HW_MODEL,
    TG1692A_HW_MODEL,
    TG1692S_HW_MODEL,
    SBG6950_HW_MODEL,
    HW_MODEL24,
    TG2482A_HW_MODEL,
    TG2482S_HW_MODEL,
    SBG7400_HW_MODEL,
    MG2402G_HW_MODEL,
    MG2400A_HW_MODEL,
    DG2400A_HW_MODEL,
    DG2460A_HW_MODEL,
    DG2460B_HW_MODEL,
    DG2470A_HW_MODEL,
    TG2472G_HW_MODEL,
    DG2490S_HW_MODEL,
    TG2492LG_HW_MODEL,
    TG2492S_HW_MODEL,
    HW_MODEL32,
    CM3200A_HW_MODEL,
    CM3200C_HW_MODEL,
    CM3200S_HW_MODEL,
    SB6190_HW_MODEL,
    TM3202A_HW_MODEL,
    TM3202G_HW_MODEL,
    DG3260A_HW_MODEL,
    DG3260B_HW_MODEL,
    DG3270A_HW_MODEL,
    DG3270B_HW_MODEL,
    SBG7580_HW_MODEL,
    TG3272G_HW_MODEL,
    TG3282G_HW_MODEL,
    HW_MAX_MODEL
// see warning above
} MAIN_HW_MODEL_t;

enum
{
    // see warning below
    ARRIS_HW_TYPE_WBM7x0 = 0x101,
    ARRIS_HW_TYPE_TM7x2,
    ARRIS_HW_TYPE_TG852,
    ARRIS_HW_TYPE_TG872,
    ARRIS_HW_TYPE_WBM750,
    ARRIS_HW_TYPE_WBM760,
    ARRIS_HW_TYPE_TG862_SERCOMM,
    ARRIS_HW_TYPE_TG862_INTEL,
    ARRIS_HW_TYPE_DG860,
    ARRIS_HW_TYPE_TG16XX,
    ARRIS_HW_TYPE_MG24XXV1,
    ARRIS_HW_TYPE_TG1682V1,   
    ARRIS_HW_TYPE_TM1602V1,
    ARRIS_HW_TYPE_TG1682,   
    ARRIS_HW_TYPE_TG24XX,
    ARRIS_HW_TYPE_OG1600,
    ARRIS_HW_TYPE_TM1602,
    ARRIS_HW_TYPE_MG24XX,
    ARRIS_HW_TYPE_TG1692,
    // add new entries immediately above this line, and this enum MUST be kept in sync with pt_table in arris_init.c!!!!
    // you also cannot change the order of any entries above    
    ARRIS_HW_TYPE_MAX_TYPES,
    ARRIS_HW_TYPE_RESERVED = 0xffff
} ARRIS_HW_TYPES;

#endif /* __ARRIS_MODELS_H__ */
