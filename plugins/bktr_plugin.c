/*	bktr_plugin.c	
 *
 *	BSD Video stream functions for motion.
 *	Copyright 2006 by Angel Carpintero (ack@telefonica.net)
 *	This software is distributed under the GNU public license version 2
 *	See also the file 'COPYING'.
 *
 */

/* Common stuff: */
#include "motion.h"
#include "bktr_plugin.h"
#include "plugins.h"

#include "newconfig.h"
/* for rotation */
#include "rotate.h"

#define BKTR_PARAM ((bktr_config_ptr)cnt->video_vars->video_params)

#include "bktr_params.h"

/* for the bktr stuff: */
#include <sys/mman.h>
#include <sys/types.h>

/* Hack from xawtv 4.x */

#define VIDEO_NONE           0
#define VIDEO_RGB08          1  /* bt848 dithered */
#define VIDEO_GRAY           2
#define VIDEO_RGB15_LE       3  /* 15 bpp little endian */
#define VIDEO_RGB16_LE       4  /* 16 bpp little endian */
#define VIDEO_RGB15_BE       5  /* 15 bpp big endian */
#define VIDEO_RGB16_BE       6  /* 16 bpp big endian */
#define VIDEO_BGR24          7  /* bgrbgrbgrbgr (LE) */
#define VIDEO_BGR32          8  /* bgr-bgr-bgr- (LE) */
#define VIDEO_RGB24          9  /* rgbrgbrgbrgb (BE) */
#define VIDEO_RGB32         10  /* -rgb-rgb-rgb (BE) */
#define VIDEO_LUT2          11  /* lookup-table 2 byte depth */
#define VIDEO_LUT4          12  /* lookup-table 4 byte depth */
#define VIDEO_YUYV          13  /* 4:2:2 */
#define VIDEO_YUV422P       14  /* YUV 4:2:2 (planar) */
#define VIDEO_YUV420P       15  /* YUV 4:2:0 (planar) */
#define VIDEO_MJPEG         16  /* MJPEG (AVI) */
#define VIDEO_JPEG          17  /* JPEG (JFIF) */
#define VIDEO_UYVY          18  /* 4:2:2 */
#define VIDEO_MPEG          19  /* MPEG1/2 */
#define VIDEO_FMT_COUNT     20

#define MAX(x,y) ( (x) > (y) ? (x) : (y) )
#define MIN(x,y) ( (x) < (y) ? (x) : (y) )
#define array_elem(x) (sizeof(x) / sizeof( (x)[0] ))

static const struct camparam_st {
  int min, max, range, drv_min, drv_range, def;
} CamParams[] = {
  {
    BT848_BRIGHTMIN, BT848_BRIGHTMIN + BT848_BRIGHTRANGE,
    BT848_BRIGHTRANGE, BT848_BRIGHTREGMIN,
    BT848_BRIGHTREGMAX - BT848_BRIGHTREGMIN + 1, BT848_BRIGHTCENTER, },
  {
    BT848_CONTRASTMIN, (BT848_CONTRASTMIN + BT848_CONTRASTRANGE),
    BT848_CONTRASTRANGE, BT848_CONTRASTREGMIN,
    (BT848_CONTRASTREGMAX - BT848_CONTRASTREGMIN + 1),
    BT848_CONTRASTCENTER, },
  {
    BT848_CHROMAMIN, (BT848_CHROMAMIN + BT848_CHROMARANGE), BT848_CHROMARANGE,
    BT848_CHROMAREGMIN, (BT848_CHROMAREGMAX - BT848_CHROMAREGMIN + 1 ),
    BT848_CHROMACENTER, },
};

#define BRIGHT 0
#define CONTR  1
#define CHROMA 2

/* Not tested yet */

static void yuv422to420p(unsigned char *map, unsigned char *cap_map, int width, int height)
{
	unsigned char *src, *dest, *src2, *dest2;
	int i, j;

	/* Create the Y plane */
	src=cap_map;
	dest=map;
	for (i=width*height; i; i--) {
		*dest++=*src;
		src+=2;
	}
	/* Create U and V planes */
	src=cap_map+1;
	src2=cap_map+width*2+1;
	dest=map+width*height;
	dest2=dest+(width*height)/4;
	for (i=height/2; i; i--) {
		for (j=width/2; j; j--) {
			*dest=((int)*src+(int)*src2)/2;
			src+=2;
			src2+=2;
			dest++;
			*dest2=((int)*src+(int)*src2)/2;
			src+=2;
			src2+=2;
			dest2++;
		}
		src+=width*2;
		src2+=width*2;
	}

}

/* FIXME seems no work with METEOR_GEO_RGB24 , check BPP as well ? */

static void rgb24toyuv420p(unsigned char *map, unsigned char *cap_map, int width, int height)
{
	unsigned char *y, *u, *v;
	unsigned char *r, *g, *b;
	int i, loop;

	b=cap_map;
	g=b+1;
	r=g+1;
	y=map;
	u=y+width*height;
	v=u+(width*height)/4;
	memset(u, 0, width*height/4);
	memset(v, 0, width*height/4);

	for(loop=0; loop<height; loop++) {
		for(i=0; i<width; i+=2) {
			*y++=(9796**r+19235**g+3736**b)>>15;
			*u+=((-4784**r-9437**g+14221**b)>>17)+32;
			*v+=((20218**r-16941**g-3277**b)>>17)+32;
			r+=3;
			g+=3;
			b+=3;
			*y++=(9796**r+19235**g+3736**b)>>15;
			*u+=((-4784**r-9437**g+14221**b)>>17)+32;
			*v+=((20218**r-16941**g-3277**b)>>17)+32;
			r+=3;
			g+=3;
			b+=3;
			u++;
			v++;
		}

		if ((loop & 1) == 0) 
		{
			u-=width/2;
			v-=width/2;
		}
	}

}


/*********************************************************************************************
                CAPTURE CARD STUFF
**********************************************************************************************/
/* NOT TESTED YET FIXME */

/*
static int camparam_normalize( int param, int cfg_value, int *ioctl_val ) 
{
	int val;

	cfg_value = MIN( CamParams[ param ].max, MAX( CamParams[ param ].min, cfg_value ) ); 
	val = (cfg_value - CamParams[ param ].min ) /
	      (CamParams[ param ].range + 0.01) * CamParams[param].drv_range + CamParams[param].drv_min;
	val = MAX( CamParams[ param ].min,
	      MIN( CamParams[ param ].drv_min + CamParams[ param ].drv_range-1,val ));
	*ioctl_val = val;
	return cfg_value;
}
*/

static int set_hue(struct context *cnt, int viddev, int new_hue )
{
	signed char ioctlval=new_hue;

	if( ioctl( viddev, METEORSHUE, &ioctlval ) < 0 ) {
                cnt->callbacks->cnt->callbacks->motion_log(LOG_ERR, 1, "METEORSHUE Error setting hue [%d]",new_hue);
                return -1;
        }

	cnt->callbacks->motion_log(-1, 0, "set hue to [%d]",ioctlval);

	return ioctlval;
}

static int get_hue(struct context *cnt, int viddev , int *hue)
{
	signed char ioctlval;

	if( ioctl( viddev, METEORGHUE, &ioctlval ) < 0 ) {
		cnt->callbacks->cnt->callbacks->motion_log(LOG_ERR, 1, "METEORGHUE Error getting hue");
		return -1;
	}

	*hue = ioctlval; 
	return ioctlval;
}

static int set_saturation(struct context *cnt, int viddev, int new_saturation ) 
{
	unsigned char ioctlval= new_saturation;

	if( ioctl( viddev, METEORSCSAT, &ioctlval ) < 0 ) {
		cnt->callbacks->cnt->callbacks->motion_log(LOG_ERR, 1, "METEORSCSAT Error setting saturation [%d]",new_saturation);
		return -1;
	}

	cnt->callbacks->cnt->callbacks->motion_log(-1, 0, "set saturation to [%d]",ioctlval);

	return ioctlval;
}

static int get_saturation(struct context *cnt, int viddev , int *saturation)
{
	unsigned char ioctlval;

	if( ioctl( viddev, METEORGCSAT, &ioctlval ) < 0 ) {

		cnt->callbacks->cnt->callbacks->motion_log(LOG_ERR, 1, "METEORGCSAT Error getting saturation");
		return -1;
	}

	*saturation = ioctlval;
	return ioctlval;
}

static int set_contrast(struct context *cnt, int viddev, int new_contrast ) 
{
	unsigned char ioctlval = new_contrast;

	if( ioctl( viddev, METEORSCONT, &ioctlval ) < 0 ) {
		cnt->callbacks->cnt->callbacks->motion_log(LOG_ERR, 1, "METEORSCONT Error setting contrast [%d]", new_contrast);
		return 0;
	}

	cnt->callbacks->cnt->callbacks->motion_log(-1, 0, "set contrast to [%d]",ioctlval);

	return ioctlval;
}

static int get_contrast(struct context *cnt, int viddev, int *contrast )
{
	unsigned char ioctlval;

	if( ioctl (viddev, METEORGCONT, &ioctlval ) < 0 ) {
		cnt->callbacks->cnt->callbacks->motion_log(LOG_ERR, 1, "METEORGCONT Error getting contrast");
		return -1;
	}

	*contrast = ioctlval; 
	return ioctlval;
}


static int set_brightness(struct context *cnt, int viddev, int new_bright )
{
	unsigned char ioctlval = new_bright;

	if( ioctl( viddev, METEORSBRIG, &ioctlval ) < 0 ) {
		cnt->callbacks->cnt->callbacks->motion_log(LOG_ERR, 1, "METEORSBRIG  brightness [%d]",new_bright);
		return -1;
	}

	cnt->callbacks->cnt->callbacks->motion_log(-1, 0, "set brightness to [%d]",ioctlval);
	
	return ioctlval;
}


static int get_brightness(struct context *cnt, int viddev, int *brightness )
{
	unsigned char ioctlval;

	if( ioctl( viddev, METEORGBRIG, &ioctlval ) < 0 ) {
                cnt->callbacks->cnt->callbacks->motion_log(LOG_ERR, 1, "METEORGBRIG  getting brightness");
                return -1;
        }

	*brightness = ioctlval;
	return ioctlval;
}

// Set channel needed ? FIXME 
/*
static int set_channel( struct video_dev *viddev, int new_channel ) 
{
	int ioctlval;

	ioctlval = new_channel;
	if( ioctl( viddev->fd_tuner, TVTUNER_SETCHNL, &ioctlval ) < 0 ) {
		cnt->callbacks->motion_log(LOG_ERR, 1, "Error channel %d",ioctlval);
		return -1;
	} else {
		cnt->callbacks->motion_log(LOG_DEBUG, 0, "channel set to %d",ioctlval);
	}

	viddev->channel = new_channel;
 
	return 0;
}
*/

/* set frequency to tuner */

static int set_freq(struct context *cnt, struct video_dev *viddev, unsigned long freq)
{
	int tuner_fd = viddev->fd_tuner;
	int old_audio;
 
	/* HACK maybe not need it , but seems that is needed to mute before changing frequency */

	if ( ioctl( tuner_fd, BT848_GAUDIO, &old_audio ) < 0 ) {
		cnt->callbacks->cnt->callbacks->motion_log(LOG_ERR, 1, "BT848_GAUDIO");
		return -1;
	}
	
	if (ioctl(tuner_fd, TVTUNER_SETFREQ, &freq) < 0){
		cnt->callbacks->motion_log(LOG_ERR, 1, "Tuning (TVTUNER_SETFREQ) failed , freq [%lu]",freq);
		return -1;
	}

	old_audio &= AUDIO_MUTE;
	if ( old_audio ){
		old_audio = AUDIO_MUTE;
		if ( ioctl(tuner_fd , BT848_SAUDIO, &old_audio ) < 0 ) {
			cnt->callbacks->motion_log(LOG_ERR, 1, "BT848_SAUDIO %i",old_audio);
			return -1;
		}
	}
	
	return 0;
}

/*
  set the input to capture images , could be tuner (METEOR_INPUT_DEV1)
  or any of others input :  
	RCA/COMPOSITE1 (METEOR_INPUT_DEV0) 
	COMPOSITE2/S-VIDEO (METEOR_INPUT_DEV2)
	S-VIDEO (METEOR_INPUT_DEV3)
	VBI ?! (METEOR_INPUT_DEV_SVIDEO)
*/

static int set_input(struct context *cnt, struct video_dev *viddev, unsigned short input)
{
	int actport;
	int portdata[] = { METEOR_INPUT_DEV0, METEOR_INPUT_DEV1,
	                 METEOR_INPUT_DEV2, METEOR_INPUT_DEV3,
	                 METEOR_INPUT_DEV_SVIDEO  };

	if( input >= array_elem( portdata ) ) {
		cnt->callbacks->motion_log(LOG_WARNING, 0, "Channel Port %d out of range (0-4)",input);
		input = IN_DEFAULT;
	}

	actport = portdata[ input ];
	if( ioctl( viddev->fd_bktr, METEORSINPUT, &actport ) < 0 ) {
		if( input != IN_DEFAULT ) {
			cnt->callbacks->motion_log(LOG_WARNING, 0,
			           "METEORSINPUT %d invalid - Trying default %d ", input, IN_DEFAULT );
			input = IN_DEFAULT;
			actport = portdata[ input ];
			if( ioctl( viddev->fd_bktr, METEORSINPUT, &actport ) < 0 ) {
				cnt->callbacks->motion_log(LOG_ERR, 1, "METEORSINPUT %d init", input);
				return -1;
			}
		} else {
			cnt->callbacks->motion_log(LOG_ERR, 1, "METEORSINPUT  %d init",input);
			return -1;
		}
	}

	return 0;
}

static int set_geometry(struct context *cnt, struct video_dev *viddev, int width, int height)
{
	struct meteor_geomet geom;

	geom.columns = width;
	geom.rows = height;

//	geom.rows = geom.rows / 2;
//	geom.oformat |= METEOR_GEO_ODD_ONLY;


	geom.oformat = METEOR_GEO_YUV_422 | METEOR_GEO_YUV_12;
//	geom.oformat |= METEOR_GEO_EVEN_ONLY;	
	
//	geom.oformat = 0;
	geom.frames  = 1;

	if( ioctl( viddev->fd_bktr, METEORSETGEO, &geom ) < 0 ) {
		cnt->callbacks->motion_log(LOG_ERR, 1, "Couldn't set the geometry");
		return -1;
	}

	return 0;
}

/*
   set input format ( PAL, NTSC, SECAM, etc ... )
*/

static int set_input_format(struct context *cnt, struct video_dev *viddev, unsigned short newformat) 
{
	int input_format[] = { NORM_PAL_NEW, NORM_NTSC_NEW, NORM_SECAM_NEW, NORM_DEFAULT_NEW};
	int format;
 
	if( newformat >= array_elem( input_format ) ) {
		cnt->callbacks->motion_log(LOG_WARNING, 0, "Input format %d out of range (0-2)",newformat );
		format = NORM_DEFAULT_NEW;
		newformat = 3;
	} else
		format = input_format[newformat]; 

	if( ioctl( viddev->fd_bktr, BT848SFMT, &format ) < 0 ) {
		cnt->callbacks->motion_log(LOG_ERR, 1, "BT848SFMT, Couldn't set the input format , try again with default");
		format = NORM_DEFAULT_NEW;
		newformat = 3;
		
		if( ioctl( viddev->fd_bktr, BT848SFMT, &format ) < 0 ) {
			cnt->callbacks->motion_log(LOG_ERR, 1, "BT848SFMT, Couldn't set the input format either default");
			return -1;
		}
	}
	
	return 0;
}

/*
statict int setup_pixelformat( int bktr )
{
	int i;
	struct meteor_pixfmt p;
	int format=-1;

	for( i=0; ; i++ ){
		p.index = i;
		if( ioctl( bktr, METEORGSUPPIXFMT, &p ) < 0 ){
			if( errno == EINVAL )
				break;
			cnt->callbacks->motion_log(LOG_ERR, 1, "METEORGSUPPIXFMT getting pixformat %d",i);	
			return -1;
		}


	// Hack from xawtv 4.x 

	switch ( p.type ){
		case METEOR_PIXTYPE_RGB:
			cnt->callbacks->motion_log(-1, 0, "setup_pixelformat METEOR_PIXTYPE_RGB");
			switch(p.masks[0]) {
				case 31744: // 15 bpp 
					format = p.swap_bytes ? VIDEO_RGB15_LE : VIDEO_RGB15_BE;
					cnt->callbacks->motion_log(-1, 0, "setup_pixelformat METEOR_PIXTYPE_RGB VIDEO_RGB15");
				break;
				case 63488: // 16 bpp 
					format = p.swap_bytes ? VIDEO_RGB16_LE : VIDEO_RGB16_BE;
					cnt->callbacks->motion_log(-1, 0, "setup_pixelformat METEOR_PIXTYPE_RGB VIDEO_RGB16");
				break;
				case 16711680: // 24/32 bpp 
					if (p.Bpp == 3 && p.swap_bytes == 1) {
						format = VIDEO_BGR24;
						cnt->callbacks->motion_log(-1, 0, "setup_pixelformat METEOR_PIXTYPE_RGB VIDEO_BGR24");
					} else if (p.Bpp == 4 && p.swap_bytes == 1 && p.swap_shorts == 1) {
						format = VIDEO_BGR32;
						cnt->callbacks->motion_log(-1, 0, "setup_pixelformat METEOR_PIXTYPE_RGB VIDEO_BGR32");
						} else if (p.Bpp == 4 && p.swap_bytes == 0 && p.swap_shorts == 0) {
							format = VIDEO_RGB32;
							cnt->callbacks->motion_log(-1, 0, "setup_pixelformat METEOR_PIXTYPE_RGB VIDEO_RGB32");
						}
					}
				break;
				case METEOR_PIXTYPE_YUV:
					format = VIDEO_YUV422P;
					cnt->callbacks->motion_log(-1, 0, "setup_pixelformat METEOR_PIXTYPE_YUV");
				break;
				case METEOR_PIXTYPE_YUV_12:
					format = VIDEO_YUV422P;
					cnt->callbacks->motion_log(-1, 0, "setup_pixelformat METEOR_PIXTYPE_YUV_12");
					break;
				case METEOR_PIXTYPE_YUV_PACKED:
					format = VIDEO_YUV422P;
					cnt->callbacks->motion_log(-1, 0, "setup_pixelformat METEOR_PIXTYPE_YUV_PACKED");
				break;
	
			}

			if( p.type == METEOR_PIXTYPE_RGB && p.Bpp == 3 ){
				// Found a good pixeltype -- set it up 
				if( ioctl( bktr, METEORSACTPIXFMT, &i ) < 0 ){
					cnt->callbacks->motion_log(LOG_WARNING, 1, "METEORSACTPIXFMT etting pixformat METEOR_PIXTYPE_RGB Bpp == 3");
				// Not immediately fatal 
				}
				cnt->callbacks->motion_log(LOG_DEBUG, 0, "input format METEOR_PIXTYPE_RGB %i",i);
				format=i;
			}

			if( p.type == METEOR_PIXTYPE_YUV_PACKED ){
				// Found a good pixeltype -- set it up
				if( ioctl( bktr, METEORSACTPIXFMT, &i ) < 0 ){
					cnt->callbacks->motion_log(LOG_WARNING, 1, "METEORSACTPIXFMT setting pixformat METEOR_PIXTYPE_YUV_PACKED");
				// Not immediately fatal
				} 
				cnt->callbacks->motion_log(LOG_DEBUG, 0, "input format METEOR_PIXTYPE_YUV_PACKED %i",i);
				format=i;
			}

		}

	return format;
}

*/

static void bktr_picture_controls(struct context *cnt, struct video_dev *viddev)
{
	int dev=viddev->fd_bktr;

	if ( (cnt->conf.contrast) && (cnt->conf.contrast != viddev->contrast) ){ 
		set_contrast(cnt, dev,cnt->conf.contrast);
		viddev->contrast = cnt->conf.contrast;	
	}

	if ( (cnt->conf.hue) && (cnt->conf.hue != viddev->hue) ) {
		set_hue(cnt, dev, cnt->conf.hue);
		viddev->hue = cnt->conf.hue;	
	}

	if ( (cnt->conf.brightness) && 
	     (cnt->conf.brightness != viddev->brightness)) {
		set_brightness(cnt, dev, cnt->conf.brightness);
		viddev->brightness = cnt->conf.brightness; 
	}

	if ( (cnt->conf.saturation ) && 
	     (cnt->conf.saturation != viddev->saturation) ){
		set_saturation(cnt, dev, cnt->conf.saturation);
		viddev->saturation = cnt->conf.saturation;
	}

	if (cnt->conf.auto_brightness) {
		/* TODO */
	}
}


/*******************************************************************************************
	Video capture routines

 - set input
 - setup_pixelformat
 - set_geometry

 - set_brightness 
 - set_chroma
 - set_contrast
 - set_channelset
 - set_channel
 - set_capture_mode

*/
static unsigned char *bktr_start(struct context *cnt, struct video_dev *viddev, int width, int height, unsigned short input, unsigned short norm, unsigned long freq)
{
	int dev_bktr=viddev->fd_bktr;
	//int dev_tunner=viddev->fd_tuner;
	/* to ensure that all device will be support the capture mode 
	  _TODO_ : Autodected the best capture mode .
	*/
	int dummy = 1;
//	int pixelformat = BSD_VIDFMT_I420;
	int single  = METEOR_CAP_SINGLE;

	void *map;

	/* if we have choose the tuner is needed to setup the frequency */
	/* Temporary HACK that allow to use input 1 ( Tuner for most of cards ) as Composite if frequency is set to -1 FIXME */
	if (( input == IN_TV ) && (freq != -1)) {
		if (!freq) {
			cnt->callbacks->motion_log(LOG_ERR, 1, "Not valid Frequency [%lu] for Source input [%i]", freq, input);
			return (NULL);
		}else if (set_freq(viddev, freq) == -1) {
			cnt->callbacks->motion_log(LOG_ERR, 1, "Frequency [%lu] Source input [%i]", freq, input);
			return (NULL);
		}
	}
	
	/* FIXME if we set as input tuner , we need to set option for tuner not for bktr */

	if ( set_input_format(viddev, norm) == -1 ) {
		cnt->callbacks->motion_log(LOG_ERR, 1, "set input format [%d]",norm);
		return (NULL);
	}

	if (set_geometry(viddev, width, height) == -1) {
		cnt->callbacks->motion_log(LOG_ERR, 1, "set geometry [%d]x[%d]",width, height);
		return (NULL);
	}
/*
	if (ioctl(dev_bktr, METEORSACTPIXFMT, &pixelformat) < 0 ){
        	cnt->callbacks->motion_log(LOG_ERR, 1, "set enconding method BSD_VIDFMT_I420"); 
		return(NULL);
        }


	NEEDED !? FIXME 

	if ( setup_pixelformat(viddev) == -1) {
		return (NULL);
	}
*/

	if (freq) {
		if (cnt->conf.setup_mode)
			cnt->callbacks->motion_log(-1, 0, "Frequency set (no implemented yet");
	/*
	 TODO missing implementation
		set_channelset(viddev);
		set_channel(viddev);
		if (set_freq (viddev, freq) == -1) {
			return (NULL);
		}
	*/
	}


	/* set capture mode and capture buffers */

	/* That is the buffer size for capture images ,
	 so is dependent of color space of input format / FIXME */

	viddev->bktr_bufsize = (((width*height*3/2)) * sizeof(unsigned char *));
	viddev->bktr_fmt = VIDEO_PALETTE_YUV420P;
	

	map = mmap((caddr_t)0,viddev->bktr_bufsize,PROT_READ|PROT_WRITE,MAP_SHARED, dev_bktr, (off_t)0);

	if (map == MAP_FAILED){
		cnt->callbacks->motion_log(LOG_ERR, 1, "mmap failed");
		return (NULL);
	}

	/* FIXME double buffer */ 
	if (0) {
		viddev->bktr_maxbuffer=2;
		viddev->bktr_buffers[0]=map;
		viddev->bktr_buffers[1]=(unsigned char *)map+0; /* 0 is not valid just a test */
		//viddev->bktr_buffers[1]=map+vid_buf.offsets[1];
	} else {
		viddev->bktr_buffers[0]=map;
		viddev->bktr_maxbuffer=1;
	}

	viddev->bktr_curbuffer=0;

	/* Clear the buffer */

        if (ioctl(dev_bktr, BT848SCBUF, &dummy) < 0) {
                cnt->callbacks->motion_log(LOG_ERR, 1, "BT848SCBUF");
                return NULL;
        }

	// settle , sleep(1) replaced
	SLEEP(1,0)

	if (ioctl(dev_bktr, METEORCAPTUR, &single) < 0){
		cnt->callbacks->motion_log(LOG_ERR, 1, "METEORCAPTUR using single method Error capturing");
        }else viddev->capture_method = CAPTURE_SINGLE;

	/* FIXME*/
	switch (viddev->bktr_fmt) {
		case VIDEO_PALETTE_YUV420P:
			viddev->bktr_bufsize=(width*height*3)/2;
			cnt->callbacks->motion_log(-1, 0, "VIDEO_PALETTE_YUV420P palette setting bufsize");
			break;
		case VIDEO_PALETTE_YUV422:
			viddev->bktr_bufsize=(width*height*2);
			break;
		case VIDEO_PALETTE_RGB24:
			viddev->bktr_bufsize=(width*height*3);
			break;
		case VIDEO_PALETTE_GREY:
			viddev->bktr_bufsize=width*height;
			break;
	}
	
	{
	int val;
	cnt->callbacks->motion_log(LOG_INFO,0,"HUE [%d]",get_hue(cnt, dev_bktr, &val));
	cnt->callbacks->motion_log(LOG_INFO,0,"SATURATION [%d]",get_saturation(cnt, dev_bktr, &val));
	cnt->callbacks->motion_log(LOG_INFO,0,"BRIGHTNESS [%d]",get_brightness(cnt, dev_bktr, &val));
	cnt->callbacks->motion_log(LOG_INFO,0,"CONTRAST [%d]",get_contrast(cnt, dev_bktr, &val));
	}
	return map;
}


/**
 * bktr_next fetches a video frame from a bktr device
 * Parameters:
 *     viddev     Pointer to struct containing video device handle
 *     map        Pointer to the buffer in which the function puts the new image
 *     width      Width of image in pixels
 *     height     Height of image in pixels
 *
 * Returns
 *     0          Success
 *    -1          Fatal error
 *     1          Non fatal error (not implemented)
 */
static int bktr_next(struct video_dev *viddev,unsigned char *map, int width, int height)
{
	int dev_bktr=viddev->fd_bktr;
	unsigned char *cap_map=NULL;
	int single  = METEOR_CAP_SINGLE;
	int continous = METEOR_CAP_CONTINOUS;
	sigset_t  set, old;


	/* ONLY MMAP method is used to Capture */

	/* Allocate a new mmap buffer */
	/* Block signals during IOCTL */
	sigemptyset (&set);
	sigaddset (&set, SIGCHLD);
	sigaddset (&set, SIGALRM);
	sigaddset (&set, SIGUSR1);
	sigaddset (&set, SIGTERM);
	sigaddset (&set, SIGHUP);
	pthread_sigmask (SIG_BLOCK, &set, &old);
	cap_map=viddev->bktr_buffers[viddev->bktr_curbuffer];

	viddev->bktr_curbuffer++;
	if (viddev->bktr_curbuffer >= viddev->bktr_maxbuffer)
		viddev->bktr_curbuffer=0;

	/* capture */

	if (viddev->capture_method == CAPTURE_CONTINOUS){
		if (ioctl(dev_bktr, METEORCAPTUR, &continous) < 0) {
			cnt->callbacks->motion_log(LOG_ERR, 1, "Error capturing using continous method");
			sigprocmask (SIG_UNBLOCK, &old, NULL);
			return (-1);
		}
	}else{

		if (ioctl(dev_bktr, METEORCAPTUR, &single) < 0) {
			cnt->callbacks->motion_log(LOG_ERR, 1, "Error capturing using single method");
			sigprocmask (SIG_UNBLOCK, &old, NULL);
			return (-1);
		}
	}

	/*undo the signal blocking*/
	pthread_sigmask (SIG_UNBLOCK, &old, NULL);
	

	switch (viddev->bktr_fmt){
		case VIDEO_PALETTE_RGB24:
			rgb24toyuv420p(map, cap_map, width, height);
			break;
		case VIDEO_PALETTE_YUV422:
			yuv422to420p(map, cap_map, width, height);
			break;
		default:
			memcpy(map, cap_map, viddev->bktr_bufsize);
	}

	
	return 0;
}


/* set input & freq if needed FIXME not allowed use Tuner yet */

static void bktr_set_input(struct context *cnt, struct video_dev *viddev, unsigned char *map, int width, int height,
                    unsigned short input, unsigned short norm, int skip, unsigned long freq)
{
	int i;
	unsigned long frequnits = freq;

	if (input != viddev->input || width != viddev->width || height!=viddev->height || freq!=viddev->freq){ 
		cnt->callbacks->motion_log(LOG_WARNING, 0, "bktr_set_input really needed ?");

		if (set_input(viddev, input) == -1)
			return;

		if (set_input_format(viddev, norm) == -1 )
			return;
		
		if (( input == IN_TV ) && (frequnits)) {
			if (set_freq (viddev, freq) == -1)
				return;
		}

		// FIXME
		/*
		if ( setup_pixelformat(viddev) == -1) {
			cnt->callbacks->motion_log(LOG_ERR, 1, "ioctl (VIDIOCSFREQ)");
			return
		}
		*/

		if (set_geometry(viddev, width, height) == -1)
			return;
	
		bktr_picture_controls(cnt, viddev);
		
		viddev->input=input;
		viddev->width=width;
		viddev->height=height;
		viddev->freq=freq;
		viddev->norm=norm;

		/* skip a few frames if needed */
		for (i=0; i<skip; i++)
			bktr_next(viddev, map, width, height);
	}else{
		/* No round robin - we only adjust picture controls */
		bktr_picture_controls(cnt, viddev);
	}
}

/*****************************************************************************
	Wrappers calling the current capture routines
 *****************************************************************************/

/*

vid_init - Allocate viddev struct.
vid_start - Setup Device parameters ( device , channel , freq , contrast , hue , saturation , brightness ) and open it.
vid_next - Capture a frame and set input , contrast , hue , saturation and brighness if necesary. 
vid_close - close devices. 
vid_cleanup - Free viddev struct.

*/

/* big lock for vid_start */
pthread_mutex_t vid_mutex;
/* structure used for per device locking */
struct video_dev **viddevs=NULL;

void vid_init(motion_ctxt_ptr cnt)
{
	pthread_mutex_lock(cnt->global_lock);
	if (!viddevs) { 
		viddevs=cnt->callbacks->motion_alloc(sizeof(struct video_dev *));
		viddevs[0]=NULL;
		pthread_mutex_init(&vid_mutex, NULL);
	}
	if (cnt->video_vars->video_params) {
		cnt->callbacks->cnt->callbacks->motion_log(LOG_ERR, 0, "in vid_init: video_config already allocated");
	} else {
		cnt->video_vars->video_params = cnt->callbacks->motion_alloc(sizeof(bktr_config));
		memset(cnt->video_vars->video_params, 0, sizeof(bktr_config));
	}

	pthread_mutex_unlock(cnt->global_lock);	
	
}

/* Called by childs to get rid of open video devices */
void vid_close(motion_ctxt_ptr cnt)
{
	int i=-1;

	if (viddevs){ 
		while(viddevs[++i]){
			close(viddevs[i]->fd_bktr);
			close(viddevs[i]->fd_tuner);
		}
	}
}


void vid_cleanup(motion_ctxt_ptr cnt)
{
	int i=-1;
	if (viddevs){ 
		while(viddevs[++i]){
			munmap(viddevs[i]->bktr_buffers[0],viddevs[i]->bktr_bufsize);
			viddevs[i]->bktr_buffers[0] = MAP_FAILED;
			free(viddevs[i]);
		}
		free(viddevs);
		viddevs=NULL;
	}
}


int vid_start(struct context *cnt)
{
	struct config *conf=&cnt->conf;
	int fd_bktr=-1,fd_tuner=-1;
	int i=-1;
	int width, height;
	unsigned short input, norm;
	unsigned long frequency;

	/* We use width and height from conf in this function. They will be assigned
	 * to width and height in imgs here, and cap_width and cap_height in 
	 * rotate_data won't be set until in rotate_init.
	 * Motion requires that width and height are multiples of 16 so we check for this
	 */
	if (conf->width % 16) {
		cnt->callbacks->motion_log(LOG_ERR, 0,
		           "config image width (%d) is not modulo 16",
		           conf->width);
		return -1;
	}
	if (conf->height % 16) {
		cnt->callbacks->motion_log(LOG_ERR, 0,
		           "config image height (%d) is not modulo 16",
		           conf->height);
		return -1;
	}
	width = conf->width;
	height = conf->height;
	input = BKTR_PARAM->conf->input;
	norm = BKTR_PARAM->conf->norm;
	frequency = BKTR_PARAM->conf->frequency;

	pthread_mutex_lock(&vid_mutex);

	/* Transfer width and height from conf to imgs. The imgs values are the ones
	 * that is used internally in Motion. That way, setting width and height via
	 * http remote control won't screw things up.
	 */
	cnt->imgs.width=width;
	cnt->imgs.height=height;

	/* First we walk through the already discovered video devices to see
	 * if we have already setup the same device before. If this is the case
	 * the device is a Round Robin device and we set the basic settings
	 * and return the file descriptor
	 */
	while (viddevs[++i]) { 
		if (!strcmp(BKTR_PARAM->conf->video_device, viddevs[i]->video_device)) {
			int fd;
			cnt->imgs.type=viddevs[i]->bktr_fmt;
			cnt->callbacks->motion_log(-1, 0, "vid_start cnt->imgs.type [%i]", cnt->imgs.type);
			switch (cnt->imgs.type) {
				case VIDEO_PALETTE_GREY:
					cnt->imgs.motionsize=width*height;
					cnt->imgs.size=width*height;
				break;
				case VIDEO_PALETTE_RGB24:
				case VIDEO_PALETTE_YUV422:
					cnt->imgs.type=VIDEO_PALETTE_YUV420P;
				case VIDEO_PALETTE_YUV420P:
					cnt->callbacks->motion_log(-1, 0,
					           " VIDEO_PALETTE_YUV420P setting imgs.size and imgs.motionsize");
					cnt->imgs.motionsize=width*height;
					cnt->imgs.size=(width*height*3)/2;
				break;
			}
			fd=viddevs[i]->fd_bktr; // FIXME return fd_tuner ?!
			pthread_mutex_unlock(&vid_mutex);
				return fd;
		}
	}

	viddevs=cnt->callbacks->motion_realloc(viddevs, sizeof(struct video_dev *)*(i+2), "vid_start");
	viddevs[i]=cnt->callbacks->motion_alloc(sizeof(struct video_dev));
	viddevs[i+1]=NULL;
	pthread_mutexattr_init(&viddevs[i]->attr);
	pthread_mutex_init(&viddevs[i]->mutex, NULL);
	fd_bktr=open(BKTR_PARAM->conf->video_device, O_RDWR);
	if (fd_bktr <0) { 
		cnt->callbacks->motion_log(LOG_ERR, 1, "open video device %s",BKTR_PARAM->conf->video_device);
		cnt->callbacks->motion_log(LOG_ERR, 0, "Motion Exits.");
		exit(1);
	}

	/* Only open tuner if freq and input is 1 that means that tunner has been selected as source FIXME ?! */
	if (( frequency ) && ( input = IN_TV )) {
		fd_tuner=open(BKTR_PARAM->conf->tuner_device, O_RDWR);
		if (fd_tuner <0) { 
			cnt->callbacks->motion_log(LOG_ERR, 1, "open tuner device %s",BKTR_PARAM->conf->tuner_device);
			cnt->callbacks->motion_log(LOG_ERR, 0, "Motion Exits.");
			exit(1);
		}
	}
	viddevs[i]->video_device=BKTR_PARAM->conf->video_device;
	viddevs[i]->tuner_device=BKTR_PARAM->conf->tuner_device;
	viddevs[i]->fd_bktr=fd_bktr;
	viddevs[i]->fd_tuner=fd_tuner;
	viddevs[i]->input=input;
	viddevs[i]->height=height;
	viddevs[i]->width=width;
	viddevs[i]->freq=frequency;
	viddevs[i]->owner=-1;

	/* We set brightness, contrast, saturation and hue = 0 so that they only get
         * set if the config is not zero.
         */
	
	viddevs[i]->brightness=0;
	viddevs[i]->contrast=0;
	viddevs[i]->saturation=0;
	viddevs[i]->hue=0;
	viddevs[i]->owner=-1;

	/* default palette */ 
	viddevs[i]->bktr_fmt=VIDEO_PALETTE_YUV420P;
	viddevs[i]->bktr_curbuffer=0;
	viddevs[i]->bktr_maxbuffer=1;

	if (!bktr_start (cnt, viddevs[i], width, height, input, norm, frequency)){ 
		pthread_mutex_unlock(&vid_mutex);
		return -1;
	}
	
	cnt->imgs.type=viddevs[i]->bktr_fmt;
	
	switch (cnt->imgs.type) { 
		case VIDEO_PALETTE_GREY:
			cnt->imgs.size=width*height;
			cnt->imgs.motionsize=width*height;
		break;
		case VIDEO_PALETTE_RGB24:
		case VIDEO_PALETTE_YUV422:
			cnt->imgs.type=VIDEO_PALETTE_YUV420P;
		case VIDEO_PALETTE_YUV420P:
			cnt->callbacks->motion_log(-1, 0, "VIDEO_PALETTE_YUV420P imgs.type");
			cnt->imgs.size=(width*height*3)/2;
			cnt->imgs.motionsize=width*height;
		break;
	}
	
	pthread_mutex_unlock(&vid_mutex);


	/* FIXME needed tuner device ?! */
	return fd_bktr;
}


/**
 * vid_next fetches a video frame from bktr device 
 * Parameters:
 *     cnt        Pointer to the context for this thread
 *     map        Pointer to the buffer in which the function puts the new image
 *
 * Returns
 *     0          Success
 *    -1          Fatal BKTR error
 *     1          Non fatal BKTR error (not implemented)
 */
int vid_next(struct context *cnt, unsigned char *map)
{
	struct config *conf=&cnt->conf;
	int ret = -1;
	int i=-1;
	int width, height;
	int dev_bktr = cnt->video_dev;

	/* NOTE: Since this is a capture, we need to use capture dimensions. */
	width = cnt->rotate_data.cap_width;
	height = cnt->rotate_data.cap_height;
		
	while (viddevs[++i])
		if (viddevs[i]->fd_bktr==dev_bktr)
			break;

	if (!viddevs[i])
		return -1;

	if (viddevs[i]->owner!=cnt->threadnr) { 
		pthread_mutex_lock(&viddevs[i]->mutex);
		viddevs[i]->owner=cnt->threadnr;
		viddevs[i]->frames=conf->roundrobin_frames;
		cnt->switched=1;
	}


	bktr_set_input(cnt, viddevs[i], map, width, height, BKTR_PARAM->conf->input, BKTR_PARAM->conf->norm,
	              conf->roundrobin_skip, BKTR_PARAM->conf->frequency);
	
	ret = bktr_next(cnt, viddevs[i], map, width, height);

	if (--viddevs[i]->frames <= 0) { 
		viddevs[i]->owner=-1;
		pthread_mutex_unlock(&viddevs[i]->mutex);
	}
	
 	if(cnt->rotate_data.degrees > 0){ 
		/* rotate the image as specified */
		cnt->callbacks->rotate_map(cnt, map);
 	}
	
	return ret;
}

static motion_video_input pointer_table = {
	video_init:        vid_init,
	video_start:       vid_start,
	video_next:        vid_next,
	video_close:       vid_close,
	video_dev_cleanup: NULL,
	video_cleanup:     vid_cleanup,
};

static void *bktr_plugin_main(motion_msg msg)
{
	switch (msg) {
		case MOTION_VALIDATION_TABLE:
			return (void *)&bktr_params;   /* in bktr_params.h */
		case MOTION_VALIDATION_SIZE:
			return (void *)&bktr_params_size;
		case MOTION_VIDEO_INPUT:
			return (void *)&pointer_table;
		default:
			return NULL;
	}		
}

motion_plugin_descr MOTION_PLUGIN_SYM = {
	name: "bktr",
	version: "1.0",
	author: "Angel Carpintero",
	motion_plugin_control: bktr_plugin_main,
};

