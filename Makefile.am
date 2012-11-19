SUBDIRS = CUDA MPI OpenMP 
MONITOR_ROOT=$(top_srcdir)/src/monitor
MRNET_ROOT=$(top_srcdir)/src/mrnet4_root

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

MRNCXX = g++
MRNCXXFLAGS = $(DEBUG) -O0 -D__STDC_LIMIT_MACROS -D__STDC_CONSTANT_MACROS -D__STDC_FORMAT_MACROS -Dcompiler_gnu -fno-default-inline -fPIC -Dos_linux=26 $(MRNINCDIRS) $(MRNWARNFLAGS)
MYLDFLAGS=-Wl,-rpath,$(top_srcdir)/src/mysql

IOLIBS=-L$(top_srcdir)/src/io/lib -lio
IOINCS=-I$(top_srcdir)/src/io/include


MYSQLLIBS = -L$(top_srcdir)/src/mysql -lmysqlclient
MRNFELIBS = -L$(MONITOR_ROOT)/.libs -lmrnfe

VPATH=$(MONITOR_ROOT)/lib
CC=mpicc
CXX=mpicxx
#######################################################################################################
bin_PROGRAMS=mpi_recv ompi_recv cmpi_recv
mpi_recv_SOURCES=mpi_recv.cpp mrnfe.h protocol.h libmrnfe.so
mpi_recv_CPPFLAGS=$(MRNCXXFLAGS) $(MYLDFLAGS) $(IOINCS)
mpi_recv_LDFLAGS=$(MRNLDFLAGS)
mpi_recv_LDADD=$(MRNFELIBS) $(MYSQLLIBS) $(MRNLIBS) $(IOLIBS)

ompi_recv_SOURCES=ompi_recv.cpp mrnfe.h protocol.h libmrnfe.so
ompi_recv_CPPFLAGS=$(MRNCXXFLAGS) $(MYLDFLAGS) $(IOINCS)
ompi_recv_LDFLAGS=$(MRNLDFLAGS)
ompi_recv_LDADD=$(MRNFELIBS) $(MYSQLLIBS) $(MRNLIBS) $(IOLIBS)

cmpi_recv_SOURCES=cmpi_recv.cpp mrnfe.h protocol.h libmrnfe.so
cmpi_recv_CPPFLAGS=$(MRNCXXFLAGS) $(MYLDFLAGS) $(IOINCS)
cmpi_recv_LDFLAGS=$(MRNLDFLAGS)
cmpi_recv_LDADD=$(MRNFELIBS) $(MYSQLLIBS) $(MRNLIBS) $(IOLIBS)


#lib_LTLIBRARIES=libmaintrace.la
#libmaintrace_la_SOURCES=Trace_main.c
#libmaintrace_la_CPPFLAGS=-fopenmp

#lib_LTLIBRARIES=libmpisend.la libompisend.la libcmpisend.la libtracer.la libmpiwrapper.la libompiwrapper.la 
#libmpisend_la_SOURCES=Record.cpp wrapper.h mpi_send.cpp Init.c libsend.so
#libmpisend_la_LDFLAGS=$(MRNLDFLAGS)
#libmpisend_la_CPPFLAGS=$(MRNCXXFLAGS) $(MYLDFLAGS) 
#libmpisend_la_LIBADD=$(MRNLIBS) -L$(MONITOR_ROOT)/.libs -lsend

#libompisend_la_SOURCES=Record.cpp wrapper.h ompi_send.cpp Init.c libsend.so
#libompisend_la_LDFLAGS=$(MRNLDFLAGS)
#libompisend_la_CPPFLAGS=$(MRNCXXFLAGS) $(MYLDFLAGS) 
#libompisend_la_LIBADD=$(MRNLIBS) -L$(MONITOR_ROOT)/.libs -lsend

#libcmpisend_la_SOURCES=Record.cpp wrapper.h cmpi_send.cpp Init.c libsend.so
#libcmpisend_la_LDFLAGS=$(MRNLDFLAGS)
#libcmpisend_la_CPPFLAGS=$(MRNCXXFLAGS) $(MYLDFLAGS)
#libcmpisend_la_LIBADD=$(MRNLIBS)

#libtracer_la_SOURCES=Tracer.c Init.c wrapper.h .libs/libmpisend.so
#libtracer_la_CFLAGS= -fPIC -ldl 

#libmpiwrapper_la_SOURCES=Init.c MPI_wrapper.c FMPI_wrapper.c wrapper.h .libs/libmpisend.so
#libmpiwrapper_la_LDFLAGS=$(MRNLDFLAGS) -fPIC -ldl
#libmpiwrapper_la_LIBADD=-L.libs/ -lmpisend -ldl
#libmpiwrapper_la_CPPFLAGS= -fPIC -ldl

#libompiwrapper_la_SOURCES=Init.c MPI_wrapper.c FMPI_wrapper.c OMP_wrapper.c omp_thread_id.c wrapper.h .#libs/libompisend.so
#libompiwrapper_la_LIBADD=-L.libs/ -lompisend -ldl
#libompiwrapper_la_CFLAGS= -fPIC -ldl


#lib_LTLIBRARIES=lib_cmpi_wrapper.la
#lib_cmpi_wrapper_la_SOURCES=Init.c MPI_wrapper.c FMPI_wrapper.c CUDA_wrapper.c wrapper.h lib_cmpi_send.so



