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

IOLIBS=-L$(top_srcdir)/src/io/lib -lio
IOINCS=-I$(top_srcdir)/src/io/include

CUPTIFLAGS = -L/usr/local/cuda/extras/CUPTI/lib -lcupti -I /usr/local/cuda/extras/CUPTI/include

CC=mpicc
CXX=mpicxx
VPATH = ..
#################################################################################################

lib_LTLIBRARIES=libmpisend.la libmpiwrapper.la 

libmpisend_la_SOURCES=../Record.cpp ../wrapper.h mpi_send.cpp ../Init.c libsend.so
libmpisend_la_LDFLAGS=$(MRNLDFLAGS)
libmpisend_la_CPPFLAGS=$(MRNCXXFLAGS) $(MYLDFLAGS) $(PAPIINC) $(PAPILIBS) $(IOINCS)
libmpisend_la_LIBADD=$(MRNLIBS) -L$(MONITOR_ROOT)/.libs -lsend $(IOLIBS)

libmpiwrapper_la_SOURCES=../Init.c ../Tracer.c MPI_wrapper.c FMPI_wrapper.c ../IO_wrapper.c  ../wrapper.h .libs/libmpisend.so
libmpiwrapper_la_LDFLAGS=$(MRNLDFLAGS) -fPIC -ldl $(CUPTIFLAGS) $(NVCCFLAGS)
libmpiwrapper_la_LIBADD=-L.libs/ -lmpisend -ldl -L../CUDA/.libs/ -lcuptitrace $(IOLIBS)
libmpiwrapper_la_CFLAGS= -fPIC -ldl -I$(WRAPPER_ROOT) $(CUPTIFLAGS) $(NVCCFLAGS) $(PAPIINC) $(PAPILIBS) $(IOINCS)

#libtracer_la_SOURCES=Tracer.c ../Init.c ../wrapper.h .libs/libmpisend.so
#libtracer_la_CFLAGS= -fPIC -I$(WRAPPER_ROOT) $(CUPTIFLAGS) $(NVCCFLAGS)
#libtracer_la_LDFLAGS=-fPIC  $(CUPTIFLAGS) $(NVCCFLAGS)
#libtracer_la_LIBADD= -L../CUDA/.libs/ -lcuptitrace

