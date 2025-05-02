/* files.h */
//-------------------------------------------------------------------------------------
// Copyright 2006, Texas Instruments Incorporated
//
// This program has been modified from its original operation by Texas Instruments
// to do the following:
// 
// 1. Defined MAX_INTERFACES to support multiple lan interfaces
//
// THIS MODIFIED SOFTWARE AND DOCUMENTATION ARE PROVIDED
// "AS IS," AND TEXAS INSTRUMENTS MAKES NO REPRESENTATIONS 
// OR WARRENTIES, EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED 
// TO, WARRANTIES OF MERCHANTABILITY OR FITNESS FOR ANY 
// PARTICULAR PURPOSE OR THAT THE USE OF THE SOFTWARE OR 
// DOCUMENTATION WILL NOT INFRINGE ANY THIRD PARTY PATENTS, 
// COPYRIGHTS, TRADEMARKS OR OTHER RIGHTS. 
//
// These changes are covered as per original license.
//------------------------------------------------------------------------------------- 

#ifndef _FILES_H
#define _FILES_H

#define MAX_INTERFACES	8
#define MAX_CLASSES	1
#define MAX_KEYWORDS	19

#include "sys_types.h"

/* Change Description:07112006
 * 1. Modified functions to include classindex to indicate correct server_config
 */

/* Change Description:09252006
 * 1. Removed k_arr and kw_arr definitions
 */
#if defined(INCLUDE_ARRIS_GW_TR69) && !defined(CONFIG_VENDOR_ARRIS_CM_TR69)    
int read_tr69_runtime_config();
#endif

int read_config(char *file);
void write_leases(int ifid,int classindex);
void read_hosts(char *file, int ifid,int classindex);
int update_options(char *file);
Bool file_updated(char *filename, struct stat *curr_stat);
void read_vendor_options(void);
#endif
