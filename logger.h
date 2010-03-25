/*
 *      logger.h
 *
 *      Include file for logger.c 
 *
 *      Copyright 2005, William M. Brack 
 *      Copyright 2008 by Angel Carpintero  (ack@telefonica.net)
 *      This software is distributed under the GNU Public License Version 2
 *      See also the file 'COPYING'.
 *
 */
#ifndef _INCLUDE_LOGGER_H_
#define _INCLUDE_LOGGER_H_

#include "motion.h"
#include <syslog.h>

/* Logging mode */
#define LOGMODE_FILE            0   /* Log messages to file   */
#define LOGMODE_SYSLOG          1   /* Log messages to syslog */

/* Debug levels */
#define CAMERA_ALERT            1   /* alerts only             */
#define CAMERA_CRITICAL         2   /* critical only           */
#define CAMERA_WARNINGS         3   /* warnings only           */
#define TRACK_DEBUG             4   /* track debug             */
#define CAMERA_INFO             5   /* info debug              */
#define CAMERA_VIDEO            6   /* debug video not verbose */
#define CAMERA_DEBUG            7   /* debug but not verbose   */
#define CAMERA_VERBOSE          8   /* verbose level           */
#define CAMERA_ALL              9   /* everything              */

void set_log_mode(int mode);
FILE * set_logfile(const char *logfile_name);
void motion_log(int, int, const char *, ...);

#endif 
