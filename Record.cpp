#include <stdlib.h>
#include "mrnbe.h"
#include "protocol.h"
#include <IO.h>

#ifndef WRAPPER_H
#include "wrapper.h"
#endif

extern int Send(Record_Event);
extern int send_Node(MPI_node_info);
extern Record_Event Event_init ();
extern long long unsigned gettime ();
extern "C" void Trace_end (void);
extern "C" int IO_info_record (IO_Event);

long long unsigned Trace_start_time;
int PAPI;
int EventSet; 
int NUM_EVENT;
char PAPI_EVENT[PAPI_MAX_EVENT_NUM][PAPI_MAX_EVENT_LEN];

extern "C" void Record (void *Event, int wrapper_type)
{
	static int be_status = 0;
	char *trace_main;

	//if the io event is sent before the mrnet startup, ignore it
	if (be_status == 0 && wrapper_type == IO_TRACE)
		return ;
	else if ( be_status == 0 && wrapper_type != IO_TRACE)
	{

		BE::Startup (gettid (), "MPI_wrapper", 0);

		atexit (Trace_end);		//register Trace_end()
		
		trace_main = getenv ("TRACEMAIN");

		//if trace_main is on, create and send trace_start event
		if (trace_main != NULL && strcmp (trace_main ,"ON") == 0)
		{
			Record_Event Trace_start_event;
			Trace_start_event.event_name = "LDMC_Application";
			Trace_start_event.pid = getpid ();
			Trace_start_event.tid = gettid ();
			Trace_start_event.mpi_rank = -1;
			Trace_start_event.omp_rank = -1;	
			Trace_start_event.p_rank = -1;
			Trace_start_event.comm = -1;
			Trace_start_event.eid = 0;
			Trace_start_event.type = NONE;
			Trace_start_event.starttime = Trace_start_time;
			gethostname (Trace_start_event.hostname, 20);

			Send (Trace_start_event);
		}
		if (wrapper_type != IO_TRACE)
		{
			Record_Event *common_event; 
			common_event = (Record_Event *) Event;
			common_event->pid = getpid ();
			common_event->tid = gettid ();
			gethostname (common_event->hostname, 20);
			
			Send (*common_event);
		}
		else if (wrapper_type == IO_TRACE)
		{
			IO_Event * io_event;
			io_event = (IO_Event *) Event;
			io_event->pid = getpid ();
			io_event->tid = gettid ();
			gethostname (io_event->hostname, 20);
			IO_info_record (*io_event);
		}
		be_status = 3;
	}
	else if (be_status == 3) 
	{
		if (wrapper_type != IO_TRACE)
		{
			Record_Event *common_event; 
			common_event = (Record_Event *) Event;
			common_event->pid = getpid ();
			common_event->tid = gettid ();
			gethostname (common_event->hostname, 20);
			Send (*common_event);
		}
		else if (wrapper_type == IO_TRACE)
		{
			IO_Event * io_event;
			io_event = (IO_Event *) Event;
			io_event->pid = getpid ();
			io_event->tid = gettid ();
			gethostname (io_event->hostname, 20);
			sprintf_d (io_event->data, "%s", "test");
			IO_info_record (*io_event);
		}
	}
}

extern "C" void MPI_node_info_record (MPI_node_info Node)
{
#ifdef DEBUG
	printf_d("[DEBUG]:\tMPI_node_info_record()\tpid:%u\tmpi_rank:%d\n", Node.pid, Node.mpi_rank);
#endif
	
	send_Node (Node);
}


extern "C" int PAPI_info_record (PAPI_Event Event)
{
	Stream * stream = BE::GetStream();
	Event.pid = getpid ();
	Event.tid = gettid ();
	gethostname (Event.hostname, 20);
	if (!stream)
		return -1;

	if (stream->send (PROT_PAPI, "%s %ud %d %s %s %ld %uld %d",
			Event.event_name,
			Event.pid,
			Event.tid,
			Event.hostname,
			Event.papi_event,
			Event.data,
			Event.time,
			Event.finish) != -1);
		return 1;
}

extern "C" int IO_info_record (IO_Event Event)
{
	Stream * stream = BE::GetStream();

	if (!stream)
		return -1;

	if (stream->send (PROT_IO, "%s %ud %d %s %s %uld %uld",
			Event.event_name,
			Event.pid,
			Event.tid,
			Event.hostname,
			Event.data,
			Event.start_time,
			Event.end_time) != -1);
		return 1;
}
