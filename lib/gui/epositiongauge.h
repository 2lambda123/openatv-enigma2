#ifndef __lib_gui_epositiongauge_h
#define __lib_gui_epositiongauge_h

#include <lib/gui/ewidget.h>
#include <set>

typedef long long pts_t;

class ePixmap;

class ePositionGauge: public eWidget
{
public:
	ePositionGauge(eWidget *parent);
	~ePositionGauge();
	void setLength(const pts_t &len);
	void setPosition(const pts_t &pos);
	
	void setInColor(const gRGB &color); /* foreground? */
	void setPointer(gPixmap *pixmap, const ePoint &center);
	
	void setInOutList(PyObject *list);
#ifndef SWIG
protected:
	int event(int event, void *data=0, void *data2=0);
private:
	void updatePosition();
	enum ePositionGaugeEvent
	{
		evtChangedPosition = evtUserWidget
	};
	ePixmap *m_point_widget;
	ePoint m_point_center;
	
	pts_t m_position, m_length;
	int m_pos;

		/* TODO: this is duplicated code from lib/service/servicedvb.h */
	struct cueEntry
	{
		pts_t where;
		unsigned int what;
		
		bool operator < (const struct cueEntry &o) const
		{
			return what < o.what;
		}
		cueEntry(const pts_t &where, unsigned int what) :
			where(where), what(what)
		{
		}
	};
	
	std::multiset<cueEntry> m_cue_entries;
	int scale(const pts_t &val);
#endif
};

#endif
