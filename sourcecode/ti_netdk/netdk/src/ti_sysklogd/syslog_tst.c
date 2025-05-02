/* Program to test daemon logging. */

/*
 * Sat Dec 11 12:07:50 CST 1993:  Dr. Wettstein
 *	Compiles clean with -Wall.  Renamed for first public distribution.
 *	Use this freely but if you make a ton of money with it I
 *	expect a cut...  :-)
 *
 * Thu Jan  6 11:52:10 CST 1994:  Dr. Wettstein
 *	Added support for reading getting log input from the standard
 *	input.  To activate this use a - as the single arguement to the
 *	the program.  Note that there is a hack in the code to pause
 *	after each 1K has been written.  This eliminates what appears
 *	to be a problem with overrunning a UNIX domain socket with
 *	excessive amounts of input.
 */
/*----------------------------------------------------------------------------
// Copyright 2007, Texas Instruments Incorporated
//
// This program has been modified from its original operation by Texas Instruments
// to do the following:
//
// 1. -h option to list out facility, priorities, options
// 2. command line add facility, priorities, options
//
// THIS MODIFIED SOFTWARE AND DOCUMENTATION ARE PROVIDED
// "AS IS," AND TEXAS INSTRUMENTS MAKES NO REPRESENTATIONS
// OR WARRENTIES, EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED
// TO, WARRANTIES OF MERCHANTABILITY OR FITNESS FOR ANY
// PARTICULAR PURPOSE OR THAT THE USE OF THE SOFTWARE OR
// DOCUMENTATION WILL NOT INFRINGE ANY THIRD PARTY PATENTS,
// COPYRIGHTS, TRADEMARKS OR OTHER RIGHTS.
//
// These changes are covered as per original license
//-----------------------------------------------------------------------------*/
#define SYSLOG_NAMES
#include <stdio.h>
#include <string.h>
#include <syslog.h>
#include <unistd.h>
#include <sys/param.h>
#include <stdlib.h>             /* exit() */

extern int main(int, char **);

/* possible options to syslog(3) used for command line */
CODE optionnames[] = {
    {"pid", LOG_PID},
    {"cons", LOG_CONS},
    {"odelay", LOG_ODELAY},
    {"ndelay", LOG_NDELAY},
    {"nowait", LOG_NOWAIT},
    {"perror", LOG_PERROR},
    {NULL, -1}
};

/* convenience function to cycle through elements of arrays of CODE, ptr, to
 * find str. returns value on success,-1 on failure */
int searchnames(CODE *ptr, char *str) {
    
    while (ptr->c_name != NULL) {
        if (strcasecmp(ptr->c_name, str) == 0) {
            return(ptr->c_val);
        }
        ptr++;
    }

    return(-1);
} /* end searchnames */
    
int main(int argc, char *argv[])
{
    auto char *nl,
        bufr[512];
    auto int logged = 0;

    if (argc > 1)
	{
            if ( (*argv[1] == '-') && (*(argv[1]+1) == '\0') )
		{
                    openlog("DOTEST", LOG_PID, LOG_DAEMON);
                    while (!feof(stdin))
                        if ( fgets(bufr, sizeof(bufr), stdin) != \
                             (char *) 0 )
                            {
                                if ( (nl = strrchr(bufr, '\n')) != \
                                     (char *) 0)
                                    *nl = '\0';
                                syslog(LOG_INFO, bufr);
                                logged += strlen(bufr);
                                if ( logged > 1024 )
                                    {
                                        sleep(1);
                                        logged = 0;
                                    }
					
                            }
		}
            else if (*argv[1] == '-' && *(argv[1]+1) == 'h') { /* help */
                int j = 0;

                printf("Usage: %s [facility] [priority] [options ...] message\n", argv[0]);
                printf("Facilitiy:\n");
                do {
                    printf(" log_%s ", facilitynames[j].c_name);
                } while (facilitynames[++j].c_name != NULL);
                j = 0;
                printf("\n\nPriority:\n");
                do {
                    printf(" log_%s ", prioritynames[j].c_name);
                } while (prioritynames[++j].c_name != NULL);
                j = 0;
                printf("\n\nOptions:\n");
                do {
                    printf(" log_%s ", optionnames[j].c_name);
                } while (optionnames[++j].c_name != NULL);
                exit(0);
            }
            else {
                int facility = -1, priority = -1, options = 0;
                int i = 1, j
;
/* handle facility, priority or option arguments prior to mesg */
                while (i < argc && strncasecmp("LOG_", argv[i], 4) == 0) {

                    j = searchnames(facilitynames, argv[i]+4);
                    if (j != -1) {
                        facility = j;
                        i++;
                        continue;
                    }
                    j = searchnames(prioritynames, argv[i]+4);
                    if (j != -1) {
                        priority = j;
                        i++;
                        continue;
                    }
                    j = searchnames(optionnames, argv[i]+4);
                    if (j != -1) {
                        options ^= j;
                        i++;
                        continue;
                    }
                }

                if (facility == -1) facility = LOG_USER; /* set default */
                if (priority == -1) priority = LOG_INFO; /* if not
                                                          * user set */

                openlog(argv[0], options, facility); /* start logging */
                while (i < argc) { /* from command line parameters */
                    syslog(facility|priority, argv[i++]);
		}
            }
	}
    else
	{
            openlog("DOTEST", LOG_PID, LOG_DAEMON);
            syslog(LOG_EMERG, "EMERG log.");
            syslog(LOG_ALERT, "Alert log.");
            syslog(LOG_CRIT, "Critical log.");
            syslog(LOG_ERR, "Error log.");
            syslog(LOG_WARNING, "Warning log.");
            syslog(LOG_NOTICE, "Notice log.");
            syslog(LOG_INFO, "Info log.");
            syslog(LOG_DEBUG, "Debug log.");
            closelog();
            return(0);
	}

    return(0);
}
