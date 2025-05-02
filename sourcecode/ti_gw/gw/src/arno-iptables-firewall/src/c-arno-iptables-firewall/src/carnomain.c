/* 
  GPL LICENSE SUMMARY

  Copyright(c) 2013-2013 Intel Corporation.

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

#define _CARNOMAIN_C_

/*! \file carnomain.c
    \brief Glue the arno script to the C translation
*/

/**************************************************************************/
/*      INCLUDES:                                                         */
/**************************************************************************/

#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <sys/time.h>       /* ARRIS ADD - for profiling    */
#include <autoconf.h>
#include "carnoutils.h"

/**************************************************************************/
/*      EXTERNS Declaration:                                              */
/**************************************************************************/

/**************************************************************************/
/*      DEFINES:                                                          */
/**************************************************************************/
/*! \def TI_GW_FILE
    \brief Add perfix for file names
    \param[in] _file: Filename without prefix */
#if defined(CONFIG_TI_SEPARATE_FS_ENABLE) && defined(CONFIG_TI_GW_SEPARATE_FS_ENABLE)
    #define TI_GW_FILE(_file) CONFIG_TI_SEPARATE_FS_ROOT_NAME CONFIG_TI_GW_SEPARATE_FS_NAME _file 
#else
    #define TI_GW_FILE(_file) _file 
#endif


#define AIF_FUNCS \
    AIF_FUNC(config_check) \
    AIF_FUNC(init_firewall_chains) \
    AIF_FUNC(main_start_c) \
    AIF_FUNC(main_restart_c) \
    AIF_FUNC(reinit_firewall_chains) \
    AIF_FUNC(setup_default_policies) \
    AIF_FUNC(setup_firewall_rules) \
    AIF_FUNC(setup_hostblock_chain) \
    AIF_FUNC(setup_kernel_settings) \
    AIF_FUNC(setup_misc) \
    AIF_FUNC(show_applied) \
    AIF_FUNC(show_disabled) \
    AIF_FUNC(show_restart) \
    AIF_FUNC(show_start) \
    AIF_FUNC(show_stop) \
    AIF_FUNC(show_stop_blocked) \
    AIF_FUNC(stop_block_firewall) \
    AIF_FUNC(stop_firewall) \
    AIF_FUNC(show_status) \

/**************************************************************************/
/*      LOCAL DECLARATIONS:                                               */
/**************************************************************************/

/**************************************************************************/
/*      LOCAL VARIABLES:                                                  */
/**************************************************************************/

#define AIF_FUNC(__f) extern void __f(void);
AIF_FUNCS
#undef AIF_FUNC

#define AIF_FUNC(__f) { #__f, __f },
typedef struct
{
    char *name;
    void (*func)(void);
} AIF_funcs_t;
AIF_funcs_t AIF_funcs[] =
{
    AIF_FUNCS

    {NULL, NULL}
};
#undef AIF_FUNC

#ifdef SHOW_PROFILE
#define ARNO_PROFILE( x )   \
{                           \
    {char str[22]; struct timeval tv; struct tm *tmp; gettimeofday(&tv, NULL); tmp = localtime(&tv.tv_sec); strftime(str, sizeof(str), "%H:%M:%S", tmp); printf(">>>>>>> %s - %s.%03ld\n", #x, str, tv.tv_usec / 1000);}  \
    x();                                                                                                                                                                                                                                    \
    {char str[22]; struct timeval tv; struct tm *tmp; gettimeofday(&tv, NULL); tmp = localtime(&tv.tv_sec); strftime(str, sizeof(str), "%H:%M:%S", tmp); printf("<<<<<<< %s - %s.%03ld\n", #x, str, tv.tv_usec / 1000);}  \
}
#else
#define ARNO_PROFILE( x )   x()
#endif

extern int ip4tables_batchinit( char* filename );
extern int ip4tables_batchapply( char* filename );
extern int ip6tables_batchinit( char* filename );
extern int ip6tables_batchapply( char* filename );
#define IPV4_FWFORCERELOAD_FILE   "/var/tmp/ipv4_fw_forcereload.tmp" 
#define IPV6_FWFORCERELOAD_FILE   "/var/tmp/ipv6_fw_forcereload.tmp" 
#define IPV4_FWSTOPBLOCK_FILE   "/var/tmp/ipv6_fw_stopblock.tmp"
#define IPV6_FWSTOPBLOCK_FILE   "/var/tmp/ipv6_fw_stopblock.tmp"

/*SERCOMM ADD*/
ARNO_SETTING_TYPE_e g_Setting_RuleType = ARNO_SETTING_TYPE_ALL;
/*END SERCOMM ADD*/

ARNO_STATUS_TABLE_e g_Status_TableType = ARNO_STATUS_TABLE_ALL; //UNIHAN ADD

/**************************************************************************/
/*      INTERFACE FUNCTIONS Implementation:                               */
/**************************************************************************/

/**************************************************************************/
/*! \fn int main(int argc, char *argv[])
 **************************************************************************
 *  \brief Executed the requested function
 *  \param[in] argv[0] is the name of the function requested
 *  \return 0
 **************************************************************************/
#define FIREWALL_BASE_CFG TI_GW_FILE("/etc/arno-iptables-firewall/firewall.env")
#define FIREWALL_USER_CFG "/var/gw_arno_config"
#define FIREWALL_ENV_CFG  TI_GW_FILE("/usr/local/share/arno-iptables-firewall/cenvironment.env")

int main(int argc, char *argv[])
{
    /*SERCOMM ADD*/
    /*source ARNO config file, set config to env*/
    //printf("run source_env\n");
    source_env(FIREWALL_BASE_CFG);
    source_env(FIREWALL_USER_CFG);
    source_env(FIREWALL_ENV_CFG);
    /*END SERCOMM ADD*/
    int arg_cnt; //UNIHAN ADD
    /* ARRIS MOD BEGIN - Do all of the high level function in one run so:
     * - we can cache some parsed settings
     * - we don't keep running env_init()
     * more 'c' then bash
     */
    ARNO_PROFILE( env_init );

#if 0
    env_dump();
#endif

    if ((ARNO_IPTABLESBATCH_VERBOSE = getenv("ARNO_IPTABLESBATCH_VERBOSE")) == NULL)
    {
        ARNO_IPTABLESBATCH_VERBOSE = "";
    }

    /*SERCOMM ADD*/
    /*For Setting IPv4/IPv6 rules separately*/
    if ( argc < 3) {
        g_Setting_RuleType = ARNO_SETTING_TYPE_ALL;
    }
    else {
        if ( strcmp( argv[2], "ipv4" ) == 0 ) {
            g_Setting_RuleType = ARNO_SETTING_TYPE_IPV4;
        }
        else if( strcmp( argv[2], "ipv6" ) == 0 ) {
            g_Setting_RuleType = ARNO_SETTING_TYPE_IPV6;
        }
        else {
            g_Setting_RuleType = ARNO_SETTING_TYPE_ALL;
        }
    }
    /*END SERCOMM ADD*/

    if ( strcmp( argv[1], "start" ) == 0 ) {
        ARNO_PROFILE( show_start );
        ARNO_PROFILE( config_check );
        ARNO_PROFILE( main_start_c );
        ARNO_PROFILE( show_applied );
    }
    else if ( strcmp( argv[1], "restart" ) == 0 ) { /*same as start, now*/
        ARNO_PROFILE( show_start );
        ARNO_PROFILE( config_check );
        /*start will flush all rules, and re-set them, so we use it to re-start, too.*/
        /*Since it will flush all rules, so we don't need to call stop, too*/
            ARNO_PROFILE( main_start_c );
        ARNO_PROFILE( show_applied );
    } 
    else if ( strcmp( argv[1], "stop" ) == 0 ) {
        ARNO_PROFILE( show_stop );        
        ARNO_PROFILE( stop_firewall );        
        ARNO_PROFILE( show_disabled );
    }
    //UNIHAN ADD START
    else if ( strcmp( argv[1], "status" ) == 0 ) {
        arg_cnt = argc - 2;
        while (arg_cnt > 0)
        {
            if ( strcmp( argv[arg_cnt], "-t" ) == 0 )
            {
                if ( strcmp( argv[arg_cnt+1], "filter" ) == 0 ) 
                {
                    g_Status_TableType = ARNO_STATUS_TABLE_FILTER;
                }
                else if( strcmp( argv[arg_cnt+1], "nat" ) == 0 ) 
                {
                    g_Status_TableType = ARNO_STATUS_TABLE_NAT;
                }
                else if( strcmp( argv[arg_cnt+1], "mangle" ) == 0 ) 
                {
                    g_Status_TableType = ARNO_STATUS_TABLE_MANGLE;
                }
                else 
                {
                    g_Status_TableType = ARNO_STATUS_TABLE_ALL;
                } 
            }
            arg_cnt--;
        }
        ARNO_PROFILE( show_status );
    }
    //END UNIHAN ADD
    else {
        printf("\033[40m\033[1;31mERROR: Bad or missing parameter(s)\033[0m : %s\n", argv[1]);
    }
    

#if 0
    if ( strcmp( argv[1], "start_dostart" ) == 0 )
    {
        // user issued start with firewall off
        ARNO_PROFILE( show_start );
        ARNO_PROFILE( config_check );
        ARNO_PROFILE( main_start_c );            
        ARNO_PROFILE( show_applied );        
    }
    else if ( strcmp( argv[1], "start_dorestart" ) == 0 )
    {
        // user issued start with firewall on - do restart
        ARNO_PROFILE( show_start );
        ARNO_PROFILE( config_check );
        ARNO_PROFILE( main_restart_c );
        ARNO_PROFILE( show_applied );        
    }
    else if ( strcmp( argv[1], "start_dostopstart" ) == 0 )
    {
        // user issued start and there was an issue - full stop / start
        ARNO_PROFILE( show_start );
        ARNO_PROFILE( config_check );
        ARNO_PROFILE( stop_firewall );
        ARNO_PROFILE( main_start_c );        
        ARNO_PROFILE( show_applied );        
    }
    else if ( strcmp( argv[1], "restart_dostart" ) == 0 )
    {
        // user issued restart with firewall off
        ARNO_PROFILE( show_restart );
        ARNO_PROFILE( config_check );
        ARNO_PROFILE( main_start_c );            
        ARNO_PROFILE( show_applied );        
    }
    else if ( strcmp( argv[1], "restart_dorestart" ) == 0 )
    {
        // user issued restart with firewall on - do restart
        ARNO_PROFILE( show_restart );
        ARNO_PROFILE( config_check );
        ARNO_PROFILE( main_restart_c );
        ARNO_PROFILE( show_applied );        
    }
    else if ( strcmp( argv[1], "restart_dostopstart" ) == 0 )
    {
        // user issued start and there was an issue - full stop / start
        ARNO_PROFILE( show_restart );
        ARNO_PROFILE( config_check );
        ARNO_PROFILE( stop_firewall );
        ARNO_PROFILE( main_start_c );        
        ARNO_PROFILE( show_applied );        
    }
    else if ( strcmp( argv[1], "force-reload" ) == 0 )
    {
        ARNO_PROFILE( config_check );        
        ip4tables_batchinit(IPV4_FWFORCERELOAD_FILE);
        ip6tables_batchinit(IPV6_FWFORCERELOAD_FILE);
        ARNO_PROFILE( setup_hostblock_chain );        
        ip4tables_batchapply(IPV4_FWFORCERELOAD_FILE);
        ip6tables_batchapply(IPV6_FWFORCERELOAD_FILE);
        ARNO_PROFILE( show_applied );                
    }
    else if ( strcmp( argv[1], "stop" ) == 0 )
    {
        ARNO_PROFILE( show_stop );        
        ARNO_PROFILE( stop_firewall );        
        ARNO_PROFILE( show_disabled );                        
    }
    else if ( strcmp( argv[1], "stop-block" ) == 0 )
    {
        ARNO_PROFILE( show_stop );        
        ip4tables_batchinit(IPV4_FWSTOPBLOCK_FILE);
        ip6tables_batchinit(IPV6_FWSTOPBLOCK_FILE);
        ARNO_PROFILE( stop_block_firewall );        
        ip4tables_batchapply(IPV4_FWSTOPBLOCK_FILE);
        ip6tables_batchapply(IPV6_FWSTOPBLOCK_FILE);
        ARNO_PROFILE( show_stop_blocked );        
    }
    else if ( strcmp( argv[1], "check-conf" ) == 0 )
    {
        ARNO_PROFILE( config_check );        
    }
    else
    {
        printf("\033[40m\033[1;31mERROR: Bad or missing parameter(s)\033[0m : %s\n", argv[1]);
    }
    /* ARRIS MOD END */
#endif

    return 0;
}
