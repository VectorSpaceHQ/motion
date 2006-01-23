/*	motion.c
 *
 *	Detect changes in a video stream.
 *	Copyright 2000 by Jeroen Vreeken (pe1rxq@amsat.org)
 *	This software is distributed under the GNU public license version 2
 *	See also the file 'COPYING'.
 *
 */
#include "ffmpeg.h"
#include "motion.h"

#include "newconfig.h"
#include "alg.h"
#include "track.h"
#include "event.h"
#include "picture.h"
#include "rotate.h"
#if 0     /* disable until updated for Version 4 */
#include "webhttpd.h"
#endif

/**
 * tls_key_ctxptr
 * 
 *   TLS key for storing thread context pointer in thread-local storage.
 */
pthread_key_t tls_key_ctxptr;

/**
 * tls_key_threadnr
 * 
 *   TLS key for storing thread number in thread-local storage.
 */
pthread_key_t tls_key_threadnr;

/**
 * global_lock 
 *
 *   Protects any global variables (like 'threads_running') during updates, 
 *   to prevent problems with multiple threads updating at the same time.
 */
//pthread_mutex_t global_lock = PTHREAD_MUTEX_INITIALIZER;
pthread_mutex_t global_lock;

/**
 * cnt_list
 *
 *   Chain of context structures, one for each main Motion thread.
 */
struct context *cnt_list=NULL;

/**
 * gconf_ctxt
 *
 *   Chain of config contexts (one for each configuration file section)
 */
config_ctxt_ptr gconf_ctxt;

/**
 * threads_running
 *
 *   Keeps track of number of Motion threads currently running. Also used 
 *   by 'main' to know when all threads have exited.
 */
volatile int threads_running=0;

/*
 * debug_level is for developers, normally used to control which
 * types of messages get output.
 */
int debug_level;

/**
 * restart
 *
 *   Differentiates between a quit and a restart. When all threads have
 *   finished running, 'main' checks if 'restart' is true and if so starts
 *   up again (instead of just quitting).
 */
int restart=0;

/**
 * plugins_loaded
 *
 *   Chain of plugin descriptors to control which plugins are currently
 *   loaded.
 */
motion_plugin_ptr plugins_loaded;

/**
 * context_init
 *
 *   Initializes a context struct with the default values for all the 
 *   variables.
 *
 * Parameters:
 *
 *   cnt - the context struct to initialize
 *
 * Returns: nothing
 */
static void context_init (struct context *cnt, int argc, char **argv)
{
	/*
	* We first clear the entire structure to zero, then fill in any
	* values which have non-zero default values.  Note that this
	* assumes that a NULL address pointer has a value of binary 0
	* (this is also assumed at other places within the code, i.e.
	* there are instances of "if (ptr)").  Just for possible future
	* changes to this assumption, any pointers which are intended
	* to be initialised to NULL are listed within a comment.
	*/

	memset(cnt, 0, sizeof(struct context));

	/* For plugins, we need a structure with callback addresses */
	cnt->callbacks = mymalloc(sizeof(motion_callbacks));
	cnt->callbacks->motion_log = motion_log;
	cnt->callbacks->motion_alloc = mymalloc;
	cnt->callbacks->motion_realloc = myrealloc;
	cnt->callbacks->motion_rotate_map = rotate_map;
	cnt->debug_level = &debug_level;
	cnt->global_lock = &global_lock;
	cnt->threads_running = &threads_running;
	cnt->tls_key_threadnr = &tls_key_threadnr;

	cnt->conf.argv = argv;
	cnt->conf.argc = argc;
	cnt->noise_level=255;
	cnt->lastrate=25;

	memcpy(&cnt->track, &track_template, sizeof(struct trackoptions));
	cnt->pipe=-1;
	cnt->mpipe=-1;

// /* Here are the pointers required to be set */
// #ifdef HAVE_MYSQL
// 	cnt->database=NULL;
// #endif /* HAVE_MYSQL */
// 
// #ifdef HAVE_PGSQL
// 	cnt->database_pg=NULL;
// #endif /* HAVE_PGSQL */
// 
// #ifdef HAVE_FFMPEG
// 	cnt->ffmpeg_new=NULL;
// 	cnt->ffmpeg_motion=NULL;
// 	cnt->ffmpeg_timelapse=NULL;
// #endif /* HAVE_FFMPEG */

}

/**
 * context_destroy
 *
 *   Destroys a context struct by freeing allocated memory, calling the
 *   appropriate cleanup functions and finally freeing the struct itself.
 *
 * Parameters:
 *
 *   cnt - the context struct to destroy
 *
 * Returns: nothing
 */
static void context_destroy(struct context *cnt)
{
	int j;
fprintf(stderr, "context_destroy for %p\n",cnt);
	if (cnt->imgs.out)
		free(cnt->imgs.out);
	if (cnt->imgs.ref)
		free(cnt->imgs.ref);
	if (cnt->imgs.image_virgin)
		free(cnt->imgs.image_virgin);
	if (cnt->imgs.image_ring_buffer)
		free(cnt->imgs.image_ring_buffer);
	if (cnt->imgs.labels)
		free(cnt->imgs.labels);
	if (cnt->imgs.labelsize)
		free(cnt->imgs.labelsize);
	if (cnt->imgs.smartmask)
		free(cnt->imgs.smartmask);
	if (cnt->imgs.smartmask_final)
		free(cnt->imgs.smartmask_final);
	if (cnt->imgs.smartmask_buffer)
		free(cnt->imgs.smartmask_buffer);
	if (cnt->imgs.common_buffer)
		free(cnt->imgs.common_buffer);
	if (cnt->imgs.timestamp)
		free(cnt->imgs.timestamp);
	if (cnt->imgs.shotstamp)
		free(cnt->imgs.shotstamp);
	if (cnt->imgs.preview_buffer)
		free(cnt->imgs.preview_buffer);
	rotate_deinit(cnt); /* cleanup image rotation data */

	if(cnt->pipe != -1)
		close(cnt->pipe);
	if(cnt->mpipe != -1)
		close(cnt->mpipe);

	/* Cleanup any video-device specific data */
	if (cnt->video_ctxt) {
		if (cnt->video_ctxt->video_dev_cleanup)
			cnt->video_ctxt->video_dev_cleanup(cnt);
	}
	if (cnt->video_vars) {
		if (cnt->video_vars->video_params)
			free(cnt->video_vars->video_params);
		free(cnt->video_vars);
	}
	if (cnt->callbacks)
		free(cnt->callbacks);

	/* Cleanup the current time structures */
	if (cnt->currenttime_tm)
		free(cnt->currenttime_tm);
	if (cnt->eventtime_tm)
		free(cnt->eventtime_tm);

	/*
	 * May be necessary to enhance the newconfig struct to have unique copies of strings,
	 * but that's not yet certain.  For now we just comment out this block.
	 */
#if 0
	/* Free memory allocated for config parameters */
	for (j = 0; config_params[j].param_name != NULL; j++) {
		if (config_params[j].copy == copy_string) {
			void **val;
			val = (void *)((char *)cnt+(int)config_params[j].conf_value);
			if (*val) {
				free(*val);
				*val = NULL;
			}
		}
	}
#endif

	free(cnt);
}

/**
 * sig_handler
 *
 *  Our SIGNAL-Handler. We need this to handle alarms and external signals.
 */
static void sig_handler(int signo)
{
	int i;
	struct context *cnt;

	switch(signo) {
		case SIGALRM:
			/* Somebody (maybe we ourself) wants us to make a snapshot
			 * This feature triggers snapshots on ALL threads that have
			 * snapshot_interval different from 0.
			 */
			cnt = cnt_list;
			while (cnt) {
				if (cnt->conf.snapshot_interval) {
					cnt->snapshot=1;
				}
				cnt = cnt->next;
			}
			break;
		case SIGUSR1:
			/* Ouch! We have been hit from the outside! Someone wants us to
			   make a movie! */
			cnt = cnt_list;
			while (cnt) {
				cnt->makemovie=1;
				cnt = cnt->next;
			}
			break;
		case SIGHUP:
			restart = 1;
			/* Fall through, as the value of 'restart' is the only difference
			 * between SIGHUP and the ones below.
			 */
		case SIGINT:
		case SIGQUIT:
		case SIGTERM:
			/* Somebody wants us to quit! We should better finish the actual
			    movie and end up! */
			cnt = cnt_list;
			while(cnt) {
				cnt->makemovie=1;
				cnt->finish=1;
				cnt = cnt->next;
			}
			break;
		case SIGSEGV:
			exit(0);
	}
}

/**
 * sigchild_handler
 *
 *   This function is a POSIX compliant replacement of the commonly used
 *   signal(SIGCHLD, SIG_IGN).
 */
static void sigchild_handler(int signo ATTRIBUTE_UNUSED)
{
	return;
}

/**
 * motion_detected
 *
 *   Called from 'motion_loop' when motion is detected, or when to act as if
 *   motion was detected (e.g. in post capture).
 *
 * Parameters:
 *
 *   cnt      - current thread's context struct
 *   diffs    - number of different pixels between the reference image and the
 *              new image (may be zero)
 *   dev      - video device file descriptor
 *   devpipe  - file descriptor of still image pipe
 *   devmpipe - file descriptor of motion pipe
 *   newimg   - pointer to the newly captured image
 */
static void motion_detected(struct context *cnt, int diffs, int dev, unsigned char *newimg)
{
	struct config *conf = &cnt->conf;
	struct images *imgs = &cnt->imgs;
	struct coord *location = &cnt->location;

	cnt->lasttime = cnt->currenttime;

	/* Take action if this is a new event */
	if (cnt->event_nr != cnt->prev_event) {
		int i, tmpshots;
		struct tm tmptime;
		cnt->preview_max = 0;

		/* Reset prev_event number to current event and save event time
		 * in both time_t and struct tm format.
		 */
		cnt->prev_event = cnt->event_nr;
		cnt->eventtime = cnt->currenttime;
		localtime_r(&cnt->eventtime, cnt->eventtime_tm);

		/* Since this is a new event we create the event_text_string used for
		 * the %C conversion specifier. We may already need it for
		 * on_motion_detected_commend so it must be done now.
		 */
		mystrftime(cnt, cnt->text_event_string, sizeof(cnt->text_event_string),
		           cnt->conf.text_event, cnt->eventtime_tm, NULL, 0);

		/* EVENT_FIRSTMOTION triggers on_event_start_command and event_ffmpeg_newfile */
		event(cnt, EVENT_FIRSTMOTION, newimg, NULL, NULL, cnt->currenttime_tm);

		if (cnt->conf.setup_mode)
			motion_log(-1, 0, "Motion detected - starting event %d", cnt->event_nr);

		/* pre_capture frames are written as jpegs and to the ffmpeg film
		 * We store the current cnt->shots temporarily until we are done with
		 * the pre_capture stuff
		 */
		 
		tmpshots = cnt->shots;

		for (i=cnt->precap_cur; i < cnt->precap_nr; i++) {
			localtime_r(cnt->imgs.timestamp + i, &tmptime);
			cnt->shots = *(cnt->imgs.shotstamp + i);
			event(cnt, EVENT_IMAGE_DETECTED,
			    cnt->imgs.image_ring_buffer + (cnt->imgs.size * i), NULL, NULL, &tmptime);
		}

		if (cnt->precap_cur) {
			localtime_r(cnt->imgs.timestamp+cnt->precap_nr, &tmptime);
			cnt->shots = *(cnt->imgs.shotstamp + cnt->precap_nr);
			event(cnt, EVENT_IMAGE_DETECTED,
			      cnt->imgs.image_ring_buffer + (cnt->imgs.size * cnt->precap_nr),
			      NULL, NULL, &tmptime);
		}

		for (i=0; i < cnt->precap_cur-1; i++) {
			localtime_r(cnt->imgs.timestamp + i, &tmptime);
			cnt->shots = *(cnt->imgs.shotstamp + i);
			event(cnt, EVENT_IMAGE_DETECTED,
			      cnt->imgs.image_ring_buffer + (cnt->imgs.size * i),
			      NULL, NULL, &tmptime);
		}
		/* If output_normal=first always capture first motion frame as preview-shot */
		if (cnt->new_img == NEWIMG_FIRST){
			cnt->preview_shot = 1;
			if (cnt->locate == LOCATE_PREVIEW){
				alg_draw_location(location, imgs, imgs->width, newimg, LOCATE_NORMAL);
			}
		}
		cnt->shots = tmpshots;
	}

	/* motion_detected is called with diffs = 0 during post_capture
	 * and if cnt->conf.output_all is enabled. We only want to draw location
	 * and call EVENT_MOTION when it is a picture frame with actual motion detected.
	 */
	if (diffs) {
		if (cnt->locate == LOCATE_ON)
			alg_draw_location(location, imgs, imgs->width, newimg, LOCATE_BOTH);

		/* EVENT_MOTION triggers event_beep and on_motion_detected_command */
		event(cnt, EVENT_MOTION, NULL, NULL, NULL, cnt->currenttime_tm);
	}

	/* Check for most significant preview-shot when output_normal=best */
	if (cnt->new_img == NEWIMG_BEST && diffs > cnt->preview_max) {
		memcpy(cnt->imgs.preview_buffer, newimg, cnt->imgs.size);
		cnt->preview_max = diffs;
		if (cnt->locate == LOCATE_PREVIEW){
			alg_draw_location(location, imgs, imgs->width, cnt->imgs.preview_buffer, LOCATE_NORMAL);
		}
	}

	if (cnt->shots < conf->framerate && cnt->currenttime - cnt->lastshottime >= conf->minimum_gap ) {
		cnt->lastshottime = cnt->currenttime;

		/* Output the latest picture 'image_new' or image_out for motion picture. */
		event(cnt, EVENT_IMAGE_DETECTED, newimg, NULL, NULL, cnt->currenttime_tm);

		/* If config option webcam_motion is enabled, send the latest motion detected image
		 * to the webcam but only if it is not the first shot within a second. This is to
		 * avoid double frames since we already have sent a frame to the webcam.
		 * We also disable this in setup_mode.
		 */
		if (conf->webcam_motion && !conf->setup_mode && cnt->shots != 1)
			event(cnt, EVENT_WEBCAM, newimg, NULL, NULL, cnt->currenttime_tm);
		cnt->preview_shot = 0;
	}

	if (cnt->track.type != 0 && diffs != 0)	{
		cnt->moved = track_move(cnt, dev, &cnt->location, imgs, 0);
	}
}

/**
 * motion_init
 *
 * This routine is called from motion_loop (the main thread of the program) to do
 * all of the initialisation required before starting the actual run.
 *
 * Parameters:
 *
 *      cnt     Pointer to the motion context structure
 *
 * Returns:     nothing
 */
static void motion_init(struct context *cnt)
{
	int i;
	FILE *picture;

	/* Store thread number in TLS. */
	pthread_setspecific(tls_key_threadnr, (void *)((unsigned long)cnt->threadnr));

	cnt->diffs = 0;
	cnt->currenttime_tm = mymalloc(sizeof(struct tm));
	cnt->eventtime_tm = mymalloc(sizeof(struct tm));
	cnt->smartmask_speed = 0;

	/* We initialize cnt->event_nr to 1 and cnt->prev_event to 0 (not really needed) so
	 * that certain code below does not run until motion has been detected the first time */
	cnt->event_nr = 1;
	cnt->prev_event = 0;
#if 0
	if (cnt->video_ctxt->video_init)
		cnt->video_ctxt->video_init(cnt);
#endif
	motion_log(LOG_DEBUG, 0, "Thread started");

	if (!cnt->conf.target_dir)
		cnt->conf.target_dir = strdup(".");

	/* set the device settings */
	if (!cnt->video_ctxt) {
		motion_log(LOG_ERR, 0, "No video context found - configuration error!");
		exit(1);
	}
	cnt->video_dev = cnt->video_ctxt->video_start(cnt);
	if (cnt->video_dev == -1) {
		motion_log(LOG_ERR, 0, "Capture error calling video_start");
		motion_log(-1 , 0, "Thread finishing...");
		exit(1);
	}

	cnt->imgs.image_ring_buffer = mymalloc(cnt->imgs.size);
	memset(cnt->imgs.image_ring_buffer, 0, cnt->imgs.size);       /* initialize to zero */
	cnt->imgs.ref = mymalloc(cnt->imgs.size);
	cnt->imgs.out = mymalloc(cnt->imgs.size);
	cnt->imgs.image_virgin = mymalloc(cnt->imgs.size);
	cnt->imgs.smartmask = mymalloc(cnt->imgs.motionsize);
	cnt->imgs.smartmask_final = mymalloc(cnt->imgs.motionsize);
	cnt->imgs.smartmask_buffer = mymalloc(cnt->imgs.motionsize*sizeof(int));
	cnt->imgs.labels = mymalloc(cnt->imgs.motionsize*sizeof(cnt->imgs.labels));
	cnt->imgs.labelsize = mymalloc((cnt->imgs.motionsize/2+1)*sizeof(cnt->imgs.labelsize));
	cnt->imgs.timestamp = mymalloc(sizeof(time_t));
	cnt->imgs.shotstamp = mymalloc(sizeof(int));

	/* Allocate a buffer for temp. usage in some places */
	/* Only despeckle for now... */
	cnt->imgs.common_buffer = mymalloc(3*cnt->imgs.width);

	/* Now is a good time to init rotation data. Since video_start has been
	 * called, we know that we have imgs.width and imgs.height. When capturing
	 * from a V4L device, these are copied from the corresponding conf values
	 * in video_start. When capturing from a netcam, they get set in netcam_start, 
	 * which is called from video_start.
	 *
	 * rotate_init will set cap_width and cap_height in cnt->rotate_data.
	 */
	rotate_init(cnt); /* rotate_deinit is called in main */

	/* Allow videodevice to settle in */

	/* Capture first image, or we will get an alarm on start */
	for (i = 0; i < 10; i++) {
		if (cnt->video_ctxt->video_next(cnt, cnt->imgs.image_ring_buffer) == 0)
			break;
		SLEEP(2,0);
	}
	if (i >= 10) {
		motion_log(LOG_ERR, 0, "Error capturing first image");
		motion_log(-1, 0, "Thread finishing...");
		exit(1);
	}

	/* create a reference frame */
	memcpy(cnt->imgs.ref, cnt->imgs.image_ring_buffer, cnt->imgs.size);

	/* open video loopback devices if enabled */
	if (cnt->conf.vidpipe) {
		if (cnt->conf.setup_mode)
			motion_log(-1, 0, "Opening video loopback device for normal pictures");
		/* video_pipe_start should get the output dimensions */
		cnt->pipe = cnt->video_ctxt->video_pipe_start(cnt, cnt->conf.vidpipe, cnt->imgs.width, cnt->imgs.height, cnt->imgs.type);
		if (cnt->pipe < 0) {
			motion_log(LOG_ERR, 0, "Failed to open video loopback");
			motion_log(-1, 0, "Thread finishing...");
			exit(1);
		}
	}
	if (cnt->conf.motionvidpipe) {
		if (cnt->conf.setup_mode)
			motion_log(-1, 0, "Opening video loopback device for motion pictures");
		/* video_pipe_start should get the output dimensions */
		cnt->mpipe = cnt->video_ctxt->video_pipe_start(cnt, cnt->conf.motionvidpipe, cnt->imgs.width, cnt->imgs.height, cnt->imgs.type);
		if (cnt->mpipe < 0) {
			motion_log(LOG_ERR, 0, "Failed to open video loopback");
			motion_log(-1, 0, "Thread finishing...");
			exit(1);
		}
	}

#ifdef HAVE_MYSQL
	if(cnt->conf.mysql_db) {
		cnt->database = (MYSQL *) mymalloc(sizeof(MYSQL));
		mysql_init(cnt->database);
		if (!mysql_real_connect(cnt->database, cnt->conf.mysql_host, cnt->conf.mysql_user,
		    cnt->conf.mysql_password, cnt->conf.mysql_db, 0, NULL, 0)) {
			motion_log(LOG_ERR, 0, "Cannot connect to MySQL database %s on host %s with user %s",
			           cnt->conf.mysql_db, cnt->conf.mysql_host, cnt->conf.mysql_user);
			motion_log(LOG_ERR, 0, "MySQL error was %s", mysql_error(cnt->database));
			motion_log(-1, 0, "Thread finishing...");
			exit(1);
		}
	}
#endif /* HAVE_MYSQL */

#ifdef HAVE_PGSQL
	if (cnt->conf.pgsql_db) {
		char connstring[255];

		/* create the connection string. 
		   Quote the values so we can have null values (blank)*/
		snprintf(connstring, 255,
		         "dbname='%s' host='%s' user='%s' password='%s' port='%d'",
		         cnt->conf.pgsql_db, /* dbname */
		         (cnt->conf.pgsql_host ? cnt->conf.pgsql_host : ""), /* host (may be blank) */
		         (cnt->conf.pgsql_user ? cnt->conf.pgsql_user : ""), /* user (may be blank) */
		         (cnt->conf.pgsql_password ? cnt->conf.pgsql_password : ""), /* password (may be blank) */
		          cnt->conf.pgsql_port
		);

		cnt->database_pg = PQconnectdb(connstring);
		if (PQstatus(cnt->database_pg) == CONNECTION_BAD) {
			motion_log(LOG_ERR, 0, "Connection to PostgreSQL database '%s' failed: %s",
			           cnt->conf.pgsql_db, PQerrorMessage(cnt->database_pg));
			motion_log(-1, 0, "Thread finishing...");
			exit(1);
		}
	}
#endif /* HAVE_PGSQL */

	/* Load the mask file if any */
	if (cnt->conf.mask_file) {
		if ((picture = fopen(cnt->conf.mask_file, "r"))) {
			/* NOTE: The mask is expected to have the output dimensions. I.e., the mask
			 * applies to the already rotated image, not the capture image. Thus, use
			 * width and height from imgs.
			 */
			cnt->imgs.mask = get_pgm(picture, cnt->imgs.width, cnt->imgs.height);
			fclose(picture);
		} else {
			motion_log(LOG_ERR, 1, "Error opening mask file %s", cnt->conf.mask_file);
			/* Try to write an empty mask file to make it easier
			   for the user to edit it */
			put_fixed_mask(cnt, cnt->conf.mask_file);
		}
		if (!cnt->imgs.mask) {
			motion_log(LOG_ERR, 0, "Failed to read mask image. Mask feature disabled.");
		} else {
			if (cnt->conf.setup_mode)
				motion_log(-1, 0, "Maskfile \"%s\" loaded.",cnt->conf.mask_file);
		}
	} else
		cnt->imgs.mask=NULL;

	/* Always initialize smart_mask - someone could turn it on later... */
	memset(cnt->imgs.smartmask, 0, cnt->imgs.motionsize);
	memset(cnt->imgs.smartmask_final, 255, cnt->imgs.motionsize);
	memset(cnt->imgs.smartmask_buffer, 0, cnt->imgs.motionsize*sizeof(int));

	/* Set noise level */
	cnt->noise_level = cnt->conf.noise_level;

	/* Set threshold value */
	cnt->threshold = cnt->conf.threshold;

	/* Initialize webcam server if webcam port is specified to not 0 */
	if (cnt->conf.webcam_port) {
		if ( webcam_init(cnt) == -1 ) {
			motion_log(LOG_ERR, 1, "Problem enabling stream server");
			cnt->finish = 1;
			cnt->makemovie = 0;
		}
		motion_log(LOG_DEBUG, 0, "Started stream webcam server in port %d", cnt->conf.webcam_port);
	}

	/* Prevent first few frames from triggering motion... */
	cnt->moved = 8;
}

/**
 * motion_loop
 *
 *   Thread function for the motion handling threads.
 *
 */
static void *motion_loop(void *arg)
{
	struct context *cnt = arg;
	int i, j, detecting_motion = 0;
	time_t lastframetime = 0;
	int postcap = 0;
	int frame_buffer_size;
	int smartmask_ratio = 0;
	int smartmask_count = 20;
	int smartmask_lastrate = 0;
	int olddiffs = 0;
	int text_size_factor;
	int passflag = 0;
	long int *rolling_average_data;
	long int rolling_average_limit, required_frame_time, frame_delay, delay_time_nsec;
	int rolling_frame = 0;
	struct timeval tv1, tv2;
	unsigned long int rolling_average, elapsedtime;
	unsigned long long int timenow = 0, timebefore = 0;
	unsigned char *newimg = NULL;     /* Pointer to where new image is stored */
	int vid_return_code = 0;          /* Return code used when calling video_next*/

#ifdef HAVE_FFMPEG
	/* Next two variables are used for timelapse feature
	 * time_last_frame is set to 1 so that first comming timelapse or second=0
	 * is acted upon.
	 */
	unsigned long int time_last_frame=1, time_current_frame;
#endif /* HAVE_FFMPEG */

	motion_init(cnt);

	/* Initialize the double sized characters if needed. */
	if(cnt->conf.text_double)
		text_size_factor = 2;
	else
		text_size_factor = 1;

	/* Work out expected frame rate based on config setting */
	if (cnt->conf.framerate)
		required_frame_time = 1000000L / cnt->conf.framerate;
	else
		required_frame_time = 0;

	frame_delay = required_frame_time;

	/*
	 * Reserve enough space for a 10 second timing history buffer. Note that,
	 * if there is any problem on the allocation, mymalloc does not return.
	 */
	rolling_average_limit = 10 * cnt->conf.framerate;
	rolling_average_data = mymalloc(sizeof(long int) * rolling_average_limit);

	/* Preset history buffer with expected frame rate */
	for (j=0; j< rolling_average_limit; j++)
		rolling_average_data[j]=required_frame_time;

	/* MAIN MOTION LOOP BEGINS HERE */
	/* Should go on forever... unless you bought vaporware :) */

	while (!cnt->finish || cnt->makemovie) {

	/***** MOTION LOOP - PREPARE FOR NEW FRAME SECTION *****/ 

		/* Get current time and preserver last time for frame interval calc. */
		timebefore = timenow;
		gettimeofday(&tv1, NULL);
		timenow = tv1.tv_usec + 1000000L * tv1.tv_sec;

		/* since we don't have sanity checks done when options are set,
		 * this sanity check must go in the main loop :(, before pre_captures
		 * are attempted. */
		if (cnt->conf.minimum_motion_frames < 1)
			cnt->conf.minimum_motion_frames = 1;
		if (cnt->conf.pre_capture < 0)
			cnt->conf.pre_capture = 0;

		/* Check if our buffer is still the right size
		 * If pre_capture or minimum_motion_frames has been changed
		 * via the http remote control we need to re-size the ring buffer
		 */
		frame_buffer_size = cnt->conf.pre_capture + cnt->conf.minimum_motion_frames - 1;
		if (cnt->precap_nr != frame_buffer_size) {
			/* Only decrease if at last position in new buffer */
			if (frame_buffer_size > cnt->precap_nr || frame_buffer_size == cnt->precap_cur) {
				unsigned char *tmp;
				time_t *tmp2;
				int *tmp3;
				int smallest;
				smallest = (cnt->precap_nr < frame_buffer_size) ? cnt->precap_nr : frame_buffer_size;
				tmp=mymalloc(cnt->imgs.size*(1+frame_buffer_size));
				tmp2=mymalloc(sizeof(time_t)*(1+frame_buffer_size));
				tmp3=mymalloc(sizeof(int)*(1+frame_buffer_size));
				memcpy(tmp, cnt->imgs.image_ring_buffer, cnt->imgs.size * (1+smallest));
				memcpy(tmp2, cnt->imgs.timestamp, sizeof(time_t) * (1+smallest));
				memcpy(tmp3, cnt->imgs.shotstamp, sizeof(int) * (1+smallest));
				free(cnt->imgs.image_ring_buffer);
				free(cnt->imgs.timestamp);
				free(cnt->imgs.shotstamp);
				cnt->imgs.image_ring_buffer = tmp;
				cnt->imgs.timestamp = tmp2;
				cnt->imgs.shotstamp = tmp3;
				cnt->precap_nr = frame_buffer_size;
			}
		}

		/* Get time for current frame */
		cnt->currenttime = time(NULL);

		/* localtime returns static data and is not threadsafe
		 * so we use localtime_r which is reentrant and threadsafe
		 */
		localtime_r(&cnt->currenttime, cnt->currenttime_tm);

		/* If we have started on a new second we reset the shots variable
		 * lastrate is updated to be the number of the last frame. last rate
		 * is used as the ffmpeg framerate when motion is detected.
		 */
		if (lastframetime != cnt->currenttime) {
			cnt->lastrate = cnt->shots + 1;
			cnt->shots = -1;
			lastframetime = cnt->currenttime;
		}

		/* Increase the shots variable for each frame captured within this second */
		cnt->shots++;

		/* Store time with pre_captured image*/
		*(cnt->imgs.timestamp + cnt->precap_cur) = cnt->currenttime;

		/* Store shot number with pre_captured image*/
		*(cnt->imgs.shotstamp+cnt->precap_cur) = cnt->shots;

		/* newimg now points to the current image. With precap_cur incremented it
		 * will be pointing to the position in the buffer for the NEXT image frame
		 * not the current!!! So newimg points to current frame about to be loaded
		 * and the cnt->precap_cur already have been incremented to point to the
		 * next frame. 
		 */
		newimg = cnt->imgs.image_ring_buffer + (cnt->imgs.size * (cnt->precap_cur++));

		/* If we are at the end of the ring buffer go to the start */
		if (cnt->precap_cur > cnt->precap_nr)
			cnt->precap_cur=0;


	/***** MOTION LOOP - IMAGE CAPTURE SECTION *****/ 

		/* Fetch next frame from camera
		 * If video_next returns 0 all is well and we got a new picture
		 * Any non zero value is an error.
		 * 0 = OK, valid picture
		 * <0 = fatal error - leave the thread by breaking out of the main loop
		 * >0 = non fatal error - copy last image or show grey image with message
		 */
		 
		vid_return_code = cnt->video_ctxt->video_next(cnt, newimg);

		if (vid_return_code == 0) {
			/* If all is well reset missing_frame_counter */
			if (cnt->missing_frame_counter >= MISSING_FRAMES_TIMEOUT * cnt->conf.framerate) {
				/* If we previously logged starting a grey image, now log video re-start */
				motion_log(LOG_ERR, 0, "Video signal re-acquired");
				// event for re-acquired video signal can be called here
			}
			cnt->missing_frame_counter = 0;

#ifdef HAVE_FFMPEG
			/* Deinterlace the image with ffmpeg, before the image is modified. */
			if(cnt->conf.ffmpeg_deinterlace) {
				ffmpeg_deinterlace(newimg, cnt->imgs.width, cnt->imgs.height);
			}	
#endif			

			/* save the newly captured still virgin image to a buffer
			 * which we will not alter with text and location graphics
			 */
			memcpy(cnt->imgs.image_virgin, newimg, cnt->imgs.size);

			/* If the camera is a streaming netcam we let the camera decide
			 * the pace. Otherwise we will keep on adding duplicate frames.
			 * By resetting the timer the framerate becomes maximum the rate
			 * of the Netcam.
			 */
//			/* FIXME: This test needs to be changed */
//			if (cnt->conf.netcam_url) {
//				gettimeofday(&tv1, NULL);
//				timenow = tv1.tv_usec + 1000000L * tv1.tv_sec;
//			}
                } else {
                        if (vid_return_code < 0) {
				/* Fatal error - break out of main loop terminating thread */
                                motion_log(LOG_ERR, 0, "Video device fatal error - terminating camera thread");
                                break;
                        } else {
                                if (debug_level)
                                        motion_log(-1, 0, "vid_return_code %d", vid_return_code);
				/* Non fatal error */
				/* First missed frame - store timestamp */
                                if (!cnt->missing_frame_counter)
                                        cnt->connectionlosttime = cnt->currenttime;

				/* Increase missing_frame_counter
				 * The first MISSING_FRAMES_TIMEOUT seconds we copy previous virgin image
				 * After 30 seconds we put a grey error image in the buffer
				 * Note: at low_cpu the timeout will be longer but we live with that
				 */
                                if (++cnt->missing_frame_counter < (MISSING_FRAMES_TIMEOUT * cnt->conf.framerate)) {
                                        memcpy(newimg, cnt->imgs.image_virgin, cnt->imgs.size);

                                } else {
                                        char tmpout[80];
                                        char tmpin[] = "CONNECTION TO CAMERA LOST\\nSINCE %Y-%m-%d %T";
                                        struct tm tmptime;
                                        localtime_r(&cnt->connectionlosttime, &tmptime);
                                        memset(newimg, 0x80, cnt->imgs.size);
                                        mystrftime(cnt, tmpout, sizeof(tmpout), tmpin, &tmptime, NULL, 0);
                                        draw_text(newimg, 10, 20 * text_size_factor, cnt->imgs.width,
                                                  tmpout, cnt->conf.text_double);

					/* Write error message only once */
                                        if (cnt->missing_frame_counter == MISSING_FRAMES_TIMEOUT * cnt->conf.framerate) {
                                                motion_log(LOG_ERR, 0, "Video signal lost - Adding grey image");
                                                // Event for lost video signal can be called from here
                                        }
                                }
                        }
                } 


	/***** MOTION LOOP - MOTION DETECTION SECTION *****/

		/* The actual motion detection takes place in the following
		 * diffs is the number of pixels detected as changed
		 * Make a differences picture in image_out
		 *
		 * alg_diff_standard is the slower full feature motion detection algorithm
		 * alg_diff first calls a fast detection algorithm which only looks at a
		 *   fraction of the pixels. If this detects possible motion alg_diff_standard
		 *   is called.
		 */
                if (cnt->threshold && !cnt->pause) {
			/* if we've already detected motion and we want to see if there's
			 * still motion, don't bother trying the fast one first. IF there's
			 * motion, the alg_diff will trigger alg_diff_standard
			 * anyway
			 */
                        if (detecting_motion || cnt->conf.setup_mode)
                                cnt->diffs = alg_diff_standard(cnt, cnt->imgs.image_virgin);
                        else
                                cnt->diffs = alg_diff(cnt, cnt->imgs.image_virgin);

			/* Lightswitch feature - has light intensity changed?
			 * This can happen due to change of light conditions or due to a sudden change of the camera
			 * sensitivity. If alg_lightswitch detects lightswitch we suspend motion detection the next
			 * 5 frames to allow the camera to settle.
			 */
                        if (cnt->conf.lightswitch) {
                                if (alg_lightswitch(cnt, cnt->diffs)) {
                                        if (cnt->conf.setup_mode)
                                                motion_log(-1, 0, "Lightswitch detected");
                                        if (cnt->moved < 5)
                                                cnt->moved = 5;
                                        cnt->diffs = 0;
                                }
                        }

			/* Switchfilter feature tries to detect a change in the video signal
			 * from one camera to the next. This is normally used in the Round
			 * Robin feature. The algoritm is not very safe.
			 * The algoritm takes a little time so we only call it when needed
			 * ie. when feature is enabled and diffs>threshold.
			 * We do not suspend motion detection like we did for lightswith 
			 * because with Round Robin this is controlled by roundrobin_skip.
			 */
                        if (cnt->conf.switchfilter && cnt->diffs > cnt->threshold) {
                                cnt->diffs = alg_switchfilter(cnt, cnt->diffs, newimg);
                                if (cnt->diffs <= cnt->threshold) {
                                        cnt->diffs = 0;
                                        if (cnt->conf.setup_mode)
                                                motion_log(-1, 0, "Switchfilter detected");
                                }
                        }

			/* Despeckle feature
			 * First we run (as given by the despeckle option iterations
			 * of erode and dilate algoritms.
			 * Finally we run the labelling feature.
			 * All this is done in the alg_despeckle code.
			 */
                        cnt->imgs.total_labels = 0;
                        cnt->imgs.largest_label = 0;
                        olddiffs = 0;
                        if (cnt->conf.despeckle && cnt->diffs > 0) {
                                olddiffs = cnt->diffs;
                                cnt->diffs = alg_despeckle(cnt, olddiffs);
                        }
                } else if (!cnt->conf.setup_mode)
                        cnt->diffs = 0;

		/* Manipulate smart_mask sensitivity (only every smartmask_ratio seconds) */
                if (cnt->smartmask_speed) {
                        if (!--smartmask_count){
                                alg_tune_smartmask(cnt);
                                smartmask_count = smartmask_ratio;
                        }
                }

		/* cnt->moved is set by the tracking code when camera has been asked to move.
		 * When camera is moving we do not want motion to detect motion or we will
		 * get our camera chasing itself like crazy and we will get motion detected
		 * which is not really motion. So we pretend there is no motion by setting
		 * cnt->diffs = 0.
		 * We also pretend to have a moving camera when we start Motion and when light
		 * switch has been detected to allow camera to settle.
		 */
                if (cnt->moved) {
                        cnt->moved--;
                        cnt->diffs = 0;
                }


	/***** MOTION LOOP - TUNING SECTION *****/

		/* if noise tuning was selected, do it now. but only when
		 * no frames have been recorded and only once per second
		 */
                if (cnt->conf.noise_tune && cnt->shots == 0) {
                        if (!detecting_motion && (cnt->diffs <= cnt->threshold))
                                alg_noise_tune(cnt, cnt->imgs.image_virgin);
                }

		/* if we are not noise tuning lets make sure that remote controlled
		 * changes of noise_level are used.
		 */
                if (!cnt->conf.noise_tune)
                        cnt->noise_level = cnt->conf.noise_level;

		/* threshold tuning if enabled 
		 * if we are not threshold tuning lets make sure that remote controlled
		 * changes of threshold are used.
		 */
                if (cnt->conf.threshold_tune)
                        alg_threshold_tune(cnt, cnt->diffs, detecting_motion);
                else
                        cnt->threshold = cnt->conf.threshold;


	/***** MOTION LOOP - TEXT AND GRAPHICS OVERLAY SECTION *****/

		/* Some overlays on top of the motion image
		 * Note that these now modifies the cnt->imgs.out so this buffer
		 * can no longer be used for motion detection features until next
		 * picture frame is captured.
		 */
                 
		/* Fixed mask overlay */
                if (cnt->imgs.mask && (cnt->conf.output_normal || cnt->conf.ffmpeg_cap_motion || cnt->conf.setup_mode) )
                        overlay_fixed_mask(cnt, cnt->imgs.out);

		/* Smartmask overlay */
                if (cnt->smartmask_speed && (cnt->conf.output_normal || cnt->conf.ffmpeg_cap_motion || cnt->conf.setup_mode) )
                        overlay_smartmask(cnt, cnt->imgs.out);

		/* Largest labels overlay */
                if (cnt->imgs.largest_label && (cnt->conf.output_normal || cnt->conf.ffmpeg_cap_motion || cnt->conf.setup_mode) )
                        overlay_largest_label(cnt, cnt->imgs.out);

		/* If motion is detected (cnt->diffs > cnt->threshold) and before we add text to the pictures
		   we find the center and size coordinates of the motion to be used for text overlays and later
		   for adding the locate rectangle */
                if (cnt->diffs > cnt->threshold)
                         alg_locate_center_size(&cnt->imgs, cnt->imgs.width, cnt->imgs.height, &cnt->location);

		/* Initialize the double sized characters if needed. */
                if(cnt->conf.text_double && text_size_factor == 1)
                        text_size_factor = 2;

		/* If text_double is set to off, then reset the scaling text_size_factor. */
                else if(!cnt->conf.text_double && text_size_factor == 2) {
                        text_size_factor = 1;
                }

		/* Add changed pixels in upper right corner of the pictures */
                if (cnt->conf.text_changes) {
                        char tmp[15];
                        sprintf(tmp, "%d", cnt->diffs);
                        draw_text(newimg, cnt->imgs.width - 10, 10, cnt->imgs.width, tmp, cnt->conf.text_double);
                }

		/* Add changed pixels to motion-images (for webcam) in setup_mode
		   and always overlay smartmask (not only when motion is detected) */
                if (cnt->conf.setup_mode) {
                        char tmp[PATH_MAX];
                        sprintf(tmp, "D:%5d L:%3d N:%3d", cnt->diffs, cnt->imgs.total_labels, cnt->noise_level);
                        draw_text(cnt->imgs.out, cnt->imgs.width - 10, cnt->imgs.height - 30 * text_size_factor,
                                  cnt->imgs.width, tmp, cnt->conf.text_double);
                        sprintf(tmp, "THREAD %d SETUP", cnt->threadnr);
                        draw_text(cnt->imgs.out, cnt->imgs.width - 10, cnt->imgs.height - 10 * text_size_factor,
                                  cnt->imgs.width, tmp, cnt->conf.text_double);
                }

		/* Add text in lower left corner of the pictures */
                if (cnt->conf.text_left) {
                        char tmp[PATH_MAX];
                        mystrftime(cnt, tmp, sizeof(tmp), cnt->conf.text_left, cnt->currenttime_tm, NULL, 0);
                        draw_text(newimg, 10, cnt->imgs.height - 10 * text_size_factor, cnt->imgs.width,
                                  tmp, cnt->conf.text_double);
                }

		/* Add text in lower right corner of the pictures */
                if (cnt->conf.text_right) {
                        char tmp[PATH_MAX];
                        mystrftime(cnt, tmp, sizeof(tmp), cnt->conf.text_right, cnt->currenttime_tm, NULL, 0);
                        draw_text(newimg, cnt->imgs.width - 10, cnt->imgs.height - 10 * text_size_factor,
                                  cnt->imgs.width, tmp, cnt->conf.text_double);
                }


	/***** MOTION LOOP - ACTIONS AND EVENT CONTROL SECTION *****/

		/* If motion has been detected we take action and start saving
		 * pictures and movies etc by calling motion_detected().
		 * Is output_all enabled we always call motion_detected()
		 * If post_capture is enabled we also take care of this in the this
		 * code section.
		 */
                if (cnt->conf.output_all) {
                        detecting_motion = 1;
                        motion_detected(cnt, 0, cnt->video_dev, newimg);
                } else if (cnt->diffs > cnt->threshold) {
			/* Did we detect motion (like the cat just walked in :) )?
			 * If so, ensure the motion is sustained if minimum_motion_frames
			 * is set, and take action by calling motion_detected().
			 * pre_capture is handled by motion_detected(), and we handle
			 * post_capture here. */
                        if (!detecting_motion)
                                detecting_motion = 1;

                        detecting_motion++;

                        if (detecting_motion > cnt->conf.minimum_motion_frames) {
                                motion_detected(cnt, cnt->diffs, cnt->video_dev, newimg);
                                postcap = cnt->conf.post_capture;
                        }
                } else if (postcap) {
                        motion_detected(cnt, 0, cnt->video_dev, newimg);
                        postcap--;
                } else {
                        detecting_motion = 0;
                }

		/* Is the mpeg movie to long? Then make movies
		 * First test for max mpegtime
		 */
                if (cnt->conf.max_mpeg_time && cnt->event_nr == cnt->prev_event)
                        if (cnt->currenttime - cnt->eventtime >= cnt->conf.max_mpeg_time)
                                cnt->makemovie = 1;

		/* Now test for quiet longer than 'gap' OR make movie as decided in
		 * previous statement.
		 */
                if (((cnt->currenttime - cnt->lasttime >= cnt->conf.gap) && cnt->conf.gap > 0) || cnt->makemovie) {
                        if (cnt->event_nr == cnt->prev_event || cnt->makemovie) {

				/* When output_normal=best save best preview_shot here at the end of event */
                                if (cnt->new_img == NEWIMG_BEST && cnt->preview_max) {
                                        preview_best(cnt);
                                        cnt->preview_max = 0;
                                }

                                event(cnt, EVENT_ENDMOTION, NULL, NULL, NULL, cnt->currenttime_tm);

				/* if tracking is enabled we center our camera so it does not
				 * point to a place where it will miss the next action
				 */
                                if (cnt->track.type)
                                        cnt->moved = track_center(cnt, cnt->video_dev, 0, 0, 0);

                                if (cnt->conf.setup_mode)
                                        motion_log(-1, 0, "End of event %d", cnt->event_nr);

                                cnt->makemovie = 0;

				/* Finally we increase the event number */
                                cnt->event_nr++;

				/* And we unset the text_event_string to avoid that buffered
				 * images get a timestamp from previous event.
				 */
                                cnt->text_event_string[0] = '\0';
                        }
                }


	/***** MOTION LOOP - REFERENCE FRAME SECTION *****/

		/* Update reference frame */
                if (cnt->moved && (cnt->track.type || cnt->conf.lightswitch)) {
			/* Prevent the motion created by moving camera or sudden light intensity
		 	 * being detected by creating a fresh reference frame
		 	 */
                        memcpy(cnt->imgs.ref, cnt->imgs.image_virgin, cnt->imgs.size);
                } else if (cnt->threshold) {
			/* Old image slowly decays, this will make it even harder on
		 	 * a slow moving object to stay undetected
		 	 */
                        unsigned char *imgs_ref_ptr = cnt->imgs.ref;
                        unsigned char *newimg_ptr = cnt->imgs.image_virgin;
                        for (i=cnt->imgs.size-1; i>=0; i--) {
                                *imgs_ref_ptr = (*imgs_ref_ptr + *newimg_ptr)/2;
                                imgs_ref_ptr++;
                                newimg_ptr++;
                        }
                }


	/***** MOTION LOOP - SETUP MODE CONSOLE OUTPUT SECTION *****/

		/* If setup_mode enabled output some numbers to console */
                if (cnt->conf.setup_mode){
                        char msg[1024] = "\0";
                        char part[100];

                        if (cnt->conf.despeckle) {
                                snprintf(part, 99, "Raw changes: %5d - changes after '%s': %5d",
                                         olddiffs, cnt->conf.despeckle, cnt->diffs);
                                strcat(msg, part);
                                if (strchr(cnt->conf.despeckle, 'l')){
                                        sprintf(part, " - labels: %3d", cnt->imgs.total_labels);
                                        strcat(msg, part);
                                }
                        }
                        else{
                                sprintf(part, "Changes: %5d", cnt->diffs);
                                strcat(msg, part);
                        }
                        if (cnt->conf.noise_tune){
                                sprintf(part, " - noise level: %2d", cnt->noise_level);
                                strcat(msg, part);
                        }
                        if (cnt->conf.threshold_tune){
                                sprintf(part, " - threshold: %d", cnt->threshold);
                                strcat(msg, part);
                        }
                        motion_log(-1, 0, "%s", msg);
                }


	/***** MOTION LOOP - SNAPSHOT FEATURE SECTION *****/

		/* Did we get triggered to make a snapshot from control http? Then shoot a snap
		 * If snapshot_interval is not zero and time since epoch MOD snapshot_interval = 0 then snap
		 * Note: Negative value means SIGALRM snaps are enabled
		 * httpd-control snaps are always enabled.
		 */
                if ( (cnt->conf.snapshot_interval > 0 && cnt->shots==0 &&
                      cnt->currenttime % cnt->conf.snapshot_interval == 0) ||
                    cnt->snapshot) {
                        event(cnt, EVENT_IMAGE_SNAPSHOT, newimg, NULL, NULL, cnt->currenttime_tm);
                        cnt->snapshot = 0;
                }


	/***** MOTION LOOP - TIMELAPSE FEATURE SECTION *****/

#ifdef HAVE_FFMPEG

                time_current_frame = cnt->currenttime;

                if (cnt->conf.ffmpeg_timelapse) {

			/* Check to see if we should start a new timelapse file. We start one when 
			 * we are on the first shot, and and the seconds are zero. We must use the seconds
			 * to prevent the timelapse file from getting reset multiple times during the minute.
			 */ 
                        if (cnt->currenttime_tm->tm_min == 0 && 
                            (time_current_frame % 60 < time_last_frame % 60) &&
                            cnt->shots == 0) {

                                if (strcasecmp(cnt->conf.ffmpeg_timelapse_mode,"manual") == 0)
                                ;/* No action */ 

				/* If we are daily, raise timelapseend event at midnight */ 
                                else if (strcasecmp(cnt->conf.ffmpeg_timelapse_mode, "daily") == 0) {
                                        if (cnt->currenttime_tm->tm_hour == 0)
                                                event(cnt, EVENT_TIMELAPSEEND, NULL, NULL, NULL, cnt->currenttime_tm);
                                }

				/* handle the hourly case */ 	
                                else if (strcasecmp(cnt->conf.ffmpeg_timelapse_mode, "hourly") == 0) {
                                        event(cnt, EVENT_TIMELAPSEEND, NULL, NULL, NULL, cnt->currenttime_tm);
                                }

				/* If we are weekly-sunday, raise timelapseend event at midnight on sunday */ 
                                else if (strcasecmp(cnt->conf.ffmpeg_timelapse_mode, "weekly-sunday") == 0) {
                                        if (cnt->currenttime_tm->tm_wday == 0 && cnt->currenttime_tm->tm_hour == 0)
                                                event(cnt, EVENT_TIMELAPSEEND, NULL, NULL, NULL, cnt->currenttime_tm);
                                }

				/* If we are weekly-monday, raise timelapseend event at midnight on monday */ 
                                else if (strcasecmp(cnt->conf.ffmpeg_timelapse_mode, "weekly-monday") == 0) {
                                        if (cnt->currenttime_tm->tm_wday == 1 && cnt->currenttime_tm->tm_hour == 0)
                                                event(cnt, EVENT_TIMELAPSEEND, NULL, NULL, NULL, cnt->currenttime_tm);
                                }

				/* If we are monthly, raise timelapseend event at midnight on first day of month */
                                else if (strcasecmp(cnt->conf.ffmpeg_timelapse_mode, "monthly") == 0) {
                                        if (cnt->currenttime_tm->tm_mday == 1 && cnt->currenttime_tm->tm_hour == 0)
                                                event(cnt, EVENT_TIMELAPSEEND, NULL, NULL, NULL, cnt->currenttime_tm);
                                }

//FIXME:  Needs to be updated for Version 4
				/* If invalid we report in syslog once and continue in manual mode */
//				else {
//					motion_log(LOG_ERR, 0, "Invalid timelapse_mode argument '%s'",
//					           cnt->conf.ffmpeg_timelapse_mode);
//					motion_log(LOG_ERR, 0, "Defaulting to manual timelapse mode");
//			 	        conf_cmdparse(&cnt, (char *)"ffmpeg_timelapse_mode",(char *)"manual");
//				}
                        }

			/* If ffmpeg timelapse is enabled and time since epoch MOD ffmpeg_timelaps = 0
			 * add a timelapse frame to the timelapse mpeg.
			 */
                        if (cnt->shots == 0 &&
                                time_current_frame % cnt->conf.ffmpeg_timelapse <= time_last_frame % cnt->conf.ffmpeg_timelapse)
                                event(cnt, EVENT_TIMELAPSE, newimg, NULL, NULL, cnt->currenttime_tm);
                }

		/* if timelapse mpeg is in progress but conf.ffmpeg_timelapse is zero then close timelapse file
		 * This is an important feature that allows manual roll-over of timelapse file using xmlrpc
		 * via a cron job
		 */
                else if (cnt->ffmpeg_timelapse)
                        event(cnt, EVENT_TIMELAPSEEND, NULL, NULL, NULL, cnt->currenttime_tm);

                time_last_frame = time_current_frame;

#endif /* HAVE_FFMPEG */


	/***** MOTION LOOP - VIDEO LOOPBACK SECTION *****/

		/* feed last image and motion image to video device pipes and the webcam clients
		 * In setup mode we send the special setup mode image to both webcam and vloopback pipe
		 * In normal mode we feed the latest image to vloopback device and we send
		 * the image to the webcam. We always send the first image in a second to the webcam.
		 * Other image are sent only when the config option webcam_motion is off
		 * The result is that with webcam_motion on the webcam stream is normally at the minimal
		 * 1 frame per second but the minute motion is detected the motion_detected() function
		 * sends all detected pictures to the webcam except the 1st per second which is already sent.
		 */
                if (cnt->conf.setup_mode) {
                        event(cnt, EVENT_IMAGE, cnt->imgs.out, NULL, &cnt->pipe, cnt->currenttime_tm);
                        event(cnt, EVENT_WEBCAM, cnt->imgs.out, NULL, NULL, cnt->currenttime_tm);
                } else {
                        event(cnt, EVENT_IMAGE, newimg, NULL, &cnt->pipe, cnt->currenttime_tm);
                        if (!cnt->conf.webcam_motion || cnt->shots == 1)
                                event(cnt, EVENT_WEBCAM, newimg, NULL, NULL, cnt->currenttime_tm);
                }

                event(cnt, EVENT_IMAGEM, cnt->imgs.out, NULL, &cnt->mpipe, cnt->currenttime_tm);


	/***** MOTION LOOP - ONCE PER SECOND PARAMETER UPDATE SECTION *****/

		/* Check for some config parameter changes but only every second */
                if (cnt->shots == 0){
                        if (strcasecmp(cnt->conf.output_normal, "on") == 0)
                                cnt->new_img=NEWIMG_ON;
                        else if (strcasecmp(cnt->conf.output_normal, "first") == 0)
                                cnt->new_img=NEWIMG_FIRST;
                        else if (strcasecmp(cnt->conf.output_normal, "best") == 0){
                                cnt->new_img=NEWIMG_BEST;
				/* alocate buffer here when not yet done */
                                if (!cnt->imgs.preview_buffer){
                                        cnt->imgs.preview_buffer = mymalloc(cnt->imgs.size);
                                        if (cnt->conf.setup_mode)
                                                motion_log(-1, 0, "Preview buffer allocated");
                                }
                        }
                        else
                                cnt->new_img = NEWIMG_OFF;

                        if (strcasecmp(cnt->conf.locate, "on") == 0)
                                cnt->locate = LOCATE_ON;
                        else if (strcasecmp(cnt->conf.locate, "preview") == 0)
                                cnt->locate = LOCATE_PREVIEW;
                        else
                                cnt->locate = LOCATE_OFF;

			/* Sanity check for smart_mask_speed, silly value disables smart mask */
                        if (cnt->conf.smart_mask_speed < 0 || cnt->conf.smart_mask_speed > 10)
                                cnt->conf.smart_mask_speed = 0;
			/* Has someone changed smart_mask_speed or framerate? */
                        if (cnt->conf.smart_mask_speed != cnt->smartmask_speed || smartmask_lastrate != cnt->lastrate){
                                if (cnt->conf.smart_mask_speed == 0){
                                        memset(cnt->imgs.smartmask, 0, cnt->imgs.motionsize);
                                        memset(cnt->imgs.smartmask_final, 255, cnt->imgs.motionsize);
                                }
                                smartmask_lastrate = cnt->lastrate;
                                cnt->smartmask_speed = cnt->conf.smart_mask_speed;
				/* Decay delay - based on smart_mask_speed (framerate independent)
				   This is always 5*smartmask_speed seconds */
                                smartmask_ratio = 5 * cnt->lastrate * (11 - cnt->smartmask_speed);
                        }

#if defined(HAVE_MYSQL) || defined(HAVE_PGSQL)
			/* Set the sql mask file according to the SQL config options
			 * We update it for every frame in case the config was updated
			 * via remote control.
			 */
                        cnt->sql_mask = cnt->conf.sql_log_image * (FTYPE_IMAGE + FTYPE_IMAGE_MOTION) +
                                        cnt->conf.sql_log_snapshot * FTYPE_IMAGE_SNAPSHOT +
                                        cnt->conf.sql_log_mpeg * (FTYPE_MPEG + FTYPE_MPEG_MOTION) +
                                        cnt->conf.sql_log_timelapse * FTYPE_MPEG_TIMELAPSE;
#endif /* defined(HAVE_MYSQL) || defined(HAVE_PGSQL) */	

                }


	/***** MOTION LOOP - FRAMERATE TIMING AND SLEEPING SECTION *****/

		/* Work out expected frame rate based on config setting which may
		   have changed from http-control */
                if (cnt->conf.framerate)
                        required_frame_time = 1000000L / cnt->conf.framerate;
                else
                        required_frame_time = 0;

		/* Get latest time to calculate time taken to process video data */
                gettimeofday(&tv2, NULL);
                elapsedtime = (tv2.tv_usec + 1000000L * tv2.tv_sec) - timenow;

		/* Update history buffer but ignore first pass as timebefore
		   variable will be inaccurate
		 */
                if (passflag)
                        rolling_average_data[rolling_frame] = timenow-timebefore;
                else
                        passflag = 1;

                rolling_frame++;
                if (rolling_frame >= rolling_average_limit)
                        rolling_frame = 0;

		/* Calculate 10 second average and use deviation in delay calculation */
                rolling_average = 0L;

                for (j=0; j < rolling_average_limit; j++)
                        rolling_average += rolling_average_data[j];

                rolling_average /= rolling_average_limit;
                frame_delay = required_frame_time-elapsedtime - (rolling_average - required_frame_time);

                if (frame_delay > 0) {
			/* Apply delay to meet frame time */
                        if (frame_delay > required_frame_time)
                                frame_delay = required_frame_time;

			/* Delay time in nanoseconds for SLEEP */
                        delay_time_nsec = frame_delay * 1000;

                        if (delay_time_nsec > 999999999)
                                delay_time_nsec = 999999999;

			/* SLEEP as defined in motion.h  A safe sleep using nanosleep */
                        SLEEP(0, delay_time_nsec);
                }

		/* This will limit the framerate to 1 frame while not detecting
		   motion. Using a different motion flag to allow for multiple frames per second
		 */

                if (cnt->conf.low_cpu && !detecting_motion) {
			/* Recalculate remaining time to delay for a total of 1/low_cpu seconds */
                        if (frame_delay + elapsedtime < (1000000L / cnt->conf.low_cpu)) {
                                frame_delay = (1000000L / cnt->conf.low_cpu) - frame_delay - elapsedtime;


				/* Delay time in nanoseconds for SLEEP */
                                delay_time_nsec = frame_delay * 1000;

                                if (delay_time_nsec > 999999999)
                                        delay_time_nsec = 999999999;

				/* SLEEP as defined in motion.h  A safe sleep using nanosleep */
                                SLEEP(0, delay_time_nsec);

				/* Correct frame times to ensure required_frame_time is maintained
				 * This is done by taking the time NOW and subtract the time
				 * required_frame_time. This way we pretend that timenow was set
				 * one frame ago.
				 */
                                gettimeofday(&tv1, NULL);
                                timenow = tv1.tv_usec + 1000000L * tv1.tv_sec - required_frame_time;
                        }
                }
	}

	/* END OF MOTION MAIN LOOP
	 * If code continues here it is because the thread is exiting or restarting
	 */

	if (cnt->video_ctxt && cnt->video_ctxt->video_dev_cleanup) {
		cnt->video_ctxt->video_dev_cleanup(cnt);
	}

	if (rolling_average_data)
		free(rolling_average_data);

	motion_log(-1, 0, "Thread exiting");
	if (!cnt->finish)
		motion_log(LOG_ERR, 1, "Somebody stole the video device, lets hope we got his picture");

	event(cnt, EVENT_STOP, NULL, NULL, NULL, NULL);

	pthread_mutex_lock(&global_lock);
	threads_running--;
	pthread_mutex_unlock(&global_lock);

	pthread_exit(NULL);
}

/**
 * become_daemon
 *
 *   Turns Motion into a daemon through forking. The parent process (i.e. the
 *   one initially calling this function) will exit inside this function, while
 *   control will be returned to the child process. Standard input/output are
 *   released properly, and the current directory is set to / in order to not
 *   lock up any file system.
 *
 * Parameters:
 *
 *   cnt - current thread's context struct
 *
 * Returns: nothing
 */
static void become_daemon(void)
{
	int i;
	struct sigaction sig_ign_action;

	/* Setup sig_ign_action */
#ifdef SA_RESTART
	sig_ign_action.sa_flags = SA_RESTART;
#else
	sig_ign_action.sa_flags = 0;
#endif
	sig_ign_action.sa_handler = SIG_IGN;
	sigemptyset(&sig_ign_action.sa_mask);

	if (fork()) {
		motion_log(-1, 0, "Motion going to daemon mode");
		exit(0);
	}

	/* changing dir to root enables people to unmount a disk
	   without having to stop Motion */
	if (chdir("/")) {
		motion_log(LOG_ERR, 1, "Could not change directory");
	}

#ifdef __freebsd__
	setpgrp(0, getpid());
#else
	setpgrp();
#endif /* __freebsd__ */

	if ((i = open("/dev/tty", O_RDWR)) >= 0) {
		ioctl(i, TIOCNOTTY, NULL);
		close(i);
	}

	setsid();
	i = open("/dev/null", O_RDONLY);

	if(i != -1) {
		dup2(i, STDIN_FILENO);
		close(i);
	}

	i = open("/dev/null", O_WRONLY);

	if(i != -1) {
		dup2(i, STDOUT_FILENO);
		dup2(i, STDERR_FILENO);
		close(i);
	}

	sigaction(SIGTTOU, &sig_ign_action, NULL);
	sigaction(SIGTTIN, &sig_ign_action, NULL);
	sigaction(SIGTSTP, &sig_ign_action, NULL);
}

/**
 * cntlist_create
 *
 *   Sets up the 'cnt_list' variable by allocating one context structure
 *   (our global configuration). Also loads the configuration from 
 *   the config file(s).
 *
 * Parameters:
 *   argc - size of argv
 *   argv - command-line options, passed initially from 'main'
 *
 * Returns: nothing
 */
static void cntlist_create(int argc, char *argv[])
{
motion_ctxt_ptr wcnt;
config_ctxt_ptr sect_chain_ptr;

	/* cnt_list is a chain of pointers to the context structures for each thread.
	 * We set the first pointer to thread 0's context structure
	 */
	cnt_list = mymalloc(sizeof(struct context));

	/* Populate context structure with start/default values */
	context_init(cnt_list, argc, argv);

	if (conf_load(cnt_list) < 0) {
		motion_log(LOG_ERR, 0, "Error trying to load configuration file");
		exit(1);
	}
	/* Now take care of the other sections. */
	wcnt = cnt_list;
	sect_chain_ptr = gconf_ctxt->next;		/* pointer to first section */
	while (sect_chain_ptr) {
		/* Is this a camera section? */
		if (!strcmp(sect_chain_ptr->node_name, "camera")) {
			/* Create a new context structure and add it to chain */
			wcnt->next = mymalloc(sizeof(struct context));
			wcnt = wcnt->next;
			context_init(wcnt, argc, argv);
			/* Copy global config values */
			memcpy(&wcnt->conf, &cnt_list->conf, sizeof(struct config));
			wcnt->next = NULL;
			/* Overwrite values unique to this section */
			set_config_values(wcnt, sect_chain_ptr, &wcnt->conf);
			/* Check if there is a video_ctxt in this section */
			if (!wcnt->video_ctxt) {
				/* if not, copy the global one */
				if (!cnt_list->video_ctxt) {
					motion_log(LOG_ERR, 0, "No video context defined for camera %s",
					                sect_chain_ptr->title);
					exit(1);
				}
				/* since it's static data, we can just copy it */
				wcnt->video_ctxt = cnt_list->video_ctxt;
				wcnt->motion_video_plugin = cnt_list->motion_video_plugin;
			}
			wcnt->video_vars = mymalloc(sizeof(video_plugin_ctxt));
			memset(wcnt->video_vars, 0, sizeof(video_plugin_ctxt));
			/* Call the applicable video init routine in the plugin */
			if (wcnt->video_ctxt->video_init) {
				/* Call the plugin's initialisation routine */
				wcnt->video_ctxt->video_init(wcnt);
				/* If there are plugin-unique config params, set them */
				if (set_ext_values(wcnt, sect_chain_ptr,
				                wcnt->motion_video_plugin->plugin_validate,
				                wcnt->motion_video_plugin->plugin_validate_numentries,
				                wcnt->video_vars->video_params)) {
					motion_log(LOG_ERR, 0, "Error setting video params");
				}
			}

		}
		sect_chain_ptr = sect_chain_ptr->next;
	}
}

/**
 * motion_shutdown
 *
 *   Responsible for performing cleanup when Motion is shut down or restarted,
 *   including freeing memory for all the context structs as well as for the
 *   context struct list itself.
 *
 * Parameters: none
 *
 * Returns:    nothing
 */
static void motion_shutdown(void)
{
	int i = -1;
	struct context *cnt = cnt_list;    /* ptr to global context */

	if (cnt->video_ctxt) {
		if (cnt->video_ctxt->video_close)
			cnt->video_ctxt->video_close(cnt);
		if (cnt->video_ctxt->video_cleanup)
			cnt->video_ctxt->video_cleanup(cnt);
	}
	while (cnt_list){
		cnt = cnt_list->next;
		context_destroy(cnt_list);
		cnt_list = cnt;
	}
	while (plugins_loaded) {
		motion_plugin_ptr pptr;
fprintf(stderr, "Cleaning up plugin %s\n", plugins_loaded->name);
                pptr = plugins_loaded->next;
                if (plugins_loaded->name)
                        free(plugins_loaded->name);
                if (plugins_loaded->handle) {
fprintf(stderr, "Calling plugin_close\n");
			/* note plugin_close frees the handle */
                        plugin_close(plugins_loaded->handle);
                }
                free(plugins_loaded);
                plugins_loaded = pptr;
	}
	if (gconf_ctxt)
		destroy_config_ctxt(gconf_ctxt);
}

/**
 * motion_startup
 *
 *   Responsible for initializing stuff when Motion starts up or is restarted,
 *   including daemon initialization and creating the context struct list.
 *
 * Parameters:
 *
 *   daemonize - non-zero to do daemon init (if the config parameters says so),
 *               or 0 to skip it
 *   argc      - size of argv
 *   argv      - command-line options, passed initially from 'main'
 *
 * Returns: nothing
 */
static void motion_startup(int daemonize, int argc, char *argv[])
{
	/* Initialize our global mutex */
	pthread_mutex_init(&global_lock, NULL);

	/* Create the list of context structures and load the
	 * configuration.  Also startup all the subsidiary threads.
	 */
	cntlist_create(argc, argv);

	initialize_chars();

	if (daemonize) {
		/* If daemon mode is requested, and we're not going into setup mode,
		 * become daemon.
		 */
		if (cnt_list->daemon && cnt_list->conf.setup_mode == 0) {
			become_daemon();
			motion_log(LOG_INFO, 0, "Motion running as daemon process");
			if (cnt_list->conf.low_cpu) {
				motion_log(LOG_INFO, 0, "Capturing %d frames/s when idle",
				           cnt_list->conf.low_cpu);
			}
		}
	}
}

/**
 * setup_signals
 *
 *   Attaches handlers to a number of signals that Motion need to catch.
 *
 * Parameters: none
 *
 * Returns:    nothing
 */
static void setup_signals(void)
{
	struct sigaction sigchild_action;
	struct sigaction sig_handler_action;

#ifdef SA_NOCLDWAIT
	sigchild_action.sa_flags = SA_NOCLDWAIT;
#else
	sigchild_action.sa_flags = 0;
#endif
	sigchild_action.sa_handler=sigchild_handler;
	sigemptyset(&sigchild_action.sa_mask);
#ifdef SA_RESTART
	sig_handler_action.sa_flags = SA_RESTART;
#else
	sig_handler_action.sa_flags = 0;
#endif
	sig_handler_action.sa_handler=sig_handler;
	sigemptyset(&sig_handler_action.sa_mask);

	/* Enable automatic zombie reaping */
	sigaction(SIGCHLD, &sigchild_action, NULL);
	sigaction(SIGPIPE, &sigchild_action, NULL);
	sigaction(SIGALRM, &sig_handler_action, NULL);
	sigaction(SIGHUP,  &sig_handler_action, NULL);
	sigaction(SIGINT,  &sig_handler_action, NULL);
	sigaction(SIGQUIT, &sig_handler_action, NULL);
	sigaction(SIGTERM, &sig_handler_action, NULL);
	sigaction(SIGUSR1, &sig_handler_action, NULL);
}

/**
 * main
 *
 *   Main entry point of Motion. Launches all the motion threads and contains
 *   the logic for starting up, restarting and cleaning up everything.
 *
 * Parameters:
 * 
 *   argc - size of argv
 *   argv - command-line options
 * 
 * Returns: Motion exit status = 0 always
 */
int main (int argc, char **argv)
{
	int i, j;
	int webcam_port;
	pthread_attr_t thread_attr;
	pthread_t thread_id;
	struct context *cnt, *cnt2;

	/*
	 * We set up the "thread local storage" variable which contains
	 * the thread number early in the code, because it's used within
	 * the motion_log routine, and any problems logged will use it.
	 *
	 * Create the TLS key for thread number.
	 */
	pthread_key_create(&tls_key_threadnr, NULL);
	pthread_setspecific(tls_key_threadnr, NULL); /* thread 0 */

	/* Setup signals and do some initialization. 1 in the call to 
	 * 'motion_startup' means that Motion will become a daemon if so has been
	 * requested, and argc and argc are necessary for reading the command
	 * line options.
	 */
	setup_signals();
	motion_startup(1, argc, argv);

#ifdef HAVE_FFMPEG
	/* FFMpeg initialization is only performed if FFMpeg support was found
	 * and not disabled during the configure phase.
	 */
	ffmpeg_init();
#endif /* HAVE_FFMPEG */

	/* In setup mode, Motion is very communicative towards the user, which 
	 * allows the user to experiment with the config parameters in order to
	 * optimize motion detection and stuff.
	 */
	if(cnt_list->conf.setup_mode)
		motion_log(-1, 0, "Motion running in setup mode.");

	/* Create a thread attribute for the threads we spawn later on.
	 * PTHREAD_CREATE_DETACHED means to create threads detached, i.e.
	 * their termination cannot be synchronized through 'pthread_join'.
	 */
	pthread_attr_init(&thread_attr);
	pthread_attr_setdetachstate(&thread_attr, PTHREAD_CREATE_DETACHED);

	do {
		if (restart) {
			/* Handle the restart situation. Currently the approach is to
			 * cleanup everything, and then initialize everything again
			 * (including re-reading the config file(s)).
			 */
			motion_shutdown();
			restart = 0; /* only one reset for now */
			SLEEP(5,0); // maybe some cameras needs less time
			motion_startup(0, argc, argv); /* 0 = skip daemon init */
		}

		/* Check the webcam port number for conflicts.
		 * First we check for conflict with the control port.
		 * Second we check for that two threads does not use the same port number
		 * for the webcam. If a duplicate port is found the webcam feature gets disabled (port =0)
		 * for this thread and a warning is written to console and syslog.
		 */
		cnt = cnt_list->next;
		while (cnt) {
			/* Get the webcam port for thread 'i', may be 0. */
			webcam_port = cnt->conf.webcam_port;

			if (cnt_list->conf.setup_mode)
				motion_log(LOG_ERR, 0, "Webcam port %d", webcam_port);

			/* Compare against the control port. */
			if (cnt_list->conf.control_port == webcam_port && webcam_port != 0) {
				cnt->conf.webcam_port = 0;
				motion_log(LOG_ERR, 0,
				           "Webcam port number %d for thread %d conflicts with the control port",
				           webcam_port, cnt->threadnr);
				motion_log(LOG_ERR, 0, "Webcam feature for thread %d is disabled.", cnt->threadnr);
			}

			/* Compare against webcam ports of other threads. */
			cnt2 = cnt->next;
			while (cnt2) {
				if (cnt2->conf.webcam_port == webcam_port && webcam_port != 0) {
					cnt2->conf.webcam_port = 0;
					motion_log(LOG_ERR, 0,
					           "Webcam port number %d for thread %d conflicts with thread %d",
					           webcam_port, cnt2->threadnr, cnt->threadnr);
					motion_log(LOG_ERR, 0,
					           "Webcam feature for thread %d is disabled.", cnt2->threadnr);
				}
				cnt2 = cnt2->next;
			}
			cnt = cnt->next;
		}

		/* Start the motion threads. First 'cnt_list' item is global if 'thread'
		 * option is used, so start at 1 then and 0 otherwise.
		 */
		cnt = cnt_list;
		if (cnt->next)
			cnt = cnt->next;
		while (cnt) {
//	FIXME: This test needs to be changed
//			if (cnt_list->conf.setup_mode)
//				motion_log(-1, 0, "Thread device: %s input %d",
//				           cnt->conf.netcam_url ? cnt->conf.netcam_url : cnt->conf.video_device,
//				           cnt->conf.netcam_url ? -1 : cnt->conf.input
//				          );

			/* Assign the thread number for this thread. This is done within a
			 * mutex lock to prevent multiple simultaneous updates to 
			 * 'threads_running'.
			 */
                        pthread_mutex_lock(&global_lock);
                        cnt->threadnr = ++threads_running;
                        pthread_mutex_unlock(&global_lock);

                        if ( strcmp(cnt->conf_filename,"") )
                                motion_log(LOG_INFO, 0, "Thread is from %s", cnt->conf_filename );  

			/* Create the actual thread. Use 'motion_loop' as the thread
			 * function.
			 */
                        pthread_create(&thread_id, &thread_attr, &motion_loop, cnt);
                        cnt = cnt->next;
                }

#if 0    /* Disabled until webhttpd can be updated for Version 4 */	
		/* Create a thread for the control interface if requested. Create it
		 * detached and with 'motion_web_control' as the thread function.
		 */
                if (cnt_list->conf.control_port)
                        pthread_create(&thread_id, &thread_attr, &motion_web_control, cnt_list);
#endif

                if (cnt_list->conf.setup_mode)
                        motion_log(-1, 0,"Waiting for threads to finish, pid: %d", getpid());

		/* Crude way of waiting for all threads to finish - check the thread
		 * counter (because we cannot do join on the detached threads).
		 */
                while(threads_running > 0) {
                        SLEEP(1,0);
                }

                if (cnt_list->conf.setup_mode)
                        motion_log(LOG_DEBUG, 0, "Threads finished");

		/* Rest for a while if we're supposed to restart. */
                if (restart)
                        SLEEP(2,0);

	} while (restart); /* loop if we're supposed to restart */

	motion_log(LOG_INFO, 0, "Motion terminating");

	/* Perform final cleanup. */
	pthread_key_delete(tls_key_threadnr);
	pthread_attr_destroy(&thread_attr);
	pthread_mutex_destroy(&global_lock);
	motion_shutdown();

	return 0;
}

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

