#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <stdlib.h>
#include <sys/time.h>
#include <string.h>
#if HAVE_STDINT_H
#include <stdint.h>
#endif
#include "statusq.h"
#include "range.h"
#include "list.h"
#include "errors.h"
#include "time.h"

int quiet=0;
#define NBT_DEBUG if(nbt_debug) fprintf
#define NBT_RESULT_FILE_PATH "/var/tmp/nbtlist"
#define NBT_RESULT_TMP_FILE_PATH "/var/tmp/nbtlist.tmp"
#define NBT_DEBUG_FILE "/var/log/nbt.log"
#define MAC_ADDR_STR_FORMAT "%02x:%02x:%02x:%02x:%02x:%02x"
#define NBTFILE_FORM    "%s %s %s\n"
#define MACADDRLEN 18
FILE *nbt_debug = NULL;

// UNIHAN ADD START , PROD00201681
#define ROUTER_LAN_IPV6_HOSTNAME_FORM "%s %s %s\n"
#define TIMEOUT_SEC 0 
#define TIMEOUT_USEC 500
#define BUFFSIZE 1024
#define MAXHOSTNAMELEN 64
#define SCAN_RESULT "/var/tmp/nbtscan6.out"
#define SCAN_RESULT_INTERFACE "/var/tmp/nbtscan6_%s.out"
#define SCAN_RESULT_INTERFACE_TMP "/var/tmp/nbtscan6_%s.out_tmp"
#define SCAN_RESULT_TMP "/var/tmp/nbtscan6.out_tmp"
#define PATH_STATIC_ARP "/var/tmp/static_arp_table"
#define FL_REQUEST		0x8000
#define FL_QUERY		0x7800
#define FL_NON_AUTH_ANSWER	0x0400
#define FL_DGRAM_NOT_TRUNCATED	0x0200
#define FL_RECURSION_NOT_DESIRED	0x0100
#define FL_RECURSION_NOT_AVAIl	0x0080
#define FL_RESERVED1		0x0040
#define FL_RESERVED2		0x0020
#define	FL_BROADCAST		0x0010
#define FL_SUCCESS		0x000F

#define	QT_NODE_STATUS_REQUEST	0x0021
#define QC_INTERNET		0x0001

//#define NB_DGRAM		137

typedef struct{
	struct in6_addr ip6addr;
	char name[MAXHOSTNAMELEN+1];
	char macaddr[18];
} attach_ipv6_info;

static attach_ipv6_info ad_info[255];
int idx=0;
// UNIHAN ADD END , PROD00201681

print_banner() {
  NBT_DEBUG( nbt_debug ,"\nNBTscan version 1.5.1. Copyright (C) 1999-2003 Alla Bezroutchko.\n");
  NBT_DEBUG( nbt_debug ,"This is a free software and it comes with absolutely no warranty.\n");
  NBT_DEBUG( nbt_debug ,"You can use, distribute and modify it under terms of GNU GPL.\n\n");
}

void usage(void) {
  NBT_DEBUG( nbt_debug ,"Usage:\nnbtscan [-v] [-d] [-e] [-l] [-t timeout] [-b bandwidth] [-r] [-q] [-s separator] [-m retransmits] (-f filename)|(<scan_range>) \n");
  NBT_DEBUG( nbt_debug ,"\t-v\t\tverbose output. Print all names received\n");
  NBT_DEBUG( nbt_debug ,"\t\t\tfrom each host\n");
  NBT_DEBUG( nbt_debug ,"\t-d\t\tdump packets. Print whole packet contents.\n");
  NBT_DEBUG( nbt_debug ,"\t-e\t\tFormat output in /etc/hosts format.\n");
  NBT_DEBUG( nbt_debug ,"\t-l\t\tFormat output in lmhosts format.\n");
  NBT_DEBUG( nbt_debug ,"\t\t\tCannot be used with -v, -s or -h options.\n");
  NBT_DEBUG( nbt_debug ,"\t-t timeout\twait timeout milliseconds for response.\n");
  NBT_DEBUG( nbt_debug ,"\t\t\tDefault 1000.\n");
  NBT_DEBUG( nbt_debug ,"\t-b bandwidth\tOutput throttling. Slow down output\n");
  NBT_DEBUG( nbt_debug ,"\t\t\tso that it uses no more that bandwidth bps.\n");
  NBT_DEBUG( nbt_debug ,"\t\t\tUseful on slow links, so that ougoing queries\n");
  NBT_DEBUG( nbt_debug ,"\t\t\tdon't get dropped.\n");
  NBT_DEBUG( nbt_debug ,"\t-r\t\tuse local port 137 for scans. Win95 boxes\n");
  NBT_DEBUG( nbt_debug ,"\t\t\trespond to this only.\n");
  NBT_DEBUG( nbt_debug ,"\t\t\tYou need to be root to use this option on Unix.\n");
  NBT_DEBUG( nbt_debug ,"\t-q\t\tSuppress banners and error messages,\n");
  NBT_DEBUG( nbt_debug ,"\t-s separator\tScript-friendly output. Don't print\n");
  NBT_DEBUG( nbt_debug ,"\t\t\tcolumn and record headers, separate fields with separator.\n");
  NBT_DEBUG( nbt_debug ,"\t-h\t\tPrint human-readable names for services.\n");
  NBT_DEBUG( nbt_debug ,"\t\t\tCan only be used with -v option.\n");
  NBT_DEBUG( nbt_debug ,"\t-m retransmits\tNumber of retransmits. Default 0.\n");
  NBT_DEBUG( nbt_debug ,"\t-f filename\tTake IP addresses to scan from file filename.\n");
  NBT_DEBUG( nbt_debug ,"\t\t\t-f - makes nbtscan take IP addresses from stdin.\n");
  NBT_DEBUG( nbt_debug ,"\t<scan_range>\twhat to scan. Can either be single IP\n");
  NBT_DEBUG( nbt_debug ,"\t\t\tlike 192.168.1.1 or\n");
  NBT_DEBUG( nbt_debug ,"\t\t\trange of addresses in one of two forms: \n");
  NBT_DEBUG( nbt_debug ,"\t\t\txxx.xxx.xxx.xxx/xx or xxx.xxx.xxx.xxx-xxx.\n");
  NBT_DEBUG( nbt_debug ,"Examples:\n");
  NBT_DEBUG( nbt_debug ,"\tnbtscan -r 192.168.1.0/24\n");
  NBT_DEBUG( nbt_debug ,"\t\tScans the whole C-class network.\n");
  NBT_DEBUG( nbt_debug ,"\tnbtscan 192.168.1.25-137\n");
  NBT_DEBUG( nbt_debug ,"\t\tScans a range from 192.168.1.25 to 192.168.1.137\n");
  NBT_DEBUG( nbt_debug ,"\tnbtscan -v -s : 192.168.1.0/24\n");
  NBT_DEBUG( nbt_debug ,"\t\tScans C-class network. Prints results in script-friendly\n");
  NBT_DEBUG( nbt_debug ,"\t\tformat using colon as field separator.\n"); 
  NBT_DEBUG( nbt_debug ,"\t\tProduces output like that:\n");
  NBT_DEBUG( nbt_debug ,"\t\t192.168.0.1:NT_SERVER:00U\n");
  NBT_DEBUG( nbt_debug ,"\t\t192.168.0.1:MY_DOMAIN:00G\n");
  NBT_DEBUG( nbt_debug ,"\t\t192.168.0.1:ADMINISTRATOR:03U\n");
  NBT_DEBUG( nbt_debug ,"\t\t192.168.0.2:OTHER_BOX:00U\n");
  NBT_DEBUG( nbt_debug ,"\t\t...\n");
  NBT_DEBUG( nbt_debug ,"\tnbtscan -f iplist\n");
  NBT_DEBUG( nbt_debug ,"\t\tScans IP addresses specified in file iplist.\n");
  //exit(2);
};

int set_range(char* range_str, struct ip_range* range_struct) 
{
    if(is_ip(range_str, range_struct))
    {
        return 1;
    }

    if(is_range1(range_str, range_struct))
    {
        return 1;
    }

    if(is_range2(range_str, range_struct))
    {
        return 1;
    }
    return 0;
};

int print_header() {
  NBT_DEBUG( nbt_debug ,"%-17s%-17s%-10s%-17s%-17s\n", "IP address", "NetBIOS Name", 
	 "Server", "User", "MAC address");
  NBT_DEBUG( nbt_debug ,"------------------------------------------------------------------------------\n");
};

int d_print_hostinfo(struct in_addr addr, const struct nb_host_info* hostinfo) {
  int i;
  unsigned char service; /* 16th byte of NetBIOS name */
  char name[16];

  NBT_DEBUG( nbt_debug ,"\nPacket dump for Host %s:\n\n", inet_ntoa(addr));
  if(hostinfo->is_broken) NBT_DEBUG( nbt_debug ,"Incomplete packet, %d bytes long.\n", hostinfo->is_broken);
	
  if(hostinfo->header) {
    NBT_DEBUG( nbt_debug ,"Transaction ID: 0x%04x (%1$d)\n", hostinfo->header->transaction_id);
    NBT_DEBUG( nbt_debug ,"Flags: 0x%04x (%1$d)\n", hostinfo->header->flags);
    NBT_DEBUG( nbt_debug ,"Question count: 0x%04x (%1$d)\n", hostinfo->header->question_count);
    NBT_DEBUG( nbt_debug ,"Answer count: 0x%04x (%1$d)\n", hostinfo->header->answer_count);
    NBT_DEBUG( nbt_debug ,"Name service count: 0x%04x (%1$d)\n", hostinfo->header->name_service_count);
    NBT_DEBUG( nbt_debug ,"Additional record count: 0x%04x (%1$d)\n", hostinfo->header->additional_record_count);
    NBT_DEBUG( nbt_debug ,"Question name: %s\n", hostinfo->header->question_name);
    NBT_DEBUG( nbt_debug ,"Question type: 0x%04x (%1$d)\n", hostinfo->header->question_type);
    NBT_DEBUG( nbt_debug ,"Question class: 0x%04x (%1$d)\n", hostinfo->header->question_class);
    NBT_DEBUG( nbt_debug ,"Time to live: 0x%08x (%1$d)\n", hostinfo->header->ttl);
    NBT_DEBUG( nbt_debug ,"Rdata length: 0x%04x (%1$d)\n", hostinfo->header->rdata_length);
    NBT_DEBUG( nbt_debug ,"Number of names: 0x%02x (%1$d)\n", hostinfo->header->number_of_names);
  };
	
  if(hostinfo->names) {
    NBT_DEBUG( nbt_debug ,"Names received:\n");
    for(i=0; i< hostinfo->header->number_of_names; i++) {
      service = hostinfo->names[i].ascii_name[15];
      strncpy(name, hostinfo->names[i].ascii_name, 15);
      name[16]=0; 
      NBT_DEBUG( nbt_debug ,"%-17s Service: 0x%02x Flags: 0x%04x\n", name, service, hostinfo->names[i].rr_flags);
    }
  };
	
  if(hostinfo->footer) {
    NBT_DEBUG( nbt_debug ,"Adapter address: %02x-%02x-%02x-%02x-%02x-%02x\n", 
	   hostinfo->footer->adapter_address[0], hostinfo->footer->adapter_address[1],
	   hostinfo->footer->adapter_address[2], hostinfo->footer->adapter_address[3],
	   hostinfo->footer->adapter_address[4], hostinfo->footer->adapter_address[5]); 
    NBT_DEBUG( nbt_debug ,"Version major: 0x%02x (%1$d)\n", hostinfo->footer->version_major);
    NBT_DEBUG( nbt_debug ,"Version minor: 0x%02x (%1$d)\n", hostinfo->footer->version_minor);
    NBT_DEBUG( nbt_debug ,"Duration: 0x%04x (%1$d)\n", hostinfo->footer->duration);
    NBT_DEBUG( nbt_debug ,"FRMRs Received: 0x%04 (%1$d)\n", hostinfo->footer->frmps_received);
    NBT_DEBUG( nbt_debug ,"FRMRs Transmitted: 0x%04 (%1$d)\n", hostinfo->footer->frmps_transmitted);
    NBT_DEBUG( nbt_debug ,"IFrame Receive errors: 0x%04 (%1$d)\n", hostinfo->footer->iframe_receive_errors);
    NBT_DEBUG( nbt_debug ,"Transmit aborts: 0x%04 (%1$d)\n", hostinfo->footer->transmit_aborts);
    NBT_DEBUG( nbt_debug ,"Transmitted: 0x%08 (%1$d)\n", hostinfo->footer->transmitted);
    NBT_DEBUG( nbt_debug ,"Received: 0x%08 (%1$d)\n", hostinfo->footer->received);
    NBT_DEBUG( nbt_debug ,"IFrame transmit errors: 0x%04 (%1$d)\n", hostinfo->footer->iframe_transmit_errors);
    NBT_DEBUG( nbt_debug ,"No receive buffers: 0x%04 (%1$d)\n", hostinfo->footer->no_receive_buffer);
    NBT_DEBUG( nbt_debug ,"tl timeouts: 0x%04 (%1$d)\n", hostinfo->footer->tl_timeouts);
    NBT_DEBUG( nbt_debug ,"ti timeouts: 0x%04 (%1$d)\n", hostinfo->footer->ti_timeouts);
    NBT_DEBUG( nbt_debug ,"Free NCBS: 0x%04 (%1$d)\n", hostinfo->footer->free_ncbs);        
    NBT_DEBUG( nbt_debug ,"NCBS: 0x%04 (%1$d)\n", hostinfo->footer->ncbs);
    NBT_DEBUG( nbt_debug ,"Max NCBS: 0x%04 (%1$d)\n", hostinfo->footer->max_ncbs);
    NBT_DEBUG( nbt_debug ,"No transmit buffers: 0x%04 (%1$d)\n", hostinfo->footer->no_transmit_buffers);
    NBT_DEBUG( nbt_debug ,"Max datagram: 0x%04 (%1$d)\n", hostinfo->footer->max_datagram);
    NBT_DEBUG( nbt_debug ,"Pending sessions: 0x%04 (%1$d)\n", hostinfo->footer->pending_sessions);
    NBT_DEBUG( nbt_debug ,"Max sessions: 0x%04 (%1$d)\n", hostinfo->footer->max_sessions);
    NBT_DEBUG( nbt_debug ,"Packet sessions: 0x%04 (%1$d)\n", hostinfo->footer->packet_sessions);
  };
};


int v_print_hostinfo(struct in_addr addr, const struct nb_host_info* hostinfo, char* sf, int hr) {
  int i, unique;
  my_uint8_t service; /* 16th byte of NetBIOS name */
  char name[16];
  char* sname;

  if(!sf) {
    NBT_DEBUG( nbt_debug ,"\nNetBIOS Name Table for Host %s:\n\n", inet_ntoa(addr));
    if(hostinfo->is_broken) 
      NBT_DEBUG( nbt_debug ,"Incomplete packet, %d bytes long.\n", hostinfo->is_broken);

    NBT_DEBUG( nbt_debug ,"%-17s%-17s%-17s\n", "Name", "Service", "Type");
    NBT_DEBUG( nbt_debug ,"----------------------------------------\n");
  };
  if(hostinfo->header && hostinfo->names) {
    for(i=0; i< hostinfo->header->number_of_names; i++) {
      service = hostinfo->names[i].ascii_name[15];
      strncpy(name, hostinfo->names[i].ascii_name, 15);
      name[16]=0;
      unique = !(hostinfo->names[i].rr_flags & 0x0080);
      if(sf) {
	NBT_DEBUG( nbt_debug ,"%s%s%s%s", inet_ntoa(addr), sf, name, sf);
	if(hr)
    {
        //NBT_DEBUG( nbt_debug ,"%s\n", (char*)getnbservicename(service, unique, name));
    }
	else {
	  NBT_DEBUG( nbt_debug ,"%02x", service);
	  if(unique) NBT_DEBUG( nbt_debug ,"U\n");
	  else NBT_DEBUG( nbt_debug ,"G\n");
	}
      } else {
	NBT_DEBUG( nbt_debug ,"%-17s",  name);
	if(hr)
    {
        //NBT_DEBUG( nbt_debug ,"%s\n", (char*)getnbservicename(service, unique, name));
    }
	else {	
	  NBT_DEBUG( nbt_debug ,"<%02x>", service);
	  if(unique)  NBT_DEBUG( nbt_debug ,"             UNIQUE\n");
	  else NBT_DEBUG( nbt_debug ,"              GROUP\n");
	};
      }
    };
  };
	
  if(hostinfo->footer) {
    if(sf) NBT_DEBUG( nbt_debug ,"%s%sMAC%s", inet_ntoa(addr), sf, sf); 
    else NBT_DEBUG( nbt_debug ,"\nAdapter address: ");
    NBT_DEBUG( nbt_debug ,"%02x-%02x-%02x-%02x-%02x-%02x\n",
	   hostinfo->footer->adapter_address[0], hostinfo->footer->adapter_address[1],
	   hostinfo->footer->adapter_address[2], hostinfo->footer->adapter_address[3],
	   hostinfo->footer->adapter_address[4], hostinfo->footer->adapter_address[5]);	
  };
  if(!sf) NBT_DEBUG( nbt_debug ,"----------------------------------------\n");
  return 1;
};

int print_hostinfo(struct in_addr addr, struct nb_host_info* hostinfo, char* sf) {
    int i;
    unsigned char service; /* 16th byte of NetBIOS name */
    char comp_name[16], user_name[16];
    int is_server=0;
    int unique;
    int first_name=1;
    char mac_str[MACADDRLEN];
    FILE *file = NULL;

    strncpy(comp_name,"<unknown>",15);
    strncpy(user_name,"<unknown>",15);
    if(hostinfo->header && hostinfo->names)
    {
        for(i=0; i< hostinfo->header->number_of_names; i++) 
        {
            service = hostinfo->names[i].ascii_name[15];
            unique = ! (hostinfo->names[i].rr_flags & 0x0080);
            if(service == 0  && unique && first_name) 
            {
                /* Unique name, workstation service - this is computer name */ 
                strncpy(comp_name, hostinfo->names[i].ascii_name, 15);
                comp_name[15] = 0;
                first_name = 0;
            };

            if(service == 0x20 && unique) 
            {
                is_server=1;
            }

            if(service == 0x03 && unique) 
            {
                strncpy(user_name, hostinfo->names[i].ascii_name, 15);
                user_name[15]=0;
            };
        };
    };

    if(sf) 
    {
        NBT_DEBUG( nbt_debug ,"%s%s%s%s", inet_ntoa(addr), sf, comp_name, sf);
        if(is_server)
        {
            NBT_DEBUG( nbt_debug ,"<server>");
        }
        NBT_DEBUG( nbt_debug ,"%s%s%s", sf, user_name, sf);
    } 
    else 
    {
        NBT_DEBUG( nbt_debug ,"%-17s%-17s",inet_ntoa(addr),comp_name);
        if(is_server)
        {
            NBT_DEBUG( nbt_debug ,"%-10s", "<server>"); 
        }
        else 
        {
            NBT_DEBUG( nbt_debug ,"%-10s","");
        }
        NBT_DEBUG( nbt_debug ,"%-17s", user_name);
    };

    if(hostinfo->footer) 
    {
        NBT_DEBUG( nbt_debug ,"%02x-%02x-%02x-%02x-%02x-%02x\n",
        hostinfo->footer->adapter_address[0], hostinfo->footer->adapter_address[1],
        hostinfo->footer->adapter_address[2], hostinfo->footer->adapter_address[3],
        hostinfo->footer->adapter_address[4], hostinfo->footer->adapter_address[5]);
    } 
    else 
    {
        NBT_DEBUG( nbt_debug ,"\n");
    };

    file = fopen( NBT_RESULT_TMP_FILE_PATH , "a+" );
    if( file )
    {
        snprintf( mac_str , MACADDRLEN , MAC_ADDR_STR_FORMAT ,
                            hostinfo->footer->adapter_address[0], 
                            hostinfo->footer->adapter_address[1],
                            hostinfo->footer->adapter_address[2], 
                            hostinfo->footer->adapter_address[3],
                            hostinfo->footer->adapter_address[4], 
                            hostinfo->footer->adapter_address[5]);
        fprintf( file , NBTFILE_FORM , mac_str , inet_ntoa(addr) , comp_name );
        //printf( NBTFILE_FORM , mac_str , inet_ntoa(addr) , comp_name );
    }
    
    if( file ) fclose( file );

    return 1;
};

/* Print hostinfo in /etc/hosts or lmhosts format */
/* If l is true adds #PRE to each line of output (for lmhosts) */

int l_print_hostinfo(struct in_addr addr, struct nb_host_info* hostinfo, int l) {
  int i;
  unsigned char service; /* 16th byte of NetBIOS name */
  char comp_name[16];
  int is_server=0;
  int unique;
  int first_name=1;

  strncpy(comp_name,"<unknown>",15);

  if(hostinfo->header && hostinfo->names) {
    for(i=0; i< hostinfo->header->number_of_names; i++) {
      service = hostinfo->names[i].ascii_name[15];
      unique = ! (hostinfo->names[i].rr_flags & 0x0080);
      if(service == 0  && unique && first_name) {
				/* Unique name, workstation service - this is computer name */ 
	strncpy(comp_name, hostinfo->names[i].ascii_name, 15);
	comp_name[15]=0;
	first_name = 0;
      };
    };
  };
  NBT_DEBUG( nbt_debug ,"%s\t%s", inet_ntoa(addr), comp_name);
  if(l) NBT_DEBUG( nbt_debug ,"\t#PRE");
  NBT_DEBUG( nbt_debug ,"\n");
}

	
#define BUFFSIZE 1024

void close_debuglog_fp()
{
    if( nbt_debug )
    {
        fclose( nbt_debug );
        nbt_debug = NULL;
    }
}

// UNIHAN ADD START , PROD00201681
void sc_pipe( char *cmd, char *data)
{
    FILE *read_fp;
    char buffer[BUFSIZ + 1];
    int chars_read;
    memset(buffer, '\0', sizeof(buffer));
    read_fp = popen(cmd, "r");
    if (read_fp != NULL) {
        chars_read = fread(buffer, sizeof(char), BUFSIZ, read_fp);
        while (chars_read > 0) 
        {
            buffer[chars_read - 1] = '\0';
            chars_read = fread(buffer, sizeof(char), BUFSIZ, read_fp);
            memcpy( data , buffer , sizeof(buffer));
        }
        pclose(read_fp);
    }
}

static int conv_ipv6_str( struct in6_addr *in6addr , char *question_name )
{
	int i=0;
	char *getTag, *tokenBrk;
	int temp[32];
	
	question_name++;
	getTag = strtok_r( question_name , "\001", &tokenBrk );

	while (getTag && i<32) 
	{
		temp[i]= strtol(getTag, NULL, 16);
		getTag = strtok_r( NULL, "\001", &tokenBrk );
		i++;
	};

	for(i=0;i<16;i++)
	{
		in6addr->s6_addr[i] = (temp[31- (2*i)] <<4) + (temp[31-(2*i)-1]);
	}
//	a6->s6_addr[i] = strtol(if_index, NULL, 16);

	return 0;
}

struct nb_ipv6_host_info* parse_ipv6_response(char* buff, int buffsize) {
	struct nb_ipv6_host_info* hostinfo = NULL;
	nbname_ipv6_response_header_t* response_header;
	int offset = 0;

	answer_t* answer;

// Dump packet
#if 0
{
	int i=0;
	printf("buffsize=%d\n", buffsize);

	for(i=0;i<buffsize;i++)
	{
		printf("%02X", *(buff+i) & 0xFF );

		if((i+1) %16==0)
			printf("\n");
		else if((i+1)%8==0 )
			printf("\t");
	}
}
#endif

	if((response_header = malloc(sizeof(nbname_ipv6_response_header_t)))==NULL) return NULL;
	bzero(response_header, sizeof(nbname_ipv6_response_header_t));

	if((answer = malloc(sizeof(answer_t))) ==NULL ) return NULL;
	bzero(answer, sizeof(answer_t));
	
	if((hostinfo = malloc(sizeof(struct nb_ipv6_host_info)))==NULL) return NULL;
	bzero(hostinfo, sizeof(struct nb_ipv6_host_info));

	/* Parsing received packet */
	/* Start with header */
	if( offset+sizeof(response_header->transaction_id) >= buffsize) goto broken_packet;
	response_header->transaction_id = get16(buff+offset); 
	//Move pointer to the next structure field
	offset+=sizeof(response_header->transaction_id);

	// Check if there is room for next field in buffer
	if( offset+sizeof(response_header->flags) >= buffsize) goto broken_packet; 
	response_header->flags = get16(buff+offset);
        offset+=sizeof(response_header->flags);
	
	if( offset+sizeof(response_header->question_count) >= buffsize) goto broken_packet;
	response_header->question_count = get16(buff+offset);
        offset+=sizeof(response_header->question_count);
        
	if( offset+sizeof(response_header->answer_count) >= buffsize) goto broken_packet;
	response_header->answer_count = get16(buff+offset);
        offset+=sizeof(response_header->answer_count);
        
	if( offset+sizeof(response_header->name_service_count) >= buffsize) goto broken_packet;
	response_header->name_service_count = get16(buff+offset);
        offset+=sizeof(response_header->name_service_count);
        
	if( offset+sizeof(response_header->additional_record_count) >= buffsize) goto broken_packet;
	response_header->additional_record_count = get16(buff+offset);
        offset+=sizeof(response_header->additional_record_count);

//	printf("question_count=%d, answer_count=%d\n", question_count, answer_count);

/* Skip query to find the beginning of answer*/
	if( offset+sizeof(query_t ) >= buffsize) goto broken_packet;
	/*TODO*/ 
        offset+=sizeof(query_t);
/* Parse answer */
	if( offset+sizeof(answer_t ) >= buffsize) goto broken_packet;
	strncpy( answer->question_name, buff+offset, sizeof( answer->question_name));
	offset+=sizeof(answer->question_name);

	if( offset+sizeof(answer->question_type) >= buffsize) goto broken_packet;
	answer->question_type = get16(buff+offset);
        offset+=sizeof(answer->question_type);
        
	if( offset+sizeof(answer->question_class) >= buffsize) goto broken_packet;
	answer->question_class = get16(buff+offset);
        offset+=sizeof(answer->question_class);

	if( offset+sizeof(answer->ttl) >= buffsize) goto broken_packet;
        answer->ttl = get32(buff+offset);
        offset+=sizeof(answer->ttl);
        
	if( offset+sizeof(answer->rdata_length) >= buffsize) goto broken_packet;
        answer->rdata_length = get16(buff+offset);
        offset+=sizeof(answer->rdata_length);

	if( (offset+ answer->rdata_length) > buffsize) goto broken_packet;

	strncpy(hostinfo->question_name, answer->question_name, sizeof( answer->question_name) );
	strncpy(hostinfo->ascii_name, buff+offset+1 , answer->rdata_length );

	return hostinfo;

	broken_packet: 
		printf("broken_packet\n");
		return NULL;	
};

int search_ipv6_hostname( char *interface )
{
	int sock,size;
	socklen_t addr_size;
	struct sockaddr_in6 src_sockaddr, dest_sockaddr;
	fd_set rfds;
        struct timeval tv;
        int retval;
	char buff[BUFFSIZE];
	struct in6_addr targetaddr;
	struct in6_addr localaddr;
	struct in6_addr destaddr = {0xff,0x02, 0,0, 0,0, 0,0, 0,0, 0,0, 0,0x01,0,0x03};   // ff02::1:3
  	struct nb_ipv6_host_info* hostinfo;
	int i=0;
	char *if_name=NULL;
	int scope_id=0;
	char data[BUFSIZ+1] = {0};
	char cmd[256];
	char ip6_addr[128];
	char mac[18];
	char  *getTag, *tokenBrk;
	struct in6_addr temp_addr;
	FILE *fp=NULL;
	int finding=0;
	char path[128] = {0};
	char path_tmp[128] = {0};
    int IsRecord = 0;

	if(!interface){
		printf("%s <Interface>\n",interface);
		return 0;
	}else
		if_name = interface;

	memset(&src_sockaddr,0,sizeof(struct sockaddr_in6));
	memset(&dest_sockaddr,0,sizeof(struct sockaddr_in6));
	memset(ad_info,0,sizeof(attach_ipv6_info) * 254);

	sock = socket(AF_INET6, SOCK_DGRAM, IPPROTO_UDP);

	if (sock < 0) {
		puts("Failed to create socket");
		return -1;
	}

	if (get_ipv6_if_info( if_name ,&scope_id, &localaddr ) <0 ) {
		printf("Failed to get interface info\n");	
		return -1;
	}
	//printf("ifname=%s, scope_id=%d\n", if_name, scope_id );

	src_sockaddr.sin6_family = AF_INET6;
	src_sockaddr.sin6_scope_id = scope_id;
	memcpy( &src_sockaddr.sin6_addr , &localaddr,sizeof(struct in6_addr));

	if (bind(sock, (struct sockaddr *)&src_sockaddr, sizeof(src_sockaddr)) == -1){
		puts("Failed to bind");
		close(sock);
		return -1;
	}

	sprintf(cmd,"ip -6 neigh show dev %s", if_name);
	sc_pipe(cmd, data);

	getTag = strtok_r( data , "\n", &tokenBrk );
	while ( getTag )
	{ 
		sscanf(getTag,"%s %*s %s %*s", ip6_addr, mac );

		if ( inet_pton(AF_INET6, ip6_addr, &targetaddr ) <=0 ) {
			printf("inet_pton fail\n");
			return -1;
		}
		
		retval = send_ipv6_query(sock, destaddr, targetaddr, 0);
		if (retval) {
			puts("send_ipv6_query fail!\n");
			close(sock);
			return -1;
		}
		getTag = strtok_r( NULL, "\n", &tokenBrk );
	};

	do{
		FD_ZERO(&rfds);
       		FD_SET(sock, &rfds);
	       	tv.tv_sec = TIMEOUT_SEC;
        	tv.tv_usec = TIMEOUT_USEC;

		retval = select(sock+1, &rfds, NULL, NULL, &tv);

		if(retval <= 0)
			break;

		if ( (size = recvfrom(sock, buff, BUFFSIZE, 0,(struct sockaddr*)&dest_sockaddr, &addr_size))<= 0 ){

			continue;
		}

		hostinfo = (struct nb_ipv6_host_info*)parse_ipv6_response(buff, size);
		//if(hostinfo)
		//printf("hostinfo->question_name=%s,\n ascii= %s\n", hostinfo->question_name, hostinfo->ascii_name);

#if 1
		if(!hostinfo){
			printf("UNKNOWN\n");
		}else{
			if( hostinfo->ascii_name && strlen(hostinfo->ascii_name)>0 ){

				strcpy( ad_info[idx].name, hostinfo->ascii_name);
				conv_ipv6_str( &temp_addr , hostinfo->question_name );
				ad_info[idx].ip6addr = temp_addr;

				//printf("name=%s\n", hostinfo->ascii_name);
				idx++;
			}
			free(hostinfo);
		}
#endif
	}while(retval);

	close(sock);

	if (    ( strcmp(if_name,"l2sd0.2") == 0 ) || 
            ( strcmp(if_name,"l2sd0.3") == 0 ) || 
            ( strcmp(if_name,"l2sd0.4") == 0 ) ||
            ( strcmp(if_name,"l2sd0.5") == 0 ) ||
            ( strcmp(if_name,"l2sd0.6") == 0 ) ||
            ( strcmp(if_name,"l2sd0.7") == 0 ) ||
            ( strcmp(if_name,"l2sd0.8") == 0 ) ||
            ( strcmp(if_name,"l2sd0.9") == 0 ) 
        )
    {
		sprintf( path_tmp , SCAN_RESULT_INTERFACE_TMP , if_name );
		sprintf( path , SCAN_RESULT_INTERFACE , if_name );
	} 
    else 
    {
		sprintf( path_tmp , "%s" , SCAN_RESULT_TMP );
		sprintf( path , "%s" , SCAN_RESULT );
	}

    int errno = 0;
	fp = fopen( path_tmp ,"a+" );
    if( fp == NULL )
    {
        printf( "OPEN FILE FAILED : %s\n",strerror(errno) );
    }

	sprintf(cmd,"ip -6 neigh show dev %s", if_name);
	sc_pipe(cmd, data);

	getTag = strtok_r( data , "\n", &tokenBrk );
	while ( getTag )
	{
#if 0
		if (strstr(getTag,"REACHABLE") == NULL) {
			getTag = strtok_r( NULL, "\n", &tokenBrk );
			continue;
		}
#endif	
		if (strstr(getTag,"IMCOMPLETE")) {
			getTag = strtok_r( NULL, "\n", &tokenBrk );
			continue;
		}

		finding=0;
		sscanf(getTag,"%s %*s %s %*s", ip6_addr, mac );

		if ( inet_pton(AF_INET6, ip6_addr, &temp_addr ) <=0 ) {
			printf("inet_pton fail\n");
			return -1;
		}
		for(i=0; i< idx ; i++)
		{	
			if( memcmp ( &ad_info[i].ip6addr, &temp_addr, sizeof(struct in6_addr) )==0)
			{
				finding=1;
				break;
			}

		}

        /*
		if(finding)
			printf(ROUTER_LAN_IPV6_HOSTNAME_FORM, ip6_addr, ad_info[i].name, mac);
		else
			printf(ROUTER_LAN_IPV6_HOSTNAME_FORM, ip6_addr, "UNKNOWN", mac);
        */

		if(fp) 
        {
			if(finding)
            {
				fprintf(fp,ROUTER_LAN_IPV6_HOSTNAME_FORM, ip6_addr, ad_info[i].name, mac);
                IsRecord = 1;
            }
            /*
			else
            {
				fprintf(fp,ROUTER_LAN_IPV6_HOSTNAME_FORM, ip6_addr, "UNKNOWN", mac);
            }
            */
		}
		getTag = strtok_r( NULL, "\n", &tokenBrk );
	};

    if( fp )
    {
        fclose( fp );
    }
    
    if( IsRecord == 1 && rename( path_tmp , path ) != 0 )
    {
        perror( "Error rename file" );
    }
    else
    {
        remove( path_tmp );
    }

	return 0;
}
// UNIHAN ADD END , PROD00201681

int main(int argc, char *argv[]) 
{
    int timeout=1000, verbose=0, use137=0, ch, dump=0, bandwidth=0, send_ok=0, hr=0, etc_hosts=0, lmhosts=0;
    extern char *optarg;
    extern int optind;
    char* target_string;
    char* sf=NULL;
    char* filename =NULL;
    int search_ipv6_flag = 0; // UNIHAN MOD , PROD00201681
    char* interface = NULL;   // UNIHAN MOD , PROD00201681
    struct ip_range range;
    void *buff = NULL;
    int sock, addr_size;
    struct sockaddr_in src_sockaddr, dest_sockaddr;
    struct  in_addr *prev_in_addr=NULL;
    struct  in_addr *next_in_addr=NULL;
    struct timeval select_timeout, last_send_time, current_time, diff_time, send_interval;
    struct timeval transmit_started, now, recv_time;
    struct nb_host_info* hostinfo;
    fd_set* fdsr = NULL;
    fd_set* fdsw = NULL;
    int sel, size;
    struct list* scanned = NULL;
    my_uint32_t rtt_base; /* Base time (seconds) for round trip time calculations */
    float rtt; /* most recent measured RTT, seconds */
    float srtt=0; /* smoothed rtt estimator, seconds */
    float rttvar=0.75; /* smoothed mean deviation, seconds */ 
    double delta; /* used in retransmit timeout calculations */
    int rto, retransmits=0, more_to_send=1, i;
    char errmsg[80];
    char str[80];
    FILE* targetlist=NULL;
    int ret_val = 0;

    if( access( NBT_DEBUG_FILE , 0 ) < 0 )
    {
        nbt_debug = NULL;
    }
    else
    {
        nbt_debug = fopen( NBT_DEBUG_FILE , "a+" );
    }

    /* Parse supplied options */
    /**************************/
    if(argc<2) 
    { 
        print_banner(); 
        usage();
        goto CLOSE_LOG_FILE;
    };

    while ((ch = getopt(argc, argv, "vrdelqhm:s:t:b:f:I:")) != -1) // UNIHAN MOD , PROD00201681
    {
        switch (ch) 
        {
            case 'v':
              verbose = 1;
              break;
            case 't':
              timeout=atoi(optarg);
              if(timeout==0) 
              { 
                  NBT_DEBUG( nbt_debug ,"Bad timeout value: %s\n", optarg);
                  usage();
                  goto CLOSE_LOG_FILE;
              };
              break;
            case 'r':
#if defined WINDOWS
              NBT_DEBUG( nbt_debug ,"Warning: -r option not supported under Windows. Running without it.\n\n");
#else
              use137=1;
#endif
              break;
            case 'd':
              dump=1;
              break;
            case 'e':
              etc_hosts=1;
              break;
            case 'l':
              lmhosts=1;
              break;
            case 'q':
              quiet=1; /* Global variable */
              break;
            case 'b':
              bandwidth=atoi(optarg);
              if(bandwidth==0)
              {
                  err_print("Bad bandwidth value, ignoring it", quiet);
              }
              break;
            case 'h':
              hr=1; /* human readable service names instead of hex codes */
              break;
            case 's':
              sf=optarg; /* script-friendly output format */
              break;
            case 'm':
              retransmits=atoi(optarg);
              if(retransmits==0) 
              { 
                  NBT_DEBUG( nbt_debug ,"Bad number of retransmits: %s\n", optarg);
                  usage();
                  goto CLOSE_LOG_FILE;
              };
              break;
            case 'f':
              filename = optarg;
              break;
            // UNIHAN ADD START , PROD00201681
            case 'I':
              search_ipv6_flag = 1;
              interface = optarg;
              break;
            // UNIHAN ADD END , PROD00201681
            default:
              print_banner();
              usage();
              goto CLOSE_LOG_FILE;
        };
    }

    // UNIHAN ADD START , PROD00201681
    if( search_ipv6_flag )
    {
        search_ipv6_hostname(interface);
        goto IPV6_SEARCH_DONE;
    }
    // UNIHAN ADD END , PROD00201681


    if(dump && verbose) 
    {
        NBT_DEBUG( nbt_debug ,"Cannot be used with both dump (-d) and verbose (-v) options.\n");
        usage();
        goto CLOSE_LOG_FILE;
    };  

    if(dump && sf) 
    {
        NBT_DEBUG( nbt_debug ,"Cannot be used with both dump (-d) and script-friendly (-s) options.\n");
        usage();
        goto CLOSE_LOG_FILE;
    };

    if(dump && lmhosts) 
    {
        NBT_DEBUG( nbt_debug ,"Cannot be used with both dump (-d) and lmhosts (-l) options.\n");
        usage;
        goto CLOSE_LOG_FILE;
    };

    if(dump && etc_hosts) 
    {
        NBT_DEBUG( nbt_debug ,"Cannot be used with both dump (-d) and /etc/hosts (-e) options.\n");
        usage;
        goto CLOSE_LOG_FILE;
    };

    if(verbose && lmhosts)
    {
        NBT_DEBUG( nbt_debug ,"Cannot be used with both verbose (-v) and lmhosts (-l) options.\n");
        usage;
        goto CLOSE_LOG_FILE;
    };

    if(verbose && etc_hosts)
    {
        NBT_DEBUG( nbt_debug ,"Cannot be used with both verbose (-v) and /etc/hosts (-e) options.\n");
        usage;
        goto CLOSE_LOG_FILE;
    };

    if(lmhosts && etc_hosts)
    {
        NBT_DEBUG( nbt_debug ,"Cannot be used with both lmhosts (-l) and /etc/hosts (-e) options.\n");
        usage;
        goto CLOSE_LOG_FILE;
    };


    if(dump && hr) 
    {
        NBT_DEBUG( nbt_debug ,"Cannot be used with both dump (-d) and \"human-readable service names\" (-h) options.\n");
        usage();
        goto CLOSE_LOG_FILE;
    };

    if(hr && !verbose) 
    {
        NBT_DEBUG( nbt_debug ,"\"Human-readable service names\" (-h) option cannot be used without verbose (-v) option.\n");
        usage();
        goto CLOSE_LOG_FILE;
    };

    if(filename) 
    {
        if(strcmp(filename, "-") == 0) /* Get IP addresses from stdin */
        { 
            targetlist = stdin; 
            target_string = "STDIN";
        } 
        else 
        {
            targetlist=fopen(filename,"r");
            target_string = filename;
        };
        
        if(!targetlist) 
        {
            snprintf(errmsg, 80, "Cannot open file %s", filename);
            err_print(errmsg, quiet);
            goto CLOSE_LOG_FILE;
        }  
    }
    else 
    {  
        argc -= optind;
        argv += optind;
        if(argc!=1)
        {
            usage();
            goto CLOSE_LOG_FILE;
        }

        if((target_string=strdup(argv[0]))==NULL) 
        {
            //err_die("Malloc failed.\n", quiet);
            err_print("Malloc failed.\n", quiet);
            goto CLOSE_LOG_FILE;
        }

        if(!set_range(target_string, &range)) 
        {
            NBT_DEBUG( nbt_debug ,"Error: %s is not an IP address or address range.\n", target_string);
            if( target_string ) 
            {
                free(target_string);
            }
                
            usage();
            goto CLOSE_LOG_FILE;
        };

        /* ARRIS DELETE BEGIN : Is this a bug?  We want to specify the IP/RANGE on the cmd line */
#if 0
        if( target_string ) 
        {
            free(target_string);
            goto CLOSE_LOG_FILE;
        }
#endif
        /* ARRIS DELETE END */
    }


    if(!(quiet || sf || lmhosts || etc_hosts)) NBT_DEBUG( nbt_debug ,"Doing NBT name scan for addresses from %s\n\n", target_string);

    /* Finished with options */
    /*************************/

    /* Prepare socket and address structures */
    /*****************************************/
    sock = socket(AF_INET, SOCK_DGRAM, IPPROTO_UDP);
    if (sock < 0) 
    {
        //err_die("Failed to create socket", quiet);
        err_print("Failed to create socket", quiet);
        goto CLOSE_LOG_FILE;
    }

    bzero((void*)&src_sockaddr, sizeof(src_sockaddr));
    src_sockaddr.sin_family = AF_INET;
    if(use137) src_sockaddr.sin_port = htons(NB_DGRAM);
    if (bind(sock, (struct sockaddr *)&src_sockaddr, sizeof(src_sockaddr)) == -1) 
    {
        //err_die("Failed to bind", quiet);
        err_print("Failed to bind", quiet);
        goto SOCK_ERROR;
    }
        
    fdsr=malloc(sizeof(fd_set));
    if(!fdsr)
    {
        //err_die("Malloc failed", quiet);
        err_print("Malloc failed", quiet);
        goto SOCK_ERROR;
    }
    FD_ZERO(fdsr);
    FD_SET(sock, fdsr);
        
    fdsw=malloc(sizeof(fd_set));
    if(!fdsw)
    {
        //err_die("Malloc failed", quiet);
        err_print("Malloc failed", quiet);
        goto FREE_FDSR;
    }
    FD_ZERO(fdsw);
    FD_SET(sock, fdsw);

    /* timeout is in milliseconds */
    select_timeout.tv_sec = timeout / 1000;
    select_timeout.tv_usec = (timeout % 1000) * 1000; /* Microseconds */

    addr_size = sizeof(struct sockaddr_in);

    next_in_addr = malloc(sizeof(struct  in_addr));
    if(!next_in_addr)
    {
        //err_die("Malloc failed", quiet);
        err_print("Malloc failed", quiet);
        goto FREE_FDSW;
    }

    buff=malloc(BUFFSIZE);
    if(!buff)
    {
        //err_die("Malloc failed", quiet);
        err_print("Malloc failed", quiet);
        goto FREE_NEXT_IN_ADDR;
    }

    /* Calculate interval between subsequent sends */

    timerclear(&send_interval);
    if(bandwidth) send_interval.tv_usec = 
          (NBNAME_REQUEST_SIZE + UDP_HEADER_SIZE + IP_HEADER_SIZE)*8*1000000 /
          bandwidth;  /* Send interval in microseconds */
    else /* Assuming 10baseT bandwidth */
    send_interval.tv_usec = 1; /* for 10baseT interval should be about 1 ms */
    if (send_interval.tv_usec >= 1000000) {
    send_interval.tv_sec = send_interval.tv_usec / 1000000;
    send_interval.tv_usec = send_interval.tv_usec % 1000000;
    }

    gettimeofday(&last_send_time, NULL); /* Get current time */

    rtt_base = last_send_time.tv_sec; 

    /* Send queries, receive answers and print results */
    /***************************************************/

    scanned = new_list();
    if( scanned == NULL )
    {
		err_print("Malloc failed", quiet);
        goto FREE_BUFF;
    }

    if(!(quiet || verbose || dump || sf || lmhosts || etc_hosts)) print_header();

    for(i=0; i <= retransmits; i++) 
    {
        gettimeofday(&transmit_started, NULL);
        while ( (select(sock+1, fdsr, fdsw, NULL, &select_timeout)) > 0) 
        {
            if(FD_ISSET(sock, fdsr)) 
            {
                if ( (size = recvfrom(sock, buff, BUFFSIZE, 0, (struct sockaddr*)&dest_sockaddr, &addr_size)) <= 0 ) 
                {
                    snprintf(errmsg, 80, "%s\tRecvfrom failed", inet_ntoa(dest_sockaddr.sin_addr));
                    err_print(errmsg, quiet);
                    continue;
                };

                gettimeofday(&recv_time, NULL);
                hostinfo = (struct nb_host_info*)parse_response(buff, size);
                if(!hostinfo) 
                {
                  err_print("parse_response returned NULL", quiet);
                  continue;
                };
                
                /* If this packet isn't a duplicate */
                ret_val = insert(scanned, ntohl(dest_sockaddr.sin_addr.s_addr));
                if( ret_val == OK ) 
                {
                    rtt = recv_time.tv_sec + 
                    recv_time.tv_usec/1000000 - rtt_base - hostinfo->header->transaction_id/1000;
                    /* Using algorithm described in Stevens' Unix Network Programming */
                    delta = rtt - srtt;
                    srtt += delta / 8;
                    if(delta < 0.0) delta = - delta;
                    rttvar += (delta - rttvar) / 4 ;
                            
                    if (verbose) 
                        v_print_hostinfo(dest_sockaddr.sin_addr, hostinfo, sf, hr);
                    else if (dump) 
                        d_print_hostinfo(dest_sockaddr.sin_addr, hostinfo);
                    else if (etc_hosts)
                        l_print_hostinfo(dest_sockaddr.sin_addr, hostinfo, 0);
                    else if (lmhosts)
                        l_print_hostinfo(dest_sockaddr.sin_addr, hostinfo, 1);
                    else 
                        print_hostinfo(dest_sockaddr.sin_addr, hostinfo,sf);
                };

                // Fixed memory leak point 1
                // hostinfo malloc at parse_resones() in statusq.c ( line 158 )
                free( hostinfo->names);
                hostinfo->names = NULL;
                free( hostinfo->header);
                hostinfo->header = NULL;
                free( hostinfo->footer);
                hostinfo->footer = NULL;
                free( hostinfo );
                hostinfo = NULL;
                
                if( ret_val == MALLOC_FAILED )
                {
                    err_print("Malloc failed", quiet);
                    goto FREE_SCANNED_LIST;
                }
            };

            FD_ZERO(fdsr);
            FD_SET(sock, fdsr);		

            /* check if send_interval time passed since last send */
            gettimeofday(&current_time, NULL);
            timersub(&current_time, &last_send_time, &diff_time);
            send_ok = timercmp(&diff_time, &send_interval, >=);
                    
                
            if(more_to_send && FD_ISSET(sock, fdsw) && send_ok) 
            {
                if(targetlist) 
                {
                    if(fgets(str, 80, targetlist)) 
                    {
                        if(!inet_aton(str, next_in_addr)) 
                        {
                            /* if(!inet_pton(AF_INET, str, next_in_addr)) { */
                            fprintf(stderr,"%s - bad IP address\n", str);
                        } 
                        else 
                        {
                            ret_val = in_list(scanned, ntohl(next_in_addr->s_addr));
                            if( ret_val == N_OK ) 
                            {
                                send_query(sock, *next_in_addr, rtt_base);
                            }
                            else if( ret_val == MALLOC_FAILED )
                            {
                                err_print("Malloc failed", quiet);
                                goto FREE_SCANNED_LIST;
                            }
                        }
                    } 
                    else 
                    {
                        if(feof(targetlist)) 
                        {
                            more_to_send=0; 
                            FD_ZERO(fdsw);
                            /* timeout is in milliseconds */
                            select_timeout.tv_sec = timeout / 1000;
                            select_timeout.tv_usec = (timeout % 1000) * 1000; /* Microseconds */
                            continue;
                        } 
                        else 
                        {
                            snprintf(errmsg, 80, "Read failed from file %s", filename);
                            //err_die(errmsg, quiet);
                            err_print(errmsg, quiet);
                            goto FREE_NEXT_IN_ADDR;
                        }
                    }
                } 
                else if(next_address(&range, prev_in_addr, next_in_addr) ) 
                {
                    ret_val = in_list(scanned, ntohl(next_in_addr->s_addr)); 
                    if( ret_val == N_OK ) 
                    {
                        send_query(sock, *next_in_addr, rtt_base);
                    }
                    else if( ret_val == MALLOC_FAILED )
                    {
                        err_print("Malloc failed", quiet);
                        goto FREE_SCANNED_LIST;
                    }

                    prev_in_addr=next_in_addr;
                    /* Update last send time */
                    gettimeofday(&last_send_time, NULL); 
                } 
                else 
                { 
                    /* No more queries to send */
                    more_to_send=0; 
                    FD_ZERO(fdsw);
                  
                    /* timeout is in milliseconds */
                    select_timeout.tv_sec = timeout / 1000;
                    select_timeout.tv_usec = (timeout % 1000) * 1000; /* Microseconds */
                    continue;
                };
            };	
              
            if(more_to_send) 
            {
                FD_ZERO(fdsw);
                FD_SET(sock, fdsw);
            };
        };

        if (i>=retransmits) break; /* If we are not going to retransmit
                     we can finish right now without waiting */

        rto = (srtt + 4 * rttvar) * (i+1);

        if ( rto < 2.0 ) rto = 2.0;
        if ( rto > 60.0 ) rto = 60.0;
        gettimeofday(&now, NULL);
            
        if(now.tv_sec < (transmit_started.tv_sec+rto)) 
          sleep((transmit_started.tv_sec+rto)-now.tv_sec);
        prev_in_addr = NULL ;
        more_to_send=1;
        FD_ZERO(fdsw);
        FD_SET(sock, fdsw);
        FD_ZERO(fdsr);
        FD_SET(sock, fdsr);
    };

    
    if( rename( NBT_RESULT_TMP_FILE_PATH , NBT_RESULT_FILE_PATH  ) != 0 )
    {
        NBT_DEBUG( nbt_debug , "Rename %s to %s Failed\n",NBT_RESULT_TMP_FILE_PATH ,NBT_RESULT_FILE_PATH );
    }

    if( remove( filename ) == -1 )
    {
        NBT_DEBUG( nbt_debug , "Remove %s Failed\n",filename );
    }

    if( targetlist ) 
    {
        fclose(targetlist);
    }

    // Fixed memory leak point 2 
FREE_SCANNED_LIST:
    delete_list(scanned);   // malloc at line 659 ( nbtscan.c )
    scanned = NULL;
FREE_BUFF:
    free( buff );           // malloc at line 631 ( nbtscan.c )
    buff = NULL;
FREE_NEXT_IN_ADDR:
    free( next_in_addr );   // malloc at line 623 ( nbtscan.c )
    next_in_addr = NULL;
FREE_FDSW:
    free( fdsw );           // malloc at line 607 ( nbtscan.c )
    fdsw = NULL;
FREE_FDSR:
    free( fdsr );           // malloc at line 597 ( nbtscan.c )
    fdsr = NULL;
SOCK_ERROR:
    close( sock );          // create at line 577
IPV6_SEARCH_DONE:   // UNIHAN MOD , PROD00201681
CLOSE_LOG_FILE:
    close_debuglog_fp();

    exit(0);
};

