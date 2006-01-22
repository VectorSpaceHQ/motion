/*
 *      webhttpd.h
 *
 *      Include file for webhttpd.c 
 *
 *      Specs : http://www.lavrsen.dk/twiki/bin/view/Motion/MotionHttpAPI
 *
 *      Copyright 2004-2005 by Angel Carpintero  (ack@telefonica.net)
 *      This software is distributed under the GNU Public License Version 2
 *      See also the file 'COPYING'.
 *
 */
#ifndef _INCLUDE_WEBHTTPD_H_
#define _INCLUDE_WEBHTTPD_H_

#define BASE64_LENGTH(len) (4 * (((len) + 2) / 3))

void * motion_web_control(void *arg); 
void httpd_run(struct context **);
void base64_encode(const char *, char *, int);

#endif
