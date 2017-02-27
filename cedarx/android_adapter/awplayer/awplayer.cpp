/*
* Copyright (c) 2008-2016 Allwinner Technology Co. Ltd.
* All rights reserved.
*
* File : awplayer.cpp
* Description : mediaplayer adapter for operator
* History :
*   Author  : AL3
*   Date    : 2015/05/05
*   Comment : first version
*
*/
#define LOG_TAG "awplayer"

#include "cdx_log.h"
#include "awplayer.h"
#include "subtitleUtils.h"
#include "awStreamingSource.h"
#include <string.h>

#include <media/Metadata.h>
#include <media/mediaplayer.h>
#include <binder/IPCThreadState.h>
#include <media/IAudioFlinger.h>
#include <fcntl.h>
#include <cutils/properties.h> // for property_get
#include <hardware/hwcomposer.h>

#include <version.h>
#include "xplayer.h"
#include "player.h"
#include "mediaInfo.h"
#include "AwMessageQueue.h"
#include "awLogRecorder.h"
#include "AwHDCPModule.h"
#include "outputCtrl.h"
#include "awLogRecorder.h"
#include "cdx_config.h"
#include "soundControl.h"

//wasu livemode4 , apk set seekTo after start, we should call start after seek
#define CMCC_LIVEMODE_4_BUG (1)

// the cmcc apk bug, change channel when buffering icon is displayed,
// the buffering icon is not diappered, so we send a buffer end message in prepare/prepareAsync
#define BUFFERING_ICON_BUG (1)

// pause then start to display, the cmcc apk call seekTo, it will flush the buffering cache(livemod)
// or seek some frames in cache ( vod) , it will discontinue
#define PAUSE_THEN_SEEK_BUG (1)

#if (!((CONF_ANDROID_MAJOR_VER == 4)&&(CONF_ANDROID_SUB_VER == 2)))
    //* android 4.4 use IGraphicBufferProducer instead of ISurfaceTexture in android 4.2.
    #include <gui/IGraphicBufferProducer.h>
    #include <gui/Surface.h>
#endif

static const char *WASU_APP_NAME = "net.sunniwell.app.ott.chinamobile";
static const char *CMCC_LOCAL_APP_NAME = "net.sunniwell.app.swplayer";
// static const char *MANGGUO_APP_NAME = "com.starcor.hunan";

static int XPlayerCallbackProcess(void* pUser, int eMessageId, int ext1, void* ext2);
static int GetCallingApkName(char* strApkName, int nMaxNameSize);
static int SubCallbackProcess(void* pUser, int eMessageId, void* param);

typedef struct PlayerPriData
{
    XPlayer*           mXPlayer;
    XPlayerConfig_t     mConfigInfo;
    uid_t               mUID;             //* no use.

    //* surface.
#if (!((CONF_ANDROID_MAJOR_VER == 4)&&(CONF_ANDROID_SUB_VER == 2)))
        //* android 4.4 use IGraphicBufferProducer instead of ISurfaceTexture in android 4.2.
    sp<IGraphicBufferProducer> mGraphicBufferProducer;
#else
    sp<ISurfaceTexture> mSurfaceTexture;
#endif
    sp<ANativeWindow>   mNativeWindow;

    char                strApkName[128];

#if (CONF_ANDROID_MAJOR_VER >= 5)
    sp<IMediaHTTPService> mHTTPService;
#endif

    //* record the id of subtitle which is displaying
    //* we set the Nums to 64 .(32 may be not enough)
    unsigned int        mSubtitleDisplayIds[64];
    int                 mSubtitleDisplayIdsUpdateIndex;

    //* note whether the sutitle is in text or in bitmap format.
    int                 mIsSubtitleInTextFormat;

    //* text codec format of the subtitle, used to transform subtitle text to
    //* utf8 when the subtitle text codec format is unknown.
    char                mDefaultTextFormat[32];

    //* whether enable subtitle show.
    int                 mIsSubtitleDisable;

    //* file descriptor of .idx file of index+sub subtitle.
    //* we save the .idx file's fd here because application set .idx file and .sub file
    //* seperately, we need to wait for the .sub file's fd, see
    //* INVOKE_ID_ADD_EXTERNAL_SOURCE_FD command in invoke() method.
    int                 mIndexFileHasBeenSet;
    int                 mIndexFileFdOfIndexSubtitle;

    //* save the currentSelectTrackIndex;
    int                 mCurrentSelectVidoeTrackIndex;
    int                 mCurrentSelectAudioTrackIndex;
    int                 mCurrentSelectSubTrackIndex;

    AwLogRecorder*      mLogRecorder;
    int                 mbIsDiagnose;

    int64_t             mPlayTimeMs;
    int64_t             mBufferTimeMs;

    int                 mFirstStart;

    int                 mKeepLastFrame;
}PlayerPriData;


static int resumeMediaScanner(const char* strApkName)
{
    CEDARX_UNUSE(strApkName);
    return property_set("mediasw.stopscaner", "0");
}


static int pauseMediaScanner(const char* strApkName)
{
    CEDARX_UNUSE(strApkName);
    return property_set("mediasw.stopscaner", "1");
}

void enableMediaBoost(MediaInfo* mi);
void disableMediaBoost();

AwPlayer::Instance AwPlayer::instance = {0, PTHREAD_MUTEX_INITIALIZER};

AwPlayer::AwPlayer()
{
    logd("AwPlayer construct.");
    pthread_mutex_lock(&instance.lock);
    instance.count++;
    pthread_mutex_unlock(&instance.lock);

    mPriData = (PlayerPriData*)malloc(sizeof(PlayerPriData));
    memset(mPriData,0x00,sizeof(PlayerPriData));

    mPriData->mLogRecorder = NULL;

    mPriData->mIsSubtitleDisable = 1;

#if (!((CONF_ANDROID_MAJOR_VER == 4)&&(CONF_ANDROID_SUB_VER == 2)))
    //* android 4.4 use IGraphicBufferProducer instead of ISurfaceTexture in android 4.2.
    mPriData->mGraphicBufferProducer = NULL;
#else
    mPriData->mSurfaceTexture = NULL;
#endif
    mPriData->mNativeWindow   = NULL;
    mPriData->mKeepLastFrame  = 0;

    GetCallingApkName(mPriData->strApkName, sizeof(mPriData->strApkName));

    mPriData->mXPlayer       = XPlayerCreate();
    if(mPriData->mXPlayer != NULL)
    {
        struct HdcpOpsS hdcpOps;
        hdcpOps.init = HDCP_Init;
        hdcpOps.deinit = HDCP_Deinit;
        hdcpOps.decrypt = HDCP_Decrypt;
        XPlayerSetHdcpOps(mPriData->mXPlayer, &hdcpOps);
        XPlayerSetNotifyCallback(mPriData->mXPlayer, XPlayerCallbackProcess, this);

        SubCtrl* pSubCtrl = NULL;

#if (CONF_ANDROID_MAJOR_VER < 6)
        if (!strcmp(mPriData->strApkName, CMCC_LOCAL_APP_NAME))
            pSubCtrl = SubNativeRenderCreate();
        else
#endif
        pSubCtrl = SubtitleCreate(SubCallbackProcess, this);

        XPlayerSetSubCtrl(mPriData->mXPlayer, pSubCtrl);

        Deinterlace* di = DeinterlaceCreate();
        XPlayerSetDeinterlace(mPriData->mXPlayer, di);
    }
    mPriData->mFirstStart = 1;

    strcpy(mPriData->mDefaultTextFormat, "GBK");

    //* clear the mSubtitleDisplayIds
    memset(mPriData->mSubtitleDisplayIds,0xff,64*sizeof(unsigned int));
    mPriData->mSubtitleDisplayIdsUpdateIndex = 0;
}


AwPlayer::~AwPlayer()
{
    if(mPriData->mLogRecorder)
    {
        AwLogRecorderDestroy(mPriData->mLogRecorder);
        mPriData->mLogRecorder = NULL;
    }

    if(mPriData->mXPlayer != NULL)
        XPlayerDestroy(mPriData->mXPlayer);
    free(mPriData);
    mPriData = NULL;

    pthread_mutex_lock(&instance.lock);
    instance.count--;
    if (instance.count == 0)
        disableMediaBoost();
    pthread_mutex_unlock(&instance.lock);
    logd("~AwPlayer");
}

status_t AwPlayer::initCheck()
{
    logv("initCheck");

    if(mPriData->mXPlayer == NULL)
    {
        loge("initCheck() fail, XPlayer::mplayer = %p", mPriData->mXPlayer);
        return NO_INIT;
    }

    return (status_t)XPlayerInitCheck(mPriData->mXPlayer);
}

status_t AwPlayer::setUID(uid_t uid)
{
    logv("setUID(), uid = %d", uid);
    mPriData->mUID = uid;
    return OK;
}

static int cmccParseHeaders(CdxKeyedVectorT **header,
                            const KeyedVector<String8, String8>* pHeaders)
{
    int nHeaderSize;
    int i;

    if (pHeaders == NULL)
        return 0;

    //*remove "x-hide-urls-from-log" ?

    nHeaderSize = pHeaders->size();
    if (nHeaderSize <= 0)
        return -1;

    *header = CdxKeyedVectorCreate(nHeaderSize);
    if (*header == NULL)
    {
        logw("CdxKeyedVectorCreate() failed");
        return -1;
    }

    String8 key;
    String8 value;
    for (i = 0; i < nHeaderSize; i++)
    {
        key = pHeaders->keyAt(i);
        value = pHeaders->valueAt(i);
        if (CdxKeyedVectorAdd(*header, key.string(), value.string()) != 0)
        {
            CdxKeyedVectorDestroy(*header);
            return -1;
        }
    }

    return 0;
}

#if (CONF_ANDROID_MAJOR_VER >= 5)
status_t AwPlayer::setDataSource(const sp<IMediaHTTPService> &httpService,
                                    const char* pUrl,
                                    const KeyedVector<String8,
                                    String8>* pHeaders)
#else
status_t AwPlayer::setDataSource(const char* pUrl,
                            const KeyedVector<String8, String8>* pHeaders)
#endif
{
    logd("setDataSource(url), url='%s'", pUrl);

    void* pHttpService = NULL;
    CdxKeyedVectorT* header = NULL;
#if (CONF_ANDROID_MAJOR_VER >= 5)
    pHttpService = (void*)httpService.get();
#endif

    int ret = cmccParseHeaders(&header, pHeaders);
    if (ret < 0)
    {
        loge("parse Headers failed.");
        return NO_MEMORY;
    }

    int livemode;
    if(strstr(pUrl, "livemode=1"))
    {
        livemode = 1;
    }
    else if(strstr(pUrl, "livemode=2"))
    {
        livemode = 2;
    }
    else if(strstr(pUrl, "livemode=4"))
    {
        livemode = 4;
    }
    else
    {
        livemode = 0;
    }

    if (strstr(pUrl, "diagnose=deep"))
    {
        logd("In cmcc deep diagnose");
        mPriData->mbIsDiagnose = 1;
    }
    else
    {
        mPriData->mbIsDiagnose = 0;
    }

    mPriData->mConfigInfo.livemode = livemode;
    if (!strcmp(mPriData->strApkName, WASU_APP_NAME))
        mPriData->mConfigInfo.appType = APP_CMCC_WASU;
    else
        mPriData->mConfigInfo.appType = APP_DEFAULT;

    mPriData->mLogRecorder = NULL;
#if defined(CONF_CMCC)
    mPriData->mLogRecorder = AwLogRecorderCreate();
#endif

    if(mPriData->mLogRecorder != NULL)
    {
        char cmccLog[4096] = "";
        snprintf(cmccLog, sizeof(cmccLog), "[info][%s %s %d]setDataSource, url: %s",
                    LOG_TAG, __FUNCTION__, __LINE__, pUrl);
        logv("AwLogRecorderRecord %s.", cmccLog);
        AwLogRecorderRecord(mPriData->mLogRecorder, cmccLog);

        snprintf(cmccLog, sizeof(cmccLog), "[info][%s %s %d]create a new player",
                LOG_TAG, __FUNCTION__, __LINE__);
        logv("AwLogRecorderRecord %s.", cmccLog);
        AwLogRecorderRecord(mPriData->mLogRecorder, cmccLog);
    }

    XPlayerConfig(mPriData->mXPlayer, &mPriData->mConfigInfo);
    XPlayerSetDataSourceUrl(mPriData->mXPlayer, pUrl, pHttpService, header);

    CdxKeyedVectorDestroy(header);

    return OK;
}

//* Warning: The filedescriptor passed into this method will only be valid until
//* the method returns, if you want to keep it, dup it!
status_t AwPlayer::setDataSource(int fd, int64_t offset, int64_t length)
{
    int ret;

    if (!strcmp(mPriData->strApkName, CMCC_LOCAL_APP_NAME))
        mPriData->mConfigInfo.appType = APP_CMCC_LOCAL;
    else
        mPriData->mConfigInfo.appType = APP_DEFAULT;
    XPlayerConfig(mPriData->mXPlayer, &mPriData->mConfigInfo);

    ret = XPlayerSetDataSourceFd(mPriData->mXPlayer, fd, offset, length);

    return (status_t)ret;
}


status_t AwPlayer::setDataSource(const sp<IStreamSource> &source)
{
    int ret;
    logd("setDataSource(IStreamSource)");

    unsigned int numBuffer, bufferSize;
    const char *suffix = "";
    if(!strcmp(mPriData->strApkName, "com.hpplay.happyplay.aw"))
    {
        numBuffer = 32;
        bufferSize = 32*1024;
        suffix = ".awts";
    }
    else if(!strcmp(mPriData->strApkName, "com.softwinner.miracastReceiver"))
    {
        numBuffer = 1024;
        bufferSize = 188*8;
        suffix = ".ts";
    }
    else
    {
        CDX_LOGW("this type is unknown.");
        numBuffer = 16;
        bufferSize = 4*1024;
    }
    CdxStreamT *stream = StreamingSourceOpen(source, numBuffer, bufferSize);
    if(stream == NULL)
    {
        loge("StreamingSourceOpen fail!");
        return UNKNOWN_ERROR;
    }

    char str[128];
    sprintf(str, "customer://%p%s",stream, suffix);
    //* send a set data source message.

    mPriData->mConfigInfo.appType = APP_STREAMING;
    XPlayerConfig(mPriData->mXPlayer, &mPriData->mConfigInfo);

    ret = XPlayerSetDataSourceStream(mPriData->mXPlayer, str);

    return (status_t)ret;
}


#if (!((CONF_ANDROID_MAJOR_VER == 4)&&(CONF_ANDROID_SUB_VER == 2)))
    //* android 4.4 use IGraphicBufferProducer instead of ISurfaceTexture in android 4.2.
status_t AwPlayer::setVideoSurfaceTexture(const sp<IGraphicBufferProducer>& bufferProducer)
#else
status_t AwPlayer::setVideoSurfaceTexture(const sp<ISurfaceTexture> &surfaceTexture)
#endif
{
    int ret;

    sp<ANativeWindow>   anw;

    logd("process message AWPLAYER_COMMAND_SET_SURFACE.");

    //* set native window before delete the old one.
    //* because the player's render thread may use the old surface
    //* before it receive the new surface.
#if (!((CONF_ANDROID_MAJOR_VER == 4)&&(CONF_ANDROID_SUB_VER == 2)))
    if(bufferProducer.get() != NULL)
        anw = new Surface(bufferProducer);
#else
    if(surfaceTexture.get() != NULL)
        anw = new SurfaceTextureClient(surfaceTexture);
#endif
    else
        anw = NULL;

    if(mPriData->mLogRecorder != NULL)
    {
        char cmccLog[4096] = "";
        sprintf(cmccLog, "[info][%s %s %d]setVideoSurfaceTexture", LOG_TAG, __FUNCTION__, __LINE__);
        logv("AwLogRecorderRecord %s.", cmccLog);
        AwLogRecorderRecord(mPriData->mLogRecorder, cmccLog);
    }

    LayerCtrl* lc;
    lc = LayerCreate(anw.get());
    ret = XPlayerSetVideoSurfaceTexture(mPriData->mXPlayer, lc);

    //* save the new surface.
#if (!((CONF_ANDROID_MAJOR_VER == 4)&&(CONF_ANDROID_SUB_VER == 2)))
    mPriData->mGraphicBufferProducer = bufferProducer;
#else
    mPriData->mSurfaceTexture = surfaceTexture;
#endif

    if(mPriData->mNativeWindow != NULL)
        native_window_api_disconnect(mPriData->mNativeWindow.get(), NATIVE_WINDOW_API_MEDIA);

    mPriData->mNativeWindow   = anw;

    return (status_t)ret;
}

void AwPlayer::setAudioSink(const sp<AudioSink> &audioSink)
{
    logv("setAudioSink");

    SoundCtrl* sc = SoundDeviceCreate(audioSink.get());
    //SoundCtrl* sc = DefaultSoundDeviceCreate();
    XPlayerSetAudioSink(mPriData->mXPlayer, sc);

    //* super class MediaPlayerInterface has mAudioSink.
    MediaPlayerInterface::setAudioSink(audioSink);

    return;
}

#if defined(CONF_PLAY_RATE)
status_t AwPlayer::setPlaybackSettings(const AudioPlaybackRate &rate)
{
    XAudioPlaybackRate xrate;
    xrate.mSpeed = rate.mSpeed;
    xrate.mPitch= rate.mPitch;
    xrate.mStretchMode= (XAudioTimestretchStretchMode)rate.mStretchMode;
    xrate.mFallbackMode= (XAudioTimestretchFallbackMode)rate.mFallbackMode;
    int ret=XPlayerSetPlaybackSettings(mPriData->mXPlayer,&xrate);
    return ret;
}

status_t AwPlayer::getPlaybackSettings(AudioPlaybackRate *rate)
{
    XAudioPlaybackRate xrate;
    XPlayerGetPlaybackSettings(mPriData->mXPlayer,&xrate);
    rate->mSpeed = xrate.mSpeed;
    rate->mPitch = xrate.mPitch;
    rate->mStretchMode = (AudioTimestretchStretchMode)xrate.mStretchMode;
    rate->mFallbackMode = (AudioTimestretchFallbackMode)xrate.mFallbackMode;
    return 0;
}
#endif

status_t AwPlayer::prepareAsync()
{
    int ret;

    logd("prepareAsync");

    char prop_value[512];
    int displayRatio = 1;
    //* set scaling mode by default setting.
    property_get("epg.default_screen_ratio", prop_value, "1");
    if(atoi(prop_value) == 0)
    {
        logd("++++++++ IPTV_PLAYER_CONTENTMODE_LETTERBOX");
        displayRatio = 0;
    }
    else
    {
        logd( "+++++++ IPTV_PLAYER_CONTENTMODE_FULL");
        displayRatio = 1;
    }

#if defined(CONF_PRODUCT_STB)
    if(mPriData->mNativeWindow != NULL)
    {
        mPriData->mNativeWindow.get()->perform(mPriData->mNativeWindow.get(),
                                    NATIVE_WINDOW_SETPARAMETER,
                                    DISPLAY_CMD_SETSCREENRADIO,
                                    displayRatio);
    }
#endif

    struct timeval tv;
    int64_t startTimeMs = 0;
    int64_t endTimeMs;
    if(mPriData->mLogRecorder != NULL)
    {
        char cmccLog[4096] = "";
        sprintf(cmccLog, "[info][%s %s %d]prepareAysnc start", LOG_TAG, __FUNCTION__, __LINE__);
        logv("AwLogRecorderRecord %s.", cmccLog);
        AwLogRecorderRecord(mPriData->mLogRecorder, cmccLog);
        gettimeofday(&tv, NULL);
        startTimeMs = (tv.tv_sec * 1000000ll + tv.tv_usec)/1000;
    }

    ret = XPlayerPrepareAsync(mPriData->mXPlayer);

    if(0 == ret && mPriData->mLogRecorder != NULL)
    {
        gettimeofday(&tv, NULL);
        endTimeMs = (tv.tv_sec * 1000000ll + tv.tv_usec)/1000;
        char cmccLog[4096] = "";
        sprintf(cmccLog, "[info][%s %s %d]Playback is ready, spend time: %lldms",
                LOG_TAG, __FUNCTION__, __LINE__, (long long int)(endTimeMs - startTimeMs));
        logv("AwLogRecorderRecord %s.", cmccLog);
        AwLogRecorderRecord(mPriData->mLogRecorder, cmccLog);
    }

    return (status_t)ret;
}


status_t AwPlayer::prepare()
{
    int ret;

    logd("prepare");
    char prop_value[512];
    int displayRatio = 1;
    //* set scaling mode by default setting.
    property_get("epg.default_screen_ratio", prop_value, "1");
    if(atoi(prop_value) == 0)
    {
        logd("++++++++ IPTV_PLAYER_CONTENTMODE_LETTERBOX");
        displayRatio = 0;
    }
    else
    {
        logd( "+++++++ IPTV_PLAYER_CONTENTMODE_FULL");
        displayRatio = 1;
    }

#if defined(CONF_PRODUCT_STB)
    if(mPriData->mNativeWindow != NULL)
    {
    mPriData->mNativeWindow.get()->perform(mPriData->mNativeWindow.get(),
                                            NATIVE_WINDOW_SETPARAMETER,
                                            DISPLAY_CMD_SETSCREENRADIO,
                                            displayRatio);
    }
#endif

    ret = XPlayerPrepare(mPriData->mXPlayer);

    return (status_t)ret;
}


status_t AwPlayer::start()
{
    int ret;
    MediaInfo* mi;

    pauseMediaScanner(mPriData->strApkName);

    mi = XPlayerGetMediaInfo(mPriData->mXPlayer);
    enableMediaBoost(mi);

    mPriData->mCurrentSelectVidoeTrackIndex = 0;
    mPriData->mCurrentSelectAudioTrackIndex = mi->nVideoStreamNum;
    mPriData->mCurrentSelectSubTrackIndex = mi->nVideoStreamNum + mi->nAudioStreamNum;

    // for livemode 4, cmcc apk invoke start before seekTo,
    // it will get a frame between start and seekTo,
#if CMCC_LIVEMODE_4_BUG
    if(mPriData->mConfigInfo.livemode == 4 && mPriData->mFirstStart &&
        mPriData->mConfigInfo.appType == APP_CMCC_WASU)
    {
        return 0;
    }
#endif

    ret = XPlayerStart(mPriData->mXPlayer);

    if(mPriData->mLogRecorder != NULL)
    {
        char cmccLog[4096] = "";
        sprintf(cmccLog, "[info][%s %s %d]start", LOG_TAG, __FUNCTION__, __LINE__);
        logv("AwLogRecorderRecord %s.", cmccLog);
        AwLogRecorderRecord(mPriData->mLogRecorder, cmccLog);
        struct timeval tv;
        gettimeofday(&tv, NULL);
        mPriData->mPlayTimeMs = (tv.tv_sec * 1000000ll + tv.tv_usec)/1000;
    }

    return (status_t)ret;
}


status_t AwPlayer::stop()
{
    int ret;

    if(mPriData->mLogRecorder != NULL)
    {
        char cmccLog[4096] = "";
        sprintf(cmccLog, "[info][%s %s %d]stop", LOG_TAG, __FUNCTION__, __LINE__);
        logd("AwLogRecorderRecord %s.", cmccLog);
        AwLogRecorderRecord(mPriData->mLogRecorder, cmccLog);
    }
    if(mPriData->mLogRecorder != NULL)
    {
        char cmccLog[4096] = "";
        sprintf(cmccLog, "[info][%s %s %d]destory this player", LOG_TAG, __FUNCTION__, __LINE__);
        logv("AwLogRecorderRecord %s.", cmccLog);
        AwLogRecorderRecord(mPriData->mLogRecorder, cmccLog);
    }

    ret = XPlayerStop(mPriData->mXPlayer);

    if(mPriData->mLogRecorder != NULL)
    {
        char cmccLog[4096] = "";
        sprintf(cmccLog, "[info][%s %s %d]Playback end", LOG_TAG, __FUNCTION__, __LINE__);
        logv("AwLogRecorderRecord %s.", cmccLog);
        AwLogRecorderRecord(mPriData->mLogRecorder, cmccLog);
    }

    //* clear the mSubtitleDisplayIds
    memset(mPriData->mSubtitleDisplayIds,0xff,64*sizeof(unsigned int));
    mPriData->mSubtitleDisplayIdsUpdateIndex = 0;

    return (status_t)ret;
}


status_t AwPlayer::pause()
{
    int ret;
    logd(" pause ");
    if(mPriData->mLogRecorder != NULL)
    {
        char cmccLog[4096] = "";
        sprintf(cmccLog, "[info][%s %s %d]pause", LOG_TAG, __FUNCTION__, __LINE__);
        logv("AwLogRecorderRecord %s.", cmccLog);
        AwLogRecorderRecord(mPriData->mLogRecorder, cmccLog);
    }

    ret = XPlayerPause(mPriData->mXPlayer);

    return (status_t)ret;
}


status_t AwPlayer::seekTo(int nSeekTimeMs)
{
    logd("==== seekTo");
    int ret;

   if(mPriData->mLogRecorder != NULL)
    {
        char cmccLog[4096] = "";
        sprintf(cmccLog, "[info][%s %s %d]seekTo, totime: %dms",
                LOG_TAG, __FUNCTION__, __LINE__, nSeekTimeMs);
        logv("AwLogRecorderRecord %s.", cmccLog);
        AwLogRecorderRecord(mPriData->mLogRecorder, cmccLog);
    }

#if CMCC_LIVEMODE_4_BUG
    //for livemode4 , apk set seekTo after start, we should call start after seek
    if(mPriData->mConfigInfo.livemode == 4 && mPriData->mFirstStart &&
        mPriData->mConfigInfo.appType == APP_CMCC_WASU)
    {
        int status;
        mPriData->mFirstStart = 0;
        status = XPlayerSeekTo(mPriData->mXPlayer, nSeekTimeMs);
        XPlayerStart(mPriData->mXPlayer);
        return status;
    }
#endif

    /* 用户seek到约最后一秒，我们认为用户真正的意图是seek到最后，相关的bug有三个：
     *
     * 1. 某些parser内部的duration单位是微妙，比如60000123 us，上报给应用的
     * duration是毫秒，比如60000 ms，在“舍入”或“只舍不入”的影响下，应用始终无法
     * seek到最后；由于关键帧位置的影响，又退回前面。
     *
     * 2. yunos上的应用在seek到最后时，把duration - 1000 ms作为seekto的位置。在
     * HLS不开启切片内seek时，会seek到duration - 10 * 1000 ms的位置，此现象被当
     * 成bug报出来
     *
     * 3. cmcc本地播放器舍去了duration里小于1秒的零头，导致无法seek到最后。测试
     * 关键帧间隔较大的mp4视频，会反复退回到离末尾较远的位置，会被当成seek失败
     */
    if (mPriData->mConfigInfo.livemode == 0)
    {
        int dura = 0;
        ret = XPlayerGetDuration(mPriData->mXPlayer, &dura);
        int n = nSeekTimeMs + 1000;
        if (ret == 0 && dura > 0 && n >= dura)
        {
            logd("change seek position from %d to %d", nSeekTimeMs, n);
            nSeekTimeMs = n;
        }
    }
    ret = XPlayerSeekTo(mPriData->mXPlayer, nSeekTimeMs);

    if(mPriData->mLogRecorder != NULL)
    {
        char cmccLog[4096] = "";
        sprintf(cmccLog, "[info][%s %s %d]seek complete", LOG_TAG, __FUNCTION__, __LINE__);
        logv("AwLogRecorderRecord %s.", cmccLog);
        AwLogRecorderRecord(mPriData->mLogRecorder, cmccLog);
    }

    return (status_t)ret;
}


status_t AwPlayer::reset()
{
    int ret;
    logd("... reset");
    resumeMediaScanner(mPriData->strApkName);

    ret = XPlayerReset(mPriData->mXPlayer);

    //* clear suface.
    if(mPriData->mKeepLastFrame == 0)
    {
        if(mPriData->mNativeWindow != NULL)
            native_window_api_disconnect(mPriData->mNativeWindow.get(),
                                NATIVE_WINDOW_API_MEDIA);
        mPriData->mNativeWindow.clear();
#if (!((CONF_ANDROID_MAJOR_VER == 4)&&(CONF_ANDROID_SUB_VER == 2)))
        mPriData->mGraphicBufferProducer.clear();
#else
        mPriData->mSurfaceTexture.clear();
#endif
    }

    //* clear audio sink.
    mAudioSink.clear();

    //* clear the mSubtitleDisplayIds
    memset(mPriData->mSubtitleDisplayIds,0xff,64*sizeof(unsigned int));
    mPriData->mSubtitleDisplayIdsUpdateIndex = 0;

    return (status_t)ret;
}


status_t AwPlayer::setSpeed(int nSpeed)
{
    int ret;
    ret = XPlayerSetSpeed(mPriData->mXPlayer, nSpeed);

    return (status_t)ret;
}

bool AwPlayer::isPlaying()
{
    int ret;
    ret = XPlayerIsPlaying(mPriData->mXPlayer);

    return (ret == 1)? true : false;
}


status_t AwPlayer::getCurrentPosition(int* msec)
{
    int ret;
    ret = XPlayerGetCurrentPosition(mPriData->mXPlayer, msec);

    if(mPriData->mLogRecorder != NULL)
    {
        char cmccLog[4096] = "";
        sprintf(cmccLog, "[info][%s %s %d]current playtime: %dms",
                LOG_TAG, __FUNCTION__, __LINE__, *msec);
        logv("AwLogRecorderRecord %s.", cmccLog);
        AwLogRecorderRecord(mPriData->mLogRecorder, cmccLog);
    }

    return (status_t)ret;
}


status_t AwPlayer::getDuration(int *msec)
{
    int ret;
    ret = XPlayerGetDuration(mPriData->mXPlayer, msec);

    return (status_t)ret;
}


status_t AwPlayer::setLooping(int loop)
{
    int ret;
    ret = XPlayerSetLooping(mPriData->mXPlayer, loop);

    return (status_t)ret;
}


player_type AwPlayer::playerType()
{
    logv("playerType");
    return AW_PLAYER;
}


status_t AwPlayer::invoke(const Parcel &request, Parcel *reply)
{
    int nMethodId;
    int ret;
    MediaInfo* mi;

    logv("invoke()");

    ret = request.readInt32(&nMethodId);
    if(ret != OK)
        return ret;

    mi = XPlayerGetMediaInfo(mPriData->mXPlayer);
    switch(nMethodId)
    {
        case INVOKE_ID_GET_TRACK_INFO:  //* get media stream counts.
        {
            logv("invode::INVOKE_ID_GET_TRACK_INFO");

            if(mi == NULL)
            {
                return NO_INIT;
            }
            else
            {
                int         i;
                int         nTrackCount;
                const char* lang;

                nTrackCount = mi->nVideoStreamNum + mi->nAudioStreamNum +
                                mi->nSubtitleStreamNum;
#if AWPLAYER_CONFIG_DISABLE_VIDEO
                nTrackCount -= mi->nVideoStreamNum;
#endif
#if AWPLAYER_CONFIG_DISABLE_AUDIO
                nTrackCount -= mi->nAudioStreamNum;
#endif
#if AWPLAYER_CONFIG_DISABLE_SUBTITLE
                nTrackCount -= mi->nSubtitleStreamNum;
#endif
                reply->writeInt32(nTrackCount);

#if !AWPLAYER_CONFIG_DISABLE_VIDEO
                for(i=0; i<mi->nVideoStreamNum; i++)
                {
#if defined(CONF_DETAIL_TRACK_INFO)
                    reply->writeInt32(4);
#else
                    reply->writeInt32(2);
#endif
                    reply->writeInt32(MEDIA_TRACK_TYPE_VIDEO);

#if (CONF_ANDROID_MAJOR_VER >= 6)
                    reply->writeString16(String16("video/"));
#endif
                    lang = " ";
                    reply->writeString16(String16(lang));
#if defined(CONF_DETAIL_TRACK_INFO)
                    reply->writeInt32(mi->pVideoStreamInfo[i].bIs3DStream);
                    //Please refer to the "enum EVIDEOCODECFORMAT" in "vdecoder.h"
                    reply->writeInt32(mi->pVideoStreamInfo[i].eCodecFormat);
#endif
                }
#endif


#if !AWPLAYER_CONFIG_DISABLE_AUDIO
                for(i=0; i<mi->nAudioStreamNum; i++)
                {
#if defined(CONF_DETAIL_TRACK_INFO)
                    reply->writeInt32(6);
#else
                    reply->writeInt32(2);
#endif
                    reply->writeInt32(MEDIA_TRACK_TYPE_AUDIO);

#if (CONF_ANDROID_MAJOR_VER >= 6)
                    reply->writeString16(String16("audio/"));
#endif

                    if(mi->pAudioStreamInfo[i].strLang[0] != 0)
                    {
                        lang = (const char*)mi->pAudioStreamInfo[i].strLang;
                        reply->writeString16(String16(lang));
                    }
                    else
                    {
                        lang = "";
                        reply->writeString16(String16(lang));
                    }
#if defined(CONF_DETAIL_TRACK_INFO)
                    reply->writeInt32(mi->pAudioStreamInfo[i].nChannelNum);
                    reply->writeInt32(mi->pAudioStreamInfo[i].nSampleRate);
                    reply->writeInt32(mi->pAudioStreamInfo[i].nAvgBitrate);
                    //Please refer to the "enum EAUDIOCODECFORMAT" in "adecoder.h"
                    reply->writeInt32(mi->pAudioStreamInfo[i].eCodecFormat);

#endif
                }
#endif

#if !AWPLAYER_CONFIG_DISABLE_SUBTITLE
                for(i=0; i<mi->nSubtitleStreamNum; i++)
                {
#if defined(CONF_DETAIL_TRACK_INFO)
                    reply->writeInt32(4);
#else
                    reply->writeInt32(2);
#endif
                    reply->writeInt32(MEDIA_TRACK_TYPE_TIMEDTEXT);

#if (CONF_ANDROID_MAJOR_VER >= 6)
                    reply->writeString16(String16("text/3gpp-tt"));
#endif
                    if (mi->pSubtitleStreamInfo[i].strLang[0] != 0)
                    {
                        lang = (const char*)mi->pSubtitleStreamInfo[i].strLang;
                        reply->writeString16(String16(lang));
                    }
                    else
                    {
                        lang = "";
                        reply->writeString16(String16(lang));
                    }
#if defined(CONF_DETAIL_TRACK_INFO)
                    //Please refer to the "ESubtitleCodec" in "sdecoder.h"
                    reply->writeInt32(mi->pSubtitleStreamInfo[i].eCodecFormat);
                    reply->writeInt32(mi->pSubtitleStreamInfo[i].bExternal);
#endif
                }
#endif

                return OK;
            }
        }

        case INVOKE_ID_ADD_EXTERNAL_SOURCE:
        case INVOKE_ID_ADD_EXTERNAL_SOURCE_FD:
        {
            logd("=== INVOKE_ID_ADD_EXTERNAL_SOURCE");
            int                 fd;
            int64_t             nOffset;
            int64_t             nLength;
            int                 fdSub;

            if(nMethodId == INVOKE_ID_ADD_EXTERNAL_SOURCE)
            {
                //* string values written in Parcel are UTF-16 values.
                String8 uri(request.readString16());
                String8 mimeType(request.readString16());

                //* if mimeType == "application/sub" and mIndexFileHasBeenSet == 0,
                //* the .sub file is a common .sub subtitle, not index+sub subtitle.

                if(strcmp((char*)mimeType.string(), "application/sub") == 0 &&
                   mPriData->mIndexFileHasBeenSet == 1)
                {
                    //* the .sub file of index+sub subtitle.
                    //* no need to use the .sub file url, because subtitle decoder will
                    //* find the .sub file by itself by using the .idx file's url.
                    //* mimetype "application/sub" is defined in
                    //* "android/base/media/java/android/media/MediaPlayer.java".
                    //* clear the flag for adding more subtitle.
                    mPriData->mIndexFileHasBeenSet = 0;
                    return OK;
                }
                else if(strcmp((char*)mimeType.string(), "application/idx+sub") == 0)
                {
                    //* set this flag to process the .sub file passed in at next call.
                    mPriData->mIndexFileHasBeenSet = 1;
                }

                //* probe subtitle info
                XPlayerSetExternalSubUrl(mPriData->mXPlayer, uri.string());
            }
            else
            {
                fd      = request.readFileDescriptor();
                nOffset = request.readInt64();
                nLength = request.readInt64();
                fdSub   = -1;
                String8 mimeType(request.readString16());

                //* if mimeType == "application/sub" and mIndexFileHasBeenSet == 0,
                //* the .sub file is a common .sub subtitle, not index+sub subtitle.

                if(strcmp((char*)mimeType.string(), "application/idx-sub") == 0)
                {
                    //* the .idx file of index+sub subtitle.
                    mPriData->mIndexFileFdOfIndexSubtitle = dup(fd);
                    mPriData->mIndexFileHasBeenSet = 1;
                    return OK;
                }
                else if(strcmp((char*)mimeType.string(), "application/sub") == 0 &&
                        mPriData->mIndexFileHasBeenSet == 1)
                {
                    //* the .sub file of index+sub subtitle.
                    //* for index+sub subtitle, PlayerProbeSubtitleStreamInfoFd() method need
                    //* the .idx file's descriptor for probe.
                    fdSub = fd;           //* save the .sub file's descriptor to fdSub.
                    //* set the .idx file's descriptor to fd.
                    fd    = mPriData->mIndexFileFdOfIndexSubtitle;
                    mPriData->mIndexFileFdOfIndexSubtitle = -1;
                    mPriData->mIndexFileHasBeenSet = 0;//* clear the flag for adding more subtitle.
                }

                //* probe subtitle info
                XPlayerSetExternalSubFd(mPriData->mXPlayer, fd,nOffset,nLength, fdSub);
            }
            break;
        }

        case INVOKE_ID_SELECT_TRACK:
        case INVOKE_ID_UNSELECT_TRACK:
        {
            int nStreamIndex;
            int nTrackCount;

            nStreamIndex = request.readInt32();
            logd("invode::INVOKE_ID_SELECT_TRACK, stream index = %d", nStreamIndex);

            if(mi == NULL)
            {
                loge("can not switch audio or subtitle, there is no media stream.");
                return NO_INIT;
            }

            nTrackCount = mi->nVideoStreamNum + mi->nAudioStreamNum +
                            mi->nSubtitleStreamNum;
            if(nTrackCount == 0)
            {
                loge("can not switch audio or subtitle, there is no media stream.");
                return NO_INIT;
            }

            if(nStreamIndex >= mi->nVideoStreamNum &&
                nStreamIndex < (mi->nVideoStreamNum + mi->nAudioStreamNum))
            {
                if(nMethodId == INVOKE_ID_SELECT_TRACK)
                {
                    //* switch audio.
                    mPriData->mCurrentSelectAudioTrackIndex = nStreamIndex;
                    nStreamIndex -= mi->nVideoStreamNum;
                    if(XPlayerSwitchAudio(mPriData->mXPlayer, nStreamIndex) != 0)
                    {
                        loge("can not switch audio, call PlayerSwitchAudio() return fail.");
                        return UNKNOWN_ERROR;
                    }
                }
                else
                {
                    mPriData->mCurrentSelectAudioTrackIndex = -1;
                    loge("Deselect an audio track (%d) is not supported", nStreamIndex);
                    return INVALID_OPERATION;
                }

                return OK;
            }
            else if(nStreamIndex >= (mi->nVideoStreamNum + mi->nAudioStreamNum) &&
                        nStreamIndex < nTrackCount)
            {
                if(nMethodId == INVOKE_ID_SELECT_TRACK)
                {
                    logv("==== INVOKE_ID_SELECT_TRACK, nStreamIndex: %d", nStreamIndex);
                    //* enable subtitle.
                    mPriData->mIsSubtitleDisable = 0;
                    mPriData->mCurrentSelectSubTrackIndex = nStreamIndex;

                    //* switch subtitle.
                    nStreamIndex -= (mi->nVideoStreamNum + mi->nAudioStreamNum);
                    if(XPlayerSwitchSubtitle(mPriData->mXPlayer, nStreamIndex) != 0)
                    {
                        loge("can not switch subtitle, call PlayerSwitchSubtitle() return fail.");
                        return UNKNOWN_ERROR;
                    }
                }
                else
                {
                    logv("==== INVOKE_ID_DESELECT_TRACK, nStreamIndex: %d", nStreamIndex);
                    if(mPriData->mCurrentSelectSubTrackIndex != nStreamIndex)
                    {
                        logw("the unselectTrack is not right: %d, %d",
                        mPriData->mCurrentSelectSubTrackIndex,nStreamIndex);
                        return INVALID_OPERATION;
                    }

                    mPriData->mIsSubtitleDisable = 1; //* disable subtitle show.
                    sendEvent(MEDIA_TIMED_TEXT);//* clear all the displaying subtitle
                    //* clear the mSubtitleDisplayIds
                    memset(mPriData->mSubtitleDisplayIds,0xff,64*sizeof(unsigned int));
                    mPriData->mSubtitleDisplayIdsUpdateIndex = 0;
                    mPriData->mCurrentSelectSubTrackIndex = -1;
                }

                return OK;
            }

            if(nMethodId == INVOKE_ID_SELECT_TRACK)
            {
                loge("can not switch audio or subtitle to track %d, \
                    stream index exceed.(%d stream in total).",
                    nStreamIndex, nTrackCount);
            }
            else
            {
                loge("can not unselect track %d, stream index exceed.(%d stream in total).",
                    nStreamIndex, nTrackCount);
            }
            return BAD_INDEX;
            break;
        }

        case INVOKE_ID_SET_VIDEO_SCALING_MODE:
        {
            int nStreamIndex;

            nStreamIndex = request.readInt32();
            logv("invode::INVOKE_ID_SET_VIDEO_SCALING_MODE");
            //* TODO.
            return OK;
        }

#if defined(CONF_PRODUCT_STB)
        case INVOKE_ID_SET_3D_MODE:
        {
            logd(" donot set 3d mode");
            break;
        }

        case INVOKE_ID_GET_3D_MODE:
        {
            break;
        }
#endif

#if (CONF_ANDROID_MAJOR_VER >= 5)
        case INVOKE_ID_GET_SELECTED_TRACK:
        {
            int trackType = request.readInt32();
            logd("==== invoke: INVOKE_ID_GET_SELECTED_TRACK, trackType(%d)", trackType);
            logd("=== mCurrentSelectSubTrackIndex: %d", mPriData->mCurrentSelectSubTrackIndex);
            if(trackType == MEDIA_TRACK_TYPE_VIDEO)
                reply->writeInt32(mPriData->mCurrentSelectVidoeTrackIndex);
            else if(trackType == MEDIA_TRACK_TYPE_AUDIO)
                reply->writeInt32(mPriData->mCurrentSelectAudioTrackIndex);
            else if(trackType == MEDIA_TRACK_TYPE_TIMEDTEXT ||
                        trackType == MEDIA_TRACK_TYPE_SUBTITLE)
                reply->writeInt32(mPriData->mCurrentSelectSubTrackIndex);
            else
                logw("invoke: INVOKE_ID_GET_SELECTED_TRACK, trackType(%d) unkown", trackType);

            break;
        }
#endif

        default:
        {
            logv("unknown invode command %d", nMethodId);
            return UNKNOWN_ERROR;
        }
    }

    return OK;
}


status_t AwPlayer::setParameter(int key, const Parcel &request)
{
    logv("setParameter(key=%d)", key);
    (void)key;
    (void)request;
    return OK;
#if 0

    switch(key)
    {
        case KEY_PARAMETER_CACHE_STAT_COLLECT_FREQ_MS:
        {
            int nCacheReportIntervalMs;

            logv("setParameter::KEY_PARAMETER_CACHE_STAT_COLLECT_FREQ_MS");

            nCacheReportIntervalMs = request.readInt32();
            if(DemuxCompSetCacheStatReportInterval(mPriData->mDemux, nCacheReportIntervalMs) == 0)
                return OK;
            else
                return UNKNOWN_ERROR;
        }

        case KEY_PARAMETER_PLAYBACK_RATE_PERMILLE:
        {
            logv("setParameter::KEY_PARAMETER_PLAYBACK_RATE_PERMILLE");
            //* TODO.
            return OK;
        }

        default:
        {
            logv("unknown setParameter command %d", key);
            return UNKNOWN_ERROR;
        }
    }
#endif
}

status_t AwPlayer::getParameter(int key, Parcel *reply)
{
    logv("getParameter");
    (void)(key);

    (void)(reply);
    return OK;

    #if 0
    switch(key)
    {
        case KEY_PARAMETER_AUDIO_CHANNEL_COUNT:
        {
            logv("getParameter::KEY_PARAMETER_AUDIO_CHANNEL_COUNT");
            //* TODO.
            return OK;
        }
#if // use define CONF_PRODUCT_STB/CONF_CMCC/CONF_YUNOS instead
        case KEY_PARAMETER_GET_CURRENT_BITRATE:
        {
            logd("getParameter::PARAM_KEY_GET_CURRENT_BITRATE");
            if(mPriData->mXPlayer != NULL)
            {
                int currentVideoBitrate = 0;
                int currentAudioBitrate = 0;
                int currentBitrate = 0;
                currentVideoBitrate = PlayerGetVideoBitrate(mPriData->mXPlayer);
                currentAudioBitrate = PlayerGetAudioBitrate(mPriData->mXPlayer);
                currentBitrate = currentVideoBitrate + currentAudioBitrate;
                logd("current Bitrate: video(%d)b/s + audio(%d)b/s = (%d)b/s",
                        currentVideoBitrate, currentAudioBitrate, currentBitrate);
                reply->writeInt32(currentBitrate);
            }
            return OK;
        }
        case KEY_PARAMETER_GET_CURRENT_CACHE_DATA_DURATION:
        {
            logv("getParameter::PARAM_KEY_GET_CURRENT_CACHE_DATA_DURATION");
            if(mPriData->mXPlayer != NULL)
            {
                int currentBitrate = 0;
                int currentCacheDataSize = 0;
                int currentCacheDataDuration = 0;
                currentBitrate = PlayerGetVideoBitrate(mPriData->mXPlayer) +
                            PlayerGetAudioBitrate(mPriData->mXPlayer);
                currentCacheDataSize = DemuxCompGetCacheSize(mPriData->mDemux);

                if(currentBitrate <= 0)
                {
                    currentCacheDataDuration = 0;
                    reply->writeInt32((int)currentCacheDataDuration);
                }
                else
                {
                    int64_t tmp = (float)currentCacheDataSize*8.0*1000.0;
                    tmp /= (float)currentBitrate;
                    currentCacheDataDuration = (int)tmp;
                    reply->writeInt32((int)currentCacheDataDuration);
                }
                logd("currentDataSize(%d)B currentBitrate(%d)b/s currentDataDuration(%d)ms",
                        currentCacheDataSize, currentBitrate, currentCacheDataDuration);
            }
            return OK;
        }
#endif
        default:
        {
            loge("unknown getParameter command %d", key);
            return UNKNOWN_ERROR;
        }
    }
    #endif
}


status_t AwPlayer::getMetadata(const media::Metadata::Filter& ids, Parcel *records)
{
    using media::Metadata;

    Metadata metadata(records);

    MediaInfo* mi;
    mi = XPlayerGetMediaInfo(mPriData->mXPlayer);

    CEDARX_UNUSE(ids);

    if(mi == NULL)
    {
        return NO_INIT;
    }

    if(mi->nDurationMs > 0)
        metadata.appendBool(Metadata::kPauseAvailable , true);
    else
        metadata.appendBool(Metadata::kPauseAvailable , false); //* live stream, can not pause.

    if(mi->bSeekable)
    {
        metadata.appendBool(Metadata::kSeekBackwardAvailable, true);
        metadata.appendBool(Metadata::kSeekForwardAvailable, true);
        metadata.appendBool(Metadata::kSeekAvailable, true);
    }
    else
    {
        metadata.appendBool(Metadata::kSeekBackwardAvailable, false);
        metadata.appendBool(Metadata::kSeekForwardAvailable, false);
        metadata.appendBool(Metadata::kSeekAvailable, false);
    }

    return OK;

    //* other metadata in include/media/Metadata.h, Keep in sync with android/media/Metadata.java.
    /*
    Metadata::kTitle;                   // String
    Metadata::kComment;                 // String
    Metadata::kCopyright;               // String
    Metadata::kAlbum;                   // String
    Metadata::kArtist;                  // String
    Metadata::kAuthor;                  // String
    Metadata::kComposer;                // String
    Metadata::kGenre;                   // String
    Metadata::kDate;                    // Date
    Metadata::kDuration;                // Integer(millisec)
    Metadata::kCdTrackNum;              // Integer 1-based
    Metadata::kCdTrackMax;              // Integer
    Metadata::kRating;                  // String
    Metadata::kAlbumArt;                // byte[]
    Metadata::kVideoFrame;              // Bitmap

    Metadata::kBitRate;                 // Integer, Aggregate rate of
    Metadata::kAudioBitRate;            // Integer, bps
    Metadata::kVideoBitRate;            // Integer, bps
    Metadata::kAudioSampleRate;         // Integer, Hz
    Metadata::kVideoframeRate;          // Integer, Hz

    // See RFC2046 and RFC4281.
    Metadata::kMimeType;                // String
    Metadata::kAudioCodec;              // String
    Metadata::kVideoCodec;              // String

    Metadata::kVideoHeight;             // Integer
    Metadata::kVideoWidth;              // Integer
    Metadata::kNumTracks;               // Integer
    Metadata::kDrmCrippled;             // Boolean
    */
}

status_t AwPlayer::setSubCharset(const char* strFormat)
{
    if(strFormat != NULL)
    {
        int i;
        for(i=0; strTextCodecFormats[i]; i++)
        {
            if(!strcmp(strTextCodecFormats[i], strFormat))
                break;
        }

        if(strTextCodecFormats[i] != NULL)
            strcpy(mPriData->mDefaultTextFormat, strTextCodecFormats[i]);
    }
    else
        strcpy(mPriData->mDefaultTextFormat, "UTF-8");

    return OK;
}

status_t AwPlayer::getSubCharset(char *charset)
{
    if(mPriData->mXPlayer == NULL)
    {
        return -1;
    }

    strcpy(charset, mPriData->mDefaultTextFormat);

    return OK;
}

status_t AwPlayer::setSubDelay(int nTimeMs)
{
    if(mPriData->mXPlayer == NULL)
    {
        return -1;
    }

    return XPlayerSetSubDelay(mPriData->mXPlayer, nTimeMs);
}

int AwPlayer::getSubDelay()
{
    if(mPriData->mXPlayer == NULL)
    {
        return -1;
    }
    return XPlayerGetSubDelay(mPriData->mXPlayer);
}

status_t AwPlayer::callbackProcess(int messageId, int ext1, void* param)
{
    switch(messageId)
    {
        case AWPLAYER_MEDIA_PREPARED:
        {

#if BUFFERING_ICON_BUG
            if (!mPriData->mbIsDiagnose &&
                mPriData->mConfigInfo.appType == APP_CMCC_WASU)
            {
                sendEvent(MEDIA_INFO, MEDIA_INFO_BUFFERING_END);
            }
#endif
            sendEvent(MEDIA_PREPARED, 0, 0);

            break;
        }

        case AWPLAYER_MEDIA_PLAYBACK_COMPLETE:
        {
            sendEvent(MEDIA_PLAYBACK_COMPLETE, 0, 0);
            break;
        }

        case AWPLAYER_MEDIA_SEEK_COMPLETE:
        {
            sendEvent(MEDIA_SEEK_COMPLETE, 0, 0);
            break;
        }

        case AWPLAYER_MEDIA_LOG_RECORDER:
        {
            if(mPriData->mLogRecorder != NULL)
            {
                logv("AwLogRecorderRecord %s.", (char*)param);
                AwLogRecorderRecord(mPriData->mLogRecorder, (char*)param);
            }
            break;
        }

        case AWPLAYER_MEDIA_BUFFERING_UPDATE:
        {
            int nTotalPercentage;
            int nBufferPercentage;
            int nLoadingPercentage;

            nTotalPercentage   = ((int*)param)[0];   //* read positon to total file size.
            nBufferPercentage  = ((int*)param)[1];   //* cache buffer fullness.
            nLoadingPercentage = ((int*)param)[2];   //* loading percentage to start play.
            sendEvent(MEDIA_BUFFERING_UPDATE, nTotalPercentage,
                    nBufferPercentage<<16 | nLoadingPercentage);
            break;
        }

        case AWPLAYER_MEDIA_ERROR:
        {
            if(mPriData->mLogRecorder != NULL)
            {
                char cmccLog[4096] = "";
                sprintf(cmccLog, "[error][%s %s %d]Error happened, event: %d",
                            LOG_TAG, __FUNCTION__, __LINE__, ext1);
                logv("AwLogRecorderRecord %s.", cmccLog);
                AwLogRecorderRecord(mPriData->mLogRecorder, cmccLog);
            }

#if defined(CONF_PRODUCT_STB)
            if(ext1 == AW_MEDIA_ERROR_UNKNOWN)
                sendEvent(MEDIA_ERROR, MEDIA_ERROR_UNKNOWN, 0);
            else if(ext1 == AW_MEDIA_ERROR_IO)
                sendEvent(MEDIA_ERROR, MEDIA_ERROR_IO, 0);
            else if(ext1 == AW_MEDIA_ERROR_UNSUPPORTED)
                sendEvent(MEDIA_ERROR, MEDIA_ERROR_UNSUPPORTED, 0);
            else
            {
                logw("unkown ext1: %d", ext1);
                sendEvent(MEDIA_ERROR, MEDIA_ERROR_UNKNOWN, 0);
            }
#else
            sendEvent(MEDIA_ERROR, MEDIA_ERROR_UNKNOWN, 0);
#endif
            break;
        }

        case AWPLAYER_MEDIA_INFO:
        {
            switch(ext1)
            {
                case AW_MEDIA_INFO_UNKNOWN:
                    sendEvent(MEDIA_INFO, MEDIA_INFO_UNKNOWN);
                    break;
                case AW_MEDIA_INFO_RENDERING_START:
                {
                    if(mPriData->mLogRecorder != NULL)
                    {
                        struct timeval tv;
                        int64_t renderTimeMs;
                        gettimeofday(&tv, NULL);
                        renderTimeMs = (tv.tv_sec * 1000000ll + tv.tv_usec)/1000;
                        char cmccLog[4096] = "";
                        sprintf(cmccLog, "[info][%s %s %d]show the first video frame,"
                                         " spend time: %lldms",
                                         LOG_TAG, __FUNCTION__, __LINE__,
                                         (long long int)(renderTimeMs - mPriData->mPlayTimeMs));
                        logv("AwLogRecorderRecord %s.", cmccLog);
                        AwLogRecorderRecord(mPriData->mLogRecorder, cmccLog);
                    }
                    sendEvent(MEDIA_INFO, MEDIA_INFO_RENDERING_START, 0);
                    break;
                }
                case AW_MEDIA_INFO_BUFFERING_START:
                {

                    if(mPriData->mLogRecorder != NULL)
                    {
                        char cmccLog[4096] = "";
                        int *pNeedBufferSize = (int*)param;
                        sprintf(cmccLog, "[info][%s %s %d]buffering start, "
                                         "need buffer data size: %.2fMB",
                                         LOG_TAG, __FUNCTION__, __LINE__,
                                         *pNeedBufferSize/(1024*1024.0f));
                        logv("AwLogRecorderRecord %s.", cmccLog);
                        AwLogRecorderRecord(mPriData->mLogRecorder, cmccLog);
                        struct timeval tv;
                        gettimeofday(&tv, NULL);
                        mPriData->mBufferTimeMs = (tv.tv_sec * 1000000ll + tv.tv_usec)/1000;
                    }
                    sendEvent(MEDIA_INFO, MEDIA_INFO_BUFFERING_START);
                    break;
                }
                case AW_MEDIA_INFO_BUFFERING_END:
                {
                     if(mPriData->mLogRecorder != NULL)
                    {
                        struct timeval tv;
                        gettimeofday(&tv, NULL);
                        int64_t bufferEndTimeMs = (tv.tv_sec * 1000000ll + tv.tv_usec)/1000;
                        char cmccLog[4096] = "";
                        sprintf(cmccLog, "[info][%s %s %d]buffering end, lasting time: %lldms",
                                    LOG_TAG, __FUNCTION__, __LINE__,
                                    (long long int)(bufferEndTimeMs - mPriData->mBufferTimeMs));
                        logv("AwLogRecorderRecord %s.", cmccLog);
                        AwLogRecorderRecord(mPriData->mLogRecorder, cmccLog);
                    }
                    sendEvent(MEDIA_INFO, MEDIA_INFO_BUFFERING_END);
                    break;
                }
                case AW_MEDIA_INFO_NOT_SEEKABLE:
                    sendEvent(MEDIA_INFO, MEDIA_INFO_NOT_SEEKABLE, 0);
                    break;

#if defined(CONF_PRODUCT_STB)
                case AW_MEDIA_INFO_DOWNLOAD_START:
                {
                    DownloadObject* obj = (DownloadObject*)param;
                    if(mPriData->mLogRecorder != NULL)
                    {
                        char cmccLog[4096] = "";
                        sprintf(cmccLog, "[info][%s %s %d]ts segment download is start, "
                            "number: %d, filesize: %.2fMB, duration: %lldms",
                                    LOG_TAG, __FUNCTION__, __LINE__, obj->seqNum,
                                    (float)obj->seqSize/(1024*1024.0f), obj->seqDuration/1000);
                        logv("AwLogRecorderRecord %s.", cmccLog);
                        AwLogRecorderRecord(mPriData->mLogRecorder, cmccLog);
                    }

                    sendEvent(MEDIA_INFO, MEDIA_INFO_DOWNLOAD_START, obj->seqNum);
                    break;
                }
                case AW_MEDIA_INFO_DOWNLOAD_END:
                {
                    DownloadObject* obj = (DownloadObject*)param;
                    if(mPriData->mLogRecorder != NULL)
                    {
                        char cmccLog[4096] = "";
                        sprintf(cmccLog, "[info][%s %s %d]ts segment download is complete, "
                                "recvsize: %.2fMB, spend time: %lldms, rate: %lldbps",
                                        LOG_TAG, __FUNCTION__, __LINE__,
                                        (float)obj->seqSize/(1024*1024.0f),
                                        obj->spendTime, obj->rate);
                        logv("AwLogRecorderRecord %s.", cmccLog);
                        AwLogRecorderRecord(mPriData->mLogRecorder, cmccLog);
                    }

                    sendEvent(MEDIA_INFO, MEDIA_INFO_DOWNLOAD_END, (int)obj->spendTime);
                    break;
                }
                case AW_MEDIA_INFO_DOWNLOAD_ERROR:
                {
                    DownloadObject* obj = (DownloadObject*)param;
                    if(mPriData->mLogRecorder != NULL)
                    {
                        char cmccLog[4096] = "";
                        sprintf(cmccLog, "[error][%s %s %d]Error happened, event: %d", LOG_TAG,
                                __FUNCTION__, __LINE__, MEDIA_INFO_DOWNLOAD_ERROR);
                        logv("AwLogRecorderRecord %s.", cmccLog);
                        AwLogRecorderRecord(mPriData->mLogRecorder, cmccLog);
                    }

                    sendEvent(MEDIA_INFO, MEDIA_INFO_DOWNLOAD_ERROR, obj->statusCode);
                    break;
                }
#endif

                default:
                    logw("unkown info msg");
                    break;
            }

            break;
        }

#if !(defined(CONF_NEW_BDMV_STREAM))
        case AWPLAYER_EXTEND_MEDIA_INFO:
        {
            switch(ext1)
            {
                case AW_EX_IOREQ_ACCESS:
                {
                    char *filePath = (char *)((uintptr_t *)param)[0];
                    int mode = ((uintptr_t *)param)[1];
                    int *pRet = (int *)((uintptr_t *)param)[2];

                    Parcel parcel;
                    Parcel replyParcel;
                    *pRet = -1;

                    // write path string as a byte array
                    parcel.writeInt32(strlen(filePath));
                    parcel.write(filePath, strlen(filePath));
                    parcel.writeInt32(mode);
                    sendEvent(AWEXTEND_MEDIA_INFO,
                            AWEXTEND_MEDIA_INFO_CHECK_ACCESS_RIGHRS,
                            0, &parcel, &replyParcel);
                    replyParcel.setDataPosition(0);
                    *pRet = replyParcel.readInt32();

                    break;
                }
                case AW_EX_IOREQ_OPEN:
                {
                    char *filePath = (char *)((uintptr_t *)param)[0];
                    int *pFd = (int *)((uintptr_t *)param)[1];
                    int fd = -1;

                    Parcel parcel;
                    Parcel replyParcel;
                    bool    bFdValid = false;

                    *pFd = -1;

                    // write path string as a byte array
                    parcel.writeInt32(strlen(filePath));
                    parcel.write(filePath, strlen(filePath));
                    sendEvent(AWEXTEND_MEDIA_INFO,
                            AWEXTEND_MEDIA_INFO_REQUEST_OPEN_FILE,
                            0, &parcel, &replyParcel);
                    replyParcel.setDataPosition(0);
                    bFdValid = replyParcel.readInt32();
                    if (bFdValid == true)
                    {
                        fd = replyParcel.readFileDescriptor();
                        if (fd < 0)
                        {
                            loge("invalid fd '%d'", fd);
                            *pFd = -1;
                            break;
                        }

                        *pFd = dup(fd);
                        if (*pFd < 0)
                        {
                            loge("dup fd failure, errno(%d) '%d'", errno, fd);
                        }
                        close(fd);
                    }

                    break;
                }
                case AW_EX_IOREQ_OPENDIR:
                {
                    char *dirPath = (char *)((uintptr_t *)param)[0];
                    int *pDirId = (int *)((uintptr_t *)param)[1];

                    Parcel parcel;
                    Parcel replyParcel;

                    *pDirId = -1;

                    // write path string as a byte array
                    parcel.writeInt32(strlen(dirPath));
                    parcel.write(dirPath, strlen(dirPath));
                    sendEvent(AWEXTEND_MEDIA_INFO,
                            AWEXTEND_MEDIA_INFO_REQUEST_OPEN_DIR,
                            0, &parcel, &replyParcel);
                    replyParcel.setDataPosition(0);
                    *pDirId = replyParcel.readInt32();
                    break;
                }
                case AW_EX_IOREQ_READDIR:
                {
                    int dirId = ((uintptr_t *)param)[0];
                    int *pRet = (int *)((uintptr_t *)param)[1];
                    char *buf = (char *)((uintptr_t *)param)[2];
                    int bufLen = ((uintptr_t *)param)[3];
                    loge("** aw-read-dir: dirId = %d, buf = %p, bufLen = %d",
                         dirId,buf,bufLen);
                    Parcel parcel;
                    Parcel replyParcel;
                    int fileNameLen = -1;
                    int32_t replyRet = -1;

                    *pRet = -1;

                    // write path string as a byte array
                    parcel.writeInt32(dirId);
                    sendEvent(AWEXTEND_MEDIA_INFO, AWEXTEND_MEDIA_INFO_REQUEST_READ_DIR,
                                0, &parcel, &replyParcel);
                    replyParcel.setDataPosition(0);
                    replyRet = replyParcel.readInt32();

                    if (0 == replyRet)
                    {
                        fileNameLen = replyParcel.readInt32();
                        if (fileNameLen > 0 && fileNameLen < bufLen)
                        {
                            const char* strdata = (const char*)replyParcel.readInplace(fileNameLen);
                            memcpy(buf, strdata, fileNameLen);
                            buf[fileNameLen] = 0;
                            *pRet = 0;
                        }
                    }
                    break;
                }
                case AW_EX_IOREQ_CLOSEDIR:
                {
                    int dirId = ((uintptr_t *)param)[0];
                    int *pRet = (int *)((uintptr_t *)param)[1];

                    Parcel parcel;
                    Parcel replyParcel;

                    // write path string as a byte array
                    parcel.writeInt32(dirId);
                    sendEvent(AWEXTEND_MEDIA_INFO,
                            AWEXTEND_MEDIA_INFO_REQUEST_CLOSE_DIR,
                            0, &parcel, &replyParcel);
                    replyParcel.setDataPosition(0);
                    *pRet = replyParcel.readInt32();

                   break;
                }
            }
        }
#endif
        case SUBCTRL_SUBTITLE_AVAILABLE:
        {
            Parcel parcel;
            unsigned int  nSubtitleId   = (unsigned int)((uintptr_t*)param)[0];
            SubtitleItem* pSubtitleItem = (SubtitleItem*)((uintptr_t*)param)[1];

            logd("subtitle available. id = %d, pSubtitleItem = %p",nSubtitleId,pSubtitleItem);
            if(pSubtitleItem == NULL)
            {
                logw("pSubtitleItem == NULL");
                break;
            }

            mPriData->mIsSubtitleInTextFormat = !!pSubtitleItem->bText;   //* 0 or 1.

            if(mPriData->mIsSubtitleDisable == 0)
            {
                if(mPriData->mIsSubtitleInTextFormat)
                {
                    SubtitleUtilsFillTextSubtitleToParcel(&parcel,
                                                          pSubtitleItem,
                                                          nSubtitleId,
                                                          mPriData->mDefaultTextFormat);
                }
                else
                {
                    //* clear the mSubtitleDisplayIds
                    memset(mPriData->mSubtitleDisplayIds,0xff,64*sizeof(unsigned int));
                    mPriData->mSubtitleDisplayIdsUpdateIndex = 0;
                    sendEvent(MEDIA_TIMED_TEXT);    //* clear bmp subtitle first
                    SubtitleUtilsFillBitmapSubtitleToParcel(&parcel, pSubtitleItem, nSubtitleId);
                }
                //*record subtitile id
                mPriData->mSubtitleDisplayIds[mPriData->mSubtitleDisplayIdsUpdateIndex] =
                                    nSubtitleId;
                mPriData->mSubtitleDisplayIdsUpdateIndex++;
                if(mPriData->mSubtitleDisplayIdsUpdateIndex>=64)
                    mPriData->mSubtitleDisplayIdsUpdateIndex = 0;

                logd("notify available message.");
                sendEvent(MEDIA_TIMED_TEXT, 0, 0, &parcel);
            }

            break;
        }

        case SUBCTRL_SUBTITLE_EXPIRED:
        {
            logd("subtitle expired.");
            Parcel       parcel;
            unsigned int nSubtitleId;
            int i;
            nSubtitleId = *(unsigned int*)param;

            if(mPriData->mIsSubtitleDisable == 0)
            {
                //* match the subtitle id which is displaying ,or we may clear null subtitle
                for(i=0;i<64;i++)
                {
                    if(nSubtitleId==mPriData->mSubtitleDisplayIds[i])
                        break;
                }

                if(i!=64)
                {
                    mPriData->mSubtitleDisplayIds[i] = 0xffffffff;
                    if(mPriData->mIsSubtitleInTextFormat == 1)
                    {
                        //* set subtitle id
                        parcel.writeInt32(KEY_GLOBAL_SETTING);

                        //* nofity app to hide this subtitle
                        parcel.writeInt32(KEY_STRUCT_AWEXTEND_HIDESUB);
                        parcel.writeInt32(1);
                        parcel.writeInt32(KEY_SUBTITLE_ID);
                        parcel.writeInt32(nSubtitleId);

                        logd("notify text expired message.");
                        sendEvent(MEDIA_TIMED_TEXT, 0, 0, &parcel);
                    }
                    else
                    {
                        //* clear the mSubtitleDisplayIds
                        memset(mPriData->mSubtitleDisplayIds,0xff,64*sizeof(unsigned int));
                        mPriData->mSubtitleDisplayIdsUpdateIndex = 0;
                        //* if the sub is bmp ,we just send "clear all" command,
                        //* nSubtitleId is not sent.
                        logd("notify subtitle expired message.");
                        sendEvent(MEDIA_TIMED_TEXT);
                    }
                }
            }
            break;
        }

        case AWPLAYER_MEDIA_SET_VIDEO_SIZE:
        {
            int nWidth  = ((int*)param)[0];
            int nHeight = ((int*)param)[1];

            logd("=== set videosize (%d, %d)", nWidth, nHeight);
            sendEvent(MEDIA_SET_VIDEO_SIZE, nWidth, nHeight);
            break;
        }

        default:
        {
            logw("message 0x%x not handled.", messageId);
            break;
        }
    }

    return OK;
}

static int XPlayerCallbackProcess(void* pUser, int eMessageId, int ext1, void* param)
{
    AwPlayer *p = (AwPlayer*)pUser;
    p->callbackProcess(eMessageId, ext1, param);

    return 0;
}

static int SubCallbackProcess(void* pUser, int eMessageId, void* param)
{
    AwPlayer* p = (AwPlayer*)pUser;

    p->callbackProcess(eMessageId, 0, param);

    return 0;

}

static int GetCallingApkName(char* strApkName, int nMaxNameSize)
{
    int fd;

    snprintf(strApkName, nMaxNameSize, "/proc/%d/cmdline",
                IPCThreadState::self()->getCallingPid());
    fd = ::open(strApkName, O_RDONLY);
    strApkName[0] = '\0';
    if (fd >= 0)
    {
        strApkName[nMaxNameSize - 1] = '\0';
        ::read(fd, strApkName, nMaxNameSize - 1);
        ::close(fd);
        logd("Calling process is: %s", strApkName);
    }
    return 0;
}

void enableMediaBoost(MediaInfo* mi)
{
    char cmd[PROP_VALUE_MAX] = {0};
    int total = 0;
    struct ScMemOpsS *memops = NULL;

    if(mi == NULL || mi->pVideoStreamInfo == NULL)
    {
        logd("input invalid args.");
        return;
    }

#if defined(CONF_MEDIA_BOOST_MEM)
    if(mi->pVideoStreamInfo->bSecureStreamFlagLevel1 == 1)
    {
        memops =  SecureMemAdapterGetOpsS();
    }
    else
    {
        memops =  MemAdapterGetOpsS();
    }
    CdcMemOpen(memops);
    total = CdcMemTotalSize(memops);
    CdcMemClose(memops);

    //set the mem property
    if((mi->pVideoStreamInfo->nWidth >= WIDTH_4K || mi->pVideoStreamInfo->nHeight >= HEIGHT_4K)
      && total < 190000 )
    {
        sprintf(cmd, "model%d:3", getpid());
        logd("setprop media.boost.pref %s", cmd);
        property_set("media.boost.pref", cmd);
    }
#endif

//setprop media.boost.pref mode123:0:c:num
//attention!!! Just for h5, num(1,2,3,4) lock cpu num; num(5) lock freq.
#if defined(CONF_MEDIA_BOOST_CPU)
    sprintf(cmd, "mode%d:0:c:5", getpid());
    logd("setprop media.boost.pref %s", cmd);
    property_set("media.boost.pref", cmd);
#endif
}

void disableMediaBoost()
{
    char value[PROP_VALUE_MAX] = {0};

    property_get("media.boost.pref", value, "0");

    size_t len = strlen(value);
    if (len > 1 && value[len - 1] > '0')
    {
        value[len - 1] = '0';
        logd("setprop media.boost.pref %s", value);
        property_set("media.boost.pref", value);
    }
}

