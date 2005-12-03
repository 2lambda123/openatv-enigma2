#include <lib/base/eerror.h>
#include <lib/base/estring.h>
#include <lib/service/service.h>
#include <lib/base/init_num.h>
#include <lib/base/init.h>

eServiceReference::eServiceReference(const std::string &string)
{
	const char *c=string.c_str();
	int pathl=-1;
	
	if ( sscanf(c, "%d:%d:%x:%x:%x:%x:%x:%x:%x:%x:%n", &type, &flags, &data[0], &data[1], &data[2], &data[3], &data[4], &data[5], &data[6], &data[7], &pathl) < 8 )
	{
		memset( data, 0, sizeof(data) );
		eDebug("find old format eServiceReference string");
		sscanf(c, "%d:%d:%x:%x:%x:%x:%n", &type, &flags, &data[0], &data[1], &data[2], &data[3], &pathl);
	}

	if (pathl)
		path=c+pathl;
}

std::string eServiceReference::toString() const
{
	std::string ret;
	ret += getNum(type);
	ret += ":";
	ret += getNum(flags);
	for (unsigned int i=0; i<sizeof(data)/sizeof(*data); ++i)
	{
		ret+=":"+ getNum(data[i], 0x10);
	}
	ret+=":"+path;
	return ret;
}


eServiceCenter *eServiceCenter::instance;

eServiceCenter::eServiceCenter()
{
	if (!instance)
	{
		eDebug("settings instance.");
		instance = this;
	}
}

eServiceCenter::~eServiceCenter()
{
	if (instance == this)
	{
		eDebug("clear instance");
		instance = 0;
	}
}

DEFINE_REF(eServiceCenter);

RESULT eServiceCenter::play(const eServiceReference &ref, ePtr<iPlayableService> &ptr)
{
	std::map<int,ePtr<iServiceHandler> >::iterator i = handler.find(ref.type);
	if (i == handler.end())
	{
		ptr = 0;
		return -1;
	}
	return i->second->play(ref, ptr);
}

RESULT eServiceCenter::record(const eServiceReference &ref, ePtr<iRecordableService> &ptr)
{
	std::map<int,ePtr<iServiceHandler> >::iterator i = handler.find(ref.type);
	if (i == handler.end())
	{
		ptr = 0;
		return -1;
	}
	return i->second->record(ref, ptr);
}

RESULT eServiceCenter::list(const eServiceReference &ref, ePtr<iListableService> &ptr)
{
	std::map<int,ePtr<iServiceHandler> >::iterator i = handler.find(ref.type);
	if (i == handler.end())
	{
		ptr = 0;
		return -1;
	}
	return i->second->list(ref, ptr);
}

RESULT eServiceCenter::info(const eServiceReference &ref, ePtr<iStaticServiceInformation> &ptr)
{
	std::map<int,ePtr<iServiceHandler> >::iterator i = handler.find(ref.type);
	if (i == handler.end())
	{
		ptr = 0;
		return -1;
	}
	return i->second->info(ref, ptr);
}

RESULT eServiceCenter::offlineOperations(const eServiceReference &ref, ePtr<iServiceOfflineOperations> &ptr)
{
	std::map<int,ePtr<iServiceHandler> >::iterator i = handler.find(ref.type);
	if (i == handler.end())
	{
		ptr = 0;
		return -1;
	}
	return i->second->offlineOperations(ref, ptr);
}

RESULT eServiceCenter::addServiceFactory(int id, iServiceHandler *hnd)
{
	handler.insert(std::pair<int,ePtr<iServiceHandler> >(id, hnd));
	return 0;
}

RESULT eServiceCenter::removeServiceFactory(int id)
{
	handler.erase(id);
	return 0;
}

	/* default handlers */
RESULT iServiceHandler::info(const eServiceReference &, ePtr<iStaticServiceInformation> &ptr)
{
	ptr = 0;
	return -1;
}

#include <lib/service/event.h>

RESULT iStaticServiceInformation::getEvent(const eServiceReference &ref, ePtr<eServiceEvent> &evt)
{
	evt = 0;
	return -1;
}

int iStaticServiceInformation::getLength(const eServiceReference &ref)
{
	return -1;
}

RESULT iServiceInformation::getEvent(ePtr<eServiceEvent> &evt, int m_nownext)
{	
	evt = 0;
	return -1;
}

int iServiceInformation::getInfo(int w)
{
	return -1;
}

std::string iServiceInformation::getInfoString(int w)
{
	return "";
}

eAutoInitPtr<eServiceCenter> init_eServiceCenter(eAutoInitNumbers::service, "eServiceCenter");
