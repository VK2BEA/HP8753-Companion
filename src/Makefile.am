MAINTAINERCLEANFILES = Makefile.in

#pkginclude_HEADERS = ../include/GPIBcomms.h ../include/hp8753comms.h ../include/hp8753.h ../include/messageEvent.h ../include/smithChartPS.h ../include/HPGLplot.h ../include/calibrationKit.h
#Library specific CPPFLAGS

if DEBUG
	AM_CFLAGS = -g3 -O0
	AM_CXXFLAGS = -g3 -O0
else
	AM_CFLAGS = -O2
	AM_CXXFLAGS = -O2
endif

hp8753_CPPFLAGS = "-I$(top_srcdir)/include"
hp8753_CPPFLAGS += @GLIB_CFLAGS@ @GTK3_CFLAGS@ @SQLITE3_CFLAGS@

hp8753_CFLAGS = $(AM_CFLAGS)
hp8753_CXXFLAGS = $(AM_CXXFLAGS)

hp8753_LDFLAGS = -lgpib -lm -lgs -rdynamic
hp8753_LDFLAGS += @GLIB_LIBS@ @GTK3_LIBS@ @SQLITE3_LIBS@

#
# bin program
#

# Program name
bin_PROGRAMS = hp8753

hp8753_SOURCES = databaseSaveAndRestore.c  GTKplotMarkers.c \
				 GTKwidgetCallbacks.c HP8753_S2P.c \
                 HPicon.c noteCalKitWidgetCallbacks.c \
                 noteOptionsWidgetCallbacks.c plotPolar.c \
                 printWidgetCallbacks.c utility.c \
                 GPIBcommsThread.c GTKrenameDialogWidgets.c \
                 hp8753.c HP8753setupAndCal.c \
                 HPlogo.c noteDataWidgets.c noteColor.c \
                 parseCalibrationKit.c plotScreen.c \
                 resource.c GTKplot.c \
                 GTKutility.c HP8753comms.c \
                 HP_FORM1toFORM3.c messageEvent.c \
                 noteGPIBwidgetCallbacks.c plotCartesian.c \
                 plotSmith.c smithHighResPDF.c \
                 GPIB_interface.c USBTMC_interface.c Prologix_interface.c

hp8753_SOURCES += $(top_srcdir)/include/GPIBcomms.h \
				  $(top_srcdir)/include/hp8753comms.h \
				  $(top_srcdir)/include/hp8753.h \
				  $(top_srcdir)/include/GTKplot.h \
				  $(top_srcdir)/include/HPGLplot.h \
				  $(top_srcdir)/include/messageEvent.h \
				  $(top_srcdir)/include/smithChartPS.h \
				  $(top_srcdir)/include/calibrationKit.h

