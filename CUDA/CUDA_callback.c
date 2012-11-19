/*
 * Copyright 2011-2012 NVIDIA Corporation. All rights reserved
 *
 * Sample CUPTI app to print a trace of CUDA API and GPU activity
 * 
 */

#include <stdio.h>
#include <cuda.h>
#include <cupti.h>
#include "wrapper.h"

extern int send_CUPTI_info (CUPTI_Event);
void Trace_end (void );

#define CUPTI_CALL(call)                                                \
  do {                                                                  \
    CUptiResult _status = call;                                         \
    if (_status != CUPTI_SUCCESS) {                                     \
      const char *errstr;                                               \
      cuptiGetResultString(_status, &errstr);                           \
      fprintf(stderr, "%s:%d: error: function %s failed with error %s.\n", \
              __FILE__, __LINE__, #call, errstr);                       \
      exit(-1);                                                         \
    }                                                                   \
  } while (0)

#define BUF_SIZE (128 * 1024)
#define ALIGN_SIZE (8)
#define ALIGN_BUFFER(buffer, align)                                            \
  (((uintptr_t) (buffer) & ((align)-1)) ? ((buffer) + (align) - ((uintptr_t) (buffer) & ((align)-1))) : (buffer)) 

// Timestamp at trace initialization time. Used to normalized other
// timestamps
static uint64_t startTimestamp;
 
static const char *
getMemcpyKindString(CUpti_ActivityMemcpyKind kind)
{
  switch (kind) {
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

static void
printActivity(CUpti_Activity *record)
{
  switch (record->kind) {
  case CUPTI_ACTIVITY_KIND_DEVICE:
    {
      CUpti_ActivityDevice *device = (CUpti_ActivityDevice *)record;
     printf("DEVICE %s (%u), capability %u.%u, global memory (bandwidth %u GB/s, size %u MB), "
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
     printf("CONTEXT %u, device %u, compute API %s\n",
             context->contextId, context->deviceId, 
             (context->computeApiKind == CUPTI_ACTIVITY_COMPUTE_API_CUDA) ? "CUDA" :
             (context->computeApiKind == CUPTI_ACTIVITY_COMPUTE_API_OPENCL) ? "OpenCL" : "unknown"); 
    break;
    }
  case CUPTI_ACTIVITY_KIND_MEMCPY:
    {
      	CUpti_ActivityMemcpy *memcpy = (CUpti_ActivityMemcpy *)record;
  /*    	CUPTI_Event Event;
      	Event.pid = getpid ();
      	Event.device_id = memcpy->deviceId;
     	Event.context_id = memcpy->contextId;
      	Event.stream_id = memcpy->streamId;
      	Event.type = CUDA_MEM;
      	Event.name = getMemcpyKindString((CUpti_ActivityMemcpyKind)memcpy->copyKind);
      	snprintf (Event.seg1, 80, "%llu Bytes", memcpy->bytes);
	snprintf (Event.seg2, 80, "%s", getTypeString(memcpy->srcKind));
	snprintf (Event.seg3, 80, "%s", getTypeString(memcpy->dstKind));
	Event.starttime = (long long unsigned)(memcpy->start);
      	Event.endtime = (long long unsigned)(memcpy->end);
      
     	send_CUPTI_info (Event);*/

      printf("MEMCPY %s [ %llu - %llu ] device %u, context %u, stream %u, correlation %u/r%u\n",
             getMemcpyKindString((CUpti_ActivityMemcpyKind)memcpy->copyKind),
             (unsigned long long)(memcpy->start ),
	     (unsigned long long)(memcpy->start - startTimestamp),
             (unsigned long long)(memcpy->end ),
             (unsigned long long)(memcpy->end - startTimestamp),
             memcpy->deviceId, memcpy->contextId, memcpy->streamId, 
             memcpy->correlationId, memcpy->runtimeCorrelationId);
      break;
    }
  case CUPTI_ACTIVITY_KIND_MEMSET:
    {
      CUpti_ActivityMemset *memset = (CUpti_ActivityMemset *)record;
      printf("MEMSET value=%u [ %llu - %llu ] device %u, context %u, stream %u, correlation %u/r%u\n",
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
/*	CUPTI_Event Event;
      	Event.pid = getpid ();
      	Event.device_id = kernel->deviceId;
      	Event.context_id = kernel->contextId;
      	Event.stream_id = kernel->streamId;
      	Event.type = CUDA_KER;
      	Event.name = kernel->name;
      	snprintf (Event.seg1, 80, "Grid (%d,%d,%d), Block (%d,%d,%d)", kernel->gridX, kernel->gridY, kernel->gridZ, kernel->blockX, kernel->blockY, kernel->blockZ);
	snprintf (Event.seg2, 80, "dynamicSharedMemory %d, staticSharedMemory %d", kernel->dynamicSharedMemory, kernel->staticSharedMemory);
	snprintf (Event.seg3, 80, "localMemoryPerThread %u, localMemoryTotal %u", kernel->localMemoryPerThread, kernel->localMemoryTotal);
	Event.starttime = (long long unsigned)(kernel->start);
      	Event.endtime = (long long unsigned)(kernel->end);
	
	send_CUPTI_info (Event);*/
      printf("KERNEL \"%s\" [ %llu - %llu ] device %u, context %u, stream %u, correlation %u/r%u\n",
             kernel->name,
             (unsigned long long)(kernel->start - startTimestamp),
             (unsigned long long)(kernel->end - startTimestamp),
             kernel->deviceId, kernel->contextId, kernel->streamId, 
             kernel->correlationId, kernel->runtimeCorrelationId);
      printf("    grid [%u,%u,%u], block [%u,%u,%u], shared memory (static %u, dynamic %u)\n",
             kernel->gridX, kernel->gridY, kernel->gridZ,
             kernel->blockX, kernel->blockY, kernel->blockZ,
             kernel->staticSharedMemory, kernel->dynamicSharedMemory);
      break;
    }
  case CUPTI_ACTIVITY_KIND_DRIVER:
    {
      CUpti_ActivityAPI *api = (CUpti_ActivityAPI *)record;
      printf("DRIVER cbid=%u [ %llu - %llu ] process %u, thread %u, correlation %u\n",
             api->cbid,
             (unsigned long long)(api->start - startTimestamp),
             (unsigned long long)(api->end - startTimestamp),
             api->processId, api->threadId, api->correlationId); 
      break;
    }
  case CUPTI_ACTIVITY_KIND_RUNTIME:
    {
      CUpti_ActivityAPI *api = (CUpti_ActivityAPI *)record;
      printf("RUNTIME cbid=%u [ %llu - %llu ] process %u, thread %u, correlation %u\n",
             api->cbid,
             (unsigned long long)(api->start - startTimestamp),
             (unsigned long long)(api->end - startTimestamp),
             api->processId, api->threadId, api->correlationId); 
      break;
    }
  default:
    printf("  <unknown>\n");
    break;
  }
}

/**
 * Allocate a new BUF_SIZE buffer and add it to the queue specified by
 * 'context' and 'streamId'.
 */
static void
queueNewBuffer(CUcontext context, uint32_t streamId)
{
  size_t size = BUF_SIZE;
  uint8_t *buffer = (uint8_t *)malloc(size+ALIGN_SIZE);
  CUPTI_CALL(cuptiActivityEnqueueBuffer(context, streamId, ALIGN_BUFFER(buffer, ALIGN_SIZE), size));
}

/**
 * Dump the contents of the top buffer in the queue specified by
 * 'context' and 'streamId', and return the top buffer. If the queue
 * is empty return NULL.
 */
static uint8_t *
dump(CUcontext context, uint32_t streamId)
{
  uint8_t *buffer = NULL;
  size_t validBufferSizeBytes;
  CUptiResult status;
  status = cuptiActivityDequeueBuffer(context, streamId, &buffer, &validBufferSizeBytes);
  if (status == CUPTI_ERROR_QUEUE_EMPTY) {
    return NULL;
  }
  CUPTI_CALL(status);
  
  if (context == NULL) {
    printf("[CUPTI] Starting dump for global\n");
  } else if (streamId == 0) {
    printf("[CUPTI] Starting dump for context %p\n", context);
  } else {
    printf("[CUPTI] Starting dump for context %p, stream %u\n", context, streamId);
  }

  CUpti_Activity *record = NULL;
  do {
    status = cuptiActivityGetNextRecord(buffer, validBufferSizeBytes, &record);
    if(status == CUPTI_SUCCESS) {
      printActivity(record);
    }
    else if (status == CUPTI_ERROR_MAX_LIMIT_REACHED) {
       	printf ("[CUPTI] CUPTI_ERROR_MAX_LIMIT_REACHED\n");
	break;
    }
    else {
      CUPTI_CALL(status);
    }
  } while (1);

  // report any records dropped from the queue
  size_t dropped;
  CUPTI_CALL(cuptiActivityGetNumDroppedRecords(context, streamId, &dropped));
  if (dropped != 0) {
    printf("Dropped %u activity records\n", (unsigned int)dropped);
  }

  if (context == NULL) {
    printf("[CUPTI] Finished dump for global\n");
  } else if (streamId == 0) {
    printf("[CUPTI] Finished dump for context %p \n", context);
  } else {
    printf("[CUPTI] Finished dump for context %p, stream %u\n", context, streamId);
  }

  return buffer;
}

/**
 * If the top buffer in the queue specified by 'context' and
 * 'streamId' is full, then dump its contents and return the
 * buffer. If the top buffer is not full, return NULL.
 */
static uint8_t *
dumpIfFull(CUcontext context, uint32_t streamId)
{
  size_t validBufferSizeBytes;
  CUptiResult status;
  status = cuptiActivityQueryBuffer(context, streamId, &validBufferSizeBytes);
  if (status == CUPTI_ERROR_MAX_LIMIT_REACHED) {
    	 printf ("[dumpIfFull]:\tCUPTI_ERROR_MAX_LIMIT_REACHED\n");
	return dump(context, streamId);
  } else if ((status != CUPTI_SUCCESS) && (status != CUPTI_ERROR_QUEUE_EMPTY)) {
	printf ("[dumpIfFull]:\tother\n");
    CUPTI_CALL(status);
  }
 else if (status == CUPTI_SUCCESS)
	printf ("[dumpIfFull]:\tCUPTI_SUCCESS\n");
 else if (status == CUPTI_ERROR_QUEUE_EMPTY)
 	printf ("[dumpIfFull]:\tCUPTI_ERROR_QUEUE_EMPTY\n");

  return NULL;
}

static void
handleSync(CUpti_CallbackId cbid, const CUpti_SynchronizeData *syncData)
{
  // check the top buffer of the global queue and dequeue if full. If
  // we dump a buffer add it back to the queue
  uint8_t *buffer = dumpIfFull(NULL, 0);

  if (buffer != NULL) {
    CUPTI_CALL(cuptiActivityEnqueueBuffer(NULL, 0, buffer, BUF_SIZE));
  }
  //else 
  //	printf ("[handleSync]:\tbuffer == NULL\n");

  // dump context buffer on context sync
  if (cbid == CUPTI_CBID_SYNCHRONIZE_CONTEXT_SYNCHRONIZED) {
	printf ("[handleSync]:\tCUPTI_CBID_SYNCHRONIZE_CONTEXT_SYNCHRONIZED\n");



    buffer = dumpIfFull(syncData->context, 0);
    if (buffer != NULL) {
      CUPTI_CALL(cuptiActivityEnqueueBuffer(syncData->context, 0, buffer, BUF_SIZE));
    }
    else
	 printf ("[handleSync]:\tbuffer == NULL\n");
  }
  // dump stream buffer on stream sync
  else if (cbid == CUPTI_CBID_SYNCHRONIZE_STREAM_SYNCHRONIZED) {
    uint32_t streamId;
	printf ("[handleSync]:\tCUPTI_CBID_SYNCHRONIZE_STREAM_SYNCHRONIZED\n");
    CUPTI_CALL(cuptiGetStreamId(syncData->context, syncData->stream, &streamId));


    buffer = dumpIfFull(syncData->context, streamId);
    if (buffer != NULL) {
      CUPTI_CALL(cuptiActivityEnqueueBuffer(syncData->context, streamId, buffer, BUF_SIZE));
    }
  }
  if (cbid == CUPTI_CBID_SYNCHRONIZE_INVALID)
  	printf ("[handleSync]:\tCUPTI_CBID_SYNCHRONIZE_INVALID\n");
  
}

static void
handleResource(CUpti_CallbackId cbid, const CUpti_ResourceData *resourceData)
{
  // enqueue buffers on a context's queue when the context is created
  if (cbid == CUPTI_CBID_RESOURCE_CONTEXT_CREATED) {
    queueNewBuffer(resourceData->context, 0);
    queueNewBuffer(resourceData->context, 0);
  }
  // dump all buffers on a context destroy
  else if (cbid == CUPTI_CBID_RESOURCE_CONTEXT_DESTROY_STARTING) {
    while (dump(resourceData->context, 0) != NULL) ;
  }

  // enqueue buffers on a stream's queue when a non-default stream is created
  if (cbid == CUPTI_CBID_RESOURCE_STREAM_CREATED) {
    uint32_t streamId;
    CUPTI_CALL(cuptiGetStreamId(resourceData->context, resourceData->resourceHandle.stream, &streamId));
    queueNewBuffer(resourceData->context, streamId);
    queueNewBuffer(resourceData->context, streamId);
  }
  // dump all buffers on a stream destroy
  else if (cbid == CUPTI_CBID_RESOURCE_STREAM_DESTROY_STARTING) {
    uint32_t streamId;
    CUPTI_CALL(cuptiGetStreamId(resourceData->context, resourceData->resourceHandle.stream, &streamId));
    while (dump(resourceData->context, streamId) != NULL) ;
  }
  if (cbid == CUPTI_CBID_RESOURCE_INVALID)
  {
	printf ("[handleResource]: CUPTI_CB_RESOURCE_INVALID\n");
  }
}

static void handleRuntimeAPI (CUpti_CallbackId cbid, const CUpti_CallbackData *cbinfo)
{
	uint64_t timestamp;
	char *name;
	//uint64_t endTimestamp;
	int finish;
	if ((cbid == CUPTI_RUNTIME_TRACE_CBID_cudaLaunch_v3020) || (cbid == CUPTI_RUNTIME_TRACE_CBID_cudaThreadSynchronize_v3020) || (cbid == CUPTI_RUNTIME_TRACE_CBID_cudaMemcpy_v3020))
	{
		if (cbinfo -> callbackSite == CUPTI_API_ENTER)
			finish = 0;
		else if (cbinfo -> callbackSite == CUPTI_API_EXIT)
			finish = 1;
		cuptiDeviceGetTimestamp(cbinfo->context, &timestamp);
		if (cbid == CUPTI_RUNTIME_TRACE_CBID_cudaLaunch_v3020)
			name = cbinfo->symbolName;
		else 
			name = cbinfo->functionName;
		
//		printf ("%d\t%d\t%u\t%s\t%llu\t%d\n",cbid, cbinfo->correlationId, cbinfo->contextUid, name, timestamp, finish);
	}
}
		

static void CUPTIAPI
traceCallback(void *userdata, CUpti_CallbackDomain domain,
              CUpti_CallbackId cbid, const void *cbdata)
{
	uint32_t streamId;
	if (domain == CUPTI_CB_DOMAIN_RESOURCE) 
	{
   		handleResource(cbid, (CUpti_ResourceData *)cbdata);
		cuptiGetStreamId(((CUpti_ResourceData *)cbdata)->context, ((CUpti_SynchronizeData *)cbdata)->stream, &streamId);
                printf ("[CUPTI_CB_DOMAIN_RESOURCE]:\t%d\t%u\n", streamId, cbid);
	} 
	else if (domain == CUPTI_CB_DOMAIN_SYNCHRONIZE) 
	{
		handleSync(cbid, (CUpti_SynchronizeData *)cbdata);
		cuptiGetStreamId(((CUpti_SynchronizeData *)cbdata)->context, ((CUpti_SynchronizeData *)cbdata)->stream, &streamId);
		printf ("[CUPTI_CB_DOMAIN_SYNCHRONIZE]:\t%d\t%u\n", streamId, cbid);
	
 	}
  	else if (domain == CUPTI_CB_DOMAIN_RUNTIME_API)
	{
//		printf ("<----------------CUPTI_CB_DOMAIN_RUNTIME_API-------------------->\n");
		handleRuntimeAPI (cbid, (CUpti_CallbackData *)cbdata);
	}
}

void
initTrace()
{

  // Enqueue a couple of buffers in the global queue
  queueNewBuffer(NULL, 0);
  queueNewBuffer(NULL, 0);

  // device activity record is created when CUDA initializes, so we
  // want to enable it before cuInit() or any CUDA runtime call
  CUPTI_CALL(cuptiActivityEnable(CUPTI_ACTIVITY_KIND_DEVICE));
//  CUPTI_CALL(cuptiActivityEnable(CUPTI_ACTIVITY_KIND_MEMCPY));
 // cuptiActivityEnable(CUPTI_ACTIVITY_KIND_CONTEXT);
 //                       cuptiActivityEnable(CUPTI_ACTIVITY_KIND_DRIVER);
//                        cuptiActivityEnable(CUPTI_ACTIVITY_KIND_RUNTIME);
 //                       cuptiActivityEnable(CUPTI_ACTIVITY_KIND_MEMCPY);
  //                      cuptiActivityEnable(CUPTI_ACTIVITY_KIND_MEMSET);
   //                     cuptiActivityEnable(CUPTI_ACTIVITY_KIND_KERNEL);

	
  CUpti_SubscriberHandle subscriber;
  CUPTI_CALL(cuptiSubscribe(&subscriber, (CUpti_CallbackFunc)traceCallback, NULL));

  CUPTI_CALL(cuptiEnableDomain(1, subscriber, CUPTI_CB_DOMAIN_RESOURCE));
  CUPTI_CALL(cuptiEnableDomain(1, subscriber, CUPTI_CB_DOMAIN_SYNCHRONIZE));
//add by wukai
  CUPTI_CALL(cuptiEnableDomain(1, subscriber, CUPTI_CB_DOMAIN_RUNTIME_API));
  cuptiActivityEnable(CUPTI_ACTIVITY_KIND_CONTEXT);
  //cuptiActivityEnable(CUPTI_ACTIVITY_KIND_DRIVER);
  //cuptiActivityEnable(CUPTI_ACTIVITY_KIND_RUNTIME);
  cuptiActivityEnable(CUPTI_ACTIVITY_KIND_MEMCPY);
  cuptiActivityEnable(CUPTI_ACTIVITY_KIND_MEMSET);
  cuptiActivityEnable(CUPTI_ACTIVITY_KIND_KERNEL);

  CUPTI_CALL(cuptiGetTimestamp(&startTimestamp));
}

void
finiTrace()
{
  // dump any remaining records from the global queue
  while (dump(NULL, 0) != NULL) ;
}

__attribute__((constructor)) void Trace_start()
{
	initTrace();
	atexit (Trace_end);
}

void Trace_end (void)
{
	finiTrace ();
}	
