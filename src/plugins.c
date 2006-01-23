/*
 * 	plugins.c
 *
 *	This module contains the "plugin" handling code for the Motion project.
 *
 *      Copyright 2005, Per Jönsson
 *      This software is distributed under the GNU Public license
 *      Version 2.  See also the file 'COPYING'.
 */

#include "motion.h"    /* Some specific 'motion' structures utilised */
#include "plugins.h"

#include <dlfcn.h>
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <pthread.h>

/**
 * TODO
 * ====
 *
 * - Expose the error returned by dlerror() in some way. It could be stored in
 *   the mhandle struct, for example.
 */

/**
 * struct mhandle
 *
 *   The definition of the mhandle struct. This struct is returned as a handle
 *   from plugin_open().
 *   Note that the structure is "private" to this module.
 */
struct mhandle
{
	void *handle;     /* handle returned by dlopen() */

	char *filename;   /* the plugin filename */

	struct motion_plugin *
	pluginfo;         /* the plugin info struct present in the plugin */
};

/**
 * thread_lock
 *
 *   This is a global mutex that is used to make the plugin-handling functions 
 *   thread-safe. The dlerror() function is not reentrant, so we need to 
 *   protect all dl* functions from concurrent access, as well as protect our
 *   internal plugin structures from potential multi-thread interference.
 */
pthread_mutex_t thread_lock = PTHREAD_MUTEX_INITIALIZER;

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
motion_plugin_descr *plugin_data(struct mhandle *handle)
{
	return (motion_plugin_descr *)handle->pluginfo;
}

/**
 * plugin_resolve_nolock
 *
 *   Resolves a symbol in the plugin, but without locking the mutex. This
 *   function is called from the other plugin functions, as they need to
 *   resolve symbols but handle locking themselves. We could also use an
 *   error-checking mutex and check for a deadlock situation, but this
 *   solution works.
 *
 * Parameters:
 *
 *   handle  - the plugin handle
 *   symbol  - the name of the symbol to resolve
 *   address - where to store the address of the resolved symbol
 *
 * Returns: 0 on failure, 1 on success
 */
int plugin_resolve_nolock(struct mhandle *handle, const char *symbol, 
                          void **address)
{
	void *addr;
	char *error;

	/* dlsym() may return a NULL address, which may be ok. Thus, to check for
	 * error, we have to clear the error with dlerror(), and then see if 
	 * dlerror() returns non-NULL after the call to dlsym().
	 */
	dlerror(); /* clear error */
	addr = dlsym(handle->handle, symbol);
	if ((error = dlerror()) != NULL) {
		fprintf(stderr, "Symbol lookup failed: %s\n", error);
		*address = NULL;
		return 0;
	}
	
	*address = addr;
	return 1;
}


/**
 * plugin_cleanup
 *
 *   Tries to free the plugin handle resources
 *
 * Parameters:
 *  
 *  handle - the plugin handle  
 *  
 * Returns: NULL
 */
struct mhandle *plugin_cleanup(struct mhandle *handle)
{
	if (handle) {
		if (handle->handle) {
			dlclose(handle->handle);
		}
		free(handle->filename);
		free(handle);
	}
	pthread_mutex_unlock(&thread_lock);
	
	return NULL;
}



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
 *
 * FIXME:  I'm still torn between a decision whether or not we should maintain
 * a global list of plugins already loaded (separate from the dlopen structure
 * controlling when a module can be unloaded) - this will depend upon whether
 * we decide it's necessary to control calling initialisation routines (i.e.
 * if the same plugin is loaded by several threads, do we need to control
 * whether one or more of the initialisations are only done once).  If we
 * decide it's needed, it should be created / maintained within this routine
 * (and maintained within plugin_unload).
 */
struct mhandle *plugin_load(const char *filename, int lazy)
{
	void *handle;
	struct mhandle *ret = NULL;
	struct motion_plugin *modinfo;
	char *error;

	pthread_mutex_lock(&thread_lock);
	
	/* Try to open the library that contains the plugin. */
	handle = dlopen(filename, lazy ? RTLD_LAZY : RTLD_NOW);
	if (!handle) {
		error = dlerror();
		fprintf(stderr, "Failed to load the plugin '%s' - %s.\n", filename, error);
		return plugin_cleanup(ret);
	}

	/* Allocate the plugin handle struct. */
	ret = mymalloc(sizeof(struct mhandle));
	memset(ret, 0, sizeof(struct mhandle)); /* reset */
	ret->handle = handle;

	/* Get the plugin info, which is mandatory. */
	if (!plugin_resolve_nolock(ret, MOTION_PLUGIN_SYM_NAME, (void **)&modinfo)) {
		fprintf(stderr, "No plugin info found in plugin '%s'.\n", filename);
		return plugin_cleanup(ret);
	}

	ret->pluginfo = modinfo;
	ret->filename = strdup(filename);

#if 0	
	/* Call the load function if it exists in the plugin (it's ok if it 
	 * isn't there).
	 */
	if (plugin_resolve_nolock(ret, M_LOAD_FUNC, (void **)&fnload)) {
		if (!(*fnload)()) {
			/* Failure return from the load function. */
			fprintf(stderr, "The plugin load function returned with failure.\n");
			goto err;
		}
	}
#endif
	
	pthread_mutex_unlock(&thread_lock);
	return ret;
}

/* plugin_close - closes/unloads the specified plugin */
void plugin_close(struct mhandle *handle)
{

	pthread_mutex_lock(&thread_lock);

#if 0
	/* Call the unload function if it exists. */
	if (plugin_resolve_nolock(handle, M_UNLOAD_FUNC, (void **)&fnunload)) {
		(*fnunload)();
	}
#endif

	/* Free stuff and close the native dl handle... */
	free(handle->filename);
	dlclose(handle->handle);
	free(handle);

	pthread_mutex_unlock(&thread_lock);
}

/* plugin_resolve - resolves a symbol (wrapper around plugin_resolve_nolock) */
int plugin_resolve(struct mhandle *handle, const char *symbol, void **address)
{
	int ret;
	
	pthread_mutex_lock(&thread_lock);
	
	ret = plugin_resolve_nolock(handle, symbol, address);

	pthread_mutex_unlock(&thread_lock);

	return ret;
}

