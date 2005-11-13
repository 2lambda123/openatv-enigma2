#ifndef __servicefs_h
#define __servicefs_h

#include <lib/service/iservice.h>

class eServiceFactoryFS: public iServiceHandler
{
DECLARE_REF(eServiceFactoryFS);
public:
	eServiceFactoryFS();
	virtual ~eServiceFactoryFS();
	enum { id = 0x2 };

		// iServiceHandler
	RESULT play(const eServiceReference &, ePtr<iPlayableService> &ptr);
	RESULT record(const eServiceReference &, ePtr<iRecordableService> &ptr);
	RESULT list(const eServiceReference &, ePtr<iListableService> &ptr);
	RESULT info(const eServiceReference &, ePtr<iStaticServiceInformation> &ptr);
	RESULT offlineOperations(const eServiceReference &, ePtr<iServiceOfflineOperations> &ptr);
private:
	ePtr<iStaticServiceInformation> m_service_information;
};

class eServiceFS: public iListableService
{
DECLARE_REF(eServiceFS);
private:
	std::string path;
	friend class eServiceFactoryFS;
	eServiceFS(const char *path);
	
	int m_list_valid;
	std::list<eServiceReference> m_list;
public:
	virtual ~eServiceFS();
	
	RESULT getContent(std::list<eServiceReference> &list);
	RESULT getNext(eServiceReference &ptr);
	int compareLessEqual(const eServiceReference &, const eServiceReference &);
	RESULT startEdit(ePtr<iMutableServiceList> &);
};

#endif
