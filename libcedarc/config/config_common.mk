
## 1. cpu arch
ifeq ($(TARGET_ARCH_VARIANT), armv8-a)
LOCAL_CFLAGS += -DCONF_ARMV8_A
endif

ifeq ($(TARGET_ARCH_VARIANT), armv7-a-neon)
os_version = $(shell echo $(PLATFORM_VERSION) | cut -c 1-3)
ifeq ($(os_version), 7.0)
LOCAL_CFLAGS +=
else
LOCAL_CFLAGS += -DCONF_ARMV7_A_NEON
endif
endif

## 2. Android Version
##    Auto detect Android version by SDK level
SDK_LEVEL_JB42 := 17
SDK_LEVEL_KK := 19
SDK_LEVEL_L := 21
SDK_LEVEL_M := 23
SDK_LEVEL_N := 24

ifeq ($(word 1,$(sort $(PLATFORM_SDK_VERSION) $(SDK_LEVEL_N))), $(SDK_LEVEL_N))
NOUGAT_AND_NEWER = yes
LOCAL_CFLAGS += -DCONF_NOUGAT_AND_NEWER
LOCAL_CFLAGS += -DCONFIG_VE_IPC_ENABLE
else
NOUGAT_AND_NEWER = no
endif

ifeq ($(word 1,$(sort $(PLATFORM_SDK_VERSION) $(SDK_LEVEL_M))), $(SDK_LEVEL_M))
MARSHMALLOW_AND_NEWER = yes
LOCAL_CFLAGS += -DCONF_MARSHMALLOW_AND_NEWER
else
MARSHMALLOW_AND_NEWER = no
endif

ifeq ($(word 1,$(sort $(PLATFORM_SDK_VERSION) $(SDK_LEVEL_L))), $(SDK_LEVEL_L))
LOLLIPOP_AND_NEWER = yes
LOCAL_CFLAGS += -DCONF_LOLLIPOP_AND_NEWER
else
LOLLIPOP_AND_NEWER = no
endif

ifeq ($(word 1,$(sort $(PLATFORM_SDK_VERSION) $(SDK_LEVEL_KK))), $(SDK_LEVEL_KK))
KITKAT_AND_NEWER = yes
LOCAL_CFLAGS += -DCONF_KITKAT_AND_NEWER
else
KITKAT_AND_NEWER = no
endif

ifeq ($(word 1,$(sort $(PLATFORM_SDK_VERSION) $(SDK_LEVEL_JB42))), $(SDK_LEVEL_JB42))
JB42_AND_NEWER = yes
LOCAL_CFLAGS += -DCONF_JB42_AND_NEWER
else
JB42_AND_NEWER = no
endif

## 3. secure os
#on semelis secure os, we transform phy addr to secure os to operate the buffer,
#but we adjust on optee secure os, just transform vir addr.
ifeq ($(BOARD_WIDEVINE_OEMCRYPTO_LEVEL), 1)
PLATFORM_SURPPORT_SECURE_OS = yes
LOCAL_CFLAGS +=-DPLATFORM_SURPPORT_SECURE_OS=1

    ifeq ($(SECURE_OS_OPTEE), yes)
        LOCAL_CFLAGS +=-DSECURE_OS_OPTEE=1
        LOCAL_CFLAGS +=-DADJUST_ADDRESS_FOR_SECURE_OS_OPTEE=1
    else
        LOCAL_CFLAGS +=-DSECURE_OS_OPTEE=0
        LOCAL_CFLAGS +=-DADJUST_ADDRESS_FOR_SECURE_OS_OPTEE=0
    endif

else
PLATFORM_SURPPORT_SECURE_OS = no
LOCAL_CFLAGS +=-DPLATFORM_SURPPORT_SECURE_OS=0
endif

#endif ## 'BOARD_WIDEVINE_OEMCRYPTO_LEVEL == 1'
