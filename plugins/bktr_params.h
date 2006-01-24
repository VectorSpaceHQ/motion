/*
 * This file is included in bktr_plugin, and contains the preset information
 * needed to perform validation of the Motion configuration file.
 */

/*
 * The address of this table will be returned when the calling program asks
 * for the parameter validation address.
 *
 * Note that params described in this table must have an entry in the structure
 * bktr_config, defined in bktr_plugin.h.
 */

static param_definition bktr_params[] = {
	{
	param_descr:    "BKTR ( BSD Video interface ) params",
	},
	{
	CFG_BKTRPARM(bktr_videodevice)
	param_descr:    "Videodevice to be used for capturing\n"
			"for FreeBSD default is /dev/bktr0\n",
	param_type:     STRING_PARAM,
	param_default:  "/dev/bktr0",
	},
	{
	CFG_BKTRPARM(bktr_tunerdevice)
	param_descr:	"tuner device to be used for capturing.\n"
			"bktr_input must be set to 1 and a rasonable to bktr_frequency\n",
	param_type:	STRING_PARAM,
	param_default:  NULL,
	},
	{
	CFG_BKTRPARM(bktr_input)
	param_descr:    "The video input channel to be used",
	param_type:     INTEGER_PARAM,
	param_default:  "0",
	param_vtype:    RANGE_VALIDATION,
	param_values:   "0 15",
	},
	{
	CFG_BKTRPARM(bktr_norm)
	param_descr:    "The video norm to use (only for video capture and TV tuner cards)\n"
	                "Values: 0 (PAL), 1 (NTSC), 2 (SECAM), 3 (PAL NC no colour)\n",
	param_type:     INTEGER_PARAM,
	param_default:  "0",
	},
	{
	CFG_BKTRPARM(bktr_frequency)
	param_descr:    "The frequency to set the tuner to (kHz) (only for TV tuner cards)",
	param_type:     INTEGER_PARAM,
	param_default:  "0",
	},
};
static int bktr_params_size = sizeof(bktr_params) / sizeof(bktr_params[0]);

