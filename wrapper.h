#include <stdio.h>
#include <string.h>
#include <sys/time.h>
#include <sys/syscall.h>
#include <mpi.h>
#include <IO.h>
#include "papi.h"

#ifndef WRAPPER_H
#define WRAPPER_H
#endif


//#define DEBUG

/*包装类型*/
#define OMP_TRACE	1
#define MPI_TRACE	2
#define HYBRID		3
#define OMPI_TRACE	4
#define CMPI_TRACE	5
#define IO_TRACE 	6

/*MPI通信类型*/
#define NONE 		0						/*没有通信*/						
#define SEND_ONLY 	1						/*只发送*/
#define RECV_ONLY	2						/*只接收*/
#define SEND_RECV	3						/*既发送又接收*/
#define BCAST		4						/*广播*/
#define REDUCE		5						/*归约*/
#define CUDA_MEM	6
#define CUDA_KER	7
//long unsigned Local_offset=0;

/*task behavior*/
#define TASK_CREATE	 	0
#define TASK_START  	1
#define TASK_SUSPEND	2
#define TASK_RESUME		3
#define TASK_RUNUING	4
#define TASK_WAIT		5
#define TASK_END 		6

/*PAPI Events*/

#define THRESHOLD 100000
#define PAPI_ERROR_RETURN(retval) { fprintf_d (stderr, "[PAPI]Error %d %s:line %d: \n", retval, __FILE__,__LINE__);  exit(retval); }

#define PAPI_ON 1
#define PAPI_OFF 2
#define PAPI_MAX_EVENT_NUM 30
#define PAPI_MAX_EVENT_LEN 20
#define PAPI_PROCESS 1
#define PAPI_THREAD 2

#define gettid() syscall(__NR_gettid)		/*获得线程号*/

typedef struct Record_Event_
{
	char *event_name;						/*事件名称*/
	int eid;								/*事件编号*/
	MPI_Comm comm;							/*通信域*/
	int mpi_rank;
	int omp_rank;
	int omp_level;

	int p_rank;								/*omp父节点号*/

	int tid;								/*线程号*/
	unsigned pid;							/*进程号*/
	char hostname[20];						/*主机名*/
	int type;								/*MPI通信类型*/

	long long unsigned starttime;			/*开始时间戳*/
	long long unsigned endtime;				/*结束时间戳*/

	/*mpi data 数据类型*/				
	int tag;								/*消息标识符*/
	int recvtype;							/*接收消息类型*/
	int sendtype;							/*发送消息类型*/
	int sendsize;							/*发送消息大小*/
	int recvsize;							/*接收消息大小*/
	int src_rank;							/*发送节点*/
	int dst_rank;							/*接收节点*/

	/*omp task 数据类型*/
	int64_t task_id_start;					/*开始任务id*/
	int64_t task_id_end;					/*结束任务id*/
	int64_t p_task_id_start;				/*开始父任务id*/			
	int64_t p_task_id_end;					/*结束父任务id*/
	int task_state_start;
	int task_state_end;

	/*cuda data 数据类型*/
	int cuda_stream;						/*CUDA 流*/
	int memcpy_size;						/*数据复制大小*/
	int memcpy_type;						/*数据复制方向*/
	int memcpy_kind;						/*数据复制类型*/
}Record_Event;

/*MPI节点信息结构*/
typedef struct MPI_node_info_ 
{
	unsigned pid;
	int mpi_rank;
	char procname[MPI_MAX_PROCESSOR_NAME];
	int procnamelength;
	int size;
}MPI_node_info;

/*CUPTI信息结构*/
typedef struct CUPTI_Event_
{
	unsigned pid;
	char hostname[20];
	int device_id;
	int context_id;
	int stream_id;
	int type;
	char *name;
	char *seg1;
	char *seg2;
	char *seg3;
	long long unsigned time;
	int finish;
//	long long unsigned starttime;
//	long long unsigned endtime;
}CUPTI_Event;

/*PAPI信息结构*/
typedef struct  PAPI_Event_
{	
	char *event_name;
	unsigned pid;
	int tid;
	char hostname[20];
	char *papi_event;
	long long data;
	long long unsigned time;
	int finish;
}PAPI_Event;

typedef struct IO_Event_
{
	char *event_name;
	unsigned pid;
	int tid;
	char hostname [20];
	char data[20];
	long long unsigned start_time;
	long long unsigned end_time;
}IO_Event;
