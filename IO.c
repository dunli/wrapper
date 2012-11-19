#define _GNU_SOURCE

#include <dlfcn.h>
#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

//print message
int print_message (const char *format, ...)
{
	va_list arg;
	int done;
	
	va_start (arg, format);
	done = vfprintf (stdout, format, arg);
	va_end (arg);

	return done;
}


/*****************************************************************/
int printf_d (const char *format, ...)
{
	va_list arg;
	int done;

	static int (*vprintf_real)(const char *, __gnuc_va_list) = NULL;
	void * handle;
	handle = dlopen ("libc.so.6", RTLD_LAZY);

	if (!handle)
	{
		print_message ("open libc.so.6 failed\n");
		exit (-1);
	}

	vprintf_real = (int (*)(const char *, __gnuc_va_list)) dlsym (handle, "vprintf");

	va_start (arg, format);
	done = vprintf_real (format, arg);
	va_end (arg);
	dlclose (handle);
	return done;
}


FILE * fopen_d(const char * p1,const char * p2)
{
	FILE * retVal;
	void *handle;
	static FILE *(*fopen_real)(const char *, const char *) = NULL;
	handle = dlopen ("libc.so.6", RTLD_LAZY);

	if (!handle)
	{
		print_message ("open libc.so.6 failed\n");
		exit (-1);
	}
	fopen_real = (FILE *(*)(const char *, const char *)) dlsym (handle, "fopen");

	retVal = fopen_real (p1, p2);
	dlclose (handle);

	return retVal	;
}

int fclose_d (FILE *fp)
{
	int retVal;
	void *handle;
	static int (* fclose_real)(FILE *) = NULL;
	handle = dlopen ("libc.so.6", RTLD_LAZY);

	if (!handle)
	{
		print_message ("open libc.so.6 failed, now exitting!\n");
		exit (-1);
	}

	fclose_real = (int (*)(FILE *))dlsym (handle, "fclose");
	retVal = fclose_real (fp);

	dlclose (handle);
	return retVal;	
}

int scanf_d (const char *format, ...)
{
	va_list arg;
	int done;
	
	va_start (arg, format);
	done = __vfscanf (stdin, format, arg, NULL);
	va_end (arg);

	return done;
}


int fprintf_d (FILE *stream, const char *format, ...)
{
	va_list arg;
	int done;
	va_start (arg, format);
	done = vfprintf (stream, format, arg);
	va_end (arg);

	return done;
}

int fputs_d (const char *str, FILE * fp)
{
	int retVal;
	void * handle; 
	static int (*fputs_real)(const char *, FILE *) = NULL;
	handle = dlopen ("libc.so.6", RTLD_LAZY);

	fputs_real = (int (*)(const char *, FILE *)) dlsym (handle, "fputs");

	retVal = fputs_real (str, fp);

	dlclose (handle);
	return retVal;
}


char * fgets_d (char *buf, int n, FILE * fp)
{
	char * retVal;
	void * handle;
	static char *(*fgets_real)(char *, int, FILE *) = NULL;
	handle = dlopen ("libc.so.6", RTLD_LAZY);

	fgets_real = (char *(*)(char *, int, FILE *))dlsym (handle, "fgets");

	retVal = fgets_real (buf, n, fp);

	dlclose (handle);
	return retVal;
}

size_t fread_d (void *buf, size_t size, size_t count, FILE *fp)
{
	size_t retVal;
	void * handle;
	static size_t (*fread_real)(void *, size_t, size_t, FILE *) = NULL;
	
	handle = dlopen ("libc.so.6", RTLD_LAZY);
	fread_real = (size_t (*)(void *, size_t, size_t, FILE *)) dlsym (handle, "fread");

	retVal = fread_real (buf, size, count, fp);
	
	dlclose (handle);
	return retVal;
}

size_t fwrite_d (const void *buf, size_t size, size_t count, FILE * fp)
{
	size_t retVal;
	void *handle;
	static size_t (*fwrite_real)(const void *, size_t, size_t, FILE *) = NULL;

	handle = dlopen ("libc.so.6", RTLD_LAZY);
	fwrite_real = (size_t (*)(const void *, size_t, size_t, FILE *)) dlsym (handle, "fwrite");

	retVal = fwrite_real (buf, size, count, fp);
	dlclose (handle);
	return retVal;
}

int getchar_d ()
{
	int retVal;
	void *handle;
	static int (*getchar_real) (void) = NULL;
	
	handle = dlopen ("libc.so.6", RTLD_LAZY);
	getchar_real = (int (*)(void)) dlsym (handle, "getchar");

	retVal = getchar_real ();

	dlclose (handle);
	return retVal;
}

int snprintf_d (char *s, size_t maxlen, const char *format, ...)
{
	va_list arg;
	int done;
	
	va_start (arg, format);
	done = __vsnprintf (s, maxlen, format, arg);
	va_end (arg);

	return done;
}

int sprintf_d (char *s, const char *format, ...)
{
	va_list arg;
	int done;

	va_start (arg, format);
	done = vsprintf (s, format, arg);
	va_end (arg);

	return done;
}


int sscanf_d (const char *s, const char *format, ...)
{
	va_list arg;
	int done;

	va_start (arg, format);
	done = __vsscanf (s, format, arg);
	va_end (arg);

	return done;
}


int ungetc_d (int c, FILE *fp)
{
	int retVal;
	void * handle;
	static int (*ungetc_real) (int, FILE *) = NULL;

	handle = dlopen ("libc.so.6", RTLD_LAZY);
	ungetc_real = (int (*)(int, FILE *)) dlsym (handle, "ungetc");
	retVal = ungetc_real (c, fp);

	dlclose (handle);
	return retVal;
}