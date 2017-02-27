/*
* Copyright (c) 2008-2016 Allwinner Technology Co. Ltd.
* All rights reserved.
*
* File : cdcionUtil.c
* Description : get phy addr for android
*              (it is only work in android, do not compile in LINUX)
* History :
*   Comment :
*
*
*/

#include <string.h>
#include <stdio.h>
#include <stdlib.h>
#include <errno.h>
#include <fcntl.h>
#include <stdio.h>
#include <sys/ioctl.h>
#include <sys/mman.h>
#include <sys/types.h>
#include <linux/ion.h>
#include <ion/ion.h>

#include "log.h"
#include "CdcUtil.h"

#define ION_IOC_SUNXI_FLUSH_RANGE           5
#define ION_IOC_SUNXI_FLUSH_ALL             6
#define ION_IOC_SUNXI_PHYS_ADDR             7
#define ION_IOC_SUNXI_DMA_COPY              8

#if defined(CONF_KERNEL_VERSION_3_10)
    typedef int aw_ion_user_handle_t;
#elif defined(CONF_LOLLIPOP_AND_NEWER)
    typedef int aw_ion_user_handle_t;
#else
    typedef void* aw_ion_user_handle_t;
#endif


typedef struct CDC_SUNXI_PHYS_DATA
{
    aw_ion_user_handle_t     handle;
    unsigned int  phys_addr;
    unsigned int  size;
}cdc_sunxi_phys_data;

unsigned long CdcIonGetPhyAdr(int fd, uintptr_t handle)
{
    int ret = 0;
    struct ion_custom_data custom_data;
    cdc_sunxi_phys_data phys_data;
    memset(&phys_data, 0, sizeof(cdc_sunxi_phys_data));
    CEDARC_UNUSE(phys_data.size);
    custom_data.cmd = ION_IOC_SUNXI_PHYS_ADDR;
    phys_data.handle = (aw_ion_user_handle_t)handle;
    custom_data.arg = (unsigned long)&phys_data;
    ret = ioctl(fd, ION_IOC_CUSTOM, &custom_data);
    if(ret < 0)
        return 0;
    return phys_data.phys_addr;
}

