#ifndef __db_h
#define __db_h

#ifndef SWIG
#include <lib/base/eptrlist.h>
#include <lib/dvb/idvb.h>
#include <set>
class ServiceDescriptionSection;
#endif

class eDVBDB: public iDVBChannelList
{
	static eDVBDB *instance;
DECLARE_REF(eDVBDB);
	friend class eDVBDBQuery;
	friend class eDVBDBBouquetQuery;
	friend class eDVBDBSatellitesQuery;
	friend class eDVBDBProvidersQuery;


	struct channel
	{
		ePtr<iDVBFrontendParameters> m_frontendParameters;
	};
	
	std::map<eDVBChannelID, channel> m_channels;
	
	std::map<eServiceReferenceDVB, ePtr<eDVBService> > m_services;
	
	std::map<std::string, eBouquet> m_bouquets;
#ifdef SWIG
	eDVBDB();
	~eDVBDB();
#endif
public:
	RESULT removeService(eServiceReferenceDVB service);
	RESULT removeServices(eDVBChannelID chid, unsigned int orb_pos);
	RESULT addFlag(eServiceReferenceDVB service, unsigned int flagmask);
	RESULT removeFlag(eServiceReferenceDVB service, unsigned int flagmask);
	RESULT removeFlags(unsigned int flagmask, eDVBChannelID chid, unsigned int orb_pos);
#ifndef SWIG
// iDVBChannelList
	RESULT addChannelToList(const eDVBChannelID &id, iDVBFrontendParameters *feparm);
	RESULT removeChannel(const eDVBChannelID &id);

	RESULT getChannelFrontendData(const eDVBChannelID &id, ePtr<iDVBFrontendParameters> &parm);
	
	RESULT addService(const eServiceReferenceDVB &service, eDVBService *service);
	RESULT getService(const eServiceReferenceDVB &reference, ePtr<eDVBService> &service);
	RESULT flush();

	RESULT startQuery(ePtr<iDVBChannelListQuery> &query, eDVBChannelQuery *query, const eServiceReference &source);

	RESULT getBouquet(const eServiceReference &ref, eBouquet* &bouquet);
//////
	void saveServicelist();
	void loadBouquet(const char *path);
	eServiceReference searchReference(int tsid, int onid, int sid);
	eDVBDB();
	virtual ~eDVBDB();
#endif
	static eDVBDB *getInstance() { return instance; }
	void reloadServicelist();
	void reloadBouquets();
};

#ifndef SWIG
	// we have to add a possibility to invalidate here.
class eDVBDBQueryBase: public iDVBChannelListQuery
{
DECLARE_REF(eDVBDBQueryBase);
protected:
	ePtr<eDVBDB> m_db;
	ePtr<eDVBChannelQuery> m_query;
	eServiceReference m_source;
public:
	eDVBDBQueryBase(eDVBDB *db, const eServiceReference &source, eDVBChannelQuery *query);
	virtual int compareLessEqual(const eServiceReferenceDVB &a, const eServiceReferenceDVB &b);
};

class eDVBDBQuery: public eDVBDBQueryBase
{
	std::map<eServiceReferenceDVB, ePtr<eDVBService> >::iterator m_cursor;
public:
	eDVBDBQuery(eDVBDB *db, const eServiceReference &source, eDVBChannelQuery *query);
	RESULT getNextResult(eServiceReferenceDVB &ref);
};

class eDVBDBBouquetQuery: public eDVBDBQueryBase
{
	std::list<eServiceReference>::iterator m_cursor;
public:
	eDVBDBBouquetQuery(eDVBDB *db, const eServiceReference &source, eDVBChannelQuery *query);
	RESULT getNextResult(eServiceReferenceDVB &ref);
};

class eDVBDBListQuery: public eDVBDBQueryBase
{
protected:
	std::list<eServiceReferenceDVB> m_list;
	std::list<eServiceReferenceDVB>::iterator m_cursor;
public:
	eDVBDBListQuery(eDVBDB *db, const eServiceReference &source, eDVBChannelQuery *query);
	RESULT getNextResult(eServiceReferenceDVB &ref);
	int compareLessEqual(const eServiceReferenceDVB &a, const eServiceReferenceDVB &b);
};

class eDVBDBSatellitesQuery: public eDVBDBListQuery
{
public:
	eDVBDBSatellitesQuery(eDVBDB *db, const eServiceReference &source, eDVBChannelQuery *query);
};

class eDVBDBProvidersQuery: public eDVBDBListQuery
{
public:
	eDVBDBProvidersQuery(eDVBDB *db, const eServiceReference &source, eDVBChannelQuery *query);
};
#endif // SWIG

#endif
