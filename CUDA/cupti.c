#include <stdio.h>
#include <cuda.h>
#include <cupti.h>
#include "wrapper.h"

#define BUFSIZE (4096*1024)

extern int send_CUPTI_info (CUPTI_Event);
void traceCallback (void *, CUpti_CallbackDomain, CUpti_CallbackId, const void *);
void handleSync (void);
//void handleResource (void);
void handleRuntimeAPI (CUpti_CallbackId cbid, const CUpti_CallbackData *cbinfo); 
uint32_t streamId[10240] ;
int flag1=0;
int flag2=0;

CUpti_SubscriberHandle subscriber;
uint8_t *activityBuffer;
uint64_t startTimestamp;
int p_num, total_num, l_num;

#define CUPTI_CALL(call)                                                \
  do {                                                                  \
    CUptiResult _status = call;                                         \
    if (_status != CUPTI_SUCCESS) {                                     \
      const char *errstr;                                               \
      cuptiGetResultString(_status, &errstr);                           \
      fprintf_d(stderr, "%s:%d: error: function %s failed with error %s.\n", \
              __FILE__, __LINE__, #call, errstr);                       \
      exit(-1);                                                         \
    }                                                                   \
  } while (0)

int function_is_memcpy(CUpti_CallbackId id) { 
	if (	id ==     CUPTI_RUNTIME_TRACE_CBID_cudaMemcpy_v3020 ||
		id ==     CUPTI_RUNTIME_TRACE_CBID_cudaMemcpyToArray_v3020 ||
		id ==     CUPTI_RUNTIME_TRACE_CBID_cudaMemcpyFromArray_v3020 ||
		id ==     CUPTI_RUNTIME_TRACE_CBID_cudaMemcpyArrayToArray_v3020 ||
		id ==     CUPTI_RUNTIME_TRACE_CBID_cudaMemcpyToSymbol_v3020 ||
		id ==     CUPTI_RUNTIME_TRACE_CBID_cudaMemcpyFromSymbol_v3020 ||
		id ==     CUPTI_RUNTIME_TRACE_CBID_cudaMemcpyAsync_v3020 ||
		id ==     CUPTI_RUNTIME_TRACE_CBID_cudaMemcpyToArrayAsync_v3020 ||
		id ==     CUPTI_RUNTIME_TRACE_CBID_cudaMemcpyFromArrayAsync_v3020 ||
		id ==     CUPTI_RUNTIME_TRACE_CBID_cudaMemcpyToSymbolAsync_v3020 ||
		id ==     CUPTI_RUNTIME_TRACE_CBID_cudaMemcpyFromSymbolAsync_v3020
	)
		return 1;
	else
		return 0;	
}


const char * getMemcpyKindString(CUpti_ActivityMemcpyKind kind)
{
	switch (kind)
	{
		case CUPTI_ACTIVITY_MEMCPY_KIND_HTOD:
			return "HtoD";
		case CUPTI_ACTIVITY_MEMCPY_KIND_DTOH:
			return "DtoH";
		case CUPTI_ACTIVITY_MEMCPY_KIND_HTOA:
			return "HtoA";
		case CUPTI_ACTIVITY_MEMCPY_KIND_ATOH:
			return "AtoH";
		case CUPTI_ACTIVITY_MEMCPY_KIND_ATOA:
			return "AtoA";
		case CUPTI_ACTIVITY_MEMCPY_KIND_ATOD:
			return "AtoD";
		case CUPTI_ACTIVITY_MEMCPY_KIND_DTOA:
			return "DtoA";
		case CUPTI_ACTIVITY_MEMCPY_KIND_DTOD:
			return "DtoD";
		case CUPTI_ACTIVITY_MEMCPY_KIND_HTOH:
			return "HtoH";
		default:
			break;
	}
	return "<unknown>";
}

static const char * getTypeString(uint8_t type)
{
	switch (type)
	{
		case CUPTI_ACTIVITY_MEMORY_KIND_UNKNOWN:
			return "UNKNOWN";
		case CUPTI_ACTIVITY_MEMORY_KIND_PAGEABLE:
			return "PAGEABLE";
		case CUPTI_ACTIVITY_MEMORY_KIND_PINNED:
			return "PINNED";
		case CUPTI_ACTIVITY_MEMORY_KIND_DEVICE:
			return "DEVICE";
		case CUPTI_ACTIVITY_MEMORY_KIND_ARRAY:
			return "ARRAY";
		default:
			break;
	}

	return "<unknown>";
}

void printActivity(CUpti_Activity *record)
{
	p_num++;
	switch (record->kind)
	{
		case CUPTI_ACTIVITY_KIND_DEVICE:
		{
			CUpti_ActivityDevice *device = (CUpti_ActivityDevice *)record;
			printf_d("DEVICE %s (%u), capability %u.%u, global memory (bandwidth %u GB/s, size %u MB), "
				"multiprocessors %u, clock %u MHz\n",
				device->name, device->id, 
				device->computeCapabilityMajor, device->computeCapabilityMinor,
				(unsigned int)(device->globalMemoryBandwidth / 1024 / 1024),
				(unsigned int)(device->globalMemorySize / 1024 / 1024),
				device->numMultiprocessors, (unsigned int)(device->coreClockRate / 1000)); 
			break;
    	}
		case CUPTI_ACTIVITY_KIND_CONTEXT:
		{
			CUpti_ActivityContext *context = (CUpti_ActivityContext *)record;
			printf_d("CONTEXT %u, device %u, compute API %s\n",
				context->contextId, context->deviceId, 
				(context->computeApiKind == CUPTI_ACTIVITY_COMPUTE_API_CUDA) ? "CUDA" :
				(context->computeApiKind == CUPTI_ACTIVITY_COMPUTE_API_OPENCL) ? "OpenCL" : "unknown"); 
			break;
		}
		case CUPTI_ACTIVITY_KIND_MEMCPY:
		{
			CUpti_ActivityMemcpy *memcpy = (CUpti_ActivityMemcpy *)record;
		        /*CUPTI_Event Event;
		      	Event.pid = getpid ();
	      		Event.device_id = memcpy->deviceId;
	     		Event.context_id = memcpy->contextId;
	      		Event.stream_id = memcpy->streamId;
	      		Event.type = CUDA_MEM;
	      		Event.name = getMemcpyKindString((CUpti_ActivityMemcpyKind)memcpy->copyKind);
	      		snprintf_d (Event.seg1, 80, "%llu Bytes", memcpy->bytes);
		 	snprintf_d (Event.seg2, 80, "%s", getTypeString(memcpy->srcKind));
			snprintf_d (Event.seg3, 80, "%s", getTypeString(memcpy->dstKind));
			Event.starttime = (long long unsigned)(memcpy->start);
	  		Event.endtime = (long long unsigned)(memcpy->end);
	      
	     		send_CUPTI_info (Event);
*/
			/*printf_d("MEMCPY %s [ %llu - %llu ] device %u, context %u, stream %u, correlation %u/r%u\n",
				getMemcpyKindString((CUpti_ActivityMemcpyKind)memcpy->copyKind),
				(unsigned long long)(memcpy->start ),
				(unsigned long long)(memcpy->start - startTimestamp),
				(unsigned long long)(memcpy->end ),
				(unsigned long long)(memcpy->end - startTimestamp),
				memcpy->deviceId, memcpy->contextId, memcpy->streamId, 
				memcpy->correlationId, memcpy->runtimeCorrelationId);*/
			break;
		}
		case CUPTI_ACTIVITY_KIND_MEMSET:
		{
			CUpti_ActivityMemset *memset = (CUpti_ActivityMemset *)record;
			printf_d("MEMSET value=%u [ %llu - %llu ] device %u, context %u, stream %u, correlation %u/r%u\n",
				memset->value,
				(unsigned long long)(memset->start - startTimestamp),
				(unsigned long long)(memset->end - startTimestamp),
				memset->deviceId, memset->contextId, memset->streamId, 
				memset->correlationId, memset->runtimeCorrelationId);  
			break;
		}
		case CUPTI_ACTIVITY_KIND_KERNEL:
		{
			CUpti_ActivityKernel *kernel = (CUpti_ActivityKernel *)record;
/*			CUPTI_Event Event;
			Event.pid = getpid ();
			Event.device_id = kernel->deviceId;
			Event.context_id = kernel->contextId;
			Event.stream_id = kernel->streamId;
			Event.type = CUDA_KER;
			Event.name = kernel->name;
		 	snprintf_d (Event.seg1, 80, "Grid (%d,%d,%d), Block (%d,%d,%d)", kernel->gridX, kernel->gridY, kernel->gridZ, kernel->blockX, kernel->blockY, kernel->blockZ);
			snprintf_d (Event.seg2, 80, "dynamicSharedMemory %d, staticSharedMemory %d", kernel->dynamicSharedMemory, kernel->staticSharedMemory);
			snprintf_d (Event.seg3, 80, "localMemoryPerThread %u, localMemoryTotal %u", kernel->localMemoryPerThread, kernel->localMemoryTotal);
			Event.starttime = (long long unsigned)(kernel->start);
			Event.endtime = (long long unsigned)(kernel->end);
			
			send_CUPTI_info (Event);
*/			/*printf_d("KERNEL \"%s\" [ %llu - %llu ] device %u, context %u, stream %u, correlation %u/r%u\n",
				kernel->name,
				(unsigned long long)(kernel->start - startTimestamp),
				(unsigned long long)(kernel->end - startTimestamp),
				kernel->deviceId, kernel->contextId, kernel->streamId, 
				kernel->correlationId, kernel->runtimeCorrelationId);
			printf_d("    grid [%u,%u,%u], block [%u,%u,%u], shared memory (static %u, dynamic %u)\n",
				kernel->gridX, kernel->gridY, kernel->gridZ,
				kernel->blockX, kernel->blockY, kernel->blockZ,
				kernel->staticSharedMemory, kernel->dynamicSharedMemory);*/
			break;
		}
		case CUPTI_ACTIVITY_KIND_DRIVER:
		{
			CUpti_ActivityAPI *api = (CUpti_ActivityAPI *)record;
			printf_d("DRIVER cbid=%u [ %llu - %llu ] process %u, thread %u, correlation %u\n",
				api->cbid,
				(unsigned long long)(api->start - startTimestamp),
				(unsigned long long)(api->end - startTimestamp),
				api->processId, api->threadId, api->correlationId); 
			break;
		}
		case CUPTI_ACTIVITY_KIND_RUNTIME:
		{
			CUpti_ActivityAPI *api = (CUpti_ActivityAPI *)record;
			printf_d("RUNTIME cbid=%u [ %llu - %llu ] process %u, thread %u, correlation %u\n",
				api->cbid,
				(unsigned long long)(api->start - startTimestamp),
				(unsigned long long)(api->end - startTimestamp),
				api->processId, api->threadId, api->correlationId); 
			break;
		}
		default:
			printf_d("  <unknown>\n");
			break;
	}
}

void initTrace ()
{
	CUptiResult err;
	CUPTI_CALL (cuptiSubscribe(&subscriber, (CUpti_CallbackFunc)traceCallback, NULL));
	CUPTI_CALL (cuptiEnableDomain(1, subscriber, CUPTI_CB_DOMAIN_RUNTIME_API));
	CUPTI_CALL (cuptiEnableDomain(1, subscriber, CUPTI_CB_DOMAIN_DRIVER_API));
	CUPTI_CALL (cuptiEnableDomain(1, subscriber, CUPTI_CB_DOMAIN_SYNCHRONIZE));
//	CUPTI_CALL(cuptiEnableDomain(1, subscriber, CUPTI_CB_DOMAIN_RESOURCE));

	activityBuffer = (uint8_t *)malloc(BUFSIZE);

	CUPTI_CALL (cuptiActivityEnqueueBuffer(NULL, 0, activityBuffer, BUFSIZE));
	
//	cuptiActivityEnable(CUPTI_ACTIVITY_KIND_CONTEXT);
//	cuptiActivityEnable(CUPTI_ACTIVITY_KIND_DRIVER);
//	cuptiActivityEnable(CUPTI_ACTIVITY_KIND_RUNTIME);
	CUPTI_CALL (cuptiActivityEnable(CUPTI_ACTIVITY_KIND_MEMCPY));
	CUPTI_CALL (cuptiActivityEnable(CUPTI_ACTIVITY_KIND_KERNEL));
	CUPTI_CALL (cuptiGetTimestamp (&startTimestamp));
	total_num = 0;
	p_num = 0;
	l_num = 0;
}

void finiTrace ()
{
	printf_d ("<------------------record num:%d.total num:%d.limit reach numi:%d----------------->\n", p_num, total_num, l_num);
}

void traceCallback (void *userdata, CUpti_CallbackDomain domain, CUpti_CallbackId cbid, const void *cbdata)
{
	if (domain == CUPTI_CB_DOMAIN_SYNCHRONIZE)
	{
	//	printf_d("[traceCallback]:\tCUPTI_CB_DOMAIN_SYNCHRONIZE\n");
		handleSync ();
	}
	else if (domain == CUPTI_CB_DOMAIN_RESOURCE)
	{
	//	printf_d("[traceCallback]:\tCUPTI_CB_DOMAIN_RESOURCE\n");
//		handleResource (cbid, (CUpti_ResourceData *)cbdata);
	}
	else if (domain == CUPTI_CB_DOMAIN_RUNTIME_API)
	{
		//printf_d("[traceCallback]:\tCUPTI_CB_DOMAIN_RUNTIME_API\n");
		handleRuntimeAPI (cbid, (CUpti_CallbackData *)cbdata);
	}
	else if (domain == CUPTI_CB_DOMAIN_DRIVER_API)
	{
		//printf_d("[traceCallback]:\tCUPTI_CB_DOMAIN_DRIVER_API\n");
	}
}


void handleSync ()
{
	CUptiResult status;
	CUpti_Activity *record = NULL;
	size_t bufferSize = 0;
	size_t dropnumber = 0;
	cuptiActivityDequeueBuffer (NULL, 0, &activityBuffer, &bufferSize);
	//int count = 0;
		
	while (status != CUPTI_ERROR_MAX_LIMIT_REACHED)
	{
		//count ++;
		total_num ++;
		status = cuptiActivityGetNextRecord (activityBuffer, bufferSize, &record);
		if (status == CUPTI_SUCCESS)
		{
			printActivity (record);
		}
		else if (status == CUPTI_ERROR_MAX_LIMIT_REACHED)
		{
			l_num ++;
			//printf_d("[handleSync]:\t%d records have been dumped.\n");
			break;
		}
	}
	
	cuptiActivityGetNumDroppedRecords(NULL, 0, &dropnumber);

	if (dropnumber > 0)
		printf_d ("[Error]\t%d CUDA Events have been dropped\n", dropnumber);
		
	cuptiActivityEnqueueBuffer(NULL, 0, activityBuffer, BUFSIZE);
}
/*
void handleResource (CUpti_CallbackId cbid, const CUpti_ResourceData *resourceData)
{
	if (cbid == CUPTI_CBID_RESOURCE_CONTEXT_CREATED)
	{
		cuptiActivityEnqueueBuffer(NULL, 0, activityBuffer, BUFSIZE);
	CUptiResult err;
	CUpti_ResourceData* resource = (CUpti_ResourceData*) params;
	CUstream stream = (CUstream) resource->resourceHandle.stream;
	printf_d("in resource callback, stream retrieved.\n");
	uint32_t streamId;
	cuptiGetStreamId(resource->context, stream, &streamId);
	err = cuptiActivityEnqueueBuffer(resource->context, streamId, activityBuffer, BUFSIZE);
	
}*/

/*
uint32_t getMemcpyId (CUpti_CallbackId cbid, const CUpti_CallbackData *cbinfo)
{
	uint32_t id;
	if (cbid == CUPTI_RUNTIME_TRACE_CBID_cudaMemcpy_v3020)
	{
		cudaMemcpyAsync_v3020_params * params = (cudaMemcpyAsync_v3020_params *) cbinfo->functionParams;
		cuptiGetStreamId(cbinfo->context, (CUstream) params->stream, &id);
	}
	else if (cbid == CUPTI_RUNTIME_TRACE_CBID_cudaMemcpyPeerAsync_v4000)
	{
		cudaMemcpyPeerAsync_v4000_params * params = (cudaMemcpyPeerAsync_v4000_params *) cbinfo->functionParams;
		cuptiGetStreamId(cbinfo->context, (CUstream) params->stream, &id);
	}
	else if (cbid == CUPTI_RUNTIME_TRACE_CBID_cudaMemcpyToArrayAsync_v3020)
	{
		cudaMemcpyToArrayAsync_v3020_params * params = (cudaMemcpyToArrayAsync_v3020_params *) cbinfo->functionParams;
		cuptiGetStreamId(cbinfo->context, (CUstream) params->stream, &id);
	}
	else if (cbid == CUPTI_RUNTIME_TRACE_CBID_cudaMemcpyFromArrayAsync_v3020)
	{
		cudaMemcpyFromArrayAsync_v3020_params * params = (cudaMemcpyFromArrayAsync_v3020_params *) cbinfo->functionParams;
		cuptiGetStreamId(cbinfo->context, (CUstream) params->stream, &id);
	}
	else if (cbid == CUPTI_RUNTIME_TRACE_CBID_cudaMemcpy2DAsync_v3020)
	{
		cudaMemcpy2DAsync_v3020_params * params = (cudaMemcpy2DAsync_v3020_params *) cbinfo->functionParams;
		cuptiGetStreamId(cbinfo->context, (CUstream) params->stream, &id);
	}
	else if (cbid == CUPTI_RUNTIME_TRACE_CBID_cudaMemcpy2DToArrayAsync_v3020)
	{
		cudaMemcpy2DToArrayAsync_v3020_params * params = (cudaMemcpy2DToArrayAsync_v3020_params *) cbinfo->functionParams;
		cuptiGetStreamId(cbinfo->context, (CUstream) params->stream, &id);
	}
	else if (cbid == CUPTI_RUNTIME_TRACE_CBID_cudaMemcpy2DFromArrayAsync_v3020)
	{
		cudaMemcpy2DFromArrayAsync_v3020_params * params = (cudaMemcpy2DFromArrayAsync_v3020_params *) cbinfo->functionParams;
		cuptiGetStreamId(cbinfo->context, (CUstream) params->stream, &id);
	}
	else if (cbid == CUPTI_RUNTIME_TRACE_CBID_cudaMemcpyToSymbolAsync_v3020)
	{
		cudaMemcpyToSymbolAsync_v3020_params * params = (cudaMemcpyToSymbolAsync_v3020_params *) cbinfo->functionParams;
		cuptiGetStreamId(cbinfo->context, (CUstream) params->stream, &id);
	}
	else if (cbid == CUPTI_RUNTIME_TRACE_CBID_cudaMemcpyFromSymbolAsync_v3020)
	{
		cudaMemcpyFromSymbolAsync_v3020_params * params = (cudaMemcpyFromSymbolAsync_v3020_params *) cbinfo->functionParams;
		cuptiGetStreamId(cbinfo->context, (CUstream) params->stream, &id);
	}	
	
//	cuptiGetStreamId(cbinfo->context, (CUstream) params->stream, &id);
//	return id;
	printf_d ("<--------------------%u---------------------->\n", id);
	return id;
}*/
void handleRuntimeAPI (CUpti_CallbackId cbid, const CUpti_CallbackData *cbinfo)
{
	uint64_t timestamp;
	char *name;
//	uint32_t streamId = 0;
	
	int finish;
	if(cbid == CUPTI_RUNTIME_TRACE_CBID_cudaConfigureCall_v3020 && cbinfo->callbackSite == CUPTI_API_ENTER)
	{
   		cudaConfigureCall_v3020_params * params = (cudaConfigureCall_v3020_params *) cbinfo->functionParams;
		cuptiGetStreamId(cbinfo->context, (CUstream) params->stream, &streamId[flag1]);
		flag1++;
//		printf_d ("<---------------------flag1:%d:--------------------->\n",flag1);
	}

	/*if ((cbid == CUPTI_RUNTIME_TRACE_CBID_cudaLaunch_v3020) || (cbid == CUPTI_RUNTIME_TRACE_CBID_cudaThreadSynchronize_v3020) || (cbid == CUPTI_RUNTIME_TRACE_CBID_cudaMemcpy_v3020))*/
	if (cbid == CUPTI_RUNTIME_TRACE_CBID_cudaLaunch_v3020)
	{
	//

	//	printf_d ("<-----------------%s------------------->\n", params1->entry);
		if (cbinfo -> callbackSite == CUPTI_API_ENTER)
			finish = 1;
		else if (cbinfo -> callbackSite == CUPTI_API_EXIT)
			finish = 2;
		cuptiDeviceGetTimestamp(cbinfo->context, &timestamp);
		CUPTI_Event Event;
		Event.pid = getpid ();
		gethostname (Event.hostname, 20);
		Event.device_id = -1;
		Event.context_id = cbinfo->contextUid;
		Event.stream_id = streamId[flag2];
		Event.type = CUDA_KER;
		Event.name = cbinfo->symbolName;
		Event.seg1 = "NULL";
		Event.seg2 = "NULL";
		Event.seg3 = "NULL";
 		Event.time = timestamp;
		Event.finish  = finish;
		send_CUPTI_info (Event);

//		printf_d ("%d\t%d\t%u\t%s\t%llu\t%d\t%d\t%d\n",cbid, cbinfo->correlationId, cbinfo->contextUid, cbinfo->symbolName, timestamp, streamId[flag2], finish ,flag2);
		if (cbinfo -> callbackSite == CUPTI_API_EXIT)
			flag2++;
	}
	else if (function_is_memcpy (cbid))
	{
//		uint32_t sid;
		//CUpti_ActivityMemcpyKind kind;
		uint32_t id = 0;
		if (cbid == CUPTI_RUNTIME_TRACE_CBID_cudaMemcpyAsync_v3020)
		{
			cudaMemcpyAsync_v3020_params * params = (cudaMemcpyAsync_v3020_params *) cbinfo->functionParams;
			cuptiGetStreamId(cbinfo->context, (CUstream) params->stream, &id);
		}
		else if (cbid == CUPTI_RUNTIME_TRACE_CBID_cudaMemcpyPeerAsync_v4000)
		{
			cudaMemcpyPeerAsync_v4000_params * params = (cudaMemcpyPeerAsync_v4000_params *) cbinfo->functionParams;
			cuptiGetStreamId(cbinfo->context, (CUstream) params->stream, &id);
		}
		else if (cbid == CUPTI_RUNTIME_TRACE_CBID_cudaMemcpyToArrayAsync_v3020)
		{
			cudaMemcpyToArrayAsync_v3020_params * params = (cudaMemcpyToArrayAsync_v3020_params *) cbinfo->functionParams;
			cuptiGetStreamId(cbinfo->context, (CUstream) params->stream, &id);
		}
		else if (cbid == CUPTI_RUNTIME_TRACE_CBID_cudaMemcpyFromArrayAsync_v3020)
		{
			cudaMemcpyFromArrayAsync_v3020_params * params = (cudaMemcpyFromArrayAsync_v3020_params *) cbinfo->functionParams;
			cuptiGetStreamId(cbinfo->context, (CUstream) params->stream, &id);
		}
		else if (cbid == CUPTI_RUNTIME_TRACE_CBID_cudaMemcpy2DAsync_v3020)
		{
			cudaMemcpy2DAsync_v3020_params * params = (cudaMemcpy2DAsync_v3020_params *) cbinfo->functionParams;
			cuptiGetStreamId(cbinfo->context, (CUstream) params->stream, &id);
		}
		else if (cbid == CUPTI_RUNTIME_TRACE_CBID_cudaMemcpy2DToArrayAsync_v3020)
		{
			cudaMemcpy2DToArrayAsync_v3020_params * params = (cudaMemcpy2DToArrayAsync_v3020_params *) cbinfo->functionParams;
			cuptiGetStreamId(cbinfo->context, (CUstream) params->stream, &id);
		}
		else if (cbid == CUPTI_RUNTIME_TRACE_CBID_cudaMemcpy2DFromArrayAsync_v3020)
		{
			cudaMemcpy2DFromArrayAsync_v3020_params * params = (cudaMemcpy2DFromArrayAsync_v3020_params *) cbinfo->functionParams;
			cuptiGetStreamId(cbinfo->context, (CUstream) params->stream, &id);
		}
		else if (cbid == CUPTI_RUNTIME_TRACE_CBID_cudaMemcpyToSymbolAsync_v3020)
		{
			cudaMemcpyToSymbolAsync_v3020_params * params = (cudaMemcpyToSymbolAsync_v3020_params *) cbinfo->functionParams;
			cuptiGetStreamId(cbinfo->context, (CUstream) params->stream, &id);
		}
		else if (cbid == CUPTI_RUNTIME_TRACE_CBID_cudaMemcpyFromSymbolAsync_v3020)
		{
			cudaMemcpyFromSymbolAsync_v3020_params * params = (cudaMemcpyFromSymbolAsync_v3020_params *) cbinfo->functionParams;
			cuptiGetStreamId(cbinfo->context, (CUstream) params->stream, &id);
		}    

		if (cbinfo -> callbackSite == CUPTI_API_ENTER)
			finish = 1;
		else if (cbinfo -> callbackSite == CUPTI_API_EXIT)
			finish = 2;
		
		cuptiDeviceGetTimestamp(cbinfo->context, &timestamp);
   
		CUPTI_Event Event;
		Event.pid = getpid ();
		gethostname (Event.hostname, 20);
		Event.device_id = -1;
		Event.context_id = cbinfo->contextUid;
		Event.stream_id = id;
		Event.type = CUDA_MEM;
		Event.name = cbinfo->functionName;
		Event.seg1 = "NULL";
		Event.seg2 = "NULL";
		Event.seg3 = "NULL";
		Event.time = timestamp;
		Event.finish  = finish;
		send_CUPTI_info (Event);

//		cudaMemcpyAsync_v3020_params * params = (cudaMemcpyAsync_v3020_params *) cbinfo->functionParams;
//                cuptiGetStreamId(cbinfo->context, (CUstream) params->stream, &id);

		
//		printf_d ("%d\t%d\t%u\t%s\t%llu\t%d\t%d\n",cbid, cbinfo->correlationId, cbinfo->contextUid, cbinfo->functionName, timestamp, id, finish);
	}
/*	else 
		printf_d ("%d\t%d\t%u\t%s\n",cbid, cbinfo->correlationId, cbinfo->contextUid, cbinfo->functionName);
*/
}
	
