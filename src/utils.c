/**
 * utils.c
 *
 * This module contains a collection of "utility routines" which are used in
 * the remaining "motion" modules.  They are gathered together here because
 * they have no particular dependencies on the other modules, and are mostly
 * used in several different modules.
 */
#include "motion.h"    /* note that motion.h includes utils.h (plus a lot more) */
/**
 * mymalloc
 * 
 *   Allocates some memory and checks if that succeeded or not. If it failed,
 *   do some error logging and bail out.
 *
 *   NOTE: Kenneth Lavrsen changed printing of size_t types so instead of using 
 *   conversion specifier %zd I changed it to %llu and casted the size_t 
 *   variable to unsigned long long. The reason for this nonsense is that older 
 *   versions of gcc like 2.95 uses %Zd and does not understand %zd. So to avoid
 *   this mess I used a more generic way. Long long should have enough bits for 
 *   64-bit machines with large memory areas.
 *
 * Parameters:
 *
 *   nbytes - no. of bytes to allocate
 *
 * Returns: a pointer to the allocated memory
 */
void * mymalloc(size_t nbytes)
{
	void *dummy = malloc(nbytes);
	if (!dummy) {
		motion_log(LOG_EMERG, 1, "Could not allocate %llu bytes of memory!", (unsigned long long)nbytes);
		exit(1);
	}

	return dummy;
}

/**
 * mystrndup
 *
 * Allocates and copies nbytes from source string to destination string.
 * If source string is NULL do some error login and bail out.
 * If allocation failed do some error login and bail out.
 * 
 * Parameters :
 * 
 * ptr : pointer to source string
 * nbytes : no. of bytes to copy from source string to destination string
 * 
 * Returns: a pointer to new allocated and copy string
 */ 
char *mystrndup(char *ptr, size_t nbytes)
{
	char *dummy = NULL;
	
	if (ptr == NULL){
		motion_log(LOG_EMERG, 1, "Could not copy NULL pointer");
		exit(1);	
	}
	
	dummy = (char *)mymalloc( nbytes+1 );
	
	if (dummy) {
		strncpy(dummy, ptr, nbytes);
		dummy[nbytes]= '\0';
	} else {
		motion_log(LOG_EMERG, 1, "Could not allocate %llu bytes of memory!", (unsigned long long)nbytes);
		exit(1);	
	}
	
	return dummy;
}

/**
 * myrealloc
 *
 *   Re-allocate (i.e., resize) some memory and check if that succeeded or not. 
 *   If it failed, do some errorlogging and bail out. If the new memory size
 *   is 0, the memory is freed.
 *
 * Parameters:
 *
 *   ptr  - pointer to the memory to resize/reallocate
 *   size - new memory size
 *   desc - name of the calling function 
 *   
 * Returns: a pointer to the reallocated memory, or NULL if the memory was 
 *          freed
 */
void *myrealloc(void *ptr, size_t size, const char *desc)
{
	void *dummy = NULL;

	if (size == 0) {
		free(ptr);
		motion_log(LOG_WARNING, 0,
		           "Warning! Function %s tries to resize memoryblock at %p to 0 bytes!",
		           desc, ptr);
	} else {
		dummy = realloc(ptr, size);
		if (!dummy) {
			motion_log(LOG_EMERG, 0,
			           "Could not resize memory-block at offset %p to %llu bytes (function %s)!",
			           ptr, (unsigned long long)size, desc);
			exit(1);
		}
	}

	return dummy;
}

/**
 * create_path
 *
 *   This function creates a whole path, like mkdir -p. Example paths:
 *      this/is/an/example/
 *      /this/is/an/example/
 *   Warning: a path *must* end with a slash!
 *
 * Parameters:
 *
 *   cnt  - current thread's context structure (for logging)
 *   path - the path to create
 *
 * Returns: 0 on success, -1 on failure
 */
int create_path(const char *path)
{
	char *start;
	mode_t mode = S_IRWXU | S_IRGRP | S_IXGRP | S_IROTH | S_IXOTH;

	if (path[0] == '/')
		start = strchr(path + 1, '/');
	else
		start = strchr(path, '/');

	while(start) {
		char *buffer = strdup(path);
		buffer[start-path] = 0x00;

		if (mkdir(buffer, mode) == -1 && errno != EEXIST) {
			motion_log(LOG_ERR, 1, "Problem creating directory %s", buffer);
			free(buffer);
			return -1;
		}

		free(buffer);

		start = strchr(start + 1, '/');
	}

	return 0;
}

/**
 * myfopen
 *
 *   This function opens a file, if that failed because of an ENOENT error 
 *   (which is: path does not exist), the path is created and then things are 
 *   tried again. This is faster then trying to create that path over and over 
 *   again. If someone removes the path after it was created, myfopen will 
 *   recreate the path automatically.
 *
 * Parameters:
 *
 *   path - path to the file to open
 *   mode - open mode
 *
 * Returns: the file stream object
 */
FILE * myfopen(const char *path, const char *mode)
{
	/* first, just try to open the file */
	FILE *dummy = fopen(path, mode);

	/* could not open file... */
	if (!dummy) {
		/* path did not exist? */
		if (errno == ENOENT) {
			//DEBUG CODE  syslog(LOG_DEBUG, "Could not open file %s directly; path did not exist. Creating path & retrying.", path);

			/* create path for file... */
			if (create_path(path) == -1)
				return NULL;

			/* and retry opening the file */
			dummy = fopen(path, mode);
			if (dummy)
				return dummy;
		}

		/* two possibilities
		 * 1: there was an other error while trying to open the file for the first time
		 * 2: could still not open the file after the path was created
		 */
		motion_log(LOG_ERR, 1, "Error opening file %s with mode %s", path, mode);

		return NULL;
	}

	return dummy;
}

/**
 * mystrftime
 *
 *   Motion-specific variant of strftime(3) that supports additional format
 *   specifiers in the format string.
 *
 * Parameters:
 *
 *   cnt        - current thread's context structure
 *   s          - destination string
 *   max        - max number of bytes to write
 *   userformat - format string
 *   tm         - time information
 *   filename   - string containing full path of filename
 *                set this to NULL if not relevant
 *   sqltype    - Filetype as used in SQL feature, set to 0 if not relevant
 *
 * Returns: number of bytes written to the string s
 */
size_t mystrftime(struct context *cnt, char *s, size_t max, const char *userformat,
                  const struct tm *tm, const char *filename, int sqltype)
{
	char formatstring[PATH_MAX] = "";
	char tempstring[255] = "";
	char *format, *tempstr;
	const char *pos_userformat;

	format = formatstring;

	/* if mystrftime is called with userformat = NULL we return a zero length string */
	if (userformat == NULL) {
		*s = '\0';
		return 0;
	}	

	for (pos_userformat = userformat; *pos_userformat; ++pos_userformat) {

		if (*pos_userformat == '%') {
			/* Reset 'tempstr' to point to the beginning of 'tempstring',
			 * otherwise we will eat up tempstring if there are many
			 * format specifiers.
			 */
			tempstr = tempstring;
			tempstr[0] = '\0';
			switch (*++pos_userformat) {
				case '\0': // end of string
					--pos_userformat;
					break;

				case 'v': // event
					sprintf(tempstr, "%02d", cnt->event_nr);
					break;

				case 'q': // shots
					sprintf(tempstr, "%02d", cnt->shots);
					break;

				case 'D': // diffs
					sprintf(tempstr, "%d", cnt->diffs);
					break;

				case 'N': // noise
					sprintf(tempstr, "%d", cnt->noise_level);
					break;

				case 'i': // motion width
					sprintf(tempstr, "%d", cnt->location.width);
					break;

				case 'J': // motion height
					sprintf(tempstr, "%d", cnt->location.height);
					break;

				case 'K': // motion center x
					sprintf(tempstr, "%d", cnt->location.x);
					break;

				case 'L': // motion center y
					sprintf(tempstr, "%d", cnt->location.y);
					break;

				case 'o': // threshold
					sprintf(tempstr, "%d", cnt->threshold);
					break;

				case 'Q': // number of labels
					sprintf(tempstr, "%d", cnt->imgs.total_labels);
					break;
				case 't': // thread number
					sprintf(tempstr, "%d",(int)(unsigned long)
					                pthread_getspecific(tls_key_threadnr));
					break;
				case 'C': // text_event
					if (cnt->text_event_string && cnt->text_event_string[0])
						sprintf(tempstr, "%s", cnt->text_event_string);
					else
						++pos_userformat;
					break;
				case 'f': // filename
					if (filename)
						sprintf(tempstr, "%s", filename);
					else
						++pos_userformat;
					break;
				case 'n': // sqltype
					if (sqltype)
						sprintf(tempstr, "%d", sqltype);
					else
						++pos_userformat;
					break;
				default: // Any other code is copied with the %-sign
					*format++ = '%';
					*format++ = *pos_userformat;
					continue;
			}

			/* If a format specifier was found and used, copy the result from
			 * 'tempstr' to 'format'.
			 */
			if (tempstr[0]) {
				while ((*format = *tempstr++) != '\0')
					++format;
				continue;
			}
		}

		/* For any other character than % we just simply copy the character */
		*format++ = *pos_userformat;
	}

	*format = '\0';
	format = formatstring;

	return strftime(s, max, format, tm);
}

/**
 * motion_log
 * 
 *	This routine is used for printing all informational, debug or error
 *	messages produced by any of the other motion functions.  It always
 *	produces a message of the form "[n] {message}", and (if the param
 *	'errno_flag' is set) follows the message with the associated error
 *	message from the library.
 *
 * Parameters:
 *
 * 	level           logging level for the 'syslog' function
 * 	                (-1 implies no syslog message should be produced)
 * 	errno_flag      if set, the log message should be followed by the
 * 	                error message.
 * 	fmt             the format string for producing the message
 * 	ap              variable-length argument list
 *
 * Returns:
 * 	                Nothing
 */
void motion_log(int level, int errno_flag, const char *fmt, ...)
{
	int errno_save, n;
	char buf[1024];
#ifndef __freebsd__ 
	char msg_buf[100];
#endif
	va_list ap;
	int threadnr = 0;

	/* If pthread_getspecific fails (e.g., because the thread's TLS doesn't
	 * contain anything for thread number, it returns NULL which casts to zero,
	 * which is nice because that's what we want in that case.
	 * Note from Bill:  The above is true according to the manual, but
	 * in testing it was noted that under some error condition (I think
	 * it's when no thread has yet been started) nothing is returned.
	 * Therefore, I changed the declaration of threadnr to set it to 0.
	 */
	threadnr = (unsigned long)pthread_getspecific(tls_key_threadnr);                     

	/*
	 * First we save the current 'error' value.  This is required because
	 * the subsequent calls to vsnprintf could conceivably change it!
	 */
	errno_save = errno;

	/* Prefix the message with the thread number */
	n = snprintf(buf, sizeof(buf), "[%d] ", threadnr);

	/* Next add the user's message */
	va_start(ap, fmt);
	n += vsnprintf(buf + n, sizeof(buf) - n, fmt, ap);

	/* If errno_flag is set, add on the library error message */
	if (errno_flag) {
		strcat(buf, ": ");
		n += 2;
		/*
		 * this is bad - apparently gcc/libc wants to use the non-standard GNU
		 * version of strerror_r, which doesn't actually put the message into
		 * my buffer :-(.  I have put in a 'hack' to get around this.
		 */
#ifdef __freebsd__
		strerror_r(errno_save, buf + n, sizeof(buf) - n);	/* 2 for the ': ' */
#else
		strcat(buf, strerror_r(errno_save, msg_buf, sizeof(msg_buf)));
#endif
	}
	/* If 'level' is not negative, send the message to the syslog */
	if (level >= 0)
		syslog(level, buf);

	/* For printing to stderr we need to add a newline */
	strcat(buf, "\n");
	fputs(buf, stderr);
	fflush(stderr);

	/* Clean up the argument list routine */
	va_end(ap);
}

/**
 * The "dictionary" routines are, at the moment, used only with newconfig.c,
 * but may (in the near future) used in some other modules.
 */
#define MIN_DICT_SIZE 50
#define MAX_HASH_LEN 4
/*
 * Define a structure for a dictionary entry
 */
typedef struct _dict_entry dict_entry;
typedef dict_entry *dict_entry_ptr;
struct _dict_entry {
	dict_entry_ptr    next;
	char              *name;
	int               len;
	int               valid;
	void              *ptr;
};

/*
 * Define the dictionary structure
 */
struct _dictionary {
	dict_entry_ptr    dict;
	int               size;
	int               nb_entries;
};

/**
 * dict_compute_key
 * Calculate the hash key for the dictionary entry
 *
 * Parameters:
 *      name            name for which a key is desired
 *      int             length of the name
 */
static unsigned long dict_compute_key(const char *name, int namelen) {
	unsigned long value = 0L;

	if (name == NULL)
		return 0;
	value = *name;
	value <<= 5;
	if (namelen > 10) {
		value += name[namelen - 1];
		namelen = 10;
	}
	switch (namelen) {
		case 10: value += name[9];
		case  9: value += name[8];
		case  8: value += name[7];
		case  7: value += name[6];
		case  6: value += name[5];
		case  5: value += name[4];
		case  4: value += name[3];
		case  3: value += name[2];
		case  2: value += name[1];
		default:
			 break;
	}
	return value;
}

/**
 * dict_create
 *
 * Create a new dictionary.
 *
 * Returns a pointer to the new dictionary, or NULL if any error
 */
dictionary_ptr dict_create(void) {
	dictionary_ptr dict;

	dict = malloc(sizeof(dictionary));
	dict->size = MIN_DICT_SIZE;
	dict->nb_entries = 0;
	dict->dict = malloc(MIN_DICT_SIZE * sizeof(dict_entry));
	memset(dict->dict, 0, MIN_DICT_SIZE * sizeof(dict_entry));
	return dict;
}

/**
 * dict_destroy
 *
 * Free the hash dictionary and it's contents.
 *
 * Parameters:
 *      dict            pointer to the dictionary
 *
 */
void dict_destroy(dictionary_ptr dict) {
	int i;
	dict_entry_ptr iter, next;
	int inside_dict = 0;

	if (dict == NULL)
		return;
	if (dict->dict) {
		for (i = 0; ((i < dict->size) && (dict->nb_entries > 0)); i++) {
			iter = &(dict->dict[i]);
			if (iter->valid == 0)
				continue;
			inside_dict = 1;
			while (iter) {
				next = iter->next;
				if (inside_dict) {
					inside_dict = 0;
				} else {
					free(iter->name);
					free(iter);
				}
				dict->nb_entries--;
				iter = next;
			}
		}
		free(dict->dict->name);
		free(dict->dict);
	}
	free(dict);
}

/**
 * dict_grow
 *
 * Resize a dictionary
 *
 * Parameters:
 *      dict            pointer to the dictionary
 *      size            new size for the dictionary
 *
 * Returns:             0 for success, -1 for any failure
 */
static int dict_grow(dictionary_ptr dict, int size) {
	unsigned long key;
	int oldsize, i;
	dict_entry_ptr iter, next, olddict;
	
	if (dict == NULL)
		return -1;
	if ((size < 8) || (size > 8 * 2048))
		return -1;
	
	oldsize = dict->size;
	olddict = dict->dict;
	if (olddict == NULL)
		return -1;

	dict->dict = malloc(size * sizeof(dict_entry));
	memset(dict->dict, 0, size * sizeof(dict_entry));
	dict->size = size;
	
	for (i = 0; i < oldsize; i++) {
		if (olddict[i].valid == 0)
			continue;
		key = dict_compute_key(olddict[i].name, olddict[i].len) % dict->size;
		memcpy(&(dict->dict[key]), &(olddict[i]), sizeof(dict_entry));
		dict->dict[key].next = NULL;
	}

	for (i = 0; i < oldsize; i++) {
		iter = olddict[i].next;
		while (iter) {
			next = iter->next;
			/* Try to put the entry into the new dict */
			key = dict_compute_key(iter->name, iter->len) % dict->size;
			if (dict->dict[key].valid == 0) {
				memcpy(&(dict->dict[key]), iter, sizeof(dict_entry));
				dict->dict[key].next = NULL;
				dict->dict[key].valid = 1;
				free(iter);
			} else {
				iter->next = dict->dict[key].next;
				dict->dict[key].next = iter;
			}
			iter = next;
		}
	}
	free(olddict);
	return 0;
}

/**
 * dict_lookup
 *
 * Check if the name is in the dictionary.  If so, return the
 * pointer associated with the name.
 *
 * Parameters:
 *      dict            pointer to the dictionary
 *      name            the name to be looked up
 *      len             the length of the name.  If -1, it will
 *                      be computed.
 *
 * Returns:             pointer associated to name if successful,
 *                      otherwise NULL
 */
void *dict_lookup(dictionary_ptr dict, const char *name, int len) {
	unsigned long key, okey = 0;
	dict_entry_ptr insert;

	if ((dict == NULL) || (name == NULL) || (len == 0))
		return NULL;

	if (len < 0)
		len = strlen(name);

	okey = dict_compute_key(name, len);
	key = okey % dict->size;
	if (dict->dict[key].valid == 0) {
		return  NULL;
	} else {
		for (insert = &(dict->dict[key]); insert->next != NULL;
				insert = insert->next) {
			if (insert->len == len) {
				if (!memcmp(insert->name, name, len))
					return insert->ptr;
			}
		}
		if ((insert->len == len) && (!strncmp(insert->name, name, len)))
			return insert->ptr;
	}
	return NULL;
}

/**
 * dict_delete
 *
 * Deletes the specified name from the dictionary
 *
 * Parameters:
 *      dict            pointer to the dictionary
 *      name            entry to be deleted
 *      len             length of the entry - if -1 will be calculated
 *
 * Returns:             0 if successful, -1 if any error
 */
int dict_delete(dictionary_ptr dict, const char *name, int len) {
	unsigned long key, okey;
	dict_entry_ptr ptr, prev;

	if ((dict == NULL) || (name == NULL) || (len == 0))
		return -1;

	if (len < 0)
		len = strlen(name);
	okey = dict_compute_key(name, len);
	key = okey % dict->size;
	if (dict->dict[key].valid == 0)
		return -1;
	/* found a valid entry */
	prev = &(dict->dict[key]);
	for (ptr = prev; ptr->next != NULL;
			ptr = ptr->next) {
		if ((ptr->len == len) && (!strncmp(ptr->name, name, len))) {
			free(ptr->name);
			*ptr = *ptr->next;
			return 0;
		}
		prev = ptr;
	}
	if ((ptr->len == len) && !(strncmp(ptr->name, name, len))) {
		prev->next = NULL;
		free(ptr->name);
		if (prev == ptr)
			prev->valid = 0;
		else
			free(ptr);
		return 0;
	}

	return -1;
}

/**
 * dict_add
 *
 * Adds a name and associated pointer value to the dictionary.
 *
 * Parameters:
 *      dict            pointer to the dictionary
 *      name            the name to be added
 *      len             length of the name; if -1, will be computed
 *      ptr             pointer to be associated with name
 *
 * Returns:             0 for success, -1 for any error
 */
int dict_add(dictionary_ptr dict, const char *name, int len, void *ptr) {
	unsigned long key, okey, nbi = 0;
	dict_entry_ptr entry, insert;

	if ((dict == NULL) || (name == NULL) || (len == 0))
		return -1;

	if (len < 0)
		len = strlen(name);
	okey = dict_compute_key(name, len);
	key = okey % dict->size;
	/*
	 * Check if the name is already in the dictionary.  If so,
	 * return with an error.
	 */
	if (dict->dict[key].valid == 0) {
		insert = NULL;
	} else {
		for (insert = &(dict->dict[key]); insert->next != NULL;
				insert = insert->next) {
			if (insert->len == len) {
				if (!memcmp(insert->name, name, len))
					return -1;
			}
		}
		if (insert->len == len) {
			if (!memcmp(insert->name, name, len))
				return -1;
		}
	}

	if (insert == NULL) {
		entry = &(dict->dict[key]);
	} else {
		entry = malloc(sizeof(dict_entry));
	}
	entry->name = strdup(name);
	entry->len = len;
	entry->next = NULL;
	entry->valid = 1;
	entry->ptr = ptr;

	if (insert != NULL)
		insert->next = entry;

	dict->nb_entries++;

	/*
	 * Check the "sparseness" of the dictionary.  If it is getting
	 * "cluttered", then grow it.
	 */
	if (nbi > MAX_HASH_LEN)
		dict_grow(dict, MAX_HASH_LEN * 2 * dict->size);

	return 0;
}

