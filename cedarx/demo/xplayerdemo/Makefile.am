
include $(top_srcdir)/Makefile.inc

bin_PROGRAMS = xplayerdemo

xplayerdemo_SOURCES =  ./xplayerdemo.c \
			./layerControl_null.c \
			./soundControl_null.c \
			./subtitleControl_null.c \
			./deinterlace_null.c \
			./layerControl_de.c \
            ./soundControl_alsa.c 

xplayerdemo_CFLAGS = $(CFLAGS_CDXG)
#xplayerdemo_CPPFLAGS = $(CPPFLAGS_CDXG)
LOCAL_INCLUDE = -I$(top_srcdir) \
		-I$(top_srcdir)/xplayer/include  \
		-I$(top_srcdir)/libcore/common/iniparser \
		-I$(top_srcdir)/libcore/base/include \
		-I$(top_srcdir)/libcore/stream/include \
		-I$(top_srcdir)/libcore/playback/include \
		-I$(top_srcdir)/libcore/parser/include \
		-I$(top_srcdir)/external/include/adecoder \
		-I$(top_srcdir)/external/include/   \
		-I$(top_srcdir)/../libcedarc/include

xplayerdemo_CFLAGS += $(LOCAL_INCLUDE)
#xplayerdemo_CPPFLAGS += $(LOCAL_INCLUDE)

xplayerdemo_LDFLAGS = $(LDFLAGS_CDXG)

xplayerdemo_LDADD = $(top_srcdir)/libcore/parser/base/libcdx_parser.la \
			$(top_srcdir)/libcore/base/libcdx_base.la \
			$(top_srcdir)/libcore/stream/base/libcdx_stream.la \
			$(top_srcdir)/libcore/playback/libcdx_playback.la \
			$(top_srcdir)/xplayer/libxplayer.la \
			$(top_srcdir)/libcore/common/libcdx_common.la

LOCAL_LIB = -dl -rt -lpthread -lz -lm -lcdc_vdecoder -ladecoder -lm -lcdc_sdecoder -lcdc_ve \
		-lcrypto -lcdc_memory -lcdc_base -lssl
#-lsalsa
xplayerdemo_LDFLAGS += $(LOCAL_LIB)

