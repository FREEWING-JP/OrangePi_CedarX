LOCAL_PATH:= $(call my-dir)
include $(CLEAR_VARS)

LOCAL_C_INCLUDES := $(LOCAL_PATH)/../../awplayer/ \

LOCAL_SRC_FILES := \
	cache.cpp \
	../../awplayer/cache.cpp \

LOCAL_MODULE_TAGS := optional

LOCAL_MODULE:= awplayer-cache

LOCAL_SHARED_LIBRARIES +=   \
        libutils            \
        libcutils           \
        libbinder           \

include $(BUILD_EXECUTABLE)

