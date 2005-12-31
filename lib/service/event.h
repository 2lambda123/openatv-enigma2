#ifndef __lib_service_event_h
#define __lib_service_event_h

#ifndef SWIG
#include <time.h>
#include <lib/base/object.h>
#include <lib/service/iservice.h>
#include <list>
#include <string>
class Event;
#endif

struct eComponentData
{
DECLARE_REF(eComponentData);
#ifndef SWIG
	uint8_t m_streamContent;
	uint8_t m_componentType;
	uint8_t m_componentTag;
	std::string m_iso639LanguageCode;
	std::string m_text;
#endif
	int getStreamContent(void) const { return m_streamContent; }
	int getComponentType(void) const { return m_componentType; }
	int getComponentTag(void) const { return m_componentTag; }
	std::string getIso639LanguageCode(void) const { return m_iso639LanguageCode; }
	std::string getText(void) const { return m_text; }
};

TEMPLATE_TYPEDEF(ePtr<eComponentData>, eComponentDataPtr);

class eServiceEvent: public iObject
{
	DECLARE_REF(eServiceEvent);
	bool loadLanguage(Event *event, std::string lang, int tsidonid);
	std::list<eComponentData> m_component_data;
	std::list<eServiceReference> m_linkage_services;
	time_t m_begin;
	int m_duration;
	int m_event_id;
	std::string m_event_name, m_short_description, m_extended_description;
	// .. additional info
public:
#ifndef SWIG
	RESULT parseFrom(Event *evt, int tsidonid=0);
#endif
	time_t getBeginTime() const { return m_begin; }
	int getDuration() const { return m_duration; }
	int getEventId() const { return m_event_id; }
	std::string getEventName() const { return m_event_name; }
	std::string getShortDescription() const { return m_short_description; }
	std::string getExtendedDescription() const { return m_extended_description; }
	std::string getBeginTimeString() const;
	SWIG_VOID(RESULT) getComponentData(ePtr<eComponentData> &SWIG_OUTPUT, int tagnum) const;
	int getNumOfLinkageServices() const { return m_linkage_services.size(); }
	SWIG_VOID(RESULT) getLinkageService(eServiceReference &SWIG_OUTPUT, int num) const;
};

TEMPLATE_TYPEDEF(ePtr<eServiceEvent>, eServiceEventPtr);
#ifndef SWIG

class eDebugClass: public iObject
{
	DECLARE_REF(eDebugClass);
public:
	int x;
	static void getDebug(ePtr<eDebugClass> &ptr, int x) { ptr = new eDebugClass(x); }
	eDebugClass(int i) { printf("build debug class %d\n", i); x = i; }
	~eDebugClass() { printf("remove debug class %d\n", x); }
};

// TEMPLATE_TYPEDEF(ePtr<eDebugClass>, eDebugClassPtr);
#endif

#endif
