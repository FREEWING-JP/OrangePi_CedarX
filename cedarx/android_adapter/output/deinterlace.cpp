/*
 * Copyright (c) 2008-2016 Allwinner Technology Co. Ltd.
 * All rights reserved.
 *
 * File : deinterlace.cpp
 * Description : hardware deinterlacing
 * History :
 *
 */

#include <cdx_log.h>

#include <memoryAdapter.h>

#include <string.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <linux/ioctl.h>
#include <errno.h>

#include "outputCtrl.h"
#include <iniparserapi.h>
//-----------------------------------------------------------------------------
// relation with deinterlace

#define DI_MODULE_TIMEOUT    0x1055
#define    DI_IOC_MAGIC        'D'

//DI_IOSTART use int instead of pointer* in double 32/64 platform to avoid bad definition.
#define    DI_IOCSTART             _IOWR(DI_IOC_MAGIC, 0, DiRectSizeT)
//#define    DI_IOCSTART             _IOWR(DI_IOC_MAGIC, 0, DiParaT *)

typedef enum DiPixelformatE {
    DI_FORMAT_NV12      =0x00,
    DI_FORMAT_NV21      =0x01,
    DI_FORMAT_MB32_12   =0x02, //UV mapping like NV12
    DI_FORMAT_MB32_21   =0x03, //UV mapping like NV21
} DiPixelformatE;

typedef struct DiRectSizeT {
    unsigned int nWidth;
    unsigned int nHeight;
} DiRectSizeT;

typedef struct DiFbT {
#if (CONF_KERN_BITWIDE == 64)
    unsigned long long         addr[2];  // the address of frame buffer
#else
    uintptr_t         addr[2];  // the address of frame buffer
#endif
    DiRectSizeT       mRectSize;
    DiPixelformatE    eFormat;
} DiFbT;

typedef struct DiParaT {
    DiFbT         mInputFb;           //current frame fb
    DiFbT         mPreFb;          //previous frame fb
    DiRectSizeT   mSourceRegion;   //current frame and previous frame process region
    DiFbT         mOutputFb;       //output frame fb
    DiRectSizeT   mOutRegion;       //output frame region
    unsigned int  nField;          //process field <0-top field ; 1-bottom field>
    /* video infomation <0-is not top_field_first; 1-is top_field_first> */
    unsigned int  bTopFieldFirst;
} DiParaT;

struct DiContext
{
    Deinterlace base;
    int fd;

    int picCount;
};

static int gUsed = 0;
//-----------------------------------------------------------------------------


// need to reset deinterlace when failed
static int para(Deinterlace* di, DiParaT *para)
{
    int ret = 0;
    struct DiContext* dc;

    dc = (struct DiContext*)di;

    if (-1 == dc->fd)
    {
        loge("not init...");
        return -1;
    }
    ret = ioctl(dc->fd, DI_IOCSTART, para);

    if (ret != 0) // DI_MODULE_TIMEOUT
    {
        loge("DE-Interlace work fialed!!!\n");
        return -1;
    }
    return 0;
}

static int dumpPara(DiParaT *para)
{
    logd("**************************************************************");
    logd("*********************deinterlace info*************************");

    logd(" input_fb: addr=0x%x, 0x%x, format=%d, size=(%d, %d)",
        para->mInputFb.addr[0], para->mInputFb.addr[1], (int)para->mInputFb.eFormat,
        (int)para->mInputFb.mRectSize.nWidth, (int)para->mInputFb.mRectSize.nHeight);
    logd("   pre_fb: addr=0x%x, 0x%x, format=%d, size=(%d, %d)",
        para->mPreFb.addr[0], para->mPreFb.addr[1], (int)para->mPreFb.eFormat,
        (int)para->mPreFb.mRectSize.nWidth, (int)para->mPreFb.mRectSize.nHeight);
    logd("output_fb: addr=0x%x, 0x%x, format=%d, size=(%d, %d)",
        para->mOutputFb.addr[0], para->mOutputFb.addr[1], (int)para->mOutputFb.eFormat,
        (int)para->mOutputFb.mRectSize.nWidth, (int)para->mOutputFb.mRectSize.nHeight);
    logd("top_field_first=%d, field=%d", para->bTopFieldFirst, para->nField);
    logd("source_regn=(%d, %d), out_regn=(%d, %d)",
        para->mSourceRegion.nWidth, para->mSourceRegion.nHeight,
        para->mOutRegion.nWidth, para->mOutRegion.nHeight);

    logd("****************************end*******************************");
    logd("**************************************************************\n\n");
    return 0;
}

static void __DiDestroy(Deinterlace* di)
{
    struct DiContext* dc;
    dc = (struct DiContext*)di;
    if (dc->fd != -1)
    {
        close(dc->fd);
    }
    gUsed = 0;
    free(dc);
}

static int __DiInit(Deinterlace* di)
{
    struct DiContext* dc;
    dc = (struct DiContext*)di;

    logd("%s", __FUNCTION__);
    if (dc->fd != -1)
    {
        logw("already init...");
        return 0;
    }

    dc->fd = open("/dev/deinterlace", O_RDWR);
    if (dc->fd == -1)
    {
        loge("open hw devices failure, errno(%d)", errno);
        return -1;
    }
    gUsed = 1;
    dc->picCount = 0;
    logd("hw deinterlace init success...");
    return 0;
}

static int __DiReset(Deinterlace* di)
{
    struct DiContext* dc = (struct DiContext*)di;

    logd("%s", __FUNCTION__);
    if (dc->fd != -1)
    {
        close(dc->fd);
        dc->fd = -1;
        gUsed = 0;
    }
    return __DiInit(di);
}

static enum EPIXELFORMAT __DiExpectPixelFormat(Deinterlace* di)
{
    struct DiContext* dc = (struct DiContext*)di;

    return PIXEL_FORMAT_NV21;
}

static int __DiFlag(Deinterlace* di)
{
    struct DiContext* dc = (struct DiContext*)di;

    return DE_INTERLACE_HW;
}

static int __DiProcess(Deinterlace* di,
                    VideoPicture *pPrePicture,
                    VideoPicture *pCurPicture,
                    VideoPicture *pOutPicture,
                    int nField)
{
    struct DiContext* dc;
    dc = (struct DiContext*)di;

    logv("call DeinterlaceProcess");

    if(pPrePicture == NULL || pCurPicture == NULL || pOutPicture == NULL)
    {
        loge("the input param is null : %p, %p, %p", pPrePicture, pCurPicture, pOutPicture);
        return -1;
    }

    if(dc->fd == -1)
    {
        loge("not init...");
        return -1;
    }

    DiParaT        mDiParaT;
    DiRectSizeT    mSrcSize;
    DiRectSizeT    mDstSize;
    DiPixelformatE eInFormat;
    DiPixelformatE eOutFormat;

    //* compute pts again
    if (dc->picCount < 2)
    {
        int nFrameRate = 30000; //* we set the frameRate to 30
        pOutPicture->nPts = pCurPicture->nPts + nField * (1000 * 1000 * 1000 / nFrameRate) / 2;
    }
    else
    {
        pOutPicture->nPts = pCurPicture->nPts + nField * (pCurPicture->nPts - pPrePicture->nPts)/2;
    }

    logv("pCurPicture->nPts = %lld  ms, pOutPicture->nPts = %lld ms, diff = %lld ms ",
          pCurPicture->nPts/1000,
          pOutPicture->nPts/1000,
          (pOutPicture->nPts -  pCurPicture->nPts)/1000
          );

    if (pOutPicture->ePixelFormat == PIXEL_FORMAT_NV12)
    {
        eOutFormat = DI_FORMAT_NV12;
    }
    else if (pOutPicture->ePixelFormat == PIXEL_FORMAT_NV21)
    {
        eOutFormat = DI_FORMAT_NV21;
    }
    else
    {
        loge("the outputPixelFormat is not support : %d",pOutPicture->ePixelFormat);
        return -1;
    }

    const char *str_difmt = GetConfigParamterString("deinterlace_fmt", NULL);
    CDX_LOG_CHECK(str_difmt, "'deinterlace_fmt' not define, pls check your cedarx.conf");
    if (strcmp(str_difmt, "mb32") == 0)
    {
        eInFormat = DI_FORMAT_MB32_12;
    }
    else if (strcmp(str_difmt, "nv") == 0)
    {
        if (pCurPicture->ePixelFormat == PIXEL_FORMAT_NV12)
        {
            eInFormat = DI_FORMAT_NV12;
        }
        else if (pCurPicture->ePixelFormat == PIXEL_FORMAT_NV21)
        {
            eInFormat = DI_FORMAT_NV21;
        }
        else
        {
            loge("the inputPixelFormat is not support : %d",pCurPicture->ePixelFormat);
            return -1;
        }
    }
    else if (strcmp(str_difmt, "nv21") == 0)
    {
        eInFormat = DI_FORMAT_NV21;
    }
    else
    {
        eInFormat = DI_FORMAT_NV12;
    }

    mSrcSize.nWidth  = pCurPicture->nWidth;
    mSrcSize.nHeight = pCurPicture->nHeight;
    mDstSize.nWidth  = pOutPicture->nLineStride;
    mDstSize.nHeight = pOutPicture->nHeight;
    mDiParaT.mInputFb.mRectSize  = mSrcSize;
    mDiParaT.mInputFb.eFormat    = eInFormat;
    mDiParaT.mPreFb.mRectSize    = mSrcSize;
    mDiParaT.mPreFb.eFormat      = eInFormat;
    mDiParaT.mOutputFb.mRectSize = mDstSize;
    mDiParaT.mOutputFb.eFormat   = eOutFormat;
    mDiParaT.mSourceRegion       = mSrcSize;
    mDiParaT.mOutRegion          = mSrcSize;
    mDiParaT.nField              = nField;
    mDiParaT.bTopFieldFirst      = pCurPicture->bTopFieldFirst;

    //* we can use the phy address
    mDiParaT.mInputFb.addr[0]    = pCurPicture->phyYBufAddr + CONF_VE_PHY_OFFSET;
    mDiParaT.mInputFb.addr[1]    = pCurPicture->phyCBufAddr + CONF_VE_PHY_OFFSET;
    mDiParaT.mPreFb.addr[0]      = pPrePicture->phyYBufAddr + CONF_VE_PHY_OFFSET;
    mDiParaT.mPreFb.addr[1]      = pPrePicture->phyCBufAddr + CONF_VE_PHY_OFFSET;
    mDiParaT.mOutputFb.addr[0]   = pOutPicture->phyYBufAddr + CONF_VE_PHY_OFFSET;
    mDiParaT.mOutputFb.addr[1]   = pOutPicture->phyCBufAddr + CONF_VE_PHY_OFFSET;

    logv("VideoRender_CopyFrameToGPUBuffer aw_di_setpara start");

    //dumpPara(&mDiParaT);

    if (para(di, &mDiParaT) < 0)
    {
        loge("aw_di_setcardpara failed!");
        return -1;
    }
    dc->picCount++;

    return 0;
}


static struct DeinterlaceOps mDi =
{
    .destroy           = __DiDestroy,
    .init              = __DiInit,
    .reset             = __DiReset,
    .expectPixelFormat = __DiExpectPixelFormat,
    .flag              = __DiFlag,
    .process           = __DiProcess,
};

Deinterlace* DeinterlaceCreate()
{
    struct DiContext* dc = (struct DiContext*)malloc(sizeof(struct DiContext));
    if(dc == NULL)
    {
        logw("deinterlace create failed");
        return NULL;
    }
    memset(dc, 0, sizeof(struct DiContext));

    // we must set the fd to -1; or it will close the file which fd is 0
    // when destroy
    dc->fd = -1;

    dc->base.ops = &mDi;

    return &dc->base;
}
