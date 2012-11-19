#include "mrnbe.h"
#include "cfparser.h"
#include "simplenet.h"
#include "wrapper.h"

#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <signal.h>
#include <unistd.h>
#include <cstring>
#include <errno.h>
#include <string>

#include <sys/socket.h>
#include <sys/ioctl.h>
#include <net/if.h>
#include <netinet/in.h>
#include <arpa/inet.h>

using std::string;
using namespace MRNADPT;
using namespace BE;

int Send(Record_Event Event)
{
#ifdef DEBUG
	printf_d("[DEBUG]:\tSend_MPI()\n");
#endif
	int start = 1;
	int finish = 2;

	Stream * stream = BE::GetStream();
	if (!stream)
	{
		printf_d ("stream error\n");
		return 1;
	}
#ifdef DEBUG
	printf_d ("[DEBUG]:\tSend_Data: %d %u %s %d %s %d %x %d %d %d %d %d %d %d %llu %d\n", 
				Event.mpi_rank,
				Event.pid,
				Event.hostname,
				Event.eid,
				Event.event_name,
				Event.type,
				Event.comm,
				Event.tag,
				Event.src_rank,
				Event.dst_rank,
				Event.sendsize,
				Event.sendtype,
				Event.recvsize,
				Event.recvtype,
				Event.starttime,
				start
		);
#endif

	if (Event.eid == -1)
	{
		if (stream->send (PROT_MPIDATA, "%d %ud %s %d %s %d %d %d %d %d %d %d %d %d %uld %d",
				Event.mpi_rank,
				Event.pid,
				Event.hostname,
				0,
				Event.event_name,
				Event.type,
				Event.comm,
				Event.tag,
				Event.src_rank,
				Event.dst_rank,
				Event.sendsize,
				Event.sendtype,
				Event.recvsize,
				Event.recvtype,
				Event.starttime,
				finish) == -1)
		return 1;
		stream->flush ();
	}
	else 
	{
		if (stream->send (PROT_MPIDATA, "%d %ud %s %d %s %d %d %d %d %d %d %d %d %d %uld %d",
				Event.mpi_rank,
				Event.pid,
				Event.hostname,
				Event.eid,
				Event.event_name,
				Event.type,
				Event.comm,
				Event.tag,
				Event.src_rank,
				Event.dst_rank,
				Event.sendsize,
				Event.sendtype,
				Event.recvsize,
				Event.recvtype,
				Event.starttime,
				start) == -1)
		return 1;
		stream->flush ();
	}

	if (Event.eid != 0 && Event.eid != -1)
	{
		if (stream->send (PROT_MPIDATA, "%d %ud %s %d %s %d %d %d %d %d %d %d %d %d %uld %d",
					Event.mpi_rank,
					Event.pid,
					Event.hostname,
					Event.eid,
					Event.event_name,
					Event.type,
					Event.comm,
					Event.tag,
					Event.src_rank,
					Event.dst_rank,
					Event.sendsize,
					Event.sendtype,
					Event.recvsize,
					Event.recvtype,
					Event.endtime,
					finish) == -1)
			return 1;
		stream->flush ();
	}
	return 0;
}

int send_Node (MPI_node_info Node)
{
	Stream * stream = BE::GetStream();
	if (!stream)
		return 1;
	if (stream->send (PROT_NODEDATA, "%ud %d",
				Node.pid,
				Node.mpi_rank) == -1)
		return -1;
	stream->flush ();
	return 0;
}
