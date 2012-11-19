#include "wrapper.h"
#include <sys/time.h>
#ifndef MPI_WRAPPER
#define MPI_WRAPPER
#endif

#ifndef MPI_MAX_PROCESSOR_NAME
#define MPI_MAX_PROCESSOR_NAME 200			/*处理器名称最大长度*/
#endif

extern void MPI_node_info_record ();
extern int Record ();
extern Record_Event Event_init ();
extern long long unsigned gettime ();
extern int PAPI_get_info (char *, int , int);

//extern int EventSet;
extern int PAPI;
extern int EventSet;
extern int NUM_EVENT;
extern char PAPI_EVENT[PAPI_MAX_EVENT_NUM][PAPI_MAX_EVENT_LEN];
//extern long unsigned Local_offset;

/*计算通信字节数*/
static int sum_array (int *counts, MPI_Datatype type, MPI_Comm comm)
{
	int typesize, commSize, commRank, i;
	int total  =  0;
	PMPI_Comm_rank (comm, &commRank);
	PMPI_Comm_size (comm, &commSize);
	PMPI_Type_size (type, &typesize );
  
	for (i = 0; i < commSize; i++)
	{
		total += counts[i];
	} 
	return total;
}
/*转换进程标识号*/
static int translateRankToWorld (MPI_Comm comm, int rank)
{
	MPI_Group commGroup, worldGroup;
	int ranks[1], worldranks[1];
	if (comm != MPI_COMM_WORLD)
	{
		int result;
		PMPI_Comm_compare (comm, MPI_COMM_WORLD, &result);
		if (result == MPI_IDENT||result == MPI_CONGRUENT)
		{
			return rank;
		}
		else
		{
			ranks[0] = rank;
			PMPI_Comm_group (MPI_COMM_WORLD, &worldGroup);
			PMPI_Comm_group (comm, &commGroup);
			/*将一个组中的进程标识号转换成另一个组的进程标识号*/
			PMPI_Group_translate_ranks (commGroup, 1, ranks, worldGroup, worldranks);
			return worldranks[0];
		}
	}
	return rank;
}
/*
	每一进程都从所有其它进程收集数据, 相当于所有进程都执行了一个 MPI_Gather 调用
*/
int MPI_Allgather (sendbuf, sendcount, sendtype, recvbuf, recvcount, recvtype, comm)
void *sendbuf;
int sendcount;
MPI_Datatype sendtype;
void *recvbuf;
int recvcount;
MPI_Datatype recvtype;
MPI_Comm comm;
{
	int retVal, typesize;

	Record_Event Event = Event_init ();
	Event.event_name = "MPI_Allgather";
	Event.eid = 1;
	Event.comm = comm;
	PMPI_Comm_rank (comm, &Event.mpi_rank);		/*获取rank号*/
	Event.type = SEND_RECV;
	Event.sendtype = sendtype;
	Event.recvtype = recvtype;

	PMPI_Type_size (recvtype, &typesize);
	Event.sendsize = typesize * recvcount;
	Event.recvsize = typesize * recvcount;
	
	if (PAPI == PAPI_ON)
	{
		PAPI_get_info ("MPI_Allgather", 0, PAPI_PROCESS);
	}

	Event.starttime = gettime ();
	retVal = PMPI_Allgather (sendbuf, sendcount, sendtype, recvbuf, recvcount, recvtype, comm);
	Event.endtime = gettime ();

	if (PAPI == PAPI_ON)
	{
		PAPI_get_info ("MPI_Allgather", 1, PAPI_PROCESS);
	}
	Record (&Event, MPI_TRACE);
	return retVal;
}


/*
  所有进程都收集数据到指定的位置, 就如同每一个进程都执行了一个 MPI_Gatherv 调用
*/
int MPI_Allgatherv (void *sendbuf, int sendcount, MPI_Datatype sendtype, void
*recvbuf, int *recvcounts, int *displs, MPI_Datatype recvtype, MPI_Comm comm)
{
	int retVal, typesize;
	Record_Event Event = Event_init ();
	Event.event_name = "MPI_Allgatherv";
	Event.eid = 2;
	Event.comm = comm;
	PMPI_Comm_rank (comm, &Event.mpi_rank);		/*获取rank号*/
	Event.type = SEND_RECV;
	Event.sendtype = sendtype;
	Event.recvtype = recvtype;

	PMPI_Type_size (recvtype, &typesize);
	Event.sendsize = sum_array (recvcounts, recvtype, comm);
	Event.recvsize = sum_array (recvcounts, recvtype, comm);
	
	if (PAPI == PAPI_ON)
	{
		PAPI_get_info ("MPI_Allgatherv", 0, PAPI_PROCESS);
	}

	Event.starttime = gettime ();
	retVal = PMPI_Allgatherv (sendbuf, sendcount, sendtype, recvbuf, recvcounts, displs, recvtype, comm);
	Event.endtime = gettime ();

	if (PAPI == PAPI_ON)
	{
		PAPI_get_info ("MPI_Allgatherv", 1, PAPI_PROCESS);
	}

	Record (&Event, MPI_TRACE);

	return retVal;
}

/*
  归约所有进程的计算结果, 并将最终的结果传递给所有其它的进程, 相当于每一个进程都执行了一次
  MPI_Reduce 调用。
*/
int MPI_Allreduce (sendbuf, recvbuf, count, datatype, op, comm)
void *sendbuf;
void *recvbuf;
int count;
MPI_Datatype datatype;
MPI_Op op;
MPI_Comm comm;
{
	int retVal, typesize;
	Record_Event Event = Event_init ();
	Event.event_name = "MPI_Allreduce";
	Event.eid = 3;
	Event.comm = comm;
	PMPI_Comm_rank (comm, &Event.mpi_rank);		/*获取rank号*/
	Event.type = REDUCE;
	Event.sendtype = datatype;
	Event.recvtype = datatype;

	PMPI_Type_size (datatype, &typesize);
	Event.sendsize = typesize * count;
	Event.recvsize = typesize * count;

	if (PAPI == PAPI_ON)
	{
		PAPI_get_info ("MPI_Allreduce", 0, PAPI_PROCESS);
	}

	Event.starttime = gettime ();
	retVal = PMPI_Allreduce (sendbuf, recvbuf, count, datatype, op, comm);
	Event.endtime = gettime ();

	if (PAPI == PAPI_ON)
	{
		PAPI_get_info ("MPI_Allreduce", 1, PAPI_PROCESS);
	}

	Record (&Event, MPI_TRACE);
	return retVal;
}
/*
  所有进程相互交换数据
*/
int MPI_Alltoall (sendbuf, sendcount, sendtype, recvbuf, recvcount, recvtype, comm)
void *sendbuf;
int sendcount;
MPI_Datatype sendtype;
void *recvbuf;
int recvcount;
MPI_Datatype recvtype;
MPI_Comm comm;
{
	int retVal, typesize;
	Record_Event Event = Event_init ();
	Event.event_name = "MPI_Alltoall";
	Event.eid = 4;
	Event.comm = comm;
	PMPI_Comm_rank (comm, &Event.mpi_rank);		/*获取rank号*/
	Event.type = SEND_RECV;

	Event.sendtype = sendtype;
	Event.recvtype = recvtype;

	if (PAPI == PAPI_ON)
	{
		PAPI_get_info ("MPI_Alltoall", 0, PAPI_PROCESS);
	}

	Event.starttime = gettime ();
	retVal = PMPI_Alltoall (sendbuf, sendcount, sendtype, recvbuf, recvcount, recvtype, comm);
	Event.endtime = gettime ();
	
	if (PAPI == PAPI_ON)
	{
		PAPI_get_info ("MPI_Alltoall", 1, PAPI_PROCESS);
	}

	PMPI_Type_size (sendtype, &typesize);
	Event.sendsize = typesize * sendcount;
	PMPI_Type_size (recvtype, &typesize);
	Event.recvsize = typesize * recvcount;

	Record (&Event, MPI_TRACE);
	return retVal;
}
/*
  所有进程相互交换数据, 但数据有一个偏移量。
*/
int MPI_Alltoallv (sendbuf, sendcnts, sdispls, sendtype, recvbuf, recvcnts, rdispls, recvtype, comm)
void *sendbuf;
int *sendcnts;
int *sdispls;
MPI_Datatype sendtype;
void *recvbuf;
int *recvcnts;
int *rdispls;
MPI_Datatype recvtype;
MPI_Comm comm;
{
	int retVal;
	int tracksize = 0;
	Record_Event Event = Event_init ();
	Event.event_name = "MPI_Alltoallv";
	Event.eid = 5;
	Event.comm = comm;
	PMPI_Comm_rank (comm, &Event.mpi_rank);		/*获取rank号*/
	Event.type = SEND_RECV;

	Event.sendtype = sendtype;
	Event.recvtype = recvtype;

	if (PAPI == PAPI_ON)
	{
		PAPI_get_info ("MPI_Alltoallv", 0, PAPI_PROCESS);
	}

	Event.starttime = gettime ();
	retVal = PMPI_Alltoallv (sendbuf, sendcnts, sdispls, sendtype, recvbuf, recvcnts, rdispls, recvtype, comm);
	Event.endtime = gettime ();

	if (PAPI == PAPI_ON)
	{
		PAPI_get_info ("MPI_Alltoallv", 1, PAPI_PROCESS);
	}

	tracksize = sum_array (sendcnts, sendtype, comm);
	tracksize += sum_array (recvcnts, recvtype, comm);
	Event.sendsize = tracksize;
	Event.recvsize = tracksize;
	Record (&Event, MPI_TRACE);
	return retVal;
}
/*
	等待直到所有的进程都执行到这一例程才继续执行下一条语句。
*/
int MPI_Barrier (MPI_Comm comm)
{
	int retVal; 
	Record_Event Event = Event_init ();
	Event.event_name = "MPI_Barrier";
	Event.eid = 6;
	Event.comm = comm;
	PMPI_Comm_rank (comm, &Event.mpi_rank);		/*获取rank号*/
	Event.type = NONE;

	if (PAPI == PAPI_ON)
	{
		PAPI_get_info ("MPI_Barrier", 0, PAPI_PROCESS);
	}

	Event.starttime = gettime ();
	retVal = PMPI_Barrier (comm);
	Event.endtime = gettime ();
	
	if (PAPI == PAPI_ON)
	{
		PAPI_get_info ("MPI_Barrier", 1, PAPI_PROCESS);
	}
	Record (&Event, MPI_TRACE);
	return retVal;
}

/*
	将 root 进程的消息广播到所有其它的进程
*/
int MPI_Bcast (buffer, count, datatype, root, comm)
void * buffer;
int count;
MPI_Datatype datatype;
int root;
MPI_Comm comm;
{
	int retVal, typesize;
	unsigned long long volume;
	Record_Event Event = Event_init ();
	Event.event_name = "MPI_Bcast";
	Event.eid = 7;
	Event.comm = comm;
	PMPI_Comm_rank (comm, &Event.mpi_rank);		/*获取rank号*/
	Event.type = BCAST;

	Event.sendtype = datatype;
	Event.recvtype = datatype;
	PMPI_Type_size (datatype, &typesize);
	Event.sendsize = typesize * count;
	Event.recvsize = typesize * count;

	Event.src_rank = root;
	if (PAPI == PAPI_ON)
	{
		PAPI_get_info ("MPI_Bcast", 0, PAPI_PROCESS);
	}
	Event.starttime = gettime ();
	retVal = PMPI_Bcast (buffer, count, datatype, root, comm);
	Event.endtime = gettime ();

	if (PAPI == PAPI_ON)
	{
		PAPI_get_info ("MPI_Bcast", 1, PAPI_PROCESS);
	}
	Record (&Event, MPI_TRACE);
	return retVal;
}
/*
	从进程组中收集消息
*/
int MPI_Gather (sendbuf, sendcnt, sendtype, recvbuf, recvcount, recvtype, root, comm)
void *sendbuf;
int sendcnt;
MPI_Datatype sendtype;
void *recvbuf;
int recvcount;
MPI_Datatype recvtype;
int root;
MPI_Comm comm;
{
	int retVal, typesize;
	Record_Event Event = Event_init ();
	Event.event_name = "MPI_Gather";
	Event.eid = 8;
	Event.comm = comm;
	PMPI_Comm_rank (comm, &Event.mpi_rank);		/*获取rank号*/
	Event.type = SEND_RECV;

	if (Event.mpi_rank == root)
	{
		PMPI_Type_size (recvtype, &typesize);
		Event.recvsize = typesize * recvcount;
		Event.dst_rank = root;
	}
	Event.sendtype = sendtype;
	Event.recvtype = recvtype;

	if (PAPI == PAPI_ON)
	{
		PAPI_get_info ("MPI_Gather", 0, PAPI_PROCESS);
	}
	Event.starttime = gettime ();
	retVal = PMPI_Gather (sendbuf, sendcnt, sendtype, recvbuf, recvcount, recvtype, root, comm);
	Event.endtime = gettime ();

	if (PAPI == PAPI_ON)
	{
		PAPI_get_info ("MPI_Gather", 1, PAPI_PROCESS);
	}

	Record (&Event, MPI_TRACE);
	return retVal;
}
/*
	从各进程组中收集消息到指定的位置
*/
int MPI_Gatherv (sendbuf, sendcnt, sendtype, recvbuf, recvcnts, displs, recvtype, root, comm)
void *sendbuf;
int sendcnt;
MPI_Datatype sendtype;
void *recvbuf;
int *recvcnts;
int *displs;
MPI_Datatype recvtype;
int root;
MPI_Comm comm;
{
	int retVal, typesize, rank;
	Record_Event Event = Event_init ();
	Event.event_name = "MPI_Gatherv";
	Event.eid = 9;
	Event.comm = comm;
	PMPI_Comm_rank (comm, &Event.mpi_rank);		/*获取rank号*/
	Event.type = SEND_RECV;

	Event.sendtype = sendtype;
	Event.recvtype = recvtype;
	Event.dst_rank = root;

	if (PAPI == PAPI_ON)
	{
		PAPI_get_info ("MPI_Gatherv", 0, PAPI_PROCESS);
	}

	Event.starttime = gettime ();
	retVal = PMPI_Gatherv (sendbuf, sendcnt, sendtype, recvbuf, recvcnts, displs, recvtype, root, comm);
	Event.endtime = gettime ();

	if (PAPI == PAPI_ON)
	{
		PAPI_get_info ("MPI_Gatherv", 1, PAPI_PROCESS);
	}

	Event.recvsize = sum_array (recvcnts, recvtype, comm);
	Record (&Event, MPI_TRACE);
	return retVal;
}
/*
	创建一个用户定义的通信函数句柄
*/
int MPI_Op_creat (function, commute, op)
MPI_User_function *function;
int commute;
MPI_Op *op;
{
	int retVal;
	Record_Event Event = Event_init ();
	Event.event_name = "MPI_Op_creat";
	Event.eid = 10;
	Event.type = NONE;

	if (PAPI == PAPI_ON)
	{
		PAPI_get_info ("MPI_Op_creat", 0, PAPI_PROCESS);
	}
	Event.starttime = gettime ();
	retVal = PMPI_Op_create (function, commute, op);
	Event.endtime = gettime ();
	
	if (PAPI == PAPI_ON)
	{
		PAPI_get_info ("MPI_Op_creat", 1, PAPI_PROCESS);
	}
	Record (&Event, MPI_TRACE);
	return retVal;
}
/*
	释放一个用户定义的通信函数句柄
*/
int MPI_Op_free (MPI_Op * op)
{
	int retVal;
	Record_Event Event = Event_init ();
	Event.event_name = "MPI_Op_free";
	Event.eid = 11;
	Event.type = NONE;

	if (PAPI == PAPI_ON)
	{
		PAPI_get_info ("MPI_Op_free", 0, PAPI_PROCESS);
	}
	Event.starttime = gettime ();
	retVal = PMPI_Op_free (op);
	Event.endtime = gettime ();
	
	if (PAPI == PAPI_ON)
	{
		PAPI_get_info ("MPI_Op_free", 1, PAPI_PROCESS);
	}
	Record (&Event, MPI_TRACE);
	return retVal;
}
/*
	将结果归约后再发送出去
*/
int MPI_Reduce_scatter (sendbuf, recvbuf, recvcnts, datatype, op, comm)
void *sendbuf;
void *recvbuf;
int *recvcnts;
MPI_Datatype datatype;
MPI_Op op;
MPI_Comm comm;
{
	int retVal, typesize;
	Record_Event Event = Event_init ();
	Event.event_name = "MPI_Reduce_scatter";
	Event.eid = 12;
	Event.comm = comm;
	PMPI_Comm_rank (comm, &Event.mpi_rank);		/*获取rank号*/
	Event.type = SEND_RECV;

	Event.recvtype = datatype;
	Event.sendtype = datatype;

	if (PAPI == PAPI_ON)
	{
		PAPI_get_info ("MPI_Reduce_scatter", 0, PAPI_PROCESS);
	}

	Event.starttime = gettime ();
	retVal = PMPI_Reduce_scatter (sendbuf, recvbuf, recvcnts, datatype, op, comm);
	Event.endtime = gettime ();
	
	if (PAPI == PAPI_ON)
	{
		PAPI_get_info ("MPI_Reduce_scatter", 1, PAPI_PROCESS);
	}
	Event.recvsize = sum_array (recvcnts, datatype, comm);

	Record (&Event, MPI_TRACE);
	return retVal;
}
/*
	将所进程的值归约到 root 进程, 得到一个结果
*/
int MPI_Reduce (sendbuf, recvbuf, count, datatype, op, root, comm)
void *sendbuf;
void *recvbuf;
int count;
MPI_Datatype datatype;
MPI_Op op;
int root;
MPI_Comm comm;
{
	int retVal, typesize;
	Record_Event Event = Event_init ();
	Event.event_name = "MPI_Reduce";
	Event.eid = 13;
	Event.comm = comm;
	PMPI_Comm_rank (comm, &Event.mpi_rank);		/*获取rank号*/
	Event.type = REDUCE;

	Event.recvtype = datatype;
	Event.sendtype = datatype;
	Event.dst_rank = root;
	PMPI_Type_size (datatype, &typesize);
	Event.recvsize = typesize * count;

	if (PAPI == PAPI_ON)
	{
		PAPI_get_info ("MPI_Reduce", 0, PAPI_PROCESS);
	}
	Event.starttime = gettime ();
	retVal = PMPI_Reduce (sendbuf, recvbuf, count, datatype, op, root, comm);
	Event.endtime = gettime ();

	if (PAPI == PAPI_ON)
	{
		PAPI_get_info ("MPI_Reduce", 1, PAPI_PROCESS);
	}
	Record (&Event, MPI_TRACE);
	return retVal;
}
/*
	在给定的进程组上进行扫描操作
*/
int MPI_Scan (sendbuf, recvbuf, count, datatype, op, comm)
void *sendbuf;
void *recvbuf;
int count;
MPI_Datatype datatype;
MPI_Op op;
MPI_Comm comm;
{
	int retVal, typesize;
	Record_Event Event = Event_init ();
	Event.event_name = "MPI_scan";
	Event.eid = 14;
	Event.comm = comm;
	PMPI_Comm_rank (comm, &Event.mpi_rank);		/*获取rank号*/
	Event.type = RECV_ONLY;

	Event.recvtype = datatype;
	PMPI_Type_size (datatype, &typesize);
	Event.recvsize = typesize*count;

	if (PAPI == PAPI_ON)
	{
		PAPI_get_info ("MPI_scan", 0, PAPI_PROCESS);
	}
	Event.starttime = gettime ();
	retVal = PMPI_Scan (sendbuf, recvbuf, count, datatype, op, comm);
	Event.endtime = gettime ();
	
	if (PAPI == PAPI_ON)
	{
		PAPI_get_info ("MPI_scan", 1, PAPI_PROCESS);
	}
	Record (&Event, MPI_TRACE);
	return retVal;
}
/*
	将数据从一个进程发送到组中其它进程
*/
int MPI_Scatter (sendbuf, sendcnt, sendtype, recvbuf, recvcnt, recvtype, root, comm)
void *sendbuf;
int sendcnt;
MPI_Datatype sendtype;
void *recvbuf;
int recvcnt;
MPI_Datatype recvtype;
int root;
MPI_Comm comm;
{
	int retVal, typesize;
	Record_Event Event = Event_init ();
	Event.event_name = "MPI_Scatter";
	Event.eid = 15;
	Event.comm = comm;
	PMPI_Comm_rank (comm, &Event.mpi_rank);		/*获取rank号*/
	Event.type = SEND_RECV;

	Event.src_rank = root;
	Event.sendtype = sendtype;
	Event.recvtype = recvtype;

	PMPI_Type_size (sendtype, &typesize);
	Event.sendsize = typesize * sendcnt;

	if (PAPI == PAPI_ON)
	{
		PAPI_get_info ("MPI_Scatter", 0, PAPI_PROCESS);
	}
	Event.starttime = gettime ();
	retVal = PMPI_Scatter (sendbuf, sendcnt, sendtype, recvbuf, recvcnt, recvtype, root, comm);
	Event.endtime = gettime ();

	if (PAPI == PAPI_ON)
	{
		PAPI_get_info ("MPI_Scatter", 1, PAPI_PROCESS);
	}

	Record (&Event, MPI_TRACE);
	return retVal;
}
/*
	将缓冲区中的部分数据从一个进程发送到组中其它进程
*/
int MPI_Scatterv (sendbuf, sendcnts, displs, sendtype, recvbuf, recvcnt, recvtype, root, comm)
void *sendbuf;
int *sendcnts;
int *displs;
MPI_Datatype sendtype;
void *recvbuf;
int recvcnt;
MPI_Datatype recvtype;
int root;
MPI_Comm comm;
{
	int retVal;
	Record_Event Event = Event_init ();
	Event.event_name = "MPI_Scatterv";
	Event.eid = 16;
	Event.comm = comm;
	PMPI_Comm_rank (comm, &Event.mpi_rank);		/*获取rank号*/
	Event.type = SEND_RECV;

	Event.sendtype = sendtype;
	Event.recvtype = recvtype;
	Event.sendsize = sum_array (sendcnts, sendtype, comm);
	Event.src_rank = root;

	if (PAPI == PAPI_ON)
	{
		PAPI_get_info ("MPI_Scatterv", 0, PAPI_PROCESS);
	}
	Event.starttime = gettime ();
	retVal = PMPI_Scatterv (sendbuf, sendcnts, displs, sendtype, recvbuf, recvcnt, recvtype, root, comm);
	Event.endtime = gettime ();
	
	if (PAPI == PAPI_ON)
	{
		PAPI_get_info ("MPI_Scatterv", 1, PAPI_PROCESS);
	}
	Record (&Event, MPI_TRACE);
	return retVal;
}
/*
	删除与指定关键词联系的属性值。
*/
int MPI_Attr_delete (comm, keyval)
MPI_Comm comm;
int keyval;
{
	int retVal;
	Record_Event Event = Event_init ();
	Event.event_name = "MPI_Attr_delete";
	Event.eid = 17;
	Event.comm = comm;
	PMPI_Comm_rank (comm, &Event.mpi_rank);		/*获取rank号*/
	Event.type = NONE;

	if (PAPI == PAPI_ON)
	{
		PAPI_get_info ("MPI_Attr_delete", 0, PAPI_PROCESS);
	}
	Event.starttime = gettime ();
	retVal = PMPI_Attr_delete (comm, keyval);
	Event.endtime = gettime ();

	if (PAPI == PAPI_ON)
	{
		PAPI_get_info ("MPI_Attr_delete", 1, PAPI_PROCESS);
	}

	Record (&Event, MPI_TRACE);
	return retVal;
}
/*
	按关键词查找属性值
*/
int MPI_Attr_get (comm, keyval, attr_value, flag)
MPI_Comm comm;
int keyval;
void *attr_value;
int *flag;
{
	int retVal;
	Record_Event Event = Event_init ();
	Event.event_name = "MPI_Attr_get";
	Event.eid = 18;
	Event.comm = comm;
	PMPI_Comm_rank (comm, &Event.mpi_rank);		/*获取rank号*/
	Event.type = NONE;

	if (PAPI == PAPI_ON)
	{
		PAPI_get_info ("MPI_Attr_get", 0, PAPI_PROCESS);
	}
	Event.starttime = gettime ();
	retVal = PMPI_Attr_get (comm, keyval, attr_value, flag);
	Event.endtime = gettime ();

	if (PAPI == PAPI_ON)
	{
		PAPI_get_info ("MPI_Attr_get", 1, PAPI_PROCESS);
	}

	Record (&Event, MPI_TRACE);
	return retVal;
}
/*
	按关键词设置属性值。
*/
int MPI_Attr_put (comm, keyval, attr_value)
MPI_Comm comm;
int keyval;
void *attr_value;
{
	int retVal;
	Record_Event Event = Event_init ();
	Event.event_name = "MPI_Attr_put";
	Event.eid = 19;
	Event.comm = comm;
	PMPI_Comm_rank (comm, &Event.mpi_rank);		/*获取rank号*/
	Event.type = NONE;

	if (PAPI == PAPI_ON)
	{
		PAPI_get_info ("MPI_Attr_put", 0, PAPI_PROCESS);
	}
	Event.starttime = gettime ();
	retVal = PMPI_Attr_put (comm, keyval, attr_value);
	Event.endtime = gettime ();

	if (PAPI == PAPI_ON)
	{
		PAPI_get_info ("MPI_Attr_put", 1, PAPI_PROCESS);
	}
	
	Record (&Event, MPI_TRACE);
	return retVal;
}
/*
	两个通信域的比较
*/
int MPI_Comm_compare (comm1, comm2, result)
MPI_Comm comm1;
MPI_Comm comm2;
int *result;
{
	int retVal;
	Record_Event Event = Event_init ();
	Event.event_name = "MPI_Comm_compare";
	Event.eid = 20;
	Event.type = NONE;

	if (PAPI == PAPI_ON)
	{
		PAPI_get_info ("MPI_Comm_compare", 0, PAPI_PROCESS);
	}
	Event.starttime = gettime ();
	retVal = PMPI_Comm_compare (comm1, comm2, result);
	Event.endtime = gettime ();
	if (PAPI == PAPI_ON)
	{
		PAPI_get_info ("MPI_Comm_compare", 1, PAPI_PROCESS);
	}

	Record (&Event, MPI_TRACE);
	return retVal;
}
/*
	根据进程组创建新的通信域
*/
int MPI_Comm_create (comm, group, comm_out)
MPI_Comm comm;
MPI_Group group;
MPI_Comm *comm_out;
{
	int retVal;
	Record_Event Event = Event_init ();
	Event.event_name = "MPI_Comm_create";
	Event.eid = 21;
	Event.comm = comm;
	PMPI_Comm_rank (comm, &Event.mpi_rank);		/*获取rank号*/
	Event.type = NONE;

	if (PAPI == PAPI_ON)
	{
		PAPI_get_info ("MPI_Comm_create", 0, PAPI_PROCESS);
	}
	Event.starttime = gettime ();
	retVal = PMPI_Comm_create (comm, group, comm_out);
	Event.endtime = gettime ();
	if (PAPI == PAPI_ON)
	{
		PAPI_get_info ("MPI_Comm_create", 1, PAPI_PROCESS);
	}
	Record (&Event, MPI_TRACE);
	return retVal;
}
/*
	通信域复制
*/
int MPI_Comm_dup (comm, comm_out)
MPI_Comm comm;
MPI_Comm *comm_out;
{
	int retVal;
	Record_Event Event = Event_init ();
	Event.event_name = "MPI_Comm_dup";
	Event.eid = 22;
	Event.comm = comm;
	PMPI_Comm_rank (comm, &Event.mpi_rank);		/*获取rank号*/
	Event.type = NONE;

	if (PAPI == PAPI_ON)
	{
		PAPI_get_info ("MPI_Comm_dup", 0, PAPI_PROCESS);
	}
	Event.starttime = gettime ();
	retVal = PMPI_Comm_dup (comm, comm_out);
	Event.endtime = gettime ();
	
	if (PAPI == PAPI_ON)
	{
		PAPI_get_info ("MPI_Comm_dup", 1, PAPI_PROCESS);
	}
	Record (&Event, MPI_TRACE);
	return retVal;
}
/*
	释放一个通信域对象
*/
int MPI_Comm_free (comm)
MPI_Comm *comm;
{
	int retVal;
	Record_Event Event = Event_init ();
	Event.event_name = "MPI_Comm_free";
	Event.eid = 23;
	Event.comm = *comm;
	PMPI_Comm_rank (*comm, &Event.mpi_rank);		/*获取rank号*/
	Event.type = NONE;

	if (PAPI == PAPI_ON)
	{
		PAPI_get_info ("MPI_Comm_free", 0, PAPI_PROCESS);
	}
	Event.starttime = gettime ();
	retVal = PMPI_Comm_free (comm);
	Event.endtime = gettime ();
	if (PAPI == PAPI_ON)
	{
		PAPI_get_info ("MPI_Comm_free", 1, PAPI_PROCESS);
	}

	Record (&Event, MPI_TRACE);
	return retVal;
}
/*
	由给定的通信域得到组信息
*/
int MPI_Comm_group (comm, group)
MPI_Comm comm;
MPI_Group *group;
{
	int retVal;
	Record_Event Event = Event_init ();
	Event.event_name = "MPI_Comm_group";
	Event.eid = 24;
	Event.comm = comm;
	PMPI_Comm_rank (comm, &Event.mpi_rank);		/*获取rank号*/
	Event.type = NONE;

	if (PAPI == PAPI_ON)
	{
		PAPI_get_info ("MPI_Comm_group", 0, PAPI_PROCESS);
	}
	Event.starttime = gettime ();
	retVal = PMPI_Comm_group (comm, group);
	Event.endtime = gettime ();
	if (PAPI == PAPI_ON)
	{
		PAPI_get_info ("MPI_Comm_group", 1, PAPI_PROCESS);
	}

	Record (&Event, MPI_TRACE);
	return retVal;
}
/*
	得到调用进程在给定通信域中的进程标识号
*/
int MPI_Comm_rank (comm, rank)
MPI_Comm comm;
int *rank;
{
	int retVal;
	Record_Event Event = Event_init ();
	Event.event_name = "MPI_Comm_rank";
	Event.eid = 25;
	Event.comm = comm;
	PMPI_Comm_rank (comm, &Event.mpi_rank);		/*获取rank号*/
	Event.type = NONE;

	if (PAPI == PAPI_ON)
	{
		PAPI_get_info ("MPI_Comm_rank", 0, PAPI_PROCESS);
	}

	Event.starttime = gettime ();
	retVal = PMPI_Comm_rank (comm, rank);
	Event.endtime = gettime ();

	
	if (PAPI == PAPI_ON)
	{
		PAPI_get_info ("MPI_Comm_rank", 1, PAPI_PROCESS);
	}

	Record (&Event, MPI_TRACE);
	/*if (comm == MPI_COMM_WORLD)
	{
		MPI_trace_rank (*rank);
	}*/
	return retVal;
}
/*
	得到组间通信域的远程组
*/
int  MPI_Comm_remote_group (comm, group)
MPI_Comm comm;
MPI_Group *group;
{
	int retVal;
	Record_Event Event = Event_init ();
	Event.event_name = "MPI_Comm_remote_group";
	Event.eid = 26;
	Event.comm = comm;
	PMPI_Comm_rank (comm, &Event.mpi_rank);		/*获取rank号*/
	Event.type = NONE;

	if (PAPI == PAPI_ON)
	{
		PAPI_get_info ("MPI_Comm_remote_group", 0, PAPI_PROCESS);
	}
	Event.starttime = gettime ();
	retVal = PMPI_Comm_remote_group (comm, group);
	Event.endtime = gettime ();

	if (PAPI == PAPI_ON)
	{
		PAPI_get_info ("MPI_Comm_remote_group", 1, PAPI_PROCESS);
	}
	Record (&Event, MPI_TRACE);
	return retVal;
}
/*
	得到远程组的进程数
*/
int MPI_Comm_remote_size (comm, size)
MPI_Comm comm;
int *size;
{
	int retVal;
	Record_Event Event = Event_init ();
	Event.event_name = "MPI_Comm_remote_size";
	Event.eid = 27;
	Event.comm = comm;
	PMPI_Comm_rank (comm, &Event.mpi_rank);		/*获取rank号*/
	Event.type = NONE;

	if (PAPI == PAPI_ON)
	{
		PAPI_get_info ("MPI_Comm_remote_size", 0, PAPI_PROCESS);
	}
	Event.starttime = gettime ();
	retVal = PMPI_Comm_remote_size (comm, size);
	Event.endtime = gettime ();
	if (PAPI == PAPI_ON)
	{
		PAPI_get_info ("MPI_Comm_remote_size", 1, PAPI_PROCESS);
	}

	Record (&Event, MPI_TRACE);
	return retVal;
}
/*
	得到通信域组的大小
*/
int MPI_Comm_size (comm, size)
MPI_Comm comm;
int *size;
{
	int retVal;
	Record_Event Event = Event_init ();
	Event.event_name = "MPI_Comm_size";
	Event.eid = 28;
	Event.comm = comm;
	PMPI_Comm_rank (comm, &Event.mpi_rank);		/*获取rank号*/
	Event.type = NONE;

	if (PAPI == PAPI_ON)
	{
		PAPI_get_info ("MPI_Comm_size", 0, PAPI_PROCESS);
	}
	Event.starttime = gettime ();
	retVal = PMPI_Comm_size (comm, size);
	Event.endtime = gettime ();

	if (PAPI == PAPI_ON)
	{
		PAPI_get_info ("MPI_Comm_size", 1, PAPI_PROCESS);
	}

	Record (&Event, MPI_TRACE);
	return retVal;
}
/*
	按照给定的颜色和关键词创建新的通信域
*/
int MPI_Comm_split (comm, color, key, comm_out)
MPI_Comm comm;
int color;
int key;
MPI_Comm *comm_out;
{
	int retVal;
	Record_Event Event = Event_init ();
	Event.event_name = "MPI_Comm_split";
	Event.eid = 29;
	Event.comm = comm;
	PMPI_Comm_rank (comm, &Event.mpi_rank);		/*获取rank号*/
	Event.type = NONE;

	if (PAPI == PAPI_ON)
	{
		PAPI_get_info ("MPI_Comm_split", 0, PAPI_PROCESS);
	}
	Event.starttime = gettime ();
	retVal = PMPI_Comm_split (comm, color, key, comm_out);
	Event.endtime = gettime ();
	if (PAPI == PAPI_ON)
	{
		PAPI_get_info ("MPI_Comm_split", 1, PAPI_PROCESS);
	}
	Record (&Event, MPI_TRACE);
	return retVal;
}
/*
	测试给定通信域是否是域间域
*/
int MPI_Comm_test_inter (comm, flag)
MPI_Comm comm;
int *flag;
{
	int retVal;
	Record_Event Event = Event_init ();
	Event.event_name = "MPI_Comm_test_inter";
	Event.eid = 30;
	Event.comm = comm;
	PMPI_Comm_rank (comm, &Event.mpi_rank);		/*获取rank号*/
	Event.type = NONE;

	if (PAPI == PAPI_ON)
	{
		PAPI_get_info ("MPI_Comm_test_inter", 0, PAPI_PROCESS);
	}
	Event.starttime = gettime ();
	retVal = PMPI_Comm_test_inter (comm, flag);
	Event.endtime = gettime ();
	if (PAPI == PAPI_ON)
	{
		PAPI_get_info ("MPI_Comm_test_inter", 1, PAPI_PROCESS);
	}

	Record (&Event, MPI_TRACE);
	return retVal;
}
/*
	比较两个组
*/
int MPI_Group_compare (group1, group2, result)
MPI_Group group1;
MPI_Group group2;
int *result;
{
	int retVal;
	Record_Event Event = Event_init ();
	Event.event_name = "MPI_Group_compare";
	Event.eid = 31;
	Event.type = NONE;

	if (PAPI == PAPI_ON)
	{
		PAPI_get_info ("MPI_Group_compare", 0, PAPI_PROCESS);
	}

	Event.starttime = gettime ();
	retVal = PMPI_Group_compare (group1, group2, result);
	Event.endtime = gettime ();

	if (PAPI == PAPI_ON)
	{
		PAPI_get_info ("MPI_Group_compare", 1, PAPI_PROCESS);
	}
	Record (&Event, MPI_TRACE);
	return retVal;
}
/*
	根据两个组的差异创建一个新组
*/
int MPI_Group_difference (group1, group2, group_out)
MPI_Group group1;
MPI_Group group2;
MPI_Group *group_out;
{
	int retVal;
	Record_Event Event = Event_init ();
	Event.event_name = "MPI_Group_difference";
	Event.eid = 32;
	Event.type = NONE;

	if (PAPI == PAPI_ON)
	{
		PAPI_get_info ("MPI_Group_difference", 0, PAPI_PROCESS);
	}
	Event.starttime = gettime ();
	retVal = PMPI_Group_difference (group1, group2, group_out);
	Event.endtime = gettime ();
	if (PAPI == PAPI_ON)
	{
		PAPI_get_info ("MPI_Group_difference", 1, PAPI_PROCESS);
	}

	Record (&Event, MPI_TRACE);
	return retVal;
}
/*
	通过重新对一个已经存在的组进行排序, 根据未列出的成员创建一个新组
*/
int MPI_Group_excl (group, n, ranks, newgroup)
MPI_Group group;
int n;
int *ranks;
MPI_Group *newgroup;
{
	int retVal;
	Record_Event Event = Event_init ();
	Event.event_name = "MPI_Group_excl";
	Event.eid = 33;
	Event.type = NONE;

	if (PAPI == PAPI_ON)
	{
		PAPI_get_info ("MPI_Group_excl", 0, PAPI_PROCESS);
	}
	Event.starttime = gettime ();
	retVal = PMPI_Group_excl (group, n, ranks, newgroup);
	Event.endtime = gettime ();
	if (PAPI == PAPI_ON)
	{
		PAPI_get_info ("MPI_Group_excl", 1, PAPI_PROCESS);
	}

	Record (&Event, MPI_TRACE);
	return retVal;
}
/*
	释放一个组
*/
int MPI_Group_free (group)
MPI_Group *group;
{
	int retVal;
	Record_Event Event = Event_init ();
	Event.event_name = "MPI_Group_free";
	Event.eid = 34;
	Event.type = NONE;

	if (PAPI == PAPI_ON)
	{
		PAPI_get_info ("MPI_Group_free", 0, PAPI_PROCESS);
	}
	Event.starttime = gettime ();
	retVal = PMPI_Group_free (group);
	Event.endtime = gettime ();

	if (PAPI == PAPI_ON)
	{
		PAPI_get_info ("MPI_Group_free", 1, PAPI_PROCESS);
	}

	Record (&Event, MPI_TRACE);
	return retVal;
}
/*
	通过重新对一个已经存在的组进行排序, 根据列出的成员创建一个新组
*/
int MPI_Group_incl (group, n, ranks, group_out)
MPI_Group group;
int n;
int *ranks;
MPI_Group *group_out;
{
	int retVal;
	Record_Event Event = Event_init ();
	Event.event_name = "MPI_Group_incl";
	Event.eid = 35;
	Event.type = NONE;

	if (PAPI == PAPI_ON)
	{
		PAPI_get_info ("MPI_Group_incl", 0, PAPI_PROCESS);
	}
	Event.starttime = gettime ();
	retVal = PMPI_Group_incl (group, n, ranks, group_out);
	Event.endtime = gettime ();
	if (PAPI == PAPI_ON)
	{
		PAPI_get_info ("MPI_Group_incl", 1, PAPI_PROCESS);
	}

	Record (&Event, MPI_TRACE);
	return retVal;
}
/*
	根据两个已存在组的交创建一个新组
*/
int MPI_Group_intersection (group1, group2, group_out)
MPI_Group group1;
MPI_Group group2;
MPI_Group *group_out;
{
	int retVal;
	Record_Event Event = Event_init ();
	Event.event_name = "MPI_Group_intersection";
	Event.eid = 36;
	Event.type = NONE;

	if (PAPI == PAPI_ON)
	{
		PAPI_get_info ("MPI_Group_intersection", 0, PAPI_PROCESS);
	}
	Event.starttime = gettime ();
	retVal = PMPI_Group_intersection (group1, group2, group_out);
	Event.endtime = gettime ();
	if (PAPI == PAPI_ON)
	{
		PAPI_get_info ("MPI_Group_intersection", 1, PAPI_PROCESS);
	}

	Record (&Event, MPI_TRACE);
	return retVal;
}
/*
	返回调用进程在给定组中的进程标识号
*/
int MPI_Group_rank (group, rank)
MPI_Group group;
int *rank;
{
	int retVal;
	Record_Event Event = Event_init ();
	Event.event_name = "MPI_Group_rank";
	Event.eid = 37;
	Event.type = NONE;

	if (PAPI == PAPI_ON)
	{
		PAPI_get_info ("MPI_Group_rank", 0, PAPI_PROCESS);
	}

	Event.starttime = gettime ();
	retVal = PMPI_Group_rank (group, rank);
	Event.endtime = gettime ();
	if (PAPI == PAPI_ON)
	{
		PAPI_get_info ("MPI_Group_rank", 1, PAPI_PROCESS);
	}

	Record (&Event, MPI_TRACE);
	return retVal;
}
/*
	根据已存在的组, 去掉指定的部分, 创建一个新组
*/
int MPI_Group_range_excl (group, n, ranges, newgroup)
MPI_Group group;
int n;
int ranges[][3];
MPI_Group *newgroup;
{
	int retVal;
	Record_Event Event = Event_init ();
	Event.event_name = "MPI_Group_range_excl";
	Event.eid = 38;
	Event.type = NONE;

	if (PAPI == PAPI_ON)
	{
		PAPI_get_info ("MPI_Group_range_excl", 0, PAPI_PROCESS);
	}
	Event.starttime = gettime ();
	retVal = PMPI_Group_range_excl (group, n, ranges, newgroup);
	Event.endtime = gettime ();
	if (PAPI == PAPI_ON)
	{
		PAPI_get_info ("MPI_Group_range_excl", 1, PAPI_PROCESS);
	}

	Record (&Event, MPI_TRACE);
	return retVal;
}
/*
	根据已存在的组, 按照指定的部分, 创建一个新组
*/
int MPI_Group_range_incl (group, n, ranges, newgroup)
MPI_Group group;
int n;
int ranges[][3];
MPI_Group *newgroup;
{
	int retVal;
	Record_Event Event = Event_init ();
	Event.event_name = "MPI_Group_range_incl";
	Event.eid = 39;
	Event.type = NONE;

	if (PAPI == PAPI_ON)
	{
		PAPI_get_info ("MPI_Group_range_incl", 0, PAPI_PROCESS);
	}
	Event.starttime = gettime ();
	retVal = PMPI_Group_range_incl (group, n, ranges, newgroup);
	Event.endtime = gettime ();
	if (PAPI == PAPI_ON)
	{
		PAPI_get_info ("MPI_Group_range_incl", 1, PAPI_PROCESS);
	}
	Record (&Event, MPI_TRACE);
	return retVal;
}
/*
	返回给定组的大小
*/
int MPI_Group_size (group, size)
MPI_Group group;
int *size;
{
	int retVal;
	Record_Event Event = Event_init ();
	Event.event_name = "MPI_Group_size";
	Event.eid = 40;
	Event.type = NONE;

	if (PAPI == PAPI_ON)
	{
		PAPI_get_info ("MPI_Group_size", 0, PAPI_PROCESS);
	}
	Event.starttime = gettime ();
	retVal = PMPI_Group_size (group, size);
	Event.endtime = gettime ();
	if (PAPI == PAPI_ON)
	{
		PAPI_get_info ("MPI_Group_size", 1, PAPI_PROCESS);
	}

	Record (&Event, MPI_TRACE);
	return retVal;
}
/*
	将一个组中的进程标识号转换成另一个组的进程标识号
*/
int MPI_Group_translate_ranks (group_a, n, ranks_a, group_b, ranks_b)
MPI_Group group_a;
int n;
int *ranks_a;
MPI_Group group_b;
int *ranks_b;
{
	int retVal; 
	Record_Event Event = Event_init ();
	Event.event_name = "MPI_Group_translate_ranks";
	Event.eid = 41;
	Event.type = NONE;

	if (PAPI == PAPI_ON)
	{
		PAPI_get_info ("MPI_Group_translate_ranks", 0, PAPI_PROCESS);
	}
	Event.starttime = gettime ();
	retVal = PMPI_Group_translate_ranks (group_a, n, ranks_a, group_b, ranks_b);
	Event.endtime = gettime ();
	if (PAPI == PAPI_ON)
	{
		PAPI_get_info ("MPI_Group_translate_ranks", 1, PAPI_PROCESS);
	}

	Record (&Event, MPI_TRACE);
	return retVal;
}
/*
	将两个组合并为一个新组
*/
int MPI_Group_union (group1, group2, group_out)
MPI_Group group1;
MPI_Group group2;
MPI_Group *group_out;
{
	int retVal;
	Record_Event Event = Event_init ();
	Event.event_name = "MPI_Group_union";
	Event.eid = 42;
	Event.type = NONE;

	if (PAPI == PAPI_ON)
	{
		PAPI_get_info ("MPI_Group_union", 0, PAPI_PROCESS);
	}
	Event.starttime = gettime ();
	retVal = PMPI_Group_union (group1, group2, group_out);
	Event.endtime = gettime ();
	if (PAPI == PAPI_ON)
	{
		PAPI_get_info ("MPI_Group_union", 1, PAPI_PROCESS);
	}

	Record (&Event, MPI_TRACE);
	return retVal;
}
/*
	根据两个组内通信域创建一个组间通信域
*/
int MPI_Intercomm_create (local_comm, local_leader, peer_comm, remote_leader, tag, comm_out)
MPI_Comm local_comm;
int local_leader;
MPI_Comm peer_comm;
int remote_leader;
int tag;
MPI_Comm *comm_out;
{
	int retVal;
	Record_Event Event = Event_init ();
	Event.event_name = "MPI_Intercomm_create";
	Event.eid = 43;
	Event.type = NONE;
	Event.tag = tag;

	if (PAPI == PAPI_ON)
	{
		PAPI_get_info ("MPI_Intercomm_create", 0, PAPI_PROCESS);
	}
	Event.starttime = gettime ();
	retVal = PMPI_Intercomm_create (local_comm, local_leader, peer_comm, remote_leader, tag, comm_out);
	Event.endtime = gettime ();
	if (PAPI == PAPI_ON)
	{
		PAPI_get_info ("MPI_Intercomm_create", 1, PAPI_PROCESS);
	}

	Record (&Event, MPI_TRACE);
	return retVal;
}
/*
	根据组间通信域创建一个组内通信域
*/
int  MPI_Intercomm_merge (comm, high, comm_out)
MPI_Comm comm;
int high;
MPI_Comm *comm_out;
{
	int retVal;
	Record_Event Event = Event_init ();
	Event.event_name = "MPI_Intercomm_merge";
	Event.eid = 44;
	/*Event.comm = comm;
	PMPI_Comm_rank (comm, &Event.mpi_rank);*/		/*获取rank号*/
	Event.type = NONE;

	if (PAPI == PAPI_ON)
	{
		PAPI_get_info ("MPI_Intercomm_merge", 0, PAPI_PROCESS);
	}
	Event.starttime = gettime ();
	retVal = PMPI_Intercomm_merge (comm, high, comm_out);
	Event.endtime = gettime ();
	if (PAPI == PAPI_ON)
	{
		PAPI_get_info ("MPI_Intercomm_merge", 1, PAPI_PROCESS);
	}

	Record (&Event, MPI_TRACE);
	return retVal;
}
/*
	创建一个新的属性关键词
*/
int MPI_Keyval_create (copy_fn, delete_fn, keyval, extra_state)
MPI_Copy_function *copy_fn;
MPI_Delete_function *delete_fn;
int *keyval;
void *extra_state;
{
	int retVal;
	Record_Event Event = Event_init ();
	Event.event_name = "MPI_Keyval_create";
	Event.eid = 45;
	Event.type = NONE;

	if (PAPI == PAPI_ON)
	{
		PAPI_get_info ("MPI_Keyval_create", 0, PAPI_PROCESS);
	}
	Event.starttime = gettime ();
	retVal = PMPI_Keyval_create (copy_fn, delete_fn, keyval, extra_state);
	Event.endtime = gettime ();
	if (PAPI == PAPI_ON)
	{
		PAPI_get_info ("MPI_Keyval_create", 1, PAPI_PROCESS);
	}

	Record (&Event, MPI_TRACE);
	return retVal;
}
/*
	释放一个属性关键词
*/
int MPI_keyval_free (keyval)
int *keyval;
{
	int retVal;
	Record_Event Event = Event_init ();
	Event.event_name = "MPI_keyval_free";
	Event.eid = 46;
	Event.type = NONE;
	if (PAPI == PAPI_ON)
	{
		PAPI_get_info ("MPI_keyval_free", 0, PAPI_PROCESS);
	}

	Event.starttime = gettime ();
	retVal = PMPI_Keyval_free (keyval);
	Event.endtime = gettime ();
	if (PAPI == PAPI_ON)
	{
		PAPI_get_info ("MPI_keyval_free", 1, PAPI_PROCESS);
	}

	Record (&Event, MPI_TRACE);
	return retVal;
}
/*
	终止 MPI 环境及 MPI 程序的执行
*/
/* LAM MPI defines MPI_Abort as a macro! We check for this and if it is
   defined that way, we change the MPI_Abort wrapper */
#if (defined (MPI_Abort)&&defined (_ULM_MPI_H_))
int _MPI_Abort (MPI_Comm comm, int errorcode, char *file, int line)
#else
int MPI_Abort (comm, errorcode)
MPI_Comm comm;
int errorcode;
#endif
{
	int retVal;
	Record_Event Event = Event_init ();
	Event.event_name = "MPI_Abort";
	Event.eid = 47;
	PMPI_Comm_rank (comm, &Event.mpi_rank);		/*获取rank号*/
	Event.type = NONE;

	if (PAPI == PAPI_ON)
	{
		PAPI_get_info ("MPI_Abort", 0, PAPI_PROCESS);
	}
	Event.starttime = gettime ();
	retVal = PMPI_Abort (comm, errorcode);
	Event.endtime = gettime ();
	if (PAPI == PAPI_ON)
	{
		PAPI_get_info ("MPI_Abort", 1, PAPI_PROCESS);
	}

	Record (&Event, MPI_TRACE);
	return retVal;
}
/*
	将错误代码转换为错误类
*/
int MPI_Error_class (errorcode, errorclass)
int errorcode;
int *errorclass;
{
	int retVal;
	Record_Event Event = Event_init ();
	Event.event_name = "MPI_Error_class";
	Event.eid = 48;
	Event.type = NONE;

	if (PAPI == PAPI_ON)
	{
		PAPI_get_info ("MPI_Error_class", 0, PAPI_PROCESS);
	}
	Event.starttime = gettime ();
	retVal = PMPI_Error_class (errorcode, errorclass);
	Event.endtime = gettime ();
	if (PAPI == PAPI_ON)
	{
		PAPI_get_info ("MPI_Error_class", 1, PAPI_PROCESS);
	}

	Record (&Event, MPI_TRACE);
	return retVal;
}
/*
	创建 MPI 错误句柄
*/
int MPI_Errhandler_create (function, errhandler)
MPI_Handler_function *function;
MPI_Errhandler *errhandler;
{
	int retVal;
	Record_Event Event = Event_init ();
	Event.event_name = "MPI_Errhandler_create";
	Event.eid = 49;
	Event.type = NONE;

	if (PAPI == PAPI_ON)
	{
		PAPI_get_info ("MPI_Errhandler_create", 0, PAPI_PROCESS);
	}
	Event.starttime = gettime ();
	retVal = PMPI_Errhandler_create (function, errhandler);
	Event.endtime = gettime ();
	if (PAPI == PAPI_ON)
	{
		PAPI_get_info ("MPI_Errhandler_create", 1, PAPI_PROCESS);
	}

	Record (&Event, MPI_TRACE);
	return retVal;
}
/*
	释放 MPI 错误句柄
*/
int MPI_Errhandler_free (errhandler)
MPI_Errhandler *errhandler;
{
	int retVal;
	Record_Event Event = Event_init ();
	Event.event_name = "MPI_Errhandler_free";
	Event.eid = 50;
	Event.type = NONE;
	if (PAPI == PAPI_ON)
	{
		PAPI_get_info ("MPI_Errhandler_free", 0, PAPI_PROCESS);
	}

	Event.starttime = gettime ();
	retVal = PMPI_Errhandler_free (errhandler);
	Event.endtime = gettime ();
	if (PAPI == PAPI_ON)
	{
		PAPI_get_info ("MPI_Errhandler_free", 1, PAPI_PROCESS);
	}

	Record (&Event, MPI_TRACE);
	return retVal;
}
/*
	得到给定通信域的错误句柄
*/
int MPI_Errhandler_get (comm, errhandler)
MPI_Comm comm;
MPI_Errhandler *errhandler;
{
	int retVal;
	Record_Event Event = Event_init ();
	Event.event_name = "MPI_Errhandler_get";
	Event.eid = 51;
	PMPI_Comm_rank (comm, &Event.mpi_rank);		/*获取rank号*/
	Event.type = NONE;


	if (PAPI == PAPI_ON)
	{
		PAPI_get_info ("MPI_Errhandler_get", 0, PAPI_PROCESS);
	}
	Event.starttime = gettime ();
	retVal = PMPI_Errhandler_get (comm, errhandler);
	Event.endtime = gettime ();
	if (PAPI == PAPI_ON)
	{
		PAPI_get_info ("MPI_Errhandler_get", 1, PAPI_PROCESS);
	}

	Record (&Event, MPI_TRACE);
	return retVal;
}
/*
	由给定的错误代码, 返回它所对应的字符串
*/
int MPI_Error_string (errorcode, string, resultlen)
int errorcode;
char *string;
int *resultlen;
{
	int retVal;
	Record_Event Event = Event_init ();
	Event.event_name = "MPI_Error_string";
	Event.eid = 52;
	Event.type = NONE;

	if (PAPI == PAPI_ON)
	{
		PAPI_get_info ("MPI_Error_string", 0, PAPI_PROCESS);
	}
	Event.starttime = gettime ();
	retVal = PMPI_Error_string (errorcode, string, resultlen);
	Event.endtime = gettime ();
	if (PAPI == PAPI_ON)
	{
		PAPI_get_info ("MPI_Error_string", 1, PAPI_PROCESS);
	}

	Record (&Event, MPI_TRACE);
	return retVal;
}
/*
	设置 MPI 错误句柄
*/
int MPI_Errhandler_set (comm, errhandler)
MPI_Comm comm;
MPI_Errhandler errhandler;
{
	int retVal;
	Record_Event Event = Event_init ();
	Event.event_name = "MPI_Errhandler_set";
	Event.eid = 53;
	PMPI_Comm_rank (comm, &Event.mpi_rank);		/*获取rank号*/
	Event.type = NONE;

	if (PAPI == PAPI_ON)
	{
		PAPI_get_info ("MPI_Errhandler_set", 0, PAPI_PROCESS);
	}
	Event.starttime = gettime ();
	retVal = PMPI_Errhandler_set (comm, errhandler);
	Event.endtime = gettime ();
	if (PAPI == PAPI_ON)
	{
		PAPI_get_info ("MPI_Errhandler_set", 1, PAPI_PROCESS);
	}

	Record (&Event, MPI_TRACE);
	return retVal;
}
/*
	结束 MPI 运行环境
*/
int MPI_Finalize ()
{
	int retVal, size;
	Record_Event Event = Event_init ();
	Event.event_name = "MPI_Finalize";
	Event.eid = 54;
	Event.type = NONE;

/*	PMPI_Get_processor_name (procname, &procnamelength);*/
	if (PAPI == PAPI_ON)
	{
		PAPI_get_info ("MPI_Finalize", 0, PAPI_PROCESS);
	}
	Event.starttime = gettime ();
	retVal = PMPI_Finalize ();
	Event.endtime = gettime ();
	if (PAPI == PAPI_ON)
	{
		PAPI_get_info ("MPI_Finalize", 1, PAPI_PROCESS);
	}
	
	Record (&Event, MPI_TRACE);
	return retVal;
}
/*
	得到处理器名称
*/
int MPI_Get_processor_name (name, resultlen)
char *name;
int *resultlen;
{
	int retVal;
	Record_Event Event = Event_init ();
	Event.event_name = "MPI_Get_processor_name";
	Event.eid = 55;
	Event.type = NONE;

	if (PAPI == PAPI_ON)
	{
		PAPI_get_info ("MPI_Get_processor_name", 0, PAPI_PROCESS);
	}
	Event.starttime = gettime ();
	retVal = PMPI_Get_processor_name (name, resultlen);
	Event.endtime = gettime ();
	if (PAPI == PAPI_ON)
	{
		PAPI_get_info ("MPI_Get_processor_name", 1, PAPI_PROCESS);
	}

	Record (&Event, MPI_TRACE);
	return retVal;
}
/*
	MPI 执行环境初始化
*/
int MPI_Init (argc, argv)
int *argc;
char ***argv;
{
	int retVal, size;

	Record_Event Event = Event_init ();
	MPI_node_info Node;
	Event.event_name = "MPI_Init";
	Event.eid = 56;
	Event.type = NONE;

	if (PAPI == PAPI_ON)
	{
		int i, ret;
		long long values1 [PAPI_MAX_EVENT_NUM];
		long long values2 [PAPI_MAX_EVENT_NUM];
		PAPI_Event Event_papi_start;
		PAPI_Event Event_papi_end;

		Event_papi_start.event_name = "MPI_Init";
		Event_papi_start.finish = 0;
		Event_papi_start.time = gettime();
		if ((ret=PAPI_start(EventSet)) != PAPI_OK)
			ERROR_RETURN(ret);

		if ((ret=PAPI_stop(EventSet,values1)) != PAPI_OK)
			ERROR_RETURN(ret);

		Event.starttime = gettime ();
		retVal = PMPI_Init (argc, argv);
		Event.endtime = gettime ();

		Event_papi_end.event_name = "MPI_Init";
		Event_papi_end.finish = 1;
		Event_papi_end.time = gettime();
		if ((ret=PAPI_start(EventSet)) != PAPI_OK)
			ERROR_RETURN(ret);

		if ((ret=PAPI_stop(EventSet,values2)) != PAPI_OK)
			ERROR_RETURN(ret);
		Record (&Event, MPI_TRACE);

		for (i = 0; i < NUM_EVENT; i++)
		{
			Event_papi_start.papi_event = PAPI_EVENT[i];
			Event_papi_start.data = values1[i];
			PAPI_info_record (Event_papi_start);
		}

		for (i = 0; i < NUM_EVENT; i++)
		{
			Event_papi_end.papi_event = PAPI_EVENT[i];
			Event_papi_end.data = values2[i];
			PAPI_info_record (Event_papi_end);
		}
		/*记录mpi节点信息*/
		PMPI_Comm_rank (MPI_COMM_WORLD, &Node.mpi_rank);
		PMPI_Get_processor_name (Node.procname, &Node.procnamelength);
		PMPI_Comm_size (MPI_COMM_WORLD, &Node.size);
		Node.pid = getpid ();
		MPI_node_info_record (Node);
	}
	else 
	{
		Event.starttime = gettime ();
		retVal = PMPI_Init (argc, argv);
		Event.endtime = gettime ();

		Record (&Event, MPI_TRACE);

		/*记录mpi节点信息*/
		PMPI_Comm_rank (MPI_COMM_WORLD, &Node.mpi_rank);
		PMPI_Get_processor_name (Node.procname, &Node.procnamelength);
		PMPI_Comm_size (MPI_COMM_WORLD, &Node.size);
		Node.pid = getpid ();
		MPI_node_info_record (Node);
	}


	return retVal;
}
/*
	初始化 MPI 和 MPI 线程环境
*/
int MPI_Init_thread (argc, argv, required, provided)
int *argc;
char ***argv;
int required;
int *provided;
{
	int retVal;
	Record_Event Event = Event_init ();
	Event.event_name = "MPI_Init_thread";
	Event.eid = 57;
	Event.type = NONE;

	if (PAPI == PAPI_ON)
	{
		PAPI_get_info ("MPI_Init_thread", 0, PAPI_PROCESS);
	}
	Event.starttime = gettime ();
	retVal = PMPI_Init_thread (argc, argv, required, provided);
	Event.endtime = gettime ();
	if (PAPI == PAPI_ON)
	{
		PAPI_get_info ("MPI_Init_thread", 1, PAPI_PROCESS);
	}

	Record (&Event, MPI_TRACE);
	/*
	...1782
	*/

	return retVal;
}
/*
	返回 MPI_Wtime 的分辨率
*/
double MPI_Wtick ()
{
	double retVal;
	Record_Event Event = Event_init ();
	Event.event_name = "MPI_Wtick";
	Event.eid = 58;
	Event.type = NONE;

	if (PAPI == PAPI_ON)
	{
		PAPI_get_info ("MPI_Wtick", 0, PAPI_PROCESS);
	}
	Event.starttime = gettime ();
	retVal = PMPI_Wtick ();
	Event.endtime = gettime ();

	if (PAPI == PAPI_ON)
	{
		PAPI_get_info ("MPI_Wtick", 1, PAPI_PROCESS);
	}
	Record (&Event, MPI_TRACE);
	return retVal;
}
/*
	返回调用进程的流逝时间
*/
double MPI_Wtime ()
{
	double retVal;
	Record_Event Event = Event_init ();
	Event.event_name = "MPI_Wtime";
	Event.eid = 59;
	Event.type = NONE;

	if (PAPI == PAPI_ON)
	{
		PAPI_get_info ("MPI_Wtime", 0, PAPI_PROCESS);
	}
	Event.starttime = gettime ();
	retVal = PMPI_Wtime ();
	Event.endtime = gettime ();
	if (PAPI == PAPI_ON)
	{
		PAPI_get_info ("MPI_Wtime", 1, PAPI_PROCESS);
	}

	Record (&Event, MPI_TRACE);
	return retVal;
}
/*
	得到给定位置在内存中的地址
*/
int MPI_Address (location, address)
void *location;
MPI_Aint *address;
{
	int retVal;
	Record_Event Event = Event_init ();
	Event.event_name = "MPI_Address";
	Event.eid = 60;
	Event.type = NONE;

	if (PAPI == PAPI_ON)
	{
		PAPI_get_info ("MPI_Address", 0, PAPI_PROCESS);
	}
	Event.starttime = gettime ();
	retVal = PMPI_Address (location, address);
	Event.endtime = gettime ();

	if (PAPI == PAPI_ON)
	{
		PAPI_get_info ("MPI_Address", 1, PAPI_PROCESS);
	}
	Record (&Event, MPI_TRACE);
	return retVal;
}
/*
	使用用户声明的缓冲进行发送
*/
int MPI_Bsend (buf, count, datatype, dest, tag, comm)
void *buf;
int count;
MPI_Datatype datatype;
int dest;
int tag;
MPI_Comm comm;
{
	int retVal, typesize;
	Record_Event Event = Event_init ();
	Event.event_name = "MPI_Bsend";
	Event.eid = 61;
	Event.comm = comm;
	Event.tag = tag;
	PMPI_Comm_rank (comm, &Event.mpi_rank);		/*获取rank号*/
	Event.type = SEND_ONLY;

	Event.sendtype = datatype;
	Event.recvtype = datatype;
	PMPI_Type_size (datatype, &typesize);
 	Event.sendsize = typesize*count;
 	Event.recvsize = typesize*count;

 	Event.src_rank = Event.mpi_rank;
	Event.dst_rank = dest;

	if (PAPI == PAPI_ON)
	{
		PAPI_get_info ("MPI_Bsend", 0, PAPI_PROCESS);
	}
	Event.starttime = gettime ();
 	retVal = PMPI_Bsend (buf, count, datatype, dest, tag, comm);
 	Event.endtime = gettime ();
 	if (PAPI == PAPI_ON)
	{
		PAPI_get_info ("MPI_Bsend", 1, PAPI_PROCESS);
	}

 	Record (&Event, MPI_TRACE);
 	return retVal;
}
/*
	建立缓存发送句柄
*/
int MPI_Bsend_init (buf, count, datatype, dest, tag, comm, request)
void *buf;
int count;
MPI_Datatype datatype;
int dest;
int tag;
MPI_Comm comm;
MPI_Request *request;
{
	int retVal;
	Record_Event Event = Event_init ();
	Event.event_name = "MPI_Bsend_init";
	Event.eid = 62;
	Event.comm = comm;
	Event.tag = tag;
	PMPI_Comm_rank (comm, &Event.mpi_rank);		/*获取rank号*/
	Event.type = NONE;

	Event.sendtype = datatype;
	Event.dst_rank = dest;

	if (PAPI == PAPI_ON)
	{
		PAPI_get_info ("MPI_Bsend_init", 0, PAPI_PROCESS);
	}
	Event.starttime = gettime ();
	retVal = PMPI_Bsend_init (buf, count, datatype, dest, tag, comm, request);
	Event.endtime = gettime ();
	if (PAPI == PAPI_ON)
	{
		PAPI_get_info ("MPI_Bsend_init", 1, PAPI_PROCESS);
	}

	Record (&Event, MPI_TRACE);
	return retVal;
}
/*
	将一个用户指定的缓冲区用于消息发送的目的
*/
int MPI_Buffer_attach (buffer, size)
void *buffer;
int size;
{
	int retVal;
	Record_Event Event = Event_init ();
	Event.event_name = "MPI_Buffer_attach";
	Event.eid = 63;
	Event.type = NONE;

	if (PAPI == PAPI_ON)
	{
		PAPI_get_info ("MPI_Buffer_attach", 0, PAPI_PROCESS);
	}
	Event.starttime = gettime ();
	retVal = PMPI_Buffer_attach (buffer, size);
	Event.endtime = gettime ();

	if (PAPI == PAPI_ON)
	{
		PAPI_get_info ("MPI_Buffer_attach", 1, PAPI_PROCESS);
	}
	Record (&Event, MPI_TRACE);
	return retVal;
}
/*
	移走一个指定的发送缓冲区
*/
int MPI_Buffer_detach (buffer, size)
void *buffer;
int *size;
{
	int retVal;
	Record_Event Event = Event_init ();
	Event.event_name = "MPI_Buffer_detach";
	Event.eid = 64;
	Event.type = NONE;

	if (PAPI == PAPI_ON)
	{
		PAPI_get_info ("MPI_Buffer_detach", 0, PAPI_PROCESS);
	}
	Event.starttime = gettime ();
	retVal = PMPI_Buffer_detach (buffer, size);
	Event.endtime = gettime ();
	if (PAPI == PAPI_ON)
	{
		PAPI_get_info ("MPI_Buffer_detach", 1, PAPI_PROCESS);
	}

	Record (&Event, MPI_TRACE);
	return retVal;
}
/*
	取消一个通信请求
*/
int MPI_Cancel (request)
MPI_Request *request;
{
	int retVal;
	Record_Event Event = Event_init ();
	Event.event_name = "MPI_Cancel";
	Event.eid = 65;
	Event.type = NONE;

	if (PAPI == PAPI_ON)
	{
		PAPI_get_info ("MPI_Cancel", 0, PAPI_PROCESS);
	}
	Event.starttime = gettime ();
	retVal = PMPI_Cancel (request);
	Event.endtime = gettime ();
	if (PAPI == PAPI_ON)
	{
		PAPI_get_info ("MPI_Cancel", 1, PAPI_PROCESS);
	}

	Record (&Event, MPI_TRACE);
	return retVal;
}
/*
	释放通信申请对象
*/
int MPI_Request_free (request)
MPI_Request *request;
{
	int retVal;
	Record_Event Event = Event_init ();
	Event.event_name = "MPI_Request_free";
	Event.eid = 66;
	Event.type = NONE;

	if (PAPI == PAPI_ON)
	{
		PAPI_get_info ("MPI_Request_free", 0, PAPI_PROCESS);
	}
	Event.starttime = gettime ();
	retVal = PMPI_Request_free (request);
	Event.endtime = gettime ();
	if (PAPI == PAPI_ON)
	{
		PAPI_get_info ("MPI_Request_free", 1, PAPI_PROCESS);
	}
	
	Record (&Event, MPI_TRACE);
	return retVal;
}
/*
	创建接收句柄
*/
int MPI_Recv_init (buf, count, datatype, source, tag, comm, request)
void *buf;
int count;
MPI_Datatype datatype;
int source;
int tag;
MPI_Comm comm;
MPI_Request *request;
{
	int retVal;
	Record_Event Event = Event_init ();
	Event.event_name = "MPI_Recv_init";
	Event.eid = 67;
	Event.comm = comm;
	Event.tag = tag;
	PMPI_Comm_rank (comm, &Event.mpi_rank);		/*获取rank号*/
	Event.type = NONE;

	Event.recvtype = datatype;
	Event.src_rank = source;

	if (PAPI == PAPI_ON)
	{
		PAPI_get_info ("MPI_Recv_init", 0, PAPI_PROCESS);
	}
	Event.starttime = gettime ();
	retVal = PMPI_Recv_init (buf, count, datatype, source, tag, comm, request);
	Event.endtime = gettime ();
	if (PAPI == PAPI_ON)
	{
		PAPI_get_info ("MPI_Recv_init", 1, PAPI_PROCESS);
	}

	Record (&Event, MPI_TRACE);
	return retVal;
}
/*
	创建一个标准发送的句柄
*/
int MPI_Send_init (buf, count, datatype, dest, tag, comm, request)
void *buf;
int count;
MPI_Datatype datatype;
int dest;
int tag;
MPI_Comm comm;
MPI_Request *request;
{
	int retVal;
	Record_Event Event = Event_init ();
	Event.event_name = "MPI_Send_init";
	Event.eid = 68;
	Event.comm = comm;
	Event.tag = tag;
	PMPI_Comm_rank (comm, &Event.mpi_rank);		/*获取rank号*/
	Event.type = NONE;

	Event.sendtype = datatype;
	Event.dst_rank = dest;

	if (PAPI == PAPI_ON)
	{
		PAPI_get_info ("MPI_Send_init", 0, PAPI_PROCESS);
	}
	Event.starttime = gettime ();
	retVal = PMPI_Send_init (buf, count, datatype, dest, tag, comm, request);
	Event.endtime = gettime ();
	if (PAPI == PAPI_ON)
	{
		PAPI_get_info ("MPI_Send_init", 1, PAPI_PROCESS);
	}

	Record (&Event, MPI_TRACE);
	return retVal;
}
/*
	返回给定数据类型中基本元素的个数
*/
int MPI_Get_elements (status, datatype, elements)
MPI_Status *status;
MPI_Datatype datatype;
int *elements;
{
	int retVal;
	Record_Event Event = Event_init ();
	Event.event_name = "MPI_Get_elements";
	Event.eid = 69;
	Event.type = NONE;

	if (PAPI == PAPI_ON)
	{
		PAPI_get_info ("MPI_Get_elements", 0, PAPI_PROCESS);
	}
	Event.starttime = gettime ();
	retVal = PMPI_Get_elements (status, datatype, elements);
	Event.endtime = gettime ();
	if (PAPI == PAPI_ON)
	{
		PAPI_get_info ("MPI_Get_elements", 1, PAPI_PROCESS);
	}

	Record (&Event, MPI_TRACE);
	return retVal;
}
/*
	得到以给定数据类型为单位的数据的个数
*/
int MPI_Get_count (status, datatype, count)
MPI_Status *status;
MPI_Datatype datatype;
int *count;
{
	int retVal;
	Record_Event Event = Event_init ();
	Event.event_name = "MPI_Get_count";
	Event.eid = 70;
	Event.type = NONE;

	if (PAPI == PAPI_ON)
	{
		PAPI_get_info ("MPI_Get_count", 0, PAPI_PROCESS);
	}
	Event.starttime = gettime ();
	retVal = PMPI_Get_count (status, datatype, count);
	Event.endtime = gettime ();
	if (PAPI == PAPI_ON)
	{
		PAPI_get_info ("MPI_Get_count", 1, PAPI_PROCESS);
	}

	Record (&Event, MPI_TRACE);
	return retVal;
}
/*
	非阻塞缓冲区发送
*/
int MPI_Ibsend (buf, count, datatype, dest, tag, comm, request)
void *buf;
int count;
MPI_Datatype datatype;
int dest;
int tag;
MPI_Comm comm;
MPI_Request *request;
{
	int retVal, typesize;
	Record_Event Event = Event_init ();
	Event.event_name = "MPI_Ibsend";
	Event.eid = 71;
	Event.comm = comm;
	Event.tag = tag;
	PMPI_Comm_rank (comm, &Event.mpi_rank);		/*获取rank号*/
	Event.type = SEND_ONLY;

	Event.sendtype = datatype;
	Event.recvtype = datatype;

	Event.src_rank = Event.mpi_rank;
	Event.dst_rank = dest;
	
	PMPI_Type_size (datatype, &typesize);
  	Event.sendsize = typesize * count;
  	Event.recvsize = typesize * count;

  	if (PAPI == PAPI_ON)
	{
		PAPI_get_info ("MPI_Ibsend", 0, PAPI_PROCESS);
	}
  	Event.starttime = gettime ();
  	retVal = PMPI_Ibsend (buf, count, datatype, dest, tag, comm, request);
  	Event.endtime = gettime ();
  	if (PAPI == PAPI_ON)
	{
		PAPI_get_info ("MPI_Ibsend", 1, PAPI_PROCESS);
	}
  	Record (&Event, MPI_TRACE);
  	return retVal;
}
/*
	非阻塞消息测试
*/
int MPI_Iprobe (source, tag, comm, flag, status)
int source;
int tag;
MPI_Comm comm;
int *flag;
MPI_Status *status;
{
	int retVal;
	Record_Event Event = Event_init ();
	Event.event_name = "MPI_Iprobe";
	Event.eid = 72;
	Event.comm = comm;
	Event.tag = tag;
	PMPI_Comm_rank (comm, &Event.mpi_rank);		/*获取rank号*/
	Event.type = SEND_ONLY;

	Event.src_rank = source;
	if (PAPI == PAPI_ON)
	{
		PAPI_get_info ("MPI_Iprobe", 0, PAPI_PROCESS);
	}
	Event.starttime = gettime ();
	retVal = PMPI_Iprobe (source, tag, comm, flag, status);
	Event.endtime = gettime ();

	if (PAPI == PAPI_ON)
	{
		PAPI_get_info ("MPI_Iprobe", 1, PAPI_PROCESS);
	}
	Record (&Event, MPI_TRACE);
	return retVal;
}
/*
	非阻塞接收
*/
int MPI_Irecv (buf, count, datatype, source, tag, comm, request)
void *buf;
int count;
MPI_Datatype datatype;
int source;
int tag;
MPI_Comm comm;
MPI_Request *request;
{
	int retVal, typesize;
	Record_Event Event = Event_init ();
	Event.event_name = "MPI_Irecv";
	Event.eid = 73;
	Event.comm = comm;
	Event.tag = tag;
	PMPI_Comm_rank (comm, &Event.mpi_rank);		/*获取rank号*/
	Event.type = RECV_ONLY;

	PMPI_Type_size (datatype, &typesize);	
	Event.sendsize = typesize * count;
	Event.recvsize = typesize * count;

	Event.sendtype = datatype;
	Event.recvtype = datatype;

	Event.src_rank = source;
	Event.dst_rank = Event.mpi_rank;
	
	if (PAPI == PAPI_ON)
	{
		PAPI_get_info ("MPI_Irecv", 0, PAPI_PROCESS);
	}
	Event.starttime = gettime ();
	retVal = PMPI_Irecv (buf, count, datatype, source, tag, comm, request);
	Event.endtime = gettime ();
	if (PAPI == PAPI_ON)
	{
		PAPI_get_info ("MPI_Irecv", 1, PAPI_PROCESS);
	}
	
	Record (&Event, MPI_TRACE);
	return retVal;
}
/*
	非阻塞就绪发送
*/
int MPI_Irsend (buf, count, datatype, dest, tag, comm, request)
void *buf;
int count;
MPI_Datatype datatype;
int dest;
int tag;
MPI_Comm comm;
MPI_Request *request;
{
	int retVal, typesize3;
	Record_Event Event = Event_init ();
	Event.event_name = "MPI_Irsend";
	Event.eid = 74;
	Event.comm = comm;
	Event.tag = tag;
	PMPI_Comm_rank (comm, &Event.mpi_rank);		/*获取rank号*/
	Event.type = SEND_ONLY;

	Event.src_rank = Event.mpi_rank;
	Event.dst_rank = dest;

	Event.sendtype = datatype;
	Event.recvtype = datatype;
	if (dest != MPI_PROC_NULL)
	{
		PMPI_Type_size (datatype, &typesize3);
		Event.sendsize = typesize3 * count;
		Event.recvsize = typesize3 * count;
	}

	if (PAPI == PAPI_ON)
	{
		PAPI_get_info ("MPI_Irsend", 0, PAPI_PROCESS);
	}
	Event.starttime = gettime ();
	retVal = PMPI_Irsend (buf, count, datatype, dest, tag, comm, request);
	Event.endtime = gettime ();
	if (PAPI == PAPI_ON)
	{
		PAPI_get_info ("MPI_Irsend", 1, PAPI_PROCESS);
	}
	
	Record (&Event, MPI_TRACE);

	return retVal;
}
/*
	非阻塞发送
*/
int MPI_Isend (buf, count, datatype, dest, tag, comm, request)
void *buf;
int count;
MPI_Datatype datatype;
int dest;
int tag;
MPI_Comm comm;
MPI_Request *request;
{
	int retVal, typesize;
	Record_Event Event = Event_init ();
	Event.event_name = "MPI_Isend";
	Event.eid = 75;
	Event.comm = comm;
	Event.tag = tag;
	PMPI_Comm_rank (comm, &Event.mpi_rank);		/*获取rank号*/
	Event.type = SEND_ONLY;

	Event.src_rank = Event.mpi_rank;
	Event.dst_rank = dest;

	Event.sendtype = datatype;
	Event.recvtype = datatype;
	PMPI_Type_size (datatype, &typesize);
	Event.sendsize = typesize * count;
	Event.recvsize = typesize * count;

	if (PAPI == PAPI_ON)
	{
		PAPI_get_info ("MPI_Isend", 0, PAPI_PROCESS);
	}
	Event.starttime = gettime ();
	retVal = PMPI_Isend (buf, count, datatype, dest, tag, comm, request);
	Event.endtime = gettime ();
	if (PAPI == PAPI_ON)
	{
		PAPI_get_info ("MPI_Issend", 1, PAPI_PROCESS);
	}
	
	Record (&Event, MPI_TRACE);
	return retVal;
}
/*
	非阻塞同步发送
*/
int MPI_Issend (buf, count, datatype, dest, tag, comm, request)
void *buf;
int count;
MPI_Datatype datatype;
int dest;
int tag;
MPI_Comm comm;
MPI_Request *request;
{
	int retVal, typesize3;
	Record_Event Event = Event_init ();
	Event.event_name = "MPI_Issend";
	Event.eid = 76;
	Event.comm = comm;
	Event.tag = tag;
	PMPI_Comm_rank (comm, &Event.mpi_rank);		/*获取rank号*/
	Event.type = SEND_ONLY;

	Event.sendtype = datatype;
	Event.recvtype = datatype;

	if (dest != MPI_PROC_NULL)
	{
		PMPI_Type_size (datatype, &typesize3);
		Event.sendsize = typesize3*count;
		Event.recvsize = typesize3*count;
	}
	Event.src_rank = Event.mpi_rank;
	Event.dst_rank = dest;
	
	if (PAPI == PAPI_ON)
	{
		PAPI_get_info ("MPI_Issend", 0, PAPI_PROCESS);
	}
	Event.starttime = gettime ();
	retVal = PMPI_Issend (buf, count, datatype, dest, tag, comm, request);
	Event.endtime = gettime ();
	if (PAPI == PAPI_ON)
	{
		PAPI_get_info ("MPI_Issend", 1, PAPI_PROCESS);
	}
	
	Record (&Event, MPI_TRACE);
	return retVal;
}
/*
	将数据打包, 放到一个连续的缓冲区中
*/
int MPI_Pack (inbuf, incount, type, outbuf, outcount, position, comm)
void *inbuf;
int incount;
MPI_Datatype type;
void *outbuf;
int outcount;
int *position;
MPI_Comm comm;
{
	int retVal, typesize;
	Record_Event Event = Event_init ();
	Event.event_name = "MPI_Pack";
	Event.eid = 77;
	Event.comm = comm;
	PMPI_Comm_rank (comm, &Event.mpi_rank);		/*获取rank号*/
	Event.type = NONE;

	if (PAPI == PAPI_ON)
	{
		PAPI_get_info ("MPI_Pack", 0, PAPI_PROCESS);
	}
	Event.starttime = gettime ();
	retVal = PMPI_Pack (inbuf, incount, type, outbuf, outcount, position, comm);
	Event.endtime = gettime ();
	if (PAPI == PAPI_ON)
	{
		PAPI_get_info ("MPI_Pack", 1, PAPI_PROCESS);
	}

	Record (&Event, MPI_TRACE);
	return retVal;
}
/*
	返回需要打包的数据类型的大小
*/
int MPI_Pack_size (incount, datatype, comm, size)
int incount;
MPI_Datatype datatype;
MPI_Comm comm;
int *size;
{
	int retVal;
	Record_Event Event = Event_init ();
	Event.event_name = "MPI_Pack_size";
	Event.eid = 78;
	Event.comm = comm;
	PMPI_Comm_rank (comm, &Event.mpi_rank);		/*获取rank号*/
	Event.type = NONE;

	if (PAPI == PAPI_ON)
	{
		PAPI_get_info ("MPI_Pack_size", 0, PAPI_PROCESS);
	}
	Event.starttime = gettime ();
	retVal = PMPI_Pack_size (incount, datatype, comm, size);
	Event.endtime = gettime ();
	if (PAPI == PAPI_ON)
	{
		PAPI_get_info ("MPI_Pack_size", 1, PAPI_PROCESS);
	}

	Record (&Event, MPI_TRACE);
	return retVal;
}
/*
	阻塞消息测试
*/
int MPI_Probe (source, tag, comm, status)
int source;
int tag;
MPI_Comm comm;
MPI_Status *status;
{
	int retVal;
	Record_Event Event = Event_init ();
	Event.event_name = "MPI_Probe";
	Event.eid = 79;
	Event.comm = comm;
	Event.tag = tag;
	PMPI_Comm_rank (comm, &Event.mpi_rank);		/*获取rank号*/
	Event.type = NONE;
	Event.src_rank = source;

	if (PAPI == PAPI_ON)
	{
		PAPI_get_info ("MPI_Probe", 0, PAPI_PROCESS);
	}
	Event.starttime = gettime ();
	retVal = PMPI_Probe (source, tag, comm, status);
	Event.endtime = gettime ();
	if (PAPI == PAPI_ON)
	{
		PAPI_get_info ("MPI_Probe", 1, PAPI_PROCESS);
	}

	Record (&Event, MPI_TRACE);
	return retVal;
}
/*
	标准接收
*/
int MPI_Recv (buf, count, datatype, source, tag, comm, status)
void *buf;
int count;
MPI_Datatype datatype;
int source;
int tag;
MPI_Comm comm;
MPI_Status *status;
{
	int retVal, size = 0;
	Record_Event Event = Event_init ();
	Event.event_name = "MPI_Recv";
	Event.eid = 80;
	Event.comm = comm;
	Event.tag = tag;
	PMPI_Comm_rank (comm, &Event.mpi_rank);		/*获取rank号*/
	Event.type = RECV_ONLY;

	Event.recvtype = datatype;
	Event.sendtype = datatype;
	Event.src_rank = source;
	Event.dst_rank = Event.mpi_rank;

	if (PAPI == PAPI_ON)
	{
		PAPI_get_info ("MPI_Recv", 0, PAPI_PROCESS);
	}
	Event.starttime = gettime ();
	retVal = PMPI_Recv (buf, count, datatype, source, tag, comm, status);
	Event.endtime = gettime ();
	if (PAPI == PAPI_ON)
	{
		PAPI_get_info ("MPI_Recv", 1, PAPI_PROCESS);
	}

	if (source != MPI_PROC_NULL && retVal == MPI_SUCCESS)
		PMPI_Get_count (status, MPI_BYTE, &size);		/*得到以给字节为单位的数据的个数*/
	Event.sendsize = size;
	Event.recvsize = size;

	Record (&Event, MPI_TRACE);
	return retVal;
}
/*
	就绪发送
*/
int MPI_Rsend (buf, count, datatype, dest, tag, comm)
void *buf;
int count;
MPI_Datatype datatype;
int dest;
int tag;
MPI_Comm comm;
{
	int retVal, typesize;
	Record_Event Event = Event_init ();
	Event.event_name = "MPI_Rsend";
	Event.eid = 81;
	Event.comm = comm;
	Event.tag = tag;
	PMPI_Comm_rank (comm, &Event.mpi_rank);		/*获取rank号*/
	Event.type = SEND_ONLY;

	Event.sendtype = datatype;
	Event.recvtype = datatype;

	PMPI_Type_size (datatype, &typesize);
	Event.sendsize = typesize * count;
	Event.recvsize = typesize * count;
	Event.src_rank = Event.mpi_rank;
	Event.dst_rank = dest;

	if (PAPI == PAPI_ON)
	{
		PAPI_get_info ("MPI_Rsend", 0, PAPI_PROCESS);
	}
	Event.starttime = gettime ();
	retVal = PMPI_Rsend (buf, count, datatype, dest, tag, comm);
	Event.endtime = gettime ();

	if (PAPI == PAPI_ON)
	{
		PAPI_get_info ("MPI_Rsend", 1, PAPI_PROCESS);
	}
	Record (&Event, MPI_TRACE);
	return retVal;
}
/*
	创建就绪发送句柄
*/
int MPI_Rsend_init (buf, count, datatype, dest, tag, comm, request)
void *buf;
int count;
MPI_Datatype datatype;
int dest;
int tag;
MPI_Comm comm;
MPI_Request *request;
{
	int retVal;
	Record_Event Event = Event_init ();
	Event.event_name = "MPI_Rsend_init";
	Event.eid = 82;
	Event.comm = comm;
	Event.tag = tag;
	PMPI_Comm_rank (comm, &Event.mpi_rank);		/*获取rank号*/
	Event.type = NONE;

	if (PAPI == PAPI_ON)
	{
		PAPI_get_info ("MPI_Rsend_init", 0, PAPI_PROCESS);
	}
	Event.starttime = gettime ();
	retVal = PMPI_Rsend_init (buf, count, datatype, dest, tag, comm, request);
	Event.endtime = gettime ();

	if (PAPI == PAPI_ON)
	{
		PAPI_get_info ("MPI_Rsend_init", 1, PAPI_PROCESS);
	}
	Record (&Event, MPI_TRACE);
	return retVal;
}
/*
	标准的数据发送
*/
int MPI_Send (buf, count, datatype, dest, tag, comm)
void *buf;
int count;
MPI_Datatype datatype;
int dest;
int tag;
MPI_Comm comm;
{
	int retVal, typesize;
	Record_Event Event = Event_init ();
	Event.event_name = "MPI_Send";
	Event.eid = 83;
	Event.comm = comm;
	Event.tag = tag;
	PMPI_Comm_rank (comm, &Event.mpi_rank);		/*获取rank号*/
	Event.type = SEND_ONLY;

	Event.sendtype = datatype;
	Event.recvtype = datatype;

	PMPI_Type_size (datatype, &typesize);
	Event.sendsize = typesize * count;
	Event.recvtype = typesize * count;

	Event.src_rank = Event.mpi_rank;
	Event.dst_rank = dest;

	if (PAPI == PAPI_ON)
	{
		PAPI_get_info ("MPI_Send", 0, PAPI_PROCESS);
	}
	Event.starttime = gettime ();
	retVal = PMPI_Send (buf, count, datatype, dest, tag, comm);
	Event.endtime = gettime ();

	if (PAPI == PAPI_ON)
	{
		PAPI_get_info ("MPI_Send", 1, PAPI_PROCESS);
	}
	Record (&Event, MPI_TRACE);
	return retVal;
}
/*
	同时完成发送和接收操作
*/
int MPI_Sendrecv (sendbuf, sendcount, sendtype, dest, sendtag, recvbuf, recvcount, recvtype, source, recvtag, comm, status)
void *sendbuf;
int sendcount;
MPI_Datatype sendtype;
int dest;
int sendtag;
void *recvbuf;
int recvcount;
MPI_Datatype recvtype;
int source;
int recvtag;
MPI_Comm comm;
MPI_Status *status;
{
	int retVal, typesize1, count;
	Record_Event Event = Event_init ();
/*	MPI_Status local_status;*/
	Event.event_name = "MPI_Sendrecv";
	Event.eid = 84;
	Event.comm = comm;
//	Event.tag = tag;
	PMPI_Comm_rank (comm, &Event.mpi_rank);		/*获取rank号*/
	Event.type = SEND_RECV;

	Event.sendtype = sendtype;
	Event.recvtype = recvtype;
	Event.src_rank = source;				/*发送节点标识*/
	Event.dst_rank = dest;					/*接收节点标识*/

	if (dest != MPI_PROC_NULL)
	{
		PMPI_Type_size (sendtype, &typesize1);
		Event.sendsize = typesize1 * sendcount;
	}
	if (PAPI == PAPI_ON)
	{
		PAPI_get_info ("MPI_Sendrecv", 0, PAPI_PROCESS);
	}
	Event.starttime = gettime ();
	retVal = PMPI_Sendrecv (sendbuf, sendcount, sendtype, dest, sendtag, recvbuf, recvcount, recvtype, source, recvtag, comm, status);
	Event.endtime = gettime ();

	if (PAPI == PAPI_ON)
	{
		PAPI_get_info ("MPI_Sendrecv", 1, PAPI_PROCESS);
	}
	if (source != MPI_PROC_NULL && retVal == MPI_SUCCESS)
	{
		PMPI_Get_count (status, MPI_BYTE, &count);
		Event.recvsize = count;
	}
	Record (&Event, MPI_TRACE);
	return retVal;
}
/*
	用同一个发送和接收缓冲区进行发送和接收操作
*/
int MPI_Sendrecv_replace (buf, count, datatype, dest, sendtag, source, recvtag, comm, status)
void *buf;
int count;
MPI_Datatype datatype;
int dest;
int sendtag;
int source;
int recvtag;
MPI_Comm comm;
MPI_Status *status;
{
	int retVal, size1, typesize2;
	Record_Event Event = Event_init ();
	MPI_Status local_status;
	Event.event_name = "MPI_Sendrecv_replace";
	Event.eid = 85;
	Event.comm = comm;
	/*Event.tag = tag;*/  //necessary?
	PMPI_Comm_rank (comm, &Event.mpi_rank);		/*获取rank号*/
	Event.type = SEND_RECV;

	Event.recvtype = datatype;
	Event.sendtype = datatype;
	Event.dst_rank = dest;
	Event.src_rank = source;

	if (dest != MPI_PROC_NULL)
	{
		PMPI_Type_size (datatype, &typesize2);
		Event.sendsize = typesize2*count;
	}
	if (PAPI == PAPI_ON)
	{
		PAPI_get_info ("MPI_Sendrecv_replace", 0, PAPI_PROCESS);
	}
	Event.starttime = gettime ();
	retVal = PMPI_Sendrecv_replace (buf, count, datatype, dest, sendtag, source, recvtag, comm, status);
	Event.endtime = gettime ();
	if (PAPI == PAPI_ON)
	{
		PAPI_get_info ("MPI_Sendrecv_replace", 1, PAPI_PROCESS);
	}

	if (dest != MPI_PROC_NULL&&retVal == MPI_SUCCESS)
	{
		PMPI_Get_count (status, MPI_BYTE, &size1);
		Event.recvsize = size1;
	}
	Record (&Event, MPI_TRACE);
	return retVal;
}
/*
	同步发送
*/
int MPI_Ssend (buf, count, datatype, dest, tag, comm)
void *buf;
int count;
MPI_Datatype datatype;
int dest;
int tag;
MPI_Comm comm;
{
	int retVal, typesize;
	Record_Event Event = Event_init ();
	Event.event_name = "MPI_Ssend";
	Event.eid = 86;
	Event.comm = comm;
	Event.tag = tag;
	PMPI_Comm_rank (comm, &Event.mpi_rank);		/*获取rank号*/
	Event.type = SEND_ONLY;

	Event.sendtype = datatype;
	Event.recvtype = datatype;
	
	Event.dst_rank = dest;
	Event.src_rank = Event.mpi_rank;
	if (dest != MPI_PROC_NULL)
	{
		PMPI_Type_size (datatype, &typesize);
		Event.sendsize = typesize * count;
		Event.recvsize = typesize * count;
	}
	if (PAPI == PAPI_ON)
	{
		PAPI_get_info ("MPI_Ssend", 0, PAPI_PROCESS);
	}
	Event.starttime = gettime ();
	retVal = PMPI_Ssend (buf, count, datatype, dest, tag, comm);
	Event.endtime = gettime ();

	if (PAPI == PAPI_ON)
	{
		PAPI_get_info ("MPI_Ssend", 1, PAPI_PROCESS);
	}
	Record (&Event, MPI_TRACE);
	return retVal;
}
/*
	创建一个同步发送句柄
*/
int MPI_Ssend_init (buf, count, datatype, dest, tag, comm, request)
void *buf;
int count;
MPI_Datatype datatype;
int dest;
int tag;
MPI_Comm comm;
MPI_Request *request;
{
	int retVal;
	Record_Event Event = Event_init ();
	Event.event_name = "MPI_Ssend_init";
	Event.eid = 87;
	Event.comm = comm;
	Event.tag = tag;
	PMPI_Comm_rank (comm, &Event.mpi_rank);		/*获取rank号*/
	Event.type = NONE;

	Event.sendtype = datatype;
	Event.dst_rank = dest;

	if (PAPI == PAPI_ON)
	{
		PAPI_get_info ("MPI_Ssend_init", 0, PAPI_PROCESS);
	}
	Event.starttime = gettime ();
	retVal = PMPI_Ssend_init (buf, count, datatype, dest, tag, comm, request);
	Event.endtime = gettime ();
	if (PAPI == PAPI_ON)
	{
		PAPI_get_info ("MPI_Ssend_init", 1, PAPI_PROCESS);
	}

	Record (&Event, MPI_TRACE);
	return retVal;
}
/*
	启动给定对象上的重复通信请求
*/
int MPI_Start (request)
MPI_Request *request;
{
	int retVal;
	Record_Event Event = Event_init ();
	Event.event_name = "MPI_Start";
	Event.eid = 88;
	Event.type = NONE;

	if (PAPI == PAPI_ON)
	{
		PAPI_get_info ("MPI_Start", 0, PAPI_PROCESS);
	}
	Event.starttime = gettime ();
	retVal = PMPI_Start (request);
	Event.endtime = gettime ();
	if (PAPI == PAPI_ON)
	{
		PAPI_get_info ("MPI_Start", 1, PAPI_PROCESS);
	}

	Record (&Event, MPI_TRACE);
	return retVal;
}
/*
	启动指定的所有重复通信请求
*/
int MPI_Startall (count, array_of_requests)
int count;
MPI_Request *array_of_requests;
{
	int retVal;
	Record_Event Event = Event_init ();
	Event.event_name = "MPI_Startall";
	Event.eid = 89;
	Event.type = NONE;

	if (PAPI == PAPI_ON)
	{
		PAPI_get_info ("MPI_Startall", 0, PAPI_PROCESS);
	}
	Event.starttime = gettime ();
	retVal = PMPI_Startall (count, array_of_requests);
	Event.endtime = gettime ();
	if (PAPI == PAPI_ON)
	{
		PAPI_get_info ("MPI_Startall", 1, PAPI_PROCESS);
	}
	
	Record (&Event, MPI_TRACE);
	return retVal;
}
/*
	测试发送或接收是否完成
*/
int MPI_Test (request, flag, status)
MPI_Request *request;
int *flag;
MPI_Status *status;
{
	int retVal;
	Record_Event Event = Event_init ();
	Event.event_name = "MPI_Test";
	Event.eid = 90;
	Event.type = NONE;

	if (PAPI == PAPI_ON)
	{
		PAPI_get_info ("MPI_Test", 0, PAPI_PROCESS);
	}
	Event.starttime = gettime ();
	retVal = PMPI_Test (request, flag, status);	
	Event.endtime = gettime ();
	if (PAPI == PAPI_ON)
	{
		PAPI_get_info ("MPI_Test", 1, PAPI_PROCESS);
	}
	
	Record (&Event, MPI_TRACE);
	return retVal;
}
/*
	测试前面所有的通信是否完成
*/
int MPI_Testall (count, array_of_requests, flag, array_of_statuses)
int count;
MPI_Request *array_of_requests;
int *flag;
MPI_Status *array_of_statuses;
{
	int retVal;
	Record_Event Event = Event_init ();
	Event.event_name = "MPI_Testall";
	Event.eid = 91;
	Event.type = NONE;

	if (PAPI == PAPI_ON)
	{
		PAPI_get_info ("MPI_Testall", 0, PAPI_PROCESS);
	}
	Event.starttime = gettime ();
	retVal = PMPI_Testall (count, array_of_requests, flag, array_of_statuses);
	Event.endtime = gettime ();
	if (PAPI == PAPI_ON)
	{
		PAPI_get_info ("MPI_Testall", 1, PAPI_PROCESS);
	}

	Record (&Event, MPI_TRACE);
	return retVal;
}
/*
	测试前面任何一个通信是否完成
*/
int MPI_Testany (count, array_of_requests, index, flag, status)
int count;
MPI_Request *array_of_requests;
int *index;
int *flag;
MPI_Status *status;
{
	int retVal;
	Record_Event Event = Event_init ();
	Event.event_name = "MPI_Testany";
	Event.eid = 92;
	Event.type = NONE;

	if (PAPI == PAPI_ON)
	{
		PAPI_get_info ("MPI_Testany", 0, PAPI_PROCESS);
	}
	Event.starttime = gettime ();
	retVal = PMPI_Testany (count, array_of_requests, index, flag, status);
	Event.endtime = gettime ();
	if (PAPI == PAPI_ON)
	{
		PAPI_get_info ("MPI_Testany", 1, PAPI_PROCESS);
	}

	Record (&Event, MPI_TRACE);
	return retVal;
}
/*
	测试一个请求对象是否已经删除
*/
int MPI_Test_cancelled (status, flag)
MPI_Status *status;
int *flag;
{
	int retVal;
	Record_Event Event = Event_init ();
	Event.event_name = "MPI_Test_cancelled";
	Event.eid = 93;
	Event.type = NONE;

	if (PAPI == PAPI_ON)
	{
		PAPI_get_info ("MPI_Test_cancelled", 0, PAPI_PROCESS);
	}
	Event.starttime = gettime ();
	retVal = PMPI_Test_cancelled (status, flag);
	Event.endtime = gettime ();
	if (PAPI == PAPI_ON)
	{
		PAPI_get_info ("MPI_Test_cancelled", 1, PAPI_PROCESS);
	}

	Record (&Event, MPI_TRACE);
	return retVal;
}
/*
	测试是否有一些通信已经完成
*/
int MPI_Testsome (incount, array_of_requests, outcount, array_of_indices, array_of_statuses)
int incount;
MPI_Request *array_of_requests;
int *outcount;
int *array_of_indices;
MPI_Status *array_of_statuses;
{
	int retVal;
	Record_Event Event = Event_init ();
	Event.event_name = "MPI_Testsome";
	Event.eid = 94;
	Event.type = NONE;

	if (PAPI == PAPI_ON)
	{
		PAPI_get_info ("MPI_Testsome", 0, PAPI_PROCESS);
	}
	Event.starttime = gettime ();
	retVal = PMPI_Testsome (incount, array_of_requests, outcount, array_of_indices, array_of_statuses);
	Event.endtime = gettime ();

	if (PAPI == PAPI_ON)
	{
		PAPI_get_info ("MPI_Testsome", 1, PAPI_PROCESS);
	}
	Record (&Event, MPI_TRACE);
	return retVal;
}
/*
	提交一个类型
*/
int MPI_Type_commit (datatype)
MPI_Datatype *datatype;
{
	int retVal;
	Record_Event Event = Event_init ();
	Event.event_name = "MPI_Type_commit";
	Event.eid = 95;
	Event.type = NONE;

	if (PAPI == PAPI_ON)
	{
		PAPI_get_info ("MPI_Type_commit", 0, PAPI_PROCESS);
	}
	Event.starttime = gettime ();
	retVal = PMPI_Type_commit (datatype);
	Event.endtime = gettime ();
	if (PAPI == PAPI_ON)
	{
		PAPI_get_info ("MPI_Type_commit", 1, PAPI_PROCESS);
	}

	Record (&Event, MPI_TRACE);

	return retVal;
}
/*
	创建一个连续的数据类型
*/
int MPI_Type_contiguous (count, old_type, newtype)
int count;
MPI_Datatype old_type;
MPI_Datatype *newtype;
{
	int retVal;
	Record_Event Event = Event_init ();
	Event.event_name = "MPI_Type_contiguous";
	Event.eid = 96;
	Event.type = NONE;

	if (PAPI == PAPI_ON)
	{
		PAPI_get_info ("MPI_Type_contiguous", 0, PAPI_PROCESS);
	}
	Event.starttime = gettime ();
	retVal = PMPI_Type_contiguous (count, old_type, newtype);
	Event.endtime = gettime ();
	if (PAPI == PAPI_ON)
	{
		PAPI_get_info ("MPI_Type_contiguous", 1, PAPI_PROCESS);
	}

	Record (&Event, MPI_TRACE);
	return retVal;
}
/*
	返回一个数据类型的范围
*/
int MPI_Type_extent (datatype, extent)
MPI_Datatype datatype;
MPI_Aint *extent;
{
	int retVal;
	Record_Event Event = Event_init ();
	Event.event_name = "MPI_Type_extent";
	Event.eid = 97;
	Event.type = NONE;

	if (PAPI == PAPI_ON)
	{
		PAPI_get_info ("MPI_Type_extent", 0, PAPI_PROCESS);
	}
	Event.starttime = gettime ();
	retVal = PMPI_Type_extent (datatype, extent);
	Event.endtime = gettime ();
	if (PAPI == PAPI_ON)
	{
		PAPI_get_info ("MPI_Type_extent", 1, PAPI_PROCESS);
	}

	Record (&Event, MPI_TRACE);
	return retVal;
}
/*
	释放一个数据类型
*/
int MPI_Type_free (datatype)
MPI_Datatype *datatype;
{
	int retVal;
	Record_Event Event = Event_init ();
	Event.event_name = "MPI_Type_free";
	Event.eid = 98;
	Event.type = NONE;

	if (PAPI == PAPI_ON)
	{
		PAPI_get_info ("MPI_Type_free", 0, PAPI_PROCESS);
	}
	Event.starttime = gettime ();
	retVal = PMPI_Type_free (datatype);
	Event.endtime = gettime ();
	if (PAPI == PAPI_ON)
	{
		PAPI_get_info ("MPI_Type_free", 1, PAPI_PROCESS);
	}
	
	Record (&Event, MPI_TRACE);
	return retVal;
}
/*
	按照字节偏移, 创建一个数据类型索引
*/
int MPI_Type_hindexed (count, blocklens, indices, old_type, newtype)
int count;
int *blocklens;
MPI_Aint *indices;
MPI_Datatype old_type;
MPI_Datatype *newtype;
{
	int retVal;
	Record_Event Event = Event_init ();
	Event.event_name = "MPI_Type_hindexed";
	Event.eid = 99;
	Event.type = NONE;

	if (PAPI == PAPI_ON)
	{
		PAPI_get_info ("MPI_Type_hindexed", 0, PAPI_PROCESS);
	}
	Event.starttime = gettime ();
	retVal = PMPI_Type_hindexed (count, blocklens, indices, old_type, newtype);
	Event.endtime = gettime ();
	if (PAPI == PAPI_ON)
	{
		PAPI_get_info ("MPI_Type_hindexed", 1, PAPI_PROCESS);
	}
	
	Record (&Event, MPI_TRACE);
	return retVal;
}
/*
	根据以字节为单位的偏移量, 创建一个向量数据类型
*/
int MPI_Type_hvector (count, blocklen, stride, old_type, newtype)
int count;
int blocklen;
MPI_Aint stride;
MPI_Datatype old_type;
MPI_Datatype *newtype;
{
	int retVal;
	Record_Event Event = Event_init ();
	Event.event_name = "MPI_Type_hvector";
	Event.eid = 100;
	Event.type = NONE;

	if (PAPI == PAPI_ON)
	{
		PAPI_get_info ("MPI_Type_hvector", 0, PAPI_PROCESS);
	}
	Event.starttime = gettime (); 
	retVal = PMPI_Type_hvector (count, blocklen, stride, old_type, newtype);
	Event.endtime = gettime ();
	if (PAPI == PAPI_ON)
	{
		PAPI_get_info ("MPI_Type_hvector", 1, PAPI_PROCESS);
	}

	Record (&Event, MPI_TRACE);
	return retVal;
}
/*
	创建一个索引数据类型
*/
int MPI_Type_indexed (count, blocklens, indices, old_type, newtype)
int count;
int *blocklens;
int *indices;
MPI_Datatype old_type;
MPI_Datatype *newtype;
{
	int retVal;
	Record_Event Event = Event_init ();
	Event.event_name = "MPI_Type_indexed";
	Event.eid = 101;
	Event.type = NONE;

	if (PAPI == PAPI_ON)
	{
		PAPI_get_info ("MPI_Type_indexed", 0, PAPI_PROCESS);
	}
	Event.starttime = gettime ();
	retVal = PMPI_Type_indexed (count, blocklens, indices, old_type, newtype);
	Event.endtime = gettime ();
	if (PAPI == PAPI_ON)
	{
		PAPI_get_info ("MPI_Type_indexed", 1, PAPI_PROCESS);
	}

	Record (&Event, MPI_TRACE);
	return retVal;
}
/*
	返回指定数据类型的下边界
*/
int MPI_Type_lb (datatype, displacement)
MPI_Datatype datatype;
MPI_Aint *displacement;
{
	int retVal;
	Record_Event Event = Event_init ();
	Event.event_name = "MPI_Type_lb";
	Event.eid = 102;
	Event.type = NONE;

	if (PAPI == PAPI_ON)
	{
		PAPI_get_info ("MPI_Type_lb", 0, PAPI_PROCESS);
	}
	Event.starttime = gettime ();
	retVal = PMPI_Type_lb (datatype, displacement);
	Event.endtime = gettime ();
	if (PAPI == PAPI_ON)
	{
		PAPI_get_info ("MPI_Type_lb", 1, PAPI_PROCESS);
	}

	Record (&Event, MPI_TRACE);
	return retVal;
}
/*
	以字节为单位, 返回给定数据类型的大小
*/
int MPI_Type_size (datatype, size)
MPI_Datatype datatype;
int *size;
{
	int retVal;
	Record_Event Event = Event_init ();
	Event.event_name = "MPI_Type_size";
	Event.eid = 103;
	Event.type = NONE;

	if (PAPI == PAPI_ON)
	{
		PAPI_get_info ("MPI_Type_size", 0, PAPI_PROCESS);
	}
	Event.starttime = gettime ();
	retVal = PMPI_Type_size (datatype, size);
	Event.endtime = gettime ();
	if (PAPI == PAPI_ON)
	{
		PAPI_get_info ("MPI_Type_size", 1, PAPI_PROCESS);
	}

	Record (&Event, MPI_TRACE);
	return retVal;
}
/*
	创建一个结构数据类型
*/
int MPI_Type_struct (count, blocklens, indices, old_types, newtype)
int count;
int *blocklens;
MPI_Aint *indices;
MPI_Datatype *old_types;
MPI_Datatype *newtype;
{
	int retVal;
	Record_Event Event = Event_init ();
	Event.event_name = "MPI_Type_struct";
	Event.eid = 104;
	Event.type = NONE;

	if (PAPI == PAPI_ON)
	{
		PAPI_get_info ("MPI_Type_struct", 0, PAPI_PROCESS);
	}
	Event.starttime = gettime ();
	retVal = PMPI_Type_struct (count, blocklens, indices, old_types, newtype);
	Event.endtime = gettime ();
	if (PAPI == PAPI_ON)
	{
		PAPI_get_info ("MPI_Type_struct", 1, PAPI_PROCESS);
	}
	
	Record (&Event, MPI_TRACE);
	return retVal;
}
/*
	返回指定数据类型的上边界
*/
int MPI_Type_ub (datatype, displacement)
MPI_Datatype datatype;
MPI_Aint *displacement;
{
	int retVal;
	Record_Event Event = Event_init ();
	Event.event_name = "MPI_Type_ub";
	Event.eid = 105;
	Event.type = NONE;

	if (PAPI == PAPI_ON)
	{
		PAPI_get_info ("MPI_Type_ub", 0, PAPI_PROCESS);
	}
	Event.starttime = gettime ();
	retVal = PMPI_Type_ub (datatype, displacement);
	Event.endtime = gettime ();
	if (PAPI == PAPI_ON)
	{
		PAPI_get_info ("MPI_Type_ub", 1, PAPI_PROCESS);
	}

	Record (&Event, MPI_TRACE);
	return retVal;
}
/*
	创建一个矢量数据类型
*/
int MPI_Type_vector (count, blocklen, stride, old_type, newtype)
int count;
int blocklen;
int stride;
MPI_Datatype old_type;
MPI_Datatype *newtype;
{
	int retVal;
	Record_Event Event = Event_init ();
	Event.event_name = "MPI_Type_vector";
	Event.eid = 106;
	Event.type = NONE;

	if (PAPI == PAPI_ON)
	{
		PAPI_get_info ("MPI_Type_vector", 0, PAPI_PROCESS);
	}
	Event.starttime = gettime ();
	retVal = PMPI_Type_vector (count, blocklen, stride, old_type, newtype);
	Event.endtime = gettime ();

	if (PAPI == PAPI_ON)
	{
		PAPI_get_info ("MPI_Type_vector", 1, PAPI_PROCESS);
	}
	Record (&Event, MPI_TRACE);
	return retVal;
}
/*
	从连续的缓冲区中将数据解开
*/
int MPI_Unpack (inbuf, insize, position, outbuf, outcount, type, comm)
void *inbuf;
int insize;
int *position;
void *outbuf;
int outcount;
MPI_Datatype type;
MPI_Comm comm;
{
	int retVal;
	Record_Event Event = Event_init ();
	Event.event_name = "MPI_Unpack";
	Event.eid = 107;
	Event.type = NONE;

	if (PAPI == PAPI_ON)
	{
		PAPI_get_info ("MPI_Unpack", 0, PAPI_PROCESS);
	}
	Event.starttime = gettime ();
	retVal = PMPI_Unpack (inbuf, insize, position, outbuf, outcount, type, comm);
	Event.endtime = gettime ();

	if (PAPI == PAPI_ON)
	{
		PAPI_get_info ("MPI_Unpack", 1, PAPI_PROCESS);
	}
	Record (&Event, MPI_TRACE);
	return retVal;
}
/*
	等待 MPI 的发送或接收语句结束
*/
int MPI_Wait (request, status)
MPI_Request *request;
MPI_Status *status;
{
	int retVal;
	Record_Event Event = Event_init ();
	Event.event_name = "MPI_Wait";
	Event.eid = 108;
	Event.type = NONE;

	if (PAPI == PAPI_ON)
	{
		PAPI_get_info ("MPI_Wait", 0, PAPI_PROCESS);
	}
	Event.starttime = gettime ();
	retVal = PMPI_Wait (request, status);
	Event.endtime = gettime ();
	if (PAPI == PAPI_ON)
	{
		PAPI_get_info ("MPI_Wait", 1, PAPI_PROCESS);
	}

	Record (&Event, MPI_TRACE);
	return retVal;
}
/*
	等待所有给定的通信结束
*/
int MPI_Waitall (count, array_of_requests, array_of_statuses)
int count;
MPI_Request * array_of_requests;
MPI_Status * array_of_statuses;
{
	int retVal;
	Record_Event Event = Event_init ();
	Event.event_name = "MPI_Waitall";
	Event.eid = 109;
	Event.type = NONE;

	if (PAPI == PAPI_ON)
	{
		PAPI_get_info ("MPI_Waitall", 0, PAPI_PROCESS);
	}
	Event.starttime = gettime ();
	retVal = PMPI_Waitall (count, array_of_requests, array_of_statuses);
	Event.endtime = gettime ();
	if (PAPI == PAPI_ON)
	{
		PAPI_get_info ("MPI_Waitall", 1, PAPI_PROCESS);
	}

	Record (&Event, MPI_TRACE);
	return retVal;
}
/*
	等待某些指定的发送或接收完成
*/
int MPI_Waitany (count, array_of_requests, index, status)
int count;
MPI_Request *array_of_requests;
int *index;
MPI_Status *status;
{
	int retVal;
	Record_Event Event = Event_init ();
	Event.event_name = "MPI_Waitany";
	Event.eid = 110;
	Event.type = NONE;

	if (PAPI == PAPI_ON)
	{
		PAPI_get_info ("MPI_Waitany", 0, PAPI_PROCESS);
	}
	Event.starttime = gettime ();
	retVal = PMPI_Waitany (count, array_of_requests, index, status);
	Event.endtime = gettime ();
	if (PAPI == PAPI_ON)
	{
		PAPI_get_info ("MPI_Waitany", 1, PAPI_PROCESS);
	}

	Record (&Event, MPI_TRACE);
	return retVal;
}
/*
	等待一些给定的通信结束
*/
int MPI_Waitsome (incount, array_of_requests, outcount, array_of_indices, array_of_statuses)
int incount;
MPI_Request *array_of_requests;
int *outcount;
int *array_of_indices;
MPI_Status *array_of_statuses;
{
	int retVal;
	Record_Event Event = Event_init ();
	Event.event_name = "MPI_Waitsome";
	Event.eid = 111;
	Event.type = NONE;

	if (PAPI == PAPI_ON)
	{
		PAPI_get_info ("MPI_Waitsome", 0, PAPI_PROCESS);
	}
	Event.starttime = gettime ();
	retVal = PMPI_Waitsome (incount, array_of_requests, outcount, array_of_indices, array_of_statuses);
	Event.endtime = gettime ();
	if (PAPI == PAPI_ON)
	{
		PAPI_get_info ("MPI_Waitsome", 1, PAPI_PROCESS);
	}

	Record (&Event, MPI_TRACE);
	return retVal;
}
/*
	给出一个进程所在组的标识号, 得到其卡氏坐标值
*/
int MPI_Cart_coords (comm, rank, maxdims, coords)
MPI_Comm comm;
int rank;
int maxdims;
int *coords;
{
	int retVal;
	Record_Event Event = Event_init ();
	Event.event_name = "MPI_Cart_coords";
	Event.eid = 112;
	Event.comm = comm;
	Event.type = NONE;

	if (PAPI == PAPI_ON)
	{
		PAPI_get_info ("MPI_Cart_coords", 0, PAPI_PROCESS);
	}
	Event.starttime = gettime ();
	retVal = PMPI_Cart_coords (comm, rank, maxdims, coords);
	Event.endtime = gettime ();
	if (PAPI == PAPI_ON)
	{
		PAPI_get_info ("MPI_Cart_coords", 1, PAPI_PROCESS);
	}
	Record (&Event, MPI_TRACE);
	return retVal;
}
/*
	按给定的拓扑创建一个新的通信域
*/
int MPI_Cart_create (comm_old, ndims, dims, periods, reorder, comm_cart)
MPI_Comm comm_old;
int ndims;
int *dims;
int *periods;
int reorder;
MPI_Comm *comm_cart;
{
	int retVal;
	Record_Event Event = Event_init ();
	Event.event_name = "MPI_Cart_create";
	Event.eid = 113;
	Event.type = NONE;

	if (PAPI == PAPI_ON)
	{
		PAPI_get_info ("MPI_Cart_create", 0, PAPI_PROCESS);
	}
	Event.starttime = gettime ();
	retVal = PMPI_Cart_create (comm_old, ndims, dims, periods, reorder, comm_cart);
	Event.endtime = gettime ();
	if (PAPI == PAPI_ON)
	{
		PAPI_get_info ("MPI_Cart_create", 1, PAPI_PROCESS);
	}

	Record (&Event, MPI_TRACE);
	return retVal;
}
/*
	得到给定通信域的卡氏拓扑信息
*/
int MPI_Cart_get (comm, maxdims, dims, periods, coords)
MPI_Comm comm;
int maxdims;
int *dims;
int *periods;
int *coords;
{
	int retVal;
	Record_Event Event = Event_init ();
	Event.event_name = "MPI_Cart_get";
	Event.eid = 114;
	Event.comm = comm;
	PMPI_Comm_rank (comm, &Event.mpi_rank);		/*获取rank号*/
	Event.type = NONE;

	if (PAPI == PAPI_ON)
	{
		PAPI_get_info ("MPI_Cart_get", 0, PAPI_PROCESS);
	}
	Event.starttime = gettime ();
	retVal = PMPI_Cart_get (comm, maxdims, dims, periods, coords);
	Event.endtime = gettime ();
	if (PAPI == PAPI_ON)
	{
		PAPI_get_info ("MPI_Cart_get", 1, PAPI_PROCESS);
	}

	Record (&Event, MPI_TRACE);
	return retVal;
}
/*
	将进程标识号映射为卡氏拓扑坐标
*/
int MPI_Cart_map (comm_old, ndims, dims, periods, newrank)
MPI_Comm comm_old;
int ndims;
int *dims;
int *periods;
int *newrank;
{
	int retVal;
	Record_Event Event = Event_init ();
	Event.event_name = "MPI_Cart_map";
	Event.eid = 115;
	Event.type = NONE;

	if (PAPI == PAPI_ON)
	{
		PAPI_get_info ("MPI_Cart_map", 0, PAPI_PROCESS);
	}
	Event.starttime = gettime ();
	retVal  =  PMPI_Cart_map (comm_old, ndims, dims, periods, newrank);
	Event.endtime = gettime ();
	if (PAPI == PAPI_ON)
	{
		PAPI_get_info ("MPI_Cart_map", 1, PAPI_PROCESS);
	}

	Record (&Event, MPI_TRACE);
	return retVal;
}
/*
	由进程标识号得到卡氏坐标
*/
int MPI_Cart_rank (comm, coords, rank)
MPI_Comm comm;
int *coords;
int *rank;
{
	int retVal;
	Record_Event Event = Event_init ();
	Event.event_name = "MPI_Cart_rank";
	Event.eid = 116;
	Event.comm = comm;
	PMPI_Comm_rank (comm, &Event.mpi_rank);		/*获取rank号*/
	Event.type = NONE;

	if (PAPI == PAPI_ON)
	{
		PAPI_get_info ("MPI_Cart_rank", 0, PAPI_PROCESS);
	}
	Event.starttime = gettime ();
	retVal = PMPI_Cart_rank (comm, coords, rank);
	Event.endtime = gettime ();
	if (PAPI == PAPI_ON)
	{
		PAPI_get_info ("MPI_Cart_rank", 1, PAPI_PROCESS);
	}

	Record (&Event, MPI_TRACE);
	return retVal;
}
/*
	给定进程标识号、平移方向与大小, 得到相对于当前进程的源和目的进程的标识号
*/
int MPI_Cart_shift (comm, direction, displ, source, dest)
MPI_Comm comm;
int direction;
int displ;
int *source;
int *dest;
{
	int retVal;
	Record_Event Event = Event_init ();
	Event.event_name = "MPI_Cart_shift";
	Event.eid = 117;
	Event.comm = comm;
	PMPI_Comm_rank (comm, &Event.mpi_rank);		/*获取rank号*/
	Event.type = NONE;

	if (PAPI == PAPI_ON)
	{
		PAPI_get_info ("MPI_Cart_shift", 0, PAPI_PROCESS);
	}
	Event.starttime = gettime ();
	retVal = PMPI_Cart_shift (comm, direction, displ, source, dest);
	Event.endtime = gettime ();
	if (PAPI == PAPI_ON)
	{
		PAPI_get_info ("MPI_Cart_shift", 1, PAPI_PROCESS);
	}

	Record (&Event, MPI_TRACE);
	return retVal;
}
/*
	将一个通信域, 保留给定的维, 得到子通信域
*/
int MPI_Cart_sub (comm, remain_dims, comm_new)
MPI_Comm comm;
int *remain_dims;
MPI_Comm *comm_new;
{
	int retVal;
	Record_Event Event = Event_init ();
	Event.event_name = "MPI_Cart_sub";
	Event.eid = 118;
	Event.comm = comm;
	PMPI_Comm_rank (comm, &Event.mpi_rank);		/*获取rank号*/
	Event.type = NONE;

	if (PAPI == PAPI_ON)
	{
		PAPI_get_info ("MPI_Cart_sub", 0, PAPI_PROCESS);
	}
	Event.starttime = gettime ();
	retVal = PMPI_Cart_sub (comm, remain_dims, comm_new);
	Event.endtime = gettime ();
	if (PAPI == PAPI_ON)
	{
		PAPI_get_info ("MPI_Cart_sub", 1, PAPI_PROCESS);
	}

	Record (&Event, MPI_TRACE);
	return retVal;
}
/*
	得到给定通信域的卡氏拓扑
*/
int MPI_Cartdim_get (comm, ndims)
MPI_Comm comm;
int *ndims;
{
	int retVal;
	Record_Event Event = Event_init ();
	Event.event_name = "MPI_Cartdim_get";
	Event.eid = 119;
	Event.comm = comm;
	PMPI_Comm_rank (comm, &Event.mpi_rank);		/*获取rank号*/
	Event.type = NONE;

	if (PAPI == PAPI_ON)
	{
		PAPI_get_info ("MPI_Cartdim_get", 0, PAPI_PROCESS);
	}

	Event.starttime = gettime ();
	retVal = PMPI_Cartdim_get (comm, ndims);
	Event.endtime = gettime ();
	if (PAPI == PAPI_ON)
	{
		PAPI_get_info ("MPI_Cartdim_get", 1, PAPI_PROCESS);
	}

	Record (&Event, MPI_TRACE);
	return retVal;
}
/*
	在卡氏网格中建立进程维的划分
*/
int MPI_Dims_create (nnodes, ndims, dims)
int nnodes;
int ndims;
int *dims;
{
	int retVal;
	Record_Event Event = Event_init ();
	Event.event_name = "MPI_Dims_create";
	Event.eid = 120;
	Event.type = NONE;

	if (PAPI == PAPI_ON)
	{
		PAPI_get_info ("MPI_Dims_create", 0, PAPI_PROCESS);
	}
	Event.starttime = gettime ();
	retVal = PMPI_Dims_create (nnodes, ndims, dims);
	Event.endtime = gettime ();
	if (PAPI == PAPI_ON)
	{
		PAPI_get_info ("MPI_Dims_create", 1, PAPI_PROCESS);
	}

	Record (&Event, MPI_TRACE);
	return retVal;
}
/*
	按照给定的拓扑创建新的通信域
*/
int MPI_Graph_create (comm_old, nnodes, index, edges, reorder, comm_graph)
MPI_Comm comm_old;
int nnodes;
int *index;
int *edges;
int reorder;
MPI_Comm *comm_graph;
{
	int retVal;
	Record_Event Event = Event_init ();
	Event.event_name = "MPI_Graph_create";
	Event.eid = 121;
	Event.type = NONE;

	if (PAPI == PAPI_ON)
	{
		PAPI_get_info ("MPI_Graph_create", 0, PAPI_PROCESS);
	}
	Event.starttime = gettime ();
	retVal = PMPI_Graph_create (comm_old, nnodes, index, edges, reorder, comm_graph);
	Event.endtime = gettime ();

	if (PAPI == PAPI_ON)
	{
		PAPI_get_info ("MPI_Graph_create", 1, PAPI_PROCESS);
	}
	Record (&Event, MPI_TRACE);
	return retVal;
}
/*
	得到给定通信域的处理器拓扑结构
*/
int MPI_Graph_get (comm, maxindex, maxedges, index, edges)
MPI_Comm comm;
int maxindex;
int maxedges;
int *index;
int *edges;
{
	int retVal;
	Record_Event Event = Event_init ();
	Event.event_name = "MPI_Graph_get";
	Event.eid = 122;
	Event.comm = comm;
	PMPI_Comm_rank (comm, &Event.mpi_rank);		/*获取rank号*/
	Event.type = NONE;

	if (PAPI == PAPI_ON)
	{
		PAPI_get_info ("MPI_Graph_get", 0, PAPI_PROCESS);
	}
	Event.starttime = gettime ();
	retVal = PMPI_Graph_get (comm, maxindex, maxedges, index, edges);
	Event.endtime = gettime ();
	if (PAPI == PAPI_ON)
	{
		PAPI_get_info ("MPI_Graph_get", 1, PAPI_PROCESS);
	}
	
	Record (&Event, MPI_TRACE);
	return retVal;
}
/*
	将进程映射到给定的拓扑
*/
int MPI_Graph_map (comm_old, nnodes, index, edges, newrank)
MPI_Comm comm_old;
int nnodes;
int *index;
int *edges;
int *newrank;
{
	int retVal;
	Record_Event Event = Event_init ();
	Event.event_name = "MPI_Graph_map";
	Event.eid = 123;
	Event.type = NONE;

	if (PAPI == PAPI_ON)
	{
		PAPI_get_info ("MPI_Graph_map", 0, PAPI_PROCESS);
	}
	Event.starttime = gettime ();
	retVal = PMPI_Graph_map (comm_old, nnodes, index, edges, newrank);
	Event.endtime = gettime ();
	if (PAPI == PAPI_ON)
	{
		PAPI_get_info ("MPI_Graph_map", 1, PAPI_PROCESS);
	}

	Record (&Event, MPI_TRACE);
	return retVal;
}
/*
	给定拓扑, 返回给定结点的相邻结点
*/
int MPI_Graph_neighbors (comm, rank, maxneighbors, neighbors)
MPI_Comm comm;
int rank;
int maxneighbors;
int *neighbors;
{
	int retVal;
	Record_Event Event = Event_init ();
	Event.event_name = "MPI_Graph_neighbors";
	Event.eid = 124;
	Event.comm = comm;
	PMPI_Comm_rank (comm, &Event.mpi_rank);		/*获取rank号*/
	Event.type = NONE;

	if (PAPI == PAPI_ON)
	{
		PAPI_get_info ("MPI_Graph_neighbors", 0, PAPI_PROCESS);
	}
	Event.starttime = gettime ();
	retVal = PMPI_Graph_neighbors (comm, rank, maxneighbors, neighbors);
	Event.endtime = gettime ();
	if (PAPI == PAPI_ON)
	{
		PAPI_get_info ("MPI_Graph_neighbors", 1, PAPI_PROCESS);
	}
	
	Record (&Event, MPI_TRACE);
	return retVal;
}
/*
	给定拓扑, 返回给定结点的相邻结点数
*/
int MPI_Graph_neighbors_count (comm, rank, nneighbors)
MPI_Comm comm;
int rank;
int *nneighbors;
{
	int retVal;
	Record_Event Event = Event_init ();
	Event.event_name = "MPI_Graph_neighbors_count";
	Event.eid = 125;
	Event.comm = comm;
	PMPI_Comm_rank (comm, &Event.mpi_rank);		/*获取rank号*/
	Event.type = NONE;

	if (PAPI == PAPI_ON)
	{
		PAPI_get_info ("MPI_Graph_neighbors_count", 0, PAPI_PROCESS);
	}
	Event.starttime = gettime ();
	retVal = PMPI_Graph_neighbors_count (comm, rank, nneighbors);
	Event.endtime = gettime ();
	if (PAPI == PAPI_ON)
	{
		PAPI_get_info ("MPI_Graph_neighbors_count", 1, PAPI_PROCESS);
	}

	Record (&Event, MPI_TRACE);
	return retVal;
}
/*
	得到给定通信域的图拓扑
*/
int MPI_Graphdims_get (comm, nnodes, nedges)
MPI_Comm comm;
int *nnodes;
int *nedges;
{
	int retVal;
	Record_Event Event = Event_init ();
	Event.event_name = "MPI_Graphdims_get";
	Event.eid = 126;
	Event.comm = comm;
	PMPI_Comm_rank (comm, &Event.mpi_rank);		/*获取rank号*/
	Event.type = NONE;

	if (PAPI == PAPI_ON)
	{
		PAPI_get_info ("MPI_Graphdims_get", 0, PAPI_PROCESS);
	}
	Event.starttime = gettime ();
	retVal = PMPI_Graphdims_get (comm, nnodes, nedges);
	Event.endtime = gettime ();
	if (PAPI == PAPI_ON)
	{
		PAPI_get_info ("MPI_Graphdims_get", 1, PAPI_PROCESS);
	}
	
	Record (&Event, MPI_TRACE);
	return retVal;
}
/*
	测试指定通信域的拓扑类型
*/
int MPI_Topo_test (comm, top_type)
MPI_Comm comm;
int *top_type;
{
	int retVal;
	Record_Event Event = Event_init ();
	Event.event_name = "MPI_Topo_test";
	Event.eid = 127;
	Event.comm = comm;
	PMPI_Comm_rank (comm, &Event.mpi_rank);		/*获取rank号*/
	Event.type = NONE;

	if (PAPI == PAPI_ON)
	{
		PAPI_get_info ("MPI_Topo_test", 0, PAPI_PROCESS);
	}
	Event.starttime = gettime ();
	retVal = PMPI_Topo_test (comm, top_type);
	Event.endtime = gettime ();
	if (PAPI == PAPI_ON)
	{
		PAPI_get_info ("MPI_Topo_test", 1, PAPI_PROCESS);
	}

	Record (&Event, MPI_TRACE);
	return retVal;
}
