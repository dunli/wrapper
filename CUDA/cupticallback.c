#include <stdlib.h>
#include <stdio.h>
#include <cuda.h>
#include <cupti.h>

void Trace_end (void);

#define CHECK_CU_ERROR(err, cufunc)                                     \
  if (err != CUDA_SUCCESS)                                              \
    {                                                                   \
      printf ("%s:%d: error %d for CUDA Driver API function '%s'\n",    \
              __FILE__, __LINE__, err, cufunc);                         \
      exit(-1);                                                         \
    }

#define CHECK_CUPTI_ERROR(err, cuptifunc)                               \
  if (err != CUPTI_SUCCESS)                                             \
    {                                                                   \
      const char *errstr;                                               \
      cuptiGetResultString(err, &errstr);                               \
      printf ("%s:%d:Error %s for CUPTI API function '%s'.\n",          \
              __FILE__, __LINE__, errstr, cuptifunc);                   \
      exit(-1);                                                         \
    }

 // Structure to hold data collected by callback
typedef struct RuntimeApiTrace_st {
  const char *functionName;
  uint64_t startTimestamp;
  uint64_t endTimestamp;
  size_t memcpy_bytes;
  enum cudaMemcpyKind memcpy_kind;
} RuntimeApiTrace_t;

static void displayTimestamps(RuntimeApiTrace_t *);

enum launchOrder{ MEMCPY_H2D1, MEMCPY_H2D2, MEMCPY_D2H, KERNEL, THREAD_SYNC, LAUNCH_LAST};

CUptiResult cuptiErr;
CUptiResult cuptierr;
CUpti_SubscriberHandle subscriber;
RuntimeApiTrace_t trace[LAUNCH_LAST];


static const char *
memcpyKindStr(enum cudaMemcpyKind kind)
{
  switch (kind) {
  case cudaMemcpyHostToDevice:
    return "HostToDevice";
  case cudaMemcpyDeviceToHost:
    return "DeviceToHost";
  default:
    break;
  }

  return "<unknown>";
}

void CUPTIAPI
getTimestampCallback(void *userdata, CUpti_CallbackDomain domain,
                     CUpti_CallbackId cbid, const CUpti_CallbackData *cbInfo)
{
  static int memTransCount = 0;
  uint64_t startTimestamp;
  uint64_t endTimestamp;
	printf ("<------------getTimestampCallback--------------->\n");
  RuntimeApiTrace_t *traceData = (RuntimeApiTrace_t*)userdata;
  CUptiResult cuptiErr;
      
  // Data is collected only for the following API
  if ((cbid == CUPTI_RUNTIME_TRACE_CBID_cudaLaunch_v3020) || 
      (cbid == CUPTI_RUNTIME_TRACE_CBID_cudaThreadSynchronize_v3020) || 
      (cbid == CUPTI_RUNTIME_TRACE_CBID_cudaMemcpy_v3020))  { 
     
    // Set pointer depending on API
    if (cbid == CUPTI_RUNTIME_TRACE_CBID_cudaLaunch_v3020)
      traceData = traceData + KERNEL;
    else if (cbid == CUPTI_RUNTIME_TRACE_CBID_cudaThreadSynchronize_v3020) 
      traceData = traceData + THREAD_SYNC;
    else if (cbid == CUPTI_RUNTIME_TRACE_CBID_cudaMemcpy_v3020)
      traceData = traceData + MEMCPY_H2D1 + memTransCount;
                 
    if (cbInfo->callbackSite == CUPTI_API_ENTER) {
      // for a kernel launch report the kernel name, otherwise use the API
      // function name.
      if (cbid == CUPTI_RUNTIME_TRACE_CBID_cudaLaunch_v3020) {
        traceData->functionName = cbInfo->symbolName;
      }
      else {
        traceData->functionName = cbInfo->functionName;
      }
	printf ("%s\t",traceData->functionName);

      // Store parameters passed to cudaMemcpy
      if (cbid == CUPTI_RUNTIME_TRACE_CBID_cudaMemcpy_v3020) {
        traceData->memcpy_bytes = ((cudaMemcpy_v3020_params *)(cbInfo->functionParams))->count;
        traceData->memcpy_kind = ((cudaMemcpy_v3020_params *)(cbInfo->functionParams))->kind;
      }

        
      // Collect timestamp for API start
      cuptiErr = cuptiDeviceGetTimestamp(cbInfo->context, &startTimestamp);
      CHECK_CUPTI_ERROR(cuptiErr, "cuptiDeviceGetTimestamp");
            
      traceData->startTimestamp = startTimestamp;
	printf ("%llu\n", traceData->startTimestamp);
    }

    if (cbInfo->callbackSite == CUPTI_API_EXIT) {
      // Collect timestamp for API exit
      cuptiErr = cuptiDeviceGetTimestamp(cbInfo->context, &endTimestamp);
      CHECK_CUPTI_ERROR(cuptiErr, "cuptiDeviceGetTimestamp");
            
      traceData->endTimestamp = endTimestamp;
     
      // Advance to the next memory transfer operation
      if (cbid == CUPTI_RUNTIME_TRACE_CBID_cudaMemcpy_v3020) {
        memTransCount++;
      }

    } 
  }
//displayTimestamps(traceData);
}

static void 
displayTimestamps(RuntimeApiTrace_t *trace1)
{
  // Calculate timestamp of kernel based on timestamp from
  // cudaThreadSynchronize() call
  trace1[KERNEL].endTimestamp = trace1[THREAD_SYNC].endTimestamp;

  printf("startTimeStamp/gpuTime reported in nano-seconds\n\n");
  printf("Name\t\tStart Time\t\tGPU Time\tBytes\tKind\n");
  printf("%s\t%llu\t%llu\t\t%llu\t%s\n", trace1[MEMCPY_H2D1].functionName,
         (unsigned long long)trace1[MEMCPY_H2D1].startTimestamp, 
         (unsigned long long)trace1[MEMCPY_H2D1].endTimestamp - trace1[MEMCPY_H2D1].startTimestamp,
         (unsigned long long)trace1[MEMCPY_H2D1].memcpy_bytes,
         memcpyKindStr(trace1[MEMCPY_H2D1].memcpy_kind));
  printf("%s\t%llu\t%llu\t\t%llu\t%s\n", trace1[MEMCPY_H2D2].functionName,
         (unsigned long long)trace1[MEMCPY_H2D2].startTimestamp,
         (unsigned long long)trace1[MEMCPY_H2D2].endTimestamp - trace1[MEMCPY_H2D2].startTimestamp, 
         (unsigned long long)trace1[MEMCPY_H2D2].memcpy_bytes,
         memcpyKindStr(trace1[MEMCPY_H2D2].memcpy_kind)); 
  printf("%s\t%llu\t%llu\t\tNA\tNA\n", trace1[KERNEL].functionName,
         (unsigned long long)trace1[KERNEL].startTimestamp,
         (unsigned long long)trace1[KERNEL].endTimestamp - trace1[KERNEL].startTimestamp);
  printf("%s\t%llu\t%llu\t\t%llu\t%s\n", trace1[MEMCPY_D2H].functionName,
         (unsigned long long)trace1[MEMCPY_D2H].startTimestamp,
         (unsigned long long)trace1[MEMCPY_D2H].endTimestamp - trace1[MEMCPY_D2H].startTimestamp, 
         (unsigned long long)trace1[MEMCPY_D2H].memcpy_bytes,
         memcpyKindStr(trace1[MEMCPY_D2H].memcpy_kind)); 
}

__attribute__((constructor)) void Trace_start()
{

	cuptierr = cuptiSubscribe(&subscriber, (CUpti_CallbackFunc)getTimestampCallback , &trace);
	CHECK_CUPTI_ERROR(cuptierr, "cuptiSubscribe");
	cuptierr = cuptiEnableDomain(1, subscriber, CUPTI_CB_DOMAIN_RUNTIME_API);
	CHECK_CUPTI_ERROR(cuptierr, "cuptiEnableDomain");

	printf("<-----------register Trace_end--------------->\n");
	atexit (Trace_end);
}


void Trace_end ()
{
	displayTimestamps(trace);
	cuptierr = cuptiUnsubscribe(subscriber);
	CHECK_CUPTI_ERROR(cuptierr, "cuptiUnsubscribe");
}
