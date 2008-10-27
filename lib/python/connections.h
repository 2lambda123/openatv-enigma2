#ifndef __lib_python_connections_h
#define __lib_python_connections_h

#include <libsig_comp.h>

		/* avoid warnigs :) */
#include <features.h>
#undef _POSIX_C_SOURCE
#define _POSIX_C_SOURCE 200112L

#include <lib/python/python.h>

class PSignal
{
protected:
	ePyObject m_list;
	bool *m_destroyed;
public:
	PSignal();
	~PSignal();
	void callPython(SWIG_PYOBJECT(ePyObject) tuple);
	PyObject *get(bool steal=false);
};

inline PyObject *PyFrom(int v)
{
	return PyInt_FromLong(v);
}

inline PyObject *PyFrom(const char *c)
{
	return PyString_FromString(c);
}

template <class R>
class PSignal0: public PSignal, public Signal0<R>
{
public:
	R operator()()
	{
		bool destroyed=false;
		m_destroyed = &destroyed;
		if (m_list)
		{
			PyObject *pArgs = PyTuple_New(0);
			callPython(pArgs);
			Org_Py_DECREF(pArgs);
		}
		if (!destroyed) {
			m_destroyed = 0;
			return Signal0<R>::operator()();
		}
	}
};

template <class R, class V0>
class PSignal1: public PSignal, public Signal1<R,V0>
{
public:
	R operator()(V0 a0)
	{
		bool destroyed=false;
		m_destroyed = &destroyed;
		if (m_list)
		{
			PyObject *pArgs = PyTuple_New(1);
			PyTuple_SET_ITEM(pArgs, 0, PyFrom(a0));
			callPython(pArgs);
			Org_Py_DECREF(pArgs);
		}
		if (!destroyed) {
			m_destroyed = 0;
			return Signal1<R,V0>::operator()(a0);
		}
	}
};

template <class R, class V0, class V1>
class PSignal2: public PSignal, public Signal2<R,V0,V1>
{
public:
	R operator()(V0 a0, V1 a1)
	{
		bool destroyed=false;
		m_destroyed = &destroyed;
		if (m_list)
		{
			PyObject *pArgs = PyTuple_New(2);
			PyTuple_SET_ITEM(pArgs, 0, PyFrom(a0));
			PyTuple_SET_ITEM(pArgs, 1, PyFrom(a1));
			callPython(pArgs);
			Org_Py_DECREF(pArgs);
		}
		if (!destroyed) {
			m_destroyed = 0;
			return Signal2<R,V0,V1>::operator()(a0, a1);
		}
	}
};

#endif
