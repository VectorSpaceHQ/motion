## Process this file with automake to produce Makefile.in

SUBDIRS = src plugins

EXTRA_DIST = \
	autogen.sh motion_version.h.in

install-data-local:
	@$(NORMAL_INSTALL)

pre-build-info:
	@echo "Welcome to the setup procedure for Motion, the motion detection daemon! If you get"
	@echo "error messages during this procedure, please report them to the mailing list. The"
	@echo "Motion Guide contains all information you should need to get Motion up and running."
	@echo "Run \"make updateguide\" to download the latest version of the Motion Guide."
	@echo
	@echo "Version:  $(VERSION)"
## Note that the "single-space" before ifneq, else, endif is a horrible-hack workaround for automake
## in order to get the (non-portable) construct generated without error.  It would probably be better
## to re-write this in a more "portable" way.
 ifneq (,$(findstring freebsd,$(VIDEO_OBJ)))
       	@echo "Platform: FreeBSD"
 else
       	@echo "Platform: Linux (if this is incorrect, please read README.FreeBSD)"
 endif
	@echo

updateguide: pre-build-info
	@echo "Downloading Motion Guide. If it fails, please check your Internet connection."
	@echo
	wget www.lavrsen.dk/twiki/bin/view/Motion/MotionGuideOneLargeDocument?skin=text -O motion_guide.tmp
	@echo "Cleaning up and fixing links..."
	@cat motion_guide.tmp | sed -e 's/\?CGISESSID=[0-9a-fA-F]*//g;s,"/twiki/,"http://www.lavrsen.dk/twiki/,g' > motion_guide.html
	@rm -f motion_guide.tmp
	@echo "All done, you should now have an up-to-date local copy of the Motion guide."
	@echo
