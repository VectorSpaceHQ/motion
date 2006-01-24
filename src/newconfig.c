/*
 *	newconfig.c
 *
 *	This module is a complete re-write of the motion configuration file
 *	processing.  It is intended to follow the format agreed on the Motion
 *	Twiki, MotionPluginConfigFileSpecification.
 *	
 *	Set debug level to 10 or higher for full debug info
 *
 */
#include "motion.h"	/* motion.h includes this module's header newconfig.h */

/* This #define is for testing checking of internal buffer overflow */
#define	INTERNAL_BUFF_SIZE	1024

#include "global_params.h"	/* global_params.h contains preset validation data */

/* The global var "dump_config_filename" is only for testing config file processing */
char *dump_config_filename;

static int parse_name_value(char *, int, char **, char **, int);

/**
 * load_plugin
 *
 * This code, called when a config param "plugin" is encountered, does the actual
 * loading of the requested plugin.
 *
 * Parameters:
 * 	cnt	pointer to motion context
 * 	name	name of the plugin
 * 	path	file path where the plugin is located
 *
 * Returns:	pointer to the entry in the global structure plugins_loaded
 * 		which describes this plugin, or NULL if any error
 * 
 */
static motion_plugin_ptr load_plugin(motion_ctxt_ptr cnt, const char *name,
                const char *path) {
	mh_ptr plug_handle;
	motion_plugin_descr *plug_descr;
	motion_plugin_ptr pptr = plugins_loaded; /* global variable */
	int *numentries;

	while (pptr) {
		if (!strcmp(pptr->plugin->name, name)) {
			cnt->callbacks->motion_log(LOG_ERR, 0, "Duplicate plugin load request");
			return NULL;
		}
		pptr = pptr->next;
	}

	if ((plug_handle = plugin_load(path, 0)) == NULL) {
		cnt->callbacks->motion_log(LOG_ERR, 0, "Error trying to load plugin");
		return NULL;
	}

	if ((plug_descr = plugin_data(plug_handle)) == NULL) {
		cnt->callbacks->motion_log(LOG_ERR, 0, "Error fetching plugin data");
		plugin_close(plug_handle);
		return NULL;
	}

	pptr = mymalloc(sizeof(motion_plugin));
	memset(pptr, 0, sizeof(motion_plugin));
	pptr->plugin = plug_descr;
	pptr->handle = plug_handle;
	pptr->plugin_validate = 
		pptr->plugin->motion_plugin_control(MOTION_VALIDATION_TABLE);
	numentries = 
		(int *)(pptr->plugin->motion_plugin_control(MOTION_VALIDATION_SIZE));
	pptr->plugin_validate_numentries = *numentries;
	return pptr;
}
/*
 * Following are the various utility routines used during validation
 */

/**
 * get_next_str
 *
 * Finds the first non-space character in a string and updates the
 * supplied pointer to point to the character.  Also provides the number
 * of non-space characters following.
 *
 * Parameters:
 * 	sptr		Pointer to a string pointer
 * 	len		Pointer to an integer to receive the length
 *
 * Returns:		Updated values for sptr and len.  Return value
 * 			of 0 for success, -1 for error.
 */
static int get_next_str(char **sptr, int *len) {
char	*wptr;
	while (**sptr == ' ')
		(*sptr)++;
	if (**sptr == 0)
		return -1;
	wptr = strchr(*sptr, ' ');
	if (wptr)
		*len = wptr - *sptr;
	else
		*len = strlen(*sptr);
	return 0;
}

/**
 * validate_set
 *
 * Compares the input value with a set of (space-separated) string values.
 * If a match is found, the routine returns the relative position within
 * the set.  If no match, an error message is printed.
 *
 * Parameters:
 * 	vptr		Pointer to the set of valid values
 * 	sptr		Pointer to the string to be validated
 * 	ival		If doing integer compare, the integer value
 * 	flag		Flag to control type of comparison
 * 			CMP_NOCASE  ignore case
 * 			CMP_INT     compare integer values
 *
 * Returns:		0 if valid, -1 for error
 *
 */

static int validate_set(const char *vptr, const char *sptr, int ival, int flag) {
int	len;
int	pos = 0;
char	*wptr = (char *)vptr;

	while (!get_next_str(&wptr, &len)) {
		int res;
		int sval;
		switch (flag) {
			case CMP_INT:
				sval = strtol(wptr, NULL, 0);
				res = (sval != ival);
				break;
			case CMP_NOCASE:
				res = strncasecmp(wptr, sptr, len);
				break;
			default:
				res = strncmp(wptr, sptr, len);
				break;
		}
		if (!res)
			return pos;
		/* Step to the next element of the set */
		wptr += len;
		/* Keep track of the relative position */
		pos++;
	}
	/* If we get here, the value wasn't found so we will print an error */
	motion_log(LOG_ERR, 0, "Validation error - should be one of '%s'", vptr);
	return -1;
}

/**
 * validate_range
 *
 * Compares the input value with a set of twp (space-separated) string values.
 *
 * Parameters:
 * 	vptr		Pointer to the set of valid values
 * 	sptr		Pointer to the string to be validated
 * 	ival		If doing integer compare, the integer value
 * 	flag		Flag to control type of comparison
 * 			CMP_NOCASE  ignore case
 * 			CMP_INT     compare integer values
 *
 * Returns:		0 if valid, -1 for error
 *
 */
static int validate_range(const char *vptr, char *sptr, int ival, int flag) {
char	*cptr1, *cptr2, *cp1, *cp2;
int	len1, len2;
int	val1, val2;
int	res;

	/* isolate the first value in the range */
	cp1 = (char *)vptr;
	get_next_str(&cp1, &len1);
	cp2 = cp1 + len1;

	cptr1 = mystrndup(cp1, len1);

	/* isolate the second value */
	get_next_str(&cp2, &len2);

	cptr2 = mystrndup(cp2, len2);

	switch (flag) {
		case CMP_INT:
			val1 = strtol(cptr1, NULL, 0);
			val2 = strtol(cptr2, NULL, 0);
			res = !((ival >= val1) && (ival <= val2));
			break;
		case CMP_NOCASE:
			if ((res = strcasecmp(cptr1, sptr)) < 0)
				break;
			if ((res = strcasecmp(cptr2, sptr)) > 0)
				break;
			res = 0;
			break;
		default:
			if ((res = strcmp(cptr1, sptr)) < 0)
				break;
			if ((res = strcmp(cptr2, sptr)) > 0)
				break;
			res = 0;
			break;
	}
	if (res) {
		motion_log(LOG_ERR, 0, "Validation error - should be in the "
		                "range of '%s' to '%s'", cptr1, cptr2);
		res = -1;
	}
	free(cptr1);
	free(cptr2);
	return res;
}

/*
 * Next are the validation routines
 */

/**
 * convert_boolean
 *
 * In order to allow boolean params to be defined as "on/off" or "yes/no"
 * or "true/false", this routine uses validate_set to check whether the
 * argument is within a set of two values.  If it is, it returns the
 * relative position (0 or 1).  If it isn't, it returns -1.
 *
 * Parameters:
 * 	val		pointer to a string containing the value
 * 	vset		pointer to the "validation set"
 *
 * Returns:
 * 	0 if val matches with the first item in vset, 1 if the second,
 * 	or -1 if any error.
 */
static int convert_boolean(const char *val, const char *vset) {
	int res;
	res = validate_set(vset, val, 0, CMP_NOCASE);
	if ((res < 0) || (res > 1) )
		return -1;
	else
		return res ^ 1;
}

 /**
 * validate_boolean
 *
 * Validates a boolean parameter declaration against a specified set
 *
 * Parameters:
 * 	pdef		pointer to the associated param definition
 * 	pptr		pointer to the parameter being validated
 *
 * Returns:		0 for success, -1 if validation failed
 * 
 */
static int validate_boolean(param_definition_ptr pdef, config_param_ptr pptr) {
	int res;
	switch (pdef->param_vtype) {
		case SET_VALIDATION:
			res = convert_boolean(pptr->str_value, pdef->param_values);
			if (res >= 0) {
				pptr->value.bool_val = res;
				return 0;
			} else
				return -1;
		default:
			motion_log(LOG_ERR, 0, "Unknown boolean validation requested");
			return -1;
	}
}

/**
 * validate_integer
 *
 * Validates an integer parameter declaration to assure that it contains
 * only values in it's specified value set.  The parameter's value is set appropriately.
 *
 * Parameters:
 * 	pdef		pointer to the associated param definition
 * 	pptr		pointer to the parameter being validated
 *
 * Returns:		0 for success, -1 if validation failed
 * 
 */
static int validate_integer(param_definition_ptr pdef, config_param_ptr pptr) {
long	lval;
char	*endptr;
int	res;

	/* First we validate that the value is a valid integer */
	lval = strtol(pptr->str_value, &endptr, 0);
	if ((*endptr != 0) || (lval < INT_MIN) || (lval > INT_MAX)) {
		motion_log(LOG_ERR, 0, "Invalid integer value '%s'",
		                pptr->str_value);
		return -1;
	}
	pptr->value.int_val = (int)lval;

	switch (pdef->param_vtype) {
		case NO_VALIDATION:
			return 0;
		case SET_VALIDATION:
			res = validate_set(pdef->param_values, NULL, pptr->value.int_val,
			                CMP_INT);
			if (res >= 0) {
				return 0;
			} else
				return -1;
		case RANGE_VALIDATION:
			res = validate_range(pdef->param_values, NULL, pptr->value.int_val,
			                CMP_INT);
			if (!res) {
				return 0;
			} else
				return -1;
		default:
			motion_log(LOG_ERR, 0, "Unknown integer validation requested");
			return -1;
	}
	return -1;
}

/**
 * validate_char
 *
 * Validates a char parameter declaration to assure that it contains
 * only values in it's specificied value set.  Also, if the param
 * is found to be valid, the parameter's value is set appropriately.
 *
 * Parameters:
 * 	pdef		pointer to the associated param definition
 * 	pptr		pointer to the parameter being validated
 *
 * Returns:		0 for success, -1 if validation failed
 * 
 */
static int validate_char(param_definition_ptr pdef, config_param_ptr pptr) {

	pptr->value.char_val = pptr->str_value[0];
	switch (pdef->param_vtype) {
		case NO_VALIDATION:
			return 0;
		case SET_VALIDATION:
			if (validate_set(pdef->param_values, pptr->str_value, 0, 0) >= 0) {
				return 0;
			}
			return -1;
		case RANGE_VALIDATION:
			return validate_range(pdef->param_values, pptr->str_value, 0, 0);
		default:
			motion_log(LOG_ERR, 0, "Unknown char validation requested");
			return -1;
	}
	return -1;
}

/**
 * validate_string
 *
 * Validates a string parameter declaration to assure that it contains
 * only values in it's specificied value set.  Also, if the param
 * is found to be valid, the parameter's value is set appropriately.
 *
 * Parameters:
 * 	pdef		pointer to the associated param definition
 * 	pptr		pointer to the parameter being validated
 *
 * Returns:		0 for success, -1 if validation failed
 * 
 */
static int validate_string(param_definition_ptr pdef, config_param_ptr pptr) {

	pptr->value.char_ptr = pptr->str_value;
	switch (pdef->param_vtype) {
		case NO_VALIDATION:
			return 0;
		case SET_VALIDATION:
			if (validate_set(pdef->param_values, pptr->str_value, 0, 0) >= 0) {
				return 0;
			}
			return -1;
		case RANGE_VALIDATION:
			return validate_range(pdef->param_values, pptr->str_value, 0, 0);
		default:
			motion_log(LOG_ERR, 0, "Unknown string validation requested");
			return -1;
	}
	return -1;
}

/**
 * validate_plugin
 *
 * Validates a plugin parameter and (if it is valid) attempts to load the requested
 * plugin.  This immediate loading is necessary, because the plugin may contain routines
 * to validate some of the following parameters.
 *
 * Parameters:
 * 	pdef		pointer to the associated param definition
 * 	pptr		pointer to the parameter being validated
 *
 * Returns:		0 for success, -1 if validation failed
 *
 */
static int validate_plugin(motion_ctxt_ptr cnt, config_param_ptr pptr) {
	char *name, *value = NULL;
	int ret = 0;
	motion_plugin_ptr plug_ptr;

	if (parse_name_value(pptr->str_value, strlen(pptr->str_value), &name, &value, 0))
		return -1;
	plug_ptr = plugins_loaded;
	while (plug_ptr) {
		if (!strcmp(plug_ptr->name, name)) {
			ret = 1;        /* not an error, just a flag */
			break;
		}
		plug_ptr = plug_ptr->next;
	}
	if (!ret) {	/* plugin not loaded previously */
		/* Check if the config entry specified a path, and if not set default */
		if (!value) {
			/* 
			 * sizeof(string) includes terminator, strlen doesn't,
			 * so this alloc includes 1 byte for the "/" and one byte
			 * for the string terminator (because of the 2 sizeof's).
			 * */
			value = mymalloc(sizeof(PLUGINDIR) + strlen(name) +
			                sizeof("_plugin.so"));
			memcpy(value, PLUGINDIR, sizeof(PLUGINDIR));
			strcat(value, "/");
			strcat(value, name);
			strcat(value, "_plugin.so");
		}
		if (!(plug_ptr = load_plugin(cnt, name, value))) {
			ret = -1;
		} else {	/* chain new plugin at beginning of chain */
			plug_ptr->name = name;
			name = NULL;
			plug_ptr->next = plugins_loaded;
			plugins_loaded = plug_ptr;
		}
	}

	if (name)
		free(name);
	if (value)
		free(value);
	return 0;
}

/* FIXME - no idea at the moment what needs to be done here */
/**
 * validate_sect
 *
 * Validates a section parameter declaration (to be continued ....)
 *
 * Parameters:
 * 	pdef		pointer to the associated param definition
 * 	pptr		pointer to the parameter being validated
 *
 * Returns:		0 for success, -1 if validation failed
 * 
 */
static int validate_sect(param_definition_ptr pdef, config_param_ptr pptr) {

	return 0;
}

/* FIXME - no idea at the moment what needs to be done here */
/**
 * validate_void
 *
 * Validates a void parameter declaration (to be continued ....)
 *
 * Parameters:
 * 	pdef		pointer to the associated param definition
 * 	pptr		pointer to the parameter being validated
 *
 * Returns:		0 for success, -1 if validation failed
 * 
 */
static int validate_void(param_definition_ptr pdef, config_param_ptr pptr) {

	return 0;
}

/*
 * There are two important structures which we will create and maintain -
 * "configuration contexts" and "params".  Here we definine routines
 * for destroying them, assuring all previously allocated memory is freed.
 * The config context is needed in order to delete the param from the
 * context's dictionary, and to remove it from the param chain.  Note
 * that a context pointer of NULL indicates that the param has not yet
 * been put into any context.
 */
static void destroy_param(config_ctxt_ptr conf, config_param_ptr ptr) {
	config_param_ptr wptr;
	
	if (debug_level > 4)
		motion_log(0, 0, "Destroying param '%s'", ptr->name);
	if (!ptr)
		return;
	/*
	 * I'm not quite sure what's the "right thing" to do here
	 * with respect to freeing value pointers.  At the moment,
	 * nothing other than the "str_value" of this param is being
	 * used, and that is freed later.  For now, I'll just free
	 * anything else which has been allocated.
	 */
	if (ptr->type ==STRING_PARAM) {
		if ((ptr->value.char_ptr != NULL) &&
		    (ptr->value.char_ptr != ptr->str_value))
			free(ptr->value.char_ptr);
	}
	if (ptr->type == VOID_PTR_PARAM) {
		if ((ptr->value.void_ptr) &&
		    (ptr->value.void_ptr != ptr->str_value))
			free(ptr->value.void_ptr);
	}
	if (conf != NULL) {
		/*
		 * We want to remove this param from the context's
		 * param chain, but there is a little trouble with
		 * the case of duplicates, which are (rarely)
		 * allowed.
		 * In this special case, there is a "duplicate" chain,
		 * with the dictionary entry pointing to the "base"
		 * of the chain.  This gives us two problems - first,
		 * we must update the chain; second, if the dictionary
		 * entry is pointing to this param, we must update
		 * the dictionary.
		 */
		wptr = dict_lookup(conf->dict, ptr->name, -1);
		/* if this is a duplicate, but not the first, it's simple */
		if (wptr != ptr) {
			while ((wptr->dupl != ptr) && (wptr->dupl != NULL)) {
				wptr = wptr->dupl;
			}
			/* We expect to have found a match */
			if (wptr->dupl == ptr) {
				/* remove this param from the chain */
				wptr->dupl = ptr->dupl;
			/* otherwise it's a program bug - log it! */
			} else {
				motion_log(LOG_ERR, 0, "Corrupt dupl chain!");
			}
		} else {
			/*
			 * The dictionary entry points to this param, so if
			 * there are duplicates we need to change the dictionary.
			 * In any event, we can delete the existing entry.
			 */
			if (dict_delete(conf->dict, ptr->name, -1)) {
				motion_log(LOG_ERR, 0, "Error updating dupl chain");
			} 
			if (ptr->dupl != NULL) {
				if (dict_add(conf->dict, ptr->name, -1, ptr->dupl)) {
					motion_log(LOG_ERR, 0, "Error updating dupl chain");
				}
			}
		}
	}
	if (ptr->name)
		free(ptr->name);
	if (ptr->str_value)
		free(ptr->str_value);
	free(ptr);
}

void destroy_config_ctxt(config_ctxt_ptr ptr) {
	config_param_ptr p1, p2;

	if (!ptr)
		return;
	if (ptr->next)
		destroy_config_ctxt(ptr->next);
	motion_log(0, 0, "Destroying context '%s' - '%s'", ptr->node_name, 
	                ptr->title ? ptr->title : "(none)");
	if (ptr->node_name)
		free(ptr->node_name);
	if (ptr->title)
		free(ptr->title);
	p1 = ptr->params;
	while (p1) {
		p2 = p1->next;
		destroy_param(ptr, p1);
		p1 = p2;
	}
	/* destroying the dictionary also cleans up any entries in it */
	if (ptr->dict)
		dict_destroy(ptr->dict);
	free(ptr);
}

/**
 * skip_nws
 *
 * Skip over all the "non-significant white space" and return a pointer to
 * the first significant character (which may be 0, or end of line)
 *
 * Parameters:
 *      cptr            pointer to the start of the string
 *
 * returns:
 *      Pointer to the first significant character
 */
static char *skip_nws(char *cptr) {
	while (*cptr && CHECK_NWS(cptr))
		cptr++;
	return cptr;
}

/**
 * parse_name_value
 *
 *      Parses the input line into a name and an optional value
 *
 * Parameters
 *
 *      line            pointer to the input line
 *      length          number of characters to look at
 *      param_name      address to put the name , it's allocated here.
 *      param_value     address to put the value, it's allocated here.
 *      comment_ok      flag to show comment allowed after
 *                      name and possible value
 *
 * Returns
 *      0 on success, -1 on any error
 *
 */
static int parse_name_value(char *line, int length, char **param_name,
                            char **param_value, int comment_ok) {
	char    *cptr, *value, wchar;
	int     namelen, linelen, ix;

	/* We can't use strpbrk, because we need to only look at 'length' */
	cptr = line;
	for (namelen=0; namelen<length; namelen++) {
		if (CHECK_NWS(cptr))
			break;
		cptr++;
	}
	if (namelen == 0)
		return -1;
	*param_name = mystrndup(line, namelen);        /* ok, we have the name */

	value = skip_nws(cptr);

	cptr = value;
	linelen = length - (cptr - line);            /* remaining length */

	/* Now we need to take care of quotes and 'single-quotes' */
	if (*cptr == '\'' || *cptr == '\"') {
		wchar = *cptr++;
		for (ix = 1; ix < linelen; ix++) {  /* find end quote */
			if (*cptr == '\\') {        /* handle 'escape' */
				cptr += 2;
				ix++;
				continue;
			}
			if (*cptr == wchar)
				break;
			cptr++;
		}
		if (ix == linelen)
			return -1;
		ix++;
		cptr++;
		
		/* assure the value doesn't include the delimiters */
		*param_value = mystrndup(value + 1, ix - 2);

		/* this check isn't required, but helps prove all ok */
		for (; ix < linelen; ix++) {
			if (comment_ok && (*cptr == ';' || *cptr == '#'))
				return 0;
			if (CHECK_NWS(cptr)) {
				motion_log(LOG_INFO, 0, "Junk at end of line");
				break;
			}
		}
		return 0;
	}
	/*
	 * Here we have the "normal" case.  value points to the beginning of
	 * the predicate, and we have to now try to determine where the end
	 * is.
	 * If comments are allowed, then we do a "foward search" from the start
	 * of the value until we find a ';' or '#', or until the end of the
	 * string has been reached and point to just before the comment.
	 * If comments are not allowed, then we point to the last char in the
	 * line.  In either event, we then go "backwards" until the first non-
	 * whitespace character is found.  That defines the length of the
	 * predicate.
	 */
	if (comment_ok) {
		for (ix = 0; ix < linelen; ix++) {
			if (*cptr == ';' || *cptr == '#')
				break;
			cptr++;
		}
		linelen = ix;
	} else {
		cptr = value + linelen;
	}
	for (ix = linelen; ix > 0; ix--) {
		wchar = *--cptr;
		if (!CHECK_NWS(cptr))
			break;
	}
	if (ix > 0){
		*param_value = mystrndup(value, ix);
	}
	return 0;
}

/**
 * parse_line
 *
 * Parses the input line from a configuration file to determine
 * whether it's a:
 *      Comment line
 *      Opening tag line
 *      Parameter line
 *          
 * No further information is supplied for a comment line.
 *      
 * For an opening tag line the tag name is returned in param_name and,
 * if there is further text associated with the tag it is returned in
 * param_val.
 *
 * For a parameter line, both param_name and param_val are returned.
 *
 * Parameters:
 *      line            pointer to the input line
 *      param_name      address to put the tag name or first param field
 *      param_val       address to put the value assigned to the param
 *
 * Returns:
 *      PARSE_COMMENT   line was a comment and can be ignored
 *      PARSE_START_TAG line was an opening tag
 *      PARSE_PARAM     line was a parameter
 *      PARSE_ERROR     line could not be parsed
 */
static parse_result parse_line(char *line, char **param_name, char **param_val) {
	char    *wline, *cptr;
	int	len;

	if (line == NULL)
		return PARSE_ERROR;

	/* Ignore any leading whitespace */
	wline = skip_nws(line);

	if (*wline == '#' || *wline == ';' || *wline == 0)
		return PARSE_COMMENT;

	if (*wline == OPEN_DELIM) {             /* found a start tag */
		/* get the length of the data inside the '[' and ']' */
		wline = skip_nws(wline + 1);
		if ((cptr = strchr(wline, CLOSE_DELIM)) == NULL)
			return PARSE_ERROR;
		len = cptr - wline;
		/* then process it just like a parameter */
		if (parse_name_value(wline, len, param_name, param_val, 0) ||
		    *param_name == NULL)
			return PARSE_ERROR;
		return PARSE_START_TAG;
	}

	/*
	 * Must be a parameter line, so just turn it over to our standard
	 * routine
	 */
	if (parse_name_value(wline, strlen(wline), param_name, param_val, 1)) {
		motion_log(LOG_ERR, 0, "Error parsing line {%s}", line);
		if (param_name)
			free(param_name);
		if (param_val)
			free(param_val);
		return PARSE_ERROR;
	}
	if (!param_val) {
		motion_log(LOG_ERR, 0, "No value on line {%s}", line);
		if (param_name)
			free(param_name);
		return PARSE_ERROR;
	}
	return PARSE_PARAM;
}

/**
 * conf_validate
 *
 * Validate a parameter description line from the configuration file.
 *
 * Parameters:
 * 	cnt		pointer to the motion context structure
 *	conf_ptr	pointer to the configuration context
 *	pptr		pointer to the parameter structure
 *
 * Returns:		Pointer to the param definition for success,
 * 			NULL for validation error.  Note that,
 * 			if validation is successful, this routine also saves
 * 			the parameter's value appropriately.
 *
 */
static param_definition_ptr conf_validate(motion_ctxt_ptr cnt, config_ctxt_ptr conf_ptr, config_param_ptr pptr) {
	int ix;
	int ret = 0;
	param_definition_ptr wptr;
	config_ctxt_ptr ccptr;
	/*
	 * First we must lookup the parameter name to determine what type of
	 * parameter it should be.  Once that is determined, we can try to
	 * validate it in accordance with its definition.
	 */
	if (!conf_ptr || !pptr) {
		motion_log(LOG_ERR, 0, "Invalid call to conf_validate");
		return NULL;
	}
	/* First we look through the plugin(s) validation */
	motion_plugin_ptr pv_ptr = plugins_loaded;
	wptr = NULL;
	while (pv_ptr) {	/* for each plugin loaded */
		/* if plugin has a param validation table */
		for (ix = 0; ix < pv_ptr->plugin_validate_numentries; ix++) {
			wptr = &pv_ptr->plugin_validate[ix];
			/* name NULL meands doc sect descriptor */
			if (!wptr->param_name)
				continue;
			if (!strcmp(pptr->name, wptr->param_name))
				goto found;
		}
		pv_ptr = pv_ptr->next;
	}

	/* If not found in the plugins, we next look in standard motion params */
	if (conf_ptr->global)
		ccptr = conf_ptr->global;
	else
		ccptr = conf_ptr;
	for  (ix = 0; ix < ccptr->num_valid_params; ix++ ) {
		wptr = &ccptr->param_valid[ix];
		if (!wptr->param_name)  /* name NULL means doc sect descriptor */
			continue;
		/* if this is not the global section, ignore global-only */
		if (conf_ptr->global) {
			if (wptr->param_flags & GLOBAL_ONLY_PARAM)
				continue;
		}
		if (!strcmp(pptr->name, wptr->param_name))
			goto found;
	}
	return NULL;

found:
	/*
	 * We have found an entry for the current parameter name.  Now
	 * we must validate the param.
	 */
	switch (wptr->param_type) {
		case BOOLEAN_PARAM:
			ret = validate_boolean(wptr, pptr);
			break;
		case INTEGER_PARAM:
			ret = validate_integer(wptr, pptr);
			break;
		case CHAR_PARAM:
			ret = validate_char(wptr, pptr);
			break;
		case STRING_PARAM:
			ret = validate_string(wptr, pptr);
			break;
		case PLUGIN_PARAM:
			ret = validate_string(wptr, pptr);
			if (ret)
				break;
			ret = validate_plugin(cnt, pptr);
			break;
#if 0
		case SECT_PTR_PARAM:
			ret = validate_sect(wptr, pptr);
			break;
		case VOID_PTR_PARAM:
			ret = validate_void(wptr, pptr);
			break;
#endif
		case UNK_PARAM:
			/* No validation possible */
			motion_log(LOG_ERR, 0, "No param type for %s", pptr->name);
			break;
	}
	if (!ret)
		return wptr;
	return NULL;
}

#if 0
static void proc_plugin(config_ctxt_ptr conf, char *name, char *value) {

	if (!strcmp(name, "input_plugin")) {
		motion_log(-1, 0, "found input_plugin");
	} else if (!strcmp(name, "movie_plugin")) {
		motion_log(-1, 0, "found movie_plugin");
	} else {
		motion_log(-1, 0, "Unrecognized param '%s %s'", name, value);
	}
	free(name);
	free(value);
}
#endif

/**
 * new_config_section
 * 
 * Process all of the configuration lines within a section, producing
 * a new structure with the result.  The structure is filled in with
 * a pointer to the parent, and all of the parameters which have been
 * defined within the section.
 *
 * Parameters:
 *
 * 	cnt		pointer to motion context
 *      chain           pointer to the chain of config files being processed
 *      conf            pointer to the current config file (NULL means global)
 *      node_name       node name for the newly-created structure.
 *                      If a subsidiary section is found, it's
 *                      name is returned in this parameter.
 *      node_title      If a subsidiary section has a qualifying
 *                      predicate, it's value is returned in this
 *                      parameter.
 *      retval          address to put any return code.
 *
 * Returns:
 *
 *      Pointer to the newly-created structure, or NULL if any error.
 *      The parameter 'retval' is also filled in with parse_result code.
 */
static config_ctxt_ptr new_config_section(motion_ctxt_ptr cnt, cfg_file_ptr *chain,
                                          config_ctxt_ptr conf,
                                          char **node_name,
                                          char **node_title,
                                          parse_result *retval) {
	config_ctxt_ptr new_config;
	char	buff[INTERNAL_BUFF_SIZE];
	char	*name, *value, *cptr;        
	int	namelen;
	int	has_error = 0;
	parse_result ret;
	config_param_ptr new_param, ppos;
	param_definition_ptr pptr;

	if (debug_level) {
		if (*node_title != NULL)
			motion_log(LOG_INFO, 0, "Creating section %s {%s}", *node_name,
				*node_title);
		else
			motion_log(LOG_INFO, 0, "Creating section %s", *node_name);
	}

	new_config = mymalloc(sizeof(config_ctxt));
	memset(new_config, 0, sizeof(config_ctxt));
	/*
	 * We copy the name and title into the new structure.  This means
	 * that we "take over" ownership of those strings, which must be
	 * freed later when this structure is destroyed.
	 */
	new_config->node_name = *node_name;
	*node_name = NULL;
	new_config->title = *node_title;
	*node_title = NULL;

	if (!conf) {             /* if this is the global config section being created */
		new_config->num_valid_params = sizeof(global_params) / sizeof(param_definition);
		new_config->param_valid = global_params;
	} else {
		new_config->global = conf;
	}
	new_config->dict = dict_create();

	while (1) {
		buff[sizeof(buff)-1] = 0xff;
		cptr = fgets(buff, sizeof(buff), (*chain)->file);
		if (cptr == NULL) {
			if (ferror((*chain)->file)) {
				motion_log(LOG_ERR, 1, "Reading config file");
				*retval = PARSE_ERROR;
				return new_config;
			}
			/*
			 * We have an EOF on our current file.  If it's
			 * an include file, we want to revert to the
			 * previous one (where the include directive
			 * was found)
			 */
			fclose((*chain)->file);
			if ((*chain)->next != NULL) {
				cfg_file_ptr ptr = (*chain)->next;
				free(*chain);
				*chain = ptr;
				continue;
			} else
				(*chain)->file = NULL;

			*retval = PARSE_EOF;
			return new_config;
		}
		if (buff[sizeof(buff)-1] == 0) {
			motion_log(LOG_ERR, 0, "Internal buffer overflow - config line starting with\n%s",
			                buff);
			*retval = PARSE_ERROR;
			return new_config;
		}

		/*
		 * Parse the next line of the config file. Note that 'name' and 'value' are
		 * allocated by this routine and must later be freed.  If they are set into a
		 * new parameter structure, this will occur when that structure is destroyed.
		 * In other cases (e.g. on an error) this must be done explicitly.
		 */
		ret = parse_line(buff, &name, &value);
		switch (ret) {
			/* Ignore any comment lines */
			case PARSE_COMMENT:
				break;

			/* If this is a start tag, it's the end of section */
			case PARSE_START_TAG:
				*node_name = name;
				*node_title = value;
				*retval = PARSE_START_TAG;
				return new_config;

			/*
			 * Here's where most of the work is done.  We know that
			 * we have a parameter, and our current context.  First
			 * we check for "special" parameters such as "include".
			 */
			case PARSE_PARAM:
				/*
				 * First we check whether this is an 'include'
				 * param.  If so, we try to open the include
				 * file and carry on.  Any error on the open
				 * is fatal.
				 */
				namelen = strlen(name);
				if (!strncmp(name, "include", namelen)) {
					cfg_file_ptr new_cfg_file;
					FILE *newfile;

					free(name);
					name = NULL;
					if ((newfile = fopen(value, "r")) == NULL) {
						motion_log(LOG_ERR, 1, "Opening "
						           "include file '%s'",
						           value);
						*retval = PARSE_ERROR;
						free(value);
						return NULL;
					}
					new_cfg_file = mymalloc(sizeof(cfg_file));
					new_cfg_file->next = *chain;
					new_cfg_file->file = newfile;
					(*chain) = new_cfg_file;
					free(value);
					value = NULL;
					continue;
				}
			 	/* We now know that we have a "general"
			         * parameter.  What we don't yet know is what
			         * type of value the param should contain, and
			         * whether the specified value is acceptable
			         * (i.e. we need to do some validation).  First
			         * we save the parameter details.
			         */
				new_param = mymalloc(sizeof(config_param));
				memset(new_param, 0, sizeof(config_param));
				new_param->name = name;
				new_param->str_value = value;
				new_param->type = UNK_PARAM;

				pptr = conf_validate(cnt, new_config, new_param);

				if (pptr == NULL) {
					motion_log(LOG_ERR, 0, "Validation failed for param '%s' with "
					                       "value '%s'", name, value);
					has_error = 1;
					break;
				} else {
#if 0
					config_param_ptr ppos, p1;  /* ppos will be insertion point */
					ppos = (config_param_ptr)&new_config->params;
					p1 = ppos->next;
					while (p1) {
						order = strcmp(p1->name, new_param->name);
						if ((order == 0) &&
						    !(pptr->param_flags & DUPS_OK_PARAM)) {
							motion_log(LOG_ERR, 0,
							           "Duplicate '%s' "
							           "in sect %s", new_param->name,
							           *node_name ? *node_name : "global");
							has_error = 1;
							break;
						}
						if (order > 0)
							break;
						ppos = p1;
						p1 = p1->next;
					}
					if (has_error)
						break;
					/* Now need to link this new param onto existing chain */
					new_param->next = ppos->next;
					ppos->next = new_param;
#endif
					/* First we check if the name is already defined */
					ppos = (config_param_ptr)dict_lookup(new_config->dict,
					        name, namelen);
					/* If it is, check whether duplicates are allowed */
					if (ppos != NULL) {
						/* If not, this is an error */
						if (!(pptr->param_flags & DUPS_OK_PARAM)) {
							motion_log(LOG_ERR, 0,
							           "Duplicate '%s' "
							           "in sect %s", new_param->name,
							           *node_name ? *node_name : "global");
							has_error = 1;
							break;
						}
						/* Dups allowed - chain this onto duplicate chain */
						while (ppos->dupl != NULL)
							ppos = ppos->dupl;
						ppos->dupl = new_param;
					/* If this param is not already present, add it to the dictionary */
					} else {
						if (dict_add(new_config->dict, name, namelen, new_param)) {
							motion_log(LOG_ERR, 0,
							           "Error looking up param name");
							has_error = 1;
							break;
						}
					}
				}
				/*
				 * The param has been validated, and is guaranteed to be in the
				 * dictionary.  We can now add it to our parameter chain, which
				 * is maintained in "occurrence" sequence.
				 */
				ppos = (config_param_ptr)&(new_config->params);
				while (ppos->next)
					ppos = ppos->next;
				ppos->next = new_param;
				
				if (debug_level > 5)
					motion_log(-1, 0, "param {%s} value {%s}",
						new_param->name,
						new_param->str_value ? new_param->str_value : "");
				break;
			default:
				motion_log(LOG_ERR, 0, "Error parsing config file");
				has_error = -1;		/* fatal error */
				break;
		}
		if (has_error) {
			/* FIXME - need to set some global flag? */
			destroy_param(NULL, new_param);
			if (has_error < 0) {	/* fatal error */
				return NULL;
			}
			has_error = 0;
		}
	}

}

/**
 * create_config_chain
 *
 * Called by the startup code in order to start the
 * parsing of the configuration file.
 *
 * Parameters:
 *
 *      cnt             pointer to the motion context
 *      filename        pathname of the configuration file
 *
 * Returns:
 *      0 on success, -1 on error
 */
static config_ctxt_ptr create_config_chain(motion_ctxt_ptr cnt, cfg_file_ptr cur_file) {
	char    *cptr, *name=NULL, *value=NULL;
	char    buff[INTERNAL_BUFF_SIZE];
	config_ctxt_ptr conf, cur_sect;
	parse_result    ret;

	do {
		buff[sizeof(buff)-1] = 0xff;
		cptr = fgets(buff, sizeof(buff), cur_file->file);
		if (buff[sizeof(buff)-1] == 0) {
			buff[sizeof(buff)-1] = 0;
			motion_log(LOG_ERR, 0, "Internal buffer overflow - config line starting with\n%s",
			                buff);
			fclose(cur_file->file);
			free(cur_file);
			return NULL;
		}
		if ((ret = parse_line(buff, &name, &value)) == PARSE_ERROR) {
			motion_log(-1, 0, "Error parsing config file");
			fclose(cur_file->file);
			free(cur_file);
			return NULL;
		}
	} while (ret == PARSE_COMMENT);

	if (ret != PARSE_START_TAG) {
		motion_log(LOG_ERR, 0, "Config file format error");
		fclose(cur_file->file);
		free(cur_file);
		return NULL;
	}
	if ((strcmp(name, "global")) || value != NULL) {
		motion_log(LOG_ERR, 0, "Config file must start with '[global]'");
		motion_log(LOG_ERR, 0, "Fix and re-start");
		fclose(cur_file->file);
		free(cur_file);
		return NULL;
	}

	/* Create the [global] section */
	if ((conf = new_config_section(cnt, &cur_file, NULL, &name, &value, &ret)) == NULL) {
		motion_log(LOG_ERR, 0, "Error creating new config section");
		fclose(cur_file->file);
		free(cur_file);
		return NULL;
	}

	/* Create any other sections which have been declared */
	cur_sect = conf;
	while (cur_file->file && !feof(cur_file->file)) {
		if ((cur_sect->next = new_config_section(cnt, &cur_file, conf, &name,
		     &value, &ret)) == NULL) {
			motion_log(LOG_ERR, 0, "Error creating config section");
			fclose(cur_file->file);
			free(cur_file);
			return NULL;
		}
		cur_sect = cur_sect->next;
	}
	free(cur_file);
	return conf;
}

/**
 * val_set
 *
 * Set the value of a configuration parameter within it's appropriate config structure.
 *
 * Parameters:
 * 	cnt		pointer to the motion context
 * 	pv		pointer to the validation table entry
 * 	p		pointer to the parameter details
 * 	config_array	pointer to the config structure
 * 	default_flag	if set, the routine will use the "default" value
 * 			in the validation table instead of the param.
 *
 * Returns:		0 for success, -1 for any error
 *
 */
static int val_set(motion_ctxt_ptr cnt, param_definition_ptr pv, config_param_ptr p, void *config_array, int default_flag) {

	char *cptr;
	int *iptr;
	char **sptr;

	if (!(pv->param_flags & MOTION_CONTEXT_PARAM)) {
		switch (pv->param_type) {
			case BOOLEAN_PARAM:
				cptr = (char *)((char *)config_array + pv->param_voffset);
				if (!default_flag) {
					*cptr = p->value.bool_val;
				} else {
					if (pv->param_default)
						*cptr = convert_boolean(pv->param_default,
							pv->param_values);
				}
				break;
			case INTEGER_PARAM:
				iptr = (int *)((char *)config_array + pv->param_voffset);
				if (!default_flag)
					*iptr = p->value.int_val;
				else {
					if (pv->param_default)
						*iptr = strtol(pv->param_default, NULL, 0);
				}
				break;
			case CHAR_PARAM:
				cptr = (char *)((char *)config_array + pv->param_voffset);
				if (!default_flag)
					*cptr = p->value.char_val;
				else {
					if (pv->param_default)
						*cptr = pv->param_default[0];
				}
				break;
			case STRING_PARAM:
				sptr = (char **)((char *)config_array + pv->param_voffset);
				if (!default_flag)
					*sptr = p->value.char_ptr;
				else {
					if (pv->param_default)
						*sptr = (char *)pv->param_default;
				}
				break;
			case PLUGIN_PARAM:
				break;
			default:
				motion_log(LOG_ERR, 0, "Trouble setting value for '%s'",pv->param_name);
				return -1;
		}
	} else {	/* this is a xxxxx_plugin param, so need to setup motion context structure */
		int len;
		motion_plugin_ptr plug_ptr;

		plug_ptr = plugins_loaded;
		while (plug_ptr) {
			if (!strcmp(plug_ptr->plugin->name, p->str_value))
				break;
			plug_ptr = plug_ptr->next;
		}

		if (!plug_ptr) {
			motion_log(LOG_ERR, 0, "Processing %s: plugin '%s' not loaded",
			                pv->param_name, p->str_value);
			return -1;
		}
		if (!(cptr = strchr(pv->param_name, '_'))) {
			motion_log(LOG_ERR, 0, "Invalid param name '%s'", pv->param_name);
			return -1;
		}
		len = cptr - pv->param_name;
		/* We need to split on type, but I can't think of anything better than successive 'if's */
		if (!strncmp(pv->param_name, "input", len)) {
			if (cnt->video_ctxt) {
				motion_log(LOG_ERR, 0, "Processing '%s %s': video_context already set",
				                pv->param_name, p->str_value);
				return -1;
			}
			cnt->video_ctxt = plug_ptr->plugin->motion_plugin_control(MOTION_VIDEO_INPUT);
			if (!cnt->video_ctxt) {
				motion_log(LOG_ERR, 0, "Processing '%s %s': "
				                       "Error calling plugin control routine",
				                       pv->param_name, p->str_value);
				return -1;
			}
			cnt->motion_video_plugin = plug_ptr;
		} else  if (!strncmp(pv->param_name, "movie", len)) {
			motion_log(LOG_WARNING, 0, "'movie_plugin' not yet implemented");
		}

	}
	return 0;
}

/**
 * set_config_values
 *
 * When validation of the configuration file is complete, this routine is
 * called to actually set the config values into the applicable motion
 * variables.
 *
 * Parameters
 *
 * 	cnt		Pointer to motion context
 * 	conf		Pointer to a configuration context
 * 	config_vars	Pointer to the config structure
 *
 * Returns		0 for success, -1 if any error.
 */
int set_config_values(motion_ctxt_ptr cnt, config_ctxt_ptr conf, void *config_vars) {
	int ix;
	int res;
	config_param_ptr p;
	param_definition_ptr pv;
	config_ctxt_ptr gparams;

	if (conf->global)
		gparams = conf->global;
	else
		gparams = conf;
	/*
	 * We use the parameter definition(s) to "drive" our actions.
	 * This is because many of the variables might require being
	 * set to the "default" value if there was no explicit entry
	 * in the user's configuration file.
	 */
	for (ix = 0; ix < gparams->num_valid_params; ix++) {
		pv = &gparams->param_valid[ix];
		if (!pv->param_name)	/* Ignore any block titles */
			continue;
		/* Look through the params present in the config file */
		p = conf->params;
		res = -1;
		while (p) {
			/* they are held in ascending order */
			res = strcmp(p->name, pv->param_name);
			if (res >= 0)
				break;
			p = p->next;
		}
		/* if res != 0 we set default value for globals */
		if (!res || ((gparams == conf) && pv->param_default))
			val_set(cnt, pv, p, config_vars, res);
	}
	return 0;
}
/**
 * set_ext_values
 *
 * This routine is called from the Motion mainline code when a new context is being
 * created.  It sets the appropriate values in the referenced confg structure.
 *
 * Parameters:
 * 	cnt		Pointer to motion context
 * 	conf		Pointer to the configuration context
 * 	pptr		Pointer to the chain of params
 * 	numvalues	Number of items in the validation table
 * 	config_vars	Pointer to the structure holding the config values
 *
 * Returns:		0 for success, -1 if any error
 * 
 */
int set_ext_values(motion_ctxt_ptr cnt, config_ctxt_ptr conf, param_definition_ptr pptr,
                int numvalues, void *config_vars) {
	int ix;
	config_param_ptr p;

	if (!config_vars) {
		motion_log(LOG_ERR, 0, "Null pointer for values in set_ext_values");
		return -1;
	}
	/*
	 * We use the parameter definition(s) to "drive" our actions.
	 * This is because many of the variables might require being
	 * set to the "default" value if there was no explicit entry
	 * in the user's configuration file.
	 */
	for (ix = 0; ix < numvalues; ix++) {
		if (!pptr[ix].param_name) /* Ignore any block titles */
			continue;
		/* Look through the params present in the config file */
#if 0
		p = conf->params;
		res = -1;
		while (p) {
			/* they are held in ascending order */
			res = strcmp(p->name, pptr[ix].param_name);
			if (res >= 0)
				break;
			p = p->next;
		}
#endif
		p = dict_lookup(conf->dict, pptr[ix].param_name, -1);
		val_set(cnt, &pptr[ix], p, config_vars, (p == NULL));
	}
	return 0;
}

/**
 * dump_config_file
 * 
 * Routine to write out the current contents of a configuration context,
 * utilising the information present in the validation sub-structure.
 *
 * Parameters:
 *
 *      outfile         pointer to the streaming file descriptor cptr1, pv->param_values, slenr
 *      conf            pointer to the configuration context
 *
 * Returns:
 *      Data written to the specified file
 */
void dump_config_file(FILE *outfile, config_ctxt_ptr conf) {
	int ix;
	static char sect_separator[]   = "###################################"
	                                 "###################################";
	static char sect_cont_pref[]   = "#    ";
	static char sect_cont_end[]    = "                                   "
	                                 "                                  #";
#define sect_separator_len (sizeof(sect_separator) - 1)
#define sect_cont_pref_len (sizeof(sect_cont_pref) - 1)
	static char param_descr_pref[] = "# ";
	const char *sptr, *cptr;
	char *cptr1;
	unsigned int slen;
	char wline[129];
	config_param_ptr   p;
	param_definition_ptr  pv;
	char has_value, first;
	int fill_length;

	if (!conf || !outfile) {
		motion_log(LOG_ERR, 0, "Invalid config_ctxt_ptr in dump_config_file");
		return;
	}

	if (!conf->title) {
		fprintf(outfile, "[%s]\n", conf->node_name);
	} else {
		fprintf(outfile, "[%s %s]\n", conf->node_name, conf->title);
	}

	for (ix=0; ix<conf->num_valid_params; ix++) {
		pv = (param_definition_ptr)&conf->param_valid[ix];
		/*
		 * A little bit of extra work here to produce some nice
		 * formatting. If there's a section title, we want to put it
		 * inside a "title box" which starts and ends with a row of
		 * "###..#".  Within the box, each line starts with "#   "
		 * and ends with a nicely-aligned "#"
		 */
		sptr = pv->param_descr;
		if (!pv->param_name) {		/* Section title */
			/* Start with a line of "###..#" */
			fprintf(outfile, "\n%s\n", sect_separator);
			/* Now, provided each line is not too long,
			 * make the box
			 */
			first = 1;
			do {
				/* calculate the length of the line */
				cptr = strchr(sptr, '\n');
				/* put the length in 'slen' */
				if (cptr)	/* if more lines follow */
					slen = cptr++ - sptr;
				else		/* if last line */
					slen = strlen(sptr);
				/* make sure line + prefix will fit in buffer */
				if (slen > sizeof(wline) - sect_cont_pref_len)
					slen = sizeof(wline) - sect_cont_pref_len;
				/* start the line with a 'prefix' ('# ') */
				strcpy(wline, sect_cont_pref);
				
				if (first) {    /* we want to "center" the first line */
					first = 0;
					/*
					 * calculate required fill based upon the size of
					 * the section separator and 2*prefix
					 */
					fill_length = ((sect_separator_len - slen) / 2) -
					               sect_cont_pref_len;
					if (fill_length > 0) {
						/* new string terminator at end */
						wline[sect_cont_pref_len + fill_length] = 0;
						/* then add in required spaces */
						while (fill_length > 0)
							wline[sect_cont_pref_len + --fill_length] = ' ';
					}
				}
				strncat(wline, sptr, slen);
				/* if description line < separator, append a "...#" */
				if (strlen(wline) < strlen(sect_separator))
					strcat(wline, &sect_cont_end[strlen(wline)]);
				fprintf(outfile, "%s\n", wline);
				sptr = cptr;
			} while (cptr);
			fprintf(outfile, "%s\n\n", sect_separator);
			continue;
		}
		/*
		 * For the parameter description, we want to output it as multiple
		 * lines "#" comments.  We allow a blank description, but it's really
		 * not a good practice.
		 */
		if (sptr) {
			do {
				cptr = strchr(sptr, '\n');
				strcpy(wline, param_descr_pref);
				if (cptr)
					slen = cptr++ - sptr;
				else
					slen = strlen(sptr);
				if (slen > sizeof(wline) - sizeof(param_descr_pref) - 1)
					slen = sizeof(wline) - sizeof(param_descr_pref) - 1;
				strncat(wline, sptr, slen);
				fputs(wline, outfile);
				if (cptr)
					fputc('\n', outfile);
				sptr = cptr;
			} while (cptr);
			fprintf(outfile, " (default: %s)\n",
			        pv->param_default? pv->param_default: "none");
			if (pv->param_values) {
				switch (pv->param_vtype) {
					case SET_VALIDATION:
						fprintf(outfile, "%sAllowed values: %s\n",
						        param_descr_pref, pv->param_values);
						break;
					case RANGE_VALIDATION:
						cptr = strchr(pv->param_values,' ');
						if (cptr) {
							/* lenght of start value + string term */	
							slen = ++cptr - pv->param_values;
							cptr1 = mystrndup((char *)pv->param_values, slen);
							fprintf(outfile, "%sAllowed values: %s - %s\n",
							        param_descr_pref,
								cptr1, cptr);
							free(cptr1);
						}
						break;
					case SPECIAL_VALIDATION:
					case NO_VALIDATION:
						break;
				}
			}
		}
		/*
		 * Now we go through the configuration context parameters to see if this
		 * one has been declared with a value.  If so, we print the current value.
		 * If it's not present, then we print a comment (with a ';') line to
		 * specify it's default value.
		 */
#if 0
		p = conf->params;
		has_value = 0;
		while (p) {
			int res;
			res = strcmp(p->name, pv->param_name);
			if (res < 0) {
				p = p->next;
				continue;
			}
			if (res > 0)
				break;
			has_value = 1;
			do { 
				fprintf(outfile, "%s  %s\n", pv->param_name, p->str_value);
				p = p->next;
			} while ((p != NULL) && !strcmp(pv->param_name, p->name));
			break;
		}
#endif
		p = dict_lookup(conf->dict, pv->param_name, -1);
		if (p != NULL) {
			while (p != NULL) {
				fprintf(outfile, "%s  %s\n", pv->param_name, p->str_value);
				p = p->dupl;
			}
			fputc('\n', outfile);
		} else {
			if (pv->param_default) {
				fprintf(outfile, ";  %s  %s\n\n", pv->param_name,
					pv->param_default);
			} else {
				fprintf(outfile, "#  %s\n\n", pv->param_name);
			}
		}
	}
}

/**
 * usage
 *
 * This routine is called when a "-h" (or any unrecognized flag) is present on the
 * Motion command line.  It prints out the allowed flags.
 *
 */
static void usage (void)
{
	printf("motion Version " MOTION_VERSION_STRING "\n"
	       "Copyright 2000-2006 Jeroen Vreeken/Folkert van Heusden/"
	       "Kenneth Lavrsen\n"
	       "\nusage:\tmotion [options]\n\n\n"
	       "Possible options:\n\n"
	       "-n\t\tRun in non-daemon mode.\n"
	       "-s\t\tRun in setup mode.\n"
	       "-c config\tFull path and filename of config file.\n"
	       "-d level\tDebug mode.\n"
	       "-z filename\tDump config file to {filename} after processing.\n"
	       "-h\t\tShow this screen.\n\n"
	       "Motion is configured using a config file only. If none is supplied,\n"
	       "it will read motion.conf from current directory, ~/.motion or "
	       SYSCONFDIR ".\n\n"); 
}

/**
 * conf_cmdline
 * 
 * Sets the conf struct options as defined by the command line.
 * Any option already set is overwritten.
 *
 * Parameters
 * 	cnt		Pointer to the context struct
 * 	thread		thread number of the calling thread
 * 			(-1 indicates called from main)
 *
 * Returns		updated context structure
 *
 */
static void conf_cmdline (motion_ctxt_ptr cnt, int thread)
{
	config_ptr conf=&cnt->conf;
	int c;

	while ((c=getopt(conf->argc, conf->argv, "c:d:z:hns?"))!=EOF)
		switch (c) {
			case 'c':
				if (thread==-1) strcpy(cnt->conf_filename, optarg);
				break;
			case 'n':
				cnt->daemon=0;
				break;
			case 's':
				conf->setup_mode=1;
				break;
			case 'd':
				/* no validation - just take what user gives */
				debug_level = atoi(optarg);
				break;
			case 'z':
				/* Just for testing config processing */
				dump_config_filename = strdup(optarg);
				break;
			case 'h':
			case '?':
			default:
				usage();
				exit(1);
		}
	optind=1;
}

/**
 * conf_load
 *
 * This routine processes the Motion configuration file and creates
 * the configuration value array.
 *
 * Parameters
 * 	cnt		The configuration context pointer
 *
 * Returns		0 if successful, -1 if any error
 *
 */
int conf_load (motion_ctxt_ptr cnt) {
	int phase = 0;
	cfg_file_ptr cur_file;
	config_ctxt_ptr sect_chain_ptr;
	motion_ctxt_ptr wcnt;
	FILE *outfile;		/* for possible dumping */

	conf_cmdline(cnt, -1);	/* parse the motion command line */
	cur_file = mymalloc(sizeof(cfg_file));
	memset(cur_file, 0, sizeof(cfg_file));

	while(!cur_file->file) {
		switch (phase) {
			case 0:
				break;
			case 1:
				sprintf(cnt->conf_filename, "motion.conf");
				break;
			case 2:
				sprintf(cnt->conf_filename, "%s/.motion/motion.conf",
				                getenv("HOME"));
				break;
			case 3:
				strcpy(cnt->conf_filename, SYSCONFDIR "/motion.conf");
				break;
			default:
				motion_log(-1, 1, "Could not open config file %s",
				                cnt->conf_filename);
				return -1;
		}
		cur_file->file = fopen(cnt->conf_filename, "r");
		phase++;
	}
	/* Config file is opened and ready to process */
	if ((gconf_ctxt = create_config_chain(cnt, cur_file)) == NULL) {
		motion_log(LOG_ERR, 0, "Config file parsing error");
		return -1;
	}
	if (set_config_values(cnt, gconf_ctxt, &cnt->conf) < 0) {
		motion_log(LOG_ERR, 0, "Error setting config file values");
		return -1;
	}

	/*
	 * In order to test the new configuration routines, there is a runtime
	 * argument flag (-z) which can be set to contain a filename for dumping
	 * the result after processing.
	 */
	if (dump_config_filename) {
		if ((outfile = fopen(dump_config_filename, "w")) == NULL) {
			motion_log(LOG_ERR, 1, "Trying to open config dump file");
			return -1;
		}
		sect_chain_ptr = gconf_ctxt;
		while (sect_chain_ptr) {
			dump_config_file(outfile, sect_chain_ptr);
			sect_chain_ptr = sect_chain_ptr->next;
		}
		fclose(outfile);
		free(dump_config_filename);
	}
	return 0;
}
#if 0
/********************************************************************************
 * Following dummy routines previously in conf.c, need to be either implemented *
 * in this routine, or else modify other routines calling them.                 *
 *   (note - currently in transitional header file "fudge.h")                   *
 ********************************************************************************/

#include "fudge.h"

/* referenced by motion.c and webhttpd.c */
struct context **conf_cmdparse(struct context **cnt, const char *v1, const char *v2) {
	motion_log(0, 0, "fudge/conf_cmdparse called");
	return NULL;
}

/* referenced only by webhttpd.c */
config_param config_params[1];

const char *config_type(config_param *cfg) {
	motion_log(0, 0, "fudge/config_type called");
	return NULL;
}

void conf_print(struct context **cnt) {
	motion_log(0, 0, "fudge/conf_print called");
}
#endif

