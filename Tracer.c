#include "wrapper.h"
#include <stdlib.h>
#include <omp.h>
#include <pthread.h>

extern int Record ();
extern Record_Event Event_init ();
extern long long unsigned gettime ();
extern void finiTrace ();

extern void initTrace();
extern long long unsigned Trace_start_time;
extern int PAPI;
extern int EventSet;
extern int NUM_EVENT;
extern char PAPI_EVENT[PAPI_MAX_EVENT_NUM][PAPI_MAX_EVENT_LEN];

int GetProcName(char *);
 
__attribute__((constructor)) void Trace_start()
{
#ifdef DEBUG
	printf_d ("<----------------------------------------trace start--------------------------------------->\n");
#endif
	
	char cmd[20];
	
	GetProcName (cmd);
	if (strcmp (cmd, "proc") == 0||strcmp (cmd, "mpdlistjobs") == 0||strcmp (cmd, "grep") == 0||strcmp (cmd, "awk") == 0||strcmp (cmd, "sort") == 0||strcmp (cmd, "uniq") == 0||strcmp (cmd, "sh") == 0||strcmp (cmd, "attacher") == 0)
	{ 
//		printf_d ("this is proc!!!!!!!!!!!!!!!!!!!!!!!%u\n", getpid());
//		printf_d ("|%s\n", cmd);
	}
	else
	{
		char *cupti, *papi, *papi_thread;
	
		cupti = getenv ("CUPTI");
		papi = getenv ("PAPI");
		papi_thread = getenv ("PAPI_THREAD");
		
		if (cupti != NULL && strcmp (cupti, "ON") == 0)
		{
			printf_d ("<-----------------------CUPTI trace init---------------------------->\n");
			initTrace();
		}
		if (papi != NULL && strcmp (papi, "ON") == 0)
		{
			int i, retval; 
   			char errstring[PAPI_MAX_STR_LEN];
		
			int event_codes_temp[PAPI_MAX_EVENT_NUM];
			int *event_codes;
			FILE *papi_config;
			papi_config = fopen_d ("PAPI.config", "r");

			if (papi_config == NULL)
			{
				printf_d("[PAPI]Can't open PAPI.config, now exitting...\n");
				exit (1);
			}
			else 
			{
				if((retval = PAPI_library_init(PAPI_VER_CURRENT)) != PAPI_VER_CURRENT )
	       		{
					printf_d ("[PAPI]Error: %s\n", errstring);
					exit(1);
				}
				fflush (stdout);
				
				//check whether papi_thread is on
				if (papi_thread  != NULL && strcmp (papi_thread, "ON") == 0)
				{
					if ((retval = PAPI_thread_init(pthread_self)) != PAPI_OK)
					{
						printf_d ("[PAPI]Thread init error!\n");
						exit (1);
					}
					else 
						printf_d ("[PAPI]PAPI thread init succeed!\n");
				}

				for (i = 0; fscanf (papi_config, "%s", PAPI_EVENT[i]) != EOF; i++)
				{
					retval = PAPI_event_name_to_code (PAPI_EVENT[i], &event_codes_temp[i]);
				
					if( retval != PAPI_OK )
						printf_d ("[PAPI]PAPI_event_name_to_code failed, retval = %d\n", retval );

				}
				fclose_d (papi_config);
			
				NUM_EVENT = i;
				if (NUM_EVENT != 0)
				{
					PAPI = PAPI_ON;
					EventSet = PAPI_NULL;

					event_codes = (int *)calloc (sizeof (int *), NUM_EVENT);
					for (i = 0; i < NUM_EVENT; i++)
						event_codes[i] = event_codes_temp[i];

					/* Creating event set   */
					if ((retval=PAPI_create_eventset(&EventSet)) != PAPI_OK)
						ERROR_RETURN(retval);

					/* Add the array of events PAPI_TOT_INS and PAPI_TOT_CYC to the eventset*/
					if ((retval=PAPI_add_events(EventSet, event_codes, NUM_EVENT)) != PAPI_OK)
					{
						ERROR_RETURN(retval);
					}
				}
				else 
					PAPI = PAPI_OFF;
			}
		}
		else
			PAPI = PAPI_OFF;
	}
	
	Trace_start_time = gettime ();
}

void Trace_end ()
{
#ifdef DEBUG
	printf_d ("<---------------------------------------trace end------------------------------------------->\n");
#endif
	char *trace_main;
	char *cupti;

	trace_main = getenv ("TRACEMAIN");
	cupti = getenv ("CUPTI");

	if (cupti != NULL && strcmp (cupti, "ON") == 0)
		finiTrace();

	if (trace_main != NULL && strcmp (trace_main, "ON") == 0)
	{
		Record_Event Event = Event_init ();
	    Event.event_name = "LDMC_Application";
	    Event.eid = -1;
	    Event.type = NONE;
	    Event.starttime = gettime ();
		printf_d ("PID:%u\tmain() total time:%llu\n", getpid(),  Event.starttime - Trace_start_time);	
	    Record (&Event, MPI_TRACE);
	}
}

int GetProcName(char *name)
{
	char filepath[1024];
	char line[20];
	int len = 20;
	name[0]='\0';

	pid_t pid = getpid();
	snprintf_d(filepath, sizeof(filepath), "/proc/%d/status", pid);
	FILE * fp = fopen_d(filepath, "r");
	if (fp == NULL)
		return -1;

	while (fgets_d(line, sizeof(line), fp) != NULL)
	{
		if (strncmp(line, "Name:", 5) == 0)
		{
			char * p = strchr(line, '\t');
			p++;
			snprintf_d(name, len, "%s", p);
			while (name[strlen(name) - 1]  == '\n') 
				name[strlen(name) - 1] = '\0';

			fclose_d(fp);
			return 0;
		}
	}

	fclose_d(fp);
	return -1;
}

