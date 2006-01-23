#include <lib/gui/elistbox.h>
#include <lib/gui/elistboxcontent.h>
#include <lib/gdi/font.h>
#include <Python.h>

/*
    The basic idea is to have an interface which gives all relevant list
    processing functions, and can be used by the listbox to browse trough
    the list.
    
    The listbox directly uses the implemented cursor. It tries hard to avoid
    iterating trough the (possibly very large) list, so it should be O(1),
    i.e. the performance should not be influenced by the size of the list.
    
    The list interface knows how to draw the current entry to a specified 
    offset. Different interfaces can be used to adapt different lists,
    pre-filter lists on the fly etc.
    
		cursorSave/Restore is used to avoid re-iterating the list on redraw.
		The current selection is always selected as cursor position, the
    cursor is then positioned to the start, and then iterated. This gives
    at most 2x m_items_per_page cursor movements per redraw, indepenent
    of the size of the list.
    
    Although cursorSet is provided, it should be only used when there is no
    other way, as it involves iterating trough the list.
 */

iListboxContent::~iListboxContent()
{
}

iListboxContent::iListboxContent(): m_listbox(0)
{
}

void iListboxContent::setListbox(eListbox *lb)
{
	m_listbox = lb;
}

DEFINE_REF(eListboxTestContent);

void eListboxTestContent::cursorHome()
{
	m_cursor = 0;
}

void eListboxTestContent::cursorEnd()
{
	m_cursor = size();
}

int eListboxTestContent::cursorMove(int count)
{
	m_cursor += count;
	
	if (m_cursor < 0)
		cursorHome();
	else if (m_cursor > size())
		cursorEnd();
	return 0;
}

int eListboxTestContent::cursorValid()
{
	return m_cursor < size();
}

int eListboxTestContent::cursorSet(int n)
{
	m_cursor = n;
	
	if (m_cursor < 0)
		cursorHome();
	else if (m_cursor > size())
		cursorEnd();
	return 0;
}

int eListboxTestContent::cursorGet()
{
	return m_cursor;
}

void eListboxTestContent::cursorSave()
{
	m_saved_cursor = m_cursor;
}

void eListboxTestContent::cursorRestore()
{
	m_cursor = m_saved_cursor;
}

int eListboxTestContent::size()
{
	return 10;
}
	
RESULT eListboxTestContent::connectItemChanged(const Slot0<void> &itemChanged, ePtr<eConnection> &connection)
{
	return 0;
}

void eListboxTestContent::setSize(const eSize &size)
{
	m_size = size;
}

void eListboxTestContent::paint(gPainter &painter, eWindowStyle &style, const ePoint &offset, int selected)
{
	ePtr<gFont> fnt = new gFont("Regular", 20);
	painter.clip(eRect(offset, m_size));
	style.setStyle(painter, selected ? eWindowStyle::styleListboxSelected : eWindowStyle::styleListboxNormal);
	painter.clear();

	if (cursorValid())
	{
		painter.setFont(fnt);
		char string[10];
		sprintf(string, "%d.)", m_cursor);
		
		ePoint text_offset = offset + (selected ? ePoint(2, 2) : ePoint(1, 1));
		
		painter.renderText(eRect(text_offset, m_size), string);
		
		if (selected)
			style.drawFrame(painter, eRect(offset, m_size), eWindowStyle::frameListboxEntry);
	}
	
	painter.clippop();
}

//////////////////////////////////////

DEFINE_REF(eListboxStringContent);

eListboxStringContent::eListboxStringContent()
{
	m_size = 0;
	cursorHome();
}

void eListboxStringContent::cursorHome()
{
	m_cursor = m_list.begin();
	m_cursor_number = 0;
}

void eListboxStringContent::cursorEnd()
{
	m_cursor = m_list.end();
	m_cursor_number = m_size;
}

int eListboxStringContent::cursorMove(int count)
{
	if (count > 0)
	{
		while (count && (m_cursor != m_list.end()))
		{
			++m_cursor;
			++m_cursor_number;
			--count;
		}
	} else if (count < 0)
	{
		while (count && (m_cursor != m_list.begin()))
		{
			--m_cursor;
			--m_cursor_number;
			++count;
		}
	}
	
	return 0;
}

int eListboxStringContent::cursorValid()
{
	return m_cursor != m_list.end();
}

int eListboxStringContent::cursorSet(int n)
{
	cursorHome();
	cursorMove(n);
	
	return 0;
}

int eListboxStringContent::cursorGet()
{
	return m_cursor_number;
}

void eListboxStringContent::cursorSave()
{
	m_saved_cursor = m_cursor;
	m_saved_cursor_number = m_cursor_number;
}

void eListboxStringContent::cursorRestore()
{
	m_cursor = m_saved_cursor;
	m_cursor_number = m_saved_cursor_number;
}

int eListboxStringContent::size()
{
	return m_size;
}
	
void eListboxStringContent::setSize(const eSize &size)
{
	m_itemsize = size;
}

void eListboxStringContent::paint(gPainter &painter, eWindowStyle &style, const ePoint &offset, int selected)
{
	ePtr<gFont> fnt = new gFont("Regular", 20);
	painter.clip(eRect(offset, m_itemsize));
	style.setStyle(painter, selected ? eWindowStyle::styleListboxSelected : eWindowStyle::styleListboxNormal);
	painter.clear();
	
	eDebug("item %d", m_cursor_number);
	if (cursorValid())
	{
		eDebug("is valid..");
		painter.setFont(fnt);
		
		ePoint text_offset = offset + (selected ? ePoint(2, 2) : ePoint(1, 1));
		
		painter.renderText(eRect(text_offset, m_itemsize), *m_cursor);
		
		if (selected)
			style.drawFrame(painter, eRect(offset, m_itemsize), eWindowStyle::frameListboxEntry);
	}
	
	painter.clippop();
}

void eListboxStringContent::setList(std::list<std::string> &list)
{
	m_list = list;
	m_size = list.size();
	cursorHome();
	m_listbox->entryReset(false);
}

//////////////////////////////////////

DEFINE_REF(eListboxPythonStringContent);

eListboxPythonStringContent::eListboxPythonStringContent()
{
	m_list = 0;
}

eListboxPythonStringContent::~eListboxPythonStringContent()
{
	Py_XDECREF(m_list);
}

void eListboxPythonStringContent::cursorHome()
{
	m_cursor = 0;
}

void eListboxPythonStringContent::cursorEnd()
{
	m_cursor = size();
}

int eListboxPythonStringContent::cursorMove(int count)
{
	m_cursor += count;
	
	if (m_cursor < 0)
		cursorHome();
	else if (m_cursor > size())
		cursorEnd();
	return 0;
}

int eListboxPythonStringContent::cursorValid()
{
	return m_cursor < size();
}

int eListboxPythonStringContent::cursorSet(int n)
{
	m_cursor = n;
	
	if (m_cursor < 0)
		cursorHome();
	else if (m_cursor > size())
		cursorEnd();
	return 0;
}

int eListboxPythonStringContent::cursorGet()
{
	return m_cursor;
}

void eListboxPythonStringContent::cursorSave()
{
	m_saved_cursor = m_cursor;
}

void eListboxPythonStringContent::cursorRestore()
{
	m_cursor = m_saved_cursor;
}

int eListboxPythonStringContent::size()
{
	if (!m_list)
		return 0;
	return PyList_Size(m_list);
}
	
void eListboxPythonStringContent::setSize(const eSize &size)
{
	m_itemsize = size;
}

void eListboxPythonStringContent::paint(gPainter &painter, eWindowStyle &style, const ePoint &offset, int selected)
{
	ePtr<gFont> fnt = new gFont("Regular", 20);
	painter.clip(eRect(offset, m_itemsize));
	style.setStyle(painter, selected ? eWindowStyle::styleListboxSelected : eWindowStyle::styleListboxNormal);
	painter.clear();

	if (m_list && cursorValid())
	{
		PyObject *item = PyList_GET_ITEM(m_list, m_cursor); // borrowed reference!
		painter.setFont(fnt);

			/* the user can supply tuples, in this case the first one will be displayed. */		
		if (PyTuple_Check(item))
			item = PyTuple_GET_ITEM(item, 0);
		
		const char *string = PyString_Check(item) ? PyString_AsString(item) : "<not-a-string>";
		
		ePoint text_offset = offset + (selected ? ePoint(2, 2) : ePoint(1, 1));
		
		painter.renderText(eRect(text_offset, m_itemsize), string);
		
		if (selected)
			style.drawFrame(painter, eRect(offset, m_itemsize), eWindowStyle::frameListboxEntry);
	}
	
	painter.clippop();
}

void eListboxPythonStringContent::setList(PyObject *list)
{
	Py_XDECREF(m_list);
	if (!PyList_Check(list))
	{
		m_list = 0;
	} else
	{
		m_list = list;
		Py_INCREF(m_list);
	}

	if (m_listbox)
		m_listbox->entryReset(false);
}

PyObject *eListboxPythonStringContent::getCurrentSelection()
{
	if (!m_list)
		return 0;
	if (!cursorValid())
		return 0;
	PyObject *r = PyList_GET_ITEM(m_list, m_cursor);
	Py_XINCREF(r);
	return r;
}

void eListboxPythonStringContent::invalidateEntry(int index)
{
	if (m_listbox)
		m_listbox->entryChanged(index);
}

void eListboxPythonStringContent::invalidate()
{
	if (m_listbox)
		m_listbox->invalidate();
}

//////////////////////////////////////

void eListboxPythonConfigContent::paint(gPainter &painter, eWindowStyle &style, const ePoint &offset, int selected)
{
	ePtr<gFont> fnt = new gFont("Regular", 20);
	ePtr<gFont> fnt2 = new gFont("Regular", 16);
	eRect itemrect(offset, m_itemsize);
	painter.clip(itemrect);
	style.setStyle(painter, selected ? eWindowStyle::styleListboxSelected : eWindowStyle::styleListboxNormal);
	painter.clear();

	if (m_list && cursorValid())
	{
			/* get current list item */
		PyObject *item = PyList_GET_ITEM(m_list, m_cursor); // borrowed reference!
		PyObject *text = 0, *value = 0;
		painter.setFont(fnt);

			/* the first tuple element is a string for the left side.
			   the second one will be called, and the result shall be an tuple.
			   
			   of this tuple,
			   the first one is the type (string).
			   the second one is the value. */
		if (PyTuple_Check(item))
		{
				/* handle left part. get item from tuple, convert to string, display. */
				
			text = PyTuple_GET_ITEM(item, 0);
			text = PyObject_Str(text); /* creates a new object - old object was borrowed! */
			const char *string = (text && PyString_Check(text)) ? PyString_AsString(text) : "<not-a-string>";
			eSize item_left = eSize(m_seperation, m_itemsize.height());
			eSize item_right = eSize(m_itemsize.width() - m_seperation, m_itemsize.height());
			painter.renderText(eRect(offset, item_left), string, gPainter::RT_HALIGN_LEFT);
			Py_XDECREF(text);
			
				/* when we have no label, align value to the left. (FIXME: 
				   don't we want to specifiy this individually?) */
			int value_alignment_left = !*string;
			
				/* now, handle the value. get 2nd part from tuple*/
			value = PyTuple_GET_ITEM(item, 1);
			if (value)
			{
				PyObject *args = PyTuple_New(1);
				PyTuple_SET_ITEM(args, 0, PyInt_FromLong(selected));
				
					/* CallObject will call __call__ which should return the value tuple */
				value = PyObject_CallObject(value, args);
				
				if (PyErr_Occurred())
					PyErr_Print();

				Py_DECREF(args);
					/* the PyInt was stolen. */
			}
			
				/*  check if this is really a tuple */
			if (value && PyTuple_Check(value))
			{
					/* convert type to string */
				PyObject *type = PyTuple_GET_ITEM(value, 0);
				const char *atype = (type && PyString_Check(type)) ? PyString_AsString(type) : 0;
				
				if (atype)
				{
					if (!strcmp(atype, "text"))
					{
						PyObject *pvalue = PyTuple_GET_ITEM(value, 1);
						const char *value = (pvalue && PyString_Check(pvalue)) ? PyString_AsString(pvalue) : "<not-a-string>";
						painter.setFont(fnt2);
						if (value_alignment_left)
							painter.renderText(eRect(offset, item_right), value, gPainter::RT_HALIGN_LEFT);
						else
							painter.renderText(eRect(offset + eSize(m_seperation, 0), item_right), value, gPainter::RT_HALIGN_RIGHT);

							/* pvalue is borrowed */
					} else if (!strcmp(atype, "slider"))
					{
						PyObject *pvalue = PyTuple_GET_ITEM(value, 1);
						
							/* convert value to Long. fallback to -1 on error. */
						int value = (pvalue && PyInt_Check(pvalue)) ? PyInt_AsLong(pvalue) : -1;
						
							/* calc. slider length */
						int width = item_right.width() * value / 100;
						int height = item_right.height();
						
												
							/* draw slider */
						//painter.fill(eRect(offset.x() + m_seperation, offset.y(), width, height));
						//hack - make it customizable
						painter.fill(eRect(offset.x() + m_seperation, offset.y() + 5, width, height-10));
						
							/* pvalue is borrowed */
					} else if (!strcmp(atype, "mtext"))
					{
						PyObject *pvalue = PyTuple_GET_ITEM(value, 1);
						const char *text = (pvalue && PyString_Check(pvalue)) ? PyString_AsString(pvalue) : "<not-a-string>";
						int xoffs = value_alignment_left ? 0 : m_seperation;
						ePtr<eTextPara> para = new eTextPara(eRect(offset + eSize(xoffs, 0), item_right));
						para->setFont(fnt2);
						para->renderString(text, 0);
						para->realign(value_alignment_left ? eTextPara::dirLeft : eTextPara::dirRight);
						int glyphs = para->size();
						
						PyObject *plist = 0;
						
						if (PyTuple_Size(value) >= 3)
							plist = PyTuple_GET_ITEM(value, 2);
						
						int entries = 0;

						if (plist && PyList_Check(plist))
							entries = PyList_Size(plist);
						
						for (int i = 0; i < entries; ++i)
						{
							PyObject *entry = PyList_GET_ITEM(plist, i);
							int num = PyInt_Check(entry) ? PyInt_AsLong(entry) : -1;
							
							if ((num < 0) || (num >= glyphs))
								eWarning("glyph index %d in PythonConfigList out of bounds!");
							else
							{
								para->setGlyphFlag(num, GS_INVERT);
								eRect bbox;
								bbox = para->getGlyphBBox(num);
								bbox = eRect(bbox.left(), offset.y(), bbox.width(), m_itemsize.height());
								painter.fill(bbox);
							}
								/* entry is borrowed */
						}
						
						painter.renderPara(para, ePoint(0, 0));
							/* pvalue is borrowed */
							/* plist is 0 or borrowed */
					}
				}
					/* type is borrowed */
			} else
				eWarning("eListboxPythonConfigContent: second value of tuple is not a tuple.");
				/* value is borrowed */
		}

		if (selected)
			style.drawFrame(painter, eRect(offset, m_itemsize), eWindowStyle::frameListboxEntry);
	}
	
	painter.clippop();
}

//////////////////////////////////////

	/* todo: make a real infrastructure here! */
RESULT SwigFromPython(ePtr<gPixmap> &res, PyObject *obj);

void eListboxPythonMultiContent::paint(gPainter &painter, eWindowStyle &style, const ePoint &offset, int selected)
{
	eRect itemrect(offset, m_itemsize);
	painter.clip(itemrect);
	style.setStyle(painter, selected ? eWindowStyle::styleListboxSelected : eWindowStyle::styleListboxNormal);
	painter.clear();

	if (m_list && cursorValid())
	{
		PyObject *items = PyList_GET_ITEM(m_list, m_cursor); // borrowed reference!
		
		if (!items)
		{
			eDebug("eListboxPythonMultiContent: error getting item %d", m_cursor);
			goto error_out;
		}
		
		if (!PyList_Check(items))
		{
			eDebug("eListboxPythonMultiContent: list entry %d is not a list", m_cursor);
			goto error_out;
		}
		
		int size = PyList_Size(items);
		for (int i = 1; i < size; ++i)
		{
			PyObject *item = PyList_GET_ITEM(items, i); // borrowed reference!
			
			if (!item)
			{
				eDebug("eListboxPythonMultiContent: ?");
				goto error_out;
			}
			
			PyObject *px = 0, *py = 0, *pwidth = 0, *pheight = 0, *pfnt = 0, *pstring = 0, *pflags = 0;
		
			/*
				we have a list of tuples:
				
				(0, x, y, width, height, fnt, flags, "bla" ),

				or, for a progress:
				(1, x, y, width, height, filled_percent )

				or, for a pixmap:
				
				(2, x, y, width, height, pixmap )
				
			 */
			
			if (!PyTuple_Check(item))
			{
				eDebug("eListboxPythonMultiContent did not receive a tuple.");
				goto error_out;
			}

			int size = PyTuple_Size(item);

			if (!size)
			{
				eDebug("eListboxPythonMultiContent receive empty tuple.");
				goto error_out;
			}

			int type = PyInt_AsLong(PyTuple_GET_ITEM(item, 0));

			if (size > 5)
			{
				px = PyTuple_GET_ITEM(item, 1);
				py = PyTuple_GET_ITEM(item, 2);
				pwidth = PyTuple_GET_ITEM(item, 3);
				pheight = PyTuple_GET_ITEM(item, 4);
				pfnt = PyTuple_GET_ITEM(item, 5); /* could also be an pixmap or an int (progress filled percent) */
				if (size > 7)
				{
					pflags = PyTuple_GET_ITEM(item, 6);
					pstring = PyTuple_GET_ITEM(item, 7);
				}
			}
			
			switch (type)
			{
			case TYPE_TEXT: // text
			{
				if (!(px && py && pwidth && pheight && pfnt && pstring))
				{
					eDebug("eListboxPythonMultiContent received too small tuple (must be (TYPE_TEXT, x, y, width, height, fnt, flags, string[, ...])");
					goto error_out;
				}
				
				const char *string = (PyString_Check(pstring)) ? PyString_AsString(pstring) : "<not-a-string>";
				int x = PyInt_AsLong(px);
				int y = PyInt_AsLong(py);
				int width = PyInt_AsLong(pwidth);
				int height = PyInt_AsLong(pheight);
				int flags = PyInt_AsLong(pflags);
				int fnt = PyInt_AsLong(pfnt);
				
				if (m_font.find(fnt) == m_font.end())
				{
					eDebug("eListboxPythonMultiContent: specified font %d was not found!", fnt);
					goto error_out;
				}
				
				eRect r = eRect(x, y, width, height);
				r.moveBy(offset);
				r &= itemrect;
				
				painter.setFont(m_font[fnt]);
				
				painter.clip(r);
				painter.renderText(r, string, flags);
				painter.clippop();
				break;
			}
			case TYPE_PROGRESS: // Progress
			{
				if (!(px && py && pwidth && pheight && pfnt))
				{
					eDebug("eListboxPythonMultiContent received too small tuple (must be (TYPE_PROGRESS, x, y, width, height, filled percent))");
					goto error_out;
				}
				int x = PyInt_AsLong(px);
				int y = PyInt_AsLong(py);
				int width = PyInt_AsLong(pwidth);
				int height = PyInt_AsLong(pheight);
				int filled = PyInt_AsLong(pfnt);

				eRect r = eRect(x, y, width, height);
				r.moveBy(offset);
				r &= itemrect;

				painter.clip(r);
				int bwidth=2;  // borderwidth hardcoded yet

				// border
				eRect rc = eRect(x, y, width, bwidth);
				rc.moveBy(offset);
				painter.fill(rc);

				rc = eRect(x, y+bwidth, bwidth, height-bwidth);
				rc.moveBy(offset);
				painter.fill(rc);

				rc = eRect(x+bwidth, y+height-bwidth, width-bwidth, bwidth);
				rc.moveBy(offset);
				painter.fill(rc);

				rc = eRect(x+width-bwidth, y+bwidth, bwidth, height-bwidth);
				rc.moveBy(offset);
				painter.fill(rc);

				// progress
				rc = eRect(x+bwidth, y+bwidth, (width-bwidth*2) * filled / 100, height-bwidth*2);
				rc.moveBy(offset);
				painter.fill(rc);

				painter.clippop();

				break;
			}
			case TYPE_PIXMAP: // pixmap
			{
				if (!(px && py && pwidth && pheight && pfnt))
				{
					eDebug("eListboxPythonMultiContent received too small tuple (must be (TYPE_PIXMAP, x, y, width, height, pixmap))");
					goto error_out;
				}
				int x = PyInt_AsLong(px);
				int y = PyInt_AsLong(py);
				int width = PyInt_AsLong(pwidth);
				int height = PyInt_AsLong(pheight);
				ePtr<gPixmap> pixmap;
				if (SwigFromPython(pixmap, pfnt))
				{
					eDebug("eListboxPythonMultiContent (Pixmap) get pixmap failed");
					goto error_out;
				}

				eRect r = eRect(x, y, width, height);
				r.moveBy(offset);
				r &= itemrect;
				
				painter.clip(r);
				painter.blit(pixmap, r.topLeft(), r);
				painter.clippop();

				break;
			}
			default:
				eWarning("eListboxPythonMultiContent received unknown type (%d)", type);
				goto error_out;
			}
		}
	}
	
	if (selected)
		style.drawFrame(painter, eRect(offset, m_itemsize), eWindowStyle::frameListboxEntry);

error_out:
	painter.clippop();
}

void eListboxPythonMultiContent::setFont(int fnt, gFont *font)
{
	if (font)
		m_font[fnt] = font;
	else
		m_font.erase(fnt);
}
