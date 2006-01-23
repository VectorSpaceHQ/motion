/*
 * 	plugins.h
 *
 *	Include file for the "plugin" handling code of the Motion project.
 *
 *      Copyright 2005, Per Jönsson
 *      This software is distributed under the GNU Public license
 *      Version 2.  See also the file 'COPYING'.
 */

#ifndef __PLUGINS_H_
#define __PLUGINS_H_

#define MOTION_PLUGIN_SYM motionPlugin
#define MOTION_PLUGIN_SYM_NAME "motionPlugin"

#define	motion_ctxt_ptr struct context *

/*
 * Struct mhandle is an opaque structure defined and created within plugins.c.
 * All external references must be only through a pointer, defined here.
 */
typedef struct mhandle *mh_ptr;

typedef enum {
	MOTION_VIDEO_PLUGIN = 1,
} motion_plugin_type;

typedef enum {
	/* First group are for control (usually sent to plugin) */
	MOTION_LOAD_MSG = 1,     /* Perform initial load actions */
	MOTION_UNLOAD_MSG,       /* Close down module functions */
	MOTION_GET_CAPS,         /* Provide capabilities */
	MOTION_VALIDATION_TABLE, /* Parameter validation routine */
	MOTION_VALIDATION_SIZE,  /* Number of entries in table */

	/* Second group are for actions (usually received from plugin) */
	MOTION_VIDEO_INPUT = 0x1000,
} motion_msg;

/**
 * struct motion_plugin_descr
 *
 *   Struct for storing plugin info. There are three important parts:
 *
 *   1. Plugin data - name, version etc. Could add more fields here, e.g.
 *      for copyright data.
 *   2. Flags - plugin flags (see above).
 *   3. API - the union defined above.
 *
 *   The idea is that a plugin defines a static instance of this struct
 *   with a specific name (see below), and the plugin loader checks for the
 *   presence of the instance (it's mandatory) and can return it to the 
 *   application.
 */
typedef struct
{
	motion_plugin_type type;   /* type identifier for the plugin */
	const char *name;          /* name of the plugin */
	const char *version;       /* plugin version as a string */
	const char *author;        /* plugin author */
	const char *homepage;      /* plugin homepage (is this relevant?) */
	unsigned int flags;        /* plugin flags, see above */
	void *(*motion_plugin_control)(motion_msg);
} motion_plugin_descr;

/**
 * typedef motion_video_input
 *
 * This structure provides pointers to the routines provided by the plugin.
 * Note that, for a given plugin, some of them may be NULL (i.e. facility
 * not provided).  Note that it is normally static data pre-defined within
 * the plugin.
 */
typedef struct _motion_video_input
{
	void *video_params;                 /* configuration parameters */
	void (*video_init)(motion_ctxt_ptr);
	int (*video_start)(motion_ctxt_ptr);
	int (*video_next)(motion_ctxt_ptr, unsigned char *);
	void (*video_close)(motion_ctxt_ptr);
	void (*video_dev_cleanup)(motion_ctxt_ptr);/* for individual device */
	void (*video_cleanup)(motion_ctxt_ptr);    /* for all devices */
	int (*video_pipe_start)(motion_ctxt_ptr, const char *, int, int, int);
	int (*video_pipe_put)(motion_ctxt_ptr, int, unsigned char *, int);
} motion_video_input, *motion_video_input_ptr;

/**
 * plugin_load
 *
 *   Loads the plugin from the file specified by 'filename'. If 'lazy' is 
 *   non-zero, undefined symbols in the loaded plugin will be resolved 
 *   when they are used, otherwise immediately.
 *
 * Parameters:
 *
 *   filename - the plugin filename
 *   lazy     - controls resolution of undefined symbols
 *
 * Returns: the plugin handle
 */
mh_ptr plugin_load(const char *filename, int lazy);

/**
 * plugin_data
 * 
 *   Returns the plugin info struct (see plugapi.h). As the handle returned 
 *   from plugin_open() is opaque, this function must be used to access the
 *   plugin data, flags and API.
 *
 *   Note: perhaps this function should be named something else...
 *
 * Parameters:
 *
 *   handle - the plugin handle returned from plugin_load()
 *
 * Returns: the plugin info struct
 */
motion_plugin_descr *plugin_data(mh_ptr handle);

/**
 * plugin_close
 *
 *   Closes/unloads the plugin specified by the handle.
 *
 * Parameters:
 *
 *   handle - the plugin handle returned from plugin_load()
 *
 * Returns: nothing
 */
void plugin_close(mh_ptr handle);

/**
 * plugin_resolve
 *
 *   Resolves a symbol in the plugin specified by the handle. The address of
 *   the symbol (be it a function, a struct or anything else) is stored in the
 *   pointer pointed to by 'address'.
 *
 * Parameters:
 *
 *   handle  - the plugin handle returned from plugin_load()
 *   symbol  - the symbol to resolve, e.g. the name of a function
 *   address - pointer to a pointer that will receive the symbol address
 *
 * Returns: non-zero (true) on success, zero (false) on failure
 */
int plugin_resolve(mh_ptr handle, const char *symbol, void **address);

int plugin_resolve_nolock(mh_ptr handle, const char *symbol,
                                  void **address);

mh_ptr plugin_cleanup(mh_ptr handle);

#endif /* __PLUGINS_H_ */

