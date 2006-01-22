## Process this file with automake to produce Makefile.in

AM_CFLAGS = -g -W -D_REENTRANT -DPLUGINDIR=\"$(pkglibdir)\" \
	-DSYSCONFDIR=\"$(sysconfdir)\"

INCLUDES = -I$(top_srcdir)/src

pkglib_LTLIBRARIES = v4l_plugin.la netcam_plugin.la
#pkglib_LTLIBRARIES = v4l_plugin.la

v4l_plugin_la_SOURCES = v4l_plugin.c
v4l_plugin_la_LDFLAGS = -module
v4l_plugin_la_CFLAGS = -I$(top_src_dir)/src $(AM_CFLAGS)

netcam_plugin_la_SOURCES = netcam_plugin.c netcam_jpeg.c netcam_wget.c
netcam_plugin_la_LDFLAGS = -module
netcam_plugin_la_CFLAGS = -I$(top_src_dir)/src $(AM_CFLAGS)
