MONITOR_ROOT=$(top_srcdir)/src/monitor
MRNET_ROOT=$(top_srcdir)/src/mrnet4_root
MYSQL_ROOT=$(top_srcdir)/src/mysql
WRAPPER_ROOT=$(top_srcdir)/src/wrapper

MRNET_PLATFORM = i686-pc-linux-gnu
MPIDIR = /usr/local/mpich2

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

CUPTIFLAGS = -L/usr/local/cuda/extras/CUPTI/lib -lcupti -I /usr/local/cuda/extras/CUPTI/include

PAPIINC = -I$(PAPI)/include
PAPILIBS = -L$(PAPI)/lib -lpapi

IOLIBS=-L$(top_srcdir)/src/io/lib -lio
IOINCS=-I$(top_srcdir)/src/io/include


PAPIFLAGS = $(PAPIINC) $(PAPILIBS)

CC=mpicc
CXX=mpicxx
#################################################################################################

lib_LTLIBRARIES=libcmpisend.la libcuptitrace.la libcmpiwrapper.la 

libcmpisend_la_SOURCES=../Record.cpp ../wrapper.h cmpi_send.cpp ../Init.c libsend.so
libcmpisend_la_LDFLAGS=$(MRNLDFLAGS)
libcmpisend_la_CPPFLAGS=$(IOINCS) $(MRNCXXFLAGS) $(PAPIFLAGS)
libcmpisend_la_LIBADD=$(MRNLIBS) -L$(MONITOR_ROOT)/.libs -lsend $(IOLIBS)

.cu.o:
	$(NVCC) $(NVCCFLAGS)  -c -o $@ $< -I$(MPIDIR)/include/ 

libcuptitrace_la_SOURCES=cupti.c
libcuptitrace_la_LDFLAGS=$(MRNLDFLAGS) -fPIC -ldl $(NVCCFLAGS) $(CUPTIFLAGS) $(IOLIBS)
libcuptitrace_la_LIBADD=-L.libs/ -lcmpisend
libcuptitrace_la_CPPFLAGS= -fPIC -ldl -I$(WRAPPER_ROOT) $(NVCCFLAGS) $(CUPTIFLAGS) $(PAPIFLAGS) $(IOINCS)

#libcupticallback_la_SOURCES=cupti.c
#libcupticallback_la_LDFLAGS=$(MRNLDFLAGS) -fPIC -ldl $(NVCCFLAGS) $(CUPTIFLAGS)
#libcupticallback_la_LIBADD=-L.libs/ -lcmpisend
#libcupticallback_la_CPPFLAGS= -fPIC -ldl -I$(WRAPPER_ROOT) $(NVCCFLAGS) $(CUPTIFLAGS)


libcmpiwrapper_la_SOURCES=../Init.c ../Tracer.c ../MPI/MPI_wrapper.c ../MPI/FMPI_wrapper.c CUDA_wrapper.c ../IO_wrapper.c ../wrapper.h .libs/libcmpisend.so 
libcmpiwrapper_la_LDFLAGS=$(MRNLDFLAGS) -fPIC -ldl $(NVCCFLAGS) 
libcmpiwrapper_la_LIBADD=-L.libs/ -lcmpisend -ldl -L.libs/ -lcuptitrace $(IOLIBS)
libcmpiwrapper_la_CPPFLAGS= -fPIC -ldl -I$(WRAPPER_ROOT) $(NVCCFLAGS) $(PAPIFLAGS) $(IOINCS)

