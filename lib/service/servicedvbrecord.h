#ifndef __servicedvbrecord_h
#define __servicedvbrecord_h

#include <lib/service/iservice.h>
#include <lib/dvb/idvb.h>

#include <lib/dvb/pmt.h>
#include <lib/dvb/eit.h>
#include <set>

#include <lib/service/servicedvb.h>

class eDVBServiceRecord: public iRecordableService, public Object
{
DECLARE_REF(eDVBServiceRecord);
public:
	RESULT prepare(const char *filename, time_t begTime, time_t endTime, int eit_event_id);
	RESULT start();
	RESULT stop();
private:
	enum { stateIdle, statePrepared, stateRecording };
	int m_state, m_want_record;
	friend class eServiceFactoryDVB;
	eDVBServiceRecord(const eServiceReferenceDVB &ref);
	
	eServiceReferenceDVB m_ref;
	eDVBServicePMTHandler m_service_handler;
	void serviceEvent(int event);
	
	ePtr<iDVBTSRecorder> m_record;
	
	int m_recording, m_tuned;
	std::set<int> m_pids_active;
	std::string m_filename;
	int m_target_fd;
	
	int doPrepare();
	int doRecord();
};

#endif
