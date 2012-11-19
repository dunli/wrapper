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
) ENGINE=MyISAM DEFAULT CHARSET=latin1 ROW_FORMAT=DYNAMIC;\
";

char sql_proc[] = "\
CREATE TABLE `%s_proc` (\
  `id` int(11) NOT NULL auto_increment,\
  `time_s` int(11) default NULL,\
  `time_us` int(11) default NULL,\
  `hostname` char(20) default NULL,\
  `pid` int(8) default NULL,\
  `metric` char(20) default NULL,\
  `instance` char(20) default NULL,\
  `data` float default NULL,\
	PRIMARY KEY  (`id`)\
) ENGINE=MyISAM DEFAULT CHARSET=latin1;\
";

/*CMPI表sql语句*/
char sql_cmpi_trace[] = "\
CREATE TABLE `%s_trace` (\
	`id` int(11) NOT NULL auto_increment,\
	`mpi_rank` int(11) ,\
	`pid` int(11) ,\
	`hostname` text,\
	`eid` int(11) ,\
	`name` text,\
	`type` int(11) ,\
	`comm` text ,\
	`time` bigint(20) default NULL,\
	`finish` int (11) default NULL,\
	PRIMARY KEY (`id`)\
	)ENGINE=MyISAM DEFAULT CHARSET=latin1 ROW_FORMAT=DYNAMIC;\
";

char sql_cmpi_comm_trace[] = "\
CREATE TABLE `%s_comm` (\
	`id` int(11) NOT NULL auto_increment,\
	`mpi_rank` int(11) ,\
	`pid` int(11) ,\
	`hostname` text,\
	`eid` int(11) ,\
	`name` text,\
	`tag` int(5),\
	`cuda_stream` int (11),\
	`memcpy_size` int(11),\
	`memcpy_type` int(11),\
	`memcpy_kind` int(11),\
	`src_rank` int(11),\
	`dst_rank` int(11),\
	`send_size` int(11),\
	`send_type` int(11),\
	`recv_size` int(11),\
	`recv_type` int(11),\
	`time` bigint(20) default NULL,\
	`finish` int (11) default NULL,\
	PRIMARY KEY (`id`)\
	)ENGINE=MyISAM DEFAULT CHARSET=latin1 ROW_FORMAT=DYNAMIC;\
";

char sql_cmpi_cupti_trace[] = "\
CREATE TABLE `%s_cupti` (\
	`id` int(11) NOT NULL auto_increment,\
	`pid` int(11) ,\
	`hostname` text,\
	`device_id` int (10),\
	`context_id` int(10),\
	`stream_id` int(10),\
	`type` int(5),\
	`name` text,\
	`seg1` text,\
	`seg2` text,\
	`seg3` text,\
	`time` bigint(20),\
	`finish` int(5) ,\
	PRIMARY KEY (`id`)\
	)ENGINE=MyISAM DEFAULT CHARSET=latin1 ROW_FORMAT=DYNAMIC;\
";

void create_database_procedure (MYSQL* conn, char * tablename)
{
	char sql[1024];
	/*删除insertdata存储过程*/
	snprintf_d(sql, 1024, "DROP PROCEDURE IF EXISTS insertdata;");
	if (mysql_query (conn, sql))
	{
		printf_d ("[MRNFE] %s\n", mysql_error (conn));
		exit (-1);
	}
	/*删除insertproc存储过程*/
	snprintf_d(sql, 1024, "DROP PROCEDURE IF EXISTS insertproc;");
	if (mysql_query (conn, sql))
	{
		printf_d ("[MRNFE] %s\n", mysql_error (conn));
		exit (-1);
	}
	/*删除insert_comm_proc存储过程*/
	snprintf_d (sql, 1024, "DROP PROCEDURE IF EXISTS insert_comm_data");
	if (mysql_query (conn, sql))
	{
		printf_d ("[MRNFE] %s\n", mysql_error (conn));
		exit (-1);
	}
	snprintf_d(sql, 1024, "DROP PROCEDURE IF EXISTS insert_cupti_data;");
	if (mysql_query (conn, sql))
	{
		printf_d ("[MRNFE] %s\n", mysql_error (conn));
		exit (-1);
	}
	/*建立insertdata存储过程*/
	snprintf_d (sql, 1024, "CREATE PROCEDURE insertdata (mpi_rank int,pid int,hostname text, eid int,name text, type int,mpi_comm text,time bigint(20),finish int)\nBEGIN\n"
			"insert into `%s_trace` (mpi_rank, pid, hostname, eid, name, type, comm, time, finish) values (mpi_rank, pid, hostname, eid, name, type, mpi_comm, time, finish);\n"
			"END;"
			,tablename);
	if (mysql_query (conn, sql))
	{
		printf_d ("[MRNFE] %s\n", mysql_error (conn));
		exit (-1);
	}

	snprintf_d (sql, 1024, "CREATE PROCEDURE insert_comm_data (mpi_rank int,pid int, hostname text, eid int,name text, tag int, cuda_stream int, memcpy_size int, memcpy_type int, memcpy_kind int, src_rank int,dst_rank int,send_size int,send_type int,recv_size int,recv_type int,time bigint(20),finish int)\nBEGIN\n"
			"insert into `%s_comm` (mpi_rank, pid, hostname, eid, name, tag, cuda_stream, memcpy_size, memcpy_type, memcpy_kind, src_rank, dst_rank, send_size, send_type, recv_size, recv_type, time, finish) values (mpi_rank, pid, hostname, eid, name, tag, cuda_stream, memcpy_size,memcpy_type, memcpy_kind, src_rank, dst_rank, send_size, send_type, recv_size, recv_type, time, finish);\n"
			"END;"
			,tablename);

	if (mysql_query (conn, sql))
	{
		printf_d ("[MRNFE] %s\n", mysql_error (conn));
		exit (-1);
	}

	snprintf_d (sql, 1024, "CREATE PROCEDURE insert_cupti_data (pid int, hostname text,device_id int ,context_id int ,stream_id int,type int,name text,seg1 text, seg2 text, seg3 text,time bigint(20),finish int(5))\nBEGIN\n"
			"insert into `%s_cupti` (pid ,hostname, device_id ,context_id ,stream_id ,type ,name ,seg1, seg2, seg3,time ,finish) values (pid ,hostname ,device_id ,context_id ,stream_id ,type ,name ,seg1, seg2, seg3, time ,finish);\n"
			"END;"
			,tablename);
	if (mysql_query (conn, sql))
	{
		printf_d ("[MRNFE] %s\n", mysql_error (conn));
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
	/*建nodelist表*/
	snprintf_d(sql, 1024, sql_nodelist, tablename);
	if (mysql_query (conn, sql))
	{
		printf_d ("[MRNFE] %s\n", mysql_error (conn));
		exit (-1);
	}
	/*建ompi_trace表*/
	snprintf_d(sql, 1024, sql_cmpi_trace, tablename);
	if (mysql_query (conn, sql))
	{
		printf_d ("[MRNFE] %s\n", mysql_error (conn));
		exit (-1);
	}
	/*建ompi_comm_trace表*/
	snprintf_d(sql, 1024, sql_cmpi_comm_trace, tablename);
	if (mysql_query (conn, sql))
	{
		printf_d ("[MRNFE] %s\n", mysql_error (conn));
		exit (-1);
	}
	/*建ompi_cupti_trace表*/
	snprintf_d(sql, 1024, sql_cmpi_cupti_trace, tablename);
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
}


void data_recving (MYSQL* conn, char * tablename, int nr_be)
{
	int nr_exited = 0;
	int tag;
	PacketPtr pack;
	int rank, eid, data, finish, nr_record, mpi_rank, type, src_rank, dst_rank, sendsize, sendtype, recvsize, recvtype;
	int mpi_comm, mpi_tag;
	int cuda_stream, memcpy_size, memcpy_type, memcpy_kind;
	MRN::Network * net = GetNetwork();
	unsigned pid;
	unsigned long long time; 
	unsigned long time_s, time_us;

	int device_id;
	int context_id;
	int stream_id;

	long long unsigned starttime;
	long long unsigned endtime;


	char sql[2048];
	char buffer[500];
	char instance[40], metric[40];

	char *ename = NULL;
	char *cupti_seg1, *cupti_seg2, *cupti_seg3;
	char *hostname = NULL;
	char *procdata = NULL;

	Communicator *comm = net->get_BroadcastCommunicator();
	Stream *stream = net->new_Stream( comm, TFILTER_NULL, SFILTER_DONTWAIT );

	stream->send( PROT_DATA, "%d", 0 );

	printf_d("[MRNFE] recieving data...nr_exited is %d nr_be is %d\n",nr_exited,nr_be);

	while (nr_exited != nr_be)
	{
		stream->recv (&tag, pack);
		switch (tag)
		{
			case PROT_DATA:
				int tid;
				pack->unpack("%d %d %d %uld %s %d %s",
						&tid,
						&pid, 
						&eid,
						&time, 
						&hostname, 
						&finish,
						&ename);

				snprintf_d(sql, 2048, "CALL insertdata (-1, %d,\"%s\", %d, \"%s\", 7,\"ffffffff\", '%llu', %d );", 
						pid,
						hostname, 
						eid, 
						ename, 
						time, 
						finish);
				
				if (mysql_query (conn, sql))
					printf_d ("[MRNFE] %s\n", mysql_error(conn));

				free(ename);
				ename = NULL;

				break;

			case PROT_CMPIDATA:											/*MPI wrapper treace 数据*/
				pack->unpack("%d %ud %s %d %s %d %d %d %d %d %d %d %d %d %d %d %d %d %uld %d",
						&mpi_rank,
						&pid,
						&hostname,
						&eid,
						&ename,
						&cuda_stream,
						&memcpy_size,
						&memcpy_type,
						&memcpy_kind,
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
				snprintf_d (sql, 2048,"CALL insertdata ( %d, %u, \"%s\", %d, \"%s\", %d, \"%x\", %llu, %d);",
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
					snprintf_d (sql, 2048,"CALL insert_comm_data ( %d, %u, \"%s\", %d, \"%s\", %d, %d, %d, %d, %d, %d, %d, %d, %d, %d, %d, %llu, %d);",
							mpi_rank,
							pid,
							hostname,
							eid,
							ename,
							mpi_tag,
							cuda_stream,
							memcpy_size,
							memcpy_type,
							memcpy_kind,
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
				ename = NULL;
				break;
			case PROT_CUPTIDATA:
				pack->unpack("%ud %s %d %d %d %d %s %s %s %s %uld %d",
						&pid, 
						&hostname,
						&device_id,
						&context_id, 
						&stream_id, 
						&type,
						&ename,
						&cupti_seg1,
						&cupti_seg2,
						&cupti_seg3,
						&time,
						&finish);

				//printf_d("[DEBUG] events: %s\n", ename);
					
				snprintf_d(sql, 2048, "CALL insert_cupti_data (%u, \"%s\", %d, %d, %d, %d, \"%s\", \"%s\", \"%s\", \"%s\", %llu, %d);" ,pid, hostname, device_id,context_id, stream_id, type,ename, cupti_seg1, cupti_seg2, cupti_seg3, time, finish);
				//	printf_d ("[DEBUG] %s\n", sql);				
				if (mysql_query (conn, sql))
				{
					printf_d ("[MRNFE] %s\n", mysql_error(conn));
					printf_d ("[DEBUG] %s\n", sql);
				}
				free(cupti_seg1);
				free(cupti_seg2);
				free(cupti_seg3);
				free(ename);
				free(hostname);
				ename = NULL;

				break;


			case PROT_PROC:												/*proc数据*/
				if(!use_proc)
					break;
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

						snprintf_d(sql, 2048, "CALL insertproc( %ld, %ld, \"%s\", %u, \"%s\", \"%s\", '%f')"
							, time_s, time_us, hostname, pid, metric, instance, fdata);

						mysql_query( conn, sql );
					}

					free(procdata);
					free(hostname);
					procdata = hostname = NULL;
				}
				break;

			case PROT_MPIRANK:											/*MPI rank数据*/

				pack->unpack("%d %d", &rank, &data);

				snprintf_d( sql, 2048, "update `%s_nodelist` set mpirank=%d where rank=%d", tablename, data, rank );
				mysql_query( conn, sql );
				//printf_d ("%s\n", sql);
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

	}

	FE::write_exiting_info();

}


int main (int argc, char *argv[])
{
	if (argc != 4 && argc != 5)
	{
		printf_d ("Usage:  mrnfe   benum fanout internum [proc]\n");
		return 0;
	}
	if (argc == 5)
	{
		if (strcmp (argv [4], "proc") != 0)
		{
			printf_d ("Usage:  mrnfe  benum fanout internum [proc]\n");
			return 0;
		}
		FE::setproc ();
		use_proc = 1;
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

	MYSQL *conn = FE::GetConn ();
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
