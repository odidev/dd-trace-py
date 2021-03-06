#include <string.h>

#define PY_SSIZE_T_CLEAN
#include <Python.h>
#include <frameobject.h>

#include "_memalloc_tb.h"
#include "_pymacro.h"

/* Temporary traceback buffer to store new traceback */
static traceback_t* traceback_buffer = NULL;

/* A string containing "<unknown>" just in case we can't store the real function
 * or file name. */
static PyObject* unknown_name = NULL;

#define TRACEBACK_SIZE(NFRAME) (sizeof(traceback_t) + sizeof(frame_t) * (NFRAME - 1))

int
memalloc_tb_init(uint16_t max_nframe)
{
    if (unknown_name == NULL) {
        unknown_name = PyUnicode_FromString("<unknown>");
        if (unknown_name == NULL)
            return -1;
        PyUnicode_InternInPlace(&unknown_name);
    }

    /* Allocate a buffer that can handle the largest traceback possible.
       This will be used a temporary buffer when converting stack traces. */
    traceback_buffer = PyMem_RawMalloc(TRACEBACK_SIZE(max_nframe));

    if (traceback_buffer == NULL)
        return -1;

    return 0;
}

void
memalloc_tb_deinit(void)
{
    PyMem_RawFree(traceback_buffer);
}

void
traceback_free(traceback_t* tb)
{
    for (uint16_t nframe = 0; nframe < tb->nframe; nframe++) {
        Py_DECREF(tb->frames[nframe].filename);
        Py_DECREF(tb->frames[nframe].name);
    }
    PyMem_RawFree(tb);
}

void
traceback_list_init(traceback_list_t* tb_list, uint16_t size)
{
    tb_list->tracebacks = PyMem_RawMalloc(sizeof(traceback_t*) * size);
    tb_list->count = 0;
    tb_list->size = size;
}

void
traceback_list_wipe(traceback_list_t* tb_list)
{
    for (uint16_t i = 0; i < tb_list->count; i++)
        PyMem_RawFree(tb_list->tracebacks[i]);
    PyMem_RawFree(tb_list->tracebacks);
}

void
traceback_list_append_traceback(traceback_list_t* tb_list, traceback_t* tb)
{
    /* WARNING: this does not check size mostly because it does not know what to do if size is reached */
    tb_list->tracebacks[tb_list->count++] = tb;
}

/* Convert PyFrameObject to a frame_t that we can store in memory */
static void
memalloc_convert_frame(PyFrameObject* pyframe, frame_t* frame)
{
    int lineno = PyFrame_GetLineNumber(pyframe);
    if (lineno < 0)
        lineno = 0;

    frame->lineno = (unsigned int)lineno;

    PyObject *filename, *name;

#ifdef _PY39_AND_LATER
    PyCodeObject* code = PyFrame_GetCode(pyframe);
#else
    PyCodeObject* code = pyframe->f_code;
#endif

    if (code == NULL) {
        filename = unknown_name;
        name = unknown_name;
    } else {
        filename = code->co_filename;
        name = code->co_name;
    }

#ifdef _PY39_AND_LATER
    Py_DECREF(code);
#endif

    if (name)
        frame->name = name;
    else
        frame->name = unknown_name;

    Py_INCREF(frame->name);

    if (filename)
        frame->filename = filename;
    else
        frame->filename = unknown_name;

    Py_INCREF(frame->filename);
}

static traceback_t*
memalloc_frame_to_traceback(PyFrameObject* pyframe, uint16_t max_nframe)
{
    traceback_buffer->total_nframe = 0;
    traceback_buffer->nframe = 0;

    for (; pyframe != NULL;) {
        if (traceback_buffer->nframe < max_nframe) {
            memalloc_convert_frame(pyframe, &traceback_buffer->frames[traceback_buffer->nframe]);
            traceback_buffer->nframe++;
        }
        /* Make sure we don't overflow */
        if (traceback_buffer->total_nframe < UINT16_MAX)
            traceback_buffer->total_nframe++;

#ifdef _PY39_AND_LATER
        PyFrameObject* back = PyFrame_GetBack(pyframe);
        Py_DECREF(pyframe);
        pyframe = back;
#else
        pyframe = pyframe->f_back;
#endif
    }

    size_t traceback_size = TRACEBACK_SIZE(traceback_buffer->nframe);
    traceback_t* traceback = PyMem_RawMalloc(traceback_size);

    if (traceback)
        memcpy(traceback, traceback_buffer, traceback_size);

    return traceback;
}

traceback_t*
memalloc_get_traceback(uint16_t max_nframe, void* ptr, size_t size)
{
    PyThreadState* tstate = PyThreadState_Get();

    if (tstate == NULL)
        return NULL;

#ifdef _PY39_AND_LATER
    PyFrameObject* pyframe = PyThreadState_GetFrame(tstate);
#else
    PyFrameObject* pyframe = tstate->frame;
#endif

    if (pyframe == NULL)
        return NULL;

    traceback_t* traceback = memalloc_frame_to_traceback(pyframe, max_nframe);

    if (traceback == NULL)
        return NULL;

    traceback->size = size;
    traceback->ptr = ptr;

#ifdef _PY37_AND_LATER
    traceback->thread_id = PyThread_get_thread_ident();
#else
    traceback->thread_id = tstate->thread_id;
#endif

    return traceback;
}
