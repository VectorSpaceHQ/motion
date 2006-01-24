#ifndef __UTILS_H__
#define __UTILS_H__

/* Some includes needed for the API prototypes */
#include <sys/types.h>  /* needed for size_t */
#include <stdio.h>      /* needed for FILE   */
#include <time.h>       /* needed for struct tm */
/*
 * The following data is for the dictionary routines
 *
 * Define a structure for the dictionary itself
 */
typedef struct _dictionary dictionary;
typedef dictionary *dictionary_ptr;
/* Now define the API's */
void * mymalloc(size_t);
char * mystrndup(char *, size_t);
void * myrealloc(void *, size_t, const char *);
FILE * myfopen(const char *, const char *);
size_t mystrftime(struct context *, char *, size_t, const char *, const struct tm *, const char *, int);
int create_path(const char *);
void motion_log(int, int, const char *, ...);
dictionary_ptr dict_create(void);
int dict_delete(dictionary_ptr, const char *, int);
void dict_destroy(dictionary_ptr);
void *dict_lookup(dictionary_ptr, const char *, int);
int dict_add(dictionary_ptr dict, const char *, int, void *);
#endif
