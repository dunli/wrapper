#if defined(_WIN32) || defined(_WIN64)  //为了支持windows平台上的编译
#include <windows.h>
#endif

#ifndef WRAPPER_H
#include "wrapper.h"
#endif
#include <stdlib.h>
#include <mysql/mysql.h>
#include <pthread.h>

#define MYSQL_ERROR() { printf_d ("[MYSQL] %s, line: %d\n",  mysql_error (conn), __LINE__); exit (-1);}

extern long long unsigned gettime ();
void DB_init ();
MYSQL * DB_get_conn ();
char * task_state (int);

char   *server   = "192.168.3.211";
char   *user     = "perfmo";
char   *password = "123456";
char   *database = "perf_behost";
char tablename[256];

int flag = 0;
int init_flag = 0;
long long unsigned Trace_start_time;

char PAPI_EVENT[PAPI_MAX_EVENT_NUM][PAPI_MAX_EVENT_LEN];
int PAPI;
int EventSet;
int NUM_EVENT;

char sql_papi[] = "\
CREATE TABLE `%s_papi` (\
	`id` int(5) NOT NULL auto_increment,\
	`name` char(20),\
	`pid` int(10),\
	`tid` int(10),\
	`hostname` char(20) ,\
	`papi_event` char(20),\
	`data` bigint(20) ,\
	`time` bigint(20) ,\
	`finish` int(5),\
	PRIMARY KEY  (`id`)\
) ENGINE=MyISAM DEFAULT CHARSET=latin1 ROW_FORMAT=DYNAMIC;\
";

char sql_omp_trace[] = "\
	CREATE TABLE `%s_trace` (\
	`id` int(11) NOT NULL auto_increment,\
	`pid` int(10) ,\
	`hostname` char(20) ,\
	`omp_rank` int(10) NOT NULL,\
	`omp_level` int(5) ,\
	`omp_p_rank` int(10),\
	`tid` int(11) NOT NULL,\
	`eid` int(11) ,\
	`name` char(50),\
	`type` int(11) ,\
	`task_id` char(16),\
	`p_task_id` char(16),\
	`task_state` char(20),\
	`time` bigint(20) default NULL,\
	`finish` int (11) default NULL,\
	PRIMARY KEY (`id`)\
	)ENGINE=MyISAM DEFAULT CHARSET=latin1 ROW_FORMAT=DYNAMIC;\
";

char sql_io[] = "\
CREATE TABLE `%s_io` (\
	`id` int(5) NOT NULL auto_increment,\
	`name` char(20),\
	`pid` int(11) ,\
	`tid` int(11),\
	`hostname` char(20),\
	`data` char(20),\
	`start_time` bigint(20) default NULL,\
	`end_time` bigint(20) default NULL,\
	PRIMARY KEY (`id`)\
	)ENGINE=MyISAM DEFAULT CHARSET=latin1 ROW_FORMAT=DYNAMIC;\
";

__attribute__((constructor)) void Trace_start()
{	
	char *trace_main, *papi, *papi_thread;
	
	trace_main = getenv ("TRACEMAIN");
	papi = getenv ("PAPI");
	papi_thread = getenv ("PAPI_THREAD");

	//init mysql connection
	DB_init ();

	//record the start time 
	if (trace_main != NULL)
	{
		if (strcmp (trace_main, "ON") == 0)
		{
			Trace_start_time = gettime ();
		}
	}

	//papi init 
	if (papi != NULL)
	{
		if (strcmp (papi, "ON") == 0)
		{
			int i, retval; 
   			char errstring[PAPI_MAX_STR_LEN];
		
			int event_codes_temp[PAPI_MAX_EVENT_NUM];
			int *event_codes;
			FILE *papi_config;
			papi_config = fopen_d ("PAPI.config", "r");
			if (papi_config == NULL)
			{
				printf_d ("[PAPI]Can't open PAPI.config, now exitting...\n");
				exit (1);
			}
			else 
			{
				if((retval = PAPI_library_init(PAPI_VER_CURRENT)) != PAPI_VER_CURRENT )
       			{
		    		printf_d ("[PAPI]Error: %s\n", errstring);
		 	        exit (1);
		        }
				fflush(stdout);

				if (papi_thread  != NULL)
				{
					if (strcmp (papi_thread, "ON") == 0)
					{
						if ((retval = PAPI_thread_init(pthread_self)) != PAPI_OK)
						{
							printf_d ("[PAPI]Thread init error!\n");
							exit (1);
						}
						else 
							printf_d ("[PAPI]PAPI thread init succeed!\n");
					}
				}
				for (i = 0; fscanf (papi_config, "%s", PAPI_EVENT[i]) != EOF; i++)
				{
					retval = PAPI_event_name_to_code (PAPI_EVENT[i], &event_codes_temp[i]);
				
					if( retval != PAPI_OK )
						printf_d("[PAPI]PAPI_event_name_to_code failed, retval = %d\n", retval );
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

					if ((retval=PAPI_create_eventset(&EventSet)) != PAPI_OK)
						ERROR_RETURN(retval);

					if ((retval=PAPI_add_events(EventSet, event_codes, NUM_EVENT)) != PAPI_OK)
						ERROR_RETURN(retval);
				}
				else 
				{
					PAPI = PAPI_OFF;
				}
			
			}
		}
		else
			PAPI = PAPI_OFF;
	}
	init_flag = 1;
	printf_d ("[PAPM] OpenMP trace start\n");	
}


void Trace_end ()
{
	char *trace_main;
	char *cupti;
	char sql [2048];
	char hostname [20];
	MYSQL *conn;
	
	conn = DB_get_conn ();

	trace_main = getenv ("TRACEMAIN");

	if (trace_main != NULL)
	{
		if (strcmp (trace_main, "ON") == 0)
		{
			gethostname (hostname, 20);

			snprintf_d (sql, 2048,"CALL insertdata ( %u, \"%s\", %d, %d, %d, %d, %d, \"%s\", %d, \"%016llx\", \"%016llx\", \"%s\", %llu, %d);",
						getpid (),
						hostname,
						-1,
						-1,
						-1,
						gettid (),
						0,
						"LDMC_Application",
						NONE,
						-1LL,
						-1LL,
						task_state (-1),
						gettime (),
						2);

			if (mysql_query (conn, sql))
				MYSQL_ERROR();
		}
	}

	snprintf_d( sql,2048 , "update `maintable` set stop=NOW(), finished=1 where name=\"%s\";", tablename);

	if (mysql_query (conn, sql))
		MYSQL_ERROR ();

	mysql_close (conn);
	printf_d ("[PAPM] OpenMP trace end\n");
}

char * task_state (int i)
{
	switch (i)
	{
		case TASK_CREATE:
			return "TASK_CREATE";
		case TASK_SUSPEND:
			return "TASK_SUSPEND";
		case TASK_RESUME:
			return "TASK_RESUME";
		case TASK_RUNUING:
			return "TASK_RUNUING";
		case TASK_WAIT:
			return "TASK_WAIT";
		case TASK_END:
			return "TASK_END";
		default:
			return "NO_TASK";
	}
}

void Record (void *Event, int wrapper_type)
{
	char sql [2048];
	char hostname [20];
	char *trace_main;

	if (init_flag == 0)
		return ;
	if (flag == 0)
	{
		flag = 1;
		trace_main = getenv ("TRACEMAIN");

		if (trace_main != NULL)
		{
			atexit (Trace_end);		//register Trace_end()
			if (strcmp (trace_main ,"ON") == 0)
			{
				MYSQL *conn;
				conn = DB_get_conn ();

				gethostname (hostname, 20);

				snprintf_d (sql, 2048,"CALL insertdata ( %u, \"%s\", %d, %d, %d, %d, %d, \"%s\", %d, \"%016llx\", \"%016llx\", \"%s\", %llu, %d);",
							getpid (),
							hostname,
							-1,
							-1,
							-1,
							gettid (),
							0,
							"LDMC_Application",
							NONE,
							-1LL,
							-1LL,
							task_state (-1),
							Trace_start_time,
							1);

				if (mysql_query (conn, sql))
					MYSQL_ERROR ();

				mysql_close (conn);
			}
		}

	}
	if (flag == 1 && wrapper_type != IO_TRACE)
	{
		Record_Event * common_event; 
		common_event = (Record_Event *) Event;
		common_event->pid = getpid ();
		common_event->tid = gettid ();
		gethostname (common_event->hostname, 20);
		OMP_info_record (*common_event);
	}
	else if (flag == 1 && wrapper_type == IO_TRACE)
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


//Database init
void DB_init ()
{
	MYSQL *conn;
	MYSQL_RES *res = NULL;
	char sql [1024];
	char owner[20];
	char * papi;	

	conn = DB_get_conn ();
	
	do{
		printf_d ("input table name:");
		scanf_d ("%s", tablename);
	
		snprintf_d ( sql, 1024,  "select * from maintable where name=\"%s\"",  tablename );

		if( mysql_query (conn, sql))
			MYSQL_ERROR ();

		if(res)
			mysql_free_result( res );

		res = mysql_store_result( conn );
		
		if ( mysql_num_rows( res ) != 0 )
			printf_d( "table exist!\n" );
		else
		{
			printf_d("input the owner of this experiment:");
			scanf("%s", owner);
		}

	}while( mysql_num_rows( res ) );

	mysql_free_result (res);

	papi = getenv ("PAPI");

	//insert exp info into maintable
	snprintf_d(sql, 1024, "insert into maintable ( name, type, owner, start ) values ( \"%s\", \"omp\", \"%s\", NOW() );", tablename, owner);
	
	if (mysql_query (conn, sql))
		MYSQL_ERROR ();

	//create trace table
	snprintf_d(sql, 1024, sql_omp_trace, tablename);
	if (mysql_query (conn, sql))
		MYSQL_ERROR ();
	
	//create io table
	snprintf_d(sql, 1024, sql_io, tablename);
	if (mysql_query (conn, sql))
		MYSQL_ERROR ();

	//create papi table
	if (papi != NULL)
	{
		if (strcmp (papi, "ON") == 0)
		{
			snprintf_d (sql, 1024, sql_papi, tablename);
			if (mysql_query (conn, sql))
				MYSQL_ERROR ();
		}
	}

	//create insertdata procedure
	snprintf_d(sql, 1024, "DROP PROCEDURE IF EXISTS insertdata;");
	if (mysql_query (conn, sql))
		MYSQL_ERROR ();

	snprintf_d (sql, 1024, "CREATE PROCEDURE insertdata (pid int, hostname char(20), omp_rank int, omp_level int, omp_p_rank int, tid int, eid int,name char(50),type int,task_id char(16), p_task_id char(16), task_state char(20),time bigint(20),finish int)\nBEGIN\n"
			"insert into `%s_trace` (pid, hostname, omp_rank, omp_level, omp_p_rank, tid, eid, name, type,task_id, p_task_id, task_state, time, finish) values (pid, hostname, omp_rank, omp_level, omp_p_rank, tid, eid, name, type, task_id, p_task_id, task_state, time, finish);\n"
			"END;"
			,tablename);
	if (mysql_query (conn, sql))
		MYSQL_ERROR ();

	//create insert_papi_data procedure
	snprintf_d (sql, 1024, "DROP PROCEDURE IF EXISTS insert_papi_data;");
	if (mysql_query (conn, sql))
		MYSQL_ERROR ();

	snprintf_d (sql, 1024, "CREATE PROCEDURE insert_papi_data (ename char(20), pid int, tid int, hostname char(20), papi_event char(20), data bigint(20), time bigint(20), finish int)\nBEGIN\n"
			"insert into `%s_papi` (name, pid, tid, hostname, papi_event, data, time, finish) values (ename, pid, tid, hostname, papi_event, data, time, finish);\n"
			"END;",
			tablename );
	if (mysql_query (conn, sql))
		MYSQL_ERROR ();

	//create insert_io_data procedure
	snprintf_d (sql, 1024, "DROP PROCEDURE IF EXISTS insert_io_data");
	if (mysql_query (conn, sql))
		MYSQL_ERROR ();

	snprintf_d (sql, 1024, "CREATE PROCEDURE insert_io_data (ename char(20), pid int, tid int, hostname char(20), data char(20), start_time bigint(20), end_time bigint(20))\nBEGIN\n"
			"insert into `%s_io` (name, pid, tid, hostname, data, start_time, end_time) values (ename, pid, tid, hostname, data, start_time, end_time);\n"
			"END;",
			tablename );
	if (mysql_query (conn, sql))
		MYSQL_ERROR ();
	
	mysql_close (conn);
	return ;
}


//get DB connection
MYSQL * DB_get_conn ()
{
	MYSQL *conn;
	conn = mysql_init (NULL);

	if (conn == NULL)
	{
		fprintf_d (stderr, "[MYSQL] MYSQL init failed!\n");
		exit (-1);
	}
	
	if (!mysql_real_connect(conn, server, user, password, database, 0, NULL, 0))
		MYSQL_ERROR ();
		
	return conn;
}

void IO_info_record (IO_Event Event)
{

	MYSQL *conn;
	char sql [2048];

	conn = DB_get_conn ();

	snprintf_d (sql, 2048, "CALL insert_io_data(\"%s\", %u, %d, \"%s\", \"%s\", %llu, %llu);",
							Event.event_name,
							Event.pid,
							Event.tid,
							Event.hostname,
							Event.data,
							Event.start_time,
							Event.end_time);
	if (mysql_query (conn, sql))
		MYSQL_ERROR ();

	mysql_close (conn);
}

void OMP_info_record (Record_Event Event)
{
	MYSQL *conn;
	char sql[2048];

	conn = DB_get_conn ();
	
	snprintf_d (sql, 2048,"CALL insertdata ( %u, \"%s\", %d, %d, %d, %d, %d, \"%s\", %d, \"%016llx\", \"%016llx\", \"%s\", %llu, %d);",
							Event.pid,
							Event.hostname,
							Event.omp_rank,
							Event.omp_level,
							Event.p_rank,
							Event.tid,
							Event.eid,
							Event.event_name,
							Event.type,
							Event.task_id_start,
							Event.p_task_id_start,
							task_state (Event.task_state_start),
							Event.starttime,
							1);

	if (mysql_query (conn, sql))
		MYSQL_ERROR ();

	snprintf_d (sql, 2048,"CALL insertdata ( %u, \"%s\", %d, %d, %d, %d, %d, \"%s\", %d, \"%016llx\", \"%016llx\", \"%s\", %llu, %d);",
							Event.pid,
							Event.hostname,
							Event.omp_rank,
							Event.omp_level,
							Event.p_rank,
							Event.tid,
							Event.eid,
							Event.event_name,
							Event.type,
							Event.task_id_end,
							Event.p_task_id_end,
							task_state (Event.task_state_end),
							Event.endtime,
							2);

	if (mysql_query (conn, sql))
		MYSQL_ERROR ();

	mysql_close (conn);
}


void PAPI_info_record (PAPI_Event Event)
{
	MYSQL *conn;
	char sql[2048];

	if (init_flag == 0)
		return ;
	
	conn = DB_get_conn ();

	Event.pid = getpid ();
	Event.tid = gettid ();
	gethostname (Event.hostname, 20);

	snprintf_d (sql, 2048, "CALL insert_papi_data(\"%s\", %u, %d, \"%s\", \"%s\", %lld, %llu, %d);", 
							Event.event_name,
							Event.pid,
							Event.tid,
							Event.hostname,
							Event.papi_event,
							Event.data,
							Event.time,
							Event.finish);

	if (mysql_query (conn, sql))
		MYSQL_ERROR ();

	mysql_close (conn);
}

