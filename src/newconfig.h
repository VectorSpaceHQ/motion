#ifndef	NEWCONFIG_H
#define	NEWCONFIG_H

#include <stddef.h>
#include <stdio.h>

/* some #defines for our tags */
#define	OPEN_DELIM      '['
#define	CLOSE_DELIM     ']'
#define	NWS             " \t\n\r"   /* Non-significant white space */
#define	CHECK_NWS(ptr) \
	(*(ptr) == ' ' || *(ptr) == '\t' || *(ptr) == '\n' || *(ptr) == '\r')

/*
 * Some #defines for parameter validation tables.  The first value is given a
 * binary value of 0, and the second a value of 1.
 */
#define	ON_OFF "on off"
#define	TRUE_FALSE "true false"
#define	YES_NO "yes no"

/* flags for the valid_set routine */
#define	CMP_NOCASE (1 << 0)
#define CMP_INT    (1 << 1)

/*
 * These need to be declared here to use in following structures.  The full
 * structure definition will appear later in this file.
 */
typedef struct _cfg_valid_struct *cfg_valid_ptr;

/**
 * enum parse_result
 *
 *      The set of possible return codes from the parse_line
 *      routine.
 *
 */
typedef enum {
	PARSE_ERROR = -1,
	PARSE_COMMENT,
	PARSE_START_TAG,
	PARSE_PARAM,
	PARSE_EOF
} parse_result;

/**
 * enum param_type
 *
 *      Here we define the different possible types for a parameter value
 */
typedef enum {
	BOOLEAN_PARAM,
	INTEGER_PARAM,
	CHAR_PARAM,
	STRING_PARAM,
	PLUGIN_PARAM,
	SECT_PTR_PARAM,
	VOID_PTR_PARAM,
	UNK_PARAM
} param_type_def;

/**
 * enum param_vtype
 *
 * Here we define the different types of validation possible
 */
typedef enum {
	NO_VALIDATION = 0,
	RANGE_VALIDATION,
	SET_VALIDATION,
	SPECIAL_VALIDATION,
} param_vtype_def;

/**
 * struct _config_file_param
 *
 *      The configuration context structure contains a chain of parameters which
 *      have been defined for the current level.  The chain is maintained in
 *      strict alphabetical order. Each element of the chain contains the
 *      details of one parameter.
 */
typedef struct _config_param{
	struct _config_param    *next;             /* pointer to next in chain */
	char                    *name;             /* parameter name */
	char                    *str_value;        /* string value of the param */
	param_type_def          type;              /* parameter type */
	union {                                    /* converted parameter value */
		unsigned char   bool_val;         /* boolean */
		int             int_val;          /* integer */
		char            char_val;         /* char */
		char            *char_ptr;        /* string */
		void            *void_ptr;        /* void */
	} value;
} config_param, *config_param_ptr;


/*
 * The global code contains a set of parameters which it will recognize, and
 * be able to validate.  Each plugin may also recognize additional parameters.
 * In that case, they must also be able to be validated.  In order to
 * accomplish this, we need a structure to describe a set of parameters.
 * Within the code, we will produce an array of these structures (one within
 * the "main" code, and one within each plugin which has unique parameters).
 * The order of entries the entries within that array will also control how the
 * system will "automatically" dump the parameters if requested by the user.
 *
 * For nicer formatting of the configuration file, we also allow any number
 * of entries with a param_name of NULL, containing a description field which
 * will be put into any output file which is dumped.
 * 
 */
typedef struct {
	const char             *param_name;        /* text name for the param 
	                                              NULL for a group header*/
	const char             *param_descr;       /* text title for this param */
	param_type_def         param_type;         /* type of this param.  This
	                                              is used when storing the
	                                              param, as well as when
	                                              validating it. */
	const char             *param_default;     /* default value */
	param_vtype_def         param_vtype;       /* 0 => set, 1 => range */
	const char             *param_values;      /* pointer to set of values */
	param_type_def         (*param_validate)(char *, config_param_ptr); 
	                                           /* pointer to
	                                              a special validation routine
	                                              (if not the default) */
	unsigned int           param_flags;
	int                    param_voffset;      /* offset within configuration
	                                              variables structure */
	                                           
} param_definition, *param_definition_ptr;

/**
 * bit values for param_flags
 */
#define GLOBAL_ONLY_PARAM     (1 << 0)              /* only for global */
#define DUPS_OK_PARAM         (1 << 1)              /* can have duplicate */
#define MOTION_CONTEXT_PARAM  (1 << 2)              /* not for config struct */

/**
 * struct _config_ctxt
 *
 *      Here we have the context for the configuration assiociated with
 *      the current section.  The context is held in a linked list, starting
 *      with the global definitions and followed by each successive section.
 *      Each section contains a chain of parameter values which are defined
 *      within the section.  If the desired param is not present within a
 *      "subsidiary" section, the global section is checked. 
 */
typedef struct _config_ctxt {
	struct _config_ctxt     *next;             /* pointer to next in chain */
	struct _config_ctxt     *global;           /* pointer to head of the chain */
	char                    *node_name;        /* the name of this node */
	char                    *title;            /* optional description */
	motion_ctxt_ptr         cnt;               /* pointer to motion context */
	config_param_ptr        params;            /* pointer to params chain */
	cfg_valid_ptr           valid_chain;       /* pointer to validation chain */
	int                     num_valid_params;  /* number of items in table */
	param_definition_ptr    param_valid;       /* pointer to a variable-length
	                                              table of valid params */
} config_ctxt, *config_ctxt_ptr;

typedef struct _cfg_file {
	struct _cfg_file        *next;
	FILE                    *file;
} cfg_file, *cfg_file_ptr;

struct _cfg_valid_struct {
	cfg_valid_ptr *next;
	int (*validate_param)(config_ctxt, config_param_ptr);
};

/*
 * Global runtime-configuration parameter structure
 *
 * This defines the global "configuration parameters", i.e. those parameters
 * which can be set by the user in the Motion configuration file.  There will
 * be one global version of this structure, and a separate (unique) copy for a
 * thread (if required).
 *
 * To add a new global configuration parameter, a new entry must be added to
 * this structure, and full details (including description, validation, default
 * values, etc.) must be added in the file global_params.h
 */
typedef struct config {
	int daemon;
	int setup_mode;
	int width;
	int height;
	int quality;
	int rotate;
	int threshold;
	int threshold_tune;
	const char *output_normal;
	int output_motion;
	int output_all;
	int gap;
	int max_mpeg_time;
	int snapshot_interval;
	const char *locate;
	int input;
	int norm;
	int framerate;
	int quiet;
	int ppm;
	int noise_level;
	int noise_tune;
	int minimum_gap;
	int lightswitch;
	int night_compensate;
	unsigned int low_cpu;
	int nochild;
	int auto_brightness;
	int brightness;
	int contrast;
	int saturation;
	int hue;
	int roundrobin_frames;
	int roundrobin_skip;
	int pre_capture;
	int post_capture;
	int switchfilter;
	int ffmpeg_cap_new;
	int ffmpeg_cap_motion;
	int ffmpeg_bps;
	int ffmpeg_variable_bitrate;
	int ffmpeg_deinterlace;
	const char *ffmpeg_video_codec;
	int webcam_port;
	int webcam_quality;
	int webcam_motion;
	int webcam_maxrate;
	int webcam_localhost;
	int webcam_limit;
	int control_port;
	int control_localhost;
	int control_html_output;
	const char *control_authentication;
	int frequency;
	int tuner_number;
	int ffmpeg_timelapse;
	const char *ffmpeg_timelapse_mode; 
	const char *video_device;
	const char *vidpipe;
	const char *target_dir;
	const char *jpeg_filename;
	const char *ffmpeg_filename;
	const char *snapshot_filename;
	const char *timelapse_filename;
	char *on_event_start;
	char *on_event_end;
	const char *mask_file;
	int smart_mask_speed;
	int sql_log_image;
	int sql_log_snapshot;
	int sql_log_mpeg;
	int sql_log_timelapse;
	const char *sql_query;
	const char *mysql_db;
	const char *mysql_host;
	const char *mysql_user;
	const char *mysql_password;
	char *on_picture_save;
	char *on_motion_detected;
	char *on_movie_start;
	char *on_movie_end;
	const char *motionvidpipe;
	const char *pgsql_db;
	const char *pgsql_host;
	const char *pgsql_user;
	const char *pgsql_password;
	int pgsql_port;
	int text_changes;
	const char *text_left;
	const char *text_right;
	const char *text_event;
	int text_double;
	const char *despeckle;
	int minimum_motion_frames;
	const char *use_plugin;
	// int debug_parameter;
	int argc;
	char **argv;
} config, *config_ptr;

/* Macro used by initilising include files */
#define CFG_PARM(V)	\
param_name:      #V,   \
param_voffset: offsetof(config, V),

/*
 * declare any external routines that are within newconfig.c
 */
void dump_config_file(FILE *, config_ctxt_ptr);
int conf_load (motion_ctxt_ptr);
int set_config_values(motion_ctxt_ptr, config_ctxt_ptr, void *);
int set_ext_values(motion_ctxt_ptr, config_ctxt_ptr, param_definition_ptr, int, void *);
void destroy_config_ctxt(config_ctxt_ptr);

extern config_ctxt_ptr gconf_ctxt;
extern motion_ctxt_ptr cnt_list;

#endif
