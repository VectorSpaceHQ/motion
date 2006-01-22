## Process this file with automake to produce Makefile.in

bin_PROGRAMS = motion

motion_SOURCES = \
	alg.c newconfig.c draw.c event.c ffmpeg.c motion.c \
	picture.c plugins.c rotate.c track.c webcam.c
# webhttpd.c temporarily removed until it is updated

AM_CFLAGS = -g -W -D_REENTRANT -DPLUGINDIR=\"$(pkglibdir)\" \
	-DSYSCONFDIR=\"$(sysconfdir)\"
