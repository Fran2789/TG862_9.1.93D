/* 
 * leases.c -- tools to manage DHCP leases 
 * Russ Dill <Russ.Dill@asu.edu> July 2001
 */

/*-------------------------------------------------------------------------------------
// Copyright 2006, Texas Instruments Incorporated
//
// This program has been modified from its original operation by Texas Instruments
// to do the following:
//
// 1. Modification related support for per interface server_config
// 2. Added a new function write_to_delta() to update a text file (udhcpd.delta) 
//    when ever an IP is allocated or de-allocated to a client device. This allows 
//    udhcpd configuration application to monitor and save the IPs in use. Following 
//    key words are defined for this purpose : LEASE_ADD and LEASE_DEL.
// 3. Added a new text file udhcpd.host to allow consistency in host IP. 
// 4. ARRIS add a column to dhcpd.host and dhcpd.lease file, the new column saves the 
//    adapter type value identified when the device connected to gw through dhcp
// 5. Added deviceManufacturerOUI, deviceSerialNumber and deviceProductClass to 
//    dhcpd.host and dhcpd.lease file.
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
//-------------------------------------------------------------------------------------*/


#include <time.h>
#include <string.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>

#include "debug.h"
#include "dhcpd.h"
#include "files.h"
//UNIHAN MODIFY
#include "options.h"
//END UNIHAN MODIFY
#include "leases.h"
#include "arpping.h"
//ARRIS ADD START
#include <errno.h>
#include <sys/file.h>

//ARRIS ADD END


#define LEASE_DELTA_FILE "/var/tmp/udhcpd.delta"
#define LEASE_ADD		1
#define LEASE_DEL 	2
/*UNIHAN ADDED START*/
#ifdef MIB_arrisRouterLanCustom	
#define MaxLineSize 80
#define MaxMacSize 20
#define MaxFileline 256
#define MAXFrdName 64
#define NewName 7
#endif

/* Change Description:07112006
 * 1. Modified functions to include classindex to indicate correct server_config
 */

unsigned char blank_chaddr[] = {[0 ... 15] = 0};

/**************************************************************************/
/*! \fn  static void update_lease_file(char *macaddr, char *newline, char *filename)
 **************************************************************************
 *  \brief     Replace or delete a text line whose mac address is macaddr in the dhcp
               lease or host file
 *  \param[in]      char *macaddr  -- human readable hex-mac address str to locate the text line
 *  \param[in]      char *newline  -- replacement text line, NULL for delete the text line located with macaddr
 *  \param[in]      char *filename -- dhcp host/lease filename
 *  \return         void
 */    
static void update_lease_file(char *macaddr, char *newline, char *filename)
{
    FILE *fp;
    fp = fopen(filename, "r+");
    char *buff1 = NULL, *buff2 = NULL, line[256];
    long buff1_size, buff2_size, pos2;
    int val;
    if (!fp)
    {
        printf("dhcp error: failed to open file %s for read, %s\n", filename, strerror(errno));
        return;
    }

    flock(fileno(fp), LOCK_EX);

    buff1_size = 0;
    while (fgets(line, sizeof(line), fp))
    {
        if (strcasestr(line, macaddr))
        {
            pos2 = ftell(fp);
            fseek(fp, 0, SEEK_END);
            buff2_size = ftell(fp) - pos2;
            if (buff1_size)
            {
                buff1 = (char*)malloc(buff1_size);
                if (!buff1) goto finish_return;
            }
            if (buff2_size)
            {
                buff2 = (char*)malloc(buff2_size);
                if (!buff2) goto finish_return;
            }
            if(buff1_size)
            {
                fseek(fp, 0, SEEK_SET);
                val = fread(buff1, 1, buff1_size, fp);
            }
            if(buff2_size)
            {
                fseek(fp, pos2, SEEK_SET);
                val = fread(buff2, 1, buff2_size, fp);
            }

            fclose(fp);
            fp = fopen(filename, "w");
            if (!fp)
            {
                printf("dhcp error: failed to open %s for write, %s\n", filename, strerror(errno));
                goto finish_return;
            }
            if (buff1) fwrite(buff1, 1, buff1_size, fp);
            if (newline) fwrite(newline, 1, strlen(newline), fp);
            if (buff2) fwrite(buff2, 1, buff2_size, fp);
            goto finish_return;
        }
        buff1_size = ftell(fp);
    }

    //append newline to file
    if (newline) fwrite(newline, 1, strlen(newline), fp);
finish_return:
    if(buff1) free(buff1);
    if(buff2) free(buff2);
    if (fp) fclose(fp);
    return;
}

//ARRIS ADD END



/* clear every lease out that chaddr OR yiaddr matches and is nonzero */
void clear_lease(u_int8_t *chaddr, u_int32_t yiaddr, int ifid, int classindex)
{
	unsigned int i, j;
	
	for (j = 0; j < 16 && !chaddr[j]; j++);
	
	for (i = 0; i < server_config[ifid][classindex].max_leases; i++)
		if ((j != 16 && !memcmp(server_config[ifid][classindex].leases[i].chaddr, chaddr, 16)) ||
		    (yiaddr && server_config[ifid][classindex].leases[i].yiaddr == yiaddr)) {
			memset(&(server_config[ifid][classindex].leases[i]), 0, sizeof(struct dhcpOfferedAddr));
		}
}

/* ARRIS ADD START */
static char hex2char(int hex)
{
    if (hex >= 0 && hex <= 9) 
    {
        return ('0' + hex);
    }
    else if (hex >= 10 && hex <= 15)
    {
        return ('a' + hex - 10);
    }
    else 
    {
        return 'x';
    }
}
static char *str2hexstr(const char *str)
{
    int len, i;
    char *hexstr, *p;
    
    str = str ? str : "\0";
    len = strlen(str);
    
    hexstr = (char *)malloc(2 * len + 1);
    if (!hexstr)
    {
        return NULL;
    }

    p = hexstr;
    i = 0;
    while (str[i])
    {
        *p ++ = hex2char((str[i] & 0xf0) >> 4);
        *p ++ = hex2char(str[i] & 0x0f);
        ++ i;
    }
    *p = 0;
    return hexstr;    
}
/* ARRIS ADD END */

//UNIHAN MODIFY
#if defined(INCLUDE_ARRIS_GW_TR69) && !defined(CONFIG_VENDOR_ARRIS_CM_TR69)
void write_to_delta(u_int8_t *chaddr, u_int32_t yiaddr, u_int8_t *hname,unsigned long leasetime,u_int8_t action,int classindex, char *devicename, int dynamic, char *optionfile, struct dhcp_option125_info *opt125)
{
#else
void write_to_delta(u_int8_t *chaddr, u_int32_t yiaddr, u_int8_t *hname,unsigned long leasetime,u_int8_t action,int classindex, char *devicename, int dynamic, char *optionfile)
{
#endif //INCLUDE_ARRIS_GW_TR69
    //END UNIHAN MODIFY
    FILE  *fp;
    //UNIHAN MODIFY
#if defined(INCLUDE_ARRIS_GW_TR69) && !defined(CONFIG_VENDOR_ARRIS_CM_TR69)
    char line[ARRIS_ROUTER_LAN_DHCP_LEASE_OPT125_MAX_SIZE * 2]; // Make more space since we'll convert ascii string to hex string.
    struct dhcp_option125_info emptyopt = {"null", "null", "null"};

#else
    char line[256];
#endif
    int count = 0 ;
    char *ip;
    char *hexhname = NULL, *hexdname = NULL; // ARRIS ADD
    //unsigned long leasetime = 86400;
	struct in_addr addr;

	if (!(fp = fopen(LEASE_DELTA_FILE, "a"))) {// ARRIS ADD
		LOG(LOG_ERR, "Unable to open %s for writing", LEASE_DELTA_FILE);
		return;
	}

    flock(fileno(fp), LOCK_EX);
    
	memset(line , 0x00 , sizeof(line) );
    addr.s_addr = yiaddr;
    ip = inet_ntoa(addr);

    hexhname = str2hexstr(hname ? hname : "unknown");;
    hexdname = str2hexstr(devicename ? devicename : "unknown device");
    optionfile = optionfile ? optionfile : "null";
    
    // File format
    //<OPR> <MAC> <IP> <LEASE> <HOST NAME(in hex)> <DEVICE NAME(in hex)> <DYNAMIC> <CLASS INDEX> <OPTION FILE> <OUI> <SN> <PRODUCT CLASS>
//UNIHAN MODIFY
#if defined(INCLUDE_ARRIS_GW_TR69) && !defined(CONFIG_VENDOR_ARRIS_CM_TR69)
    opt125 = opt125 ? opt125 : &emptyopt;
    if(action == LEASE_ADD)
    {
        count = snprintf(line,sizeof(line),"%s %02x:%02x:%02x:%02x:%02x:%02x %s %ld %s %s %d %d %s %s %s %s\n",
                        "ADD",chaddr[0],chaddr[1],chaddr[2],chaddr[3],chaddr[4],chaddr[5],
                        ip,leasetime,hexhname, hexdname, dynamic, classindex, optionfile, 
                        opt125->deviceManufacturerOUI[0] ? opt125->deviceManufacturerOUI :"null",
                        opt125->deviceSerialNumber[0] ? opt125->deviceSerialNumber : "null",
                        opt125->deviceProductClass[0] ? opt125->deviceProductClass : "null");
    }
    else
    {
        count = snprintf(line,sizeof(line),"%s %02x:%02x:%02x:%02x:%02x:%02x %s %ld %s %s %d %d\n",
                        "DEL",chaddr[0],chaddr[1],chaddr[2],chaddr[3],chaddr[4],chaddr[5],
                        ip,leasetime,hexhname,hexdname,dynamic,classindex); //,opt125->deviceManufacturerOUI,opt125->deviceSerialNumber,opt125->deviceProductClass);
    }
#else
    if(action == LEASE_ADD)
        count = sprintf(line,"%s %02x:%02x:%02x:%02x:%02x:%02x %s %ld %s %s %d %d %s\n",
                        "ADD",chaddr[0],chaddr[1],chaddr[2],chaddr[3],chaddr[4],chaddr[5],
                        ip,leasetime,hexhname, hexdname, dynamic, classindex, optionfile);
    else
        count = sprintf(line,"%s %02x:%02x:%02x:%02x:%02x:%02x %s %ld %s %s %d %d\n",
                        "DEL",chaddr[0],chaddr[1],chaddr[2],chaddr[3],chaddr[4],chaddr[5],
                        ip,leasetime,hexhname,hexdname,dynamic,classindex);
#endif //INCLUDE_ARRIS_GW_TR69
//END UNIHAN MODIFY
    if (hexhname) free(hexhname);
    if (hexdname) free(hexdname);
// ARRIS remove worthless print // LOG(LOG_INFO, "%s",line);

  fwrite(line, sizeof(char), count, fp);
  fclose(fp);
  return;
}

static int zerohwaddr( u_int8_t *chaddr )
{
	int i;
  for(i=0;i<6;i++)
	{
		if( chaddr[i] != 0x00)
			return 0;
	}
	return 1;
}

/*add host parameters to host file*/
//UNIHAN MODIFY
#if defined(INCLUDE_ARRIS_GW_TR69) && !defined(CONFIG_VENDOR_ARRIS_CM_TR69)
/* Add deviceManufacturerOUI, deviceSerialNumber and deviceProductClass to host file. */
static void write_to_host(u_int8_t *chaddr, u_int32_t yiaddr, int ifid, int classindex, u_int8_t *hname, u_int32_t adptype, struct dhcp_option125_info *opt125)
{
#else
static void write_to_host(u_int8_t *chaddr, u_int32_t yiaddr, int ifid, int classindex, u_int8_t *hname, u_int32_t adptype) //ARRIS MOD
{
#endif //INCLUDE_ARRIS_GW_TR69
//END UNIHAN MODIFY
    FILE  *fp;
    char line[100]; //ARRIS MOD
	char macAddrStr[20];
    int count = 0 ;
    char *ip;
    struct in_addr addr;
    char bufline[256]; //ARRIS ADD

    memset(line , 0x00 , sizeof(line) );
    addr.s_addr = yiaddr;
    ip = inet_ntoa(addr);

#if defined(INCLUDE_ARRIS_GW_TR69) && !defined(CONFIG_VENDOR_ARRIS_CM_TR69)
    if (!opt125->deviceManufacturerOUI[0])
    {
        strcpy(opt125->deviceManufacturerOUI, ARRIS_NULL_STRING);
    }
    if (!opt125->deviceSerialNumber[0])
    {
        strcpy(opt125->deviceSerialNumber, ARRIS_NULL_STRING);
    }
    if (!opt125->deviceProductClass[0])
    {
        strcpy(opt125->deviceProductClass, ARRIS_NULL_STRING);
    }
#endif

    if(server_config[ifid][classindex].host_file)
    {
//UNIHAN MODIFY
#if defined(INCLUDE_ARRIS_GW_TR69) && !defined(CONFIG_VENDOR_ARRIS_CM_TR69)
        count = sprintf(line,"%02x:%02x:%02x:%02x:%02x:%02x %s %ld %s %u %s %s %s\n",
                        chaddr[0],chaddr[1],chaddr[2],chaddr[3],chaddr[4],chaddr[5],ip,0, (char*)hname, adptype, 
                        opt125->deviceManufacturerOUI, opt125->deviceSerialNumber, opt125->deviceProductClass);
#else
        //ARRIS MOD START
        count = sprintf(line,"%02x:%02x:%02x:%02x:%02x:%02x %s %ld %s %u\n",
                        chaddr[0],chaddr[1],chaddr[2],chaddr[3],chaddr[4],chaddr[5],ip,0, (char*)hname, adptype);
        //ARRIS MOD END
#endif //INCLUDE_ARRIS_GW_TR69
//END UNIHAN MODIFY
        
        sprintf(macAddrStr,"%02x:%02x:%02x:%02x:%02x:%02x",
                        chaddr[0],chaddr[1],chaddr[2],chaddr[3],chaddr[4],chaddr[5]);

        // UNIHAN MOD START, to update IP of the same MAC in host file
        if( fp = fopen(server_config[ifid][classindex].host_file, "r") )
        {
            char macAddrStr_old[20];
            char ip_old[15];
            unsigned int leaseTime = 0;
            char hName[50];
            //ARRIS ADD START
            unsigned int adptypeold;
            int ncols;
            char linestrnew[100];
            //ARRIS ADD END
//UNIHAN ADD
#if defined(INCLUDE_ARRIS_GW_TR69) && !defined(CONFIG_VENDOR_ARRIS_CM_TR69)
            struct dhcp_option125_info opt125Old;
#endif //INCLUDE_ARRIS_GW_TR69
//END UNIHAN ADD

            //ARRIS MOD START
            //while( EOF != (ncols = fscanf(fp, "%s %s %u %s %u\n", macAddrStr_old, ip_old, &leaseTime, hName, &adptypeold)))
            while (fgets(bufline, sizeof(bufline), fp))
            {
//UNIHAN MODIFY
#if defined(INCLUDE_ARRIS_GW_TR69) && !defined(CONFIG_VENDOR_ARRIS_CM_TR69)
                ncols = sscanf(bufline, "%s %s %u %s %u %s %s %s\n", macAddrStr_old, ip_old, &leaseTime, hName, &adptypeold, opt125Old.deviceManufacturerOUI, opt125Old.deviceSerialNumber, opt125Old.deviceProductClass);
#else
                ncols = sscanf(bufline, "%s %s %u %s %u\n", macAddrStr_old, ip_old, &leaseTime, hName, &adptypeold);
#endif //INCLUDE_ARRIS_GW_TR69
//END UNIHAN MODIFY

                if (ncols < DHCP_HOSTFILE_PARAMETERCOUNT_INCLUDE_ADPTYPE)//UNIHAN MODIFY
                {
                    /* Doesn't include adptype */
                    adptypeold = DHCP_LAN_CLIENT_UNKNOWN;
                }

//UNIHAN ADD
#if defined(INCLUDE_ARRIS_GW_TR69) && !defined(CONFIG_VENDOR_ARRIS_CM_TR69)
                if (ncols < DHCP_HOSTFILE_PARAMETERCOUNT_INCLUDE_DEVICEID)
                {
                    /* Doesn't include device manufacturer OUI, device serial number and device product class. */
                    strcpy(opt125Old.deviceManufacturerOUI, ARRIS_NULL_STRING);
                    strcpy(opt125Old.deviceSerialNumber, ARRIS_NULL_STRING);
                    strcpy(opt125Old.deviceProductClass, ARRIS_NULL_STRING);
                }
#endif //INCLUDE_ARRIS_GW_TR69
//END UNIHAN ADD
                /* MAC exist */                
                if( !strcmp(macAddrStr_old, macAddrStr) )
                {
                    fclose(fp);

                    /* update its IP if needed */
//UNIHAN MODIFY
#if defined(INCLUDE_ARRIS_GW_TR69) && !defined(CONFIG_VENDOR_ARRIS_CM_TR69)
                    if( strcmp(ip_old, ip) ||
                        (ncols < DHCP_HOSTFILE_PARAMETERCOUNT_INCLUDE_DEVICEID) ||
                        (adptypeold != adptype) ||
                        strcmp(opt125Old.deviceManufacturerOUI, opt125->deviceManufacturerOUI) ||
                        strcmp(opt125Old.deviceSerialNumber, opt125->deviceSerialNumber) ||
                        strcmp(opt125Old.deviceProductClass, opt125->deviceProductClass))
                    {
                        sprintf(linestrnew, "%s %s %u %s %u %s %s %s\n", macAddrStr, ip, leaseTime, hName, adptype, opt125->deviceManufacturerOUI, opt125->deviceSerialNumber, opt125->deviceProductClass);
                        update_lease_file(macAddrStr, linestrnew, server_config[ifid][classindex].host_file);
                    }
#else
                    if( strcmp(ip_old, ip) || (ncols < DHCP_HOSTFILE_PARAMETERCOUNT_INCLUDE_ADPTYPE) || (adptypeold != adptype) )
                    {
                        sprintf(linestrnew, "%s %s %u %s %u\n", macAddrStr, ip, leaseTime, hName, adptype);
                        update_lease_file(macAddrStr, linestrnew, server_config[ifid][classindex].host_file);
                        //sprintf(line, "sed -i 's/%s/%s/g' %s", ip_old, ip, server_config[ifid][classindex].host_file);
                        //system(line);
                        //ARRIS MOD END
                    }
#endif //INCLUDE_ARRIS_GW_TR69
//END UNIHAN MODIFY
                    return;
                }
            }
            fclose(fp);
        }

        /* MAC dosen't exist. Add a new host to the end of file */
        if( fp = fopen(server_config[ifid][classindex].host_file, "a+") )
        {
            fwrite(line, sizeof(char), count, fp);
            fclose(fp);
        }
        // UNIHAN MOD END
    }
    return;
}

/* UNIHAN ADD START */
#ifdef MIB_arrisRouterLanCustom	
/* To support LanCustom */
/*add host parameters to Custom*/
static void write_to_Custom(u_int8_t *chaddr, u_int32_t yiaddr, int ifid, int classindex, u_int8_t *hname,  char *Fname, char *Ccomments, char *MACfg)
{
    FILE  *fp;
    char line[MaxLineSize];
	char macAddrStr[MaxMacSize]; 
    char fileline[MaxFileline];
    int count = 0 ;
    char *ip;
    struct in_addr addr;

    memset(line , 0x00 , sizeof(line) );
    addr.s_addr = yiaddr;
    ip = inet_ntoa(addr);

    if(server_config[ifid][classindex].Custom_file)
    {
        sprintf(macAddrStr,"%02x:%02x:%02x:%02x:%02x:%02x",
                        chaddr[0],chaddr[1],chaddr[2],chaddr[3],chaddr[4],chaddr[5]);

        //assign default CustomFriendlyName= hostname : 1st hname:hostname, 2nd hname:fname
        count = sprintf(line,"%s %s %ld %s %s %s %s\n",
                        macAddrStr,ip,0,hname,hname,Ccomments,MACfg);

        if ((fp = fopen(server_config[ifid][classindex].Custom_file, "a+")))
        {
            while (fgets(fileline, MaxFileline, fp) != NULL)
            {                    
                if (strstr(fileline, macAddrStr))
                {    
                    fclose(fp);
                    return;
                }
            }     
            fwrite(line, sizeof(char), count, fp);
            fclose(fp);
        }
    }
    return;
}
#endif
/* UNIHAN ADD END */

/* add a lease into the table, clearing out any old ones */
//UNIHAN MODIFY
#if defined(INCLUDE_ARRIS_GW_TR69) && !defined(CONFIG_VENDOR_ARRIS_CM_TR69)
struct dhcpOfferedAddr *add_lease(u_int8_t *chaddr, u_int32_t yiaddr, unsigned long lease, int ifid,int classindex, u_int8_t *hname, unsigned int adptype, struct dhcp_option125_info *opt125)
{
#else
struct dhcpOfferedAddr *add_lease(u_int8_t *chaddr, u_int32_t yiaddr, unsigned long lease, int ifid,int classindex, u_int8_t *hname)//ARRIS MOD
{
#endif //INCLUDE_ARRIS_GW_TR69
//END UNIHAN MODIFY
	struct dhcpOfferedAddr *oldest;

	/* clean out any old ones */
    if( !zerohwaddr(chaddr ))
        clear_lease(chaddr, yiaddr, ifid, classindex);
		
	oldest = oldest_expired_lease(ifid,classindex);
	
	if (oldest) {
		memcpy(oldest->chaddr, chaddr, 16);
		strcpy(oldest->hostname, hname);
		oldest->yiaddr = yiaddr;
	if( lease != server_config[ifid][classindex].inflease_time )
    	oldest->expires = time(0) + lease;
    else
    	oldest->expires = server_config[ifid][classindex].inflease_time;
//UNIHAN MODIFY
#if defined(INCLUDE_ARRIS_GW_TR69) && !defined(CONFIG_VENDOR_ARRIS_CM_TR69)
        //write_to_delta(chaddr,yiaddr,hname,lease,LEASE_ADD,classindex, opt125);
        write_to_host(chaddr, yiaddr, ifid, classindex, hname, 0, opt125);
#else
        //write_to_delta(chaddr,yiaddr,hname,lease,LEASE_ADD,classindex);
        write_to_host(chaddr, yiaddr, ifid, classindex, hname, 0); //ARRIS MOD
#endif //INCLUDE_ARRIS_GW_TR69
//END UNIHAN MODIFY
	}
	return oldest;
}


/* true if a lease has expired */
int lease_expired(struct dhcpOfferedAddr *lease, int ifid, int classindex)
{
	if( lease->expires != server_config[ifid][classindex].inflease_time)
		return (lease->expires < (unsigned long) time(0));
	else
		return 0;
}	


/* Find the oldest expired lease, NULL if there are no expired leases */
struct dhcpOfferedAddr *oldest_expired_lease(int ifid,int classindex)
{
	struct dhcpOfferedAddr *oldest = NULL;
	unsigned long oldest_lease = time(0);
	unsigned int i;

	
	for (i = 0; i < server_config[ifid][classindex].max_leases; i++)
	{
		if( server_config[ifid][classindex].leases[i].expires == server_config[ifid][classindex].inflease_time)
			continue;

        if ((server_config[ifid][classindex].leases[i].expires == 0) &&
            (server_config[ifid][classindex].leases[i].yiaddr == 0))
        {
            oldest = &(server_config[ifid][classindex].leases[i]);
            return oldest;
        }
		else if (oldest_lease > server_config[ifid][classindex].leases[i].expires)
		{
			oldest_lease = server_config[ifid][classindex].leases[i].expires;
			oldest = &(server_config[ifid][classindex].leases[i]);
		}
	}
	return oldest;
		
}


/* Find the first lease that matches chaddr, NULL if no match */
struct dhcpOfferedAddr *find_lease_by_chaddr(u_int8_t *chaddr, int ifid, int classindex)
{
	unsigned int i;

	for (i = 0; i < server_config[ifid][classindex].max_leases; i++)
		if (!memcmp(server_config[ifid][classindex].leases[i].chaddr, chaddr, 16)) 
		{
		    if(reservedIp(server_config[ifid][classindex].static_leases, server_config[ifid][classindex].leases[i].yiaddr))
            {
				memset(&(server_config[ifid][classindex].leases[i]), 0, sizeof(struct dhcpOfferedAddr));
            }
			else
			{
			    return &(server_config[ifid][classindex].leases[i]);
			}
		}
	
	return NULL;
}


/* Find the first lease that matches yiaddr, NULL is no match */
struct dhcpOfferedAddr *find_lease_by_yiaddr(u_int32_t yiaddr, int ifid,int classindex)
{
	unsigned int i;

	for (i = 0; i < server_config[ifid][classindex].max_leases; i++)
		if (server_config[ifid][classindex].leases[i].yiaddr == yiaddr) return &(server_config[ifid][classindex].leases[i]);
	
	return NULL;
}

/*SERCOMM ADD*/
/*check the ip if in the static lease file*/
u_int32_t skip_lease_ip(u_int32_t addr, int ifid, int classindex) 
{
    struct static_lease *cur = NULL;
    int ret = 0;

    cur = server_config[ifid][classindex].static_leases;

    while(cur != NULL)
    {
        if (addr == cur->ip)
        {   
            ret = 1; 
            break; 
        }
        cur = cur->next;
    }
    return ret;
}
/*SERCOMM ADD END*/

/* find an assignable address, it check_expired is true, we check all the expired leases as well.
 * Maybe this should try expired leases by age... */
u_int32_t find_address(int check_expired, int ifid,int classindex) 
{
	u_int32_t addr, ret;
	struct dhcpOfferedAddr *lease = NULL;		

	addr = ntohl(server_config[ifid][classindex].start); /* addr is in host order here */
	for (;addr <= ntohl(server_config[ifid][classindex].end); addr++) {
		/* ie, 192.168.55.0 */
		if (!(addr & 0xFF)) continue;

		/* ie, 192.168.55.255 */
		if ((addr & 0xFF) == 0xFF) continue;

	        /*SERCOMM ADD*/
        	if (skip_lease_ip(addr, ifid, classindex) == 1) continue;
	        /*SERCOMM ADD END*/

		/* lease is not taken */
		ret = htonl(addr);
		if ((!(lease = find_lease_by_yiaddr(ret, ifid,classindex)) ||
		     /* or it expired and we are checking for expired leases */
			(check_expired  && lease_expired(lease,ifid,classindex))) &&
		     /* and it isn't on the network */
			!check_ip(ret, ifid, classindex)) {
			return ret;
			break;
		}
	}
	return 0;
}


/* check is an IP is taken, if it is, add it to the lease table */
int check_ip(u_int32_t addr, int ifid,int classindex)
{
    struct in_addr temp;
    unsigned char hwaddr[6];
//UNIHAN ADD
#if defined(INCLUDE_ARRIS_GW_TR69) && !defined(CONFIG_VENDOR_ARRIS_CM_TR69)
    struct dhcp_option125_info def_opt125;
#endif //INCLUDE_ARRIS_GW_TR69
//END UNIHAN ADD

	if (arpping(addr, server_config[ifid][classindex].server, server_config[ifid][classindex].arp, server_config[ifid][classindex].interface,hwaddr) == 0) {
		temp.s_addr = addr;
	 	LOG(LOG_INFO, "%s belongs to someone, reserving it for %ld seconds", 
			inet_ntoa(temp), server_config[ifid][classindex].conflict_time);
		//add_lease(blank_chaddr, addr, server_config[ifid].conflict_time, ifid, "unknown");
      printf("%x:%x:%x:%x:%x:%x\n",hwaddr[0],hwaddr[1],hwaddr[2],hwaddr[3],hwaddr[4],hwaddr[5]);
//UNIHAN MODIFY
#if defined(INCLUDE_ARRIS_GW_TR69) && !defined(CONFIG_VENDOR_ARRIS_CM_TR69)
        init_option125_info(&def_opt125);
		add_lease(hwaddr, addr, server_config[ifid][classindex].conflict_time, ifid, classindex, "unknown", 0, &def_opt125);
#else
		add_lease(hwaddr, addr, server_config[ifid][classindex].conflict_time, ifid, classindex, "unknown");//ARRIS MOD
#endif //INCLUDE_ARRIS_GW_TR69
//END UNIHAN MODIFY
		return 1;
	} else 
	{
		return 0;
	}
}

