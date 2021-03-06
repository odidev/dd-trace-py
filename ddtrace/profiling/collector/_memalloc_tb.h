#ifndef _DDTRACE_MEMALLOC_TB_H
#define _DDTRACE_MEMALLOC_TB_H

#include <stdint.h>

#include <Python.h>

typedef struct
#ifdef __GNUC__
  __attribute__((packed))
#elif defined(_MSC_VER)
#pragma pack(push, 4)
#endif
{
    PyObject* filename;
    PyObject* name;
    unsigned int lineno;
} frame_t;

typedef struct
{
    /* Total number of frames in the traceback */
    uint16_t total_nframe;
    /* Number of frames in the traceback */
    uint16_t nframe;
    /* Memory pointer allocated */
    void* ptr;
    /* Memory size allocated in bytes */
    size_t size;
    /* Thread ID */
    unsigned long thread_id;
    /* List of frames, top frame first */
    frame_t frames[1];
} traceback_t;

/* The maximum number of frames we can store in `traceback_t.nframe` */
#define TRACEBACK_MAX_NFRAME UINT16_MAX

typedef struct
{
    /* List of traceback */
    traceback_t** tracebacks;
    /* Size of the traceback list */
    uint16_t size;
    /* Number of tracebacks in the list of traceback */
    uint16_t count;
} traceback_list_t;

/* The maximum number of events we can store in `traceback_list_t.count` */
#define TRACEBACK_LIST_MAX_COUNT UINT16_MAX

int
memalloc_tb_init(uint16_t max_nframe);
void
memalloc_tb_deinit();

void
traceback_free(traceback_t* tb);

void
traceback_list_init(traceback_list_t* tb_list, uint16_t size);
void
traceback_list_wipe(traceback_list_t* tb_list);
void
traceback_list_append_traceback(traceback_list_t* tb_list, traceback_t* tb);

traceback_t*
memalloc_get_traceback(uint16_t max_nframe, void* ptr, size_t size);

#endif
