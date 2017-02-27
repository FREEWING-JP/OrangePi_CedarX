LOCAL_PATH:= $(call my-dir)
include $(CLEAR_VARS)

CEDARM_PATH=$(LOCAL_PATH)/../..
CEDARC_PATH=$(TOP)/frameworks/av/media/libcedarc

include $(CEDARM_PATH)/config.mk

LOCAL_ARM_MODE := arm

LOCAL_SRC_FILES := \
    awplayer.cpp        \
    awStreamingSource.cpp \
    awStreamListener.cpp  \
    subtitleUtils.cpp     \
    AwHDCPModule.cpp \
    awLogRecorder.cpp   \

LOCAL_C_INCLUDES  := \
        $(TOP)/frameworks/av/                               \
        $(TOP)/frameworks/av/include/                       \
        $(TOP)/frameworks/native/include/android            \
        $(CEDARC_PATH)/include                              \
        $(CEDARC_PATH)/vdecoder/include                     \
        $(CEDARC_PATH)/adecoder/include                     \
        $(CEDARC_PATH)/sdecoder/include                     \
        $(CEDARM_PATH)/external/include/adecoder            \
        $(CEDARM_PATH)/libcore/playback/include             \
        $(CEDARM_PATH)/libcore/common/iniparser/            \
        $(CEDARM_PATH)/libcore/parser/include/              \
        $(CEDARM_PATH)/libcore/stream/include/              \
        $(CEDARM_PATH)/libcore/base/include/                \
        $(CEDARM_PATH)/xplayer/include                      \
        $(CEDARM_PATH)/android_adapter/output               \
        $(CEDARM_PATH)/                                     \


# for subtitle character set transform.
ifeq ($(CONF_ANDROID_VERSION), 5.0)
LOCAL_C_INCLUDES += $(TOP)/external/icu/icu4c/source/common
else ifeq ($(CONF_ANDROID_VERSION), 5.1)
LOCAL_C_INCLUDES += $(TOP)/external/icu/icu4c/source/common
else ifeq ($(CONF_ANDROID_VERSION), 6.0)
LOCAL_C_INCLUDES += $(TOP)/external/icu/icu4c/source/common
else
LOCAL_C_INCLUDES += $(TOP)/external/icu4c/common
endif


LOCAL_MODULE_TAGS := optional

LOCAL_CFLAGS +=

LOCAL_MODULE:= libawplayer

LOCAL_SHARED_LIBRARIES +=   \
        libutils            \
        libcutils           \
        libbinder           \
        libmedia            \
        libui               \
        libgui              \
        libion              \
        libcdx_playback     \
        libcdx_parser       \
        libcdx_stream       \
        libcdx_base         \
        libicuuc            \
        libMemAdapter       \
        libxplayer          \
        libaw_output        \
        libcdx_common

include $(BUILD_SHARED_LIBRARY)

