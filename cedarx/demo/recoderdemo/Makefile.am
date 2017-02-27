
include $(top_srcdir)/Makefile.inc

bin_PROGRAMS = recoderdemo

recoderdemo_SOURCES =  recoderdemo.c \
			RecoderWriter.c

recoderdemo_CFLAGS = $(CFLAGS_CDXG)
recoderdemo_CPPFLAGS = $(CPPFLAGS_CDXG)
LOCAL_INCLUDE = -I$(top_srcdir) \
		-I$(top_srcdir)/libcore/include \
		-I$(top_srcdir)/awrecorder  \
		-I$(top_srcdir)/libcore/base/include \
		-I$(top_srcdir)/libcore/muxer/include \
		-I$(top_srcdir)/external/include/aencoder \
		-I$(top_srcdir)/../libcedarc/include

recoderdemo_CFLAGS += $(LOCAL_INCLUDE)
recoderdemo_CPPFLAGS += $(LOCAL_INCLUDE)

recoderdemo_LDFLAGS = $(LDFLAGS_CDXG)

recoderdemo_LDADD =  \
			$(top_srcdir)/libcore/base/libcdx_base.la \
			$(top_srcdir)/libcore/muxer/base/libcdx_muxer.la \
			$(top_srcdir)/awrecorder/libawrecorder.la

LOCAL_LIB = -lpthread -lz -lcdc_vencoder -laencoder -lcdc_ve \
		-lcrypto -lcdc_memory -lcdc_base
recoderdemo_LDFLAGS += $(LOCAL_LIB)

