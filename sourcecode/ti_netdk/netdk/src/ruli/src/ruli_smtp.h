/*-GNU-GPL-BEGIN-*
RULI - Resolver User Layer Interface - Querying DNS SRV records
Copyright (C) 2003 Everton da Silva Marques

RULI is free software; you can redistribute it and/or modify
it under the terms of the GNU General Public License as published by
the Free Software Foundation; either version 2, or (at your option)
any later version.

RULI is distributed in the hope that it will be useful,
but WITHOUT ANY WARRANTY; without even the implied warranty of
MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
GNU General Public License for more details.

You should have received a copy of the GNU General Public License
along with RULI; see the file COPYING.  If not, write to
the Free Software Foundation, Inc., 59 Temple Place - Suite 330,
Boston, MA 02111-1307, USA.
*-GNU-GPL-END-*/

/*
  $Id: ruli_smtp.h,v 1.2 2004/06/21 00:22:48 evertonm Exp $
  */


#ifndef RULI_SMTP_H
#define RULI_SMTP_H


#include <ruli_search.h>


ruli_search_srv_t *ruli_search_smtp_submit(ruli_res_t *resolver, 
					   void *(*call)
					   (ruli_search_srv_t *search, 
					    void *arg),
					   void *call_arg,
					   long options,
					   const char *txt_domain);


#endif /* RULI_SMTP_H */
