#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <IO.h>
#include "mrnfe.h"
#include "protocol.h"
#include <mysql/mysql.h>
#include <mrnet/MRNet.h>
#include <unistd.h>

#include <cstdio>
#include <cstring>
#include <vector>
#include <string>

#include <sys/socket.h>
#include <sys/ioctl.h>
#include <sys/time.h>
#include <net/if.h>
#include <netinet/in.h>
#include <arpa/inet.h>
using namespace FE;
using namespace MRN;

int use_proc = 0;
int use_papi = 0;


char sql_nodelist[] = "\
CREATE TABLE `%s_nodelist` (\
  hostname text NOT NULL,\
  pid int(11) NOT NULL default '0',\
  parhost text,\
  parport int(11) default NULL,\
  parrank int(11) default NULL,\
  rank int(11) default NULL,\
  mpirank int(11) default NULL,\
  ip char(20) default NULL,\
  duty int(11) default NULL,\
  PRIMARY KEY  (hostname(256),pid)\
) ENGINE=InnoDB DEFAULT CHARSET=latin1 ROW_FORMAT=DYNAMIC;\
";

char sql_proc[] = "\
CREATE TABLE `%s_proc` (\
  `id` int(11) NOT NULL auto_increment,\
  `time_s` int(11) default NULL,\
  `time_us` int(11) default NULL,\
  `hostname` char(20) default NULL,\
  `pid` int (8) default NULL,\
  `metric` char(10) default NULL,\
  `instance` char(20) default NULL,\
  `data` float(10) default NULL,\
	PRIMARY KEY  (`id`)\
) ENGINE=InnoDB DEFAULT CHARSET=latin1 ROW_FORMAT=DYNAMIC;\
";

char sql_papi[] = "\
CREATE TABLE `%s_papi` (\
  `id` int(5) NOT NULL auto_increment,\
  `name` text,\
  `pid` int(10),\
  `tid` int(10),\
  `hostname` char(20) ,\
  `papi_event` char(20),\
  `data` bigint(20) ,\
  `time` bigint(20) ,\
  `finish` int(5),\
  PRIMARY KEY  (`id`)\
) ENGINE=InnoDB DEFAULT CHARSET=latin1 ROW_FORMAT=DYNAMIC;\
";

char sql_mpi_trace[] = "\
CREATE TABLE `%s_trace` (\
	`id` int(11) NOT NULL auto_increment,\
	`rank` int(11) ,\
	`pid` int(11) ,\
	`hostname` text ,\
	`eid` int(11) ,\
	`name` text,\
	`type` int(11) ,\
	`comm` text ,\
	`time` bigint(20) default NULL,\
	`finish` int (11) default NULL,\
	PRIMARY KEY (`id`)\
	)ENGINE=InnoDB DEFAULT CHARSET=latin1 ROW_FORMAT=DYNAMIC;\
";

char sql_mpi_comm_trace[] = "\
CREATE TABLE `%s_comm` (\
	`id` int(11) NOT NULL auto_increment,\
	`rank` int(11),\
	`pid` int(11),\
	`hostname` text,\
	`eid` int(11),\
	`name` text,\
	`tag` int(5),\
	`src_rank` int(11),\
	`dst_rank` int(11),\
	`send_size` int(11),\
	`send_type` int(11),\
	`recv_size` int(11),\
	`recv_type` int(11),\
	`time` bigint(20) default NULL,\
	`finish` int (11) default NULL,\
	PRIMARY KEY (`id`)\
	)ENGINE=InnoDB DEFAULT CHARSET=latin1 ROW_FORMAT=DYNAMIC;\
";

char sql_io[] = "\
CREATE TABLE `%s_io` (\
	`id` int(5) NOT NULL auto_increment,\
	`name` text,\
	`pid` int(11) ,\
	`tid` int(11),\
	`hostname` text,\
	`data` text,\
	`start_time` bigint(20) default NULL,\
	`end_time` bigint(20) default NULL,\
	PRIMARY KEY (`id`)\
	)ENGINE=InnoDB DEFAULT CHARSET=latin1 ROW_FORMAT=DYNAMIC;\
";

void create_database_procedure (MYSQL* conn, char * tablename)
{
	char sql[1024];
	snprintf_d (sql, 1024, "DROP PROCEDURE IF EXISTS insertdata;");
	if (mysql_query (conn, sql))
	{
		printf_d ("[MRNFE] %s\n", mysql_error (conn));
		exit (-1);
	}
	
	snprintf_d (sql, 1024, "DROP PROCEDURE IF EXISTS insertproc;");
	if (mysql_query (conn, sql))
	{
		printf_d ("[MRNFE] %s\n", mysql_error (conn));
		exit (-1);
	}

	snprintf_d (sql, 1024, "DROP PROCEDURE IF EXISTS insert_papi_data;");
	if (mysql_query (conn, sql))
	{
		printf_d ("[MRNFE] %s\n", mysql_error (conn));
		exit (-1);
	}

	snprintf_d (sql, 1024, "DROP PROCEDURE IF EXISTS insert_comm_data");
	if (mysql_query (conn, sql))
	{
		printf_d ("[MRNFE] %s\n", mysql_error (conn));
		exit (-1);
	}

	snprintf_d (sql, 1024, "DROP PROCEDURE IF EXISTS insert_io_data");
	if (mysql_query (conn, sql))
	{
		printf_d ("[MRNFE] %s\n", mysql_error (conn));
		exit (-1);
	}

	snprintf_d (sql, 1024, "CREATE PROCEDURE insertdata (mpi_rank int,pid int,hostname text,eid int,ename text,type int,mpi_comm text,time bigint(20),finish int)\nBEGIN\n"
			"insert into `%s_trace` (rank, pid, hostname, eid, name, type, comm, time, finish) values (mpi_rank, pid, hostname, eid, ename, type, mpi_comm, time, finish);\n"
			"END;"
			, tablename);

	if (mysql_query (conn, sql))
	{
		printf_d ("[MRNFE] %s\n", mysql_error (conn));
		exit (-1);
	}

	snprintf_d (sql, 1024, "CREATE PROCEDURE insert_comm_data (rank int, pid int, hostname text, eid int, name text, tag int, src_rank int, dst_rank int, send_size int, send_type int, recv_size int, recv_type int, time bigint(20), finish int)\nBEGIN\n"
			"insert into `%s_comm` (rank, pid, hostname, eid, name, tag, src_rank, dst_rank, send_size, send_type, recv_size, recv_type, time, finish) values (rank, pid, hostname, eid, name, tag, src_rank, dst_rank, send_size, send_type, recv_size, recv_type, time, finish);\n"
			"END;"
			, tablename);

	if (mysql_query (conn, sql))
	{
		printf_d ("[MRNFE] %s\n", mysql_error (conn));
		exit (-1);
	}

	snprintf_d (sql, 1024, "CREATE PROCEDURE insert_papi_data (ename text, pid int, tid int, hostname text, papi_event text, data bigint(20), time bigint(20), finish int)\nBEGIN\n"
			"insert into `%s_papi` (name, pid, tid, hostname, papi_event, data, time, finish) values (ename, pid, tid, hostname, papi_event, data, time, finish);\n"
			"END;",
			tablename );
	if (mysql_query (conn, sql))
	{
		printf_d("[MRNFE] %s\n", mysql_error(conn));
		exit (-1);
	}

	snprintf_d (sql, 1024, "CREATE PROCEDURE insert_io_data (ename text, pid int, tid int, hostname text, data text, start_time bigint(20), end_time bigint(20))\nBEGIN\n"
			"insert into `%s_io` (name, pid, tid, hostname, data, start_time, end_time) values (ename, pid, tid, hostname, data, start_time, end_time);\n"
			"END;",
			tablename );
	if (mysql_query (conn, sql))
	{
		printf_d("[MRNFE] %s\n", mysql_error(conn));
		exit (-1);
	}
}

void create_tables (MYSQL* conn, char * tablename, char * exp_type, char * owner)
{
	char sql [1024];
	snprintf_d(sql, 1024, "insert into maintable ( name, type, owner, start ) values ( \"%s\", \"%s\", \"%s\", NOW() );", tablename, exp_type, owner);

	if (mysql_query (conn, sql))
	{
		printf_d ("[MRNFE] %s\n", mysql_error (conn));
		exit (-1);
	}

	snprintf_d(sql, 1024, sql_nodelist, tablename);
	if (mysql_query (conn, sql))
	{
		printf_d ("[MRNFE] %s\n", mysql_error (conn));
		exit (-1);
	}

	snprintf_d(sql, 1024, sql_mpi_trace, tablename);
	if (mysql_query (conn, sql))
	{
		printf_d ("[MRNFE] %s\n", mysql_error (conn));
		exit (-1);
	}

	snprintf_d(sql, 1024, sql_mpi_comm_trace, tablename);
	if (mysql_query (conn, sql))
	{
		printf_d ("[MRNFE] %s\n", mysql_error (conn));
		exit (-1);
	}

	snprintf_d(sql, 1024, sql_io, tablename);
	if (mysql_query (conn, sql))
	{
		printf_d ("[MRNFE] %s\n", mysql_error (conn));
		exit (-1);
	}

	if (use_proc)
	{
		snprintf_d (sql, 1024, sql_proc, tablename);

		if (mysql_query (conn, sql))
		{
			printf_d ("[MRNFE] %s\n", mysql_error (conn));
			exit (-1);
		}
	}

	if (use_papi)
	{
		snprintf_d (sql, 1024, sql_papi, tablename);
		if (mysql_query (conn, sql))
		{
			printf_d("[MRNFE] %s\n", mysql_error(conn));
			exit (-1);
		}
	}

}


void data_recving (MYSQL* conn, char * tablename, int nr_be)
{
	int nr_exited = 0;
	int tag;
	int count = 0;
//	PacketPtr pack;
	int rank, eid, tid, data, finish, nr_record, mpi_rank, type, src_rank, dst_rank, sendsize, sendtype, recvsize, recvtype;
	int mpi_comm, mpi_tag;
	MRN::Network * net = GetNetwork();
	unsigned pid;
	long long unsigned time; 
	unsigned long time_s, time_us;

	char sql[2048];
	char buffer[500];
	char instance[40], metric[40];

	char *ename = NULL;
	char *hostname = NULL;
	char *procdata = NULL;


	Communicator *comm = net->get_BroadcastCommunicator();
	Stream *stream = net->new_Stream( comm, TFILTER_NULL, SFILTER_DONTWAIT );

	stream->send( PROT_DATA, "%d", 0 );

	printf_d("[MRNFE] recieving data...nr_exited is %d nr_be is %d\n",nr_exited,nr_be);

	if (mysql_autocommit (conn, 0) != 0)
		printf_d ("[MRNFE] %s\n", mysql_error(conn));

	while( nr_exited != nr_be )
	{
		PacketPtr pack;
		stream->recv( &tag, pack );
		count ++;
		switch (tag)
		{
			case PROT_DATA:
				pack->unpack("%d %d %d %uld %s %d %s",
						&tid,
						&pid,
						&eid,
						&time, 
						&hostname, 
						&finish,
						&ename);
				
				char *sql_tmp;
				sql_tmp = (char *) calloc (sizeof (char *), 200);
				snprintf_d(sql_tmp, 200, "CALL insertdata ( -1,%d, \"%s\" , %d, \"%s\", 7,\"FFFFFFFF\", '%llu', %d );"
						, pid, hostname, eid, ename, time, finish);
				if (mysql_query (conn, sql_tmp))
					printf_d ("[MRNFE] %s\n", mysql_error(conn));
				free (sql_tmp);
				free (hostname);
				free (ename);
				hostname = NULL;
				ename = NULL;
				
				break;

			case PROT_MPIDATA:											/*MPI wrapper trace 数据*/
				pack->unpack("%d %ud %s %d %s %d %d %d %d %d %d %d %d %d %uld %d",
						&mpi_rank,
						&pid,
						&hostname,
						&eid,
						&ename,
						&type,
						&mpi_comm,
						&mpi_tag,
						&src_rank,
						&dst_rank,
						&sendsize,
						&sendtype,
						&recvsize,
						&recvtype,
						&time,
						&finish);
				
			#ifdef debug
				printf_d ("[DEBUG]:\tRecv_Data: %d %u %s %d %s %d %x %d %d %d %d %d %d %d %llu %d\n", 
						mpi_rank,
						pid,
						hostname,
						eid,
						ename,
						type,
						mpi_comm,
						mpi_tag,
						src_rank,
						dst_rank,
						sendsize,
						sendtype,
						recvsize,
						recvtype,
						time,
						finish);
			#endif
				snprintf_d (sql, 2048,"CALL insertdata (%d, %u, \"%s\", %d, \"%s\", %d, \"%x\", %llu, %d);",
						mpi_rank,
						pid,
						hostname,
						eid,
						ename,
						type,
						mpi_comm,
						time,
						finish);

			#ifdef DEBUG
				printf_d ("[DEBUG]:\t%s\n", sql);
			#endif

				if (mysql_query (conn, sql))
				{
					printf_d ("[MRNFE] %s\n", mysql_error(conn));
				}

				if (type != 0)
				{
					snprintf_d (sql, 2048,"CALL insert_comm_data (%d, %u, \"%s\", %d, \"%s\", %d, %d, %d, %d, %d, %d, %d, %llu, %d);",
						mpi_rank,
						pid,
						hostname,
						eid,
						ename,
						mpi_tag,
						src_rank,
						dst_rank,
						sendsize,
						sendtype,
						recvsize,
						recvtype,
						time,
						finish);
					if (mysql_query (conn, sql))
					{
						printf_d ("[MRNFE] %s\n", mysql_error(conn));
					}

				}
				free (ename);
				free (hostname);
				ename = NULL;
				hostname = NULL;
				break;

			case PROT_PROC:												/*proc数据*/
				if(!use_proc)
				{
					printf_d ("[MRNFE] no proc!!! ignored\n");
					break;
				}
				{
					pack->unpack("%ud %ud %s %d %s %ud",
						&time_s, &time_us, &hostname, &nr_record,
						&procdata, &pid
						);

					//printf_d(" [DEBUG] %ld, %ld, %d %s\n", time_s, time_us, nr_record, procdata );
					char *p = procdata;
					float fdata;
					for( int i = 0; i < nr_record; i++ ){

						sscanf_d( p, "%s", buffer );
						p+=strlen(buffer)+1;
						sscanf_d( buffer, "%[^#]#%[^#]#%f", instance, metric, &fdata );

						snprintf_d(sql, 2048, "CALL insertproc( %ld, %ld, \"%s\", %d, \"%s\", \"%s\", '%f')"
							, time_s, time_us, hostname, pid, metric, instance, fdata);

						if (mysql_query (conn, sql))
                	   			{
			                        	printf_d ("[MRNFE] %s\n", mysql_error(conn));
        	        			}
						//printf_d ("%s\n", sql);
						//mysql_query( conn, sql );
					}

					free(procdata);
					free(hostname);
					procdata = hostname = NULL;
				}
				break;
			case PROT_PAPI:
				if (!use_papi)
				{
					printf_d ("[MRNFE] no papi!!! ignored\n");
					break;
				}
				else
				{
					long long papi_data;
					char *papi_event;
					pack->unpack("%s %ud %d %s %s %ld %uld %d",
							&ename,
							&pid,
							&tid,
							&hostname,
							&papi_event,
							&papi_data,
							&time,
							&finish);

					snprintf_d (sql, 2048, "CALL insert_papi_data(\"%s\", %u, %d, \"%s\", \"%s\", %lld, %llu, %d);", 
							ename,
							pid,
							tid,
							hostname,
							papi_event,
							papi_data,
							time,
							finish);
					if (mysql_query (conn, sql))
							printf_d ("[MRNFE] %s\n", mysql_error(conn));
					free (hostname);
					free (ename);
					free (papi_event);
					hostname = NULL;
					ename = NULL;
					papi_event = NULL;
				}
				break;
			case PROT_IO:
				char *io_data;
				long long unsigned stime, etime;
				pack->unpack ("%s %ud %d %s %s %uld %uld", 
						&ename,
						&pid,
						&tid,
						&hostname,
						&io_data,
						&stime,
						&etime);
				snprintf_d (sql, 2048, "CALL insert_io_data(\"%s\", %u, %d, \"%s\", \"%s\", %llu, %llu);",
						ename,
						pid,
						tid,
						hostname,
						io_data,
						stime,
						etime);	
				if (mysql_query (conn, sql))
						printf_d ("[MRNFE] %s\n", mysql_error(conn));
				free (hostname);
				free (ename);
				free (io_data);
				hostname = NULL;
				ename = NULL;
				io_data = NULL;
				break;
			case PROT_MPIRANK:											/*MPI rank数据*/

				pack->unpack("%d %d", &rank, &data);

				snprintf_d( sql, 2048, "update `%s_nodelist` set mpirank=%d where rank=%d", tablename, data, rank );
				mysql_query( conn, sql );

				break;
			case PROT_NODEDATA:											/*MPI节点数据*/
				pack->unpack ("%ud %d", &pid, &mpi_rank);
				snprintf_d (sql, 2048, "update `%s_nodelist` set mpirank=%d where pid=%u", tablename, mpi_rank, pid);
				mysql_query( conn, sql );
				break;
			case PROT_HALT:												/*发送停止*/
				nr_exited ++ ;
				printf_d( "[DEBUG] %d Back-ends Halted\n", nr_exited );
				break;

			default:
				break;
		}
		if (count % 10000 == 0)
			mysql_commit(conn);
	}

	FE::write_exiting_info();

}


int main (int argc, char *argv[])
{
	if (argc != 4 && argc != 5 && argc != 6)
	{
		printf_d ("Usage:  mrnfe   benum fanout internum [proc] [papi]\n");
		return 0;
	}
	if (argc == 5)
	{
		if (strcmp (argv [4], "proc") == 0)
		{
			FE::setproc ();
			use_proc = 1;
		}
		else if (strcmp (argv [4], "papi") == 0)
		{
			use_papi = 1;
		}
		else
		{
			printf_d ("Usage:  mrnfe  benum fanout internum [proc] [papi]\n");
			return 0;
		}
	}
	else if (argc == 6)
	{
		if (strcmp (argv [4], "proc") == 0 && strcmp (argv [5], "papi") == 0)
		{
			FE::setproc ();
			use_proc = 1;
			use_papi = 1;
		}
		else 
		{
			printf_d ("Usage:  mrnfe  benum fanout internum [proc] [papi]\n");
			return 0;
		}
	}

	int nr_be = atoi (argv [1]);
	int fanout = atoi (argv [2]);
	int nr_inter = atoi (argv [3]);

	if (!FE::exam_config ())
	{
		printf_d("[MRNFE] config file error\n");
	}
	FE::gen_be_hostlist (nr_be);
	FE::app2comm (nr_be, nr_inter);
	FE::gen_top_file (fanout, nr_inter);

	MYSQL *conn = FE::GetConn();
	char * tablename = FE::GetTableName ();
	char * exp_type = FE::GetExpType ();
	char * owner = FE::GetOwner ();

	create_tables (conn, tablename, exp_type, owner);
	FE::network_startup (nr_be);
	FE::init_database();

	create_database_procedure (conn, tablename);

	FE::proc_dataprocess ();

	data_recving (conn, tablename, nr_be);
	
	FE::clear_database_procedure ();
	FE::network_shutdown ();
	FE::time_end ();
}
