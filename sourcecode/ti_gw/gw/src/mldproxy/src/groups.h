/*


  Copyright(c) 2012 Intel Corporation. All rights reserved.

  Redistribution and use in source and binary forms, with or without 
  modification, are permitted provided that the following conditions 
  are met:

	* Redistributions of source code must retain the above copyright 
	  notice, this list of conditions and the following disclaimer.
	* Redistributions in binary form must reproduce the above copyright 
	  notice, this list of conditions and the following disclaimer in 
	  the documentation and/or other materials provided with the 
	  distribution.
	* Neither the name of Intel Corporation nor the names of its 
	  contributors may be used to endorse or promote products derived 
	  from this software without specific prior written permission.

  THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS 
  "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT 
  LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR 
  A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT 
  OWNER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, 
  SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT 
  LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, 
  DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY 
  THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT 
  (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE 
  OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.

*/
#ifndef __MLD_PROXY_GROUPS_H_
	#define __MLD_PROXY_GROUPS_H_
	#include "vif.h"

	typedef struct in6_addr (*LIST_OF_SOURCES_IN_REPORT)[1];
	typedef struct sockaddr_in6 * IP_ADDRESS;

extern struct listaddr *make_new_group (mifi_t mifi, IP_ADDRESS group_multicast_address, u_int8_t mld_mode);

extern void delete_group (mifi_t mifi, struct listaddr *group);

extern void delete_group_upstream (mifi_t mifi, IP_ADDRESS group_multicast_address);
extern int mld_merge_with_upstream (mifi_t mifi, IP_ADDRESS mcast_group_address, uint8_t mld_version,
									IP_ADDRESS source,
									uint8_t filter_mode);

extern struct listaddr * find_group_in_list (struct mvif *v, struct listaddr *group);

extern struct listaddr * find_multicast_group (struct mvif *v, IP_ADDRESS group);
extern struct listaddr * find_requested_mcast_transmitter __P((struct mvif * v,
							   IP_ADDRESS  mcast_group_address,
							   struct listaddr * wan_group,
							   IP_ADDRESS  mcast_sender
															  ));

extern struct listaddr *make_new_source (mifi_t mifi, struct listaddr *group,
						 IP_ADDRESS  group_multicast_address,
						 IP_ADDRESS  required_source_address,
						 uint8_t filter_mode);

extern int delete_source (mifi_t mifi, struct listaddr *group, struct listaddr *source);

extern int delete_source_upstream (mifi_t mifi,
								   IP_ADDRESS group_multicast_address,
								   IP_ADDRESS source_address);
extern int delete_group_sources_upstream (mifi_t mifi,
										  struct listaddr *group);
extern int delete_group_sources (mifi_t mifi, struct listaddr *group);
extern void start_all_sources_timers (struct mvif *v, struct listaddr *group, u_int16 timeout);
extern void stop_all_sources_timers (struct mvif *v, struct listaddr *group);
extern int set_sources_mode( mifi_t mifi, struct listaddr *group, uint16_t numsrc, 
				LIST_OF_SOURCES_IN_REPORT,
				uint8_t v2_report_record_type);
#endif
