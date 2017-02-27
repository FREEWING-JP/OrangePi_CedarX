/*
 * Copyright (c) 2008-2016 Allwinner Technology Co. Ltd.
 * All rights reserved.
 *
 * File : videoRenderComponent_newDisplay.cpp
 * Description : video render component
 * History :
 *
 */

//#define CONFIG_LOG_LEVEL    OPTION_LOG_LEVEL_DETAIL
//#define LOG_TAG "videoRenderComponent_newDisplay"
#include "cdx_log.h"

#include <pthread.h>
#include <semaphore.h>
#include <malloc.h>
#include <memory.h>
#include <time.h>

#include "baseComponent.h"
#include "videoRenderComponent.h"
#include "AwMessageQueue.h"
#include "layerControl.h"
#include "memoryAdapter.h"
#include <deinterlace.h>

#include<sys/types.h>
#include<sys/stat.h>
#include<fcntl.h>
#include <linux/ioctl.h>

#define USE_DETNTERLACE 1

enum VIDEO_RENDER_RESULT
{
    VIDEO_RENDER_MESSAGE_COME      = 1,
    VIDEO_RENDER_DEINTERLACE_RESET = 2,
    VIDEO_RENDER_DROP_THE_PICTURE  = 3,
    VIDEO_RENDER_THREAD_CONTINUE   = 4,
};

struct VideoRenderComp
{
    AwMessageQueue      *mq;
    BaseCompCtx          base;

    pthread_t            sRenderThread;

    enum EPLAYERSTATUS   eStatus;
    void*                pNativeWindow;

    LayerCtrl*           pLayerCtrl;
    VideoDecComp*        pDecComp;

    enum EPICTURE3DMODE  ePicture3DMode;
    enum EDISPLAY3DMODE  eDisplay3DMode;

    //* objects set by user.
    AvTimer*             pAvTimer;
    PlayerCallback       callback;
    void*                pUserData;
    int                  bEosFlag;

    //*
    int                  bResolutionChange;

    int                  bHadSetLayerInfoFlag;
    int                  bProtectedBufferFlag;//* 1: mean the video picture is secure

    //* for 3D video stream
    int                  bVideoWithTwoStream;

    //* for deinterlace
    Deinterlace         *di;
    int                  bDeinterlaceFlag;
    FbmBufInfo           mFbmBufInfo;

    //******
    int                  bFirstPictureShowed;
    int                  bNeedResetLayerParams;
    int                  bHideVideo;
    VideoPicture*        pPicture;
    VideoPicture*        pPrePicture;
    int                  bHadGetVideoFbmBufInfoFlag;
    int                  nDeinterlaceDispNum;
    int                  nGpuBufferNum;
    int                  bHadSetBufferToDecoderFlag;
    VideoPicture*        pCancelPicture[4];

    VideoPicture*        pDiOutPicture;
    int                  bResetBufToDecoderFlag;
    int                  bHadRequestReleasePicFlag;
    int                  nNeedReleaseBufferNum;

    FramerateEstimater  *pFrameRateEstimater;

    int                  ptsSecs;

    int                  bHoldLastPicture;
    int                  bFirstPtsNotified;
    float                fPlayRate;
};

static void handleStart(AwMessage *msg, void *arg);
static void handleStop(AwMessage *msg, void *arg);
static void handlePause(AwMessage *msg, void *arg);
static void handleReset(AwMessage *msg, void *arg);
static void handleSetEos(AwMessage *msg, void *arg);
static void handleQuit(AwMessage *msg, void *arg);
static void doRender(AwMessage *msg, void *arg);

static void* VideoRenderThread(void* arg);


static void NotifyVideoSizeAndSetDisplayRegion(VideoRenderComp* p);

static inline int ProcessVideoSync(VideoRenderComp* p, VideoPicture* pPicture);
static int QueueBufferToShow(VideoRenderComp* p, VideoPicture* pPicture);
static int ProcessDeinterlace(VideoRenderComp* p, int nDeinterlaceTime);
static int RenderGetVideoFbmBufInfo(VideoRenderComp* p, LayerCtrl *lc);

static int SetGpuBufferToDecoder(VideoRenderComp *p);

static int ResetBufToDecoder(VideoRenderComp *p, LayerCtrl *newLc);

VideoRenderComp* VideoRenderCompCreate(void)
{
    VideoRenderComp* p;
    int err;

    p = (VideoRenderComp*)calloc(1, sizeof(*p));
    if(p == NULL)
    {
        loge("memory alloc fail.");
        return NULL;
    }

    p->nDeinterlaceDispNum = 1;
    p->bVideoWithTwoStream = -1;
    p->mq = AwMessageQueueCreate(4, "VideoRenderMq");
    if(p->mq == NULL)
    {
        loge("video render component create message queue fail.");
        free(p);
        return NULL;
    }

    BaseMsgHandler handler = {
        .start = handleStart,
        .stop = handleStop,
        .pause = handlePause,
        .reset = handleReset,
        .setEos = handleSetEos,
        .quit = handleQuit,
        .render = doRender,
    };

    if (BaseCompInit(&p->base, "video render", p->mq, &handler))
    {
        AwMessageQueueDestroy(p->mq);
        free(p);
        return NULL;
    }

    p->eStatus = PLAYER_STATUS_STOPPED;
    p->ptsSecs = -1;
    p->fPlayRate = 1.0f;

    //p->di = CdxDiCreate();
    //if (!p->di)
    //{
    //    logw("No deinterlace...");
    //}

    err = pthread_create(&p->sRenderThread, NULL, VideoRenderThread, p);
    if(err != 0)
    {
        loge("video render component create thread fail.");
        BaseCompDestroy(&p->base);
        AwMessageQueueDestroy(p->mq);
        free(p);
        return NULL;
    }

    return p;
}

int VideoRenderCompDestroy(VideoRenderComp* p)
{
    BaseCompQuit(&p->base, NULL, NULL);
    pthread_join(p->sRenderThread, NULL);
    BaseCompDestroy(&p->base);

    AwMessageQueueDestroy(p->mq);
    free(p);

    return 0;
}

int VideoRenderCompStart(VideoRenderComp* p)
{
    return BaseCompStart(&p->base, NULL, NULL);
}

int VideoRenderCompStop(VideoRenderComp* p)
{
    return BaseCompStop(&p->base, NULL, NULL);
}

int VideoRenderCompPause(VideoRenderComp* p)
{
    return BaseCompPause(&p->base, NULL, NULL);
}

enum EPLAYERSTATUS VideoRenderCompGetStatus(VideoRenderComp* p)
{
    return p->eStatus;
}

int VideoRenderCompReset(VideoRenderComp* p)
{
    return BaseCompReset(&p->base, 0, NULL, NULL);
}

int VideoRenderCompSetEOS(VideoRenderComp* p)
{
    return BaseCompSetEos(&p->base, NULL, NULL);
}

int VideoRenderCompSetCallback(VideoRenderComp* p, PlayerCallback callback, void* pUserData)
{
    p->callback  = callback;
    p->pUserData = pUserData;

    return 0;
}

int VideoRenderCompSetTimer(VideoRenderComp* p, AvTimer* timer)
{
    p->pAvTimer = timer;
    return 0;
}

static inline void ContinueRender(VideoRenderComp* p)
{
    if (p->eStatus == PLAYER_STATUS_STARTED ||
            (p->eStatus == PLAYER_STATUS_PAUSED &&
             p->bFirstPictureShowed == 0))
        BaseCompContinue(&p->base);
}

static void handleSetDeinterlace(AwMessage *msg, void *arg)
{
    VideoRenderComp *p = arg;
    p->di = msg->opaque;
    ContinueRender(p);
}

int VideoRenderCompSetDeinterlace(VideoRenderComp* p, Deinterlace* pDi)
{
    AwMessage msg = {
        .messageId = MESSAGE_ID_SET_DI,
        .replySem = NULL,
        .opaque = pDi,
        .execute = handleSetDeinterlace,
    };

    logd("video render component setting deinterlace: %p",pDi);

    if(AwMessageQueuePostMessage(p->mq, &msg) != 0)
    {
        loge("fatal error, video render component post message fail.");
        abort();
    }

    return 0;
}

static void returnOldPic(VideoRenderComp *p)
{
    if(p->pPicture != NULL)
    {
        VideoDecCompReturnPicture(p->pDecComp, p->pPicture);
        p->pPicture = NULL;
    }

    if(p->pPrePicture != NULL)
    {
        VideoDecCompReturnPicture(p->pDecComp, p->pPrePicture);
        p->pPrePicture = NULL;
    }

    if(p->pDiOutPicture != NULL)
    {
        if(p->pLayerCtrl != NULL)
            LayerQueueBuffer(p->pLayerCtrl, p->pDiOutPicture, 0);
        p->pDiOutPicture = NULL;
    }
}

static void setLayerInfo(VideoRenderComp *p)
{
    enum EPIXELFORMAT eDisplayPixelFormat = PIXEL_FORMAT_DEFAULT;
    FbmBufInfo* pFbmBufInfo = &p->mFbmBufInfo;

     //* we init deinterlace device here
    if(p->di != NULL && pFbmBufInfo->bProgressiveFlag == 0 &&
            USE_DETNTERLACE)
    {
        if (CdxDiInit(p->di) == 0)
        {
            int di_flag = CdxDiFlag(p->di);
            p->bDeinterlaceFlag   = 1;
            p->nDeinterlaceDispNum   = (di_flag == DE_INTERLACE_HW) ? 2 : 1;
        }
        else
        {
            logw("open deinterlace failed , don't use deinterlace!");
        }
    }

    eDisplayPixelFormat = p->bDeinterlaceFlag ?
        CdxDiExpectPixelFormat(p->di) : (enum EPIXELFORMAT)pFbmBufInfo->ePixelFormat;

    LayerSetDisplayPixelFormat(p->pLayerCtrl, eDisplayPixelFormat);
    LayerSetVideoWithTwoStreamFlag(p->pLayerCtrl, p->bVideoWithTwoStream);
    LayerSetIsSoftDecoderFlag(p->pLayerCtrl, pFbmBufInfo->bIsSoftDecoderFlag);
    LayerSetDisplayBufferSize(p->pLayerCtrl, pFbmBufInfo->nBufWidth, pFbmBufInfo->nBufHeight);
    p->nGpuBufferNum = LayerSetDisplayBufferCount(p->pLayerCtrl, pFbmBufInfo->nBufNum);

    p->bHadSetLayerInfoFlag = 1;
}

static void handleSetWindow(AwMessage *msg, void *arg)
{
    VideoRenderComp *p = arg;
    logd("process MESSAGE_ID_SET_WINDOW message, p->pPicture(%p)",p->pPicture);

    returnOldPic(p);
    LayerCtrl *lc = msg->opaque;

    // if the video Window change
    if(p->pNativeWindow != NULL) //old nativewindow
    {
        //* On the new-displayer of android, we not call LayerRelease,
        //* just reset nativeWindow.
        VideoPicture* pPicture = NULL;
        int nWhileNum = 0;
        while(1)
        {
            nWhileNum++;
            if(nWhileNum >= 100)
            {
                loge("get pic node time more than 100, it is wrong");
                break;
            }

            pPicture = LayerGetBufferOwnedByGpu(p->pLayerCtrl);
            if(pPicture == NULL)
            {
                break;
            }
            VideoDecCompReturnPicture(p->pDecComp, pPicture);
        }

        p->pNativeWindow = lc->pNativeWindow;
        LayerResetNativeWindow(p->pLayerCtrl, p->pNativeWindow); //release old resource,  set new nw.
        VideoDecCompSetVideoFbmBufRelease(p->pDecComp);
        p->bResetBufToDecoderFlag = 1;
        p->nNeedReleaseBufferNum  = p->nGpuBufferNum;

        RenderGetVideoFbmBufInfo(p, lc);

        nWhileNum = 0;
        while(p->bResetBufToDecoderFlag == 1)
        {
            int ret = ResetBufToDecoder(p, lc);
            if(ret == 1 || ret == 2)
            {
                logd("ret=%d", ret);
                nWhileNum++;
                usleep(5000);
                if(nWhileNum >= 10)
                {
                    logw("ResetBufToDecoder fail more than 10 times, check...");
                    break;
                }
            }
        }
        p->pLayerCtrl = lc; //set new lc.

        goto set_nativeWindow_exit;
    }

    p->pLayerCtrl = lc;

    p->pNativeWindow = lc->pNativeWindow;

    //* on linux, pNativeWindow == NULL, and the LayerCtrl module will
    //* create a layer to show video picture.
    if(p->pLayerCtrl != NULL)
        LayerSetSecureFlag(p->pLayerCtrl, p->bProtectedBufferFlag);

    p->bNeedResetLayerParams = 1;

    //* we should set layer info here if it hadn't set it
    if(p->bHadSetLayerInfoFlag == 0 && p->mFbmBufInfo.nBufNum != 0)
        setLayerInfo(p);

set_nativeWindow_exit:
    sem_post(msg->replySem);
    ContinueRender(p);
}

int VideoRenderCompSetWindow(VideoRenderComp* p, LayerCtrl* lc)
{
    if(lc == NULL)
        return -1;

    sem_t replySem;
    sem_init(&replySem, 0, 0);

    AwMessage msg = {
        .messageId = MESSAGE_ID_SET_WINDOW,
        .replySem = &replySem,
        .opaque = lc,
        .execute = handleSetWindow,
    };

    logd("video render component setting window: %p", lc);

    if(AwMessageQueuePostMessage(p->mq, &msg) != 0)
    {
        loge("fatal error, video render component post message fail.");
        abort();
    }

    if(SemTimedWait(&replySem, -1) < 0)
    {
        loge("video render component wait for setting window finish failed.");
        sem_destroy(&replySem);
        return -1;
    }

    sem_destroy(&replySem);
    return 0;
}

int VideoRenderCompSetDecodeComp(VideoRenderComp* p, VideoDecComp* d)
{
    p->pDecComp  = d;
    return 0;
}

int VideoRenderSet3DMode(VideoRenderComp* p,
                         enum EPICTURE3DMODE ePicture3DMode,
                         enum EDISPLAY3DMODE eDisplay3DMode)
{
    logv("video render component setting 3d mode.");

    // These two variables are useless now.
    p->ePicture3DMode = ePicture3DMode;
    p->eDisplay3DMode = eDisplay3DMode;
    return 0;
}

int VideoRenderGet3DMode(VideoRenderComp* p,
                         enum EPICTURE3DMODE* ePicture3DMode,
                         enum EDISPLAY3DMODE* eDisplay3DMode)
{
    *ePicture3DMode = p->ePicture3DMode;
    *eDisplay3DMode = p->eDisplay3DMode;
    return 0;
}

int VideoRenderSetPlayRate(VideoRenderComp* p,float rate)
{
    p->fPlayRate = rate;
    return 0;
}

static void handleVideoHide(AwMessage *msg, void *arg)
{
    VideoRenderComp *p = arg;

    p->bHideVideo = msg->int64Value;

    if (p->pLayerCtrl != NULL)
    {
        if (p->bHideVideo)
            LayerCtrlHideVideo(p->pLayerCtrl);
        else if (p->bFirstPictureShowed == 1)
            LayerCtrlShowVideo(p->pLayerCtrl);
    }

    sem_post(msg->replySem);
    ContinueRender(p);
}

int VideoRenderVideoHide(VideoRenderComp* p, int bHideVideo)
{
    sem_t replySem;
    sem_init(&replySem, 0, 0);

    AwMessage msg = {
        .messageId = MESSAGE_ID_SET_VIDEO_HIDE,
        .replySem = &replySem,
        .int64Value = bHideVideo,
        .execute = handleVideoHide,
    };

    logv("video render component setting video hide(%d).", bHideVideo);

    if(AwMessageQueuePostMessage(p->mq, &msg) != 0)
    {
        loge("fatal error, video render component post message fail.");
        abort();
    }

    if(SemTimedWait(&replySem, -1) < 0)
    {
        loge("video render component wait for setting 3d mode finish failed.");
        sem_destroy(&replySem);
        return -1;
    }

    sem_destroy(&replySem);
    return 0;
}

static void handleSetHoldLastPicture(AwMessage *msg, void *arg)
{
    VideoRenderComp *p = arg;
    p->bHoldLastPicture = msg->int64Value;

    if (p->pLayerCtrl && p->eStatus == PLAYER_STATUS_STOPPED)
    {
        LayerCtrlHoldLastPicture(p->pLayerCtrl, p->bHoldLastPicture);

        if (!p->bHoldLastPicture)
            LayerCtrlHideVideo(p->pLayerCtrl);
    }

    ContinueRender(p);
}

int VideoRenderSetHoldLastPicture(VideoRenderComp* p, int bHold)
{
    AwMessage msg = {
        .messageId = MESSAGE_ID_SET_HOLD_LAST_PICTURE,
        .replySem = NULL,
        .int64Value = bHold,
        .execute = handleSetHoldLastPicture,
    };

    logv("video render component setting hold last picture(bHold=%d).", bHold);

    if(AwMessageQueuePostMessage(p->mq, &msg) != 0)
    {
        loge("fatal error, video render component post message fail.");
        abort();
    }

    return 0;
}

void VideoRenderCompSetProtecedFlag(VideoRenderComp* p, int bProtectedFlag)
{
    p->bProtectedBufferFlag = bProtectedFlag;
    return;
}

int VideoRenderCompSetVideoStreamInfo(VideoRenderComp* v, VideoStreamInfo* pStreamInfo)
{
    CEDARX_UNUSE(v);
    CEDARX_UNUSE(pStreamInfo);
    return 0;
}

int VideoRenderCompSetSyncFirstPictureFlag(VideoRenderComp* v, int bSyncFirstPictureFlag)
{
    //*TODO
    (void)v;
    (void)bSyncFirstPictureFlag;
    return 0;
}

int VideoRenderCompSetFrameRateEstimater(VideoRenderComp* p, FramerateEstimater* fe)
{
    p->pFrameRateEstimater = fe;
    return 0;
}

static void* VideoRenderThread(void* arg)
{
    VideoRenderComp *p = arg;
    AwMessage msg;

    while (AwMessageQueueGetMessage(p->mq, &msg) == 0)
    {
        if (msg.execute != NULL)
            msg.execute(&msg, p);
        else
            loge("msg with msg_id %d doesn't have a handler", msg.messageId);
    }

    return NULL;
}

static void handleStart(AwMessage *msg, void *arg)
{
    VideoRenderComp *p = arg;

    if(p->eStatus == PLAYER_STATUS_STARTED)
    {
        logw("already in started status.");
        *msg->result = -1;
        sem_post(msg->replySem);
        BaseCompContinue(&p->base);
        return;
    }

    if(p->eStatus == PLAYER_STATUS_STOPPED)
    {
        p->bFirstPictureShowed = 0;
        p->bFirstPtsNotified = 0;
        p->bEosFlag = 0;
    }

    p->eStatus = PLAYER_STATUS_STARTED;
    *msg->result = 0;
    sem_post(msg->replySem);

    BaseCompContinue(&p->base);
}

static void handleStop(AwMessage *msg, void *arg)
{
    VideoRenderComp *p = arg;

    if(p->eStatus == PLAYER_STATUS_STOPPED)
    {
        logw("already in stopped status.");
        *msg->result = -1;
        sem_post(msg->replySem);
        return;
    }

    returnOldPic(p);

    if(p->pLayerCtrl != NULL)
    {
        LayerCtrlHoldLastPicture(p->pLayerCtrl, p->bHoldLastPicture);

        if (!p->bHoldLastPicture)
            LayerCtrlHideVideo(p->pLayerCtrl);
    }

    //* set status to stopped.
    p->eStatus = PLAYER_STATUS_STOPPED;
    *msg->result = 0;
    sem_post(msg->replySem);
}

static void handlePause(AwMessage *msg, void *arg)
{
    VideoRenderComp *p = arg;

    if(p->eStatus != PLAYER_STATUS_STARTED  &&
       !(p->eStatus == PLAYER_STATUS_PAUSED && p->bFirstPictureShowed == 0))
    {
        logw("not in started status, pause operation invalid.");
        *msg->result = -1;
        sem_post(msg->replySem);
        return;
    }

    p->eStatus = PLAYER_STATUS_PAUSED;

    *msg->result = 0;
    sem_post(msg->replySem);

    if(p->bFirstPictureShowed == 0)
        BaseCompContinue(&p->base);
}

static void handleQuit(AwMessage *msg, void *arg)
{
    VideoRenderComp *p = arg;

    returnOldPic(p);

    sem_post(msg->replySem);
    p->eStatus = PLAYER_STATUS_STOPPED;

    if (p->pLayerCtrl != NULL)
    {
        LayerRelease(p->pLayerCtrl);
        p->pLayerCtrl = NULL;
    }

    pthread_exit(NULL);
}

static void handleReset(AwMessage *msg, void *arg)
{
    VideoRenderComp *p = arg;

    returnOldPic(p);

    p->bEosFlag = 0;
    p->bFirstPictureShowed = 0;
    p->bFirstPtsNotified = 0;

#if defined(CONF_PTS_TOSF)
    LayerControl(p->pLayerCtrl, CDX_LAYER_CMD_RESTART_SCHEDULER, NULL);
#endif

    *msg->result = 0;
    sem_post(msg->replySem);
    ContinueRender(p);
}

static void handleSetEos(AwMessage *msg, void *arg)
{
    VideoRenderComp *p = arg;

    p->bEosFlag = 1;
    sem_post(msg->replySem);

    ContinueRender(p);
}

static int requestPicture(VideoRenderComp *p)
{
    while(p->pPicture == NULL)
    {
        p->pPicture = VideoDecCompRequestPicture(p->pDecComp, 0, &p->bResolutionChange);
        logv("get picture, picture %p", p->pPicture);
        if(p->pPicture != NULL || (p->pPicture == NULL && p->bEosFlag))
            break;

        if(p->bResolutionChange)
        {
            //* reopen the video engine.
            VideoDecCompReopenVideoEngine(p->pDecComp);
            //* reopen the layer.
            if(p->pLayerCtrl != NULL)
            {
                LayerReset(p->pLayerCtrl);
                p->bNeedResetLayerParams = 1;
            }
            p->bResolutionChange          = 0;
            p->bHadSetLayerInfoFlag       = 0;
            p->bHadGetVideoFbmBufInfoFlag = 0;
            p->bHadSetBufferToDecoderFlag = 0;

            BaseCompContinue(&p->base);
            return -1;
        }

        if (AwMessageQueueWaitMessage(p->mq, 5) > 0)
            return -1;
    }

    if(p->pPicture == NULL && p->bEosFlag == 1)
    {
        p->callback(p->pUserData, PLAYER_VIDEO_RENDER_NOTIFY_EOS, NULL);
        return -1;
    }

    return 0;
}

static int notifyFirstPts(VideoRenderComp *p)
{
    /* this callback may block because the player need wait
     * audio first frame to sync.
     */
    int ret = p->callback(p->pUserData, PLAYER_VIDEO_RENDER_NOTIFY_FIRST_PICTURE,
            (void*)&p->pPicture->nPts);
    if(ret == TIMER_DROP_VIDEO_DATA)
    {
        //* video first frame pts small (too much) than the audio,
        //* discard this frame to catch up the audio.
        VideoDecCompReturnPicture(p->pDecComp, p->pPicture);
        p->pPicture = NULL;
        BaseCompContinue(&p->base);
        return -1;
    }
    else if(ret == TIMER_NEED_NOTIFY_AGAIN)
    {
        /* waiting process for first frame sync with audio is
         * broken by a new message to player, so the player tell
         * us to notify again later.
         */
        if (AwMessageQueueWaitMessage(p->mq, 10) <= 0)
            BaseCompContinue(&p->base);
        return -1;
    }

    p->bFirstPtsNotified = 1;
    return 0;
}

static int showProgressivePicture(VideoRenderComp *p)
{
#if defined(CONFIG_DTV)
    //* the first picture is showed unsychronized.
    if(p->bFirstPictureShowed == 0)
    {
        VideoPicture* pReturnPicture = NULL;

        QueueBufferToShow(p, p->pPicture);

        LayerDequeueBuffer(p->pLayerCtrl, &pReturnPicture, 0);
        VideoDecCompReturnPicture(p->pDecComp, pReturnPicture);
        p->pPicture = NULL;

        return 0;
    }
#endif

    //* wait according to the presentation time stamp.
    if(p->bFirstPictureShowed == 0)
        return 0;

    int ret = ProcessVideoSync(p, p->pPicture);
    if(ret == VIDEO_RENDER_MESSAGE_COME)
    {
        return -1;
    }
    else if(ret == VIDEO_RENDER_DROP_THE_PICTURE)
    {
        VideoDecCompReturnPicture(p->pDecComp, p->pPicture);
        p->pPicture = NULL;
        return -1;
    }
    else
    {
        VideoPicture* pReturnPicture = NULL;
        QueueBufferToShow(p, p->pPicture);
        LayerDequeueBuffer(p->pLayerCtrl, &pReturnPicture, 0);
        VideoDecCompReturnPicture(p->pDecComp, pReturnPicture);
        p->pPicture = NULL;
    }

    return 0;
}

static int showInterlacedPicture(VideoRenderComp *p)
{
    int result = 0;
    int n;
    for(n = 0; n < p->nDeinterlaceDispNum; n++)
    {
        int ret = ProcessDeinterlace(p, n);
        if(ret == VIDEO_RENDER_DEINTERLACE_RESET)
        {
            n = 0;
            continue;
        }
        else if (ret == -1)
        {
            loge("process deinterlace fail!");
            p->pDiOutPicture = NULL;
            CdxDiReset(p->di);
            result = -1;
            break;
        }

        // ***************** field error
        if(p->pPicture->bTopFieldError && n == 0)
        {
            logd("++++ top field error");
            LayerQueueBuffer(p->pLayerCtrl, p->pDiOutPicture, 0);
            p->pDiOutPicture = NULL;
            CdxDiReset(p->di);
            continue;
        }

        if((p->pPicture->bTopFieldError || p->pPicture->bBottomFieldError)
            && n == 1)
        {
            logd("+++++ bottom field error");
            LayerQueueBuffer(p->pLayerCtrl, p->pDiOutPicture, 0);
            p->pDiOutPicture = NULL;
            CdxDiReset(p->di);
            result = -1;
            break;
        }

        if(p->bFirstPictureShowed != 0)
        {
            ret = ProcessVideoSync(p, p->pDiOutPicture);
            if(ret == VIDEO_RENDER_MESSAGE_COME)
            {
                LayerQueueBuffer(p->pLayerCtrl, p->pDiOutPicture, 0);
                p->pDiOutPicture = NULL;
                result = -1;
                break;
            }
            else if(ret == VIDEO_RENDER_DROP_THE_PICTURE)
            {
                LayerQueueBuffer(p->pLayerCtrl, p->pDiOutPicture, 0);
                p->pDiOutPicture = NULL;
                result = -1;
                break;
            }
        }

        QueueBufferToShow(p, p->pDiOutPicture);
        p->pDiOutPicture = NULL;
    }

    if(p->pPicture != p->pPrePicture && p->pPrePicture != NULL)
    {
        VideoDecCompReturnPicture(p->pDecComp, p->pPrePicture);
    }

    p->pPrePicture = p->pPicture;
    p->pPicture = NULL;

    return result;
}

static int checkFlags(VideoRenderComp *p)
{
    //* when nativeWindow change ,we should reset buffer to decoder
 #if 0
    while(p->bResetBufToDecoderFlag == 1)
    {
        int ret = ResetBufToDecoder(p);
        if(ret == VIDEO_RENDER_THREAD_CONTINUE)
        {
            if (AwMessageQueueWaitMessage(p->mq, 5) <= 0)
                BaseCompContinue(&p->base);
            return -1;
        }
        else if (AwMessageQueueWaitMessage(p->mq, 5) > 0)
        {
            return -1;
        }
    }
#endif
    logv("bHadGetVideoFbmBufInfoFlag %d",p->bHadGetVideoFbmBufInfoFlag);
    //* get video fbm buf info
    while(p->bHadGetVideoFbmBufInfoFlag == 0)
    {
        if(p->bEosFlag == 1)
        {
            p->callback(p->pUserData, PLAYER_VIDEO_RENDER_NOTIFY_EOS, NULL);
            return -1;
        }

        if(RenderGetVideoFbmBufInfo(p, p->pLayerCtrl) == 0)
            p->bHadGetVideoFbmBufInfoFlag = 1;
        else if (AwMessageQueueWaitMessage(p->mq, 5) > 0)
            return -1;
    }

    logv("bHadSetBufferToDecoderFlag %d",p->bHadSetBufferToDecoderFlag);
    //* set buffer to decoder
    while(p->bHadSetBufferToDecoderFlag == 0)
    {
        if(p->bHadSetLayerInfoFlag)
        {
            SetGpuBufferToDecoder(p);
            p->bHadSetBufferToDecoderFlag = 1;
        }
        else if (AwMessageQueueWaitMessage(p->mq, 5) > 0)
        {
            return -1;
        }
    }

    return 0;
}

static void doRender(AwMessage *msg, void *arg)
{
    VideoRenderComp *p = arg;
    (void)msg;

    if(p->eStatus != PLAYER_STATUS_STARTED &&
      !(p->eStatus == PLAYER_STATUS_PAUSED && p->bFirstPictureShowed == 0))
    {
        logw("not in started status, render message ignored.");
        return;
    }

    if (checkFlags(p) != 0)
        return;

    if (p->pPicture == NULL)
    {
        if (requestPicture(p) != 0)
            return;

        if(p->bFirstPictureShowed == 0 || p->bNeedResetLayerParams == 1)
        {
            NotifyVideoSizeAndSetDisplayRegion(p);
            p->bNeedResetLayerParams = 0;
        }
    }

    /************************************************************
     * notify the first sync frame to set timer. the first sync
     * frame is the second picture, the first picture need to be
     * showed as soon as we can.(unsynchroized)
     ***********************************************************/
    if(p->bFirstPictureShowed && p->bFirstPtsNotified == 0)
    {
        if (notifyFirstPts(p) != 0)
            return;
    }

    //******************************************************
    //* sync and show the picture
    //******************************************************
    logv("** p->bDeinterlaceFlag[%d]",p->bDeinterlaceFlag);
    if(p->bDeinterlaceFlag == 0)
    {
        if (showProgressivePicture(p) != 0)
            return;
    }
    else
    {
        if (showInterlacedPicture(p) != 0)
            return;
    }

    if(p->bFirstPictureShowed == 0)
        p->bFirstPictureShowed = 1;

    if(p->eStatus == PLAYER_STATUS_STARTED)
    {
        BaseCompContinue(&p->base);
    }
    else
    {
        logi("first picture showed at paused status.");
    }
}

static int IsVideoWithTwoStream(VideoDecComp* pDecComp)
{
    VideoStreamInfo videoStreamInfo;
    if(VideoDecCompGetVideoStreamInfo(pDecComp, &videoStreamInfo) == 0)
        return videoStreamInfo.bIs3DStream;
    return 0;
}

static void NotifyVideoSizeAndSetDisplayRegion(VideoRenderComp* p)
{
    int size[4];

    if((p->pPicture->nRightOffset - p->pPicture->nLeftOffset) > 0 &&
       (p->pPicture->nBottomOffset - p->pPicture->nTopOffset) > 0)
    {
        int width = p->pPicture->nRightOffset - p->pPicture->nLeftOffset;
        int height = p->pPicture->nBottomOffset - p->pPicture->nTopOffset;
        size[0] = width;
        size[1] = height;
        size[2] = 0;
        size[3] = 0;
        p->callback(p->pUserData, PLAYER_VIDEO_RENDER_NOTIFY_VIDEO_SIZE, (void*)size);

        size[0] = p->pPicture->nLeftOffset;
        size[1] = p->pPicture->nTopOffset;
        size[2] = width;
        size[3] = height;
        p->callback(p->pUserData, PLAYER_VIDEO_RENDER_NOTIFY_VIDEO_CROP, (void*)size);

        if(p->pLayerCtrl != NULL)
        {
            LayerSetDisplayRegion(p->pLayerCtrl, size[0], size[1], size[2], size[3]);
        }
    }
    else
    {
        logw("the offsets of picture are not right, we set bufferWidht and \
              bufferHeight as video size, this maybe wrong, offset: %d, %d, %d, %d",
              p->pPicture->nLeftOffset,p->pPicture->nRightOffset,
              p->pPicture->nTopOffset,p->pPicture->nBottomOffset);
        size[0] = p->pPicture->nWidth;
        size[1] = p->pPicture->nHeight;
        size[2] = 0;
        size[3] = 0;
        p->callback(p->pUserData, PLAYER_VIDEO_RENDER_NOTIFY_VIDEO_SIZE, (void*)size);

        if(p->pLayerCtrl != NULL)
        {
            LayerSetDisplayRegion(p->pLayerCtrl,
                                  0,
                                  0,
                                  p->pPicture->nWidth,
                                  p->pPicture->nHeight);
        }
    }

    return ;
}

static inline int ProcessVideoSync(VideoRenderComp* p, VideoPicture* pPicture)
{
    int nWaitTime;

    nWaitTime = p->callback(p->pUserData, PLAYER_VIDEO_RENDER_NOTIFY_PICTURE_PTS,
            (void*)&pPicture->nPts);

#if defined(CONF_PTS_TOSF)
    return 0;
#endif

    double frameRate = 0.0;
    if (p->pFrameRateEstimater)
        frameRate = FramerateEstimaterGetFramerate(p->pFrameRateEstimater);
    if (frameRate <= 0.0)
        frameRate = pPicture->nFrameRate;
    if (frameRate > 1000.0)
        frameRate /= 1000.0;

    double frameDuration = 0.0;
    if (frameRate > 0.0)
        frameDuration = 1000.0 / frameRate;

    if(nWaitTime > 0)
    {
        if (p->fPlayRate == 1.0)
        {
            if ((frameDuration > 0.0) && (nWaitTime > frameDuration * 2))
                nWaitTime = frameDuration * 2;

            if (nWaitTime > 500)
                nWaitTime = 500;
        }
        if (AwMessageQueueWaitMessage(p->mq, nWaitTime) > 0)
            return VIDEO_RENDER_MESSAGE_COME;
    }
    else if(nWaitTime < -10 && p->bDeinterlaceFlag == 1)
    {
        //* if it is deinterlace and expired, we should drop it
        return VIDEO_RENDER_DROP_THE_PICTURE;
    }
#if defined(CONF_PRODUCT_STB)
    else
    {
        int nDispFPS = LayerGetDisplayFPS(p->pLayerCtrl);
        if ((nDispFPS > 0) && (frameRate > nDispFPS) && (nWaitTime < -frameDuration))
        {
            logv("drop frame nWaitTime=%d, nDispFPS=%d, frameRate=%f.",
                   nWaitTime, nDispFPS, frameRate);
            return VIDEO_RENDER_DROP_THE_PICTURE;
        }
    }
#endif

    return 0;
}

static int QueueBufferToShow(VideoRenderComp* p, VideoPicture* pPicture)
{
    if(p->pLayerCtrl != NULL)
    {
#if defined(CONF_PTS_TOSF)
        int64_t ptsAbs = p->pAvTimer->PtsToSystemTime(p->pAvTimer, pPicture->nPts);
        LayerSetBufferTimeStamp(p->pLayerCtrl, ptsAbs);
#endif
        LayerQueueBuffer(p->pLayerCtrl, pPicture, 1);
    }

    int ptsSecs = (int)(pPicture->nPts/1000000);
    if (p->ptsSecs != ptsSecs)
    {
        p->ptsSecs = ptsSecs;
        logd("video pts(%.3f) ", pPicture->nPts/1000000.0);
    }

    if(p->pLayerCtrl != NULL && p->bHideVideo == 0)
        LayerCtrlShowVideo(p->pLayerCtrl);

    return 0;
}

static int ProcessDeinterlace(VideoRenderComp* p,
                                              int            nDeinterlaceTime)
{
    //* deinterlace process
    int ret = -1;

    if(p->pPrePicture == NULL)
    {
        p->pPrePicture          = p->pPicture;
    }
    ret = LayerDequeueBuffer(p->pLayerCtrl, &p->pDiOutPicture, 0);
    if(ret != 0)
    {
        loge("** dequeue buffer failed when process deinterlace");
        return -1;
    }

    int diret = CdxDiProcess(p->di, p->pPrePicture,
                      p->pPicture,
                      p->pDiOutPicture,
                      nDeinterlaceTime);
    if (diret != 0)
    {
        CdxDiReset(p->di);

        VideoDecCompReturnPicture(p->pDecComp, p->pPrePicture);
        LayerQueueBuffer(p->pLayerCtrl, p->pDiOutPicture, 0);
        p->pPrePicture = NULL;
        p->pDiOutPicture = NULL;
        return VIDEO_RENDER_DEINTERLACE_RESET;
    }
    return 0;
}

static int RenderGetVideoFbmBufInfo(VideoRenderComp* p, LayerCtrl* lc)
{
    enum EPIXELFORMAT eDisplayPixelFormat = PIXEL_FORMAT_DEFAULT;
    FbmBufInfo* pFbmBufInfo =  VideoDecCompGetVideoFbmBufInfo(p->pDecComp);

    logv("pFbmBufInfo = %p",pFbmBufInfo);

    if(pFbmBufInfo == NULL)
        return -1;

    p->mFbmBufInfo = *pFbmBufInfo;
     //* We check whether it is a 3D stream here,
    //* because Layer must know whether it 3D stream at the beginning;
    p->bVideoWithTwoStream = IsVideoWithTwoStream(p->pDecComp);

    logd("video buffer info: nWidth[%d],nHeight[%d],nBufferCount[%d],ePixelFormat[%d]",
          pFbmBufInfo->nBufWidth,pFbmBufInfo->nBufHeight,
          pFbmBufInfo->nBufNum,pFbmBufInfo->ePixelFormat);
    logd("video buffer info: nAlignValue[%d],bProgressiveFlag[%d],bIsSoftDecoderFlag[%d]",
          pFbmBufInfo->nAlignValue,pFbmBufInfo->bProgressiveFlag,
          pFbmBufInfo->bIsSoftDecoderFlag);

    if (lc == NULL)
    {
        logw("lc is NULL");
        return 0;
    }

    //* we init deinterlace device here
    if(p->di != NULL && pFbmBufInfo->bProgressiveFlag == 0 &&
            USE_DETNTERLACE)
    {
        if (CdxDiInit(p->di) == 0)
        {
            int di_flag = CdxDiFlag(p->di);
            p->bDeinterlaceFlag   = 1;
            p->nDeinterlaceDispNum   = (di_flag == DE_INTERLACE_HW) ? 2 : 1;
        }
        else
        {
            logw(" open deinterlace failed , we not to use deinterlace!");
        }
    }

    if(p->bDeinterlaceFlag == 1)
    {
        eDisplayPixelFormat = CdxDiExpectPixelFormat(p->di);
    }
    else
    {
        eDisplayPixelFormat = (enum EPIXELFORMAT)pFbmBufInfo->ePixelFormat;
    }

    LayerSetDisplayPixelFormat(lc, eDisplayPixelFormat);
    LayerSetDisplayBufferSize(lc, pFbmBufInfo->nBufWidth,
            pFbmBufInfo->nBufHeight);
    LayerSetVideoWithTwoStreamFlag(lc, p->bVideoWithTwoStream);
    LayerSetIsSoftDecoderFlag(lc, pFbmBufInfo->bIsSoftDecoderFlag);
    p->nGpuBufferNum = LayerSetDisplayBufferCount(lc,
            pFbmBufInfo->nBufNum);

    p->bHadSetLayerInfoFlag  = 1;

    return 0;
}

static int SetGpuBufferToDecoder(VideoRenderComp*p)
{
    VideoPicture mTmpVideoPicture;
    int i;
    VideoPicture* pTmpVideoPicture = &mTmpVideoPicture;
    int nLayerBufferNum = LayerGetBufferNumHoldByGpu(p->pLayerCtrl);
    memset(pTmpVideoPicture, 0, sizeof(VideoPicture));

    for(i = 0; i< p->nGpuBufferNum; i++)
    {
        int ret = LayerDequeueBuffer(p->pLayerCtrl, &pTmpVideoPicture, 1);
        if(ret == 0)
        {
            if (i >= p->nGpuBufferNum - nLayerBufferNum)
            {
                p->pCancelPicture[i-(p->nGpuBufferNum-nLayerBufferNum)] =
                    VideoDecCompSetVideoFbmBufAddress(p->pDecComp, pTmpVideoPicture, 1);
            }
            else
            {
                VideoDecCompSetVideoFbmBufAddress(p->pDecComp, pTmpVideoPicture, 0);
            }
        }
        else
        {
            loge("*** dequeue buffer failed when set-buffer-to-decoder");
            abort();
        }
    }

    for (i = 0; i < nLayerBufferNum; ++i)
    {
        LayerQueueBuffer(p->pLayerCtrl, p->pCancelPicture[i], 0);
    }
    return 0;
}

//return 0: means ResetBufToDecoder ok.
//return 1: means just continue LayerDequeueBuffer from newLc next time.
//return 2: means VideoDecCompRequestPicture fail, should ResetBufToDecoder again.
static int ResetBufToDecoder(VideoRenderComp*p, LayerCtrl *newLc)
{
    int ret = 0;
    VideoPicture* pReleasePicture = NULL;

    if(p->bHadRequestReleasePicFlag == 0)
    {
        pReleasePicture = VideoDecCompRequestReleasePicture(p->pDecComp);

        logv("*** pReleasePicture(%p),nNeedReleaseBufferNum(%d)",
             pReleasePicture,p->nNeedReleaseBufferNum);

        if(pReleasePicture != NULL)
        {
            LayerReleaseBuffer(p->pLayerCtrl, pReleasePicture); //old
        }
        else
        {
            //* we drop the picture here, or the decoder will block and
            //* can not return all the ReleasePicture
            VideoPicture* pRequestPic = NULL;
            int nResolutionChange = 0;
            pRequestPic = VideoDecCompRequestPicture(p->pDecComp, 0, &nResolutionChange);
            if(pRequestPic != NULL)
                return VideoDecCompReturnPicture(p->pDecComp, pRequestPic);
            else
                return 2;
        }
    }

    if(p->bHadRequestReleasePicFlag == 1 || pReleasePicture != NULL)
    {
        VideoPicture mTmpReturnPicture;
        memset(&mTmpReturnPicture, 0, sizeof(VideoPicture));
        VideoPicture* pTmpReturnPicture = &mTmpReturnPicture;
        ret = LayerDequeueBuffer(newLc, &pTmpReturnPicture, 1);//new
        if(ret == 0)
        {
            if(p->nNeedReleaseBufferNum <= LayerGetBufferNumHoldByGpu(newLc))
            {
                pTmpReturnPicture = VideoDecCompReturnRelasePicture(p->pDecComp,
                        pTmpReturnPicture, 1);
                LayerQueueBuffer(newLc, pTmpReturnPicture, 0);
            }
            else
            {
                VideoDecCompReturnRelasePicture(p->pDecComp, pTmpReturnPicture, 0);
            }

            if(p->bHadRequestReleasePicFlag == 1)
            {
                p->bHadRequestReleasePicFlag = 0;
            }

            p->nNeedReleaseBufferNum--;

            if(p->nNeedReleaseBufferNum <= 0)
            {
                p->bResetBufToDecoderFlag = 0;
            }
        }
        else
        {
            p->bHadRequestReleasePicFlag = 1;
            return 1;
        }
    }
    return 0;
}
