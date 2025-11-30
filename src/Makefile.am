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
hp8753_CPPFLAGS += @GLIB_CFLAGS@ @GTK4_CFLAGS@ @SQLITE3_CFLAGS@

hp8753_CFLAGS = $(AM_CFLAGS)
hp8753_CXXFLAGS = $(AM_CXXFLAGS)

hp8753_LDFLAGS = -lgpib -lm -lgs -rdynamic
hp8753_LDFLAGS += @GLIB_LIBS@ @GTK4_LIBS@ @SQLITE3_LIBS@

#
# bin program
#

# Program name
bin_PROGRAMS = hp8753

hp8753_SOURCES = catalogWidgets.c databaseSaveAndRestore.c GPIBcommsThread.c \
		GPIB_interface.c GTKmainDialog.c GTKnoteCalibration.c \
		GTKnoteCalKit.c GTKnoteColor.c GTKnoteData.c GTKnoteGPIB.c \
		GTKnoteOptions.c GTKnoteTraces.c GTKplot.c GTKplotMarkers.c \
                GTKprint.c GTKrenameDialog.c GTKutility.c hp8753.c \
                hp8753comms.c hp8753-GTK4.c hp8753_S2P.c hp8753setupAndCal.c \
                HP_FORM1toFORM3.c HPlogo.c messageEvent.c parseCalibrationKit.c \
                PDF+PNG+SVG.c plotCartesian.c plotPolar.c plotScreen.c \
                plotSmith.c Prologix_interface.c smithHighResPDF.c \
                USBTMC_interface.c utility.c

hp8753_SOURCES += $(top_srcdir)/include/GPIBcomms.h \
				  $(top_srcdir)/include/hp8753comms.h \
				  $(top_srcdir)/include/hp8753.h \
				  $(top_srcdir)/include/GTKplot.h \
				  $(top_srcdir)/include/HPGLplot.h \
				  $(top_srcdir)/include/messageEvent.h \
				  $(top_srcdir)/include/smithChartPS.h \
				  $(top_srcdir)/include/calibrationKit.h \
				  $(top_srcdir)/include/widgetID.h

