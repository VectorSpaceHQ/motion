/*
 * This file is included in v4l_plugin, and contains the preset information
 * needed to perform validation of the Motion configuration file.
 */

/*
 * The address of this table will be returned when the calling program asks
 * for the parameter validation address.
 *
 * Note that params described in this table must have an entry in the structure
 * v4l_config, defined in v4l_plugin.h.
 */

static param_definition v4l_params[] = {
	{
	param_descr:    "Video for Linux params",
	},
	{
	CFG_V4LPARM(v4l_videodevice)
	param_descr:    "Video device to be used for capturing",
	param_type:     STRING_PARAM,
	param_default:  "/dev/video0",
	},
	{
	CFG_V4LPARM(v4l_input)
	param_descr:    "The video input channel to be used",
	param_type:     INTEGER_PARAM,
	param_default:  "0",
	param_vtype:    RANGE_VALIDATION,
	param_values:   "0 15",
	},
	{
	CFG_V4LPARM(v4l_norm)
	param_descr:    "The video norm to use (only for video capture and TV tuner cards)\n"
	                "Values: 0 (PAL), 1 (NTSC), 2 (SECAM), 3 (PAL NC no colour)\n",
	param_type:     INTEGER_PARAM,
	param_default:  "0",
	},
	{
	CFG_V4LPARM(v4l_frequency)
	param_descr:    "The frequency to set the tuner to (kHz) (only for TV tuner cards)",
	param_type:     INTEGER_PARAM,
	param_default:  "0",
	},
};
static int v4l_params_size = sizeof(v4l_params) / sizeof(v4l_params[0]);

