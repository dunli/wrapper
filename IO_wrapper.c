#define _GNU_SOURCE
#include <dlfcn.h>
#include "wrapper.h"

extern long long unsigned gettime ();

//print message
int print_message (const char *format, ...)
{
	va_list arg;
	int retVal;
	
	va_start (arg, format);
	retVal = vfprintf (stdout, format, arg);
	va_end (arg);

	return retVal;
}


int fclose (FILE *fp)
{
	int retVal;
	void *handle;
	static int (* fclose_real) (FILE *) = NULL;
	handle = dlopen ("libc.so.6", RTLD_LAZY);	
	if (!handle)
	{
		print_message ("[IO_Wrapper] open libc.so.6 failed, now exiting!\n");
		exit (-1);
	}

	IO_Event Event;
	Event.event_name = "fclose";

	fclose_real = (int (*)(FILE *)) dlsym (handle, "fclose");
	Event.start_time = gettime ();
	retVal = fclose_real (fp);
	Event.end_time = gettime ();

	Record (&Event, IO_TRACE);
	dlclose (handle);
	return retVal;
}

char * fgets (char *buf, int n, FILE * fp)
{
	char * retVal;
	void * handle;
	static char *(*fgets_real)(char *, int, FILE *) = NULL;
	handle = dlopen ("libc.so.6", RTLD_LAZY);
	if (!handle)
	{
		print_message ("[IO_Wrapper] open libc.so.6 failed, now exiting!\n");
		exit (-1);
	}

	IO_Event Event;
	Event.event_name = "fgets";
	fgets_real = (char *(*)(char *, int, FILE *))dlsym (handle, "fgets");

	Event.start_time = gettime ();
	retVal = fgets_real (buf, n, fp);
	Event.end_time = gettime ();

	Record (&Event, IO_TRACE);
	dlclose (handle);
	return retVal;
}

FILE * fopen(const char * p1,const char * p2)
{
	FILE * retVal;
	void *handle;
	static FILE *(*fopen_real)(const char *, const char *) = NULL;
	handle = dlopen ("libc.so.6", RTLD_LAZY);
	if (!handle)
	{
		print_message ("[IO_Wrapper] open libc.so.6 failed, now exiting!\n");
		exit (-1);
	}

	IO_Event Event;
	Event.event_name = "fopen";
	sprintf_d (Event.data, "%s", p2);
	
	fopen_real = (FILE *(*)(const char *, const char *)) dlsym (handle, "fopen");
	
	Event.start_time = gettime ();
	retVal = fopen_real (p1, p2);
	Event.end_time = gettime ();
	
	Record (&Event, IO_TRACE);
	dlclose (handle);
	return retVal;
}


int fprintf (FILE *stream, const char *format, ...)
{
	va_list arg;
	int retVal;

	IO_Event Event;
	Event.event_name = "fprintf";

	Event.start_time = gettime ();
	va_start (arg, format);
	retVal = vfprintf (stream, format, arg);
	va_end (arg);
	Event.end_time = gettime ();

	Record (&Event, IO_TRACE);
	return retVal;
}

int fputs (const char *str, FILE * fp)
{
	int retVal;
	void * handle; 
	static int (*fputs_real)(const char *, FILE *) = NULL;
	handle = dlopen ("libc.so.6", RTLD_LAZY);
	if (!handle)
	{
		print_message ("[IO_Wrapper] open libc.so.6 failed, now exiting!\n");
		exit (-1);
	}

	IO_Event Event;
	Event.event_name = "fputs";

	fputs_real = (int (*)(const char *, FILE *)) dlsym (handle, "fputs");

	Event.start_time = gettime ();
	retVal = fputs_real (str, fp);
	Event.end_time = gettime ();

	Record (&Event, IO_TRACE);
	dlclose (handle);
	return retVal;
}

size_t fread (void *buf, size_t size, size_t count, FILE *fp)
{
	size_t retVal;
	void * handle;
	static size_t (*fread_real)(void *, size_t, size_t, FILE *) = NULL;
	
	handle = dlopen ("libc.so.6", RTLD_LAZY);
	if (!handle)
	{
		print_message ("[IO_Wrapper] open libc.so.6 failed, now exiting!\n");
		exit (-1);
	}

	IO_Event Event;
	Event.event_name = "fread";

	fread_real = (size_t (*)(void *, size_t, size_t, FILE *)) dlsym (handle, "fread");

	Event.start_time = gettime ();
	retVal = fread_real (buf, size, count, fp);
	Event.end_time = gettime ();

	Record (&Event, IO_TRACE);
	dlclose (handle);
	return retVal;
}

size_t fwrite (const void *buf, size_t size, size_t count, FILE * fp)
{
	size_t retVal;
	void *handle;
	static size_t (*fwrite_real)(const void *, size_t, size_t, FILE *) = NULL;

	handle = dlopen ("libc.so.6", RTLD_LAZY);
	if (!handle)
	{
		print_message ("[IO_Wrapper] open libc.so.6 failed, now exiting!\n");
		exit (-1);
	}

	IO_Event Event;
	Event.event_name = "fwrite";
	fwrite_real = (size_t (*)(const void *, size_t, size_t, FILE *)) dlsym (handle, "fwrite");

	Event.start_time = gettime ();
	retVal = fwrite_real (buf, size, count, fp);
	Event.end_time = gettime ();

	Record (&Event, IO_TRACE);
	dlclose (handle);
	return retVal;
}

int getchar ()
{
	int retVal;
	void *handle;
	static int (*getchar_real) (void) = NULL;
	
	handle = dlopen ("libc.so.6", RTLD_LAZY);
	if (!handle)
	{
		print_message ("[IO_Wrapper] open libc.so.6 failed, now exiting!\n");
		exit (-1);
	}

	IO_Event Event;
	Event.event_name = "getchar";
	getchar_real = (int (*)(void)) dlsym (handle, "getchar");

	Event.start_time = gettime ();
	retVal = getchar_real ();
	Event.end_time = gettime ();

	Record (&Event, IO_TRACE);
	dlclose (handle);
	return retVal;
}

//when printf only have one argument ,puts() will be called instead of 
int puts (const char * str)
{
	int retVal = EOF;

    static int (*puts_real)(const char *) = NULL;
	void * handle;
	handle = dlopen ("libc.so.6", RTLD_LAZY);
	if (!handle)
	{
		print_message ("[IO_Wrapper] open libc.so.6 failed, now exiting!\n");
		exit (-1);
	}

	IO_Event Event;
	Event.event_name = "puts";
	puts_real = (int (*)(const char *)) dlsym (handle, "puts");

	Event.start_time = gettime ();
	retVal = puts_real (str);
	Event.end_time = gettime ();

	Record (&Event, IO_TRACE);
	dlclose (handle);
	return retVal;
}

int printf (const char *format, ...)
{
	int retVal;
	va_list arg;
	
	static int (*vprintf_real)(const char *, __gnuc_va_list) = NULL;
	void * handle;
	handle = dlopen ("libc.so.6", RTLD_LAZY);
	if (!handle)
	{
		print_message ("[IO_Wrapper] open libc.so.6 failed, now exiting!\n");
		exit (-1);
	}

	IO_Event Event;
	Event.event_name = "printf";
	// Event.data 

	vprintf_real = (int (*)(const char *, __gnuc_va_list)) dlsym (handle, "vprintf");

	Event.start_time = gettime ();
	va_start (arg, format);
	retVal = vprintf_real (format, arg);
	va_end (arg);
	Event.end_time = gettime ();
	
	Record (&Event, IO_TRACE);
	dlclose (handle);
	return retVal;
}

int scanf (const char *format, ...)
{
	va_list arg;
	int retVal;
	IO_Event Event;
	Event.event_name = "scanf";

	Event.start_time = gettime ();
	va_start (arg, format);
	retVal = __vfscanf (stdin, format, arg, NULL);
	va_end (arg);
	Event.end_time = gettime ();

	Record (&Event, IO_TRACE);
	return retVal;
}

int snprintf (char *s, size_t maxlen, const char *format, ...)
{
	va_list arg;
	int retVal;

	IO_Event Event;
	Event.event_name = "snprintf";

	Event.start_time = gettime ();
	va_start (arg, format);
	retVal = __vsnprintf (s, maxlen, format, arg);
	va_end (arg);
	Event.end_time = gettime ();

	Record (&Event, IO_TRACE);
	return retVal;
}


int sprintf (char *s, const char *format, ...)
{
	va_list arg;
	int retVal;

	IO_Event Event;
	Event.event_name = "snprintf";

	Event.start_time = gettime ();
	va_start (arg, format);
	retVal = vsprintf (s, format, arg);
	va_end (arg);
	Event.end_time = gettime ();

	Record (&Event, IO_TRACE);
	return retVal;
}

int sscanf (const char *s, const char *format, ...)
{
	va_list arg;
	int retVal;

	IO_Event Event;
	Event.event_name = "sscanf";

	Event.start_time = gettime ();
	va_start (arg, format);
	retVal = __vsscanf (s, format, arg);
	va_end (arg);
	Event.end_time = gettime ();

	Record (&Event, IO_TRACE);
	return retVal;
}

int ungetc (int c, FILE *fp)
{
	int retVal;
	void * handle;
	static int (*ungetc_real) (int, FILE *) = NULL;

	handle = dlopen ("libc.so.6", RTLD_LAZY);
	if (!handle)
	{
		print_message ("[IO_Wrapper] open libc.so.6 failed, now exiting!\n");
		exit (-1);
	}

	IO_Event Event;
	Event.event_name = "ungetc";

	ungetc_real = (int (*)(int, FILE *)) dlsym (handle, "ungetc");

	Event.start_time = gettime ();
	retVal = ungetc_real (c, fp);
	Event.end_time = gettime ();

	Record (&Event, IO_TRACE);
	dlclose (handle);
	return retVal;
}


