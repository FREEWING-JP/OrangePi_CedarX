
include $(top_srcdir)/Makefile.inc

lib_LTLIBRARIES = libcdx_playback.la

libcdx_playback_la_SOURCES = audioDecComponent.c     \
    audioRenderComponent.c              \
    videoDecComponent.c                 \
    subtitleDecComponent.c              \
    subtitleRenderComponent.c           \
    avtimer.c                           \
    bitrateEstimater.c                  \
    framerateEstimater.c                \
    streamManager.c                     \
    player.c                            \
    videoRenderComponent_newDisplay.c   \
    baseComponent.c

#	videoRenderComponent_newDisplay.c

libcdx_playback_la_CFLAGS = $(CFLAGS_CDXG)
#libcdx_playback_la_CPPFLAGS = $(CFLAGS_CPPDXG)
LOCAL_INCLUDE = -I$(top_srcdir)/libcore/include \
		-I$(top_srcdir)/libcore/base/include \
		-I$(top_srcdir)/libcore/common/iniparser \
		-I$(top_srcdir)/ \
		-I$(top_srcdir)/external/include/adecoder/ \
		-I$(top_srcdir)/../libcedarc/include \
		-I./include

libcdx_playback_la_CFLAGS += $(LOCAL_INCLUDE)
#libcdx_playback_la_CPPFLAGS += $(LOCAL_INCLUDE)


libcdx_playback_la_LDFLAGS = $(LDFLAGS_CDXG)

libcdx_playback_la_LIBADD = $(top_srcdir)/libcore/common/libcdx_common.la \
			$(top_srcdir)/libcore/base/libcdx_base.la

LOCAL_LIB = -lpthread -lrt -lcdc_vdecoder -ladecoder -lcdc_sdecoder -lcdc_memory
libcdx_playback_la_LDFLAGS += $(LOCAL_LIB)

