#ifndef __MEMORY_H__
#define __MEMORY_H__
#include <stddef.h>
#include <errno.h>
#include "memtypes.h"
/*if memory allocation error, execute abort()*/
extern const char *safe_strerror(int errnum);
void *_malloc(int type, size_t size, const char *funcname, int line);
#define XMALLOC(a, b) _malloc(a, b, __FUNCTION__, __LINE__)
void *_calloc(int type, size_t size, const char *funcname, int line);
#define XCALLOC(a, b) _calloc(a, b, __FUNCTION__, __LINE__)
void *_realloc(int type, void *ptr, size_t size, const char *funcname, int line);
#define XREALLOC(a, b, c) _realloc(a, b, c, __FUNCTION__, __LINE__)
char *_strdup(int type, const char *str, const char *funcname, int line);
#define XSTRDUP(a, b) _strdup(a, b, __FUNCTION__, __LINE__)
void _free(int type, void *ptr);
#define XFREE(mtype, ptr) \
	do { \
		_free((mtype), (ptr)); \
		ptr = NULL; \
	}while (0)

#endif
