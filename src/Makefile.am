MAINTAINERCLEANFILES = Makefile.in

#pkginclude_HEADERS = ../include/GPIBcomms.h ../include/hp8753comms.h ../include/hp8753.h ../include/messageEvent.h ../include/smithChartPS.h
#Library specific CPPFLAGS
hp8753_CPPFLAGS = "-I$(top_srcdir)/include"
hp8753_CPPFLAGS += @GLIB_CFLAGS@ @GTK3_CFLAGS@ @SQLITE3_CFLAGS@

hp8753_CXXFLAGS = $(AM_CXXFLAGS)

hp8753_LDFLAGS = -lgpib -lm -lgs -lsystemd -rdynamic
hp8753_LDFLAGS += @GLIB_LIBS@ @GTK3_LIBS@ @SQLITE3_LIBS@

#
# bin program
#

# Program name
bin_PROGRAMS = hp8753

hp8753_SOURCES = databaseSaveAndRestore.c GTKplotMarkers.c \
                 hp8753.c HP8753setupAndCal.c HPlogo.c \
                 smithHighResPDF.c GPIBcommsThread.c \
                 GTKutility.c HP8753comms.c HP_FORM1toFORM3.c \
                 messageEvent.c utility.c GTKplot.c \
                 plotCartesian.c plotPolar.c plotSmith.c \
                 GTKwidgetCallbacks.c HP8753_S2P.c HPicon.c \
                 resource.c 
hp8753_SOURCES += $(top_srcdir)/include/GPIBcomms.h \
				  $(top_srcdir)/include/hp8753comms.h \
				  $(top_srcdir)/include/hp8753.h \
				  $(top_srcdir)/include/GTKplot.h \
				  $(top_srcdir)/include/messageEvent.h \
				  $(top_srcdir)/include/smithChartPS.h

