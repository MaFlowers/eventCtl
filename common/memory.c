#include "memory.h"
#include <string.h>
#include "memtypes.h"
#include <stdio.h>
#include <stdlib.h>

extern int errno;
/*
 *	Records of dynamic memory allocation
 */
static struct 
{
  char *name;	/*The name of the memory*/
  long alloc;	/*The amount of the memory which has the name*/
}mstat[MTYPE_MAX];

/*Wrapper around strerror to handle case where it returns NULL.*/
const char *safe_strerror(int errnum)
{
	const char *s = strerror(errnum);
	return (s != NULL) ? s : "Unknown error";
}

/* Increment allocation counter. */
static void alloc_inc(int type)
{
	mstat[type].alloc++;
}

/* Decrement allocation counter. */
static void alloc_dec(int type)
{
	mstat[type].alloc--;
}

/* Memory allocation. */
void *_malloc(int type, size_t size, const char *funcname, int line)
{
	void *memory = NULL;

	memory = malloc(size);
	if(NULL == memory)
	{			
		fprintf(stderr, "<%s, %d> malloc: can't allocate memory for '%s' size %d: %s\n", 
			  		funcname, line, mtype_str[type], (int)size, safe_strerror(errno));
		abort();
	}

	alloc_inc(type);

	return memory;
}

/* Memory allocation with num * size with cleared. */
void *_calloc(int type, size_t size, const char *funcname, int line)
{
	void *memory;

	memory = calloc(1, size);
	if(NULL == memory)
	{			
		fprintf(stderr, "<%s, %d> calloc: can't allocate memory for '%s' size %d: %s\n", 
					funcname, line, mtype_str[type], (int)size, safe_strerror(errno));
		abort();
	}
	
	alloc_inc (type);

	return memory;
}

/* Memory reallocation. */
void *_realloc(int type, void *ptr, size_t size, const char *funcname, int line)
{
	void *memory;

	memory = realloc(ptr, size);
	if(NULL == memory)
	{			
		fprintf(stderr, "<%s, %d> realloc: can't allocate memory for '%s' size %d: %s\n", 
					funcname, line, mtype_str[type], (int)size, safe_strerror(errno));
		abort();
	}
	
	return memory;
}

/* String duplication. */
char *_strdup(int type, const char *str, const char *funcname, int line)
{
	void *dup;

	dup = strdup (str);
	if (dup == NULL)	
	{			
		fprintf(stderr, "<%s, %d> strdup: can't allocate memory for '%s' : %s\n", 
					funcname, line, mtype_str[type], safe_strerror(errno));
		abort();
	}
	alloc_inc(type);
	
	return dup;
}

/* Memory free. */
void _free(int type, void *ptr)
{
	alloc_dec(type);
	free(ptr);
}

