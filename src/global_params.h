/*
 * This file is included by newconfig.c, and contains the preset information
 * needed to perform the initial Motion configuration.
 *
 * FIXME: basically everything from Motion 3.4 was copied into the "global"
 * 	  param list.  Several should probably be removed!
 */

/*
 * Details of global parameters.
 *
 * See newconfig.h for a description of the param_definition structure.
 *
 * Note that entries with no param_name value indicate a "Section Description"
 * which will be output to any dumped configuration file.  Also note that the
 * dumped configuration file will follow the exact sequence declared here.
 * Descriptions can be multi-line by including the '\n' character.
 * Also note that, in accordance with standard C coding, a string may be
 * continued across lines by finishing a line with a " and starting the next
 * line with a ".
 */
param_definition global_params [] = {
	{
	param_descr:    "Daemon",
	},
	{
	CFG_PARM(daemon)
	param_descr:    "Start in daemon (background) mode and release terminal",
	param_type:     BOOLEAN_PARAM,
	param_default:  "off",
	param_vtype:    SET_VALIDATION,
	param_values:   ON_OFF,
	param_flags:    GLOBAL_ONLY_PARAM,
	},
	{
	param_descr:    "Basic Setup Mode",
	},
	{
	CFG_PARM(setup_mode)
	param_descr:    "Start in Setup-Mode, daemon disabled",
	param_type:     BOOLEAN_PARAM,
	param_default:  "off",
	param_vtype:    SET_VALIDATION,
	param_values:   ON_OFF,
	param_flags:    GLOBAL_ONLY_PARAM,
	},
	{
	param_descr:    "Load Plugins\n"
	                "Load plugins that are to be used in Motion"
	},
	/*
	 * Can't use our CFG_PARM macro for plugin, because there is no entry in
	 * the configuration structure, and there can be instances
	 */
	{
	param_name:     "plugin",
	param_descr:    "Name and location of plugin routine to be loaded",
	param_type:     PLUGIN_PARAM,
	param_flags:    GLOBAL_ONLY_PARAM | DUPS_OK_PARAM,
	},
	{
	param_name:     "input_plugin",
	param_descr:    "Specifies which plugin to use for video input",
	param_type:     STRING_PARAM,
	param_flags:    MOTION_CONTEXT_PARAM,
	},
	{
	param_name:     "movie_plugin",
	param_descr:    "Specifies which plugin to use for creating motion movies",
	param_type:     STRING_PARAM,
	param_flags:    MOTION_CONTEXT_PARAM,
	},
	{
	param_descr:    "Capture device options",
	},
	{
	CFG_PARM(rotate)
	param_descr:    "Rotate image this number of degrees. The rotation affects all saved images as\n"
	                "well as mpeg movies",
	param_type:     INTEGER_PARAM,
	param_default:  "0",
	param_vtype:    SET_VALIDATION,
	param_values:   "0 90 180 270",
	},
	{
	CFG_PARM(width)
	param_descr:    "Image width (pixels). Valid range: Camera dependent",
	param_type:     INTEGER_PARAM,
	param_default:  "352",
	},
	{
	CFG_PARM(height)
	param_descr:    "Image height (pixels). Valid range: Camera dependent",
	param_type:     INTEGER_PARAM,
	param_default:  "288",
	},
	{
	CFG_PARM(framerate)
	param_descr:    "Maximum number of frames to be captured per second.\n"
	                "(100 is almost no limit)",
	param_type:     INTEGER_PARAM,
	param_default:  "100",
	param_vtype:    RANGE_VALIDATION,
	param_values:   "2 100",
	},
	{
	CFG_PARM(auto_brightness)
	param_descr:    "Let motion regulate the brightness of a video device.\n"
	                "The auto_brightness feature uses the brightness option as its\n"
	                "target value.  If brightness is zero auto_brightness will adjust\n"
	                "to average brightness value 128.  Only recommended for cameras\n"
	                "without auto brightness",
	param_type:     BOOLEAN_PARAM,
	param_default:  "off",
	param_vtype:    SET_VALIDATION,
	param_values:   ON_OFF,
	},
	{
	CFG_PARM(brightness)
	param_descr:    "Set the initial brightness of a video device.\n"
	                "If auto_brightness is enabled, this value defines the average\n"
	                "brightness level which Motion will try and adjust to.\n"
	                "(0 = disabled)",
	param_type:     INTEGER_PARAM,
	param_default:  "0",
	param_vtype:    RANGE_VALIDATION,
	param_values:   "0 255",
	},
	{
	CFG_PARM(contrast)
	param_descr:    "Set the contrast of a video device.\n"
	                "(0 = disabled)",
	param_type:     INTEGER_PARAM,
	param_default:  "0",
	param_vtype:    RANGE_VALIDATION,
	param_values:   "0 255",
	},
	{
	CFG_PARM(saturation)
	param_descr:    "Set the saturation of a video device.\n"
	                "(0 = disabled)",
	param_type:     INTEGER_PARAM,
	param_default:  "0",
	param_vtype:    RANGE_VALIDATION,
	param_values:   "0 255",
	},
	{
	CFG_PARM(hue)
	param_descr:    "Set the hue of a video device (NTSC feature).\n"
	                "(0 = disabled)",
	param_type:     INTEGER_PARAM,
	param_default:  "0",
	param_vtype:    RANGE_VALIDATION,
	param_values:   "0 255",
	},
	{
	param_descr:    "Round Robin (multiple inputs on same video device name)",
	},
	{
	CFG_PARM(roundrobin_frames)
	param_descr:    "Number of frames to capture in each roundrobin step",
	param_type:     INTEGER_PARAM,
	param_default:  "1",
	},
	{
	CFG_PARM(roundrobin_skip)
	param_descr:    "Number of frames to skip before each roundrobin step",
	param_type:     INTEGER_PARAM,
	param_default:  "1",
	},
	{
	CFG_PARM(switchfilter)
	param_descr:    "Try to filter out noise generated by roundrobin",
	param_type:     BOOLEAN_PARAM,
	param_default:  "off",
	param_vtype:    SET_VALIDATION,
	param_values:   ON_OFF,
	},
	{
	param_descr:    "Motion Detection Settings",
	},
	{
	CFG_PARM(threshold)
	param_descr:    "Threshold for number of changed pixels in an image that\n"
	                "triggers motion detection",
	param_type:     INTEGER_PARAM,
	param_default:  "1500",
	},
	{
	CFG_PARM(threshold_tune)
	param_descr:    "Automatically tune the threshold down if possible",
	param_type:     BOOLEAN_PARAM,
	param_default:  "off",
	param_vtype:    SET_VALIDATION,
	param_values:   ON_OFF,
	},
	{
	CFG_PARM(noise_level)
	param_descr:    "Noise threshold for the motion detection",
	param_type:     INTEGER_PARAM,
	param_default:  "32",
	},
	{
	CFG_PARM(noise_tune)
	param_descr:    "Automatically tune the noise threshold",
	param_type:     BOOLEAN_PARAM,
	param_default:  "on",
	param_vtype:    SET_VALIDATION,
	param_values:   ON_OFF,
	},
	{
	CFG_PARM(night_compensate)
	param_descr:    "Enables motion to adjust its detection/noise level for\n"
	                "very dark frames. Don't use this with noise_tune on",
	param_type:     BOOLEAN_PARAM,
	param_default:  "off",
	param_vtype:    SET_VALIDATION,
	param_values:   ON_OFF,
	},
	{
	CFG_PARM(despeckle)
	param_descr:    "Despeckle motion image using (e)rode or (d)ilate or (l)abel\n"
	                "Recommended value is EedDl. Any combination (and number) of\n"
	                "E, e, d, and D is valid.  (l)abeling must only be used once\n"
	                "and the 'l' must be the last letter",
	param_type:     STRING_PARAM,
	},
	{
	CFG_PARM(mask_file)
	param_descr:    "PGM file to use as a sensitivity mask.\n"
	                "Full path name to",
	param_type:     STRING_PARAM,
	},
	{
	CFG_PARM(smart_mask_speed)
	param_descr:    "Dynamically create a mask file during operation.\n"
	                "Adjust speed of mask changes from 0 (off) to 10 (fast)",
	param_type:     INTEGER_PARAM,
	param_default:  "0",
	param_vtype:    RANGE_VALIDATION,
	param_values:   "0 10",
	},
	{
	CFG_PARM(lightswitch)
	param_descr:    "Ignore sudden massive light intensity changes given as a\n"
	                "percentage of the picture area that changed intensity. Valid\n"
	                "(0 = disabled)",
	param_type:     INTEGER_PARAM,
	param_default:  "0",
	param_vtype:    RANGE_VALIDATION,
	param_values:   "0 100",
	},
	{
	CFG_PARM(minimum_motion_frames)
	param_descr:    "Picture frames must contain motion at least the specified number\n"
	                "of frames in a row before they are detected as true motion. At the\n"
	                "default of 1, all motion is detected. Valid range: 1 to thousands,\n"
	                "recommended 1-5",
	param_type:     INTEGER_PARAM,
	param_default:  "1",
	},
	{
	CFG_PARM(pre_capture)
	param_default:  "Specifies the number of pre-captured (buffered) pictures from before\n"
	                "motion was detected that will be output at motion detection. Do not\n"
	                "use large values! Large values will cause Motion to skip video frames\n"
	                "and cause unsmooth mpegs. To smooth mpegs use larger values of\n"
	                "post_capture instead. Recommended range: 0 to 5",
	param_type:     INTEGER_PARAM,
	param_default:  "0",
	},
	{
	CFG_PARM(post_capture)
	param_descr:    "Number of frames to capture after motion is no longer detected",
	param_type:     INTEGER_PARAM,
	param_default:  "0",
	},
	{
	CFG_PARM(gap)
	param_descr:    "Gap is the seconds of no motion detection that triggers the end of\n"
	                "an event. An event is defined as a series of motion images taken within\n"
	                "a short timeframe. Recommended value is 60 seconds. The value 0 is\n"
	                "allowed and disables all events causing all Motion to be written to\n"
	                "one single mpeg file and no pre_capture",
	param_type:     INTEGER_PARAM,
	param_default:  "60",
	},
	{
	CFG_PARM(minimum_gap)
	param_descr:    "Minimum gap in seconds between the storing pictures while detecting motion.\n"
	                "0 = as fast as possible (given by the camera framerate)\n"
	                "Note: This option has nothing to do with the option 'gap'",
	param_type:     INTEGER_PARAM,
	param_default:  "0",
	},
	{
	CFG_PARM(max_mpeg_time)
	param_descr:    "Maximum length in seconds of an mpeg movie (0 = infinite),\n"
	                "When value is exceeded a new mpeg file is created",
	param_type:     INTEGER_PARAM,
	param_default:  "0",
	},
	{
	CFG_PARM(low_cpu)
	param_descr:    "Number of frames per second to capture when not detecting\n"
	                "motion (saves CPU load) (0 = disabled)",
	param_type:     INTEGER_PARAM,
	param_default:  "0",
	},
	{
	CFG_PARM(output_all)
	param_descr:    "Always save images even if there was no motion",
	param_type:     BOOLEAN_PARAM,
	param_default:  "off",
	param_vtype:    SET_VALIDATION,
	param_values:   ON_OFF,
	},
	{
	param_descr:    "Image File Output",
	},
	{
	CFG_PARM(output_normal)
	param_descr:    "Output 'normal' pictures when motion is detected\n"
	                "Valid values: on, off, first, best\n"
	                "When set to 'first', only the first picture of an event is saved.\n"
	                "Picture with most motion of an event is saved when set to 'best'.\n"
	                "Can be used as preview shot for the corresponding movie",
	param_type:     STRING_PARAM,
	param_default:  "on",
	},
	{
	CFG_PARM(output_motion)
	param_descr:    "Output pictures with only the pixels moving object (ghost images)",
	param_type:     BOOLEAN_PARAM,
	param_default:  "off",
	param_vtype:    SET_VALIDATION,
	param_values:   ON_OFF,
	},
	{
	CFG_PARM(quality)
	param_descr:    "The quality (in percent) to be used by the jpeg compression",
	param_type:     INTEGER_PARAM,
	param_default:  "75",
	},
	{
	CFG_PARM(ppm)
	param_descr:    "Output ppm images instead of jpeg",
	param_type:     BOOLEAN_PARAM,
	param_default:  "off",
	param_vtype:    SET_VALIDATION,
	param_values:   ON_OFF,
	},
#ifdef HAVE_FFMPEG
	{
	param_descr:    "Film (mpeg) File Output - ffmpeg based",
	},
	{
	CFG_PARM(ffmpeg_cap_new)
	param_descr:    "Use ffmpeg to encode mpeg movies in realtime",
	param_type:     BOOLEAN_PARAM,
	param_default:  "off",
	param_vtype:    SET_VALIDATION,
	param_values:   ON_OFF,
	},
	{
	CFG_PARM(ffmpeg_cap_motion)
	param_descr:    "Use ffmpeg to make movies with only the pixels moving\n"
	                "object (ghost images)",
	param_type:     BOOLEAN_PARAM,
	param_default:  "off",
	param_vtype:    SET_VALIDATION,
	param_values:   ON_OFF,
	},
	{
	CFG_PARM(ffmpeg_timelapse)
	param_descr:    "Use ffmpeg to encode a timelapse movie\n"
	                "Default value 0 = off - else save frame every Nth second",
	param_type:     INTEGER_PARAM,
	param_default:  "0",
	},
	{
	CFG_PARM(ffmpeg_timelapse_mode)
	param_descr:    "The file rollover mode of the timelapse video\n"
	                "Valid values: hourly, daily, weekly-sunday, weekly-monday, monthly, manual",
	param_type:     STRING_PARAM,
	param_default:  "daily",
	},
	{
	CFG_PARM(ffmpeg_bps)
	param_descr:    "Bitrate to be used by the ffmpeg encoder\n"
	                "This option is ignored if ffmpeg_variable_bitrate is not 0 (disabled)",
	param_type:     INTEGER_PARAM,
	param_default:  "400000",
	},
	{
	CFG_PARM(ffmpeg_variable_bitrate)
	param_descr:    "Enables and defines variable bitrate for the ffmpeg encoder.\n"
	                "ffmpeg_bps is ignored if variable bitrate is enabled.\n"
	                "Valid values: 0 = fixed bitrate defined by ffmpeg_bps,\n"
	                "or the range 2 - 31 where 2 means best quality and 31 is worst.",
	param_type:     INTEGER_PARAM,
	param_default:  "0",
	},
	{
	CFG_PARM(ffmpeg_deinterlace)
	param_descr:    "Use ffmpeg to deinterlace video. Necessary if you use an analog camera\n"
	                "and see horizontal combing on moving objects in video or pictures.\n"
	                "(default: off)",
	param_type:     BOOLEAN_PARAM,
	param_default:  "off",
	param_vtype:    SET_VALIDATION,
	param_values:   ON_OFF,
	},
	{
	CFG_PARM(ffmpeg_video_codec)
	param_descr:    "Codec to used by ffmpeg for the video compression.\n"
	                "Timelapse mpegs are always made in mpeg1 format independent from this option.\n"
	                "Supported formats are: mpeg1 (ffmpeg-0.4.8 only), mpeg4, and msmpeg4.\n"
	                "mpeg1 - gives you files with extension .mpg\n"
	                "mpeg4 or msmpeg4 - give you files with extension .avi\n"
	                "msmpeg4 is recommended for use with Windows Media Player because\n"
	                "it requires no installation of codec on the Windows client.",
	param_type:     STRING_PARAM,
	param_default:  "mpeg4",
	},
#endif /* HAVE_FFMPEG */

	{
	param_descr:    "Snapshots (Traditional Periodic Webcam File Output)",
	},
	{
	CFG_PARM(snapshot_interval)
	param_descr:    "Make automated snapshot every N seconds (0 = disabled)",
	param_type:     INTEGER_PARAM,
	param_default:  "0",
	},
	{
	param_descr:    "Text Display\n"
	                "%Y = year, %m = month, %d = date,\n"
	                "%H = hour, %M = minute, %S = second, %T = HH:MM:SS,\n"
	                "%v = event, %q = frame number, %t = thread (camera) number,\n"
	                "%D = changed pixels, %N = noise level, \\n = new line,\n"
	                "%i and %J = width and height of motion area,\n"
	                "%K and %L = X and Y coordinates of motion center\n"
	                "%C = value defined by text_event - do not use with text_event!\n"
	                "You can put quotation marks around the text to allow\n"
	                "leading spaces",
	},
	{
	CFG_PARM(locate)
	param_descr:    "Locate and draw a box around the moving object.\n"
	                "Valid values: on, off and preview\n"
	                "Set to 'preview' will only draw a box in preview_shot pictures.",
	param_type:     STRING_PARAM,
	param_default:  "off",
	},
	{
	CFG_PARM(text_right)
	param_descr:    "Draws the timestamp using same options as C function strftime(3)\n"
	                "Default: %Y-%m-%d\\n%T = date in ISO format and time in 24 hour clock\n"
	                "Text is placed in lower right corner",
	param_type:     STRING_PARAM,
	param_default:  "%Y-%m-%d\\n%T",
	},
	{
	CFG_PARM(text_left)
	param_descr:    "Draw a user defined text on the images using same options as\n"
	                "the C function strftime(3)\n"
	                "Default: Not defined = no text\n"
	                "Text is placed in lower left corner",
	param_type:     STRING_PARAM,
	},
  {
	CFG_PARM(text_changes)
	param_descr:    "Draw the number of changed pixed on the images. Will normally\n"
	        "be set to off except when you setup and adjust the motion settings\n"
	                "Text is placed in upper right corner",
	param_type:     BOOLEAN_PARAM,
	param_default:  "off",
	param_vtype:    SET_VALIDATION,
	param_values:   ON_OFF,
	},
	{
	CFG_PARM(text_event)
	param_descr:    "This option defines the value of the speciel event conversion specifier %C\n"
	                "You can use any conversion specifier in this option except %C. Date and time\n"
	                "values are from the timestamp of the first image in the current event.\n"
	                "The idea is that %C can be used filenames and text_left/right for creating\n"
	                "a unique identifier for each event.",
	param_type:     STRING_PARAM,
	param_default:  "%Y%m%d%H%M%S",
	},
	{
	CFG_PARM(text_double)
	param_descr:    "Draw characters at twice normal size on images.",
	param_type:     BOOLEAN_PARAM,
	param_default:  "off",
	param_vtype:    SET_VALIDATION,
	param_values:   ON_OFF,
	},

	{
	param_descr:    "Target Directories and filenames For Images And Films\n"
	                "For the options snapshot_, jpeg_, mpeg_ and\n"
	                "timelapse_filename you can use conversion specifiers\n"
	                "%Y = year, %m = month, %d = date,\n"
	                "%H = hour, %M = minute, %S = second,\n"
	                "%v = event, %q = frame number, %t = thread (camera) number,\n"
	                "%D = changed pixels, %N = noise level,\n"
	                "%i and %J = width and height of motion area,\n"
	                "%K and %L = X and Y coordinates of motion center\n"
	                "%C = value defined by text_event\n"
	                "Quotation marks around string are allowed.",
	},
	{
	CFG_PARM(target_dir)
	param_descr:    "Target base directory for pictures and films\n"
	                "Recommended to use absolute path. (Default: current working directory)",
	param_type:     STRING_PARAM,
	param_default:     ".",
	},
	{
	CFG_PARM(snapshot_filename)
	param_descr:    "File path for snapshots (jpeg or ppm) relative to target_dir\n"
	                "Default value is equivalent to legacy oldlayout option\n"
	                "For Motion 3.0 compatible mode choose: %Y/%m/%d/%H/%M/%S-snapshot\n"
	                "File extension .jpg or .ppm is automatically added so do not include this.\n"
	                "Note: A symbolic link called lastsnap.jpg created in the target_dir will\n"
	                "always point to the latest snapshot, unless snapshot_filename is\n"
	                "exactly 'lastsnap'",
	param_type:     STRING_PARAM,
	param_default:  "DEF_SNAPPATH",
	},
	{
	CFG_PARM(jpeg_filename)
	param_descr:    "File path for motion triggered images (jpeg or ppm) relative to target_dir\n"
	                "Default value is equivalent to legacy oldlayout option\n"
	                "For Motion 3.0 compatible mode choose: %Y/%m/%d/%H/%M/%S-%q\n"
	                "File extension .jpg or .ppm is automatically added so do not include this\n"
	                "Set to 'preview' together with best-preview feature enables special naming\n"
	                "convention for preview shots. See motion guide for details",
	param_type:     STRING_PARAM,
	param_default:  "DEF_JPEGPATH",
	},
#ifdef HAVE_FFMPEG
// FIXME that should not be FFMPEG dependent , it should be movie_plugin dependent	
	{
	CFG_PARM(movie_filename)
	param_descr:    "File path for motion triggered ffmpeg films (mpeg) relative to target_dir\n"
	                "Default value is equivalent to legacy oldlayout option\n"
	                "For Motion 3.0 compatible mode choose: %Y/%m/%d/%H%M%S\n"
	                "File extension .mpg or .avi is automatically added so do not\n"
	                "include this",
	param_type:     STRING_PARAM,
	param_default:  "DEF_MPEGPATH",
	},
	{
	CFG_PARM(timelapse_filename)
	param_descr:    "File path for timelapse mpegs relative to target_dir\n"
	                "Default value is near equivalent to legacy oldlayout option\n"
	                "For Motion 3.0 compatible mode choose: %Y/%m/%d-timelapse\n"
	                "File extension .mpg is automatically added so do not include\n"
	                "this",
	param_type:     STRING_PARAM,
	param_default:  "DEF_TIMEPATH",
	},
#endif /* HAVE_FFMPEG */

	{
	param_descr:    "Live Webcam Server",
	},
	{
	CFG_PARM(webcam_port)
	param_descr:    "The mini-http server listens to this port for requests (0 = disabled)\n",
	param_type:     INTEGER_PARAM,
	param_default:  "0",
	},
	{
	CFG_PARM(webcam_quality)
	param_descr:    "Quality of the jpeg images produced",
	param_type:     INTEGER_PARAM,
	param_default:  "50",
	},
	{
	CFG_PARM(webcam_motion)
	param_descr:    "Output frames at 1 fps when no motion is detected and increase to the\n"
	                "rate given by webcam_maxrate when motion is detected",
	param_type:     BOOLEAN_PARAM,
	param_default:  "off",
	param_vtype:    SET_VALIDATION,
	param_values:   ON_OFF,
	},
	{
	CFG_PARM(webcam_maxrate)
	param_descr:    "Maximum framerate for webcam streams",
	param_type:     INTEGER_PARAM,
	param_default:  "1",
	},
	{
	CFG_PARM(webcam_localhost)
	param_descr:    "Restrict webcam connections to localhost only",
	param_type:     BOOLEAN_PARAM,
	param_default:  "on",
	param_vtype:    SET_VALIDATION,
	param_values:   ON_OFF,
	},
	{
	CFG_PARM(webcam_limit)
	param_descr:    "Limits the number of images per connection (0 = unlimited)\n"
	                "Number can be defined by multiplying actual webcam rate by desired\n"
	                "number of seconds. Actual webcam rate is the smallest of the numbers\n"
	                "framerate and webcam_maxrate",
	param_type:     INTEGER_PARAM,
	param_default:  "0",
	},
	{
	param_descr:    "HTTP Based Control",
	},
	{
	CFG_PARM(control_port)
	param_descr:    "TCP/IP port for the http server to listen on (0 = disabled)",
	param_type:     INTEGER_PARAM,
	param_default:  "0",
	},
	{
	CFG_PARM(control_localhost)
	param_descr:    "Restrict control connections to localhost only",
	param_type:     BOOLEAN_PARAM,
	param_default:  "on",
	param_vtype:    SET_VALIDATION,
	param_values:   ON_OFF,
	},
	{
	CFG_PARM(control_html_output)
	param_descr:    "Output for http server, select off to choose raw text plain",
	param_type:     BOOLEAN_PARAM,
	param_default:  "on",
	param_vtype:    SET_VALIDATION,
	param_values:   ON_OFF,
	},
	{
	CFG_PARM(control_authentication)
	param_descr:    "Authentication for the http based control. Syntax username:password\n"
	                "Default: not defined (Disabled)",
	param_type:     STRING_PARAM,
	},
/*
	{
	param_descr:		"Tracking (Pan/Tilt)",
	},
	{
	CFG_PARM(track_type)
	param_descr:    "Type of tracker (0=none, 1=stepper, 2=iomojo, 3=pwc, 4=generic)\n"
				"The generic type enables the definition of motion center and motion size to\n"
				"be used with the convertion specifiers for options like on_motion_detected",
	param_type:		INTEGER_PARAM,
	param_default:		"0",
	},
	{
	CFG_PARM(track_auto)
	param_descr:    "Enable auto tracking",
	param_type:		BOOLEAN_PARAM,
	param_default:		"off",
	param_vtype:		SET_VALIDATION,
	param_values:		ON_OFF,
	},
	{
	CFG_PARM(track_port)
	param_descr:    "Serial port of motor",
	param_type:		STRING_PARAM,
	},
	{
	CFG_PARM(track_motorx)
	param_descr:    "Motor number for x-axis",
	param_type:		INTEGER_PARAM,
	param_default:		"-1",
	},
	{
	CFG_PARM(track_maxx)
	param_descr:    "Maximum value on x-axis",
	param_type:		INTEGER_PARAM,
	param_default:		"0",
	},
	{
	CFG_PARM(track_iomojo_id)
	param_descr:    "ID of an iomojo camera if used",
	param_type:		INTEGER_PARAM,
	param_default:		"0",
	},
	{
	CFG_PARM(track_step_angle_x)
	param_descr:    "Angle in degrees the camera moves per step on the X-axis\n"
				"with auto-track\n"
				"Currently only used with pwc type cameras",
	param_type:		INTEGER_PARAM,
	param_default:		"10",
	},
	{
	CFG_PARM(track_step_angle_y)
	param_descr:    "Angle in degrees the camera moves per step on the Y-axis\n"
				"with auto-track\n"
				"Currently only used with pwc type cameras",
	param_type:		INTEGER_PARAM,
	param_default:		"10",
	},
	{
	CFG_PARM(track_move_wait)
	param_descr:    "Delay to wait for after tracking movement as number\n"
				"of picture frames",
	param_type:		INTEGER_PARAM,
	param_default:		"10",
	},
	{
	CFG_PARM(track_speed)
	param_descr:    "Speed to set the motor to (stepper motor option)",
	param_type:		INTEGER_PARAM,
	param_default:		"255",
	},
	{
	CFG_PARM(track_stepsize)
	param_descr:    "Number of steps to make (stepper motor option)",
	param_type:		INTEGER_PARAM,
	param_default:		"40",
	},
*/
	{
	param_descr:    "External Commands, Warnings and Logging:\n"
	                "You can use conversion specifiers for the on_xxxx commands\n"
	                "%Y = year, %m = month, %d = date,\n"
	                "%H = hour, %M = minute, %S = second,\n"
	                "%v = event, %q = frame number, %t = thread (camera) number,\n"
	                "%D = changed pixels, %N = noise level,\n"
	                "%i and %J = width and height of motion area,\n"
	                "%K and %L = X and Y coordinates of motion center\n"
	                "%C = value defined by text_event\n"
	                "%f = filename with full path\n"
	                "%n = number indicating filetype\n"
	                "Both %f and %n are only defined for on_picture_save,\n"
	                "on_movie_start and on_movie_end\n" 
	                "Quotation marks round string are allowed.",
	},
	{
	CFG_PARM(quiet)
	param_descr:    "Do not sound beeps when detecting motion\n"
	                "Note: Motion never beeps when running in daemon mode.",
	param_type:     BOOLEAN_PARAM,
	param_default:  "on",
	param_vtype:    SET_VALIDATION,
	param_values:   ON_OFF,
	},
	{
	CFG_PARM(on_event_start)
	param_descr:    "Command to be executed when an event starts.\n"
	                "An event starts at first motion detected after a period of no\n"
	                "motion defined by gap",
	param_type:     STRING_PARAM,
	},
	{
	CFG_PARM(on_event_end)
	param_descr:    "Command to be executed when an event ends after a period of no motion.\n"
	                "The period of no motion is defined by option gap.",
	param_type:     STRING_PARAM,
	},
	{
	CFG_PARM(on_picture_save)
	param_descr:    "Command to be executed when a picture (.ppm|.jpg) is saved\n"
	                "To give the filename as an argument to a command append it with %f",
	param_type:     STRING_PARAM,
	},
	{
	CFG_PARM(on_motion_detected)
	param_descr:    "Command to be executed when a motion frame is detected",
	param_type:     STRING_PARAM,
	},
#ifdef HAVE_FFMPEG
	{
	CFG_PARM(on_movie_start)
	param_descr:    "Command to be executed when a movie file (.mpg|.avi) is created.\n"
	                "To give the filename as an argument to a command append it with %f",
	param_type:     STRING_PARAM,
	},
	{
	CFG_PARM(on_movie_end)
	param_descr:    "Command to be executed when a movie file (.mpg|.avi) is closed.\n"
	                "To give the filename as an argument to a command append it with %f",
	param_type:     STRING_PARAM,
	},
#endif /* HAVE_FFMPEG */

#if defined(HAVE_MYSQL) || defined(HAVE_PGSQL)
	{
	param_descr:    "Common Options For MySQL and PostgreSQL database features.\n"
	                "Options require the MySQL/PostgreSQL options to be active\n"
	                "also."
	},
	{
	CFG_PARM(sql_log_image)
	param_descr:    "Log to the database when creating motion triggered image file",
	param_type:     BOOLEAN_PARAM,
	param_default:  "on",
	param_vtype:    SET_VALIDATION,
	param_values:   ON_OFF,
	},
	{
	CFG_PARM(sql_log_snapshot)
	param_descr:    "Log to the database when creating a snapshot image file",
	param_type:     BOOLEAN_PARAM,
	param_default:  "on",
	param_vtype:    SET_VALIDATION,
	param_values:   ON_OFF,
	},
	{
	CFG_PARM(sql_log_mpeg)
	param_descr:    "Log to the database when creating motion triggered mpeg file",
	param_type:     BOOLEAN_PARAM,
	param_default:  "off",
	param_vtype:    SET_VALIDATION,
	param_values:   ON_OFF,
	},
	{
	CFG_PARM(sql_log_timelapse)
	param_descr:    "Log to the database when creating timelapse mpeg file",
	param_type:     BOOLEAN_PARAM,
	param_default:  "off",
	param_vtype:    SET_VALIDATION,
	param_values:   ON_OFF,
	},
	{
	CFG_PARM(sql_query)
	param_descr:    "SQL query string that is sent to the database\n"
	                "Use same conversion specifiers has for text features\n"
	                "Additional special conversion specifiers are\n"
	                "%n = the number representing the file_type\n"
	                "%f = filename with full path\n",
	param_type:     STRING_PARAM,
	param_default:  "insert into security(camera, filename, frame, file_type, time_stamp, text_event) values('%t', '%f', '%q', '%n', '%Y-%m-%d %T', '%C')",
	},

#endif /* defined(HAVE_MYSQL) || defined(HAVE_PGSQL) */

#ifdef HAVE_MYSQL
	{
	param_descr:    "Database Options For MySQL",
	},
	{
	CFG_PARM(mysql_db)
	param_descr:    "Mysql database to log to",
	param_type:     STRING_PARAM,
	},
	{
	CFG_PARM(mysql_host)
	param_descr:    "The host on which the database is located",
	param_type:     STRING_PARAM,
	},
	{
	CFG_PARM(mysql_user)
	param_descr:    "User account name for MySQL database",
	param_type:     STRING_PARAM,
	},
	{
	CFG_PARM(mysql_password)
	param_descr:    "User password for MySQL database",
	param_type:     STRING_PARAM,
	},
#endif /* HAVE_MYSQL */

#ifdef HAVE_PGSQL
	{
	param_descr:    "Database Options For PostgreSQL",
	},
	{
	CFG_PARM(pgsql_db)
	param_descr:    "PostgreSQL database to log to",
	param_type:     STRING_PARAM,
	},
	{
	CFG_PARM(pgsql_host)
	param_descr:    "The host on which the database is located",
	param_type:     STRING_PARAM,
	},
	{
	CFG_PARM(pgsql_user)
	param_descr:    "User account name for PostgreSQL database",
	param_type:     STRING_PARAM,
	},
	{
	CFG_PARM(pgsql_password)
	param_descr:    "User password for PostgreSQL database",
	param_type:     STRING_PARAM,
	},
	{
	CFG_PARM(pgsql_port)
	param_descr:    "Port on which the PostgreSQL database is located",
	param_type:     INTEGER_PARAM,
	param_default:  "5432",
	},
#endif /* HAVE_PGSQL */
/*	
	{
	param_descr:		"Video Loopback Device (vloopback project)",
	},
	{
	CFG_PARM(video_pipe)
	param_descr:    "Output images to a video4linux loopback device\n"
				"The value '-' means next available",
	param_type:		STRING_PARAM,
	},
	{
	CFG_PARM(motion_video_pipe)
	param_descr:    "Output motion images to a video4linux loopback device\n"
				"The value '-' means next available",
	param_type:		STRING_PARAM,
	},
*/	
};

