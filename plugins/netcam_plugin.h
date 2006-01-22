#ifndef _INCLUDE_NETCAMPLUGIN_H
#define _INCLUDE_NETCAMPLUGIN_H
/*
 *	netcam_plugin.h
 *
 *	Include file for handling network cameras.
 *
 *	This code was inspired by the original netcam.c module
 *	written by Jeroen Vreeken and enhanced by several Motion
 *	project contributors, particularly Angel Carpintero and
 *	Christopher Price.
 *	
 *	Copyright 2005, William M. Brack
 *	This software is distributed under the GNU Public license
 *	Version 2.  See also the file 'COPYING'.
 */

#include <jpeglib.h>
#include <setjmp.h>
#include <sys/socket.h>
#include <sys/types.h>

#ifdef HAVE_ANSIDECL_H
#include <ansidecl.h>
#endif

/* netcam_wget.h needs to have netcam_context_ptr */
typedef struct netcam_context *netcam_context_ptr;

#include "netcam_wget.h"        /* needed for struct rbuf */

/**
 * NETCAM_BUFFSIZE
 *
 * Initial size reserved for a JPEG image.  If expansion is required,
 * this value is also used for the amount to increase.
 */
#define NETCAM_BUFFSIZE 4096
/*
 * Error return codes for netcam routines.  The values are "bit
 * significant".  All error returns will return bit 1 set to indicate
 * these are "netcam errors"; additional bits set will give more detail
 * as to what kind of error it was.
 *
 */
#define NETCAM_GENERAL_ERR         0x02          /* binary 000010 */
#define NETCAM_NOTHING_NEW_ERR     0x06          /* binary 000110 */
#define NETCAM_JPEG_CONV_ERR       0x0a          /* binary 001010 */

/*
 * struct url_t is used when parsing the user-supplied URL, as well as
 * when attempting to connect to the netcam.
 */
struct url_t {
	char *service;
	char *userpass;
	char *host;
	int port;
	char *path;
};

/*
 * We use a special "triple-buffer" technique.  There are
 * three separate buffers (latest, receiving and jpegbuf)
 * which are each described using a struct netcam_image_buff
 */
typedef struct netcam_image_buff {
	char *ptr;
	int content_length;
	size_t size;                    /* total allocated size */
	size_t used;                    /* bytes already used */
	struct timeval image_time;      /* time this image was received */
} netcam_buff;
typedef netcam_buff *netcam_buff_ptr;

struct netcam_caps {        /* netcam capabilities: */
	unsigned char streaming;        /*  1 - supported       */
	unsigned char content_length;   /*  0 - unsupported     */
};
/*
 * struct netcam_context contains all the structures and other data
 * for an individual netcam.
 */
typedef struct netcam_context {
	struct context *cnt;        /* pointer to parent motion
	                               context structure */

	int finish;                 /* flag to break the camera-
	                               handling thread out of it's
	                               infinite loop in emergency */

	int threadnr;               /* motion's thread number for
	                               the camera-handling thread
	                               (if required).  Used for
	                               error reporting */

	pthread_t thread_id;        /* thread i.d. for a camera-
	                               handling thread (if required).
	                               Not currently used, but may be
	                               useful in the future */

	pthread_mutex_t mutex;      /* mutex used with conditional waits */

	pthread_cond_t exiting;     /* pthread condition structure to let
	                               the camera-handler acknowlege that
	                               it's finished */

	pthread_cond_t cap_cond;    /* pthread condition structure to
	                               initiate next capture request (used
	                               only with non-streaming cameras */

	pthread_cond_t pic_ready;   /* pthread condition structure used
	                                              for synchronisation between the
	                                              camera handler and the motion main
	                                              loop, showing new frame is ready */

	int start_capture;          /* besides our signalling condition,
	                               we also keep a flag to assure the
	                               camera-handler will always start
	                               a new cycle as soon as possible,
	                               even if it's not currently waiting
	                               on the condition. */

	char *connect_host;         /* the host to connect to (may be
	                               either the camera host, or
	                               possibly a proxy) */

	int connect_port;           /* usually will be 80, but can be
	                               specified as something else by
	                               the user */

	char *connect_request;      /* contains the complete string
	                               required for connection to the
	                               camera */

	int sock;                   /* fd for the camera's socket.
	                               Note that this value is also
	                               present within the struct
	                               rbuf *response. */

	struct timeval timeout;     /* The current timeout setting for
	                               the socket. */

	struct rbuf *response;      /* this structure (defined in the
	                               netcam_wget module) contains
	                               the context for an HTTP
	                               connection.  Note that this
	                               structure includes a large
	                               buffer for the HTTP data */

	struct netcam_caps caps;    /* netcam capabilities: */

	char *boundary;             /* 'boundary' string when used to
	                               separate mjpeg images */

	size_t boundary_length;     /* string length of the boundary
	                               string */

	                            /* Three separate buffers are used
	                               for handling the data.  Their
	                               definitions follow: */

	netcam_buff_ptr latest;     /* This buffer contains the latest
	                               frame received from the camera */

	netcam_buff_ptr receiving;  /* This buffer is used for receiving
	                               data from the camera */

	netcam_buff_ptr jpegbuf;    /* This buffer is used for jpeg
	                               decompresssion */

	int imgcnt;                 /* count for # of received jpegs */
	int imgcnt_last;            /* remember last count to check if a new
	                               image arrived */

	int error_count;            /* simple count of number of errors since
	                               last good frame was received */

	unsigned int width;         /* info for decompression */
	unsigned int height;

	struct timeval last_image;  /* time the most recent image was
	                               received */

	float av_frame_time;        /* "running average" of time between
	                               successive frames (microseconds) */

	struct jpeg_error_mgr jerr;
	jmp_buf setjmp_buffer;

	int jpeg_error;             /* flag to show error or warning
	                               occurred during decompression*/
} netcam_context;

typedef struct netcam_config {
	char *netcam_videodevice;
	char *netcam_url;
	char *netcam_userpass;
	char *netcam_proxy;
} netcam_config, *netcam_config_ptr;

#define CFG_NETCAMPARM(V)	\
param_name:	#V,	\
param_voffset:	offsetof(netcam_config, V),

/*
 * Declare prototypes for our external entry points
 */
/*     Within netcam_jpeg.c    */
int netcam_proc_jpeg (netcam_context_ptr, unsigned char *);
void netcam_get_dimensions (netcam_context_ptr);
/*     Within netcam.c        */
static int netcam_start (motion_ctxt_ptr);
static int netcam_next (motion_ctxt_ptr, unsigned char *);
static void netcam_cleanup (motion_ctxt_ptr);
ssize_t netcam_recv(netcam_context_ptr, void *, size_t, struct timeval *t);

#endif
