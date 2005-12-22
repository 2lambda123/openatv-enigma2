#ifndef __epgcache_h_
#define __epgcache_h_

#ifndef SWIG

#include <vector>
#include <list>
#include <ext/hash_map>
#include <ext/hash_set>

#include <errno.h>

#include <lib/dvb/eit.h>
#include <lib/dvb/lowlevel/eit.h>
#include <lib/dvb/idvb.h>
#include <lib/dvb/demux.h>
#include <lib/dvb/dvbtime.h>
#include <lib/base/ebase.h>
#include <lib/base/thread.h>
#include <lib/base/message.h>
#include <lib/service/event.h>

#define CLEAN_INTERVAL 60000    //  1 min
#define UPDATE_INTERVAL 3600000  // 60 min
#define ZAP_DELAY 2000          // 2 sek

#define HILO(x) (x##_hi << 8 | x##_lo)

class eventData;
class eServiceReferenceDVB;

struct uniqueEPGKey
{
	int sid, onid, tsid;
	uniqueEPGKey( const eServiceReference &ref )
		:sid( ref.type != eServiceReference::idInvalid ? ((eServiceReferenceDVB&)ref).getServiceID().get() : -1 )
		,onid( ref.type != eServiceReference::idInvalid ? ((eServiceReferenceDVB&)ref).getOriginalNetworkID().get() : -1 )
		,tsid( ref.type != eServiceReference::idInvalid ? ((eServiceReferenceDVB&)ref).getTransportStreamID().get() : -1 )
	{
	}
	uniqueEPGKey()
		:sid(-1), onid(-1), tsid(-1)
	{
	}
	uniqueEPGKey( int sid, int onid, int tsid )
		:sid(sid), onid(onid), tsid(tsid)
	{
	}
	bool operator <(const uniqueEPGKey &a) const
	{
		return memcmp( &sid, &a.sid, sizeof(int)*3)<0;
	}
	operator bool() const
	{ 
		return !(sid == -1 && onid == -1 && tsid == -1); 
	}
	bool operator==(const uniqueEPGKey &a) const
	{
		return !memcmp( &sid, &a.sid, sizeof(int)*3);
	}
	struct equal
	{
		bool operator()(const uniqueEPGKey &a, const uniqueEPGKey &b) const
		{
			return !memcmp( &a.sid, &b.sid, sizeof(int)*3);
		}
	};
};

//eventMap is sorted by event_id
#define eventMap std::map<__u16, eventData*>
//timeMap is sorted by beginTime
#define timeMap std::map<time_t, eventData*>

#define channelMapIterator std::map<iDVBChannel*, channel_data*>::iterator
#define updateMap std::map<eDVBChannelID, time_t>

struct hash_uniqueEPGKey
{
	inline size_t operator()( const uniqueEPGKey &x) const
	{
		return (x.onid << 16) | x.tsid;
	}
};

#define tidMap std::set<__u32>
#if defined(__GNUC__) && ((__GNUC__ == 3 && __GNUC_MINOR__ >= 1) || __GNUC__ == 4 )  // check if gcc version >= 3.1
	#define eventCache __gnu_cxx::hash_map<uniqueEPGKey, std::pair<eventMap, timeMap>, hash_uniqueEPGKey, uniqueEPGKey::equal>
#else // for older gcc use following
	#define eventCache std::hash_map<uniqueEPGKey, std::pair<eventMap, timeMap>, hash_uniqueEPGKey, uniqueEPGKey::equal >
#endif

#define descriptorPair std::pair<int,__u8*>
#define descriptorMap std::map<__u32, descriptorPair >

class eventData
{
	friend class eEPGCache;
private:
	__u8* EITdata;
	__u8 ByteSize;
	__u8 type;
	static descriptorMap descriptors;
	static __u8 data[4108];
	static int CacheSize;
	static void load(FILE *);
	static void save(FILE *);
public:
	eventData(const eit_event_struct* e=NULL, int size=0, int type=0);
	~eventData();
	const eit_event_struct* get() const;
	operator const eit_event_struct*() const
	{
		return get();
	}
	int getEventID()
	{
		return (EITdata[0] << 8) | EITdata[1];
	}
	time_t getStartTime()
	{
		return parseDVBtime(EITdata[2], EITdata[3], EITdata[4], EITdata[5], EITdata[6]);
	}
	int getDuration()
	{
		return fromBCD(EITdata[7])*3600+fromBCD(EITdata[8])*60+fromBCD(EITdata[9]);
 }
};
#endif

class eEPGCache: public eMainloop, private eThread, public Object
{
#ifndef SWIG
	DECLARE_REF(eEPGCache)
	struct channel_data: public Object
	{
		channel_data(eEPGCache*);
		eEPGCache *cache;
		eTimer abortTimer, zapTimer;
		int prevChannelState;
		__u8 state, isRunning, haveData, can_delete;
		ePtr<eDVBChannel> channel;
		ePtr<eConnection> m_stateChangedConn, m_NowNextConn, m_ScheduleConn, m_ScheduleOtherConn;
		ePtr<iDVBSectionReader> m_NowNextReader, m_ScheduleReader, m_ScheduleOtherReader;
		tidMap seenSections[3], calcedSections[3];
		void readData(const __u8 *data);
		void startChannel();
		void startEPG();
		bool finishEPG();
		void abortEPG();
		void abortNonAvail();
	};
public:
	enum {NOWNEXT=1, SCHEDULE=2, SCHEDULE_OTHER=4};
	struct Message
	{
		enum
		{
			flush,
			startChannel,
			leaveChannel,
			pause,
			restart,
			updated,
			isavail,
			quit,
			timeChanged
		};
		int type;
		iDVBChannel *channel;
		uniqueEPGKey service;
		union {
			int err;
			time_t time;
			bool avail;
		};
		Message()
			:type(0), time(0) {}
		Message(int type)
			:type(type) {}
		Message(int type, bool b)
			:type(type), avail(b) {}
		Message(int type, iDVBChannel *channel, int err=0)
			:type(type), channel(channel), err(err) {}
		Message(int type, const eServiceReference& service, int err=0)
			:type(type), service(service), err(err) {}
		Message(int type, time_t time)
			:type(type), time(time) {}
	};
	eFixedMessagePump<Message> messages;
private:
	friend class channel_data;
	static eEPGCache *instance;

	eTimer cleanTimer;
	std::map<iDVBChannel*, channel_data*> m_knownChannels;
	ePtr<eConnection> m_chanAddedConn;

	eventCache eventDB;
	updateMap channelLastUpdated;
	static pthread_mutex_t cache_lock, channel_map_lock;

	void thread();  // thread function

// called from epgcache thread
	void save();
	void load();
	void sectionRead(const __u8 *data, int source, channel_data *channel);
	void gotMessage(const Message &message);
	void flushEPG(const uniqueEPGKey & s=uniqueEPGKey());
	void cleanLoop();

// called from main thread
	void timeUpdated();
	void DVBChannelAdded(eDVBChannel*);
	void DVBChannelStateChanged(iDVBChannel*);
	void DVBChannelRunning(iDVBChannel *);

	timeMap::iterator m_timemap_cursor, m_timemap_end;
	int currentQueryTsidOnid; // needed for getNextTimeEntry.. only valid until next startTimeQuery call
#endif // SWIG
public:
	static eEPGCache *getInstance() { return instance; }
#ifndef SWIG
	eEPGCache();
	~eEPGCache();
#endif

	// called from main thread
	inline void Lock();
	inline void Unlock();

	// at moment just for one service..
	RESULT startTimeQuery(const eServiceReference &service, time_t begin=-1, int minutes=-1);

#ifndef SWIG
	// eventData's are plain entrys out of the cache.. it's not safe to use them after cache unlock
	// but its faster in use... its not allowed to delete this pointers via delete or free..
	SWIG_VOID(RESULT) lookupEventId(const eServiceReference &service, int event_id, const eventData *&SWIG_OUTPUT);
	SWIG_VOID(RESULT) lookupEventTime(const eServiceReference &service, time_t, const eventData *&SWIG_OUTPUT);
	SWIG_VOID(RESULT) getNextTimeEntry(const eventData *&SWIG_OUTPUT);

	// eit_event_struct's are plain dvb eit_events .. it's not safe to use them after cache unlock
	// its not allowed to delete this pointers via delete or free..
	RESULT lookupEventId(const eServiceReference &service, int event_id, const eit_event_struct *&);
	RESULT lookupEventTime(const eServiceReference &service, time_t , const eit_event_struct *&);
	RESULT getNextTimeEntry(const eit_event_struct *&);

	// Event's are parsed epg events.. it's safe to use them after cache unlock
	// after use this Events must be deleted (memleaks)
	RESULT lookupEventId(const eServiceReference &service, int event_id, Event* &);
	RESULT lookupEventTime(const eServiceReference &service, time_t, Event* &);
	RESULT getNextTimeEntry(Event *&);
#endif

	// eServiceEvent are parsed epg events.. it's safe to use them after cache unlock
	// for use from python ( members: m_start_time, m_duration, m_short_description, m_extended_description )
	SWIG_VOID(RESULT) lookupEventId(const eServiceReference &service, int event_id, ePtr<eServiceEvent> &SWIG_OUTPUT);
	SWIG_VOID(RESULT) lookupEventTime(const eServiceReference &service, time_t, ePtr<eServiceEvent> &SWIG_OUTPUT);
	SWIG_VOID(RESULT) getNextTimeEntry(ePtr<eServiceEvent> &SWIG_OUTPUT);
};

#ifndef SWIG
inline void eEPGCache::Lock()
{
	pthread_mutex_lock(&cache_lock);
}

inline void eEPGCache::Unlock()
{
	pthread_mutex_unlock(&cache_lock);
}
#endif

#endif
