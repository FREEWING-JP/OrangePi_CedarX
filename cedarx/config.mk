#############################################################################
############################## configuration. ############################### 
#############################################################################

########## configure CONF_ANDROID_VERSION ##########
CONF_ANDROID_VERSION = $(shell echo $(PLATFORM_VERSION) | cut -c 1-3)
ifeq ($(CONF_ANDROID_VERSION), 4.2)
	LOCAL_CFLAGS += -DCONF_ANDROID_MAJOR_VER=4
	LOCAL_CFLAGS += -DCONF_ANDROID_SUB_VER=2
else ifeq ($(CONF_ANDROID_VERSION), 4.4)
	LOCAL_CFLAGS += -DCONF_ANDROID_MAJOR_VER=4
	LOCAL_CFLAGS += -DCONF_ANDROID_SUB_VER=4
else ifeq ($(CONF_ANDROID_VERSION), 5.0)
	LOCAL_CFLAGS += -DCONF_ANDROID_MAJOR_VER=5
	LOCAL_CFLAGS += -DCONF_ANDROID_SUB_VER=0
else ifeq ($(CONF_ANDROID_VERSION), 5.1)
	LOCAL_CFLAGS += -DCONF_ANDROID_MAJOR_VER=5
	LOCAL_CFLAGS += -DCONF_ANDROID_SUB_VER=1
else ifeq ($(CONF_ANDROID_VERSION), 6.0)
	LOCAL_CFLAGS += -DCONF_ANDROID_MAJOR_VER=6
	LOCAL_CFLAGS += -DCONF_ANDROID_SUB_VER=0
else ifeq ($(CONF_ANDROID_VERSION), 7.0)
	LOCAL_CFLAGS += -DCONF_ANDROID_MAJOR_VER=7
	LOCAL_CFLAGS += -DCONF_ANDROID_SUB_VER=0
	LOCAL_32_BIT_ONLY := true
else ifeq ($(CONF_ANDROID_VERSION), 7.1)
        LOCAL_CFLAGS += -DCONF_ANDROID_MAJOR_VER=7
        LOCAL_CFLAGS += -DCONF_ANDROID_SUB_VER=1
        LOCAL_32_BIT_ONLY := true
else
    $(warning "not support android version: "$(CONF_ANDROID_VERSION))
endif

########## configure CONFIG_TARGET_PRODUCT ##########
LIB_CEDARM_PATH := $(TOP)/frameworks/av/media/libcedarx

CONF_AC_PRODUCT = $(shell echo $(TARGET_PRODUCT) | cut -d '_' -f 1)
include $(LIB_CEDARM_PATH)/config/$(CONF_AC_PRODUCT)_config.mk

###################################end define####################################

