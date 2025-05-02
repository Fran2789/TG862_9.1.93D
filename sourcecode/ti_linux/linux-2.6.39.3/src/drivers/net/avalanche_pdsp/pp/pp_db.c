/*
  GPL LICENSE SUMMARY

  Copyright(c) 2014 Intel Corporation.

  This program is free software; you can redistribute it and/or modify
  it under the terms of version 2 of the GNU General Public License as
  published by the Free Software Foundation.

  This program is distributed in the hope that it will be useful, but
  WITHOUT ANY WARRANTY; without even the implied warranty of
  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
  General Public License for more details.

  You should have received a copy of the GNU General Public License
  along with this program; if not, write to the Free Software
  Foundation, Inc., 51 Franklin St - Fifth Floor, Boston, MA 02110-1301 USA.
  The full GNU General Public License is included in this distribution
  in the file called LICENSE.GPL.

  Contact Information:
    Intel Corporation
    2200 Mission College Blvd.
    Santa Clara, CA  97052
*/



#include <linux/kernel.h>
#include <linux/list.h>
#include <string.h>
#include <linux/semaphore.h>
#include <linux/slab.h>
#include <linux/errno.h>
#include <io.h>
#include "pp_db.h"

PP_DB_t     PP_DB;


#define FOR_ALL_SESSIONS(i)     for (i=0; i < AVALANCHE_PP_MAX_ACCELERATED_SESSIONS; i++)
#define FOR_ALL_VPIDS(i)        for (i=0; i < AVALANCHE_PP_MAX_VPID; i++)
#define FOR_ALL_PIDS(i)         for (i=0; i < AVALANCHE_PP_MAX_PID; i++)
#define FOR_ALL_LUT1(i)         for (i=0; i < AVALANCHE_PP_MAX_LUT1_KEYS; i++)

/**************************************************************************
 * FUNCTION NAME : AVALANCHE_PP_RET_e pp_db_init( void )
 **************************************************************************
 * DESCRIPTION   :
 *  This function initialized the PP DB.
 * RETURNS       :
 *      0  -   Success
 *      >0  -   Error
 **************************************************************************/
AVALANCHE_PP_RET_e  pp_db_init( void )
{
    int i,j;
    struct list_head *   pos = NULL;

    memset  ( &PP_DB, 0, sizeof(PP_DB) );

    printk(KERN_DEBUG " =====[ PP_DB is @ %p, total size %d ]==== \n", virt_to_phys(&PP_DB), sizeof(PP_DB));
    printk(KERN_DEBUG "  PP_DB_Session_Entry_t             ==> %08X @ %p\n", sizeof( PP_DB_Session_Entry_t            ), virt_to_phys( &PP_DB.repository_sessions  ));
    printk(KERN_DEBUG "  PP_DB_Session_LUT2_hash_entry_t   ==> %08X @ %p\n", sizeof( PP_DB_Session_LUT2_hash_entry_t  ), virt_to_phys( &PP_DB.repository_lut2_hash ));
    printk(KERN_DEBUG "  PP_DB_Session_LUT1_hash_entry_t   ==> %08X @ %p\n", sizeof( PP_DB_Session_LUT1_hash_entry_t  ), virt_to_phys( &PP_DB.repository_lut1_hash ));
    printk(KERN_DEBUG "  PP_DB_VPID_Entry_t                ==> %08X @ %p\n", sizeof( PP_DB_VPID_Entry_t               ), virt_to_phys( &PP_DB.repository_VPIDs     ));
    printk(KERN_DEBUG "  PP_DB_PID_Entry_t                 ==> %08X @ %p\n", sizeof( PP_DB_PID_Entry_t                ), virt_to_phys( &PP_DB.repository_PIDs      ));

    /* ========================================================================================= */
    /* Initialize ALL the list links for ALL entries */
    {
        FOR_ALL_SESSIONS(i)
        {
            INIT_LIST_HEAD( &PP_DB.repository_lut2_hash[i].link );
            INIT_LIST_HEAD( &PP_DB.repository_lut2_hash[i].lut2_hash_link );

            for (j=0; j < PP_LIST_ID_ALL; j++)
            {
                INIT_LIST_HEAD( &PP_DB.repository_sessions[i].list[j] );
            }

            INIT_LIST_HEAD( &PP_DB.lut2_hash[i] );
        }

        FOR_ALL_LUT1(i)
        {
            INIT_LIST_HEAD( &PP_DB.repository_lut1_hash[i].link );
            INIT_LIST_HEAD( &PP_DB.repository_lut1_hash[i].lut1_hash_link );

            INIT_LIST_HEAD( &PP_DB.lut1_hash[i] );
        }

        INIT_LIST_HEAD( &PP_DB.pool_lut1[ AVALANCHE_PP_SESSIONS_POOL_DATA  ][ PP_DB_POOL_FREE ] );
        INIT_LIST_HEAD( &PP_DB.pool_lut1[ AVALANCHE_PP_SESSIONS_POOL_DATA  ][ PP_DB_POOL_BUSY ] );
        INIT_LIST_HEAD( &PP_DB.pool_lut1[ AVALANCHE_PP_SESSIONS_POOL_VOICE ][ PP_DB_POOL_FREE ] );
        INIT_LIST_HEAD( &PP_DB.pool_lut1[ AVALANCHE_PP_SESSIONS_POOL_VOICE ][ PP_DB_POOL_BUSY ] );

        INIT_LIST_HEAD( &PP_DB.pool_lut2[ AVALANCHE_PP_SESSIONS_POOL_DATA  ][ PP_DB_POOL_FREE ] );
        INIT_LIST_HEAD( &PP_DB.pool_lut2[ AVALANCHE_PP_SESSIONS_POOL_DATA  ][ PP_DB_POOL_BUSY ] );
        INIT_LIST_HEAD( &PP_DB.pool_lut2[ AVALANCHE_PP_SESSIONS_POOL_VOICE ][ PP_DB_POOL_FREE ] );
        INIT_LIST_HEAD( &PP_DB.pool_lut2[ AVALANCHE_PP_SESSIONS_POOL_VOICE ][ PP_DB_POOL_BUSY ] );

        FOR_ALL_VPIDS(i)
        {
            INIT_LIST_HEAD( &PP_DB.repository_VPIDs[i].link );

            for (j=0; j < PP_LIST_ID_ALL; j++)
            {
                INIT_LIST_HEAD( &PP_DB.repository_VPIDs[i].list[j] );
            }
        }

        INIT_LIST_HEAD( &PP_DB.pool_VPIDs[ PP_DB_POOL_FREE ] );
        INIT_LIST_HEAD( &PP_DB.pool_VPIDs[ PP_DB_POOL_BUSY ] );

        INIT_LIST_HEAD( &PP_DB.pool_TDOX [ PP_DB_POOL_FREE ] );
        INIT_LIST_HEAD( &PP_DB.pool_TDOX [ PP_DB_POOL_BUSY ] );

        INIT_LIST_HEAD( &PP_DB.eventHandlers );
    }

    /* ========================================================================================= */

    FOR_ALL_SESSIONS(i)
    {
        PP_DB.repository_lut2_hash[i].handle = i;
    }
    FOR_ALL_LUT1(i)
    {
        PP_DB.repository_lut1_hash[i].handle = i;
    }
    FOR_ALL_VPIDS(i)
    {
        PP_DB.repository_VPIDs[i].handle = i;
    }


    for (i=0; i< (AVALANCHE_PP_MAX_LUT1_KEYS - AVALANCHE_PP_MAX_ACCELERATED_VOICE_SESSIONS/2); i++)
    {
        list_add( &PP_DB.repository_lut1_hash[i].link,
                  &PP_DB.pool_lut1[ AVALANCHE_PP_SESSIONS_POOL_DATA  ][ PP_DB_POOL_FREE ]  );
    }

    for (; i<AVALANCHE_PP_MAX_LUT1_KEYS; i++)
    {
        list_add( &PP_DB.repository_lut1_hash[i].link,
                  &PP_DB.pool_lut1[ AVALANCHE_PP_SESSIONS_POOL_VOICE  ][ PP_DB_POOL_FREE ]  );
    }

    for (i=0; i<AVALANCHE_PP_MAX_ACCELERATED_TDOX_SESSIONS; i++)
    {
        PP_DB.repository_TDOX[i].handle = i;
        list_add( &PP_DB.repository_TDOX[i].link, &PP_DB.pool_TDOX[ PP_DB_POOL_FREE ] );
    }

    for (i=0; i<(AVALANCHE_PP_MAX_ACCELERATED_SESSIONS - AVALANCHE_PP_MAX_ACCELERATED_VOICE_SESSIONS); i++)
    {
        list_add( &PP_DB.repository_lut2_hash[i].link,
                  &PP_DB.pool_lut2[ AVALANCHE_PP_SESSIONS_POOL_DATA ][ PP_DB_POOL_FREE ]  );
    }

    for (; i<(AVALANCHE_PP_MAX_ACCELERATED_SESSIONS); i++)
    {
        list_add( &PP_DB.repository_lut2_hash[i].link,
                  &PP_DB.pool_lut2[ AVALANCHE_PP_SESSIONS_POOL_VOICE ][ PP_DB_POOL_FREE ]  );
    }

    FOR_ALL_VPIDS(i)
    {
        list_add( &PP_DB.repository_VPIDs[i].link,  &PP_DB.pool_VPIDs[ PP_DB_POOL_FREE ] );
    }

    /* ========================================================================================= */
    atomic_set( &PP_DB.lock_nesting, 0 );
    /* ========================================================================================= */


    return (PP_RC_SUCCESS);
}

/**************************************************************************
 * FUNCTION NAME : AVALANCHE_PP_RET_e PP_DB_LOCK( void )
 **************************************************************************
 * DESCRIPTION   :
 *  This function locked the PP DB.
 * RETURNS       :
 *      0  -   Success
 *      >0  -   Error
 **************************************************************************/
AVALANCHE_PP_RET_e  PP_DB_LOCK( void )
{
    Uint32  state;
    local_irq_save( state );  

    if ( 1 == atomic_add_return( 1, &PP_DB.lock_nesting ) )
    {
        PP_DB.lock_state = state;  
    }
    return (PP_RC_SUCCESS);
}
/**************************************************************************
 * FUNCTION NAME : AVALANCHE_PP_RET_e PP_DB_UNLOCK( void )
 **************************************************************************
 * DESCRIPTION   :
 *  This function unlocked the PP DB.
 * RETURNS       :
 *      0  -   Success
 *      >0  -   Error
 **************************************************************************/

AVALANCHE_PP_RET_e  PP_DB_UNLOCK( void )
{
    if ( 0 == atomic_sub_return( 1, &PP_DB.lock_nesting ) )
    {
        local_irq_restore( PP_DB.lock_state );
    }
    return (PP_RC_SUCCESS);
}

#define hashsize(n) ((Uint32)1<<(n))
#define hashmask(n) (hashsize(n)-1)

/*
--------------------------------------------------------------------
mix -- mix 3 32-bit values reversibly.
For every delta with one or two bits set, and the deltas of all three
  high bits or all three low bits, whether the original value of a,b,c
  is almost all zero or is uniformly distributed,
* If mix() is run forward or backward, at least 32 bits in a,b,c
  have at least 1/4 probability of changing.
* If mix() is run forward, every bit of c will change between 1/3 and
  2/3 of the time.  (Well, 22/100 and 78/100 for some 2-bit deltas.)
mix() was built out of 36 single-cycle latency instructions in a
  structure that could supported 2x parallelism, like so:
      a -= b;
      a -= c; x = (c>>13);
      b -= c; a ^= x;
      b -= a; x = (a<<8);
      c -= a; b ^= x;
      c -= b; x = (b>>13);
      ...
  Unfortunately, superscalar Pentiums and Sparcs can't take advantage
  of that parallelism.  They've also turned some of those single-cycle
  latency instructions into multi-cycle latency instructions.  Still,
  this is the fastest good hash I could find.  There were about 2^^68
  to choose from.  I only looked at a billion or so.
--------------------------------------------------------------------
*/
#define mix(a,b,c) \
{ \
  a -= b; a -= c; a ^= (c>>13); \
  b -= c; b -= a; b ^= (a<<8); \
  c -= a; c -= b; c ^= (b>>13); \
  a -= b; a -= c; a ^= (c>>12);  \
  b -= c; b -= a; b ^= (a<<16); \
  c -= a; c -= b; c ^= (b>>5); \
  a -= b; a -= c; a ^= (c>>3);  \
  b -= c; b -= a; b ^= (a<<10); \
  c -= a; c -= b; c ^= (b>>15); \
}

/*
--------------------------------------------------------------------
hash() -- hash a variable-length key into a 32-bit value
  k       : the key (the unaligned variable-length array of bytes)
  len     : the length of the key, counting by bytes
  initval : can be any 4-byte value
Returns a 32-bit value.  Every bit of the key affects every bit of
the return value.  Every 1-bit and 2-bit delta achieves avalanche.
About 6*len+35 instructions.

The best hash table sizes are powers of 2.  There is no need to do
mod a prime (mod is sooo slow!).  If you need less than 32 bits,
use a bitmask.  For example, if you need only 10 bits, do
  h = (h & hashmask(10));
In which case, the hash table should have hashsize(10) elements.

If you are hashing n strings (Uint8 **)k, do it like this:
  for (i=0, h=0; i<n; ++i) h = hash( k[i], len[i], h);

By Bob Jenkins, 1996.  bob_jenkins@burtleburtle.net.  You may use this
code any way you wish, private, educational, or commercial.  It's free.

See http://burtleburtle.net/bob/hash/evahash.html
Use for hash table lookup, or anything where one collision in 2^^32 is
acceptable.  Do NOT use for cryptographic purposes.
--------------------------------------------------------------------
*/

/* The function takes the following parameters:-
 *  a) The key
 *  b) Length of the key
 *  c) the previous hash, or an arbitrary value */
Uint32 pp_db_hash( register Uint8 *k, register Uint32  length, register Uint32  initval )
{
   register Uint32 a,b,c,len;

   /* Set up the internal state */
   len = length;
   a = b = 0x9e3779b9;  /* the golden ratio; an arbitrary value */
   c = initval;         /* the previous hash value */

   /*---------------------------------------- handle most of the key */
   while (len >= 12)
   {
      a += (k[0] +((Uint32)k[1]<<8) +((Uint32)k[2]<<16) +((Uint32)k[3]<<24));
      b += (k[4] +((Uint32)k[5]<<8) +((Uint32)k[6]<<16) +((Uint32)k[7]<<24));
      c += (k[8] +((Uint32)k[9]<<8) +((Uint32)k[10]<<16)+((Uint32)k[11]<<24));
      mix(a,b,c);
      k += 12; len -= 12;
   }

   /*------------------------------------- handle the last 11 bytes */
   c += length;
   switch(len)              /* all the case statements fall through */
   {
   case 11: c+=((Uint32)k[10]<<24);
   case 10: c+=((Uint32)k[9]<<16);
   case 9 : c+=((Uint32)k[8]<<8);
      /* the first byte of c is reserved for the length */
   case 8 : b+=((Uint32)k[7]<<24);
   case 7 : b+=((Uint32)k[6]<<16);
   case 6 : b+=((Uint32)k[5]<<8);
   case 5 : b+=k[4];
   case 4 : a+=((Uint32)k[3]<<24);
   case 3 : a+=((Uint32)k[2]<<16);
   case 2 : a+=((Uint32)k[1]<<8);
   case 1 : a+=k[0];
     /* case 0: nothing left to add */
   }
   mix(a,b,c);

   /*-------------------------------------------- report the result */
   return c & hashmask(8);
}

