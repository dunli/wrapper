#define _GNU_SOURCE
#include "wrapper.h"
#include <cuda_runtime_api.h>
#include <dlfcn.h>
#include <stdio.h>
#include <stdlib.h>

/*CPUTI*/
//#include "cupti.h"
//#include "cuda.h"
//#include "cuda_runtime.h"
//extern void initTrace();


#define SYNC	1
#define ASYNC	2 

extern Record_Event Event_init ();
extern int Record ();
extern long long unsigned gettime ();

typedef struct kernelName_t
{
	const char* host;
	const char* dev;
	struct kernelName_t *next;
} kernelName;

/*static void (*__cudaRegisterFunction_real) (void **, const char *, char *, const char *, int, uint3 *, uint3 *, dim3 *, dim3 *, int *) = NULL;*/


/*CUDA Device Management*/
static cudaError_t (*cudaGetDeviceCount_real) (int *) = NULL;
static cudaError_t (*cudaDeviceReset_real) (void) = NULL;
static cudaError_t (*cudaGetDeviceProperties_real) (struct cudaDeviceProp *, int) = NULL;
static cudaError_t (*cudaChooseDevice_real) (int *, const struct cudaDeviceProp *) = NULL;
static cudaError_t (*cudaSetDevice_real) (int) = NULL;
static cudaError_t (*cudaGetDevice_real) (int *) = NULL;
static cudaError_t (*cudaSetValidDevices_real) (int *, int) = NULL;
static cudaError_t (*cudaSetDeviceFlags_real) (unsigned int) = NULL;

/*CUDA Thread Management*/
static cudaError_t (*cudaThreadSynchronize_real) (void) = NULL;
static cudaError_t (*cudaThreadExit_real) (void) = NULL;

/*CUDA StreamManagement*/
static cudaError_t (*cudaStreamCreate_real) (cudaStream_t *) = NULL;
static cudaError_t (*cudaStreamDestroy_real) (cudaStream_t) = NULL;
static cudaError_t (*cudaStreamWaitEvent_real) (cudaStream_t, cudaEvent_t, unsigned int) = NULL;
static cudaError_t (*cudaStreamSynchronize_real) (cudaStream_t) = NULL;
static cudaError_t (*cudaStreamQuery_real) (cudaStream_t) = NULL;

/*CUDA EventManagement*/
static cudaError_t (*cudaEventCreate_real) (cudaEvent_t *) = NULL;
static cudaError_t (*cudaEventCreateWithFlags_real) (cudaEvent_t *, unsigned int) = NULL;
static cudaError_t (*cudaEventRecord_real) (cudaEvent_t, cudaStream_t) = NULL;
static cudaError_t (*cudaEventQuery_nosync_real) (cudaEvent_t) = NULL;
static cudaError_t (*cudaEventQuery_real) (cudaEvent_t) = NULL;
static cudaError_t (*cudaEventSynchronize_real) (cudaEvent_t) = NULL;
static cudaError_t (*cudaEventDestroy_real) (cudaEvent_t) = NULL;
static cudaError_t (*cudaEventElapsedTime_real) (float *, cudaEvent_t, cudaEvent_t) = NULL;

/*CUDA Error Handling*/
static cudaError_t (*cudaGetLastError_real) (void) = NULL;
static cudaError_t (*cudaPeekAtLastError_real) (void) = NULL;
static const char *(*cudaGetErrorString_real) (cudaError_t) = NULL;

/*CUDA Memory Management*/
static cudaError_t (*cudaMemcpy_real) (void*,const void*,size_t,enum cudaMemcpyKind) = NULL;
static cudaError_t (*cudaMalloc_real) (void **, size_t) = NULL;
static cudaError_t (*cudaMallocHost_real) (void **, size_t) = NULL;
static cudaError_t (*cudaMallocPitch_real) (void **, size_t * ,size_t, size_t) = NULL;
static cudaError_t (*cudaMallocArray_real) (struct cudaArray **, const struct cudaChannelFormatDesc *, size_t, size_t, unsigned int) = NULL;
static cudaError_t (*cudaFree_real) (void *) = NULL;
static cudaError_t (*cudaFreeHost_real) (void *) = NULL;
static cudaError_t (*cudaFreeArray_real) (struct cudaArray *) = NULL;
static cudaError_t (*cudaHostAlloc_real) (void **, size_t, unsigned int) = NULL;
static cudaError_t (*cudaHostGetDevicePointer_real) (void **, void *, unsigned int) = NULL;
static cudaError_t (*cudaHostGetFlags_real) (unsigned int *, void *) = NULL;
static cudaError_t (*cudaMalloc3D_real) (struct cudaPitchedPtr *, struct cudaExtent) = NULL;
static cudaError_t (*cudaMalloc3DArray_real) (struct cudaArray **, const struct cudaChannelFormatDesc *, struct cudaExtent, unsigned int) = NULL;
static cudaError_t (*cudaMemcpy3D_real) (const struct cudaMemcpy3DParms *) = NULL;
static cudaError_t (*cudaMemcpy3DAsync_real) (const struct cudaMemcpy3DParms *, cudaStream_t) = NULL;
static cudaError_t (*cudaMemGetInfo_real) (size_t *, size_t *) = NULL;
static cudaError_t (*cudaMemcpyToArray_real) (struct cudaArray *, size_t, size_t, const void *, size_t, enum cudaMemcpyKind) = NULL;
static cudaError_t (*cudaMemcpyFromArray_real) (void *, const struct cudaArray *, size_t, size_t, size_t, enum cudaMemcpyKind) = NULL;
static cudaError_t (*cudaMemcpyArrayToArray_real) (struct cudaArray *, size_t, size_t, const struct cudaArray *, size_t, size_t, size_t, enum cudaMemcpyKind) = NULL;
static cudaError_t (*cudaMemcpy2D_real) (void *, size_t, const void *, size_t, size_t, size_t, enum cudaMemcpyKind) = NULL;
static cudaError_t (*cudaMemcpy2DToArray_real) (struct cudaArray *, size_t, size_t, const void *, size_t, size_t, size_t, enum cudaMemcpyKind) = NULL;
static cudaError_t (*cudaMemcpy2DFromArray_real) (void *, size_t, const struct cudaArray *, size_t, size_t, size_t, size_t, enum cudaMemcpyKind) = NULL;
static cudaError_t (*cudaMemcpy2DArrayToArray_real) (struct cudaArray *, size_t, size_t, const struct cudaArray *, size_t, size_t, size_t, size_t, enum cudaMemcpyKind) = NULL;
static cudaError_t (*cudaMemcpyToSymbol_real) (const char *, const void *, size_t, size_t, enum cudaMemcpyKind) = NULL;
static cudaError_t (*cudaMemcpyFromSymbol_real) (void *, const char *, size_t, size_t, enum cudaMemcpyKind) = NULL;
static cudaError_t (*cudaMemcpyAsync_real) (void *, const void *, size_t, enum cudaMemcpyKind, cudaStream_t) = NULL;
static cudaError_t (*cudaMemcpyToArrayAsync_real) (struct cudaArray *, size_t, size_t, const void *, size_t, enum cudaMemcpyKind, cudaStream_t) = NULL;
static cudaError_t (*cudaMemcpyFromArrayAsync_real) (void *, const struct cudaArray *, size_t, size_t, size_t, enum cudaMemcpyKind, cudaStream_t) = NULL;
static cudaError_t (*cudaMemcpy2DAsync_real) (void *, size_t, const void *, size_t, size_t, size_t, enum cudaMemcpyKind, cudaStream_t) = NULL;
static cudaError_t (*cudaMemcpy2DToArrayAsync_real) (struct cudaArray *, size_t, size_t, const void *, size_t, size_t, size_t, enum cudaMemcpyKind, cudaStream_t) = NULL;
static cudaError_t (*cudaMemcpy2DFromArrayAsync_real) (void *, size_t, const struct cudaArray *, size_t, size_t, size_t, size_t, enum cudaMemcpyKind, cudaStream_t) = NULL;
static cudaError_t (*cudaMemcpyToSymbolAsync_real) (const char *, const void *, size_t, size_t, enum cudaMemcpyKind, cudaStream_t) = NULL;
static cudaError_t (*cudaMemcpyFromSymbolAsync_real) (void *, const char *, size_t, size_t, enum cudaMemcpyKind, cudaStream_t) = NULL;
static cudaError_t (*cudaMemset_real) (void *, int, size_t) = NULL;
static cudaError_t (*cudaMemset2D_real) (void *, size_t, int, size_t, size_t) = NULL;
static cudaError_t (*cudaMemset3D_real) (struct cudaPitchedPtr, int, struct cudaExtent) = NULL;
static cudaError_t (*cudaMemsetAsync_real) (void *, int, size_t, cudaStream_t) = NULL;
static cudaError_t (*cudaMemset2DAsync_real) (void *, size_t, int, size_t, size_t,cudaStream_t) = NULL;
static cudaError_t (*cudaMemset3DAsync_real) (struct cudaPitchedPtr, int, struct cudaExtent, cudaStream_t) = NULL;
static cudaError_t (*cudaGetSymbolAddress_real) (void **, const char *) = NULL;
static cudaError_t (*cudaGetSymbolSize_real) (size_t *, const char *) = NULL;


/*CUDA ExecutionControl*/
static cudaError_t (*cudaConfigureCall_real) (dim3, dim3, size_t, cudaStream_t) = NULL;
static cudaError_t (*cudaSetupArgument_real) (const void *, size_t, size_t) = NULL;
static cudaError_t (*cudaLaunch_real) (const char *) = NULL;
static cudaError_t (*cudaFuncGetAttributes_real) (struct cudaFuncAttributes *, const char *) = NULL;
static cudaError_t (*cudaSetDoubleForDevice_real) (double *) = NULL;
static cudaError_t (*cudaSetDoubleForHost_real) (double *) = NULL;

kernelName *kernelNamesHead = NULL;


/*************************************************
/*            CUDA Device Management            */
/************************************************/
cudaError_t cudaGetDeviceCount (int *a1)
{
	cudaError_t retVal;
	Record_Event Event = Event_init ();
	Event.event_name = "cudaGetDeviceCount";
	Event.eid = 300;
	Event.type = NONE;
	cudaGetDeviceCount_real = (cudaError_t (*)(int *)) dlsym (RTLD_NEXT, "cudaGetDeviceCount");

	Event.starttime = gettime ();
	retVal = (*cudaGetDeviceCount_real) (a1);;
	Event.endtime = gettime ();

	Record (&Event, CMPI_TRACE);
	return retVal;
}

cudaError_t cudaDeviceReset ()
{
	cudaError_t retVal;
	Record_Event Event = Event_init ();
	Event.event_name = "cudaDeviceReset";
	Event.eid = 301;
	Event.type = NONE;
	cudaDeviceReset_real = (cudaError_t (*)(void)) dlsym (RTLD_NEXT, "cudaDeviceReset"); 

	//printf ("this is the start of cudaDeviceReset() --------->\n");
	Event.starttime = gettime ();
	retVal = (*cudaDeviceReset_real) ();
	Event.endtime = gettime ();
	
	Record (&Event, CMPI_TRACE);
	return retVal;
}

cudaError_t cudaGetDeviceProperties (struct cudaDeviceProp * a1, int a2)
{
	cudaError_t retVal;
	Record_Event Event = Event_init ();
	Event.event_name = "cudaGetDeviceProperties";
	Event.eid = 302;
	Event.type = NONE;

	cudaGetDeviceProperties_real = (cudaError_t (*)(struct cudaDeviceProp *, int)) dlsym (RTLD_NEXT, "cudaGetDeviceProperties");
	Event.starttime = gettime ();
	retVal = cudaGetDeviceProperties_real (a1, a2);
	Event.endtime = gettime ();

	Record (&Event, CMPI_TRACE);
	return retVal;
}

cudaError_t cudaChooseDevice (int *a1, const struct cudaDeviceProp * a2)
{
	cudaError_t retVal;
	Record_Event Event = Event_init ();
	Event.event_name = "cudaChooseDevice";
	Event.eid = 303;
	Event.type = NONE;
	cudaChooseDevice_real = (cudaError_t (*)(int *,const struct cudaDeviceProp *)) dlsym (RTLD_NEXT, "cudaChooseDevice");

	Event.starttime = gettime ();
	retVal = cudaChooseDevice_real (a1, a2);
	Event.endtime = gettime ();

	Record (&Event, CMPI_TRACE);
	return retVal;
}

cudaError_t cudaSetDevice (int a1)
{
	cudaError_t retVal;
	Record_Event Event = Event_init ();
	Event.event_name = "cudaSetDevice";
	Event.eid = 304;
	Event.type = NONE;

	cudaSetDevice_real = (cudaError_t (*)(int)) dlsym (RTLD_NEXT, "cudaSetDevice");
	Event.starttime = gettime ();
	retVal = cudaSetDevice_real (a1);
	Event.endtime = gettime ();

	Record (&Event, CMPI_TRACE);
	return retVal;
}

cudaError_t cudaGetDevice (int *a1)
{
	cudaError_t retVal;
	Record_Event Event = Event_init ();
	Event.event_name = "cudaGetDevice";
	Event.eid = 305;
	Event.type = NONE;

	cudaGetDevice_real = (cudaError_t (*)(int *)) dlsym (RTLD_NEXT, "cudaGetDevice");
	Event.starttime = gettime ();
	retVal = cudaGetDevice_real (a1);
	Event.endtime = gettime ();

	Record (&Event, CMPI_TRACE);
	return retVal;
}

cudaError_t cudaSetValidDevices (int *a1, int a2)
{
	cudaError_t retVal;
	Record_Event Event = Event_init ();
	Event.event_name = "cudaSetValidDevices";
	Event.eid = 306;
	Event.type = NONE;

	cudaSetValidDevices_real = (cudaError_t (*)(int *, int)) dlsym (RTLD_NEXT, "cudaSetValidDevices");
	Event.starttime = gettime();
	retVal = cudaSetValidDevices_real (a1, a2);
	Event.endtime = gettime ();

	Record (&Event, CMPI_TRACE);
	return retVal;
}

cudaError_t cudaSetDeviceFlags (unsigned int a1)
{
	cudaError_t retVal;
	Record_Event Event = Event_init ();
	Event.event_name = "cudaSetDeviceFlags";
	Event.eid = 307;
	Event.type = NONE;

	cudaSetDeviceFlags_real = (cudaError_t (*)(unsigned int)) dlsym (RTLD_NEXT, "cudaSetDeviceFlags");
	Event.starttime = gettime ();
	retVal = cudaSetDeviceFlags_real (a1);
	Event.endtime = gettime ();

	Record (&Event, CMPI_TRACE);
	return retVal;
}

/*************************************************
/*            CUDA Thread Management            */
/************************************************/
cudaError_t cudaThreadExit ()
{
	cudaError_t retVal;
	Record_Event Event = Event_init ();
	Event.event_name = "cudaThreadExit";
	Event.eid = 308;
	Event.type = NONE;
	cudaThreadExit_real = (cudaError_t (*)(void)) dlsym (RTLD_NEXT, "cudaThreadExit");

	Event.starttime = gettime ();
	retVal = (*cudaThreadExit_real) ();
	Event.endtime = gettime ();

	Record (&Event, CMPI_TRACE);
	return retVal;
}

cudaError_t cudaThreadSynchronize ()
{
	cudaError_t retVal;
	Record_Event Event = Event_init ();
	Event.event_name = "cudaThreadSynchronize";
	Event.eid = 309;
	Event.type = NONE;

	cudaThreadSynchronize_real = (cudaError_t(*)(void)) dlsym (RTLD_NEXT, "cudaThreadSynchronize");
	Event.starttime = gettime ();
	retVal =  cudaThreadSynchronize_real ();
	Event.endtime = gettime ();

	Record (&Event, CMPI_TRACE);
	return retVal;
}


/*************************************************
/*            CUDA StreamManagement             */
/************************************************/
cudaError_t cudaStreamCreate (cudaStream_t *a1)
{
	cudaError_t retVal;
	Record_Event Event = Event_init ();
	Event.event_name = "cudaStreamCreate";
	Event.eid = 310;
	Event.type = NONE;

	cudaStreamCreate_real = (cudaError_t (*)(cudaStream_t *)) dlsym (RTLD_NEXT, "cudaStreamCreate");
	Event.starttime = gettime ();
	retVal = cudaStreamCreate_real (a1);
	Event.endtime = gettime ();

	Record (&Event, CMPI_TRACE);
	return retVal;
}

cudaError_t cudaStreamDestroy (cudaStream_t a1)
{
	cudaError_t retVal;
	Record_Event Event = Event_init ();
	Event.event_name = "cudaStreamDestroy";
	Event.eid = 311;
	Event.type = NONE;

	cudaStreamDestroy_real = (cudaError_t (*)(cudaStream_t)) dlsym (RTLD_NEXT, "cudaStreamDestroy");
	Event.starttime = gettime ();
	retVal = cudaStreamDestroy_real (a1);
	Event.endtime = gettime ();

	Record (&Event, CMPI_TRACE);
	return retVal;
}

cudaError_t cudaStreamWaitEvent (cudaStream_t a1, cudaEvent_t a2, unsigned int a3)
{
	cudaError_t retVal;
	Record_Event Event = Event_init ();
	Event.event_name = "cudaStreamWaitEvent";
	Event.eid = 312;
	Event.type = NONE;

	cudaStreamWaitEvent_real = (cudaError_t (*)(cudaStream_t, cudaEvent_t, unsigned int)) dlsym (RTLD_NEXT, "cudaStreamWaitEvent");
	Event.starttime = gettime ();
	retVal = cudaStreamWaitEvent_real (a1, a2, a3);
	Event.endtime = gettime ();

	Record (&Event, CMPI_TRACE);
	return retVal;
}

cudaError_t cudaStreamSynchronize (cudaStream_t a1)
{
	cudaError_t retVal;
	Record_Event Event = Event_init ();
	Event.event_name = "cudaStreamSynchronize";
	Event.eid = 313;
	Event.type = NONE;

	cudaStreamSynchronize_real = (cudaError_t (*)(cudaStream_t)) dlsym (RTLD_NEXT, "cudaStreamSynchronize");
	Event.starttime = gettime ();
	retVal = cudaStreamSynchronize_real (a1);
	Event.endtime = gettime ();

	Record (&Event, CMPI_TRACE);
	return retVal;
}

cudaError_t cudaStreamQuery (cudaStream_t a1)
{
	cudaError_t retVal;
	Record_Event Event = Event_init ();
	Event.event_name = "cudaStreamQuery";
	Event.eid = 314;
	Event.type = NONE;

	cudaStreamQuery_real = (cudaError_t (*)(cudaStream_t)) dlsym (RTLD_NEXT, "cudaStreamQuery");
	Event.starttime = gettime ();
	retVal = cudaStreamQuery_real (a1);
	Event.endtime = gettime ();

	Record (&Event, CMPI_TRACE);
	return retVal;
}

/*************************************************
/*            CUDA EventManagement             */
/************************************************/
cudaError_t cudaEventCreate (cudaEvent_t * a1)
{
	cudaError_t retVal;
	Record_Event Event = Event_init ();
	Event.event_name = "cudaEventCreate";
	Event.eid = 315;
	Event.type = NONE;

	cudaEventCreate_real = (cudaError_t (*)(cudaEvent_t *)) dlsym (RTLD_NEXT, "cudaEventCreate");
	Event.starttime = gettime ();
	retVal = cudaEventCreate_real (a1);
	Event.endtime = gettime ();

	Record (&Event, CMPI_TRACE);
	return retVal;
}

cudaError_t cudaEventCreateWithFlags (cudaEvent_t *a1, unsigned int a2)
{
	cudaError_t retVal;
	Record_Event Event = Event_init ();
	Event.event_name = "cudaEventCreateWithFlags";
	Event.eid = 316;
	Event.type = NONE;

	cudaEventCreateWithFlags_real = (cudaError_t (*)(cudaEvent_t *, unsigned int a2)) dlsym (RTLD_NEXT, "cudaEventCreateWithFlags");
	Event.starttime = gettime ();
	retVal = cudaEventCreateWithFlags_real (a1, a2);
	Event.endtime = gettime ();

	Record (&Event, CMPI_TRACE);
	return retVal;
}

cudaError_t cudaEventRecord (cudaEvent_t a1, cudaStream_t a2)
{
	cudaError_t retVal;
	Record_Event Event = Event_init ();
	Event.event_name = "cudaEventRecord";
	Event.eid = 317;
	Event.type = NONE;

	cudaEventRecord_real = (cudaError_t (*)(cudaEvent_t, cudaStream_t)) dlsym (RTLD_NEXT, "cudaEventRecord");
	Event.starttime = gettime ();
	retVal = cudaEventRecord_real (a1, a2);
	Event.endtime = gettime ();

	Record (&Event, CMPI_TRACE);
	return retVal;
}

cudaError_t cudaEventQuery_nosync (cudaEvent_t a1)
{
	cudaError_t retVal;
	Record_Event Event = Event_init ();
	Event.event_name = "cudaEventQuery_nosync";
	Event.eid = 318;
	Event.type = NONE;

	cudaEventQuery_nosync_real = (cudaError_t (*)(cudaEvent_t)) dlsym (RTLD_NEXT, "cudaEventQuery_nosync");
	Event.starttime = gettime ();
	retVal = cudaEventQuery_nosync_real (a1);
	Event.endtime = gettime ();

	Record (&Event, CMPI_TRACE);
	return retVal;
}

cudaError_t cudaEventQuery (cudaEvent_t a1)
{
	cudaError_t retVal;
	Record_Event Event = Event_init ();
	Event.event_name = "cudaEventQuery";
	Event.eid = 319;
	Event.type = NONE;

	cudaEventQuery_real = (cudaError_t (*)(cudaEvent_t)) dlsym (RTLD_NEXT, "cudaEventQuery");
	Event.starttime = gettime ();
	retVal = cudaEventQuery_real (a1);
	Event.endtime = gettime ();

	Record (&Event, CMPI_TRACE);
	return retVal;
}

cudaError_t cudaEventSynchronize (cudaEvent_t a1)
{
	cudaError_t retVal;
	Record_Event Event = Event_init ();
	Event.event_name = "cudaEventSynchronize";
	Event.eid = 320;
	Event.type = NONE;

	cudaEventSynchronize_real = (cudaError_t (*)(cudaEvent_t)) dlsym (RTLD_NEXT, "cudaEventSynchronize");
	Event.starttime = gettime ();
	retVal = cudaEventSynchronize_real (a1);
	Event.endtime = gettime ();

	Record (&Event, CMPI_TRACE);
	return retVal;
}

cudaError_t cudaEventDestroy (cudaEvent_t a1)
{
	cudaError_t retVal;
	Record_Event Event = Event_init ();
	Event.event_name = "cudaEventDestroy";
	Event.eid = 321;
	Event.type = NONE;
	
	cudaEventDestroy_real = (cudaError_t (*)(cudaEvent_t)) dlsym (RTLD_NEXT, "cudaEventDestroy");
	Event.starttime = gettime ();
	retVal = cudaEventDestroy_real (a1);
	Event.endtime = gettime ();

	Record (&Event, CMPI_TRACE);
	return retVal;
}

cudaError_t cudaEventElapsedTime (float *a1, cudaEvent_t a2, cudaEvent_t a3)
{
	cudaError_t retVal;
	Record_Event Event = Event_init ();
	Event.event_name = "cudaEventElapsedTime";
	Event.eid = 322;
	Event.type = NONE;

	cudaEventElapsedTime_real = (cudaError_t (*)(float *, cudaEvent_t, cudaEvent_t)) dlsym (RTLD_NEXT, "cudaEventElapsedTime");
	Event.starttime = gettime ();
	retVal = cudaEventElapsedTime_real (a1, a2, a3);
	Event.endtime = gettime ();

	Record (&Event, CMPI_TRACE);
	return retVal;
}
/*************************************************
/*            CUDA Error Handling               */
/************************************************/
cudaError_t cudaGetLastError ()
{
	cudaError_t retVal;
	Record_Event Event = Event_init ();
	Event.event_name = "cudaGetLastError";
	Event.eid = 323;
	Event.type = NONE;

	cudaGetLastError_real = (cudaError_t (*)(void)) dlsym (RTLD_NEXT, "cudaGetLastError");
	Event.starttime = gettime ();
	retVal = (*cudaGetLastError_real) ();
	Event.endtime = gettime ();

	Record (&Event, CMPI_TRACE);
	return retVal;
}

cudaError_t cudaPeekAtLastError ()
{
	cudaError_t retVal;
	Record_Event Event = Event_init ();
	Event.event_name = "cudaPeekAtLastError";
	Event.eid = 324;
	Event.type = NONE;

	cudaPeekAtLastError_real = (cudaError_t (*)(void)) dlsym (RTLD_NEXT, "cudaPeekAtLastError");
	Event.starttime = gettime ();
	retVal = (*cudaPeekAtLastError_real) ();
	Event.endtime = gettime ();

	Record (&Event, CMPI_TRACE);
	return retVal;
}

const char * cudaGetErrorString (cudaError_t a1)
{
	const char * retVal;
	Record_Event Event = Event_init ();
	Event.event_name = "cudaGetErrorString";
	Event.eid = 325;
	Event.type = NONE;

	cudaGetErrorString_real = (const char *(*)(cudaError_t)) dlsym (RTLD_NEXT, "cudaGetErrorString");
	Event.starttime = gettime ();
	retVal = (*cudaGetErrorString) (a1);
	Event.endtime = gettime ();

	Record (&Event, CMPI_TRACE);
	return retVal;
}


/*************************************************
/*            CUDA MemoryManagement             */
/************************************************/
cudaError_t cudaMemcpy (void * a1, const void * a2, size_t a3, enum cudaMemcpyKind a4)
{
	cudaError_t retVal;
	Record_Event Event = Event_init ();
	Event.event_name = "cudaMemcpy";
	Event.eid = 326;
	Event.type = CUDA_MEM;
	Event.memcpy_size = a3;
	Event.memcpy_type = a4;
	Event.memcpy_kind = SYNC;

	cudaMemcpy_real = (cudaError_t (*)(void*, const void*, size_t, enum cudaMemcpyKind)) dlsym(RTLD_NEXT, "cudaMemcpy");
	Event.starttime = gettime ();
	retVal = (*cudaMemcpy_real)( a1,  a2,  a3,  a4);
	Event.endtime = gettime ();

#ifdef DEBUG
	printf("[DEBUG]:\t%s\tlu\t%lu\n", 
		Event.event_name,
		Event.starttime,
		Event.endtime
		);
#endif

	Record (&Event, CMPI_TRACE);
	return retVal;
}

cudaError_t cudaMalloc (void ** a1, size_t a2)
{
	cudaError_t retVal;
	Record_Event Event = Event_init ();
	Event.event_name = "cudaMalloc";
	Event.eid = 327;
	Event.type = NONE;

	cudaMalloc_real = (cudaError_t (*)(void **, size_t)) dlsym (RTLD_NEXT, "cudaMalloc");
	Event.starttime = gettime ();
	retVal = cudaMalloc_real (a1, a2);
	Event.endtime = gettime ();

	Record (&Event, CMPI_TRACE);
	return retVal;
}

cudaError_t cudaMallocHost (void ** a1, size_t a2)
{
	cudaError_t retVal;
	Record_Event Event = Event_init ();
	Event.event_name = "cudaMallocHost";
	Event.eid = 328;
	Event.type = NONE;

	cudaMallocHost_real = (cudaError_t (*)(void **, size_t)) dlsym (RTLD_NEXT, "cudaMallocHost");
	Event.starttime = gettime ();
	retVal = cudaMallocHost_real (a1, a2);
	Event.endtime = gettime ();

	Record (&Event, CMPI_TRACE);
	return retVal;
}

cudaError_t cudaMallocPitch (void ** a1, size_t * a2, size_t a3, size_t a4)
{
	cudaError_t retVal;
	Record_Event Event = Event_init ();
	Event.event_name = "cudaMallocPitch";
	Event.eid = 329;
	Event.type = NONE;

	cudaMallocPitch_real = (cudaError_t (*)(void **, size_t *, size_t, size_t)) dlsym (RTLD_NEXT, "cudaMallocPitch");
	Event.starttime = gettime ();
	retVal = cudaMallocPitch_real (a1, a2, a3, a4);
	Event.endtime = gettime ();

	Record (&Event, CMPI_TRACE);
	return retVal;
}

cudaError_t cudaMallocArray (struct cudaArray ** a1, const struct cudaChannelFormatDesc * a2, size_t a3, size_t a4, unsigned int a5)
{
	cudaError_t retVal;
	Record_Event Event = Event_init ();
	Event.event_name = "cudaMallocArray";
	Event.eid = 330;
	Event.type = NONE;

	cudaMallocArray_real = (cudaError_t (*)(struct cudaArray **, const struct cudaChannelFormatDesc *, size_t, size_t, unsigned int)) dlsym (RTLD_NEXT, "cudaMallocArray");
	Event.starttime = gettime ();
	retVal = cudaMallocArray_real (a1, a2, a3 ,a4, a5);
	Event.endtime = gettime ();

	Record (&Event, CMPI_TRACE);
	return retVal;
}

cudaError_t cudaFree (void *a1)
{
	cudaError_t retVal;
	Record_Event Event = Event_init ();
	Event.event_name = "cudaFree";
	Event.eid = 331;
	Event.type = NONE;

	cudaFree_real = (cudaError_t (*)(void *)) dlsym (RTLD_NEXT, "cudaFree");
	Event.starttime = gettime ();
	retVal = cudaFree_real (a1);
	Event.endtime = gettime ();

	Record (&Event, CMPI_TRACE);
	return retVal;
}

cudaError_t cudaFreeHost (void *a1)
{
	cudaError_t retVal;
	Record_Event Event = Event_init ();
	Event.event_name = "cudaFreeHost";
	Event.eid = 332;
	Event.type = NONE;

	cudaFreeHost_real = (cudaError_t (*)(void *)) dlsym (RTLD_NEXT, "cudaFreeHost");
	Event.starttime = gettime ();
	retVal = cudaFreeHost_real (a1);
	Event.endtime = gettime ();

	Record (&Event, CMPI_TRACE);
	return retVal;
}

cudaError_t cudaFreeArray (struct cudaArray * a1)
{
	cudaError_t retVal;
	Record_Event Event = Event_init ();
	Event.event_name = "cudaFreeArray";
	Event.eid = 333;
	Event.type = NONE;

	cudaFreeArray_real = (cudaError_t (*)(struct cudaArray *)) dlsym (RTLD_NEXT, "cudaFreeArray");
	Event.starttime = gettime ();
	retVal = cudaFreeArray_real (a1);
	Event.endtime = gettime ();

	Record (&Event, CMPI_TRACE);
	return retVal;
}

cudaError_t cudaHostAlloc (void ** a1, size_t a2, unsigned int a3)
{
	cudaError_t retVal;
	Record_Event Event = Event_init ();
	Event.event_name = "cudaHostAlloc";
	Event.eid = 334;
	Event.type = NONE;

	cudaHostAlloc_real = (cudaError_t (*)(void **, size_t, unsigned int)) dlsym (RTLD_NEXT, "cudaHostAlloc");
	Event.starttime = gettime ();
	retVal = cudaHostAlloc_real (a1, a2, a3);
	Event.endtime = gettime ();

	Record (&Event, CMPI_TRACE);
	return retVal;
}

cudaError_t cudaHostGetDevicePointer (void ** a1, void * a2, unsigned int a3)
{
	cudaError_t retVal;
	Record_Event Event = Event_init ();
	Event.event_name = "cudaHostGetDevicePointer";
	Event.eid = 335;
	Event.type = NONE;

	cudaHostGetDevicePointer_real = (cudaError_t (*)(void **, void *, unsigned int)) dlsym (RTLD_NEXT, "cudaHostGetDevicePointer");
	Event.starttime = gettime ();
	retVal = cudaHostGetDevicePointer_real (a1, a2, a3);
	Event.endtime = gettime ();

	Record (&Event, CMPI_TRACE);
	return retVal;
}

cudaError_t cudaHostGetFlags (unsigned int * a1, void * a2)
{
	cudaError_t retVal;
	Record_Event Event = Event_init ();
	Event.event_name = "cudaHostGetFlags";
	Event.eid = 336;
	Event.type = NONE;

	cudaHostGetFlags_real =  (cudaError_t (*)(unsigned int *, void *)) dlsym (RTLD_NEXT, "cudaHostGetFlags");
	Event.starttime = gettime ();
	retVal =  cudaHostGetFlags_real (a1, a2);
	Event.endtime = gettime ();

	Record (&Event, CMPI_TRACE);
	return retVal;
}

cudaError_t cudaMalloc3D (struct cudaPitchedPtr * a1, struct cudaExtent a2)
{
	cudaError_t retVal;
	Record_Event Event = Event_init ();
	Event.event_name = "cudaMalloc3D";
	Event.eid = 337;
	Event.type = NONE;

	cudaMalloc3D_real = (cudaError_t (*)(struct cudaPitchedPtr *, struct cudaExtent)) dlsym (RTLD_NEXT, "cudaMalloc3D");
	Event.starttime = gettime ();
	retVal = cudaMalloc3D_real (a1, a2);
	Event.endtime = gettime ();

	Record (&Event, CMPI_TRACE);
	return retVal;
}

cudaError_t cudaMalloc3DArray (struct cudaArray ** a1, const struct cudaChannelFormatDesc * a2, struct cudaExtent a3, unsigned int a4)
{
	cudaError_t retVal;
	Record_Event Event = Event_init ();
	Event.event_name = "cudaMalloc3DArray";
	Event.eid = 338;
	Event.type = NONE;

	cudaMalloc3DArray_real = (cudaError_t (*)(struct cudaArray **, const struct cudaChannelFormatDesc *, struct cudaExtent, unsigned int)) dlsym (RTLD_NEXT, "cudaMalloc3DArray");
	Event.starttime = gettime ();
	retVal =  cudaMalloc3DArray_real (a1, a2, a3, a4);
	Event.endtime = gettime ();

	Record (&Event, CMPI_TRACE);
	return retVal;
}

cudaError_t cudaMemcpy3D (const struct cudaMemcpy3DParms * a1)
{
	cudaError_t retVal;
	Record_Event Event = Event_init ();
	Event.event_name = "cudaMemcpy3D";
	Event.eid = 339;
	Event.type = CUDA_MEM;
	/*cudaArray大小无法获得*/
	//Event.memcpy_type = a1.kind;
	Event.memcpy_kind = SYNC;

	cudaMemcpy3D_real = (cudaError_t (*)(const struct cudaMemcpy3DParms *)) dlsym (RTLD_NEXT, "cudaMemcpy3D");
	Event.starttime = gettime ();
	retVal =  cudaMemcpy3D_real (a1);
	Event.endtime = gettime ();

	Record (&Event, CMPI_TRACE);
	return retVal;
}

cudaError_t cudaMemcpy3DAsync (const struct cudaMemcpy3DParms * a1, cudaStream_t a2)
{
	cudaError_t retVal;
	Record_Event Event = Event_init ();
	Event.event_name = "cudaMemcpy3DAsync";
	Event.eid = 340;
	Event.type = CUDA_MEM;
	/*cudaArray大小无法获得*/
	//Event.cuda_stream = a2;
	//Event.memcpy_type = a1.kind;
	Event.memcpy_kind = ASYNC;

	cudaMemcpy3DAsync_real = (cudaError_t (*)(const struct cudaMemcpy3DParms *, cudaStream_t)) dlsym (RTLD_NEXT, "cudaMemcpy3DAsync");
	Event.starttime = gettime ();
	retVal = cudaMemcpy3DAsync_real (a1, a2);
	Event.endtime = gettime ();

	Record (&Event, CMPI_TRACE);
	return retVal;
}

cudaError_t cudaMemGetInfo (size_t * a1, size_t * a2)
{
	cudaError_t retVal;
	Record_Event Event = Event_init ();
	Event.event_name = "cudaMemGetInfo";
	Event.eid = 341;
	Event.type = NONE;

	cudaMemGetInfo_real = (cudaError_t (*)(size_t *, size_t *)) dlsym (RTLD_NEXT, "cudaMemGetInfo");
	Event.starttime = gettime ();
	retVal = cudaMemGetInfo_real (a1, a2);
	Event.endtime = gettime ();

	Record (&Event, CMPI_TRACE);
	return retVal;
}

cudaError_t cudaMemcpyToArray(struct cudaArray * a1, size_t a2, size_t a3, const void * a4, size_t a5, enum cudaMemcpyKind a6)
{
	cudaError_t retVal;
	Record_Event Event = Event_init ();
	Event.event_name = "cudaMemcpyToArray";
	Event.eid = 342;
	Event.type = CUDA_MEM;
	Event.memcpy_size = a5;
	Event.memcpy_type = a6;
	Event.memcpy_kind = SYNC;

	cudaMemcpyToArray_real = (cudaError_t (*)(struct cudaArray *, size_t, size_t, const void *, size_t, enum cudaMemcpyKind)) dlsym (RTLD_NEXT, "cudaMemcpyToArray");
	Event.starttime = gettime ();
	retVal = cudaMemcpyToArray_real (a1, a2, a3, a4, a5, a6);
	Event.endtime = gettime ();

	Record (&Event, CMPI_TRACE);
	return retVal;
}

cudaError_t cudaMemcpyFromArray (void * a1, const struct cudaArray * a2, size_t a3, size_t a4, size_t a5, enum cudaMemcpyKind a6)
{
	cudaError_t retVal;
	Record_Event Event = Event_init ();
	Event.event_name = "cudaMemcpyFromArray";
	Event.eid = 343;
	Event.type = CUDA_MEM;
	Event.memcpy_size = a5;
	Event.memcpy_type = a6;
	Event.memcpy_kind = SYNC;

	cudaMemcpyFromArray_real = (cudaError_t (*)(void *, const struct cudaArray *, size_t, size_t, size_t, enum cudaMemcpyKind)) dlsym (RTLD_NEXT, "cudaMemcpyFromArray");
	Event.starttime = gettime ();
	retVal =  cudaMemcpyFromArray_real (a1, a2, a3, a4, a5, a6);
	Event.endtime = gettime ();

	Record (&Event, CMPI_TRACE);
	return retVal;
}

cudaError_t cudaMemcpyArrayToArray (struct cudaArray * a1, size_t a2, size_t a3, const struct cudaArray * a4, size_t a5, size_t a6, size_t a7, enum cudaMemcpyKind a8)
{
	cudaError_t retVal;
	Record_Event Event = Event_init ();
	Event.event_name = "cudaMemcpyArrayToArray";
	Event.eid = 344;
	Event.type = CUDA_MEM;
	Event.memcpy_size = a7;
	Event.memcpy_type = a8;
	Event.memcpy_kind = SYNC;

	cudaMemcpyArrayToArray_real = (cudaError_t (*)(struct cudaArray *, size_t, size_t, const struct cudaArray *, size_t, size_t, size_t, enum cudaMemcpyKind)) dlsym (RTLD_NEXT, "cudaMemcpyArrayToArray");
	Event.starttime = gettime ();
	retVal = cudaMemcpyArrayToArray_real (a1, a2, a3, a4, a5, a6 ,a7, a8);
	Event.endtime = gettime ();

	Record (&Event, CMPI_TRACE);
	return retVal;
}

cudaError_t cudaMemcpy2D (void * a1, size_t a2, const void * a3, size_t a4, size_t a5, size_t a6, enum cudaMemcpyKind a7)
{
	cudaError_t retVal;
	Record_Event Event = Event_init ();
	Event.event_name = "cudaMemcpy2D";
	Event.eid = 345;
	Event.type = CUDA_MEM;
	Event.memcpy_size = a5 * a6;
	Event.memcpy_type = a7;
	Event.memcpy_kind = SYNC;

	cudaMemcpy2D_real = (cudaError_t (*)(void *, size_t, const void *, size_t, size_t, size_t, enum cudaMemcpyKind)) dlsym (RTLD_NEXT, "cudaMemcpy2D");
	Event.starttime = gettime ();
	retVal = cudaMemcpy2D_real (a1, a2, a3, a4, a5, a6, a7);
	Event.endtime = gettime ();

	Record (&Event, CMPI_TRACE);
	return retVal;
}

cudaError_t cudaMemcpy2DToArray (struct cudaArray * a1, size_t a2, size_t a3, const void * a4, size_t a5, size_t a6, size_t a7, enum cudaMemcpyKind a8)
{
	cudaError_t retVal;
	Record_Event Event = Event_init ();
	Event.event_name = "cudaMemcpy2DToArray";
	Event.eid = 346;
	Event.type = CUDA_MEM;
	Event.memcpy_size = a6 * a7;
	Event.memcpy_type = a8;
	Event.memcpy_kind = SYNC;

	cudaMemcpy2DToArray_real = (cudaError_t (*)(struct cudaArray *, size_t, size_t, const void *, size_t, size_t, size_t, enum cudaMemcpyKind)) dlsym (RTLD_NEXT, "cudaMemcpy2DToArray");
	Event.starttime = gettime ();
	retVal = cudaMemcpy2DToArray_real (a1, a2, a3, a4, a5, a6, a7, a8);
	Event.endtime = gettime ();

	Record (&Event, CMPI_TRACE);
	return retVal;
}

cudaError_t cudaMemcpy2DFromArray (void * a1, size_t a2, const struct cudaArray * a3, size_t a4, size_t a5, size_t a6, size_t a7, enum cudaMemcpyKind a8)
{
	cudaError_t retVal;
	Record_Event Event = Event_init ();
	Event.event_name = "cudaMemcpy2DFromArray";
	Event.eid = 347;
	Event.type = CUDA_MEM;
	Event.memcpy_size = a6 * a7;
	Event.memcpy_type = a8;
	Event.memcpy_kind = SYNC;

	cudaMemcpy2DFromArray_real = (cudaError_t (*)(void *, size_t, const struct cudaArray *, size_t, size_t, size_t, size_t, enum cudaMemcpyKind)) dlsym (RTLD_NEXT, "cudaMemcpy2DFromArray");
	Event.starttime = gettime ();
	retVal = cudaMemcpy2DFromArray_real (a1, a2, a3, a4, a5, a6, a7, a8);
	Event.endtime = gettime ();

	Record (&Event, CMPI_TRACE);
	return retVal;
}

cudaError_t cudaMemcpy2DArrayToArray (struct cudaArray * a1, size_t a2, size_t a3, const struct cudaArray * a4, size_t a5, size_t a6, size_t a7, size_t a8, enum cudaMemcpyKind a9)
{
	cudaError_t retVal;
	Record_Event Event = Event_init ();
	Event.event_name = "cudaMemcpy2DArrayToArray";
	Event.eid = 348;
	Event.type = CUDA_MEM;
	Event.memcpy_size = a7 * a8;
	Event.memcpy_type = a9;
	Event.memcpy_kind = SYNC;

	cudaMemcpy2DArrayToArray_real = (cudaError_t (*)(struct cudaArray *, size_t, size_t, const struct cudaArray *, size_t, size_t, size_t, size_t, enum cudaMemcpyKind)) dlsym (RTLD_NEXT, "cudaMemcpy2DArrayToArray");
	Event.starttime = gettime ();
	retVal = cudaMemcpy2DArrayToArray_real (a1, a2, a3, a4, a5, a6, a7, a8, a9);
	Event.endtime = gettime ();

	Record (&Event, CMPI_TRACE);
	return retVal;
}

cudaError_t cudaMemcpyToSymbol (const char * a1, const void * a2, size_t a3, size_t a4, enum cudaMemcpyKind a5)
{
	cudaError_t retVal;
	Record_Event Event = Event_init ();
	Event.event_name = "cudaMemcpyToSymbol";
	Event.eid = 349;
	Event.type = CUDA_MEM;
	Event.memcpy_size = a3;
	Event.memcpy_type = a5;
	Event.memcpy_kind = SYNC;

	cudaMemcpyToSymbol_real = (cudaError_t (*)(const char *, const void *, size_t, size_t, enum cudaMemcpyKind)) dlsym (RTLD_NEXT, "cudaMemcpyToSymbol");
	Event.starttime = gettime ();
	retVal = cudaMemcpyToSymbol_real (a1, a2, a3, a4, a5);
	Event.endtime = gettime ();

	Record (&Event, CMPI_TRACE);
	return retVal;
}

cudaError_t cudaMemcpyFromSymbol (void * a1, const char * a2, size_t a3, size_t a4, enum cudaMemcpyKind a5)
{
	cudaError_t retVal;
	Record_Event Event = Event_init ();
	Event.event_name = "cudaMemcpyFromSymbol";
	Event.eid = 350;
	Event.type = CUDA_MEM;
	Event.memcpy_size = a3;
	Event.memcpy_type = a5;
	Event.memcpy_kind = SYNC;

	cudaMemcpyFromSymbol_real = (cudaError_t (*)(void *, const char *, size_t, size_t, enum cudaMemcpyKind)) dlsym (RTLD_NEXT, "cudaMemcpyFromSymbol");
	Event.starttime = gettime ();
	retVal = cudaMemcpyFromSymbol_real (a1, a2, a3, a4, a5);
	Event.endtime = gettime ();

	Record (&Event, CMPI_TRACE);
	return retVal;
}

cudaError_t cudaMemcpyAsync (void * a1, const void * a2, size_t a3, enum cudaMemcpyKind a4, cudaStream_t a5)
{
	cudaError_t retVal;
	Record_Event Event = Event_init ();
	Event.event_name = "cudaMemcpyAsync";
	Event.eid = 351;
	Event.type = CUDA_MEM;
	Event.memcpy_size = a3;
	Event.memcpy_type = a5;
	Event.memcpy_kind = ASYNC;
	//Event.cuda_stream = a6

	cudaMemcpyAsync_real = (cudaError_t (*)(void *, const void *, size_t, enum cudaMemcpyKind, cudaStream_t)) dlsym (RTLD_NEXT, "cudaMemcpyAsync");
	Event.starttime = gettime ();
	retVal = cudaMemcpyAsync_real (a1, a2, a3, a4, a5);
	Event.endtime = gettime ();

	Record (&Event, CMPI_TRACE);
	return retVal;
}

cudaError_t cudaMemcpyToArrayAsync (struct cudaArray * a1, size_t a2, size_t a3, const void * a4, size_t a5, enum cudaMemcpyKind a6, cudaStream_t a7)
{
	cudaError_t retVal;
	Record_Event Event = Event_init ();
	Event.event_name = "cudaMemcpyToArrayAsync";
	Event.eid = 352;
	Event.type = CUDA_MEM;
	Event.memcpy_size = a5;
	Event.memcpy_type = a6;
	Event.memcpy_kind = ASYNC;
	//Event.cuda_stream = a7;

	cudaMemcpyToArrayAsync_real = (cudaError_t (*)(struct cudaArray *, size_t, size_t, const void *, size_t, enum cudaMemcpyKind, cudaStream_t)) dlsym (RTLD_NEXT, "cudaMemcpyToArrayAsync");
	Event.starttime = gettime ();
	retVal =  cudaMemcpyToArrayAsync_real (a1, a2, a3, a4, a5, a6, a7);
	Event.endtime = gettime ();

	Record (&Event, CMPI_TRACE);
	return retVal;
}

cudaError_t cudaMemcpyFromArrayAsync (void * a1, const struct cudaArray * a2, size_t a3, size_t a4, size_t a5, enum cudaMemcpyKind a6, cudaStream_t a7)
{
	cudaError_t retVal;
	Record_Event Event = Event_init ();
	Event.event_name = "cudaMemcpyFromArrayAsync";
	Event.eid = 353;
	Event.type = CUDA_MEM;
	Event.memcpy_size = a5;
	Event.memcpy_type = a6;
	Event.memcpy_kind = ASYNC;
	//Event.cuda_stream = a7;

	cudaMemcpyFromArrayAsync_real = (cudaError_t (*)(void *, const struct cudaArray *, size_t, size_t, size_t, enum cudaMemcpyKind, cudaStream_t)) dlsym (RTLD_NEXT, "cudaMemcpyFromArrayAsync");
	Event.starttime = gettime ();
	retVal = cudaMemcpyFromArrayAsync_real (a1, a2, a3, a4, a5, a6, a7);
	Event.endtime = gettime ();

	Record (&Event, CMPI_TRACE);
	return retVal;
}

cudaError_t cudaMemcpy2DAsync (void * a1, size_t a2, const void * a3, size_t a4, size_t a5, size_t a6, enum cudaMemcpyKind a7, cudaStream_t a8)
{
	cudaError_t retVal;
	Record_Event Event = Event_init ();
	Event.event_name = "cudaMemcpy2DAsync";
	Event.eid = 354;
	Event.type = CUDA_MEM;
	Event.memcpy_size = a5 * a6;
	Event.memcpy_type = a7;
	Event.memcpy_kind = ASYNC;
	//Event.cuda_stream = a8;

	cudaMemcpy2DAsync_real = (cudaError_t (*)(void *, size_t, const void *, size_t, size_t, size_t, enum cudaMemcpyKind, cudaStream_t)) dlsym (RTLD_NEXT, "cudaMemcpy2DAsync");
	Event.starttime = gettime ();
	retVal = cudaMemcpy2DAsync_real (a1, a2, a3, a4, a5, a6, a7, a8);
	Event.endtime = gettime ();

	Record (&Event, CMPI_TRACE);
	return retVal;
}

cudaError_t cudaMemcpy2DToArrayAsync (struct cudaArray * a1, size_t a2, size_t a3, const void * a4, size_t a5, size_t a6, size_t a7, enum cudaMemcpyKind a8, cudaStream_t a9)
{
	
	cudaError_t retVal;
	Record_Event Event = Event_init ();
	Event.event_name = "cudaMemcpy2DToArrayAsync";
	Event.eid = 355;
	Event.type = CUDA_MEM;
	Event.memcpy_size = a6 * a7;
	Event.memcpy_type = a8;
	Event.memcpy_kind = ASYNC;
	//Event.cuda_stream = a9;

	cudaMemcpy2DToArrayAsync_real = (cudaError_t (*)(struct cudaArray *, size_t, size_t, const void *, size_t, size_t, size_t, enum cudaMemcpyKind, cudaStream_t)) dlsym (RTLD_NEXT, "cudaMemcpy2DToArrayAsync");
	Event.starttime = gettime ();
	retVal = cudaMemcpy2DToArrayAsync_real (a1, a2, a3, a4, a5, a6, a7, a8, a9);
	Event.endtime = gettime ();

	Record (&Event, CMPI_TRACE);
	return retVal;
}
	
cudaError_t cudaMemcpy2DFromArrayAsync (void * a1, size_t a2, const struct cudaArray * a3, size_t a4, size_t a5, size_t a6, size_t a7, enum cudaMemcpyKind a8, cudaStream_t a9)
{
	cudaError_t retVal;
	Record_Event Event = Event_init ();
	Event.event_name = "cudaMemcpy2DFromArrayAsync";
	Event.eid = 356;
	Event.type = CUDA_MEM;
	Event.memcpy_size = a6 * a7;
	Event.memcpy_type = a8;
	Event.memcpy_kind = ASYNC;
	//Event.cuda_stream = a9;

	cudaMemcpy2DFromArrayAsync_real = (cudaError_t (*)(void *, size_t, const struct cudaArray *, size_t, size_t, size_t, size_t, enum cudaMemcpyKind, cudaStream_t)) dlsym (RTLD_NEXT, "cudaMemcpy2DFromArrayAsync");
	Event.starttime = gettime ();
	retVal = cudaMemcpy2DFromArrayAsync_real (a1, a2, a3, a4, a5, a6, a7, a8, a9);
	Event.endtime = gettime ();

	Record (&Event, CMPI_TRACE);
	return retVal;
}

cudaError_t cudaMemcpyToSymbolAsync (const char * a1, const void * a2, size_t a3, size_t a4, enum cudaMemcpyKind a5, cudaStream_t a6)
{
	cudaError_t retVal;
	Record_Event Event = Event_init ();
	Event.event_name = "cudaMemcpyToSymbolAsync";
	Event.eid = 357;
	Event.type = CUDA_MEM;
	Event.memcpy_size = a3;
	Event.memcpy_type = a5;
	Event.memcpy_kind = ASYNC;
	//Event.cuda_stream = a6;

	cudaMemcpyToSymbolAsync_real = (cudaError_t (*)(const char *, const void *, size_t, size_t, enum cudaMemcpyKind, cudaStream_t)) dlsym (RTLD_NEXT, "cudaMemcpyToSymbolAsync");
	Event.starttime = gettime ();
	retVal = cudaMemcpyToSymbolAsync_real (a1, a2, a3, a4, a5, a6);
	Event.endtime = gettime ();

	Record (&Event, CMPI_TRACE);
	return retVal;
}

cudaError_t cudaMemcpyFromSymbolAsync (void * a1, const char * a2, size_t a3, size_t a4, enum cudaMemcpyKind a5, cudaStream_t a6)
{
	cudaError_t retVal;
	Record_Event Event = Event_init ();
	Event.event_name = "cudaMemcpyFromSymbolAsync";
	Event.eid = 358;
	Event.type = CUDA_MEM;
	Event.memcpy_size = a3;
	Event.memcpy_type = a5;
	Event.memcpy_kind = ASYNC;
	//Event.cuda_stream = a6;

	cudaMemcpyFromSymbolAsync_real = (cudaError_t (*) (void *, const char *, size_t, size_t, enum cudaMemcpyKind, cudaStream_t)) dlsym (RTLD_NEXT, "cudaMemcpyFromSymbolAsync");
	Event.starttime = gettime ();
	retVal = cudaMemcpyFromSymbolAsync_real (a1, a2, a3, a4, a5, a6);
	Event.endtime = gettime ();

	Record (&Event, CMPI_TRACE);
	return retVal;
}

cudaError_t cudaMemset (void * a1, int a2, size_t a3)
{
	cudaError_t retVal;
	Record_Event Event = Event_init ();
	Event.event_name = "cudaMemset";
	Event.eid = 359;
	Event.type = NONE;

	cudaMemset_real = (cudaError_t (*)(void *, int, size_t)) dlsym (RTLD_NEXT, "cudaMemset");
	Event.starttime = gettime ();
	retVal = cudaMemset_real (a1, a2, a3);
	Event.endtime = gettime ();

	Record (&Event, CMPI_TRACE);
	return retVal;
}

cudaError_t cudaMemset2D (void * a1, size_t a2, int a3, size_t a4, size_t a5)
{
	cudaError_t retVal;
	Record_Event Event = Event_init ();
	Event.event_name = "cudaMemset2D";
	Event.eid = 360;
	Event.type = NONE;

	cudaMemset2D_real = (cudaError_t (*)(void *, size_t, int, size_t, size_t)) dlsym (RTLD_NEXT, "cudaMemset2D");
	Event.starttime = gettime ();
	retVal = cudaMemset2D_real (a1, a2, a3, a4, a5);
	Event.endtime = gettime ();

	Record (&Event, CMPI_TRACE);
	return retVal;
}

cudaError_t cudaMemset3D (struct cudaPitchedPtr a1, int a2, struct cudaExtent a3)
{
	cudaError_t retVal;
	Record_Event Event = Event_init ();
	Event.event_name = "cudaMemset3D";
	Event.eid = 361;
	Event.type = NONE;

	cudaMemset3D_real = (cudaError_t (*)(struct cudaPitchedPtr, int, struct cudaExtent)) dlsym (RTLD_NEXT, "cudaMemset3D");
	Event.starttime = gettime ();
	retVal = cudaMemset3D_real (a1, a2, a3);
	Event.endtime = gettime ();

	Record (&Event, CMPI_TRACE);
	return retVal;
}

cudaError_t cudaMemsetAsync (void * a1, int a2, size_t a3, cudaStream_t a4)
{
	cudaError_t retVal;
	Record_Event Event = Event_init ();
	Event.event_name = "cudaMemsetAsync";
	Event.eid = 362;
	Event.type = NONE;

	cudaMemsetAsync_real = (cudaError_t (*)(void *, int, size_t, cudaStream_t)) dlsym (RTLD_NEXT, "cudaMemsetAsync");
	Event.starttime = gettime ();
	retVal = cudaMemsetAsync_real (a1, a2, a3, a4);
	Event.endtime = gettime ();

	Record (&Event, CMPI_TRACE);
	return retVal;
}

cudaError_t cudaMemset2DAsync (void * a1, size_t a2, int a3, size_t a4, size_t a5, cudaStream_t a6)
{
	cudaError_t retVal;
	Record_Event Event = Event_init ();
	Event.event_name = "cudaMemset2DAsync";
	Event.eid = 363;
	Event.type = NONE;

	cudaMemset2DAsync_real = (cudaError_t (*)(void *, size_t, int, size_t, size_t, cudaStream_t)) dlsym (RTLD_NEXT, "cudaMemset2DAsync");
	Event.starttime = gettime ();
	retVal = cudaMemset2DAsync_real (a1, a2, a3, a4, a5, a6);
	Event.endtime = gettime ();

	Record (&Event, CMPI_TRACE);
	return retVal;
}

cudaError_t cudaMemset3DAsync (struct cudaPitchedPtr a1, int a2, struct cudaExtent a3, cudaStream_t a4)
{
	cudaError_t retVal;
	Record_Event Event = Event_init ();
	Event.event_name = "cudaMemset3DAsync";
	Event.eid = 364;
	Event.type = NONE;

	cudaMemset3DAsync_real = (cudaError_t (*)(struct cudaPitchedPtr, int, struct cudaExtent, cudaStream_t)) dlsym (RTLD_NEXT, "cudaMemset3DAsync");
	Event.starttime = gettime ();
	retVal = cudaMemset3DAsync_real (a1, a2, a3, a4);
	Event.endtime = gettime ();

	Record (&Event, CMPI_TRACE);
	return retVal;
}

cudaError_t cudaGetSymbolAddress (void ** a1, const char * a2)
{
	cudaError_t retVal;
	Record_Event Event = Event_init ();
	Event.event_name = "cudaGetSymbolAddress";
	Event.eid = 365;
	Event.type = NONE;

	cudaGetSymbolAddress_real = (cudaError_t (*)(void **, const char *)) dlsym (RTLD_NEXT, "cudaGetSymbolAddress");
	Event.starttime = gettime ();
	retVal = cudaGetSymbolAddress_real (a1, a2);
	Event.endtime = gettime ();

	Record (&Event, CMPI_TRACE);
	return retVal;
}

cudaError_t cudaGetSymbolSize (size_t * a1, const char * a2)
{
	cudaError_t retVal;
	Record_Event Event = Event_init ();
	Event.event_name = "cudaGetSymbolSize";
	Event.eid = 366;
	Event.type = NONE;

	cudaGetSymbolSize_real = (cudaError_t (*)(size_t *, const char *)) dlsym (RTLD_NEXT, "cudaGetSymbolSize");
	Event.starttime = gettime ();
	retVal = cudaGetSymbolSize_real (a1, a2);
	Event.endtime = gettime ();

	Record (&Event, CMPI_TRACE);
	return retVal;
}
/*************************************************
/*           CUDA ExecutionControl              */
/************************************************/
cudaError_t cudaConfigureCall (dim3 a1, dim3 a2, size_t a3, cudaStream_t a4)
{
	cudaError_t retVal;
	Record_Event Event = Event_init ();
	Event.event_name = "cudaConfigureCall";
	Event.eid = 367;
	Event.type = NONE;

	cudaConfigureCall_real = (cudaError_t (*)(dim3, dim3, size_t, cudaStream_t)) dlsym (RTLD_NEXT, "cudaConfigureCall");
	Event.starttime = gettime ();
	retVal = cudaConfigureCall_real (a1, a2, a3, a4);
	Event.endtime = gettime ();

	Record (&Event, CMPI_TRACE);
	return retVal;
}

cudaError_t cudaSetupArgument (const void * a1, size_t a2, size_t a3)
{
	cudaError_t retVal;
	Record_Event Event = Event_init ();
	Event.event_name = "cudaSetupArgument";
	Event.eid = 368;
	Event.type = NONE;

	cudaSetupArgument_real = (cudaError_t (*)(const void *, size_t, size_t)) dlsym (RTLD_NEXT, "cudaSetupArgument");
	Event.starttime = gettime ();
	retVal = cudaSetupArgument_real (a1, a2, a3);
	Event.endtime = gettime ();

	Record (&Event, CMPI_TRACE);
	return retVal;
}
/*
 * This function is being called before execution of a cuda program for every
 * cuda kernel (host_runtime.h)
 * Borrowed from VampirTrace.
 */
/*void __cudaRegisterFunction(void ** a1, const char * a2, char * a3, const char * a4, int a5, uint3 * a6, uint3 * a7, dim3 * a8, dim3 * a9, int * a10)
{
	Record_Event Event = Event_init ();
	Event.event_name = "cudaRegisterFunction";
	Event.eid = 369;
	Event.type = NONE;

	__cudaRegisterFunction_real = (void (*)(void **, const char *, char *, const char *, int, uint3 *, uint3 *, dim3 *, dim3 *, int *)) dlsym (RTLD_NEXT, "__cudaRegisterFunction");
	
	Event.starttime = gettime ();
	(*__cudaRegisterFunction_real)(a1, a2, a3, a4, a5, a6, a7, a8, a9, a10);
	Event.endtime = gettime ();

	kernelName *new_name_pair = (kernelName*) malloc(sizeof(kernelName));
	new_name_pair->host = a2;
	new_name_pair->dev = a3;
	new_name_pair->next = kernelNamesHead;
	kernelNamesHead = new_name_pair;

	Record (&Event, CMPI_TRACE);
}*/

cudaError_t cudaLaunch (const char * a1)
{
	cudaError_t retVal;
	Record_Event Event = Event_init ();
	Event.event_name = "cudaLaunch";
	Event.eid = 370;
	Event.type = NONE;
	

	cudaLaunch_real = (cudaError_t (*)(const char *)) dlsym (RTLD_NEXT, "cudaLaunch");
	Event.starttime = gettime ();
	retVal =  cudaLaunch_real (a1);
	Event.endtime = gettime ();

	Record (&Event, CMPI_TRACE);
	return retVal;
}

cudaError_t cudaFuncGetAttributes (struct cudaFuncAttributes *a1, const char *a2)
{
	cudaError_t retVal;
	Record_Event Event = Event_init ();
	Event.event_name = "cudaFuncGetAttributes";
	Event.eid = 371;
	Event.type = NONE;

	cudaFuncGetAttributes_real = (cudaError_t (*)(struct cudaFuncAttributes *, const char *)) dlsym (RTLD_NEXT, "cudaFuncGetAttributes");
	Event.starttime = gettime ();
	retVal = cudaFuncGetAttributes_real (a1, a2);
	Event.endtime = gettime ();

	Record (&Event, CMPI_TRACE);
	return retVal;
}

cudaError_t cudaSetDoubleForDevice (double * a1)
{
	cudaError_t retVal;
	Record_Event Event = Event_init ();
	Event.event_name = "cudaSetDoubleForDevice";
	Event.eid = 372;
	Event.type = NONE;

	cudaSetDoubleForDevice_real = (cudaError_t (*)(double *)) dlsym (RTLD_NEXT, "cudaSetDoubleForDevice");
	Event.starttime = gettime ();
	retVal = cudaSetDoubleForDevice_real (a1);
	Event.endtime = gettime ();

	Record (&Event, CMPI_TRACE);
	return retVal;
}

cudaError_t cudaSetDoubleForHost (double *a1)
{
	cudaError_t retVal;
	Record_Event Event = Event_init ();
	Event.event_name = "cudaSetDoubleForHost";
	Event.eid = 373;
	Event.type = NONE;

	cudaSetDoubleForHost_real =(cudaError_t (*)(double *)) dlsym (RTLD_NEXT, "cudaSetDoubleForHost");
	Event.starttime = gettime ();
	retVal =  cudaSetDoubleForHost_real (a1);
	Event.endtime = gettime ();

	Record (&Event, CMPI_TRACE);
	return retVal;
}
