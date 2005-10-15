#ifndef __lib_gui_ewidgetdesktop_h
#define __lib_gui_ewidgetdesktop_h

#include <lib/gdi/grc.h>
#include <lib/base/eptrlist.h>

class eWidget;
class eMainloop;
class eTimer;

		/* an eWidgetDesktopCompBuffer is a composition buffer. in 
		   immediate composition  mode, we only have one composition 
		   buffer - the screen.
		   in buffered mode, we have one buffer for each widget, plus
		   the screen.
		   
		   even in buffered mode, we have a background region, because
		   a window can be arbitrary shaped. the screen size acts as a bounding
		   box in these cases. */

struct eWidgetDesktopCompBuffer
{
	ePoint m_position;
	eSize m_screen_size;
	gRegion m_dirty_region;
	gRegion m_background_region;
	ePtr<gDC> m_dc;
	gRGB m_background_color;
};

class eWidgetDesktop: public Object
{
public:
	eWidgetDesktop(eSize screen);
	~eWidgetDesktop();
	void addRootWidget(eWidget *root, int top);
	void removeRootWidget(eWidget *root);
	
		/* try to move widget content. */
		/* returns -1 if there's no move support. */
		/* call this after recalcClipRegions for that widget. */
		/* you probably want to invalidate if -1 was returned. */
	int movedWidget(eWidget *root);
	
	void recalcClipRegions(eWidget *root);
	
	void invalidate(const gRegion &region);
	void paint();
	void setDC(gDC *dc);
	
	void setBackgroundColor(gRGB col);
	void setBackgroundColor(eWidgetDesktopCompBuffer *comp, gRGB col);
	
	void setPalette(gPixmap &pm);
	
	void setRedrawTask(eMainloop &ml);
	
	void makeCompatiblePixmap(gPixmap &pm);
	
	enum {
		cmImmediate,
		cmBuffered
	};
	
	void setCompositionMode(int mode);
private:
	ePtrList<eWidget> m_root;
	void calcWidgetClipRegion(eWidget *widget, gRegion &parent_visible);
	void paintBackground(eWidgetDesktopCompBuffer *comp);
	
	eMainloop *m_mainloop;
	eTimer *m_timer;
	
	int m_comp_mode;
	int m_require_redraw;
	
	eWidgetDesktopCompBuffer m_screen;
	
	void createBufferForWidget(eWidget *widget);
	void removeBufferForWidget(eWidget *widget);
	
	void redrawComposition(int notifed);
	void notify();
	
	void clearVisibility(eWidget *widget);
};

#endif
