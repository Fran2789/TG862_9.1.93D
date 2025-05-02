/*

ISAAC: a fast cryptographic random number generator
http://burtleburtle.net/bob/rand/isaacafa.html

------------------------------------------------------------------------------
rand.h: definitions for a random number generator
By Bob Jenkins, 1996, Public Domain
MODIFIED:
  960327: Creation (addition of randinit, really)
  970719: use context, not global variables, for internal state
  980324: renamed seed to flag
  980605: recommend RANDSIZL=4 for noncryptography.
  010626: note this is public domain
------------------------------------------------------------------------------

$Id: ruli_isaac.h,v 1.1 2003/05/07 15:40:18 evertonm Exp $

*/

typedef unsigned long int isaac_ub4;   /* unsigned 4-byte quantities */
#define ISAAC_UB4MAXVAL 0xffffffff
typedef signed long int isaac_sb4;
#define ISAAC_UB4BITS 32
#define ISAAC_SB4MAXVAL 0x7fffffff
typedef unsigned short int isaac_ub2;
#define ISAAC_UB2MAXVAL 0xffff
#define ISAAC_UB2BITS 16
typedef signed short int isaac_sb2;
#define ISAAC_SB2MAXVAL 0x7fff
typedef unsigned char isaac_ub1;
#define ISAAC_UB1MAXVAL 0xff
#define ISAAC_UB1BITS 8
typedef signed char isaac_sb1;   /* signed 1-byte quantities */
#define ISAAC_SB1MAXVAL 0x7f
typedef int isaac_word;  /* fastest type available */

#define isaac_bis(target,mask)  ((target) |=  (mask))
#define isaac_bic(target,mask)  ((target) &= ~(mask))
#define isaac_bit(target,mask)  ((target) &   (mask))

#ifndef min
# define min(a,b) (((a)<(b)) ? (a) : (b))
#endif /* min */

#ifndef max
# define max(a,b) (((a)<(b)) ? (b) : (a))
#endif /* max */

#ifndef align
# define align(a) (((ub4)a+(sizeof(void *)-1))&(~(sizeof(void *)-1)))
#endif /* align */

#ifndef abs
# define abs(a)   (((a)>0) ? (a) : -(a))
#endif

#define ISAAC_TRUE  1
#define ISAAC_FALSE 0
#define ISAAC_SUCCESS 0  /* 1 on VAX */

#ifndef ISAAC_RAND
#define ISAAC_RAND
#define ISAAC_RANDSIZL   (8)  /* I recommend 8 for crypto, 4 for simulations */
#define ISAAC_RANDSIZ    (1<<ISAAC_RANDSIZL)

/* context of random number generator */
struct isaac_randctx
{
  isaac_ub4 randcnt;
  isaac_ub4 randrsl[ISAAC_RANDSIZ];
  isaac_ub4 randmem[ISAAC_RANDSIZ];
  isaac_ub4 randa;
  isaac_ub4 randb;
  isaac_ub4 randc;
};
typedef struct isaac_randctx isaac_randctx;

/*
------------------------------------------------------------------------------
 If (flag==TRUE), then use the contents of randrsl[0..RANDSIZ-1] as the seed.
------------------------------------------------------------------------------
*/
void isaac_randinit(isaac_randctx *r, isaac_word flag);

void isaac(isaac_randctx *r);


/*
------------------------------------------------------------------------------
 Call isaac_rand(/o_ randctx *r _o/) to retrieve a single 32-bit random value
------------------------------------------------------------------------------
*/
#define isaac_rand(r) \
   (!(r)->randcnt-- ? \
     (isaac(r), (r)->randcnt=ISAAC_RANDSIZ-1, (r)->randrsl[(r)->randcnt]) : \
     (r)->randrsl[(r)->randcnt])

#endif  /* RAND */


