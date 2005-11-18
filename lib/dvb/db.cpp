#include <errno.h>
#include <lib/dvb/db.h>
#include <lib/dvb/frontend.h>
#include <lib/base/eerror.h>
#include <lib/base/estring.h>
#include <dvbsi++/service_description_section.h>
#include <dvbsi++/descriptor_tag.h>
#include <dvbsi++/service_descriptor.h>
#include <dvbsi++/satellite_delivery_system_descriptor.h>

DEFINE_REF(eDVBService);

RESULT eBouquet::addService(const eServiceReference &ref)
{
	list::iterator it =
		std::find(m_services.begin(), m_services.end(), ref);
	if ( it != m_services.end() )
		return -1;
	m_services.push_back(ref);
	return 0;
}

RESULT eBouquet::removeService(const eServiceReference &ref)
{
	list::iterator it =
		std::find(m_services.begin(), m_services.end(), ref);
	if ( it == m_services.end() )
		return -1;
	m_services.erase(it);
	return 0;
}

RESULT eBouquet::moveService(const eServiceReference &ref, unsigned int pos)
{
	if ( pos < 0 || pos >= m_services.size() )
		return -1;
	list::iterator source=m_services.end();
	list::iterator dest=m_services.end();
	for (list::iterator it(m_services.begin()); it != m_services.end(); ++it)
	{
		if (dest == m_services.end() && !pos--)
			dest = it;
		if (*it == ref)
			source = it;
		if (dest != m_services.end() && source != m_services.end())
			break;
	}
	if (dest == m_services.end() || source == m_services.end() || source == dest)
		return -1;
	std::iter_swap(source,dest);
	return 0;
}

RESULT eBouquet::flushChanges()
{
	FILE *f=fopen(m_path.c_str(), "wt");
	if (!f)
		return -1;
	if ( fprintf(f, "#NAME %s\r\n", m_bouquet_name.c_str()) < 0 )
		goto err;
	for (list::iterator i(m_services.begin()); i != m_services.end(); ++i)
	{
		eServiceReference tmp = *i;
		std::string str = tmp.path;
		if ( (i->flags&eServiceReference::flagDirectory) == eServiceReference::flagDirectory )
		{
			unsigned int p1 = str.find("FROM BOUQUET \"");
			if (p1 == std::string::npos)
			{
				eDebug("doof... kaputt");
				continue;
			}
			str.erase(0, p1+14);
			p1 = str.find("\"");
			if (p1 == std::string::npos)
			{
				eDebug("doof2... kaputt");
				continue;
			}
			str.erase(p1);
			tmp.path=str;
		}
		if ( fprintf(f, "#SERVICE %s\r\n", tmp.toString().c_str()) < 0 )
			goto err;
		if ( i->name.length() )
			if ( fprintf(f, "#DESCRIPTION %s\r\n", i->name.c_str()) < 0 )
				goto err;
	}
	fclose(f);
	return 0;
err:
	fclose(f);
	eDebug("couldn't write file %s", m_path.c_str());
	return -1;
}

eDVBService::eDVBService()
{
}

eDVBService::~eDVBService()
{
}

eDVBService &eDVBService::operator=(const eDVBService &s)
{
	m_service_name = s.m_service_name;
	m_service_name_sort = s.m_service_name_sort;
	m_provider_name = s.m_provider_name;
	m_flags = s.m_flags;
	m_ca = s.m_ca;
	m_cache = s.m_cache;
	return *this;
}

void eDVBService::genSortName()
{
	m_service_name_sort = removeDVBChars(m_service_name);
	makeUpper(m_service_name_sort);
	while ((!m_service_name_sort.empty()) && m_service_name_sort[0] == ' ')
		m_service_name_sort.erase(0, 1);
	
		/* put unnamed services at the end, not at the beginning. */
	if (m_service_name_sort.empty())
		m_service_name_sort = "\xFF";
}

RESULT eDVBService::getName(const eServiceReference &ref, std::string &name)
{
	if (!ref.name.empty())
		name = ref.name;
	else if (!m_service_name.empty())
		name = m_service_name;
	else
		name = "(...)";
	return 0;
}

int eDVBService::getLength(const eServiceReference &ref)
{
	return -1;
}

int eDVBService::checkFilter(const eServiceReferenceDVB &ref, const eDVBChannelQuery &query)
{
	int res = 0;
	switch (query.m_type)
	{
	case eDVBChannelQuery::tName:
		res = m_service_name_sort.find(query.m_string) != std::string::npos;
		break;
	case eDVBChannelQuery::tProvider:
		res = m_provider_name.find(query.m_string) != std::string::npos;
		break;
	case eDVBChannelQuery::tType:
		res = ref.getServiceType() == query.m_int;
		break;
	case eDVBChannelQuery::tBouquet:
		res = 0;
		break;
	case eDVBChannelQuery::tSatellitePosition:
		res = (ref.getDVBNamespace().get() >> 16) == query.m_int;
		break;
	case eDVBChannelQuery::tChannelID:
	{
		eDVBChannelID chid;
		ref.getChannelID(chid);
		res = chid == query.m_channelid;
		break;
	}
	case eDVBChannelQuery::tAND:
		res = checkFilter(ref, *query.m_p1) && checkFilter(ref, *query.m_p2);
		break;
	case eDVBChannelQuery::tOR:
		res = checkFilter(ref, *query.m_p1) || checkFilter(ref, *query.m_p2);
		break;
	}

	if (query.m_inverse)
		return !res;
	else
		return res;
}

int eDVBService::getCachePID(cacheID id)
{
	std::map<int, int>::iterator it = m_cache.find(id);
	if ( it != m_cache.end() )
		return it->second;
	return -1;
}

void eDVBService::setCachePID(cacheID id, int pid)
{
	m_cache[id] = pid;
}

DEFINE_REF(eDVBDB);

	/* THIS CODE IS BAD. it should be replaced by somethine better. */
void eDVBDB::load()
{
	eDebug("---- opening lame channel db");
	FILE *f=fopen("lamedb", "rt");
	if (!f)
		return;
	char line[256];
	if ((!fgets(line, 256, f)) || strncmp(line, "eDVB services", 13))
	{
		eDebug("not a servicefile");
		fclose(f);
		return;
	}
	eDebug("reading services");
	if ((!fgets(line, 256, f)) || strcmp(line, "transponders\n"))
	{
		eDebug("services invalid, no transponders");
		fclose(f);
		return;
	}

	// clear all transponders

	while (!feof(f))
	{
		if (!fgets(line, 256, f))
			break;
		if (!strcmp(line, "end\n"))
			break;
		int dvb_namespace=-1, transport_stream_id=-1, original_network_id=-1;
		sscanf(line, "%x:%x:%x", &dvb_namespace, &transport_stream_id, &original_network_id);
		if (original_network_id == -1)
			continue;
		eDVBChannelID channelid = eDVBChannelID(
			eDVBNamespace(dvb_namespace),
			eTransportStreamID(transport_stream_id),
			eOriginalNetworkID(original_network_id));

		ePtr<eDVBFrontendParameters> feparm = new eDVBFrontendParameters;
		while (!feof(f))
		{
			fgets(line, 256, f);
			if (!strcmp(line, "/\n"))
				break;
			if (line[1]=='s')
			{
				eDVBFrontendParametersSatellite sat;
				int frequency, symbol_rate, polarisation, fec, orbital_position, inversion;
				sscanf(line+2, "%d:%d:%d:%d:%d:%d", &frequency, &symbol_rate, &polarisation, &fec, &orbital_position, &inversion);
				sat.frequency = frequency;
				sat.symbol_rate = symbol_rate;
				sat.polarisation = polarisation;
				sat.fec = fec;
				sat.orbital_position = orbital_position;
				sat.inversion = inversion;
				feparm->setDVBS(sat);
			} else if (line[1]=='t')
			{
				eDVBFrontendParametersTerrestrial ter;
				int frequency, bandwidth, code_rate_HP, code_rate_LP, modulation, transmission_mode, guard_interval, hierarchy, inversion;
				sscanf(line+2, "%d:%d:%d:%d:%d:%d:%d:%d:%d", &frequency, &bandwidth, &code_rate_HP, &code_rate_LP, &modulation, &transmission_mode, &guard_interval, &hierarchy, &inversion);
				ter.frequency = frequency;
				ter.bandwidth = bandwidth;
				ter.code_rate_HP = code_rate_HP;
				ter.code_rate_LP = code_rate_LP;
				ter.modulation = modulation;
				ter.transmission_mode = transmission_mode;
				ter.guard_interval = guard_interval;
				ter.hierarchy = hierarchy;
				ter.inversion = inversion;
				
				feparm->setDVBT(ter);
			} else if (line[1]=='c')
			{
				int frequency, symbol_rate, inversion=0, modulation=3;
				sscanf(line+2, "%d:%d:%d:%d", &frequency, &symbol_rate, &inversion, &modulation);
//				t.setCable(frequency, symbol_rate, inversion, modulation);
			}
		}
		addChannelToList(channelid, feparm);
	}

	if ((!fgets(line, 256, f)) || strcmp(line, "services\n"))
	{
		eDebug("services invalid, no services");
		return;
	}

	// clear all services

	int count=0;

	while (!feof(f))
	{
		if (!fgets(line, 256, f))
			break;
		if (!strcmp(line, "end\n"))
			break;

		int service_id=-1, dvb_namespace, transport_stream_id=-1, original_network_id=-1, service_type=-1, service_number=-1;
		sscanf(line, "%x:%x:%x:%x:%d:%d", &service_id, &dvb_namespace, &transport_stream_id, &original_network_id, &service_type, &service_number);
		if (service_number == -1)
			continue;
		ePtr<eDVBService> s = new eDVBService;
		eServiceReferenceDVB ref =
						eServiceReferenceDVB(
						eDVBNamespace(dvb_namespace),
						eTransportStreamID(transport_stream_id),
						eOriginalNetworkID(original_network_id),
						eServiceID(service_id),
						service_type);
		count++;
		fgets(line, 256, f);
		if (strlen(line))
			line[strlen(line)-1]=0;

		s->m_service_name = line;
		s->genSortName();
		 
		fgets(line, 256, f);
		if (strlen(line))
			line[strlen(line)-1]=0;

		std::string str=line;

		if (str[1]!=':')	// old ... (only service_provider)
		{
			s->m_provider_name=line;
		} else
			while ((!str.empty()) && str[1]==':') // new: p:, f:, c:%02d...
			{
				unsigned int c=str.find(',');
				char p=str[0];
				std::string v;
				if (c == std::string::npos)
				{
					v=str.substr(2);
					str="";
				} else
				{
					v=str.substr(2, c-2);
					str=str.substr(c+1);
				}
//				eDebug("%c ... %s", p, v.c_str());
				if (p == 'p')
					s->m_provider_name=v;
				else if (p == 'f')
				{
					sscanf(v.c_str(), "%x", &s->m_flags);
				} else if (p == 'c')
				{
					int cid, val;
					sscanf(v.c_str(), "%02d%04x", &cid, &val);
					s->m_cache[cid]=val;
				} else if (p == 'C')
				{
					int val;
					sscanf(v.c_str(), "%04x", &val);
					s->m_ca.insert(val);
				}
			}
		addService(ref, s);
	}

	eDebug("loaded %d services", count);

	fclose(f);
}

void eDVBDB::save()
{
	eDebug("---- saving lame channel db");
	FILE *f=fopen("lamedb", "wt");
	int channels=0, services=0;
	if (!f)
		eFatal("couldn't save lame channel db!");
	fprintf(f, "eDVB services /3/\n");
	fprintf(f, "transponders\n");
	for (std::map<eDVBChannelID, channel>::const_iterator i(m_channels.begin());
			i != m_channels.end(); ++i)
	{
		const eDVBChannelID &chid = i->first;
		const channel &ch = i->second;

		fprintf(f, "%08x:%04x:%04x\n", chid.dvbnamespace.get(),
				chid.transport_stream_id.get(), chid.original_network_id.get());
		eDVBFrontendParametersSatellite sat;
		eDVBFrontendParametersTerrestrial ter;
		if (!ch.m_frontendParameters->getDVBS(sat))
		{
			fprintf(f, "\ts %d:%d:%d:%d:%d:%d\n",
				sat.frequency, sat.symbol_rate,
				sat.polarisation, sat.fec, sat.orbital_position,
				sat.inversion);
		}
		if (!ch.m_frontendParameters->getDVBT(ter))
		{
			fprintf(f, "\tt %d:%d:%d:%d:%d:%d:%d:%d:%d\n",
				ter.frequency, ter.bandwidth, ter.code_rate_HP,
				ter.code_rate_LP, ter.modulation, ter.transmission_mode,
				ter.guard_interval, ter.hierarchy, ter.inversion);
		}
		fprintf(f, "/\n");
		channels++;
	}
	fprintf(f, "end\nservices\n");

	for (std::map<eServiceReferenceDVB, ePtr<eDVBService> >::iterator i(m_services.begin());
		i != m_services.end(); ++i)
	{
		const eServiceReferenceDVB &s = i->first;
		fprintf(f, "%04x:%08x:%04x:%04x:%d:%d\n",
				s.getServiceID().get(), s.getDVBNamespace().get(),
				s.getTransportStreamID().get(),s.getOriginalNetworkID().get(),
				s.getServiceType(),
				0);

		fprintf(f, "%s\n", i->second->m_service_name.c_str());
		fprintf(f, "p:%s", i->second->m_provider_name.c_str());

		// write cached pids
		for (std::map<int,int>::const_iterator ca(i->second->m_cache.begin());
			ca != i->second->m_cache.end(); ++ca)
			fprintf(f, ",c:%02d%04x", ca->first, ca->second);

		// write cached ca pids
		for (std::set<int>::const_iterator ca(i->second->m_ca.begin());
			ca != i->second->m_ca.end(); ++ca)
			fprintf(f, ",C:%04x", *ca);

		fprintf(f, "\n");
		services++;
	}
	fprintf(f, "end\nHave a lot of bugs!\n");
	eDebug("saved %d channels and %d services!", channels, services);
	fclose(f);
}

void eDVBDB::loadBouquet(const char *path)
{
	std::string bouquet_name = path;
	if (!bouquet_name.length())
	{
		eDebug("Bouquet load failed.. no path given..");
		return;
	}
	unsigned int pos = bouquet_name.rfind('/');
	if ( pos != std::string::npos )
		bouquet_name.erase(0, pos+1);
	if (bouquet_name.empty())
	{
		eDebug("Bouquet load failed.. no filename given..");
		return;
	}
	eBouquet &bouquet = m_bouquets[bouquet_name];
	bouquet.m_path = path;
	std::list<eServiceReference> &list = bouquet.m_services;
	list.clear();

	eDebug("loading bouquet... %s", path);
	FILE *fp=fopen(path, "rt");
	int entries=0;
	if (!fp)
	{
		eDebug("failed to open.");
		if ( strstr(path, "bouquets.tv") )
		{
			eDebug("recreate bouquets.tv");
			bouquet.m_bouquet_name="Bouquets (TV)";
			bouquet.flushChanges();
		}
		else if ( strstr(path, "bouquets.radio") )
		{
			eDebug("recreate bouquets.radio");
			bouquet.m_bouquet_name="Bouquets (Radio)";
			bouquet.flushChanges();
		}
		return;
	}
	char line[256];
	bool read_descr=false;
	eServiceReference *e = NULL;
	while (1)
	{
		if (!fgets(line, 256, fp))
			break;
		line[strlen(line)-1]=0;
		if (strlen(line) && line[strlen(line)-1]=='\r')
			line[strlen(line)-1]=0;
		if (!line[0])
			break;
		if (line[0]=='#')
		{
			if (!strncmp(line, "#SERVICE ", 9) || !strncmp(line, "#SERVICE: ", 10))
			{
				int offs = line[8] == ':' ? 10 : 9;
				eServiceReference tmp(line+offs);
				if (tmp.type != eServiceReference::idDVB)
				{
					eDebug("only DVB Bouquets supported");
					continue;
				}
				if ( (tmp.flags&eServiceReference::flagDirectory) == eServiceReference::flagDirectory )
				{
					std::string str = tmp.path;
					unsigned int pos = str.rfind('/');
					if ( pos != std::string::npos )
						str.erase(0, pos+1);
					if (str.empty())
					{
						eDebug("Bouquet load failed.. no filename given..");
						continue;
					}
					loadBouquet(tmp.path.c_str());
					char buf[256];
					snprintf(buf, 256, "(type == %d) FROM BOUQUET \"%s\" ORDER BY bouquet", tmp.data[0], str.c_str());
					tmp.path = buf;
				}
				list.push_back(tmp);
				e = &list.back();
				read_descr=true;
				++entries;
			}
			else if (read_descr && !strncmp(line, "#DESCRIPTION ", 13))
			{
				e->name = line+13;
				read_descr=false;
			}
			else if (!strncmp(line, "#NAME ", 6))
				bouquet.m_bouquet_name=line+6;
			continue;
		}
	}
	fclose(fp);
	eDebug("%d entries in Bouquet %s", entries, bouquet_name.c_str());
}

void eDVBDB::loadBouquets()
{
	loadBouquet("bouquets.tv");
	loadBouquet("bouquets.radio");
// create default bouquets when missing
	if ( m_bouquets.find("userbouquet.favourites.tv") == m_bouquets.end() )
	{
		eBouquet &b = m_bouquets["userbouquet.favourites.tv"];
		b.m_path = "userbouquet.favourites.tv";
		b.m_bouquet_name = "Favourites (TV)";
		b.flushChanges();
		eServiceReference ref;
		memset(ref.data, 0, sizeof(ref.data));
		ref.type=1;
		ref.flags=7;
		ref.data[0]=1;
		ref.path="(type == 1) FROM BOUQUET \"userbouquet.favourites.tv\" ORDER BY bouquet";
		eBouquet &parent = m_bouquets["bouquets.tv"];
		parent.m_services.push_back(ref);
		parent.flushChanges();
	}
	if ( m_bouquets.find("userbouquet.favourites.radio") == m_bouquets.end() )
	{
		eBouquet &b = m_bouquets["userbouquet.favourites.radio"];
		b.m_path = "userbouquet.favourites.radio";
		b.m_bouquet_name = "Favourites (Radio)";
		b.flushChanges();
		eServiceReference ref;
		memset(ref.data, 0, sizeof(ref.data));
		ref.type=1;
		ref.flags=7;
		ref.data[0]=1;
		ref.path="(type == 2) FROM BOUQUET \"userbouquet.favourites.radio\" ORDER BY bouquet";
		eBouquet &parent = m_bouquets["bouquets.radio"];
		parent.m_services.push_back(ref);
		parent.flushChanges();
	}
}

eDVBDB::eDVBDB()
{
	load();
	loadBouquets();
}

eDVBDB::~eDVBDB()
{
//	save();
}

RESULT eDVBDB::addChannelToList(const eDVBChannelID &id, iDVBFrontendParameters *feparm)
{
	channel ch;
	assert(feparm);
	ch.m_frontendParameters = feparm;
	m_channels.insert(std::pair<eDVBChannelID, channel>(id, ch));
	return 0;
}

RESULT eDVBDB::removeChannel(const eDVBChannelID &id)
{
	m_channels.erase(id);
	return 0;
}

RESULT eDVBDB::getChannelFrontendData(const eDVBChannelID &id, ePtr<iDVBFrontendParameters> &parm)
{
	std::map<eDVBChannelID, channel>::iterator i = m_channels.find(id);
	if (i == m_channels.end())
	{
		parm = 0;
		return -ENOENT;
	}
	parm = i->second.m_frontendParameters;
	return 0;
}

RESULT eDVBDB::addService(const eServiceReferenceDVB &serviceref, eDVBService *service)
{
	m_services.insert(std::pair<eServiceReferenceDVB, ePtr<eDVBService> >(serviceref, service));
	return 0;
}

RESULT eDVBDB::getService(const eServiceReferenceDVB &reference, ePtr<eDVBService> &service)
{
	std::map<eServiceReferenceDVB, ePtr<eDVBService> >::iterator i;
	i = m_services.find(reference);
	if (i == m_services.end())
	{
		service = 0;
		return -ENOENT;
	}
	service = i->second;
	return 0;
}

RESULT eDVBDB::flush()
{
	save();
	return 0;
}

RESULT eDVBDB::getBouquet(const eServiceReference &ref, eBouquet* &bouquet)
{
	std::string str = ref.path;
	if (str.empty())
	{
		eDebug("getBouquet failed.. no path given!");
		return -1;
	}
	unsigned int pos = str.find("FROM BOUQUET \"");
	if ( pos != std::string::npos )
	{
		str.erase(0, pos+14);
		pos = str.find('"');
		if ( pos != std::string::npos )
			str.erase(pos);
		else
			str.clear();
	}
	if (str.empty())
	{
		eDebug("getBouquet failed.. couldn't parse bouquet name");
		return -1;
	}
	std::map<std::string, eBouquet>::iterator i =
		m_bouquets.find(str);
	if (i == m_bouquets.end())
	{
		bouquet = 0;
		return -ENOENT;
	}
	bouquet = &i->second;
	return 0;
}

RESULT eDVBDB::startQuery(ePtr<iDVBChannelListQuery> &query, eDVBChannelQuery *q, const eServiceReference &source)
{
	if ( q && q->m_bouquet_name.length() )
		query = new eDVBDBBouquetQuery(this, source, q);
	else
		query = new eDVBDBQuery(this, source, q);
	return 0;
}

DEFINE_REF(eDVBDBQueryBase);

eDVBDBQueryBase::eDVBDBQueryBase(eDVBDB *db, const eServiceReference &source, eDVBChannelQuery *query)
	:m_db(db), m_query(query), m_source(source)
{
}

int eDVBDBQueryBase::compareLessEqual(const eServiceReferenceDVB &a, const eServiceReferenceDVB &b)
{
	ePtr<eDVBService> a_service, b_service;
	
	int sortmode = m_query ? m_query->m_sort : eDVBChannelQuery::tName;
	
	if ((sortmode == eDVBChannelQuery::tName) || (sortmode == eDVBChannelQuery::tProvider))
	{
		if (m_db->getService(a, a_service))
			return 1;
		if (m_db->getService(b, b_service))
			return 1;
	}
	
	switch (sortmode)
	{
	case eDVBChannelQuery::tName:
		return a_service->m_service_name_sort < b_service->m_service_name_sort;
	case eDVBChannelQuery::tProvider:
		return a_service->m_provider_name < b_service->m_provider_name;
	case eDVBChannelQuery::tType:
		return a.getServiceType() < b.getServiceType();
	case eDVBChannelQuery::tBouquet:
		return 0;
	case eDVBChannelQuery::tSatellitePosition:
		return (a.getDVBNamespace().get() >> 16) < (b.getDVBNamespace().get() >> 16);
	default:
		return 1;
	}
	return 0;
}

eDVBDBQuery::eDVBDBQuery(eDVBDB *db, const eServiceReference &source, eDVBChannelQuery *query)
	:eDVBDBQueryBase(db, source, query)
{
	m_cursor = m_db->m_services.begin();
}

RESULT eDVBDBQuery::getNextResult(eServiceReferenceDVB &ref)
{
	while (m_cursor != m_db->m_services.end())
	{
		ref = m_cursor->first;

		int res = (!m_query) || m_cursor->second->checkFilter(ref, *m_query);

		++m_cursor;

		if (res)
			return 0;
	}

	ref.type = eServiceReference::idInvalid;

	return 1;
}

eDVBDBBouquetQuery::eDVBDBBouquetQuery(eDVBDB *db, const eServiceReference &source, eDVBChannelQuery *query)
	:eDVBDBQueryBase(db, source, query), m_cursor(db->m_bouquets[query->m_bouquet_name].m_services.begin())
{
}

RESULT eDVBDBBouquetQuery::getNextResult(eServiceReferenceDVB &ref)
{
	eBouquet &bouquet = m_db->m_bouquets[m_query->m_bouquet_name];
	std::list<eServiceReference> &list = bouquet.m_services;
	while (m_cursor != list.end())
	{
		ref = *((eServiceReferenceDVB*)&(*m_cursor));

		std::map<eServiceReferenceDVB, ePtr<eDVBService> >::iterator it =
			m_db->m_services.find(ref);

		int res = (!m_query) || it == m_db->m_services.end() || it->second->checkFilter(ref, *m_query);

		++m_cursor;

		if (res)
			return 0;
	}

	ref.type = eServiceReference::idInvalid;

	return 1;
}

/* (<name|provider|type|bouquet|satpos|chid> <==|...> <"string"|int>)[||,&& (..)] */

static int decodeType(const std::string &type)
{
	if (type == "name")
		return eDVBChannelQuery::tName;
	else if (type == "provider")
		return eDVBChannelQuery::tProvider;
	else if (type == "type")
		return eDVBChannelQuery::tType;
	else if (type == "bouquet")
		return eDVBChannelQuery::tBouquet;
	else if (type == "satellitePosition")
		return eDVBChannelQuery::tSatellitePosition;
	else if (type == "channelID")
		return eDVBChannelQuery::tChannelID;
	else
		return -1;
}

	/* never, NEVER write a parser in C++! */
RESULT parseExpression(ePtr<eDVBChannelQuery> &res, std::list<std::string>::const_iterator begin, std::list<std::string>::const_iterator end)
{
	std::list<std::string>::const_iterator end_of_exp;
	if (*begin == "(")
	{
		end_of_exp = begin;
		while (end_of_exp != end)
			if (*end_of_exp == ")")
				break;
			else
				++end_of_exp;
	
		if (end_of_exp == end)
		{
			eDebug("expression parse: end of expression while searching for closing brace");
			return -1;
		}
		
		++begin;
		// begin..end_of_exp
		int r = parseExpression(res, begin, end_of_exp);
		if (r)
			return r;
		++end_of_exp;
		
			/* we had only one sub expression */
		if (end_of_exp == end)
		{
//			eDebug("only one sub expression");
			return 0;
		}
		
			/* otherwise we have an operator here.. */
		
		ePtr<eDVBChannelQuery> r2 = res;
		res = new eDVBChannelQuery();
		res->m_sort = 0;
		res->m_p1 = r2;
		res->m_inverse = 0;
		r2 = 0;
		
		if (*end_of_exp == "||")
			res->m_type = eDVBChannelQuery::tOR;
		else if (*end_of_exp == "&&")
			res->m_type = eDVBChannelQuery::tAND;
		else
		{
			eDebug("found operator %s, but only && and || are allowed!", end_of_exp->c_str());
			res = 0;
			return 1;
		}
		
		++end_of_exp;
		
		return parseExpression(res->m_p2, end_of_exp, end);
	}
	
	// "begin" <op> "end"
	std::string type, op, val;
	
	res = new eDVBChannelQuery();
	res->m_sort = 0;
	
	int cnt = 0;
	while (begin != end)
	{
		switch (cnt)
		{
		case 0:
			type = *begin;
			break;
		case 1:
			op = *begin;
			break;
		case 2:
			val = *begin;
			break;
		case 3:
			eDebug("malformed query: got '%s', but expected only <type> <op> <val>", begin->c_str());
			return 1;
		}
		++begin;
		++cnt;
	}
	
	if (cnt != 3)
	{
		eDebug("malformed query: missing stuff");
		res = 0;
		return 1;
	}
	
	res->m_type = decodeType(type);
	
	if (res->m_type == -1)
	{
		eDebug("malformed query: invalid type %s", type.c_str());
		res = 0;
		return 1;
	}
	
	if (op == "==")
		res->m_inverse = 0;
	else if (op == "!=")
		res->m_inverse = 1;
	else
	{
		eDebug("invalid operator %s", op.c_str());
		res = 0;
		return 1;
	}
	
	res->m_string = val;
	res->m_int = atoi(val.c_str());
//	res->m_channelid = eDVBChannelID(val);
	
	return 0;
}

RESULT eDVBChannelQuery::compile(ePtr<eDVBChannelQuery> &res, std::string query)
{
	std::list<std::string> tokens;
	
	std::string current_token;
	std::string bouquet_name;

//	eDebug("splitting %s....", query.c_str());
	unsigned int i = 0;
	const char *splitchars="()";
	int quotemode = 0, lastsplit = 0, lastalnum = 0;
	while (i <= query.size())
	{
		int c = (i < query.size()) ? query[i] : ' ';
		++i;
		
		int issplit = !!strchr(splitchars, c);
		int isaln = isalnum(c);
		int iswhite = c == ' ';
		int isquot = c == '\"';
		
		if (quotemode)
		{
			iswhite = issplit = 0;
			isaln = lastalnum;
		}
		
		if (issplit || iswhite || isquot || lastsplit || (lastalnum != isaln))
		{
			if (current_token.size())
				tokens.push_back(current_token);
			current_token.clear();
		}
		
		if (!(iswhite || isquot))
			current_token += c;
		
		if (isquot)
			quotemode = !quotemode;
		lastsplit = issplit;
		lastalnum = isaln;
	}
	
//	for (std::list<std::string>::const_iterator a(tokens.begin()); a != tokens.end(); ++a)
//	{
//		printf("%s\n", a->c_str());
//	}

	int sort = eDVBChannelQuery::tName;
		/* check for "ORDER BY ..." */

	while (tokens.size() > 2)
	{
		std::list<std::string>::iterator it = tokens.end();
		--it; --it; --it;
		if (*it == "ORDER")
		{
			++it;
			if (*it == "BY")
			{
				++it;
				sort = decodeType(*it);
				tokens.pop_back(); // ...
				tokens.pop_back(); // BY
				tokens.pop_back(); // ORDER
			} else
				sort = -1;
		}
		else if (*it == "FROM")
		{
			++it;
			if (*it == "BOUQUET")
			{
				++it;
				bouquet_name = *it;
				tokens.pop_back(); // ...
				tokens.pop_back(); // FROM
				tokens.pop_back(); // BOUQUET
			}
		}
		else
			break;
	}

	if (sort == -1)
	{
		eWarning("ORDER BY .. string invalid.");
		res = 0;
		return -1;
	}
	
//	eDebug("sort by %d", sort);
	
		/* now we recursivly parse that. */
	int r = parseExpression(res, tokens.begin(), tokens.end());
	
	if (res)
	{
		res->m_sort = sort;
		res->m_bouquet_name = bouquet_name;
	}

//	eDebug("return: %d", r);
	return r;
}

DEFINE_REF(eDVBChannelQuery);
