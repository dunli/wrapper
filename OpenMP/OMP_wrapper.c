#define _GNU_SOURCE

#include <dlfcn.h>
#include <pthread.h>
#include <stdbool.h>
#include <unistd.h>

#include "wrapper.h"

#define TASK_MAX_NUM 100
#define TASK_MAX_LEVEL	100

typedef struct TaskInfo_
{
	// an id that can identify this thread 
	int thread_id;
	int64_t task_id;
	int64_t task_parent_id;
}TaskInfo;

TaskInfo task_list [TASK_MAX_NUM];
int task_num = 0;

static __thread int64_t parent_task_id = -1;
static __thread int64_t current_task_id = -1;
static __thread int32_t task_count = 0;
static __thread int64_t old_task_id [TASK_MAX_NUM];
static int64_t task_level [TASK_MAX_LEVEL];
static int current_level = 0;
static __thread int flag = 0;

static int pardo_uf_id = 0;								//parallel for 用户子函数编号
static int par_uf_id = 0;								//parallel 用户子函数编号
static int task_uf_id = 0;								//task 用户子函数编号

static void (*GOMP_parallel_start_real)(void*,void*,unsigned) = NULL;
static void (*GOMP_parallel_end_real)(void) = NULL;
static void (*GOMP_barrier_real)(void) = NULL;
static void (*GOMP_critical_name_start_real)(void**) = NULL;
static void (*GOMP_critical_name_end_real)(void**) = NULL;
static void (*GOMP_critical_start_real)(void) = NULL;
static void (*GOMP_critical_end_real)(void) = NULL;
static void (*GOMP_atomic_start_real)(void) = NULL;
static void (*GOMP_atomic_end_real)(void) = NULL;

static void (*GOMP_parallel_loop_static_start_real)(void*,void*,unsigned, long, long, long, long) = NULL;
static void (*GOMP_parallel_loop_runtime_start_real)(void*,void*,unsigned, long, long, long, long) = NULL;
static void (*GOMP_parallel_loop_dynamic_start_real)(void*,void*,unsigned, long, long, long, long) = NULL;
static void (*GOMP_parallel_loop_guided_start_real)(void*,void*,unsigned, long, long, long, long) = NULL;

static int (*GOMP_loop_static_next_real)(long*,long*) = NULL;
static int (*GOMP_loop_runtime_next_real)(long*,long*) = NULL;
static int (*GOMP_loop_dynamic_next_real)(long*,long*) = NULL;
static int (*GOMP_loop_guided_next_real)(long*,long*) = NULL;

static int (*GOMP_loop_static_start_real)(long,long,long,long,long*,long*) = NULL;
static int (*GOMP_loop_runtime_start_real)(long,long,long,long,long*,long*) = NULL;
static int (*GOMP_loop_guided_start_real)(long,long,long,long,long*,long*) = NULL;
static int (*GOMP_loop_dynamic_start_real)(long,long,long,long,long*,long*) = NULL;

static void (*GOMP_loop_end_real)(void) = NULL;
static void (*GOMP_loop_end_nowait_real)(void) = NULL;

static unsigned (*GOMP_sections_start_real)(unsigned) = NULL;
static unsigned (*GOMP_sections_next_real)(void) = NULL;
static void (*GOMP_sections_end_real)(void) = NULL;
static void (*GOMP_sections_end_nowait_real)(void) = NULL;
static void (*GOMP_parallel_sections_start_real)(void*,void*,unsigned,unsigned) = NULL;

static void (*GOMP_taskwait_real)(void)=NULL;
static void (*GOMP_task_real)(void *,void *,void *,long,long,_Bool,unsigned)=NULL;

static void (*omp_set_lock_real)(int *) = NULL;
static void (*omp_unset_lock_real)(int *) = NULL;
static void (*omp_set_num_threads_real)(int) = NULL;

static void (*pardo_uf)(void*) = NULL;
static void (*par_uf)(void*) = NULL;
static void (*task_uf)(void*) = NULL;

extern int Record ();
extern Record_Event Event_init ();
extern int get_thread_num ();
//extern int get_thread_num();
extern int get_level ();
extern long long unsigned gettime ();

extern int PAPI_get_info (char *, int, int);
extern int PAPI_info_record (PAPI_Event);
extern int NUM_EVENT;
extern char PAPI_EVENT[PAPI_MAX_EVENT_NUM][PAPI_MAX_EVENT_LEN];
extern int PAPI;


void itoa(int value,char buf [])
{	
	int i, j;
	char temp [10];

	if (value == 0)
	{
		buf [0] = '0';
		buf [1] = '\0';
		return ;
	}
	else
	{
		for (i = 0; value != 0; ++i)
		{
			temp [i] = '0' + value % 10;
			value = value / 10;
		}
		--i;
		for (j = 0; i >= 0; ++j, --i)
			buf [j] = temp[i];
		buf [j] = '\0';
	}
}

int get_thread_id (int level)
{
	int thread_id = 0;

	if (level < 0)
		return -1;
	
	while (level > 0)
		thread_id = thread_id * 10 + omp_get_ancestor_thread_num (level--);

	return thread_id;
}

//allocate a new task id for the current task, this task id is related to the thread num and the thread level
int64_t get_new_task_id ()
{
	int64_t thread_num = gettid ();
	int64_t level = get_level ();
	int64_t new_task_id;

	new_task_id = (thread_num << 48) + (level << 32) + task_count ++;
	return new_task_id;
}

int64_t get_task_id (int thread_id)
{
	int i;
	for (i = task_num; i >= 0; --i)
	{
		if (task_list[i].thread_id == thread_id)
			return task_list [i].task_id;
	}

	return -1;
}

TaskInfo get_new_task ()
{
	TaskInfo task;
	task.thread_id = get_thread_id (get_level ());
	task.task_id = get_new_task_id ();
	task.task_parent_id = get_task_id (get_thread_id (get_level () - 1));

	return task;
}

/**************************************************    User function     ************************************************/
/*
	指导语句:	#pragma omp parallel for
	结构功能:	for用户子函数
	函数功能:	for指导语句中调用的用户子函数
*/
static void callme_pardo(void *p1)
{
//	int64_t old_task_id_temp;
	char fun_name[30] = "Parallel_User_do_fun_";
	char id [10];
	TaskInfo current_task;

	Record_Event Event = Event_init ();

	current_task = get_new_task ();
	task_list [task_num++] = current_task;
	
	itoa (pardo_uf_id, id);
	strcat (fun_name, id);
	Event.event_name = fun_name;
	Event.eid = 233;
	Event.type = NONE;

	Event.omp_rank = get_thread_num ();
	Event.omp_level = get_level ();
	Event.p_rank = omp_get_ancestor_thread_num (get_level () - 1);

	Event.p_task_id_start = current_task.task_parent_id;
	Event.task_id_start = current_task.task_id;
	Event.task_state_start = TASK_CREATE;

	if (pardo_uf == NULL)
	{
		printf_d("Error! Invalid initialization of 'pardo_uf'\n");
		return ;
	}
	if (PAPI == PAPI_ON)
	{
		PAPI_get_info (fun_name, 0, PAPI_THREAD);
		Event.starttime = gettime ();
		pardo_uf (p1);
		Event.endtime = gettime ();
		PAPI_get_info (fun_name, 1, PAPI_THREAD);
	}
	else 
	{
		Event.starttime = gettime ();
		pardo_uf (p1);
		Event.endtime = gettime ();
	}


	Event.p_task_id_end = current_task.task_parent_id;
	Event.task_id_end = current_task.task_id;
	Event.task_state_end = TASK_END;
	
	Record (&Event, OMPI_TRACE);
}
/*
	指导语句:	#pragma omp parallel
	结构功能:	parallel用户子函数
	函数功能:	parallel中调用的用户子函数
*/
static void callme_par (void *p1)
{
//	int64_t old_task_id_temp;
	TaskInfo current_task;
	char fun_name[30] = "Parallel_User_fun_";
	char id [10];
	Record_Event Event = Event_init ();

	current_task = get_new_task ();
	task_list [task_num++] = current_task;
	
	itoa (par_uf_id, id);
	strcat (fun_name, id);
	Event.event_name = fun_name;
	Event.eid = 234;
	Event.type = NONE;

	Event.omp_rank = get_thread_num ();
	Event.omp_level = get_level ();
	Event.p_rank = omp_get_ancestor_thread_num (get_level () - 1);

	Event.p_task_id_start = current_task.task_parent_id;
	Event.task_id_start = current_task.task_id;
	Event.task_state_start = TASK_CREATE;

	if (par_uf == NULL)
	{
		printf_d("Error! Invalid initialization of 'par_uf'\n");
		return ;
	}

	if (PAPI == PAPI_ON)
	{
		PAPI_get_info (fun_name, 0, PAPI_THREAD);
		Event.starttime = gettime ();
		par_uf (p1);
		Event.endtime = gettime ();
		PAPI_get_info (fun_name, 1, PAPI_THREAD);
	}
	else 
	{
		Event.starttime = gettime ();
		par_uf (p1);
		Event.endtime = gettime ();
	}

	Event.p_task_id_end = current_task.task_parent_id;
	Event.task_id_end = current_task.task_id;
	Event.task_state_end = TASK_END;
	
	Record (&Event, OMPI_TRACE);
}
/*
	指导语句:	#pragma omp task
	结构功能:	task用户子函数
	函数功能:	task中调用的用户子函数
*/
static void callme_task (void *p1)
{
	TaskInfo current_task;
	char fun_name[30] = "Task_User_do_fun_";
	char id [10];

	Record_Event Event = Event_init ();

	current_task = get_new_task ();
	task_list [task_num++]

	itoa (task_uf_id, id);
	strcat (fun_name, id);
	Event.event_name = fun_name;
	Event.eid = 235;
	Event.type = NONE;

	Event.omp_rank = get_thread_num ();
	Event.omp_level = get_level ();
	Event.p_rank = omp_get_ancestor_thread_num (get_level () - 1);

	Event.p_task_id_start = current_task.task_parent_id;
	Event.task_id_start = current_task.task_id;
	Event.task_state_start = TASK_CREATE;
	if (task_uf == NULL)	
	{
		printf_d("Error! Invalid initialization of 'task_uf'\n");
		return ;
	}
	if (PAPI == PAPI_ON)
	{
		PAPI_get_info (fun_name, 0, PAPI_THREAD);
		Event.starttime = gettime ();`
		task_uf (p1);
		Event.endtime = gettime ();
		PAPI_get_info (fun_name, 1, PAPI_THREAD);
	}
	else 
	{
		Event.starttime = gettime ();
		task_uf (p1);
		Event.endtime = gettime ();
	}


	Event.p_task_id_end = current_task.task_parent_id;
	Event.task_id_end = current_task.task_id;
	Event.task_state_end = TASK_END;

	Record (&Event, OMPI_TRACE);
}

/**************************************************    Parallel     ************************************************/

/*
	指导语句:	#pragma omp parallel
	结构功能:	parallel开始函数
	函数功能:	初始化一个parallel并行结构
*/
void GOMP_parallel_start (void *p1, void *p2, unsigned p3)
{
	TaskInfo current_task;
	int current_level;
	Record_Event Event = Event_init ();										//初始化
	Event.event_name = "GOMP_parallel_start";								//获取函数名
	Event.eid = 200;
	Event.type = NONE;
	Event.omp_rank = get_thread_num ();										//获取线程编号
	Event.omp_level = get_level ();

	//If current_level is larger than 0 ,it means that the current task is going to be suspended and new task is going
	//to be created
	if (current_level > 0)
	{
		Event.p_task_id_start = get_task_id (get_thread_id (current_level - 1));
		Event.task_id_start = get_task_id (get_thread_id (current_level));
		Event.task_state_start = TASK_SUSPEND;
	}

	current_task = get_new_task ();
	task_list [task_num ++] = current_task;

	/*获取父线程编号。通过get_level()获取当前嵌套层数,get_level ()-1即为
	  上一层嵌套层数。omp_get_ancestor_thread_num(get_level()-1)返回其在上
	  一层的线程编号，即为其父线程编号。-1表示无父线程。
	  */
	Event.p_rank = omp_get_ancestor_thread_num (get_level () - 1);

	/*dlsym函数返回 GOMP_parallel_start 在动态链接库中的下一个地址，供调用使用*/
	GOMP_parallel_start_real = (void(*)(void*,void*,unsigned))dlsym (RTLD_NEXT, "GOMP_parallel_start");
	if (GOMP_parallel_start_real != NULL)
	{	
		par_uf = (void(*)(void*))p1;										//调用子函数的包装函数
		par_uf_id++;

		/*if (PAPI == PAPI_ON)
			retVal = PAPI_thread_init(get_thread_num());
		if (retVal != PAPI_OK)
			ERROR_RETURN(retVal);*/
		
		Event.starttime = gettime();										//获取开始时间
		GOMP_parallel_start_real (callme_par, p2, p3);						//调用OpenMP库中的GOMP_parallel_start()实现功能
		Event.endtime = gettime ();											//获取结束时间
	}
	else
	{
		printf_d ("GOMP_parallel_start is not hooked! exiting!!\n");
	}

	Event.p_task_id_end = current_task.task_parent_id;
	Event.task_id_end = current_task.task_id;
	Event.task_state_end = TASK_CREATE;

	Record (&Event, OMPI_TRACE);
}
/*
	指导语句:	#pragma omp parallel
	结构功能:	parallel结束函数
	函数功能:	结束一个parallel并行结构
*/
void GOMP_parallel_end (void)
{
	int64_t task_id_temp;
	Record_Event Event = Event_init ();
	Event.event_name = "GOMP_parallel_end";
	Event.eid = 201;
	Event.type = NONE;
	Event.omp_rank = get_thread_num ();
	Event.omp_level = get_level ();
	Event.p_rank = omp_get_ancestor_thread_num (get_level () - 1);

	//save the current taskid
	task_id_temp = current_task_id;				
	if (flag > 0)
	{
		if (current_level > 0)
			Event.p_task_id_start = task_level [current_level - 1];
		
		Event.task_id_start = current_task_id;
		Event.task_state_start = TASK_WAIT;
	}

	GOMP_parallel_end_real = (void(*)(void))dlsym (RTLD_NEXT, "GOMP_parallel_end");
	if (GOMP_parallel_end_real != NULL)
	{
		Event.starttime = gettime ();
		GOMP_parallel_end_real ();
		Event.endtime = gettime ();
	}
	else
	{
		printf_d ("GOMP_parallel_end is not hooked! exiting!!\n");
	}

	//restore the taskid 
	current_task_id = task_id_temp;
	if (flag > 0)
	{
		if (current_level > 1)
		{
			--current_level;
			Event.p_task_id_end = task_level [current_level - 1];
			Event.task_id_end = current_task_id;
			Event.task_state_end = TASK_END;
		}
		else
		{
			Event.task_id_end = current_task_id;
			Event.task_state_end = TASK_END;
		}
		current_task_id = old_task_id [flag--];
	}
	else 
	{
		current_task_id = -1;
		flag = 0;
	}

	Record (&Event, OMPI_TRACE);
}


/**************************************************    Section     ************************************************/

/*
	指导语句:	#pragma omp sections
	结构功能:	section开始函数
	函数功能:	创建一个共享任务结构
*/
unsigned GOMP_sections_start (unsigned p1)
{
	Record_Event Event = Event_init ();
	Event.event_name = "GOMP_sections_start";
	Event.eid = 223;
	Event.type = NONE;
	Event.omp_rank = get_thread_num ();
	Event.omp_level = get_level ();
	Event.p_rank = omp_get_ancestor_thread_num (get_level () - 1);
	unsigned res=0;
	
	GOMP_sections_start_real=(unsigned(*)(unsigned)) dlsym (RTLD_NEXT, "GOMP_sections_start");
	if (GOMP_sections_start_real != NULL)
	{
		Event.starttime = gettime ();
		res = GOMP_sections_start_real (p1);
		Event.endtime = gettime ();
	}
	else
	{
		printf_d("GOMP_sections_start is not hooked! exiting!!\n");
	}

	Record (&Event, OMPI_TRACE);
	return res;
}
/*
	指导语句:	#pragma omp sections
	结构功能:	section调度函数
	函数功能:	当一个线程结束其执行的任务时，调用该函数分配下一个任务
*/
unsigned GOMP_sections_next (void)
{
	Record_Event Event = Event_init ();
	Event.event_name = "GOMP_sections_next";
	Event.eid = 224;
	Event.type = NONE;
	Event.omp_rank = get_thread_num ();
	Event.omp_level = get_level ();
	Event.p_rank = omp_get_ancestor_thread_num (get_level () - 1);
	unsigned res = 0;

	GOMP_sections_next_real=(unsigned(*)(void)) dlsym (RTLD_NEXT, "GOMP_sections_next");
	if (GOMP_sections_next_real != NULL)
	{
		Event.starttime=gettime();
		res = GOMP_sections_next_real();
		Event.endtime=gettime();
		Record(&Event, OMPI_TRACE);
	}
	else
	{
		printf_d("GOMP_sections_next is not hooked! exiting!!\n");
	}
	return res;
}

/*
	指导语句:	#pragma omp sections
	结构功能:	section结束函数
	函数功能:	结束一个共享任务结构
*/
void GOMP_sections_end_nowait (void)
{
	Record_Event Event = Event_init ();
	Event.event_name = "GOMP_sections_end_nowait";
	Event.eid = 226;
	Event.type = NONE;
	Event.omp_rank = get_thread_num ();
	Event.omp_level = get_level ();
	Event.p_rank = omp_get_ancestor_thread_num (get_level () - 1);

	GOMP_sections_end_nowait_real=(void(*)(void))dlsym (RTLD_NEXT, "GOMP_sections_end_nowait");
	if (GOMP_sections_end_nowait_real != NULL)
	{
		Event.starttime=gettime();
		GOMP_sections_end_nowait_real();
		Event.endtime=gettime();
		Record(&Event, OMPI_TRACE);
	}
	else
	{
		printf_d("GOMP_sections_end_nowait is not hooked! exiting!!\n");
	}
}

void GOMP_sections_end(void)
{
	Record_Event Event = Event_init ();
	Event.event_name = "GOMP_sections_end";
	Event.eid = 225;
	Event.type = NONE;
	Event.omp_rank = get_thread_num ();
	Event.omp_level = get_level ();
	Event.p_rank = omp_get_ancestor_thread_num (get_level () - 1);

	GOMP_sections_end_real=(void(*)(void)) dlsym (RTLD_NEXT, "GOMP_sections_end");
	if (GOMP_sections_end_real != NULL)
	{
		Event.starttime=gettime();
		GOMP_sections_end_real();
		Event.endtime=gettime();
		Record(&Event, OMPI_TRACE);
	}
	else
	{
		printf_d("GOMP_sections_end is not hooked! exiting!!\n");
	}
}

/**************************************************    Critical     ************************************************/

void GOMP_critical_start (void)
{
	Record_Event Event = Event_init ();
	Event.event_name = "GOMP_critical_start";
	Event.eid = 205;
	Event.type = NONE;
	Event.omp_rank = get_thread_num ();
	Event.omp_level = get_level ();
	Event.p_rank = omp_get_ancestor_thread_num (get_level () - 1);

	GOMP_critical_start_real=(void(*)(void)) dlsym (RTLD_NEXT, "GOMP_critical_start");
	if (GOMP_critical_start_real != NULL)
	{
		Event.starttime=gettime();
		GOMP_critical_start_real();
		Event.endtime=gettime();
		Record(&Event, OMPI_TRACE);
	}
	else
	{
		printf_d("GOMP_critical_start is not hooked! exiting!!\n");		
	}
}

void GOMP_critical_end (void)
{
	Record_Event Event = Event_init ();
	Event.event_name = "GOMP_critical_end";
	Event.eid = 206;
	Event.type = NONE;
	Event.omp_rank = get_thread_num ();
	Event.omp_level = get_level ();
	Event.p_rank = omp_get_ancestor_thread_num (get_level () - 1);

	GOMP_critical_end_real=(void(*)(void)) dlsym (RTLD_NEXT, "GOMP_critical_end");
	if (GOMP_critical_end_real != NULL)
	{
		Event.starttime=gettime();
		GOMP_critical_end_real ();
		Event.endtime=gettime();
		Record(&Event, OMPI_TRACE);
	}
	else
	{
		printf_d("GOMP_critical_end is not hooked! exiting!!\n");
	}
}

void GOMP_critical_name_start (void **p1)
{
	Record_Event Event = Event_init ();
	Event.event_name = "GOMP_critical_name_start";
	Event.eid = 203;
	Event.type = NONE;
	Event.omp_rank = get_thread_num ();
	Event.omp_level = get_level ();
	Event.p_rank = omp_get_ancestor_thread_num (get_level () - 1);

	GOMP_critical_name_start_real=(void(*)(void**)) dlsym (RTLD_NEXT, "GOMP_critical_name_start");
	if (GOMP_critical_name_start_real != NULL)
	{
		Event.starttime=gettime();
		GOMP_critical_name_start_real (p1);
		Event.endtime=gettime();
		Record(&Event, OMPI_TRACE);
	}
	else
	{
		printf_d("GOMP_critical_name_start is not hooked! exiting!!\n");
	}
}

void GOMP_critical_name_end(void **p1)
{
	Record_Event Event = Event_init ();
	Event.event_name = "GOMP_critical_name_end";
	Event.eid = 204;
	Event.type = NONE;
	Event.omp_rank = get_thread_num ();
	Event.omp_level = get_level ();
	Event.p_rank = omp_get_ancestor_thread_num (get_level () - 1);

	GOMP_critical_name_end_real=(void(*)(void**)) dlsym (RTLD_NEXT, "GOMP_critical_name_end");
	if (GOMP_critical_name_end_real != NULL)
	{
		Event.starttime=gettime();
		GOMP_critical_name_end_real (p1);
		Event.endtime=gettime();
		Record(&Event, OMPI_TRACE);
	}
	else
	{
		printf_d("GOMP_critical_name_end is not hooked! exiting!!\n");
	}
}


/**************************************************    Barrier     ************************************************/

void GOMP_barrier (void)
{
	int64_t old_task_id;
	Record_Event Event = Event_init ();
	Event.event_name = "GOMP_barrier";
	Event.eid = 202;
	Event.type = NONE;
	Event.omp_rank = get_thread_num ();
	Event.omp_level = get_level ();
	Event.p_rank = omp_get_ancestor_thread_num (get_level () - 1);

	old_task_id = current_task_id;
	if (current_task_id != -1)
	{
		if (current_level > 1)
			Event.p_task_id_start = task_level [current_level - 1];
		Event.task_id_start = current_task_id;
		Event.task_state_start = TASK_SUSPEND;
	}
	
	GOMP_barrier_real = (void(*)(void)) dlsym (RTLD_NEXT, "GOMP_barrier");

	if (GOMP_barrier_real != NULL)
	{
		Event.starttime = gettime();
		GOMP_barrier_real ();
		Event.endtime = gettime ();
	}
	else
		printf_d ("GOMP_barrier is not hooked! exiting!!\n");

	if (current_task_id != -1)
	{
		if (old_task_id != current_task_id)
		{
			if (current_level > 1)
				Event.p_task_id_end = task_level [current_level - 1];
			Event.task_id_end = current_task_id;
			Event.task_state_end = TASK_SUSPEND;			
		}
		else
		{
			if (current_level > 1)
				Event.p_task_id_end = task_level [current_level - 1];
			Event.task_id_end = current_task_id;
			Event.task_state_end = TASK_RESUME;
		}
	}
	
	current_task_id = old_task_id;
	Record (&Event, OMPI_TRACE);
}


/**************************************************    Atomic     ************************************************/


void GOMP_atomic_start(void)
{
	Record_Event Event = Event_init ();
	Event.event_name = "GOMP_atomic_start";
	Event.eid = 207;
	Event.type = NONE;
	Event.omp_rank = get_thread_num ();
	Event.omp_level = get_level ();
	Event.p_rank = omp_get_ancestor_thread_num (get_level () - 1);

	GOMP_atomic_start_real = (void(*)(void)) dlsym (RTLD_NEXT, "GOMP_atomic_start");
	if(GOMP_atomic_start_real != NULL)
	{
		Event.starttime=gettime();
		GOMP_atomic_start_real();
		Event.endtime=gettime();
		Record(&Event, OMPI_TRACE);
	}
	else
	{
		printf_d("GOMP_atomic_start is not hooked! exiting!!\n");
	}
}

void GOMP_atomic_end (void)
{
	Record_Event Event = Event_init ();
	Event.event_name = "GOMP_atomic_end";
	Event.eid = 208;
	Event.type = NONE;
	Event.omp_rank = get_thread_num ();
	Event.omp_level = get_level ();
	Event.p_rank = omp_get_ancestor_thread_num (get_level () - 1);

	GOMP_atomic_start_real = (void(*)(void)) dlsym (RTLD_NEXT, "GOMP_atomic_end");
	if (GOMP_atomic_end_real != NULL)
	{
		Event.starttime=gettime();
		GOMP_atomic_end_real();
		Event.endtime=gettime();
		Record(&Event, OMPI_TRACE);
	}
	else
	{
		printf_d("GOMP_atomic_end is not hooked! exiting!!\n");
	}
}

/**************************************************    Loop     ************************************************/
/*
	指导语句:	#pragma omp for
	结构功能:	for开始函数（无parallel时）
	函数功能:	创建一个任务共享结构
*/
int GOMP_loop_static_start(long p1, long p2, long p3, long p4, long *p5, long *p6)
{
	int res = 0;
	Record_Event Event = Event_init ();
	Event.event_name = "GOMP_loop_static_start";
	Event.eid = 217;
	Event.type = NONE;
	Event.omp_rank = get_thread_num ();
	Event.omp_level = get_level ();
	Event.p_rank = omp_get_ancestor_thread_num (get_level () - 1);

	GOMP_loop_static_start_real = (int(*)(long, long, long, long, long*, long*)) dlsym (RTLD_NEXT, "GOMP_loop_static_start");
	if (GOMP_loop_static_start_real != NULL)
	{
		Event.starttime = gettime ();
		res = GOMP_loop_static_start_real (p1, p2, p3, p4, p5, p6);
		Event.endtime = gettime ();
		Record (&Event, OMPI_TRACE);
	}
	else
	{
		printf_d ("GOMP_loop_static_start is not hooked! exiting!!\n");
	}
	return res;
}
/*
	指导语句:	#pragma omp for
	结构功能:	for开始函数（无parallel时）
	函数功能:	创建一个任务共享结构 	
*/
int GOMP_loop_runtime_start(long p1, long p2, long p3, long p4, long *p5, long *p6)
{
	int res = 0;
	Record_Event Event = Event_init ();
	Event.event_name = "GOMP_loop_runtime_start";
	Event.eid = 218;
	Event.type = NONE;
	Event.omp_rank = get_thread_num ();
	Event.omp_level = get_level ();
	Event.p_rank = omp_get_ancestor_thread_num (get_level () - 1);

	GOMP_loop_runtime_start_real=(int(*)(long,long,long,long,long*,long*)) dlsym (RTLD_NEXT, "GOMP_loop_runtime_start");
	if (GOMP_loop_runtime_start_real != NULL)
	{
		Event.starttime=gettime();
		res = GOMP_loop_runtime_start_real (p1, p2, p3, p4, p5, p6);
		Event.endtime=gettime();
		Record(&Event, OMPI_TRACE);
	}
	else
	{
		printf_d("GOMP_loop_runtime_start is not hooked! exiting!!\n");
	}
	return res;
}
/*
	指导语句:	#pragma omp for
	结构功能:	for开始函数（无parallel时）
	函数功能:	创建一个任务共享结构
*/
int GOMP_loop_guided_start (long p1, long p2, long p3, long p4, long *p5, long *p6)
{
	int res = 0;
	Record_Event Event = Event_init ();
	Event.event_name = "GOMP_loop_guided_start";
	Event.eid = 219;
	Event.type = NONE;
	Event.omp_rank = get_thread_num ();
	Event.omp_level = get_level ();
	Event.p_rank = omp_get_ancestor_thread_num (get_level () - 1);

	GOMP_loop_guided_start_real=(int(*)(long,long,long,long,long*,long*)) dlsym (RTLD_NEXT, "GOMP_loop_guided_start");

	if (GOMP_loop_guided_start_real != NULL)
	{
		Event.starttime=gettime();
		res = GOMP_loop_guided_start_real (p1, p2, p3, p4, p5, p6);
		Event.endtime=gettime();
		Record(&Event, OMPI_TRACE);
	}
	else
	{
		printf_d("GOMP_loop_guided_start is not hooked! exiting!!\n");
	}
	return res;
}
/*
	指导语句:	#pragma omp for
	结构功能:	for开始函数（无parallel时）
	函数功能:	创建一个任务共享结构
*/
int GOMP_loop_dynamic_start (long p1, long p2, long p3, long p4, long *p5, long *p6)
{
	int res = 0;
	Record_Event Event = Event_init ();
	Event.event_name = "GOMP_loop_dynamic_start";
	Event.eid = 220;
	Event.type = NONE;
	Event.omp_rank = get_thread_num ();
	Event.omp_level = get_level ();
	Event.p_rank = omp_get_ancestor_thread_num (get_level () - 1);

	GOMP_loop_dynamic_start_real = (int(*)(long,long,long,long,long*,long*)) dlsym (RTLD_NEXT, "GOMP_loop_dynamic_start");
	if (GOMP_loop_dynamic_start_real != NULL)
	{

		Event.starttime = gettime ();
		res = GOMP_loop_dynamic_start_real (p1, p2, p3, p4, p5, p6);
		Event.endtime = gettime ();
		Record (&Event, OMPI_TRACE);
	}
	else
	{
		printf_d("GOMP_loop_dynamic_start is not hooked! exiting!!\n");
	}
	return res;
}
/*
	指导语句:	#pragma omp for
	结构功能:	for预初始化函数（有parallel时）
	函数功能:	预初始化一个任务共享结构
*/
void GOMP_parallel_loop_static_start (void *p1, void *p2, unsigned p3, long p4, long p5, long p6, long p7)
{
	Record_Event Event = Event_init ();
	Event.event_name = "GOMP_parallel_loop_static_start";
	Event.eid = 209;
	Event.type = NONE;
	Event.omp_rank = get_thread_num ();
	Event.omp_level = get_level ();
	Event.p_rank = omp_get_ancestor_thread_num (get_level () - 1);

	if (current_level > 0)
	{
		Event.p_task_id_start = task_level [current_level-1];
		Event.task_id_start = current_task_id;
		Event.task_state_start = TASK_SUSPEND;
	}
	
	task_level [current_level++] = current_task_id;
	//store the current_task_id in the old_task_id array
	old_task_id [++flag] = current_task_id;
	current_task_id = get_new_task_id ();

	GOMP_parallel_loop_static_start_real=(void(*)(void*,void*,unsigned,long,long,long,long))dlsym(RTLD_NEXT,"GOMP_parallel_loop_static_start");
	if(GOMP_parallel_loop_static_start_real!= NULL)
	{
		pardo_uf = (void(*)(void*))p1;
		pardo_uf_id++;

		/*if (PAPI == PAPI_ON)
			retVal = PAPI_thread_init(get_thread_num());
		if (retVal != PAPI_OK)
			ERROR_RETURN(retVal);*/

		Event.starttime=gettime();
		GOMP_parallel_loop_static_start_real (callme_pardo, p2, p3, p4, p5, p6, p7);
		Event.endtime=gettime();
	}
	else
	{
		printf_d("GOMP_parallel_loop_static_start is not hooked! exiting!!\n");
	}

	Event.p_task_id_end = task_level [current_level - 1];
	Event.task_id_end = current_task_id;
	Event.task_state_end = TASK_CREATE;

	Record (&Event, OMPI_TRACE);
}
/*
	指导语句:	#pragma omp for
	结构功能:	for预初始化函数（有parallel时）
	函数功能:	预初始化一个任务共享结构
*/
void GOMP_parallel_loop_runtime_start (void *p1, void *p2, unsigned p3, long p4, long p5, long p6, long p7)
{
//	printf_d ("[paraloop] thread %d is   now waiting task %llx, time:%u\n", gettid (), current_task_id, clock ());
	Record_Event Event = Event_init ();
	Event.event_name = "GOMP_parallel_loop_runtime_start";
	Event.eid = 210;
	Event.type = NONE;
	Event.omp_rank = get_thread_num ();
	Event.omp_level = get_level ();
	Event.p_rank = omp_get_ancestor_thread_num (get_level () - 1);

	if (current_level > 0)
	{
		Event.p_task_id_start = task_level [current_level-1];
		Event.task_id_start = current_task_id;
		Event.task_state_start = TASK_SUSPEND;
	}
	else
		printf_d ("-------------------current_level : %d\n", current_level);
	
	task_level [current_level++] = current_task_id;

	//store the current_task_id in the old_task_id array
	old_task_id [++flag] = current_task_id;
	current_task_id = get_new_task_id ();

	GOMP_parallel_loop_runtime_start_real = (void(*)(void*,void*,unsigned, long, long, long, long)) dlsym (RTLD_NEXT, "GOMP_parallel_loop_runtime_start");
	if (GOMP_parallel_loop_runtime_start_real != NULL)
	{
		pardo_uf = (void(*)(void*))p1;
		pardo_uf_id++;
		
		Event.starttime=gettime();
		GOMP_parallel_loop_runtime_start_real (callme_pardo, p2, p3, p4, p5, p6, p7);
		Event.endtime=gettime();
	}
	else
	{
		printf_d("GOMP_parallel_loop_runtime_start is not hooked! exiting!!\n");
	}

	Event.p_task_id_end = task_level [current_level - 1];
	Event.task_id_end = current_task_id;
	Event.task_state_end = TASK_CREATE;

	Record(&Event, OMPI_TRACE);
}
/*
	指导语句:	#pragma omp for
	结构功能:	for预初始化函数（有parallel时）
	函数功能:	预初始化一个任务共享结构
*/
void GOMP_parallel_loop_dynamic_start (void *p1, void *p2, unsigned p3, long p4, long p5, long p6, long p7)
{
	Record_Event Event = Event_init ();
	Event.event_name = "GOMP_parallel_loop_dynamic_start";
	Event.eid = 211;
	Event.type = NONE;
	Event.omp_rank = get_thread_num ();
	Event.omp_level = get_level ();
	Event.p_rank = omp_get_ancestor_thread_num (get_level () - 1);
	
	if (current_level > 0)
	{
		Event.p_task_id_start = task_level [current_level-1];
		Event.task_id_start = current_task_id;
		Event.task_state_start = TASK_SUSPEND;
	}
	
	task_level [current_level++] = current_task_id;

	//store the current_task_id in the old_task_id array
	old_task_id [++flag] = current_task_id;
	current_task_id = get_new_task_id ();

	GOMP_parallel_loop_dynamic_start_real = (void(*)(void*,void*,unsigned, long, long, long, long)) dlsym (RTLD_NEXT, "GOMP_parallel_loop_dynamic_start");
	if (GOMP_parallel_loop_dynamic_start_real != NULL)
	{
		pardo_uf = (void(*)(void*))p1;
		++pardo_uf_id;
		
		Event.starttime = gettime ();
		GOMP_parallel_loop_dynamic_start_real (callme_pardo, p2, p3, p4, p5, p6, p7);
		Event.endtime = gettime();
	}
	else
	{
		printf_d ("GOMP_parallel_loop_dynamic_start is not hooked! exiting!!\n");
	}

	Event.p_task_id_end = task_level [current_level - 1];
	Event.task_id_end = current_task_id;
	Event.task_state_end = TASK_CREATE;

	Record (&Event, OMPI_TRACE);
}
/*
	指导语句:	#pragma omp for
	结构功能:	for预初始化函数（有parallel时）
	函数功能:	预初始化一个任务共享结构
*/
void GOMP_parallel_loop_guided_start(void *p1, void *p2, unsigned p3, long p4, long p5, long p6, long p7)
{
	Record_Event Event = Event_init ();
	Event.event_name = "GOMP_parallel_loop_guided_start";
	Event.eid = 212;
	Event.type = NONE;
	Event.omp_rank = get_thread_num ();
	Event.omp_level = get_level ();
	Event.p_rank = omp_get_ancestor_thread_num (get_level () - 1);
	
	if (current_level > 0)
	{
		Event.p_task_id_start = task_level [current_level-1];
		Event.task_id_start = current_task_id;
		Event.task_state_start = TASK_SUSPEND;
	}
	
	task_level [current_level++] = current_task_id;

	//store the current_task_id in the old_task_id array
	old_task_id [++flag] = current_task_id;
	current_task_id = get_new_task_id ();

	GOMP_parallel_loop_guided_start_real=(void(*)(void*,void*,unsigned, long, long, long, long)) dlsym (RTLD_NEXT, "GOMP_parallel_loop_guided_start");
	if (GOMP_parallel_loop_guided_start_real != NULL)
	{
		pardo_uf = (void(*)(void*))p1;
		pardo_uf_id++;

		/*if (PAPI == PAPI_ON)
			retVal = PAPI_thread_init(get_thread_num());
		if (retVal != PAPI_OK)
			ERROR_RETURN(retVal);*/
		
		Event.starttime=gettime();
		GOMP_parallel_loop_guided_start_real (callme_pardo, p2, p3, p4, p5, p6, p7);
		Event.endtime=gettime();
	}
	else
	{
		printf_d("GOMP_parallel_loop_guided_start is not hooked! exiting!!\n");
	}

	Event.p_task_id_end = task_level [current_level - 1];
	Event.task_id_end = current_task_id;
	Event.task_state_end = TASK_CREATE;

	Record(&Event, OMPI_TRACE);
}
/*
	指导语句:	#pragma omp for
	结构功能:	for调度函数
	函数功能:	当一个线程完成指定给它的任务时，调用该函数分配下个任务
*/
int GOMP_loop_static_next (long *p1, long *p2)
{
	int res = 0;
	Record_Event Event = Event_init ();
	Event.event_name = "GOMP_loop_static_next";
	Event.eid = 213;
	Event.type = NONE;
	Event.omp_rank = get_thread_num ();
	Event.omp_level = get_level ();
	Event.p_rank = omp_get_ancestor_thread_num (get_level () - 1);

	Event.p_task_id_start = task_level [current_level - 1];
	Event.task_id_start = current_task_id;
	Event.task_state_start = TASK_END;

	GOMP_loop_static_next_real=(int(*)(long*,long*)) dlsym (RTLD_NEXT, "GOMP_loop_static_next");
	if (GOMP_loop_static_next_real != NULL)
	{
		Event.starttime=gettime();
		res = GOMP_loop_static_next_real (p1, p2);
		Event.endtime=gettime();
	}
	else
	{
		printf_d ("GOMP_loop_static_next is not hooked! exiting!!\n");
	}

	if (res == 1)
	{
		current_task_id = get_new_task_id ();
		Event.p_task_id_end = task_level [current_level - 1];
		Event.task_id_end = current_task_id;
		Event.task_state_end = TASK_CREATE;
	}
	else if (flag > 1)
	{
		current_task_id = old_task_id [flag--];
		Event.p_task_id_end = task_level [current_level - 1];
		Event.task_id_end = current_task_id;
		Event.task_state_end = TASK_WAIT;
	}
	else 
		flag = 0;

	Record (&Event, OMPI_TRACE);
	return res;
}
/*
	指导语句:	#pragma omp for
	结构功能:	for调度函数
	函数功能:	当一个线程完成指定给它的任务时，调用该函数分配下个任务
*/
int GOMP_loop_runtime_next (long *p1, long *p2)
{
	int res = 0;
	Record_Event Event = Event_init ();
	Event.event_name = "GOMP_loop_runtime_next";
	Event.eid = 214;
	Event.type = NONE;
	Event.omp_rank = get_thread_num ();
	Event.omp_level = get_level ();
	Event.p_rank = omp_get_ancestor_thread_num (get_level () - 1);

	Event.p_task_id_start = task_level [current_level - 1];
	Event.task_id_start = current_task_id;
	Event.task_state_start = TASK_END;

	GOMP_loop_runtime_next_real = (int(*)(long*,long*)) dlsym (RTLD_NEXT, "GOMP_loop_runtime_next");

	if (GOMP_loop_runtime_next_real != NULL)
	{
		Event.starttime=gettime ();
		res = GOMP_loop_runtime_next_real (p1, p2);
		Event.endtime = gettime ();
	}
	else
	{
		printf_d ("GOMP_loop_runtime_next is not hooked! exiting!!\n");
	}

	if (res == 1)
	{
		current_task_id = get_new_task_id ();
		Event.p_task_id_end = task_level [current_level - 1];
		Event.task_id_end = current_task_id;
		Event.task_state_end = TASK_CREATE;
	}
	else if (flag > 1)
	{
		current_task_id = old_task_id [flag--];
		Event.p_task_id_end = task_level [current_level - 1];
		Event.task_id_end = current_task_id;
		Event.task_state_end = TASK_WAIT;
	}
	else 
		flag = 0;

	Record (&Event, OMPI_TRACE);
	return res;
}
int GOMP_loop_dynamic_next(long *p1, long *p2)
{
	int res = 0;
	Record_Event Event = Event_init ();
	Event.event_name = "GOMP_loop_dynamic_next";
	Event.eid = 215;
	Event.type = NONE;
	Event.omp_rank = get_thread_num ();
	Event.omp_level = get_level ();
	Event.p_rank = omp_get_ancestor_thread_num (get_level () - 1);

	Event.p_task_id_start = task_level [current_level - 1];
	Event.task_id_start = current_task_id;
	Event.task_state_start = TASK_END;

	GOMP_loop_dynamic_next_real=(int(*)(long*,long*)) dlsym (RTLD_NEXT, "GOMP_loop_dynamic_next");

	if (GOMP_loop_dynamic_next_real != NULL)
	{
		Event.starttime=gettime();
		res = GOMP_loop_dynamic_next_real (p1, p2);
		Event.endtime=gettime();
	}
	else
	{
		printf_d("GOMP_loop_dynamic_next is not hooked! exiting!!\n");
	}

	if (res == 1)
	{
		current_task_id = get_new_task_id ();
		Event.p_task_id_end = task_level [current_level - 1];
		Event.task_id_end = current_task_id;
		Event.task_state_end = TASK_CREATE;
	}
	else if (flag > 1)
	{
		current_task_id = old_task_id [flag--];
		Event.p_task_id_end = task_level [current_level - 1];
		Event.task_id_end = current_task_id;
		Event.task_state_end = TASK_WAIT;
	}
	else 
		flag = 0;

	Record (&Event, OMPI_TRACE);
	return res;
}
/*
	指导语句:	#pragma omp parallel for
	结构功能:	for调度函数
	函数功能:	当一个线程完成指定给它的任务时，调用该函数分配下个任务
*/
int GOMP_loop_guided_next(long *p1, long *p2)
{
	int res = 0;
	Record_Event Event = Event_init ();
	Event.event_name = "GOMP_loop_guided_next";
	Event.eid = 216;
	Event.type = NONE;
	Event.omp_rank = get_thread_num ();
	Event.omp_level = get_level ();
	Event.p_rank = omp_get_ancestor_thread_num (get_level () - 1);

	Event.p_task_id_start = task_level [current_level - 1];
	Event.task_id_start = current_task_id;
	Event.task_state_start = TASK_END;

	GOMP_loop_guided_next_real=(int(*)(long*,long*)) dlsym (RTLD_NEXT, "GOMP_loop_guided_next");

	if (GOMP_loop_guided_next_real != NULL)
	{
		Event.starttime=gettime();
		res = GOMP_loop_guided_next_real (p1, p2);
		Event.endtime=gettime();
		Record(&Event, OMPI_TRACE);
	}
	else
	{
		printf_d("GOMP_loop_guided_next is not hooked! exiting!!\n");
	}

	if (res == 1)
	{
		current_task_id = get_new_task_id ();
		Event.p_task_id_end = task_level [current_level - 1];
		Event.task_id_end = current_task_id;
		Event.task_state_end = TASK_CREATE;
	}
	else if (flag > 1)
	{
		current_task_id = old_task_id [flag--];
		Event.p_task_id_end = task_level [current_level - 1];
		Event.task_id_end = current_task_id;
		Event.task_state_end = TASK_WAIT;
	}
	else 
		flag = 0;

	Record (&Event, OMPI_TRACE);
	return res;
}
/*
	指导语句:	#pragma omp parallel for
	结构功能:	for结束函数
	函数功能:	结束一个任务共享结构,并同步所有线程
*/
void GOMP_loop_end (void)
{
	int64_t task_id_temp;
	Record_Event Event = Event_init ();
	Event.event_name = "GOMP_loop_end";
	Event.eid = 221;
	Event.type = NONE;
	Event.omp_rank = get_thread_num ();
	Event.omp_level = get_level ();
	Event.p_rank = omp_get_ancestor_thread_num (get_level () - 1);

	task_id_temp = current_task_id;
	if (flag > 0)
	{
		if (current_level > 0)
			Event.p_task_id_start = task_level [current_level - 1];
		
		Event.task_id_start = current_task_id;
		Event.task_state_start = TASK_WAIT;
	}

	GOMP_loop_end_real=(void(*)(void)) dlsym (RTLD_NEXT, "GOMP_loop_end");
	if (GOMP_loop_end_real != NULL)
	{
		Event.starttime=gettime();
		GOMP_loop_end_real();
		Event.endtime=gettime();
		Record(&Event, OMPI_TRACE);
	}
	else
	{
		printf_d("GOMP_loop_end is not hooked! exiting!!\n");
	}

	current_task_id = task_id_temp;
	if (flag > 0)
	{
		if (current_level > 1)
		{
			--current_level;
			Event.p_task_id_end = task_level [current_level - 1];
			Event.task_id_end = current_task_id;
			Event.task_state_end = TASK_END;
		}
		else
		{
			Event.task_id_end = current_task_id;
			Event.task_state_end = TASK_END;
		}
		current_task_id = old_task_id [flag--];
	}
	else 
	{
		current_task_id = -1;
		flag = 0;
	}
}
/*
	指导语句:	#pragma omp parallel for
	结构功能:	for结束函数
	函数功能:	结束一个任务共享结构,不同步所有线程
*/
void GOMP_loop_end_nowait (void)
{
	int64_t task_id_temp;
	Record_Event Event = Event_init ();
	Event.event_name = "GOMP_loop_end_nowait";
	Event.eid = 222;
	Event.type = NONE;
	Event.omp_rank = get_thread_num ();
	Event.omp_level = get_level ();
	Event.p_rank = omp_get_ancestor_thread_num (get_level () - 1);

	task_id_temp = current_task_id;
	if (flag > 0)
	{
		if (current_level > 0)
			Event.p_task_id_start = task_level [current_level - 1];
		
		Event.task_id_start = current_task_id;
		Event.task_state_start = TASK_WAIT;
	}

	GOMP_loop_end_nowait_real=(void(*)(void)) dlsym (RTLD_NEXT, "GOMP_loop_end_nowait");
	if (GOMP_loop_end_nowait_real != NULL)
	{
		Event.starttime=gettime();
		GOMP_loop_end_nowait_real();
		Event.endtime=gettime();
		Record(&Event, OMPI_TRACE);
	}
	else
	{
		printf_d("GOMP_loop_end_nowait is not hooked! exiting!!\n");
	}

	current_task_id = task_id_temp;
	if (flag > 0)
	{
		if (current_level > 1)
		{
			--current_level;
			Event.p_task_id_end = task_level [current_level - 1];
			Event.task_id_end = current_task_id;
			Event.task_state_end = TASK_END;
		}
		else
		{
			Event.task_id_end = current_task_id;
			Event.task_state_end = TASK_END;
		}
		current_task_id = old_task_id [flag--];
	}
	else 
	{
		current_task_id = -1;
		flag = 0;
	}
}

void GOMP_parallel_sections_start(void *p1, void *p2, unsigned p3, unsigned p4)
{
	Record_Event Event = Event_init ();
	Event.event_name = "GOMP_parallel_sections_start";
	Event.eid = 227;
	Event.type = NONE;
	Event.omp_rank = get_thread_num ();
	Event.omp_level = get_level ();
	Event.p_rank = omp_get_ancestor_thread_num (get_level () - 1);

	GOMP_parallel_sections_start_real=(void(*)(void*,void*,unsigned,unsigned)) dlsym (RTLD_NEXT, "GOMP_parallel_sections_start");
	if (GOMP_parallel_sections_start_real != NULL)
	{
		Event.starttime=gettime();
		GOMP_parallel_sections_start_real (p1, p2, p3, p4);
		Event.endtime=gettime();
		Record(&Event, OMPI_TRACE);
	}
	else
	{
		printf_d("GOMP_parallel_sections_start is not hooked! exiting!!\n");
	}
}

/**************************************************    Lock     ************************************************/

void omp_set_lock (int *p1)
{
	Record_Event Event = Event_init ();
	Event.event_name = "omp_set_lock";
	Event.eid = 230;
	Event.type = NONE;
	Event.omp_rank = get_thread_num ();
	Event.omp_level = get_level ();
	Event.p_rank = omp_get_ancestor_thread_num (get_level () - 1);

	omp_set_lock_real = (void(*)(int*)) dlsym (RTLD_NEXT, "omp_set_lock");
	if (omp_set_lock_real != NULL)
	{
		Event.starttime=gettime();
		omp_set_lock_real(p1);
		Event.endtime=gettime();
		Record(&Event, OMPI_TRACE);
	}
	else
	{
		printf_d("omp_set_lock is not hooked! exiting!!\n");
	}
}

void omp_unset_lock (int *p1)
{
	Record_Event Event = Event_init ();
	Event.event_name = "omp_unset_lock";
	Event.eid = 231;
	Event.type = NONE;
	Event.omp_rank = get_thread_num ();
	Event.omp_level = get_level ();
	Event.p_rank = omp_get_ancestor_thread_num (get_level () - 1);

	omp_unset_lock_real = (void(*)(int*)) dlsym (RTLD_NEXT, "omp_unset_lock");
	if(omp_unset_lock_real!= NULL)
	{
		Event.starttime = gettime ();
		omp_unset_lock_real (p1);
		Event.endtime = gettime ();
		Record (&Event, OMPI_TRACE);
	}
	else
	{
		printf_d ("omp_unset_lock is not hooked! exiting!!\n");
	}
}
/*
	设置期望的线程数
*/
void omp_set_num_threads (int p1)
{
	Record_Event Event = Event_init ();
	Event.event_name = "omp_set_num_threads";
	Event.eid = 232;
	Event.type = NONE;
	Event.omp_rank = get_thread_num ();
	Event.omp_level = get_level ();
	Event.p_rank = omp_get_ancestor_thread_num (get_level () - 1);

	omp_set_num_threads_real = (void(*)(int)) dlsym (RTLD_NEXT, "omp_set_num_threads");

	if (omp_set_num_threads_real != NULL)
	{
		Event.starttime = gettime();
		omp_set_num_threads_real (p1);
		Event.endtime = gettime();
		Record (&Event, OMPI_TRACE);
	}
	else
	{
		printf_d ("omp_set_num_threads is not hooked! exiting!!\n");
	}
}

/**************************************************    Task     ************************************************/

void GOMP_task (void *p1, void *p2, void *p3,long p4, long p5, _Bool p6, unsigned p7)
{
	int64_t old_task_id = current_task_id;
 
	Record_Event Event = Event_init ();
	Event.event_name = "GOMP_task";
	Event.eid = 229;
	Event.type = NONE;
	Event.omp_rank = get_thread_num ();
	Event.omp_level = get_level ();
	Event.p_rank = omp_get_ancestor_thread_num (get_level () - 1);

	if (current_level > 1)
		Event.p_task_id_start = task_level [current_level - 1];
	task_level [current_level] = current_task_id;
	
	Event.task_id_start = current_task_id;
	Event.task_state_start = TASK_SUSPEND;

	GOMP_task_real = (void(*)(void *,void *,void *,long,long,_Bool,unsigned))dlsym(RTLD_NEXT,"GOMP_task");

	if(GOMP_task_real != NULL)
	{
		task_uf = (void(*)(void*))p1;
		task_uf_id++;

		Event.starttime = gettime();
		GOMP_task_real (callme_task, p2, p3, p4, p5, p6, p7);
		Event.endtime = gettime();
	}
	current_task_id = old_task_id;

	if (current_level > 1)
		Event.p_task_id_end = task_level [current_level - 1];
	Event.task_id_end = current_task_id;
	Event.task_state_end = TASK_RESUME;

	Record(&Event, OMPI_TRACE);
}

void GOMP_taskwait (void)
{
	int64_t old_task_id = current_task_id;
	Record_Event Event = Event_init ();
	Event.event_name = "GOMP_taskwait";
	Event.eid = 228;
	Event.type = NONE;                                                                                     
	Event.omp_rank = get_thread_num ();
	Event.omp_level = get_level ();
	Event.p_rank = omp_get_ancestor_thread_num (get_level () - 1);

	if (current_level > 1)
		Event.p_task_id_start = task_level [current_level - 1];

	Event.task_id_start = current_task_id;
	Event.task_state_start = TASK_SUSPEND;

	GOMP_taskwait_real = (void(*)(void))dlsym(RTLD_NEXT,"GOMP_taskwait");

	if(GOMP_taskwait_real != NULL)
	{
		Event.starttime = gettime();
		GOMP_taskwait_real ();
		Event.endtime = gettime ();
	}

	if (current_level > 1)
		Event.p_task_id_end = task_level [current_level - 1];

	current_task_id = old_task_id;

	Event.task_id_end = current_task_id;
	Event.task_state_end = TASK_RESUME;

	Record (&Event, OMPI_TRACE);
}
