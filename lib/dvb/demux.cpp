#include <stdio.h>
#include <fcntl.h>
#include <sys/ioctl.h>
#include <errno.h>
#include <unistd.h>
#include <signal.h>

#if HAVE_DVB_API_VERSION < 3
#include <ost/dmx.h>

#ifndef DMX_SET_NEGFILTER_MASK
	#define DMX_SET_NEGFILTER_MASK   _IOW('o',48,uint8_t *)
#endif

#ifndef DMX_GET_STC
	struct dmx_stc
	{
		unsigned int num;	/* input : which STC? O..N */
		unsigned int base;	/* output: divisor for stc to get 90 kHz clock */
		unsigned long long stc; /* output: src in 'base'*90 kHz units */
	};
	#define DMX_GET_STC		_IOR('o', 50, struct dmx_stc)
#endif

#else
#include <linux/dvb/dmx.h>
#endif

#include "crc32.h"

#include <lib/base/eerror.h>
#include <lib/base/filepush.h>
#include <lib/dvb/idvb.h>
#include <lib/dvb/demux.h>
#include <lib/dvb/esection.h>
#include <lib/dvb/decoder.h>
#include <lib/dvb/pvrparse.h>

eDVBDemux::eDVBDemux(int adapter, int demux): adapter(adapter), demux(demux)
{
	m_dvr_busy = 0;
}

eDVBDemux::~eDVBDemux()
{
}

int eDVBDemux::openDemux(void)
{
	char filename[128];
#if HAVE_DVB_API_VERSION < 3
	snprintf(filename, 128, "/dev/dvb/card%d/demux%d", adapter, demux);
#else
	snprintf(filename, 128, "/dev/dvb/adapter%d/demux%d", adapter, demux);
#endif
	return ::open(filename, O_RDWR);
}

DEFINE_REF(eDVBDemux)

RESULT eDVBDemux::setSourceFrontend(int fenum)
{
#if HAVE_DVB_API_VERSION >= 3
	int fd = openDemux();
	
	int n = DMX_SOURCE_FRONT0 + fenum;
	int res = ::ioctl(fd, DMX_SET_SOURCE, &n);
	if (res)
		eDebug("DMX_SET_SOURCE failed! - %m");
	::close(fd);
	return res;
#endif
	return 0;
}

RESULT eDVBDemux::setSourcePVR(int pvrnum)
{
#if HAVE_DVB_API_VERSION >= 3
	int fd = openDemux();
	int n = DMX_SOURCE_DVR0 + pvrnum;
	int res = ::ioctl(fd, DMX_SET_SOURCE, &n);
	::close(fd);
	return res;
#endif
	return 0;
}

RESULT eDVBDemux::createSectionReader(eMainloop *context, ePtr<iDVBSectionReader> &reader)
{
	RESULT res;
	reader = new eDVBSectionReader(this, context, res);
	if (res)
		reader = 0;
	return res;
}

RESULT eDVBDemux::createTSRecorder(ePtr<iDVBTSRecorder> &recorder)
{
	if (m_dvr_busy)
		return -EBUSY;
	recorder = new eDVBTSRecorder(this);
	return 0;
}

RESULT eDVBDemux::getMPEGDecoder(ePtr<iTSMPEGDecoder> &decoder)
{
	decoder = new eTSMPEGDecoder(this, 0);
	return 0;
}

RESULT eDVBDemux::getSTC(pts_t &pts, int num)
{
	int fd = openDemux();
	
	if (fd < 0)
		return -ENODEV;

	struct dmx_stc stc;
	stc.num = num;
	stc.base = 1;
	
	if (ioctl(fd, DMX_GET_STC, &stc) < 0)
	{
		::close(fd);
		return -1;
	}
	
	pts = stc.stc;
	
	::close(fd);
	return 0;
}

RESULT eDVBDemux::flush()
{
	// FIXME: implement flushing the PVR queue here.
	
	m_event(evtFlush);
	return 0;
}

RESULT eDVBDemux::connectEvent(const Slot1<void,int> &event, ePtr<eConnection> &conn)
{
	conn = new eConnection(this, m_event.connect(event));
	return 0;
}

void eDVBSectionReader::data(int)
{
	__u8 data[4096]; // max. section size
	int r;
	r = ::read(fd, data, 4096);
	if(r < 0)
	{
		eWarning("ERROR reading section - %m\n");
		return;
	}
	if (checkcrc)
	{
			// this check should never happen unless the driver is crappy!
		unsigned int c;
		if ((c = crc32((unsigned)-1, data, r)))
		{
			eDebug("crc32 failed! is %x\n", c);
			return;
		}
	}
	if (active)
		read(data);
	else
		eDebug("data.. but not active");
}

eDVBSectionReader::eDVBSectionReader(eDVBDemux *demux, eMainloop *context, RESULT &res): demux(demux)
{
	char filename[128];
	fd = demux->openDemux();
	
	if (fd >= 0)
	{
		notifier=new eSocketNotifier(context, fd, eSocketNotifier::Read, false);
		CONNECT(notifier->activated, eDVBSectionReader::data);
		res = 0;
	} else
	{
		perror(filename);
		res = errno;
	}
}

DEFINE_REF(eDVBSectionReader)

eDVBSectionReader::~eDVBSectionReader()
{
	if (notifier)
		delete notifier;
	if (fd >= 0)
		::close(fd);
}

RESULT eDVBSectionReader::start(const eDVBSectionFilterMask &mask)
{
	RESULT res;
	if (fd < 0)
		return -ENODEV;

	notifier->start();
#if HAVE_DVB_API_VERSION < 3
	dmxSctFilterParams sct;
#else
	dmx_sct_filter_params sct;
#endif
	sct.pid     = mask.pid;
	sct.timeout = 0;
#if HAVE_DVB_API_VERSION < 3
	sct.flags   = 0;
#else
	sct.flags   = DMX_IMMEDIATE_START;
#endif
	if (mask.flags & eDVBSectionFilterMask::rfCRC)
	{
		sct.flags |= DMX_CHECK_CRC;
		checkcrc = 1;
	} else
		checkcrc = 0;
	
	memcpy(sct.filter.filter, mask.data, DMX_FILTER_SIZE);
	memcpy(sct.filter.mask, mask.mask, DMX_FILTER_SIZE);
#if HAVE_DVB_API_VERSION >= 3
	memcpy(sct.filter.mode, mask.mode, DMX_FILTER_SIZE);
	if (::ioctl(fd, DMX_SET_BUFFER_SIZE, 8192*8) < 0)
		eDebug("DMX_SET_BUFFER_SIZE failed(%m)");
#endif
	
	res = ::ioctl(fd, DMX_SET_FILTER, &sct);
	if (!res)
	{
#if HAVE_DVB_API_VERSION < 3
		res = ::ioctl(fd, DMX_SET_NEGFILTER_MASK, mask.mode);
		if (!res)
		{
			res = ::ioctl(fd, DMX_START, 0);
			if (!res)
				active = 1;
		}
#else
		active = 1;
#endif
	}
	return res;
}

RESULT eDVBSectionReader::stop()
{
	if (!active)
		return -1;

	active=0;
	::ioctl(fd, DMX_STOP);
	notifier->stop();

	return 0;
}

RESULT eDVBSectionReader::connectRead(const Slot1<void,const __u8*> &r, ePtr<eConnection> &conn)
{
	conn = new eConnection(this, read.connect(r));
	return 0;
}

class eDVBRecordFileThread: public eFilePushThread
{
public:
	eDVBRecordFileThread();
	void setTimingPID(int pid);
protected:
	void filterRecordData(const char *data, int len);
private:
	eMPEGStreamParserTS m_ts_parser;
	eMPEGStreamInformation m_stream_info;
	off_t m_current_offset;
	int m_pid;
};

eDVBRecordFileThread::eDVBRecordFileThread()
	: m_ts_parser(m_stream_info)
{
	m_current_offset = 0;
}

void eDVBRecordFileThread::setTimingPID(int pid)
{
	m_ts_parser.setPid(pid);
}

void eDVBRecordFileThread::filterRecordData(const char *data, int len)
{
	m_ts_parser.parseData(m_current_offset, data, len);
	
	m_current_offset += len;
}

DEFINE_REF(eDVBTSRecorder);

eDVBTSRecorder::eDVBTSRecorder(eDVBDemux *demux): m_demux(demux)
{
	m_running = 0;
	m_target_fd = -1;
	m_thread = new eDVBRecordFileThread();
	m_demux->m_dvr_busy = 1;
}

eDVBTSRecorder::~eDVBTSRecorder()
{
	stop();
	delete m_thread;
	m_demux->m_dvr_busy = 0;
}

RESULT eDVBTSRecorder::start()
{
	if (m_running)
		return -1;
	
	if (m_target_fd == -1)
		return -2;
		
	char filename[128];
#if HAVE_DVB_API_VERSION < 3
	snprintf(filename, 128, "/dev/dvb/card%d/dvr%d", m_demux->adapter, m_demux->demux);
#else
	snprintf(filename, 128, "/dev/dvb/adapter%d/dvr%d", m_demux->adapter, m_demux->demux);
#endif
	m_source_fd = ::open(filename, O_RDONLY);
	
	if (m_source_fd < 0)
	{
		eDebug("FAILED to open dvr (%s) in ts recoder (%m)", filename);
		return -3;
	}
	
	m_thread->start(m_source_fd, m_target_fd);
	m_running = 1;
	
	for (std::map<int,int>::iterator i(m_pids.begin()); i != m_pids.end(); ++i)
		startPID(i->first);
	
	return 0;
}

RESULT eDVBTSRecorder::addPID(int pid)
{
	if (m_pids.find(pid) != m_pids.end())
		return -1;
	
	m_pids.insert(std::pair<int,int>(pid, -1));
	if (m_running)
		startPID(pid);
	return 0;
}

RESULT eDVBTSRecorder::removePID(int pid)
{
	if (m_pids.find(pid) == m_pids.end())
		return -1;
		
	if (m_running)
		stopPID(pid);
	
	m_pids.erase(pid);
	return 0;
}

RESULT eDVBTSRecorder::setTimingPID(int pid)
{
	if (m_running)
		return -1;
	m_thread->setTimingPID(pid);
	return 0;
}

RESULT eDVBTSRecorder::setTargetFD(int fd)
{
	m_target_fd = fd;
	return 0;
}

RESULT eDVBTSRecorder::setBoundary(off_t max)
{
	return -1; // not yet implemented
}

RESULT eDVBTSRecorder::stop()
{
	for (std::map<int,int>::iterator i(m_pids.begin()); i != m_pids.end(); ++i)
		stopPID(i->first);

	if (!m_running)
		return -1;
	m_thread->stop();
	
	close(m_source_fd);
	
	return 0;
}

RESULT eDVBTSRecorder::connectEvent(const Slot1<void,int> &event, ePtr<eConnection> &conn)
{
	conn = new eConnection(this, m_event.connect(event));
	return 0;
}

RESULT eDVBTSRecorder::startPID(int pid)
{
	int fd = m_demux->openDemux();
	if (fd < 0)
	{
		eDebug("FAILED to open demux in ts recoder (%m)");
		return -1;
	}

#if HAVE_DVB_API_VERSION < 3
	dmxPesFilterParams flt;
	
	flt.pesType = DMX_PES_OTHER;
#else
	dmx_pes_filter_params flt;
	
	flt.pes_type = DMX_PES_OTHER;
#endif

	flt.pid     = pid;
	flt.input   = DMX_IN_FRONTEND;
	flt.output  = DMX_OUT_TS_TAP;
	
	flt.flags   = DMX_IMMEDIATE_START;

	int res = ::ioctl(fd, DMX_SET_PES_FILTER, &flt);
	if (res < 0)
	{
		eDebug("set pes filter failed!");
		::close(fd);
		return -1;
	}
	m_pids[pid] = fd;

	return 0;
}

void eDVBTSRecorder::stopPID(int pid)
{
	if (m_pids[pid] != -1)
		::close(m_pids[pid]);
	m_pids[pid] = -1;
}
