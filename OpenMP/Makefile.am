MONITOR_ROOT=$(top_srcdir)/src/monitor
MRNET_ROOT=$(top_srcdir)/src/mrnet4_root
MYSQL_ROOT=$(top_srcdir)/src/mysql
WRAPPER_ROOT=$(top_srcdir)/src/wrapper

MRNET_PLATFORM = i686-pc-linux-gnu


MRNLIBDIR  = $(MRNET_ROOT)/lib
MRNLIBMRNET             = -lmrnet
MRNLIBXPLAT             = -lxplat
MRNLIBS                 = -L$(MRNLIBDIR) $(MRNLIBMRNET) $(MRNLIBXPLAT) -lm -lpthread -ldl 

DEBUG = -g

MRNLDFLAGS =  -Wl,-E

MRNINCDIRS = -I$(MRNET_ROOT)/include -I$(MRNET_ROOT)/xplat/include 

MRNWARNFLAGS = -Wall \
	-Wno-system-headers \
	-Wfloat-equal \
	-Wconversion \
	-Wshadow \
	-Wpointer-arith \
	-Wcast-qual \
	-Wcast-align \
	-Wwrite-strings \
	-Wsign-compare \
	-Wredundant-decls \
	-Wlong-long


MRNCXXFLAGS = $(DEBUG) -O0 -D__STDC_LIMIT_MACROS -D__STDC_CONSTANT_MACROS -D__STDC_FORMAT_MACROS -Dcompiler_gnu -fno-default-inline -fPIC -Dos_linux=26 $(MRNINCDIRS) $(MRNWARNFLAGS) -I$(WRAPPER_ROOT)

MYLDFLAGS=-Wl,-rpath,$(top_srcdir)/src/mysql

MYSQLLIBS = -L$(MYSQL_ROOT) -lmysqlclient
MRNFELIBS = -L$(MONITOR_ROOT)/.libs -lmrnfe

PAPIINC = -I$(PAPI)/include
PAPILIBS = -L$(PAPI)/lib -lpapi
PAPIFLAGS = $(PAPIINC) $(PAPILIBS)

IOLIBS=-L$(top_srcdir)/src/io/lib -lio
IOINCS=-I$(top_srcdir)/src/io/include

CC=mpicc
CXX=mpicxx

#################################################################################################

lib_LTLIBRARIES=libompisend.la libompiwrapper.la libompwrapper.la

libompisend_la_SOURCES=../Record.cpp ../wrapper.h ompi_send.cpp ../Init.c libsend.so
libompisend_la_LDFLAGS=$(MRNLDFLAGS)
libompisend_la_CPPFLAGS=$(MRNCXXFLAGS) $(MYLDFLAGS) $(PAPIFLAGS) $(IOINCS)
libompisend_la_LIBADD=$(MRNLIBS) -L$(MONITOR_ROOT)/.libs -lsend $(IOLIBS)

libompiwrapper_la_SOURCES=../Init.c ../Tracer.c ../MPI/MPI_wrapper.c ../MPI/FMPI_wrapper.c OMP_wrapper.c ../IO_wrapper.c omp_thread_id.c ../wrapper.h .libs/libompisend.so
libompiwrapper_la_LIBADD=-L.libs/ -lompisend -ldl -L../CUDA/.libs/ -lcuptitrace $(IOLIBS)
libompiwrapper_la_CFLAGS= -fPIC -ldl -I$(WRAPPER_ROOT) $(PAPIFLAGS) -fopenmp $(IOINCS)

libompwrapper_la_SOURCES=OMP_Record.c ../wrapper.h ../Init.c ../IO_wrapper.c omp_thread_id.c OMP_wrapper.c
libompwrapper_la_CFLAGS=-fPIC -ldl -I$(WRAPPER_ROOT) $(PAPIFLAGS) -fopenmp $(IOINCS) $(MYLDFLAGS)
libompwrapper_la_LIBADD=$(MYSQLLIBS) $(IOLIBS)
