/* v4l_plugin.h
 *
 *      Include file for v4l_plugin.c
 *      Copyright 2005 by Jeroen Vreeken (pe1rxq@amsat.org)
 *      This software is distributed under the GNU public license version 2
 *      See also the file 'COPYING'.
 *
 */

#ifndef _INCLUDE_V4LPLUGIN_H
#define _INCLUDE_V4LPLUGIN_H

#include <stddef.h>

#define _LINUX_TIME_H 1
#ifndef WITHOUT_V4L
#include <linux/videodev.h>
#endif

/* video4linux stuff */
#define NORM_DEFAULT    0
#define NORM_PAL        0
#define NORM_NTSC       1
#define NORM_SECAM      2
#define NORM_PAL_NC     3
#define IN_DEFAULT      8
#define IN_TV           0
#define IN_COMPOSITE    1
#define IN_COMPOSITE2   2
#define IN_SVIDEO       3

#define VIDEO_DEVICE "/dev/video0"

struct video_dev {
	int fd;
	const char *video_device;
	int input;
	int width;
	int height;
	int brightness;
	int contrast;
	int saturation;
	int hue;
	unsigned long freq;
	int tuner_number;

	pthread_mutex_t mutex;
	pthread_mutexattr_t attr;
	int owner;
	int frames;

	/* Device type specific stuff: */
	/* v4l */
	int size_map;
	int v4l_fmt;
	unsigned char *v4l_buffers[2];
	int v4l_curbuffer;
	int v4l_maxbuffer;
	int v4l_bufsize;
};

typedef struct v4l_config {
	char    *v4l_videodevice;
	int     v4l_input;
	int     v4l_norm;
	int     v4l_frequency;
	int     v4l_tuner_number;
} v4l_config, *v4l_config_ptr;

#define CFG_V4LPARM(V)	\
param_name:      #V,  \
param_voffset:   offsetof(v4l_config, V),

/* video functions, v4l_plugin.c */
static int vid_start(motion_ctxt_ptr);
static int vid_next(motion_ctxt_ptr, unsigned char *);
static void vid_init(motion_ctxt_ptr);
static int vid_startpipe(motion_ctxt_ptr, const char *, int, int, int);
static int vid_putpipe(motion_ctxt_ptr, int, unsigned char *, int);
static void vid_close(motion_ctxt_ptr);
static void vid_cleanup(motion_ctxt_ptr);

#endif /* _INCLUDE_V4LPLUGIN_H */
