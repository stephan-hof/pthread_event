#include "Python.h"
#include <pthread.h>
#include <semaphore.h>
#include <errno.h>

typedef struct {
    PyObject_HEAD
    pthread_mutex_t lock;
    pthread_cond_t cond;
    unsigned int flag:1;
} Event;

#define NANO_SEC 1000000000

/* Helpers to convert the error-numbers from pthread to python exceptions */
static PyObject *
py_error_from_error(char *prefix, int errnum)
{
    return PyErr_Format(PyExc_OSError, "%s:%s", prefix, strerror(errnum));
}

static PyObject *
Event_new(PyTypeObject *type, PyObject *args, PyObject *kwargs)
{
    int error;
    Event *self = (Event*) type->tp_alloc(type, 0);

    error = pthread_mutex_init(&self->lock, NULL);
    if (error) {
        type->tp_free(self);
        return py_error_from_error("Cannot init mutex", error);
    }


    error = pthread_cond_init(&self->cond, NULL);
    if (error) {
        pthread_mutex_destroy(&self->lock);
        type->tp_free(self);
        return py_error_from_error("Cannot init condition", error);
    }

    self->flag = 0;
    return (PyObject*)self;
}

static void
Event_dealloc(Event *self)
{
    pthread_mutex_destroy(&self->lock);
    pthread_cond_destroy(&self->cond);
    Py_TYPE(self)->tp_free(self);
}


static PyObject *
Event_is_set(Event *self)
{
    if (self->flag == 0) {
        Py_RETURN_FALSE;
    }
    Py_RETURN_TRUE;
}

static PyObject *
Event_set(Event *self)
{
    Py_BEGIN_ALLOW_THREADS

    pthread_mutex_lock(&self->lock);
    self->flag = 1;
    pthread_cond_broadcast(&self->cond);
    pthread_mutex_unlock(&self->lock);

    Py_END_ALLOW_THREADS

    Py_RETURN_NONE;
}


static PyObject *
Event_clear(Event *self)
{
    Py_BEGIN_ALLOW_THREADS

    pthread_mutex_lock(&self->lock);
    self->flag = 0;
    pthread_mutex_unlock(&self->lock);

    Py_END_ALLOW_THREADS

    Py_RETURN_NONE;
}

static int
parse_timeout(PyObject *py_timeout, struct timespec *timeout)
{
    double timeout_seconds;
    struct timeval now;

    timeout_seconds = PyFloat_AsDouble(py_timeout);
    if (PyErr_Occurred()) {
        PyErr_Format(PyExc_ValueError, "'timeout' is not a valid float");
        return -1;
    }

    if (timeout_seconds < 0) {
        timeout_seconds = 0;
    }

    if (gettimeofday(&now, NULL)) {
        PyErr_SetFromErrno(PyExc_OSError);
        return -1;
    }

    TIMEVAL_TO_TIMESPEC(&now, timeout);

    timeout->tv_sec += timeout_seconds;
    timeout->tv_nsec += fmod(timeout_seconds * NANO_SEC, NANO_SEC);
    if (timeout->tv_nsec >= NANO_SEC) {
        timeout->tv_sec += 1;
        timeout->tv_nsec -= NANO_SEC;
    }

    return 0;
}

static PyObject *
Event_wait(Event *self, PyObject *args)
{
    PyObject *py_timeout = NULL;
    unsigned int flag;
    struct timespec timeout;

    if (!PyArg_ParseTuple(args, "|O:wait", &py_timeout)) {
        return NULL;
    }

    /* Do the parsing of py_timeout before acquireing self->lock
     * => Easier error handling
     */
    if (py_timeout != NULL) {
        if (parse_timeout(py_timeout, &timeout) == -1) {
            return NULL;
        }
    }

    Py_BEGIN_ALLOW_THREADS
    pthread_mutex_lock(&self->lock);

    if (self->flag == 0) {
        if (py_timeout == NULL) {
            pthread_cond_wait(&self->cond, &self->lock);
        }
        else {
            pthread_cond_timedwait(&self->cond, &self->lock, &timeout);
        }
    }

    flag = self->flag;

    pthread_mutex_unlock(&self->lock);
    Py_END_ALLOW_THREADS

    if (flag == 1) {
        Py_RETURN_TRUE;
    }
    Py_RETURN_FALSE;
}


static PyMethodDef Event_methods[] = {
    {"is_set", (PyCFunction)Event_is_set, METH_NOARGS, ""},
    {"set", (PyCFunction)Event_set, METH_NOARGS, ""},
    {"clear", (PyCFunction)Event_clear, METH_NOARGS, ""},
    {"wait", (PyCFunction)Event_wait, METH_VARARGS, ""},
    {NULL, NULL, 0, NULL}
};


static PyTypeObject EventType = {
    PyObject_HEAD_INIT(NULL)
    0,                         /*ob_size*/
    "_pthread_event.Event",     /*tp_name*/
    sizeof(Event),             /*tp_basicsize*/
    0,                         /*tp_itemsize*/
    (destructor)Event_dealloc, /*tp_dealloc*/
    0,                         /*tp_print*/
    0,                         /*tp_getattr*/
    0,                         /*tp_setattr*/
    0,                         /*tp_compare*/
    0,                         /*tp_repr*/
    0,                         /*tp_as_number*/
    0,                         /*tp_as_sequence*/
    0,                         /*tp_as_mapping*/
    0,                         /*tp_hash */
    0,                         /*tp_call*/
    0,                         /*tp_str*/
    0,                         /*tp_getattro*/
    0,                         /*tp_setattro*/
    0,                         /*tp_as_buffer*/
    Py_TPFLAGS_DEFAULT | Py_TPFLAGS_BASETYPE, /*tp_flags*/
    "",                        /* tp_doc */
    0,                         /* tp_traverse */
    0,                         /* tp_clear */
    0,                         /* tp_richcompare */
    0,                         /* tp_weaklistoffset */
    0,                         /* tp_iter */
    0,                         /* tp_iternext */
    Event_methods,             /* tp_methods */
    0,                         /* tp_members */
    0,                         /* tp_getset */
    0,                         /* tp_base */
    0,                         /* tp_dict */
    0,                         /* tp_descr_get */
    0,                         /* tp_descr_set */
    0,                         /* tp_dictoffset */
    0,                         /* tp_init */
    0,                         /* tp_alloc */
    Event_new,                 /* tp_new */
};

typedef struct {
    PyObject_HEAD
    sem_t lock;
    unsigned int locked:1;
} SemLock;

static PyObject *
SemLock_new(PyTypeObject *type, PyObject *args, PyObject *kwargs)
{
    int error;
    SemLock *self = (SemLock*) type->tp_alloc(type, 0);

    error = sem_init(&self->lock, 0, 1);
    if (error) {
        type->tp_free(self);
        return py_error_from_error("Cannot init sem", errno);
    }
    self->locked = 0;
    return (PyObject*)self;
}

static void
SemLock_dealloc(SemLock *self)
{
    sem_destroy(&self->lock);
    Py_TYPE(self)->tp_free(self);
}

static PyObject*
SemLock_acquire(SemLock *self, PyObject *args)
{
    PyObject *py_timeout = NULL;
    struct timespec timeout;
    int error;

    if (!PyArg_ParseTuple(args, "|O:timeout", &py_timeout)) {
        return NULL;
    }

    if (py_timeout != NULL) {
        if (parse_timeout(py_timeout, &timeout) == -1) {
            return NULL;
        }
    }

    Py_BEGIN_ALLOW_THREADS
    if (py_timeout == NULL) {
        error = sem_wait(&self->lock);
    }
    else {
        error = sem_timedwait(&self->lock, &timeout);
    }
    Py_END_ALLOW_THREADS

    if (error) {
        if ((errno == ETIMEDOUT) || (errno == EINTR)) {
            Py_RETURN_FALSE;
        }
        else {
            return py_error_from_error("Cannot acquire", errno);
        }
    }
    self->locked = 1;
    Py_RETURN_TRUE;
}

static PyObject*
SemLock_release(SemLock *self)
{
    int error;
    if (self->locked == 0) {
        return PyErr_Format(PyExc_Exception, "release unlocked lock");
    }

    error = sem_post(&self->lock);
    if (error) {
        return py_error_from_error("Cannot release", errno);
    }
    self->locked = 0;
    Py_RETURN_NONE;
}

static PyMethodDef SemLock_methods[] = {
    {"release", (PyCFunction)SemLock_release, METH_NOARGS, ""},
    {"acquire", (PyCFunction)SemLock_acquire, METH_VARARGS, ""},
    {NULL, NULL, 0, NULL}
};


static PyTypeObject SemLockType = {
    PyObject_HEAD_INIT(NULL)
    0,                            /*ob_size*/
    "_pthread_event.SemLock",      /*tp_name*/
    sizeof(SemLock),              /*tp_basicsize*/
    0,                            /*tp_itemsize*/
    (destructor)SemLock_dealloc,  /*tp_dealloc*/
    0,                         /*tp_print*/
    0,                         /*tp_getattr*/
    0,                         /*tp_setattr*/
    0,                         /*tp_compare*/
    0,                         /*tp_repr*/
    0,                         /*tp_as_number*/
    0,                         /*tp_as_sequence*/
    0,                         /*tp_as_mapping*/
    0,                         /*tp_hash */
    0,                         /*tp_call*/
    0,                         /*tp_str*/
    0,                         /*tp_getattro*/
    0,                         /*tp_setattro*/
    0,                         /*tp_as_buffer*/
    Py_TPFLAGS_DEFAULT | Py_TPFLAGS_BASETYPE, /*tp_flags*/
    "",                        /* tp_doc */
    0,                         /* tp_traverse */
    0,                         /* tp_clear */
    0,                         /* tp_richcompare */
    0,                         /* tp_weaklistoffset */
    0,                         /* tp_iter */
    0,                         /* tp_iternext */
    SemLock_methods,           /* tp_methods */
    0,                         /* tp_members */
    0,                         /* tp_getset */
    0,                         /* tp_base */
    0,                         /* tp_dict */
    0,                         /* tp_descr_get */
    0,                         /* tp_descr_set */
    0,                         /* tp_dictoffset */
    0,                         /* tp_init */
    0,                         /* tp_alloc */
    SemLock_new,               /* tp_new */
};



PyMODINIT_FUNC
init_pthread_event(void) {
    PyObject *module;

    if (PyType_Ready(&EventType) < 0) {
        return;
    }

    if (PyType_Ready(&SemLockType) < 0) {
        return;
    }

    module = Py_InitModule("pthread_event._pthread_event", NULL);
    if (module == NULL) {
        return;
    }

    Py_INCREF((PyObject*) &EventType);
    PyModule_AddObject(module, "Event", (PyObject*)&EventType);

    Py_INCREF((PyObject*) &SemLockType);
    PyModule_AddObject(module, "SemLock", (PyObject*)&SemLockType);
}
