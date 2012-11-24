#define _GNU_SOURCE

#include <dlfcn.h>
#include <pthread.h>
#include <stdbool.h>
#include <unistd.h>

#include "wrapper.h"

#define TASK_MAX_NUM	100
#define TASK_MAX_LEVEL	100
#define MAX_TEAM_SIZE 	10
#define MAX_TEAM_NUM	20
#define MAX_THREAD_NUM	10

#define EXPLICIT_TASK	1
#define IMPLICIT_TASK	2

//Implict task data struct
typedef struct iTask
{
	int thread_id;
	int64_t task_id;
	int64_t task_parent_id;
	int flag;
}TaskInfo;

//Explict task data struct
typedef struct eTask
{
	TaskInfo task_info;
	struct eTask * parent_task;
	struct eTask * child_task;
	struct eTask * prev_task;
	struct eTask * next_task;
};

typedef struct TeamInfo_
{
	TaskInfo task;										//The information of the task that create this thread team
	int team_size;
	int team_num;										//The number of this thread team in Team []
	int team_flag;										//When a thread team is ended, the vaule of team_flag will be 0
	struct eTask *etask;
	struct iTask itask[MAX_TEAM_SIZE];
}TeamInfo;

TeamInfo Team [MAX_TEAM_NUM];	
int Team_num = 0;

static __thread TaskInfo current_task = {0};
//static __thread blockCount = 0;						//Count the parallel blocks that this thread has created

static __thread int32_t task_count = 0;

/*static __thread int64_t parent_task_id = -1;
static __thread int64_t current_task_id = -1;
static __thread int64_t old_task_id [TASK_MAX_NUM];
static int64_t task_level [TASK_MAX_LEVEL];
static int current_level = 0;
static __thread int flag = 0;*/

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
		thread_id = thread_id * MAX_THREAD_NUM + omp_get_ancestor_thread_num (level--);

	return thread_id;
}

int get_team_num ()
{
	int i;
	int team_thread_id  = get_thread_id (get_level () -1);
	for (i = 0; i < Team_num; ++i)
	{
		if (Team[i].team_flag == 1 && Team [i].task.thread_id == team_thread_id)
			return i;
	}
	//If the thread team is not exist ,return 0
	return 0;
}

//Allocate a new task id for the current task, this task id is related to the tid and the team number
int64_t get_new_task_id ()
{
	int64_t team_num = get_team_num();
	int64_t thread_num = gettid ();
	int64_t new_task_id;

	new_task_id = (team_num << 60) + (thread_num << 12) + task_count ++;
	printf_d ("team_num:%llx\t,thead_num:%llx\t, task_id:%llx\n ",team_num,thread_num, new_task_id );
	return new_task_id;
}

//Get the task id that running on the thread
int64_t get_task_id (int thread_id)
{
	int i;

	if (thread_id < 0)
		return -1;
	
	for (i = 0; i < Team_num; ++i)
	{
		if (Team [i].task.thread_id == thread_id)
			return Team [i].task.task_id;
	}
	return -1;
}


TaskInfo get_current_task ()
{
	TaskInfo task = {0};
	int i, num = Team_num;
	int team_thread_id = get_thread_id (get_level () - 1);
	int thread_num = get_thread_num ();

	for (i = 0; i < num; ++i)
	{
		if (Team [i].task.thread_id == team_thread_id && Team [i].team_flag == 1)
		{
			task = Team [i].itask [thread_num];
			break;
		}
	}

	return task;
}
/*//Get the current task 
TaskInfo get_current_task (int task_type)
{
	TaskInfo task;
	int i;
	int team_thread_id = get_thread_id (get_level () - 1);
	int thread_num = get_thread_num ();

	if (type == EXPLICIT_TASK)
	{
		need to be done
	}
	else if (type == IMPLICIT_TASK)
	{
		for (i = 0; i < Team_num; ++i)
		{
			if (Team [i].task.thread_id == team_thread_id)
			{
				task = Team [i].itask [thread_num];
				break;
			}
		}
	}

	return task;
}*/

//Create an implicit task struct 
struct iTask create_itask ()
{
	struct iTask task;
	task.thread_id = get_thread_id (get_level ());
	task.task_id = get_new_task_id ();
	task.task_parent_id = get_task_id (get_thread_id (get_level () - 1));
	task.flag = 1;

	return task;
}

//Create an explicit task struct
struct eTask create_etask ()
{
	struct eTask task;
	TaskInfo task_info;

	task_info.task_id = get_new_task_id ();
	task_info.task_parent_id = current_task.task_id;
	task_info.flag = 1;

	/*need to be done*/

	return task;
}

//Add a new implicit task to the team
int add_itask (struct iTask task)
{
	int i;
	int team_thread_id = get_thread_id (get_level () - 1);
	int thread_num = get_thread_num ();

	for (i = 0; i < Team_num; ++i)
	{
		if (Team [i].team_flag == 1 && Team [i].task.thread_id == team_thread_id)
		{
			Team [i].itask [thread_num] = task;
			return 0;
		}
	}
	return -1;
}

//Add a new explicit task to the team, waitting to be scheduled.
//This function will only be called in Gomp_task () function
int add_etask (struct eTask task)
{
	int i;
	int team_thread_id = get_thread_id (get_level () - 1);

	for (i = 0; i < Team_num; ++i)
	{
		if (Team [i].team_flag == 1 && Team [i].task.thread_id == team_thread_id)
		{
			/*need to be done*/
			return 0;
		}
	}

	return -1;
}

struct eTask etask_schedule ()
{
	struct eTask task;
	
	/* need to be done */
	
	return task;
}


//Remove an implicit task from the thread team
int remove_itask (struct iTask task)
{
	int i;
	int team_thread_id = get_thread_id (get_level () - 1);
	int thread_num = get_thread_num ();

	for (i = 0; i < Team_num; ++i)
	{
		if (Team [i].team_flag == 1 && Team [i].task.thread_id == team_thread_id)
		{
			Team [i].itask [thread_num].task_id = -1;
			Team [i].itask [thread_num].task_parent_id = -1;
			Team [i].itask [thread_num].flag = 0;
			return 0;
		}
	}
	return -1;
}

//Remove an explicit task from the thread team
int remove_etask (struct eTask task)
{
	int i;
	int team_thread_id = get_thread_id (get_level () - 1);

	for (i = 0; i < Team_num; ++i)
	{
		if (Team [i].team_flag == 1 && Team [i].task.thread_id == team_thread_id)
		{
			/*need to be done*/
			return 0;
		}
	}
	return -1;
}

//Create a new thread team
void create_team (TaskInfo task)
{
	int i, num = Team_num;
	
	for (i = 0; i < num && i < MAX_TEAM_NUM; ++i)
	{
		if (Team [i].team_flag == 0)
		{
			Team [i].team_flag = 1;
			break;
		}
	}

	if (i == num)
		num = Team_num++;
	else
		num = i;

	Team [num].task = current_task;
	Team [num].team_num = num;
	Team [num].team_flag = 1;
	Team [num].etask = NULL;
	Team [num].itask [0] = task;					//Current task is the task of master thread
}

//Remove a thread team
void remove_team ()
{
	int num = get_team_num ();
	Team [num].team_flag = 0;
}


/**************************************************    User function     ************************************************/
/*
	指导语句:	#pragma omp parallel for
	结构功能:	user function of parallel for struct ,an implicit task
	函数功能:	for指导语句中调用的用户子函数
*/
static void callme_pardo(void *p1)
{
	TaskInfo old_task;
	char fun_name[30] = "Parallel_User_do_fun_";
	char id [10];

	Record_Event Event = Event_init ();

	old_task = current_task;
	current_task = create_itask ();
	//Is it necessary to add this task to the thread team ?
	add_itask (current_task);
	
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
	remove_itask (current_task);
	current_task = old_task;
	
	Record (&Event, OMPI_TRACE);
}
/*
	指导语句:	#pragma omp parallel
	结构功能:	parallel用户子函数
	函数功能:	parallel中调用的用户子函数
*/
static void callme_par (void *p1)
{
	TaskInfo old_task;

	char fun_name[30] = "Parallel_User_fun_";
	char id [10];
	Record_Event Event = Event_init ();

	old_task = current_task;
	current_task = create_itask ();
	//Is it necessary to add this task to the thread team ?
	add_itask (current_task);
	
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
	remove_itask (current_task);
	current_task = old_task;
	
	Record (&Event, OMPI_TRACE);
}
/*
	指导语句:	#pragma omp task
	结构功能:	task用户子函数
	函数功能:	task中调用的用户子函数
*/
static void callme_task (void *p1)
{
	TaskInfo old_task;
	struct eTask task;
	char fun_name[30] = "Task_User_do_fun_";
	char id [10];

	Record_Event Event = Event_init ();

	old_task = current_task;
	task = etask_schedule ();
	current_task = task.task_info;

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
	Event.task_state_start = TASK_START;
	if (task_uf == NULL)	
	{
		printf_d("Error! Invalid initialization of 'task_uf'\n");
		return ;
	}
	if (PAPI == PAPI_ON)
	{
		PAPI_get_info (fun_name, 0, PAPI_THREAD);
		Event.starttime = gettime ();
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
	remove_etask (task);
	current_task = old_task;

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
	TaskInfo old_task;

	Record_Event Event = Event_init ();										//初始化
	Event.event_name = "GOMP_parallel_start";								//获取函数名
	Event.eid = 200;
	Event.type = NONE;
	Event.omp_rank = get_thread_num ();										//获取线程编号
	Event.omp_level = get_level ();
	Event.p_rank = omp_get_ancestor_thread_num (get_level () - 1);

	old_task = current_task;

	//If current task is not exist, create a new task
	if (current_task.flag == 0)
	{
		current_task = create_itask ();
		Event.task_state_start = TASK_CREATE;
	}
	else
		Event.task_state_start = TASK_SUSPEND;

	create_team (current_task);
	
	Event.p_task_id_start = current_task.task_parent_id;
	Event.task_id_start = current_task.task_id;

	/*dlsym函数返回 GOMP_parallel_start 在动态链接库中的下一个地址，供调用使用*/
	GOMP_parallel_start_real = (void(*)(void*,void*,unsigned))dlsym (RTLD_NEXT, "GOMP_parallel_start");
	if (GOMP_parallel_start_real != NULL)
	{	
		par_uf = (void(*)(void*))p1;										//调用子函数的包装函数
		par_uf_id++;
		
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
	
	if (old_task.flag == 0)
		Event.task_state_end = TASK_START;
	else
		Event.task_state_end = TASK_RESUME;
	Record (&Event, OMPI_TRACE);
}
/*
	指导语句:	#pragma omp parallel
	结构功能:	parallel结束函数
	函数功能:	结束一个parallel并行结构
*/
void GOMP_parallel_end (void)
{
	TaskInfo old_task = current_task;
	Record_Event Event = Event_init ();
	Event.event_name = "GOMP_parallel_end";
	Event.eid = 201;
	Event.type = NONE;
	Event.omp_rank = get_thread_num ();
	Event.omp_level = get_level ();
	Event.p_rank = omp_get_ancestor_thread_num (get_level () - 1);

	Event.p_task_id_start = current_task.task_parent_id;
	Event.task_id_start = current_task.task_id;
	Event.task_state_start = TASK_WAIT;

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

	current_task = old_task;
	Event.p_task_id_end = current_task.task_parent_id;
	Event.task_id_end = current_task.task_id;
	Event.task_state_end = TASK_END;

	remove_team ();
	current_task = get_current_task ();

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
	TaskInfo old_task;
	Record_Event Event = Event_init ();
	Event.event_name = "GOMP_barrier";
	Event.eid = 202;
	Event.type = NONE;
	Event.omp_rank = get_thread_num ();
	Event.omp_level = get_level ();
	Event.p_rank = omp_get_ancestor_thread_num (get_level () - 1);

	old_task = current_task;
	if (current_task.flag == 1)
	{
		Event.p_task_id_start = current_task.task_parent_id;
		Event.task_id_start = current_task.task_id;
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

	current_task = old_task;

	if (current_task.flag == 1)
	{
		Event.p_task_id_end = current_task.task_parent_id;
		Event.task_id_end = current_task.task_id;
		Event.task_state_end = TASK_RESUME;
	}

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
	TaskInfo old_task;

	Record_Event Event = Event_init ();
	Event.event_name = "GOMP_parallel_loop_static_start";
	Event.eid = 209;
	Event.type = NONE;
	Event.omp_rank = get_thread_num ();
	Event.omp_level = get_level ();
	Event.p_rank = omp_get_ancestor_thread_num (get_level () - 1);

	old_task = current_task;

	if (current_task.flag == 0)
	{
		current_task = create_itask ();
		Event.task_state_start = TASK_CREATE;
	}
	else
		Event.task_state_start = TASK_SUSPEND;

	create_team (current_task);

	Event.p_task_id_start = current_task.task_parent_id;
	Event.task_id_start = current_task.task_id;

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

	Event.p_task_id_end = current_task.task_parent_id;
	Event.task_id_end = current_task.task_id;

	if (old_task.flag == 0)
		Event.task_state_end = TASK_START;
	else
		Event.task_state_end = TASK_RESUME;
	Record (&Event, OMPI_TRACE);
}
/*
	指导语句:	#pragma omp for
	结构功能:	for预初始化函数（有parallel时）
	函数功能:	预初始化一个任务共享结构
*/
void GOMP_parallel_loop_runtime_start (void *p1, void *p2, unsigned p3, long p4, long p5, long p6, long p7)
{
	TaskInfo old_task;
	Record_Event Event = Event_init ();
	Event.event_name = "GOMP_parallel_loop_runtime_start";
	Event.eid = 210;
	Event.type = NONE;
	Event.omp_rank = get_thread_num ();
	Event.omp_level = get_level ();
	Event.p_rank = omp_get_ancestor_thread_num (get_level () - 1);

	old_task = current_task;

	//If current task is not exist, create a new task
	if (current_task.flag == 0)
	{
		current_task = create_itask ();
		Event.task_state_start = TASK_CREATE;
	}
	else
		Event.task_state_start = TASK_SUSPEND;

	create_team (current_task);
	
	Event.p_task_id_start = current_task.task_parent_id;
	Event.task_id_start = current_task.task_id;

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

	Event.p_task_id_end = current_task.task_parent_id;
	Event.task_id_end = current_task.task_id;
	
	if (old_task.flag == 0)
		Event.task_state_end = TASK_START;
	else
		Event.task_state_end = TASK_RESUME;

	Record(&Event, OMPI_TRACE);
}
/*
	指导语句:	#pragma omp for
	结构功能:	for预初始化函数（有parallel时）
	函数功能:	预初始化一个任务共享结构
*/
void GOMP_parallel_loop_dynamic_start (void *p1, void *p2, unsigned p3, long p4, long p5, long p6, long p7)
{
	TaskInfo old_task;
	Record_Event Event = Event_init ();
	Event.event_name = "GOMP_parallel_loop_dynamic_start";
	Event.eid = 211;
	Event.type = NONE;
	Event.omp_rank = get_thread_num ();
	Event.omp_level = get_level ();
	Event.p_rank = omp_get_ancestor_thread_num (get_level () - 1);
	
	old_task = current_task;

	//If current task is not exist, create a new task
	if (current_task.flag == 0)
	{
		current_task = create_itask ();
		Event.task_state_start = TASK_CREATE;
	}
	else
		Event.task_state_start = TASK_SUSPEND;

	create_team (current_task);
	
	Event.p_task_id_start = current_task.task_parent_id;
	Event.task_id_start = current_task.task_id;

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

	Event.p_task_id_end = current_task.task_parent_id;
	Event.task_id_end = current_task.task_id;
	
	if (old_task.flag == 0)
		Event.task_state_end = TASK_START;
	else
		Event.task_state_end = TASK_RESUME;

	Record (&Event, OMPI_TRACE);
}
/*
	指导语句:	#pragma omp for
	结构功能:	for预初始化函数（有parallel时）
	函数功能:	预初始化一个任务共享结构
*/
void GOMP_parallel_loop_guided_start(void *p1, void *p2, unsigned p3, long p4, long p5, long p6, long p7)
{
	TaskInfo old_task;
	Record_Event Event = Event_init ();
	Event.event_name = "GOMP_parallel_loop_guided_start";
	Event.eid = 212;
	Event.type = NONE;
	Event.omp_rank = get_thread_num ();
	Event.omp_level = get_level ();
	Event.p_rank = omp_get_ancestor_thread_num (get_level () - 1);
	
	old_task = current_task;

	//If current task is not exist, create a new task
	if (current_task.flag == 0)
	{
		current_task = create_itask ();
		Event.task_state_start = TASK_CREATE;
	}
	else
		Event.task_state_start = TASK_SUSPEND;

	create_team (current_task);
	
	Event.p_task_id_start = current_task.task_parent_id;
	Event.task_id_start = current_task.task_id;

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

	Event.p_task_id_end = current_task.task_parent_id;
	Event.task_id_end = current_task.task_id;
	
	if (old_task.flag == 0)
		Event.task_state_end = TASK_START;
	else
		Event.task_state_end = TASK_RESUME;

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

	Event.p_task_id_start = current_task.task_parent_id;
	Event.task_id_start = current_task.task_id;
	Event.task_state_start= TASK_END;

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

	if (res == 1)																//Create a new task for this thread
	{
		current_task = create_itask ();
		Event.p_task_id_end = current_task.task_parent_id;
		Event.task_id_end= current_task.task_id;
		Event.task_state_end = TASK_CREATE;
	}
	else
	{
		current_task = get_current_task ();
		if (current_task.flag == 1)
		{
			Event.p_task_id_end = current_task.task_parent_id;
			Event.task_id_end= current_task.task_id;
			Event.task_state_end = TASK_RESUME;
		}
	}

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

	Event.p_task_id_start = current_task.task_parent_id;
	Event.task_id_start = current_task.task_id;
	Event.task_state_start= TASK_END;

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

	if (res == 1)																//Create a new task for this thread
	{
		current_task = create_itask ();
		Event.p_task_id_end = current_task.task_parent_id;
		Event.task_id_end= current_task.task_id;
		Event.task_state_end = TASK_CREATE;
	}
	else
	{
		current_task = get_current_task ();
		if (current_task.flag == 1)
		{
			Event.p_task_id_end = current_task.task_parent_id;
			Event.task_id_end= current_task.task_id;
			Event.task_state_end = TASK_RESUME;
		}
	}

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

	Event.p_task_id_start = current_task.task_parent_id;
	Event.task_id_start = current_task.task_id;
	Event.task_state_start= TASK_END;

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

	if (res == 1)																//Create a new task for this thread
	{
		current_task = create_itask ();
		Event.p_task_id_end = current_task.task_parent_id;
		Event.task_id_end= current_task.task_id;
		Event.task_state_end = TASK_CREATE;
	}
	else
	{
		current_task = get_current_task ();
		if (current_task.flag == 1)
		{
			Event.p_task_id_end = current_task.task_parent_id;
			Event.task_id_end= current_task.task_id;
			Event.task_state_end = TASK_RESUME;
		}
	}

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

	Event.p_task_id_start = current_task.task_parent_id;
	Event.task_id_start = current_task.task_id;
	Event.task_state_start= TASK_END;

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

	if (res == 1)																//Create a new task for this thread
	{
		current_task = create_itask ();
		Event.p_task_id_end = current_task.task_parent_id;
		Event.task_id_end= current_task.task_id;
		Event.task_state_end = TASK_CREATE;
	}
	else
	{
		current_task = get_current_task ();
		if (current_task.flag == 1)
		{
			Event.p_task_id_end = current_task.task_parent_id;
			Event.task_id_end= current_task.task_id;
			Event.task_state_end = TASK_RESUME;
		}
	}

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
	TaskInfo old_task;
	Record_Event Event = Event_init ();
	Event.event_name = "GOMP_loop_end";
	Event.eid = 221;
	Event.type = NONE;
	Event.omp_rank = get_thread_num ();
	Event.omp_level = get_level ();
	Event.p_rank = omp_get_ancestor_thread_num (get_level () - 1);

	old_task = current_task;
	if (current_task.flag == 1)
	{
		Event.p_task_id_start = current_task.task_parent_id;
		Event.task_id_start = current_task.task_id;
		Event.task_state_start = TASK_WAIT;
	}

	GOMP_loop_end_real=(void(*)(void)) dlsym (RTLD_NEXT, "GOMP_loop_end");
	if (GOMP_loop_end_real != NULL)
	{
		Event.starttime=gettime();
		GOMP_loop_end_real();
		Event.endtime=gettime();
	}
	else
	{
		printf_d("GOMP_loop_end is not hooked! exiting!!\n");
	}

	current_task = old_task;
	Event.p_task_id_end = current_task.task_parent_id;
	Event.task_id_end = current_task.task_id;
	Event.task_state_end = TASK_END;

	remove_team ();
	current_task = get_current_task ();

	Record(&Event, OMPI_TRACE);
}
/*
	指导语句:	#pragma omp parallel for
	结构功能:	for结束函数
	函数功能:	结束一个任务共享结构,不同步所有线程
*/
void GOMP_loop_end_nowait (void)
{
	TaskInfo old_task;
	Record_Event Event = Event_init ();
	Event.event_name = "GOMP_loop_end_nowait";
	Event.eid = 222;
	Event.type = NONE;
	Event.omp_rank = get_thread_num ();
	Event.omp_level = get_level ();
	Event.p_rank = omp_get_ancestor_thread_num (get_level () - 1);

	old_task = current_task;
	if (current_task.flag == 1)
	{
		Event.p_task_id_start = current_task.task_parent_id;
		Event.task_id_start = current_task.task_id;
		Event.task_state_start = TASK_WAIT;
	}

	GOMP_loop_end_nowait_real=(void(*)(void)) dlsym (RTLD_NEXT, "GOMP_loop_end_nowait");
	if (GOMP_loop_end_nowait_real != NULL)
	{
		Event.starttime=gettime();
		GOMP_loop_end_nowait_real();
		Event.endtime=gettime();
	}
	else
	{
		printf_d("GOMP_loop_end_nowait is not hooked! exiting!!\n");
	}

	current_task = old_task;
	Event.p_task_id_end = current_task.task_parent_id;
	Event.task_id_end = current_task.task_id;
	Event.task_state_end = TASK_END;

	remove_team ();
	current_task = get_current_task ();

	Record(&Event, OMPI_TRACE);
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
	Record_Event Event = Event_init ();
	Event.event_name = "GOMP_task";
	Event.eid = 229;
	Event.type = NONE;
	Event.omp_rank = get_thread_num ();
	Event.omp_level = get_level ();
	Event.p_rank = omp_get_ancestor_thread_num (get_level () - 1);

	if (current_task.flag == 1)
	{
		Event.p_task_id_start = current_task.task_parent_id;
		Event.task_id_start = current_task.task_id;
		Event.task_state_start = TASK_CREATE;
	}

	create_etask ();

	GOMP_task_real = (void(*)(void *,void *,void *,long,long,_Bool,unsigned))dlsym(RTLD_NEXT,"GOMP_task");

	if(GOMP_task_real != NULL)
	{
		task_uf = (void(*)(void*))p1;
		task_uf_id++;

		Event.starttime = gettime();
		GOMP_task_real (callme_task, p2, p3, p4, p5, p6, p7);
		Event.endtime = gettime();
	}

	if (current_task.flag == 1)
	{
		Event.p_task_id_end = current_task.task_parent_id;
		Event.task_id_end = current_task.task_id;
		Event.task_state_end = TASK_RESUME;
	}

	Record(&Event, OMPI_TRACE);
}

void GOMP_taskwait (void)
{
	TaskInfo old_task;
	Record_Event Event = Event_init ();
	Event.event_name = "GOMP_taskwait";
	Event.eid = 228;
	Event.type = NONE;
	Event.omp_rank = get_thread_num ();
	Event.omp_level = get_level ();
	Event.p_rank = omp_get_ancestor_thread_num (get_level () - 1);

	old_task = current_task;
	if (current_task.flag == 1)
	{
		Event.p_task_id_start = current_task.task_parent_id;
		Event.task_id_start = current_task.task_id;
		Event.task_state_start = TASK_SUSPEND;
	}

	GOMP_taskwait_real = (void(*)(void))dlsym(RTLD_NEXT,"GOMP_taskwait");

	if(GOMP_taskwait_real != NULL)
	{
		Event.starttime = gettime();
		GOMP_taskwait_real ();
		Event.endtime = gettime ();
	}

	current_task = old_task;
	if (current_task.flag == 1)
	{
		Event.p_task_id_end = current_task.task_parent_id;
		Event.task_id_end = current_task.task_id;
		Event.task_state_end = TASK_RESUME;
	}

	Record (&Event, OMPI_TRACE);
}
