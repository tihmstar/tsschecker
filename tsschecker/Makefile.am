AM_CFLAGS = -I$(top_srcdir)/include $(GLOBAL_CFLAGS) $(libcurl_CFLAGS) $(libfragmentzip_CFLAGS) \
							$(libgeneral_CFLAGS) $(libirecovery_CFLAGS) $(libplist_CFLAGS) $(openssl_CFLAGS)
AM_CXXFLAGS = $(AM_CFLAGS) $(GLOBAL_CXXFLAGS)
AM_LDFLAGS =	$(libcurl_LIBS) $(libfragmentzip_LIBS) $(libgeneral_LIBS) $(libirecovery_LIBS) \
							$(libplist_LIBS) $(openssl_LIBS)

lib_LTLIBRARIES = libtsschecker.la
bin_PROGRAMS = tsschecker

libtsschecker_la_CFLAGS = $(AM_CFLAGS)
libtsschecker_la_CXXFLAGS = $(AM_CXXFLAGS)
libtsschecker_la_LDFLAGS = $(AM_LDFLAGS)
libtsschecker_la_SOURCES = 	tsschecker.cpp \
							../external/jssy/jssy/jssy.c \
							TssRequest.cpp \
							TSSException.cpp \
							FirmwareAPI.cpp \
							FirmwareAPI_IPSWME.cpp

tsschecker_CFLAGS = $(AM_CFLAGS)
tsschecker_CXXFLAGS = $(AM_CXXFLAGS)
tsschecker_LDFLAGS = $(AM_LDFLAGS)
tsschecker_LDADD = libtsschecker.la
tsschecker_SOURCES = 	main.cpp
