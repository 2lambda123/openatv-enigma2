#include <lib/python/python.h>
#include <lib/base/eerror.h>
#include <Python.h>

extern "C" void init_enigma();

#if 0
ePyObject::ePyObject(void *ptr): m_object(ptr)
{
	Py_XINCREF((PyObject*)ptr);
}

ePyObject::ePyObject(ePyObject &p)
{
	m_object = p.m_object;
	Py_XINCREF((PyObject*)m_object);
}

ePyObject::ePyObject(): m_object(0)
{
}

ePyObject::~ePyObject()
{
	Py_XDECREF((PyObject*)m_object);
}

ePyObject &ePyObject::operator=(ePyObject &p)
{
	Py_XDECREF((PyObject*)m_object);
	m_object = p.m_object;
	Py_XINCREF((PyObject*)m_object);
	return *this;
}

ePyObject &ePyObject::operator=(void *object)
{
	Py_XDECREF((PyObject*)m_object);
	m_object = object;
	Py_XINCREF((PyObject*)m_object);
	return *this;
}
#endif

ePython::ePython()
{
	Py_Initialize();
	
	init_enigma();
}

ePython::~ePython()
{
	Py_Finalize();
}

int ePython::execute(const std::string &pythonfile, const std::string &funcname)
{
	PyObject *pName, *pModule, *pDict, *pFunc, *pArgs, *pValue;
	
	pName = PyString_FromString(pythonfile.c_str());
	
	pModule = PyImport_Import(pName);
	Py_DECREF(pName);
	
	if (pModule != NULL)
	{
		pDict = PyModule_GetDict(pModule);
		
		pFunc = PyDict_GetItemString(pDict, funcname.c_str());
		
		if (pFunc && PyCallable_Check(pFunc))
		{
			pArgs = PyTuple_New(0);
				// implement arguments..
			pValue = PyObject_CallObject(pFunc, pArgs);
			Py_DECREF(pArgs);
			if (pValue != NULL)
			{
				printf("Result of call: %ld\n", PyInt_AsLong(pValue));
				Py_DECREF(pValue);
			} else
			{
				Py_DECREF(pModule);
				PyErr_Print();
				return 1;
			}
		}
	} else
	{
		if (PyErr_Occurred())
			PyErr_Print();
		return 1;
	}
	return 0;
}

void ePython::call(PyObject *pFunc, PyObject *pArgs)
{
	PyObject *pValue;
	if (pFunc && PyCallable_Check(pFunc))
	{
		pValue = PyObject_CallObject(pFunc, pArgs);
 		if (pValue != NULL)
		{
			printf("Result of call: %ld\n", PyInt_AsLong(pValue));
			Py_DECREF(pValue);
		} else
		{
		 	PyErr_Print();
		}
	}
}

PyObject *ePython::resolve(const std::string &pythonfile, const std::string &funcname)
{
	PyObject *pName, *pModule, *pDict, *pFunc;

	pName = PyString_FromString(pythonfile.c_str());

	pModule = PyImport_Import(pName);
	Py_DECREF(pName);
	
	if (pModule != NULL)
	{
		pDict = PyModule_GetDict(pModule);
		pFunc = PyDict_GetItemString(pDict, funcname.c_str());
		Py_XINCREF(pFunc);
		Py_DECREF(pModule);
		eDebug("resolved to %p", pFunc);
		return pFunc;
	} else
	{
		if (PyErr_Occurred())
			PyErr_Print();
		return 0;
	}
}
