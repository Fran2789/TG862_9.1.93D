/*
 *  linux/lib/string.c
 *
 *  Copyright (C) 1991, 1992  Linus Torvalds
 */

/*
 * stupid library routines.. The optimized versions should generally be found
 * as inline code in <asm-xx/string.h>
 *
 * These are buggy as well..
 *
 * * Fri Jun 25 1999, Ingo Oeser <ioe@informatik.tu-chemnitz.de>
 * -  Added strsep() which will replace strtok() soon (because strsep() is
 *    reentrant and should be faster). Use only strsep() in new code, please.
 */

/* -------------------------------------------------------------------------------------
 * Copyright 2009, Texas Instruments Incorporated
 *
 * This program has been modified from its original operation by Texas Instruments
 * to do the following:
 *
 * 1. Added an optimized memmove function for Puma5 platform, which reads and writes
 *    data in blocks and not in bytes.
 *
 * THIS MODIFIED SOFTWARE AND DOCUMENTATION ARE PROVIDED
 * "AS IS," AND TEXAS INSTRUMENTS MAKES NO REPRESENTATIONS
 * OR WARRENTIES, EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED
 * TO, WARRANTIES OF MERCHANTABILITY OR FITNESS FOR ANY
 * PARTICULAR PURPOSE OR THAT THE USE OF THE SOFTWARE OR
 * DOCUMENTATION WILL NOT INFRINGE ANY THIRD PARTY PATENTS,
 * COPYRIGHTS, TRADEMARKS OR OTHER RIGHTS.
 *
 * These changes are covered as per original license.
 * ------------------------------------------------------------------------------------- */   

#include <linux/types.h>
#include <linux/string.h>
#include <linux/ctype.h>
#include <malloc.h>
#include <common.h>
#ifdef CONFIG_EXTENDED_FLASH
#include <serial_flash.h>
#endif
#if defined(CONFIG_USE_HW_MUTEX)
#include <puma6_hw_mutex.h>
#endif

#if defined(CONFIG_HARBORPARK)
void* aligned_memmove(void * dest,const void *src,size_t count)
{
    volatile char *dest_byte;
    volatile char *src_byte;
    volatile sfi_read_buf_t *src_blk;
	volatile sfi_read_buf_t *dest_blk;

#if defined(CONFIG_USE_HW_MUTEX)
    int mutex_on = 0;

    /* Check if we read from a memory ranges within the SPI Flash */
    if ( ((src  >= (void*)CFG_FLASH_BASE) && (src < ((void*)(CFG_FLASH_BASE + CFG_FLASH_SIZE))))  ||
         ((src  <  (void*)CFG_FLASH_BASE) && (src+count >= (void*)CFG_FLASH_BASE))              )
    {
        /* Lock the HW Mutex */
        if (hw_mutex_lock(HW_MUTEX_NOR_SPI) == 0)
        {
            printf("memmove: Failed to lock HW Mutex\n");
            return 0;
        }
        mutex_on = 1;
    }
#endif

	if (dest <= src) 
    {
        /* Move non-aligned data */ 
		dest_byte = (volatile char *) dest;
		src_byte = (volatile char *) src;
        /* if both pointers are not aligned, but have the same remainder, then we can copy and
               advance the pointers until we get alignment.*/
        if (((unsigned)dest_byte&0x03) == ((unsigned)src_byte&0x03))
        {
            /* Copy until we get alignment (up to 4 loop cycles)*/
            while (((((unsigned)dest_byte&0x03)!=0) && (((unsigned)src_byte&0x03)!=0)) && (count != 0))
            {
                *dest_byte++ = *src_byte++;
                count--;
            }
        }
        else
        {
            /* Copy all in bytes */
            while (count != 0)
            {
                *dest_byte++ = *src_byte++;
                count--;
            }
        }

        /* Move aligned data */
        dest_blk = (volatile sfi_read_buf_t *)(dest_byte);
        src_blk  =  (volatile sfi_read_buf_t *)(src_byte);
       	while( count>=SFI_BUF_SIZE ) 
        {
            *dest_blk++ = *src_blk++;
            count-=SFI_BUF_SIZE;
        }
        /* Move the the rest of non-aligned tail data */
        dest_byte = (char *) dest_blk;
		src_byte = (char *) src_blk;
        while(count != 0) 
        {
            *dest_byte++ = *src_byte++;
            count--;
        }
    }
	else 
    {
        dest_byte = (volatile char *) dest + count;
		src_byte = (volatile char *) src + count;
		/* if both pointers are not aligned, but have the same remainder, then we can copy and
               advance the pointers until we get alignment.*/
        if (((unsigned)dest_byte&0x03) == ((unsigned)src_byte&0x03))
        {
            /* Copy until we get alignment (up to 4 loop cycles)*/
            while (((((unsigned)dest_byte&0x03)!=0) && (((unsigned)src_byte&0x03)!=0)) && (count != 0))
            {
                *--dest_byte = *--src_byte;
                count--;
            }
        }
        else
        {
            /* Copy all in bytes */
            while (count != 0)
            {
                *--dest_byte = *--src_byte;
                count--;
            }
        }

        /* Move aligned data */
        dest_blk = (volatile sfi_read_buf_t *)(dest_byte);
        src_blk  =  (volatile sfi_read_buf_t *)(src_byte);
        while( count>=SFI_BUF_SIZE ) 
        {
            *--dest_blk = *--src_blk;
            count-=SFI_BUF_SIZE;
        }

         /* Move the non-aligned tail data */
        dest_byte = (volatile char *) dest_blk;
		src_byte = (volatile char *) src_blk;
        while(count != 0) 
		{
            *--dest_byte = *--src_byte;
            count--;
        }
	}
#if defined(CONFIG_USE_HW_MUTEX)
    /* Release HW Mutes */
    if (mutex_on == 1)
    {
        hw_mutex_unlock(HW_MUTEX_NOR_SPI);
        mutex_on = 0;
    }
#endif

    //printf("done\n");
	return dest;
}
#else 
#if defined CONFIG_TNETC550 
/* Hai: This code part is optimized for Puma5 Serial flash.
   It significantly reduces the read time by reading blocks of data and reducing read overhead. */

void * aligned_memmove(void * dest,const void *src,size_t count)
{
#ifdef CONFIG_EXTENDED_FLASH
    int switch_res_src, switch_dest;
    ulong retlen_src, retlen_dest;
#endif
	sfi_read_buf_t *src_blk = (sfi_read_buf_t *)((char *)src + count);
	sfi_read_buf_t *dest_blk = (sfi_read_buf_t *)((char *)dest + count);

	/* Read data in bursts */
	while( count>=SFI_BUF_SIZE ) {
#ifdef CONFIG_EXTENDED_FLASH
        switch_res_src = process_flash_bank(NULL, (char*)src_blk - SFI_BUF_SIZE, SFI_BUF_SIZE, &retlen_src, 1);
        if (switch_res_src == EMM_HIGH)
        {       
            src_blk = (sfi_read_buf_t *)((char*) src_blk - FLASH_BORDER_OFFSET_16MB);   
		    *--dest_blk = *--src_blk;
		    count-=SFI_BUF_SIZE;
            src_blk = (sfi_read_buf_t *)((char*) src_blk + FLASH_BORDER_OFFSET_16MB);
        }
        else if (switch_res_src == EMM_LOW)
        {
            if (retlen_src != SFI_BUF_SIZE)
            {
                /*If we need to copy first part of data from low bank and 
                the rest from high bank, do it one by one byte */
                debug("%s:%d low bank contains %d bytes from %d\n", 
                            __FUNCTION__, __LINE__, retlen_src, SFI_BUF_SIZE);
                break;                    
            } 

            *--dest_blk = *--src_blk;
            count-=SFI_BUF_SIZE;  
        }
#else
		*--dest_blk = *--src_blk;
		count-=SFI_BUF_SIZE;
#endif
	}

	/* Read the last bytes */
	if(count) {
		char *tmp = (char *) dest_blk;
		char *s = (char *) src_blk;
	
		while(count--) {
#ifdef CONFIG_EXTENDED_FLASH
            switch_res_src = process_flash_bank(NULL, s - 1, 1, &retlen_src, 1);
            if (switch_res_src == EMM_HIGH)
            {                
                s -= FLASH_BORDER_OFFSET_16MB; 
			    *--tmp = *--s;
                s += FLASH_BORDER_OFFSET_16MB;   
            }
            else if (switch_res_src == EMM_LOW)
            {   
                *--tmp = *--s;   
            }
#else
			*--tmp = *--s;
#endif
		}
	}
	return dest;
}

#endif
#endif



#if 0 /* not used - was: #ifndef __HAVE_ARCH_STRNICMP */
/**
 * strnicmp - Case insensitive, length-limited string comparison
 * @s1: One string
 * @s2: The other string
 * @len: the maximum number of characters to compare
 */
int strnicmp(const char *s1, const char *s2, size_t len)
{
	/* Yes, Virginia, it had better be unsigned */
	unsigned char c1, c2;

	c1 = 0;	c2 = 0;
	if (len) {
		do {
			c1 = *s1; c2 = *s2;
			s1++; s2++;
			if (!c1)
				break;
			if (!c2)
				break;
			if (c1 == c2)
				continue;
			c1 = tolower(c1);
			c2 = tolower(c2);
			if (c1 != c2)
				break;
		} while (--len);
	}
	return (int)c1 - (int)c2;
}
#endif

char * ___strtok;

#ifndef __HAVE_ARCH_STRCPY
/**
 * strcpy - Copy a %NUL terminated string
 * @dest: Where to copy the string to
 * @src: Where to copy the string from
 */
char * strcpy(char * dest,const char *src)
{
	char *tmp = dest;

	while ((*dest++ = *src++) != '\0')
		/* nothing */;
	return tmp;
}
#endif

#ifndef __HAVE_ARCH_STRNCPY
/**
 * strncpy - Copy a length-limited, %NUL-terminated string
 * @dest: Where to copy the string to
 * @src: Where to copy the string from
 * @count: The maximum number of bytes to copy
 *
 * Note that unlike userspace strncpy, this does not %NUL-pad the buffer.
 * However, the result is not %NUL-terminated if the source exceeds
 * @count bytes.
 */
char * strncpy(char * dest,const char *src,size_t count)
{
	char *tmp = dest;

	while (count-- && (*dest++ = *src++) != '\0')
		/* nothing */;

	return tmp;
}
#endif

#ifndef __HAVE_ARCH_STRCAT
/**
 * strcat - Append one %NUL-terminated string to another
 * @dest: The string to be appended to
 * @src: The string to append to it
 */
char * strcat(char * dest, const char * src)
{
	char *tmp = dest;

	while (*dest)
		dest++;
	while ((*dest++ = *src++) != '\0')
		;

	return tmp;
}
#endif

#ifndef __HAVE_ARCH_STRNCAT
/**
 * strncat - Append a length-limited, %NUL-terminated string to another
 * @dest: The string to be appended to
 * @src: The string to append to it
 * @count: The maximum numbers of bytes to copy
 *
 * Note that in contrast to strncpy, strncat ensures the result is
 * terminated.
 */
char * strncat(char *dest, const char *src, size_t count)
{
	char *tmp = dest;

	if (count) {
		while (*dest)
			dest++;
		while ((*dest++ = *src++)) {
			if (--count == 0) {
				*dest = '\0';
				break;
			}
		}
	}

	return tmp;
}
#endif

#ifndef __HAVE_ARCH_STRCMP
/**
 * strcmp - Compare two strings
 * @cs: One string
 * @ct: Another string
 */
int strcmp(const char * cs,const char * ct)
{
	register signed char __res;

	while (1) {
		if ((__res = *cs - *ct++) != 0 || !*cs++)
			break;
	}

	return __res;
}
#endif

#ifndef __HAVE_ARCH_STRNCMP
/**
 * strncmp - Compare two length-limited strings
 * @cs: One string
 * @ct: Another string
 * @count: The maximum number of bytes to compare
 */
int strncmp(const char * cs,const char * ct,size_t count)
{
	register signed char __res = 0;

	while (count) {
		if ((__res = *cs - *ct++) != 0 || !*cs++)
			break;
		count--;
	}

	return __res;
}
#endif

#ifndef __HAVE_ARCH_STRCHR
/**
 * strchr - Find the first occurrence of a character in a string
 * @s: The string to be searched
 * @c: The character to search for
 */
char * strchr(const char * s, int c)
{
	for(; *s != (char) c; ++s)
		if (*s == '\0')
			return NULL;
	return (char *) s;
}
#endif

#ifndef __HAVE_ARCH_STRRCHR
/**
 * strrchr - Find the last occurrence of a character in a string
 * @s: The string to be searched
 * @c: The character to search for
 */
char * strrchr(const char * s, int c)
{
       const char *p = s + strlen(s);
       do {
	   if (*p == (char)c)
	       return (char *)p;
       } while (--p >= s);
       return NULL;
}
#endif

#ifndef __HAVE_ARCH_STRLEN
/**
 * strlen - Find the length of a string
 * @s: The string to be sized
 */
size_t strlen(const char * s)
{
	const char *sc;

	for (sc = s; *sc != '\0'; ++sc)
		/* nothing */;
	return sc - s;
}
#endif

#ifndef __HAVE_ARCH_STRNLEN
/**
 * strnlen - Find the length of a length-limited string
 * @s: The string to be sized
 * @count: The maximum number of bytes to search
 */
size_t strnlen(const char * s, size_t count)
{
	const char *sc;

	for (sc = s; count-- && *sc != '\0'; ++sc)
		/* nothing */;
	return sc - s;
}
#endif

#ifndef __HAVE_ARCH_STRDUP
char * strdup(const char *s)
{
	char *new;

	if ((s == NULL)	||
	    ((new = malloc (strlen(s) + 1)) == NULL) ) {
		return NULL;
	}

	strcpy (new, s);
	return new;
}
#endif

#ifndef __HAVE_ARCH_STRSPN
/**
 * strspn - Calculate the length of the initial substring of @s which only
 * 	contain letters in @accept
 * @s: The string to be searched
 * @accept: The string to search for
 */
size_t strspn(const char *s, const char *accept)
{
	const char *p;
	const char *a;
	size_t count = 0;

	for (p = s; *p != '\0'; ++p) {
		for (a = accept; *a != '\0'; ++a) {
			if (*p == *a)
				break;
		}
		if (*a == '\0')
			return count;
		++count;
	}

	return count;
}
#endif

#ifndef __HAVE_ARCH_STRPBRK
/**
 * strpbrk - Find the first occurrence of a set of characters
 * @cs: The string to be searched
 * @ct: The characters to search for
 */
char * strpbrk(const char * cs,const char * ct)
{
	const char *sc1,*sc2;

	for( sc1 = cs; *sc1 != '\0'; ++sc1) {
		for( sc2 = ct; *sc2 != '\0'; ++sc2) {
			if (*sc1 == *sc2)
				return (char *) sc1;
		}
	}
	return NULL;
}
#endif

#ifndef __HAVE_ARCH_STRTOK
/**
 * strtok - Split a string into tokens
 * @s: The string to be searched
 * @ct: The characters to search for
 *
 * WARNING: strtok is deprecated, use strsep instead.
 */
char * strtok(char * s,const char * ct)
{
	char *sbegin, *send;

	sbegin  = s ? s : ___strtok;
	if (!sbegin) {
		return NULL;
	}
	sbegin += strspn(sbegin,ct);
	if (*sbegin == '\0') {
		___strtok = NULL;
		return( NULL );
	}
	send = strpbrk( sbegin, ct);
	if (send && *send != '\0')
		*send++ = '\0';
	___strtok = send;
	return (sbegin);
}
#endif

#ifndef __HAVE_ARCH_STRSEP
/**
 * strsep - Split a string into tokens
 * @s: The string to be searched
 * @ct: The characters to search for
 *
 * strsep() updates @s to point after the token, ready for the next call.
 *
 * It returns empty tokens, too, behaving exactly like the libc function
 * of that name. In fact, it was stolen from glibc2 and de-fancy-fied.
 * Same semantics, slimmer shape. ;)
 */
char * strsep(char **s, const char *ct)
{
	char *sbegin = *s, *end;

	if (sbegin == NULL)
		return NULL;

	end = strpbrk(sbegin, ct);
	if (end)
		*end++ = '\0';
	*s = end;

	return sbegin;
}
#endif

#ifndef __HAVE_ARCH_STRSWAB
/**
 * strswab - swap adjacent even and odd bytes in %NUL-terminated string
 * s: address of the string
 *
 * returns the address of the swapped string or NULL on error. If
 * string length is odd, last byte is untouched.
 */
char *strswab(const char *s)
{
	char *p, *q;

	if ((NULL == s) || ('\0' == *s)) {
		return (NULL);
	}

	for (p=(char *)s, q=p+1; (*p != '\0') && (*q != '\0'); p+=2, q+=2) {
		char  tmp;

		tmp = *p;
		*p  = *q;
		*q  = tmp;
	}

	return (char *) s;
}
#endif

#ifndef __HAVE_ARCH_MEMSET
/**
 * memset - Fill a region of memory with the given value
 * @s: Pointer to the start of the area.
 * @c: The byte to fill the area with
 * @count: The size of the area.
 *
 * Do not use memset() to access IO space, use memset_io() instead.
 */
void * memset(void * s,int c,size_t count)
{
	char *xs = (char *) s;

	while (count--)
		*xs++ = c;

	return s;
}
#endif

#ifndef __HAVE_ARCH_BCOPY
/**
 * bcopy - Copy one area of memory to another
 * @src: Where to copy from
 * @dest: Where to copy to
 * @count: The size of the area.
 *
 * Note that this is the same as memcpy(), with the arguments reversed.
 * memcpy() is the standard, bcopy() is a legacy BSD function.
 *
 * You should not use this function to access IO space, use memcpy_toio()
 * or memcpy_fromio() instead.
 */
char * bcopy(const char * src, char * dest, int count)
{
	char *tmp = dest;

	while (count--)
		*tmp++ = *src++;

	return dest;
}
#endif

#ifndef __HAVE_ARCH_MEMCPY
/**
 * memcpy - Copy one area of memory to another
 * @dest: Where to copy to
 * @src: Where to copy from
 * @count: The size of the area.
 *
 * You should not use this function to access IO space, use memcpy_toio()
 * or memcpy_fromio() instead.
 */
void * memcpy(void * dest,const void *src,size_t count)
{
	char *tmp = (char *) dest, *s = (char *) src;

	while (count--)
		*tmp++ = *s++;

	return dest;
}
#endif

#ifndef __HAVE_ARCH_MEMMOVE
/**
 * memmove - Copy one area of memory to another
 * @dest: Where to copy to
 * @src: Where to copy from
 * @count: The size of the area.
 *
 * Unlike memcpy(), memmove() copes with overlapping areas.
 */
void * memmove(void * dest,const void *src,size_t count)
{
	char *tmp, *s;

#if defined(CONFIG_HARBORPARK)
    return aligned_memmove( dest,src,count );
#endif

	if (dest <= src) {
		tmp = (char *) dest;
		s = (char *) src;
		while (count--)
			*tmp++ = *s++;
		}
	else {
#if defined CONFIG_TNETC550
		/* This is the only use-case we take care in Puma5 */
        if( !((unsigned)(dest) & 3) && !((unsigned)(src) & 3) && !(count & 3) ) {
			return aligned_memmove( dest,src,count );
		}
#endif
		tmp = (char *) dest + count;
		s = (char *) src + count;
		while (count--)
			*--tmp = *--s;
		}

	return dest;
}
#endif

#ifndef __HAVE_ARCH_MEMCMP
/**
 * memcmp - Compare two areas of memory
 * @cs: One area of memory
 * @ct: Another area of memory
 * @count: The size of the area.
 */
int memcmp(const void * cs,const void * ct,size_t count)
{
	const unsigned char *su1, *su2;
	int res = 0;

	for( su1 = cs, su2 = ct; 0 < count; ++su1, ++su2, count--)
		if ((res = *su1 - *su2) != 0)
			break;
	return res;
}
#endif

#ifndef __HAVE_ARCH_MEMSCAN
/**
 * memscan - Find a character in an area of memory.
 * @addr: The memory area
 * @c: The byte to search for
 * @size: The size of the area.
 *
 * returns the address of the first occurrence of @c, or 1 byte past
 * the area if @c is not found
 */
void * memscan(void * addr, int c, size_t size)
{
	unsigned char * p = (unsigned char *) addr;

	while (size) {
		if (*p == c)
			return (void *) p;
		p++;
		size--;
	}
	return (void *) p;
}
#endif

#ifndef __HAVE_ARCH_STRSTR
/**
 * strstr - Find the first substring in a %NUL terminated string
 * @s1: The string to be searched
 * @s2: The string to search for
 */
char * strstr(const char * s1,const char * s2)
{
	int l1, l2;

	l2 = strlen(s2);
	if (!l2)
		return (char *) s1;
	l1 = strlen(s1);
	while (l1 >= l2) {
		l1--;
		if (!memcmp(s1,s2,l2))
			return (char *) s1;
		s1++;
	}
	return NULL;
}
#endif

#ifndef __HAVE_ARCH_MEMCHR
/**
 * memchr - Find a character in an area of memory.
 * @s: The memory area
 * @c: The byte to search for
 * @n: The size of the area.
 *
 * returns the address of the first occurrence of @c, or %NULL
 * if @c is not found
 */
void *memchr(const void *s, int c, size_t n)
{
	const unsigned char *p = s;
	while (n-- != 0) {
		if ((unsigned char)c == *p++) {
			return (void *)(p-1);
		}
	}
	return NULL;
}

#endif
