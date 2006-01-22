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

static param_definition netcam_params[] = {
	{
	param_descr:    "Video for Linux params",
	},
	{
	CFG_NETCAMPARM(netcam_videodevice)
	param_descr:    "Video device to be used for capturing",
	},
	{
	CFG_NETCAMPARM(netcam_url)
	param_descr:    "URL to use if you are using a network camera.  Must be a URL\n"
	                "that returns a single jpeg picture or a raw mjpeg stream\n"
	                "(include 'http://').  Size will be autodetected",
	param_type:     STRING_PARAM,
	},
	{
	CFG_NETCAMPARM(netcam_userpass)
	param_descr:    "Username and password for network camera (only if required)\n"
	                "Syntax is user:password",
	param_type:     STRING_PARAM,
	},
	{
	CFG_NETCAMPARM(netcam_proxy)
	param_descr:    "URL to use for a netcam proxy server, if required, e.g.\n"
	                "\"http://myproxy\".  If a port number other than 80 is needed,\n"
	                "use \"http://myproxy:1234\"",
	param_type:     STRING_PARAM,
	},
};
static int netcam_params_size = sizeof(netcam_params) / sizeof(netcam_params[0]);
