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

#include <sys/types.h>
#include <sys/ioctl.h>
#include <sys/param.h>
#include <sys/epoll.h>
#include <bits/time.h>
#include <unistd.h>
#include <stdlib.h>

#ifndef CLOCK_MONOTONIC
#define CLOCK_MONOTONIC           1  //usr/include/bits/time.h
#define CLOCK_REALTIME            0
#endif

#include <sys/timerfd.h>
#include <errno.h>
#include "defs.h"
#include "debug.h"

#include "timers.h"


int create_rxmt_timer ( struct mvif * v, struct listaddr *g, cfunc_t  callback )
{	
   int rc;
   
        if ( ( ! (v && g) ) )
	{
	  log_msg(LOG_ERR, 0, "%s sanity failure, null in params", __func__ ) ;
	}
	IF_DEBUG(DEBUG_TIMER)
        log_msg(LOG_DEBUG,0, 
	       "create_rxmt_timer for G=%s ob %s", sa6_fmt(&g->mcast_group), v->mvif_name);
	rc=g->group_rxmt_timer = timerfd_create( CLOCK_MONOTONIC ,0 ) ;
	if (rc < 0) 
        {
                log_msg(LOG_ERR, errno, "cannot create rxmt_timer"); 
        }
   
	g->group_rxmt_timer_event.events=EPOLLIN;
	
	g->rxmt_timer_callback.g=g;
	g->rxmt_timer_callback.v=v;
	g->rxmt_timer_callback.mifi = find_vifi_by_address(v);
	g->rxmt_timer_callback.mcast_group = &g->mcast_group;
	
	g->rxmt_timer_callback.callback=callback;
        g->group_rxmt_timer_event.data.ptr= &g->rxmt_timer_callback;
	g->ma_llqc = (v->uv_fastleave) ? 0 : v->uv_mld_llqc;	/* Prepare retry count of Group Specific queries */
     

       
       
	
        /* add timer  to the timers poll set/queue */
	rc=epoll_ctl(epfd, EPOLL_CTL_ADD, g->group_rxmt_timer,  &g->group_rxmt_timer_event);
	if (rc < 0) 
        {
                log_msg(LOG_ERR, errno, "cannot add  rxmt_timer for G=%s on %s to epoll list", sa6_fmt(&g->mcast_group), v->mvif_name );
       }
      return 0;
}

int start_rxmt_timer (struct mvif *v, struct listaddr *g, short sec)
{
    struct itimerspec  tspec;
	short secs;
	int rc;
	
	if ( ( ! (v && g) ) )
	{
	  log_msg(LOG_ERR, 0, "%s sanity failure, null in params", __func__ ) ;
	}
	
	IF_DEBUG(DEBUG_TIMER)
	    log_msg(LOG_DEBUG,0,
		  "start_rxmt_timer for G=%s on %s with period=%d\n", sa6_fmt(&g->mcast_group ), v->mvif_name, sec); 
	if (g->group_rxmt_timer <=0)
	{
	   log_msg(LOG_CRIT, 0, "Rxmt_timer was not created for G=%s on %s", sa6_fmt(&g->mcast_group ), v->mvif_name);
	   
	}
	
	 /*minimum leave - 1sec, retransmit delay could not be . 10- sec */
	tspec.it_interval.tv_sec=sec; // timer interval
	tspec.it_interval.tv_nsec=random() % 10000;
	tspec.it_value.tv_sec=sec; // timer first expire at tv_sec
	tspec.it_value.tv_nsec=random() % 100000;
	rc=timerfd_settime(g->group_rxmt_timer, 0, &tspec, NULL);
	if (rc < 0) 
        {
                log_msg(LOG_ERR, errno, "cannot start rxmt_timer ");
         }
         g->ma_time=time(NULL);
  return 0;
}

int stop_rxmt_timer (struct mvif *v, struct listaddr *g)
{
        struct itimerspec  tspec;
	int rc;
	// Sanity
	if ( ( ! (v && g) ) )
	{
	  log_msg(LOG_ERR, 0, "%s sanity failure, null in params", __func__ ) ;
	}
	                 
	IF_DEBUG(DEBUG_TIMER)
	    log_msg(LOG_DEBUG, 0, "Stopping rxmt timer of G ? S =%s  on %s ", sa6_fmt(&g->mcast_group ),  
	                  v->mvif_name);
	   
	memset(&tspec,0, sizeof(tspec));
	
	if (g->group_rxmt_timer)
	{
	  
 	    /* report received :-  delete leave timer from timers poll set */
	    rc= timerfd_settime(g->group_rxmt_timer, 0, &tspec, NULL); // tspec Zero stops the timer
	    if (rc < 0) 
             {
                log_msg(LOG_ERR, errno, "cannot stop  rxmt_timer on %s ", v->mvif_name );
     
             }	
		
	}
	return 0;
}

int create_back_to_mldv2_timer (struct mvif *v, struct listaddr *g, cfunc_t  callback)
{
	/* Create timer to wait for all groups became v=MLDv2  */
	// Sanity
	if ( ( ! (v && g) ) )
	{
	  log_msg(LOG_ERR, 0, "%s sanity failure, null in params", __func__ ) ;
	}
	
	 g->uv_back2mldv2_timer=timerfd_create( CLOCK_MONOTONIC ,0) ;
	 if (g->uv_back2mldv2_timer <=0 ) {
	      log_msg(LOG_ERR,0, "Fain to create uv_back_to_mldv2_timer for G=%s on %s , cannot return to mldv2",sa6_fmt(&g->mcast_group), v->mvif_name);
	      return -1;
         }
         
         g->uv_back2mldv2_timer_callback.q_time =
	               MLD6_OLDER_VERSION_HOST_PRESENT;
         g->uv_back2mldv2_timer_callback.mcast_group = &g->mcast_group;
        
         

         
         g->uv_back2mldv2_time_event.data.ptr=&g->uv_back2mldv2_timer_callback ;
         g->uv_back2mldv2_timer_callback.callback= callback;
	 g->uv_back2mldv2_timer_callback.v = v;
	 g->uv_back2mldv2_timer_callback.g = g;
	 g->uv_back2mldv2_timer_callback.mifi = find_vifi_by_address( v) ;
	 g->uv_back2mldv2_timer_callback.source = NULL;
	 if (epoll_ctl( epfd, EPOLL_CTL_ADD, g->uv_back2mldv2_timer, &g->uv_back2mldv2_time_event ) < 0 )
         {
	     log_msg(LOG_ERR,0, "ERROR - can not activate  back_to_mldv2_timer  forG=%s on %s,cannot return to mldv2", sa6_fmt(&g->mcast_group), v->mvif_name);
	     return -1;
         }
}

int start_back_to_mldv2_timer ( struct mvif * v, struct listaddr * g, int secs)
{
        struct itimerspec  tspec;
	int rc;
	/*
	 * The timer is re-set whenever a
   new MLDv1 Report is received for that multicast address.*/
	
   tspec.it_interval.tv_sec=secs; // timer interval
	tspec.it_interval.tv_nsec=random() % 100000;
	tspec.it_value.tv_sec=secs; // timer initial expiration
	tspec.it_value.tv_nsec=0;
	
        IF_DEBUG(DEBUG_TIMER)
	log_msg(LOG_DEBUG, 0, "Start Back_To_MLDv2_Timer, period=%d\n", secs);
	rc=timerfd_settime(g->uv_back2mldv2_timer, 0, &tspec, NULL);
        if ( rc <0 ) {
	    log_msg(LOG_ERR, errno, "settime uv_back_to_mldv2_time for G=%s on %s but cannot return to mldv2",sa6_fmt(&g->mcast_group), v->mvif_name );
	    return -1;
       }
      g->ma_time=time(NULL);
} 
int stop_back_to_mldv2_timer ( struct mvif *v,struct listaddr * g )
{	
	struct itimerspec  tspec;
	int rc;
	memset(&tspec,0, sizeof(tspec) );
        IF_DEBUG(DEBUG_TIMER)
	log_msg(LOG_DEBUG, 0,"Stop Back_To_MLDv2_Timer\n");
	if (g->uv_back2mldv2_timer)
	{	
	      rc=timerfd_settime(g->uv_back2mldv2_timer, 0, &tspec, NULL); // tspec Zero stops the timer
	      if ( rc <0 ) 
	          log_msg(LOG_ALERT, errno, "Cannot stop to uv_back_to_mldv2_timer  for G=%s on %s", sa6_fmt(&g->mcast_group), v->mvif_name);
             
	}
	else
	{
	       log_msg(LOG_ALERT, 0, "Invalid uv_back_to_mldv2_timer  cannot  stop it ");
	}
	     
	return 0;
}
int delete_back_to_mldv2_timer ( struct mvif *v, struct listaddr * g)
{	
	struct itimerspec  tspec;
	int rc;
	memset(&tspec,0, sizeof(tspec) );
        IF_DEBUG(DEBUG_TIMER)
	log_msg(LOG_DEBUG, 0,"Delete Back_To_MLDv2_Timer\n");
	if (g->uv_back2mldv2_timer)
	{	
	      rc = timerfd_settime(g->uv_back2mldv2_timer, 0, &tspec, NULL); // tspec Zero stops the timer
	      
	      if ( rc <0 ) 
	          log_msg(LOG_ALERT, errno, " cannot stop to uv_back_to_mldv2_timer ");
	      
	      if ( (rc=epoll_ctl(epfd, EPOLL_CTL_DEL,g->uv_back2mldv2_timer, &g->uv_back2mldv2_time_event ) ) <= 0) 
	      {
		  log_msg(LOG_ALERT,errno, "cannot disable  stop_back_to_mldv2_timer ");
			         
	      }
	 	
	else
	{
	       log_msg(LOG_ALERT, 0, "Invalid uv_back_to_mldv2_timer  cannot  stop it ");
	}	
	
	}	
	return 0;
}
int create_report_timer ( struct mvif * v, struct listaddr *g, cfunc_t  callback)
{	
        int rc;
	// Sanity
	if ( ( ! (v && g) ) )
	{
	  log_msg(LOG_ERR, 0, "%s sanity failure, null in params", __func__ ) ;
	}
	
        IF_DEBUG(DEBUG_TIMER)
	log_msg(LOG_DEBUG,0,"create_report_timer for G=%s on %s",sa6_fmt(&g->mcast_group),  v->mvif_name );
	rc=g->group_report_timer = timerfd_create( CLOCK_MONOTONIC , 0) ;
	if (rc <= 0) 
        {
            log_msg(LOG_ERR, errno, "cannot create group_report_timer ");
        }
	 
	 g->report_timer_callback.g = g;
	 g->report_timer_callback.v = v;
	 g->report_timer_callback.mifi = find_vifi_by_address(v);
	 g->report_timer_callback.mcast_group = &g->mcast_group;
	 
	 g->group_report_timer_event.events=EPOLLIN;
         g->report_timer_callback.callback= (void *) callback;
         g->group_report_timer_event.data.ptr = &g->report_timer_callback;
         /* add timer  to the timers poll set/queue */
	rc=epoll_ctl(epfd, EPOLL_CTL_ADD, g->group_report_timer,  &g->group_report_timer_event);
	if (rc < 0) 
        {
            log_msg(LOG_ERR, errno, "cannot add group_report_timer to epoll list");
        }
        return 0;
}
/*
 * Time out muticast address or a source(s)
*/
int start_report_timer ( struct mvif * v , struct listaddr *g, short secs)
{
        struct itimerspec  tspec;
	int rc;
	
	// Sanity
	if ( ( ! (v && g) ) )
	{
	  log_msg(LOG_ERR, 0, "%s sanity failure, null in params", __func__ ) ;
	}

	
        IF_DEBUG(DEBUG_TIMER)
        log_msg(LOG_DEBUG,0,
	       "start_report_timer of G=%s on %s for period=%d secs\n", sa6_fmt(&g->mcast_group ), v->mvif_name, secs); 
	if (g->group_report_timer <=0 )
	{
		        log_msg(LOG_ALERT,0, "The group already exists, but report timer was not created as it should");
			
	}
        /* Even if the timer was started - just renew the timer */
	tspec.it_interval.tv_sec=secs; // timer period
	tspec.it_interval.tv_nsec=random() % 100000;
	tspec.it_value.tv_sec=secs; // timer expiration
	tspec.it_value.tv_nsec=random() % 100000;
	rc = timerfd_settime(g->group_report_timer, 0, &tspec, NULL);
	if (rc <0 )
	{
	    log_msg(LOG_ERR, errno, "BUG The group G=%s on %s , cannot start report timer at %d secs.%d nsecs ", sa6_fmt(&g->mcast_group ), v->mvif_name, 
		     tspec.it_value.tv_sec, tspec.it_value.tv_nsec );
	}
	IF_DEBUG(DEBUG_TIMER)
	log_msg(LOG_DEBUG,0, 
          "Restarted Report Timer for G=%s on %s,  after in %lu secs ",
         sa6_fmt( &(g->mcast_group)) , v->mvif_name , time(NULL) - g->ma_time );
	g->ma_time=time(NULL);
} 


int stop_report_timer ( struct mvif * v, struct listaddr *g)
{	
        struct itimerspec  tspec;
	int rc;
	
	// Sanity
	if ( ( ! (v && g) ) )
	{
	  log_msg(LOG_ERR, 0, "%s sanity failure, null in params", __func__ ) ;
	}
	
	memset(&tspec , 0 , sizeof(tspec) );
	IF_DEBUG(DEBUG_TIMER)
	    log_msg(LOG_DEBUG, 0, "Stopping report timer of G ? S =%s on %s ", sa6_fmt(&g->mcast_group ),  
	                  v->mvif_name);
	
	if (g->group_report_timer)
	{
 	        /* report received :-  delete leave timer from timers poll set */
		rc=timerfd_settime(g->group_report_timer, 0, &tspec, NULL); // tspec Zero stops the timer
		if (rc <0 )
	      {
		      log_msg(LOG_ERR, errno, "BUG: cannot stop report timer of G =%s on %s", sa6_fmt(&g->mcast_group ),  v->mvif_name );
	      }
	}
	
	return 0;
}

int delete_report_timer ( struct mvif * v, struct listaddr *g)
{
  struct itimerspec  tspec;
  int rc;
  
  
  // Sanity
  if ( ( ! (v && g) ) )
  {
	  log_msg(LOG_ERR, 0, "%s sanity failure, null in params", __func__ ) ;
  }
  memset(&tspec , 0 , sizeof(tspec) );
  if (g->group_report_timer)
  {
       IF_DEBUG(DEBUG_TIMER)
	    log_msg(LOG_DEBUG, 0, "Deleting report timer of G ? S =%s on %s ", sa6_fmt(&g->mcast_group ),  
	                  v->mvif_name);
      
      rc=timerfd_settime(g->group_report_timer, 0, &tspec, NULL);
      if (rc < 0 )
      { 
          log_msg(LOG_ERR, errno, "cannot stop group G=%s report_timer on %s", sa6_fmt(&g->mcast_group ), v->mvif_name);
      } 
      
      if (epoll_ctl(epfd, EPOLL_CTL_DEL,g->group_report_timer, &g->group_report_timer_event ) < 0) 
      {
	   log_msg(LOG_ERR,errno, "cannot remove group_report_timer of G =%s from epoll list on %s", sa6_fmt(&g->mcast_group ), v->mvif_name );
       }
       close(g->group_report_timer);
       g->group_report_timer=0;
       
  }
}
int delete_rxmt_timer (  struct mvif * v, struct listaddr *g)
{ 
  struct itimerspec  tspec;
  int rc;
  
  // Sanity	
  if ( ( ! (v && g) ) )
  {
	  log_msg(LOG_ERR, 0, "%s sanity failure, null in params", __func__ ) ;
  }
	
  memset(&tspec,0, sizeof(tspec) );
  
  if (g->group_rxmt_timer)
  {
      
       IF_DEBUG(DEBUG_TIMER)
	    log_msg(LOG_DEBUG, 0, "Deleting rxmt timer of G ? S =%s on %s ", sa6_fmt(&g->mcast_group ),  
	                  v->mvif_name);
      rc=timerfd_settime(g->group_rxmt_timer, 0, &tspec, NULL);
      if (rc < 0 )
      { 
          log_msg(LOG_ERR, errno, "cannot stop group G=%s_rxmt_timer on %s", sa6_fmt(&g->mcast_group ), v->mvif_name );
      }
      if (epoll_ctl(epfd, EPOLL_CTL_DEL,g->group_rxmt_timer, &g->group_rxmt_timer_event ) < 0) 
      {
          log_msg(LOG_ERR, errno, "cannot remove group G=%s_rxmt_timer from epoll list on %s", sa6_fmt(&g->mcast_group ),  v->mvif_name);
      }
      close(g->group_rxmt_timer);
      g->group_rxmt_timer=0;
  }
}

int create_filterMode_timer (struct mvif *v,  struct listaddr * g)
{	
	g->mldv2_filterMode_timer = timerfd_create( CLOCK_MONOTONIC , 0 ) ;
	if ( ! g->mldv2_filterMode_timer )
	{
	  log_msg(LOG_DEBUG, errno, "Cannot create Filter Mode  Timer  for %s on %s", sa6_fmt(&g->mcast_group ),  
	                  v->mvif_name);
	}
	g->filterMode_timer_event.events = EPOLLIN;
	g->filterMode_timer_event.data.ptr = &g->filterMode_timer_callback;
        g->filterMode_timer_callback.callback = ExpireFilterModeTimer;
	
	g->filterMode_timer_callback.g = g;
	g->filterMode_timer_callback.v = v;
	log_msg(LOG_DEBUG,0,"adding epoll event %p\n", g->filterMode_timer_event.data.ptr );
	/* add timer  to the timers poll set/queue */
	if (epoll_ctl( epfd, EPOLL_CTL_ADD, g->mldv2_filterMode_timer,  &g->filterMode_timer_event)  < 0 )
	{
	    log_msg(LOG_ERR, errno, "Cannot Add FilterMode Timer Event for %s on %s",sa6_fmt(&g->mcast_group ),  
	                  v->mvif_name);
	}
	IF_DEBUG(DEBUG_TIMER)
        log_msg(LOG_DEBUG,0,"Created FilterMode  Timer for %s on %",sa6_fmt(&g->mcast_group ),  
	                  v->mvif_name);
        return 0;
}
int start_filterMode_timer (struct mvif * v, struct listaddr * g, int secs )
{
      struct itimerspec  tspec;
	int rc;
	
        IF_DEBUG(DEBUG_TIMER)
	    log_msg(LOG_DEBUG,0,"starting FilterMode timer  with period=%d for %s on %s\n",secs,
		sa6_fmt(&g->mcast_group ),  
	                  v->mvif_name);
	if (g->mldv2_filterMode_timer <=0 )
	{
		        log_msg(LOG_ERR,0, "The multicast group already exists, but timer was not created as it should for %s on %s", 
			sa6_fmt(&g->mcast_group ),  
	                  v->mvif_name);
	}
        /* If no state change - just renew the timer */
        rc=secs;
	
	tspec.it_interval.tv_sec=secs; // timer period
	tspec.it_interval.tv_nsec=random() % 100000;
	tspec.it_value.tv_sec=secs ; // Initial timer expiration 
	tspec.it_value.tv_nsec=0;
	IF_DEBUG(DEBUG_TIMER)
		log_msg(LOG_DEBUG,0,"start_filterMode_timer  epfd=%d tfd=%d, interval=%u, nsec=%lu\n", epfd, g->mldv2_filterMode_timer,tspec.it_interval.tv_sec, tspec.it_interval.tv_nsec );
	rc=timerfd_settime(g->mldv2_filterMode_timer, 0, &tspec, NULL);
	if (rc <0 )
	{
	    log_msg(LOG_ERR,errno, "Cannot renew the  filterMode timer for %s on %s ",sa6_fmt(&g->mcast_group ),  
	                  v->mvif_name);
	}
	g->ma_time=time(NULL);
}


int stop_filterMode_timer ( struct mvif * v, struct listaddr * g)
{	struct itimerspec  tspec;
	memset(&tspec,0, sizeof(tspec) );
	IF_DEBUG(DEBUG_TIMER)
	    log_msg(LOG_DEBUG,0, "stop FilterMode timer for %s on %s", sa6_fmt(&g->mcast_group ),  
	                  v->mvif_name);
	if (g->mldv2_filterMode_timer)
	{
	      timerfd_settime(g->mldv2_filterMode_timer, 0, &tspec, NULL); // tspec Zero stops the timer
	}
	return 0;
}

int delete_filterMode_timer (struct mvif * v, struct listaddr * g )
{
    struct itimerspec  tspec;
    
    IF_DEBUG(DEBUG_TIMER)
    	log_msg(LOG_DEBUG,0, "delete FilterMode timerfor %s on %s", sa6_fmt(&g->mcast_group ),  
	                  v->mvif_name);
    memset(&tspec,0, sizeof(tspec) );
    if (g->mldv2_filterMode_timer)
	timerfd_settime(g->mldv2_filterMode_timer, 0, &tspec, NULL);
    if (epoll_ctl(epfd, EPOLL_CTL_DEL,g->mldv2_filterMode_timer, &g->filterMode_timer_event ) < 0) 
    {
	log_msg(LOG_ERR, errno, "cannot disable uv_filterMode_timer for %s on %s ", sa6_fmt(&g->mcast_group ),  
	                  v->mvif_name);
   }
    close(g->mldv2_filterMode_timer);
}


int create_genQuery_timer (mifi_t mifi, struct mvif *v, cfunc_t callback)
{	
	v->uv_genQuery_timer = timerfd_create( CLOCK_MONOTONIC , 0 ) ;
	if ( ! v->uv_genQuery_timer )
	{
	  perror("Cannot create Generic Queries  Timer");
	  log_msg(LOG_DEBUG,errno, "Cannot create Generic Queries  Timer on interface %s", v->mvif_name);
	}
	v->uv_genQuery_timer_event.events=EPOLLIN;
	v->uv_genQuery_timer_event.data.ptr=&v->uv_genQuery_timer_callback;
        v->uv_genQuery_timer_callback.callback = callback;
	v->uv_genQuery_timer_callback.mifi = mifi;
	v->uv_genQuery_timer_callback.v = v;
	//log_msg(LOG_DEBUG,0, "adding epoll event %p\n", v->uv_genQuery_timer_event.data.ptr );
	/* add timer  to the timers poll set/queue */
	if (epoll_ctl( epfd, EPOLL_CTL_ADD, v->uv_genQuery_timer,  &v->uv_genQuery_timer_event)  < 0 )
	{
	    perror("Cannot Add Generic Queries  Timer Event");
	}
	IF_DEBUG(DEBUG_TIMER)
	      log_msg(LOG_DEBUG,0, "Created Generic queries timer on interface %s/n", v->mvif_name);
        return 0;
}
int start_genQuery_timer ( struct mvif *v, int secs)
{
        struct itimerspec  tspec;
	int rc;
        IF_DEBUG(DEBUG_TIMER)
	    log_msg(LOG_DEBUG,0, "start Generic queries timer with period=%d on interface %s", secs, v->mvif_name ); 
	if (v->uv_genQuery_timer <=0 )
	{
		        log_msg(LOG_ERR,0, "The mvif already exists, but timer on %s was not created as it should", v->mvif_name );
	}
        /* If no state change - just renew the timer */
        rc=secs;
	tspec.it_interval.tv_sec=rc; // timer period
	tspec.it_interval.tv_nsec=random() % 100000;
	tspec.it_value.tv_sec=secs ; // Initial timer expiration 
	tspec.it_value.tv_nsec=random() % 100000;
	IF_DEBUG(DEBUG_TIMER)
	    log_msg(LOG_DEBUG,0, "start_genQuery_timer  on %s epfd=%d tfd=%d, interval=%u, nsec=%lu",  v->mvif_name, epfd, 
		    v->uv_genQuery_timer,tspec.it_interval.tv_sec, tspec.it_interval.tv_nsec );
	rc=timerfd_settime(v->uv_genQuery_timer, 0, &tspec, NULL);
	if (rc <0 )
	{
	    log_msg(LOG_ERR,errno, "Cannot renew the  genQuery timer on interface %s/n", v->mvif_name);
	}
	v->uv_q_time=time(NULL);
}


int stop_genQuery_timer ( struct mvif *v)
{	
        struct itimerspec  tspec;
	memset(&tspec,0, sizeof(tspec) );
	IF_DEBUG(DEBUG_TIMER)
	    log_msg(LOG_DEBUG,0, "stop Generic queries timer on interface %s/n", v->mvif_name);
	if (v->uv_genQuery_timer)
	{
	      timerfd_settime(v->uv_genQuery_timer, 0, &tspec, NULL); // tspec Zero stops the timer
	}
	return 0;
}

int delete_genQuery_timer ( struct mvif *v)
{
    struct itimerspec  tspec;
    IF_DEBUG(DEBUG_TIMER)
	  log_msg(LOG_DEBUG,0, "delete Generic queries timer on interface %s/n", v->mvif_name);
    memset(&tspec,0, sizeof(tspec) );
    if (v->uv_genQuery_timer)
	timerfd_settime(v->uv_genQuery_timer, 0, &tspec, NULL);
    if (epoll_ctl(epfd, EPOLL_CTL_DEL,v->uv_genQuery_timer, &v->uv_genQuery_timer_event ) < 0) 
    {
	log_msg(LOG_ERR, errno, "cannot disable uv_genQuery_timer on interface %s/n", v->mvif_name);
   }
    close(v->uv_genQuery_timer);
}
