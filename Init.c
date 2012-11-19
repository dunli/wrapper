#include <sys/time.h>
#include <stdio.h>
#include <stdlib.h>
#include "wrapper.h"

extern int EventSet;
extern int NUM_EVENT;
extern char PAPI_EVENT[PAPI_MAX_EVENT_NUM][PAPI_MAX_EVENT_LEN];
extern int PAPI_info_record (PAPI_Event);

long long unsigned gettime ();
int omp_flag = 0;

/*初始化 Event*/
Record_Event Event_init ()
{
	Record_Event Event;
	Event.event_name = "NULL";
	Event.comm = -1;

	Event.mpi_rank = -1;
	Event.omp_rank = -1;
	Event.omp_level = -1;
	Event.p_rank = -1;

	Event.eid = -1;
	Event.tid = -1;
	Event.pid = -1;
	Event.type = -1;

	Event.starttime = 0;
	Event.endtime = 0;
	
	Event.tag = -1;

	Event.recvtype = -1;
	Event.sendtype = -1;

	Event.sendsize = -1;
	Event.recvsize = -1;

	Event.src_rank = -1;
	Event.dst_rank = -1;

	Event.task_id_start = -1;
	Event.task_id_end = -1;
	Event.p_task_id_start = -1;
	Event.p_task_id_end = -1;
	Event.task_state_start = -1;
	Event.task_state_end = -1;


	Event.cuda_stream = -1;
	Event.memcpy_size = -1;
	Event.memcpy_type = -1;
	Event.memcpy_kind = -1;

	return Event;
}


int PAPI_get_info (char *event_name, int finish, int type)
{
	int retval,i;
	PAPI_Event Event_papi;
	Event_papi.event_name = event_name;
	Event_papi.finish = finish;
	Event_papi.time = gettime();

	long long values [PAPI_MAX_EVENT_NUM];

	if (type == PAPI_PROCESS)
	{
		if ( (retval=PAPI_start(EventSet)) != PAPI_OK)
		ERROR_RETURN(retval);

		if ((retval=PAPI_stop(EventSet,values)) != PAPI_OK)
			ERROR_RETURN(retval);

//		printf_d ("%s\n", Event_papi.event_name);
		for (i = 0; i < NUM_EVENT; i++)
		{
			Event_papi.papi_event = PAPI_EVENT[i];
			Event_papi.data = values[i];
			if ((PAPI_info_record (Event_papi)) == -1)
				printf_d ("[PAPI]GET stream error!!\n");
		}
		return 0;
	}
	else if (type == PAPI_THREAD)
	{
		int EventSet1 = PAPI_NULL;

		int *event_codes;
		event_codes = (int *)calloc (sizeof (int *), NUM_EVENT);
		for (i = 0; i < NUM_EVENT; ++i)
		{
			retval = PAPI_event_name_to_code (PAPI_EVENT[i], &event_codes[i]);
			if( retval != PAPI_OK )
				printf_d("[PAPI]PAPI_event_name_to_code failed, retval = %d\n", retval );
		}
		if ((retval = PAPI_register_thread ()) != PAPI_OK)
			ERROR_RETURN (retval);

		if ((retval=PAPI_create_eventset(&EventSet1)) != PAPI_OK)
			ERROR_RETURN(retval);
		if ((retval=PAPI_add_events(EventSet1, event_codes, NUM_EVENT)) != PAPI_OK)
			ERROR_RETURN(retval);

		if ((retval=PAPI_start(EventSet1)) != PAPI_OK)
			ERROR_RETURN(retval);

		if ((retval=PAPI_stop(EventSet1,values)) != PAPI_OK)
			ERROR_RETURN(retval);

		for (i = 0; i < NUM_EVENT; i++)
		{
			Event_papi.papi_event = PAPI_EVENT[i];
			Event_papi.data = values[i];
			PAPI_info_record (Event_papi);
		}
		if ((PAPI_unregister_thread()) != PAPI_OK)
			ERROR_RETURN(retval);
		return 0;
	}
	return 1;
}


/*获取时间，精确到微秒*/
long long unsigned gettime ()
{
	struct timeval tv;
	long long unsigned time_usec, temp;
	
	gettimeofday(&tv,NULL);
	temp = tv.tv_sec;
	time_usec = temp * 1000000 + tv.tv_usec;
	return time_usec;
}
