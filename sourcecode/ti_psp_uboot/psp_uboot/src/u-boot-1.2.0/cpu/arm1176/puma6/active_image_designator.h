
/*
 *  active_image_designator.h
 *
 *  GPL LICENSE SUMMARY
 *
 *  Copyright(c) 2012 Intel Corporation. All rights reserved.
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
 */

#ifndef _ACTIVE_IMAGE_DESIGNATOR_H_
#define _ACTIVE_IMAGE_DESIGNATOR_H_


#define AID_IA_KERNEL     0
#define AID_IA_ROOT_FS    1
//#define AID_IA_VGW_FS     2   /* ARRIS REMOVE */
#define AID_IA_APP_FS     2     /* ARRIS ADD */
#define AID_ARM_KERNEL    3
#define AID_ARM_ROOT_FS   4
#define AID_ARM_GW_FS     5
#define AID_RSVD_6        6
#define AID_RSVD_7        7
#define AID_RSVD_8        8
#define AID_RSVD_9        9
#define AID_RSVD_10      10
#define AID_RSVD_11      11
#define AID_RSVD_12      12
#define AID_RSVD_13      13
#define AID_RSVD_14      14
#define AID_RSVD_15      15

#define AID_MAX_IMGAGES 16

// ARRIS ADD
#define     AID_STRUCTURE_VERSION    2      // Added a new field to AID that defines a structure version in case we add some type of future migration

#define     AID_ACTIVE_BANK_BIT      (1 << 0)
#define     AID_BANK1_ACTIVE         0
#define     AID_BANK2_ACTIVE         1
#define     AID_BANK1_VALID_BIT      (1 << 8)
#define     AID_BANK2_VALID_BIT      (1 << 9)
#define     AID_ACTIVE_BANK_MASK     AID_ACTIVE_BANK_BIT
#define     AID_VALID_BANKS_MASK     (AID_BANK1_VALID_BIT | AID_BANK2_VALID_BIT)

// The following is a structure that the Intel Atom team added to the AID in the CEFDK but didn't port to here.  I am doing it now since Arris is upgrading this sector again.
struct imageAttr
{
    unsigned int size[2];
} __attribute__ ((packed));
// END ARRIS ADD

// ARRIS CHANGE - DO NOT MERGE WITHOUT TALKING TO DEREK
typedef struct _active_image_designator {
    unsigned int crc32;                     /* crc32 of 'valid' and 'actimage' */
    unsigned int not_valid;                 /* state if this AID structure is valid or not. Valid=0  */
    unsigned int actimage[AID_MAX_IMGAGES]; /* the active image 0 or 1 */
    unsigned int aidVersion;                // Track AID revision
    unsigned int ipDownloadLock;            // Add a flag for Comcast's IP download mode
    unsigned int unused[AID_MAX_IMGAGES];    // Reserved for future Arris use
    struct imageAttr attr[AID_MAX_IMGAGES];
}active_image_designator;
// END ARRIS CHANGE

#endif /* _ACTIVE_IMAGE_DESIGNATOR_H_ */
