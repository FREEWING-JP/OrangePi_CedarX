/*
* Copyright (c) 2008-2016 Allwinner Technology Co. Ltd.
* All rights reserved.
*
* File : cache.c
* Description : cache policy for net stream
* History :
*   Author  : AL3
*   Date    : 2015/05/05
*   Comment : first version
*
*/

#include <unistd.h>
#include <stdlib.h>
#include <malloc.h>
#include "cache.h"
#include "cdx_log.h"
#include "iniparserapi.h"

// should not larger than 5 / 10 and less than 1 / 10
#define MIN_PASSED_DATA_KEPT_RATIO      3 / 10
#define MIN_PASSED_DATA_KEPT_SIZE(x)    ((x)->nMaxBufferSize * MIN_PASSED_DATA_KEPT_RATIO)

static int START_PLAY_CACHE_VIDEO_FRAME_NUM = -1;

static void StreamCacheFlushPassedList(StreamCache* c);
static int  StreamCacheIsKeyFrame(StreamCache* c, CacheNode* pNode);
static int64_t StreamCacheSeekByPts(StreamCache* c, int64_t nSeekTimeUs);
static int64_t StreamCacheSeekByPcr(StreamCache* c, int64_t nSeekTimeUs);
static int StreamCacheSeekByOffset(StreamCache* c, int64_t nOffset);
static int StreamCachePlayerCacheTime(Player* p);

static int StreamCacheIsMpeg12KeyFrame(CacheNode* pNode);
static int StreamCacheIsWMV3KeyFrame(CacheNode* pNode);
static int StreamCacheIsH264KeyFrame(CacheNode* pNode);
static int StreamCacheIsH265KeyFrame(CacheNode* pNode);

StreamCache* StreamCacheCreate(void)
{
    StreamCache* c;
    c = (StreamCache*)malloc(sizeof(StreamCache));
    if(c == NULL)
        return NULL;
    memset(c, 0, sizeof(StreamCache));

    c->eContainerFormat  = CDX_PARSER_UNKNOW;
    c->eVideoCodecFormat = VIDEO_CODEC_FORMAT_UNKNOWN;
    c->nLastValidPts     = -1;
    c->nLastValidPcr     = -1;
    c->nFirstPts         = -1;
    c->nMaxBufferSize = 10 * 1024 * 1024;
    c->nStartPlaySize = 1024;
    pthread_mutex_init(&c->mutex, NULL);
    return c;
}

static void NodeInfoDestroy(CacheNode *node)
{
    if (node->info == NULL)
        return;

    if(node->eMediaType == CDX_MEDIA_VIDEO)
    {
        struct VideoInfo *pVideoInfo = (struct VideoInfo *)node->info;
        int i;
        for(i = 0; i < pVideoInfo->videoNum; i++)
        {
            if(pVideoInfo->video[i].pCodecSpecificData)
            {
                free(pVideoInfo->video[i].pCodecSpecificData);
                pVideoInfo->video[i].pCodecSpecificData = NULL;
            }
        }
    }
    else if(node->eMediaType == CDX_MEDIA_AUDIO)
    {
        struct AudioInfo *pAudioInfo = (struct AudioInfo *)node->info;
        int i;
        for(i = 0; i < pAudioInfo->audioNum; i++)
        {
            if(pAudioInfo->audio[i].pCodecSpecificData)
            {
                free(pAudioInfo->audio[i].pCodecSpecificData);
                pAudioInfo->audio[i].pCodecSpecificData = NULL;
            }
        }
    }

    free(node->info);
    node->info = NULL;
    return;
}

void StreamCacheDestroy(StreamCache* c)
{
    CacheNode* node;

    pthread_mutex_lock(&c->mutex);
    node = c->pPassedHead;
    while(node != NULL)
    {
        c->pPassedHead = node->pNext;
        free(node->pData);
        NodeInfoDestroy(node);
        free(node);
        node = c->pPassedHead;
    }
    pthread_mutex_unlock(&c->mutex);

    free(c);

    return;
}


#if (10 * MIN_PASSED_DATA_KEPT_RATIO > 5)
#error "MIN_PASSED_DATA_KEPT_RATIO too large"
#elif (10 * MIN_PASSED_DATA_KEPT_RATIO < 1)
#error "MIN_PASSED_DATA_KEPT_RATIO too little"
#endif
void StreamCacheSetSize(StreamCache* c, int nStartPlaySize, int nMaxBufferSize)
{
    int n = nMaxBufferSize * MIN_PASSED_DATA_KEPT_RATIO;
    if (n <= 0)
    {
        loge("check nMaxBufferSize");
        return;
    }

    if ((nStartPlaySize + n) > (nMaxBufferSize * 8 / 10))
        nStartPlaySize = nMaxBufferSize * 8 / 10 - n;

    pthread_mutex_lock(&c->mutex);
    c->nMaxBufferSize = nMaxBufferSize;
    c->nStartPlaySize = nStartPlaySize;
    pthread_mutex_unlock(&c->mutex);

    if (unlikely(START_PLAY_CACHE_VIDEO_FRAME_NUM < 0))
        START_PLAY_CACHE_VIDEO_FRAME_NUM =
            GetConfigParamterInt("start_play_cache_video_frame_num", 30);

    return;
}


int StreamCacheGetSize(StreamCache* c)
{
    return c->nDataSize;
}


int StreamCacheUnderflow(StreamCache* c)
{
    int bUnderFlow;
    pthread_mutex_lock(&c->mutex);
    bUnderFlow = (c->nFrameNum <= 0);
    pthread_mutex_unlock(&c->mutex);
    return bUnderFlow;
}


int StreamCacheOverflow(StreamCache* c)
{
    int bOverFlow;
    pthread_mutex_lock(&c->mutex);
    bOverFlow = ((c->nDataSize + c->nPassedDataSize) >= c->nMaxBufferSize) &&
                (c->nPassedDataSize <= MIN_PASSED_DATA_KEPT_SIZE(c));
    pthread_mutex_unlock(&c->mutex);
    return bOverFlow;
}


int StreamCacheDataEnough(StreamCache* c)
{
    int bDataEnough;
    pthread_mutex_lock(&c->mutex);
    if(c->eVideoCodecFormat != VIDEO_CODEC_FORMAT_UNKNOWN)
    {
        bDataEnough = (c->nDataSize >= c->nStartPlaySize) &&
                  (c->nVideoFrameNum >= START_PLAY_CACHE_VIDEO_FRAME_NUM);
        logv("nDataSize %d, nStartPlaySize %d, nVideoFrameNum %d, data enough %d",
                c->nDataSize, c->nStartPlaySize, c->nVideoFrameNum, bDataEnough);
    }
    else
    {
        bDataEnough = (c->nDataSize >= c->nStartPlaySize) &&
                  (c->nFrameNum > 0);
    }
    pthread_mutex_unlock(&c->mutex);
    return bDataEnough;
}


CacheNode* StreamCacheNextFrame(StreamCache* c)
{
    CacheNode* node;
    pthread_mutex_lock(&c->mutex);
    node = c->pHead;
    pthread_mutex_unlock(&c->mutex);
    return node;
}


void StreamCacheFlushOneFrame(StreamCache* c)
{
    CacheNode* node;

    pthread_mutex_lock(&c->mutex);

    node = c->pHead;
    if (unlikely(node == NULL))
    {
        loge("This is impossible! Call StreamCacheNextFrame() first.");
        pthread_mutex_unlock(&c->mutex);
        return;
    }

    c->pHead  = node->pNext;
    c->nFrameNum--;
    c->nDataSize -= node->nLength;

    c->nPassedDataSize += node->nLength;
    c->nPassedFrameNum++;

    if (node->eMediaType == CDX_MEDIA_VIDEO) {
        c->nVideoFrameNum--;
        c->nPassedVideoFrameNum++;
    }

    pthread_mutex_unlock(&c->mutex);
    return;
}


int StreamCacheAddOneFrame(StreamCache* c, CacheNode* node)
{
    CacheNode* newNode;

    newNode = (CacheNode*)malloc(sizeof(CacheNode));
    if(newNode == NULL)
        return -1;

    *newNode = *node;
    newNode->pNext = NULL;

    if(newNode->nPts != -1)
    {
        c->nLastValidPts = node->nPts;
        c->pNodeWithLastValidPts = newNode;
        if (unlikely(c->nFirstPts == -1))
            c->nFirstPts = node->nPts;
    }
    if(newNode->nPcr != -1)
    {
        c->nLastValidPcr = node->nPcr;
        c->pNodeWithLastValidPcr = newNode;
    }

    pthread_mutex_lock(&c->mutex);

    while ((c->nDataSize + c->nPassedDataSize) >= c->nMaxBufferSize &&
            c->nPassedDataSize > MIN_PASSED_DATA_KEPT_SIZE(c))
        StreamCacheFlushPassedList(c);

    if (likely(c->pTail != NULL))
    {
        c->pTail->pNext = newNode;
        c->pTail = c->pTail->pNext;
        /* after call StreamCacheFlushOneFrame, pHead can be NULL */
        if (c->pHead == NULL)
            c->pHead = newNode;
    }
    else
    {
        c->pPassedHead = c->pHead = c->pTail = newNode;
    }

    c->nDataSize += newNode->nLength;
    c->nFrameNum++;
    if (node->eMediaType == CDX_MEDIA_VIDEO)
        c->nVideoFrameNum++;

    pthread_mutex_unlock(&c->mutex);

    return 0;
}


void StreamCacheFlushAll(StreamCache* c)
{
    CacheNode* node;

    pthread_mutex_lock(&c->mutex);

    node = c->pPassedHead;
    while(node != NULL)
    {
        c->pPassedHead = node->pNext;
        free(node->pData);
        NodeInfoDestroy(node);
        free(node);
        node = c->pPassedHead;
    }

    c->nDataSize = 0;
    c->nFrameNum = 0;
    c->nVideoFrameNum = 0;
    c->pHead = c->pTail = NULL;
    c->nPassedDataSize = 0;
    c->nPassedFrameNum = 0;
    c->nPassedVideoFrameNum = 0;
    c->nLastValidPts         = -1;
    c->pNodeWithLastValidPts = NULL;
    c->nLastValidPcr         = -1;
    c->pNodeWithLastValidPcr = NULL;

    pthread_mutex_unlock(&c->mutex);
    return;
}


int StreamCacheGetBufferFullness(StreamCache* c)
{
    logd("c->nPassedDataSize %dM, c->nDataSize %dM, c->nMaxBufferSize %dM",
            c->nPassedDataSize / 1024 / 1024,
            c->nDataSize  / 1024 / 1024,
            c->nMaxBufferSize / 1024 / 1024);

    return (c->nDataSize * 100LL) / c->nMaxBufferSize;
}


int StreamCacheGetLoadingProgress(StreamCache* c)
{
    if(c->nDataSize >= c->nStartPlaySize)
        return 100;
    else
        return (c->nDataSize * 100LL) / c->nStartPlaySize;
}


int StreamCacheSetMediaFormat(StreamCache*           c,
                              CdxParserTypeT         eContainerFormat,
                              enum EVIDEOCODECFORMAT eVideoCodecFormat,
                              int                    nBitrate)
{
    c->eContainerFormat  = eContainerFormat;
    c->eVideoCodecFormat = eVideoCodecFormat;
    c->nBitrate          = nBitrate;
    return 0;
}


int StreamCacheSetPlayer(StreamCache* c, Player* pPlayer)
{
    c->pPlayer = pPlayer;
    return 0;
}


static void StreamCacheFlushPassedList(StreamCache* c)
{
    CacheNode* node = c->pPassedHead;
    if (node == c->pHead || node == c->pTail || node == NULL)
    {
        loge("Please check!");
        return;
    }

    c->pPassedHead  = node->pNext;
    c->nPassedFrameNum--;
    c->nPassedDataSize -= node->nLength;

    if(node == c->pNodeWithLastValidPts)
    {
        c->nLastValidPts = -1;
        c->pNodeWithLastValidPts = NULL;
    }
    if(node == c->pNodeWithLastValidPcr)
    {
        c->nLastValidPcr = -1;
        c->pNodeWithLastValidPcr = NULL;
    }

    free(node->pData);
    NodeInfoDestroy(node);
    free(node);

    return;
}

static int getSeekOffset(StreamCache* c, int64_t nSeekTimeUs, int64_t *pOffSet)
{
    int nBitrate = c->nBitrate;
    if (nBitrate <= 0)
    {
        nBitrate = 0;
        if(PlayerHasVideo(c->pPlayer))
            nBitrate += PlayerGetVideoBitrate(c->pPlayer);
        if(PlayerHasAudio(c->pPlayer))
            nBitrate += PlayerGetAudioBitrate(c->pPlayer);
    }

    if (nBitrate <= 0)
        return -1;

    int64_t nCurrUs = PlayerGetPosition(c->pPlayer);
    if (nCurrUs < 0)
        return -1;

    int64_t nTimeUs = nSeekTimeUs - nCurrUs;
    if (nTimeUs > 0)
        nTimeUs -= StreamCachePlayerCacheTime(c->pPlayer);

    *pOffSet = (nBitrate * nTimeUs / (8*1000*1000));
    if (nTimeUs < 0)
    {
        double timeRatio = nCurrUs ? -(double)nTimeUs / nCurrUs : 0;
        double offSetRatio = c->nPassedDataSize ?
                             -(double)*pOffSet / c->nPassedDataSize : 2;
        if (timeRatio > offSetRatio)
        {
            logd("offset calculated from bitrate is too small, "
                    "timeRatio %f, offSetRatio %f, use timeRatio",
                    timeRatio, offSetRatio);
            *pOffSet = -timeRatio * c->nPassedDataSize;
        }
    }

    return 0;
}

static void adjustList(StreamCache *c, CacheNode *pNodeFound)
{
    int nPassedDataSize = 0;
    int nPassedFrameNum = 0;
    int nPassedVideoFrameNum = 0;
    CacheNode *pNode;
    for (pNode = c->pPassedHead; pNode != pNodeFound; pNode = pNode->pNext)
    {
        nPassedDataSize += pNode->nLength;
        nPassedFrameNum++;
        if (pNode->eMediaType == CDX_MEDIA_VIDEO)
            nPassedVideoFrameNum++;
    }
    c->pHead = pNodeFound;
    c->nDataSize = c->nPassedDataSize + c->nDataSize - nPassedDataSize;
    c->nFrameNum = c->nPassedFrameNum + c->nFrameNum - nPassedFrameNum;
    c->nVideoFrameNum = c->nPassedVideoFrameNum + c->nVideoFrameNum - nPassedVideoFrameNum;
    c->nPassedDataSize = nPassedDataSize;
    c->nPassedFrameNum = nPassedFrameNum;
    c->nPassedVideoFrameNum = nPassedVideoFrameNum;
}

int64_t StreamCacheSeekTo(StreamCache* c, int64_t nSeekTimeUs)
{
    int64_t ret;
    switch(c->eContainerFormat)
    {
        case CDX_PARSER_TS:
        case CDX_PARSER_BD:
        {

            int64_t nByteOffset;
            if (getSeekOffset(c, nSeekTimeUs, &nByteOffset) == -1)
                return -1;
#if 0
            /* Try seek by pts first */
            CacheNode *pHead = c->pHead;
            int nOldPassedSize = c->nPassedDataSize;
            int64_t nTimeUs = nSeekTimeUs + c->nFirstPts;

            ret = StreamCacheSeekByPts(c, nTimeUs);
            if (ret != -1)
            {
                int n = c->nPassedDataSize - nOldPassedSize;
                logd("offsetByPts / offsetByBitrate %llf", (double)n/nByteOffset);
                if (abs(n - nByteOffset) * 4 < abs(nByteOffset))
                    return ret - c->nFirstPts;

                logd("difference between seekByPts and seekByBitrate is too big");
                adjustList(c, pHead);
            }
#endif
            ret = StreamCacheSeekByOffset(c, nByteOffset);
            if(ret == 0)
                ret = nSeekTimeUs;
            break;
        }

        case CDX_PARSER_HLS:
            ret = StreamCacheSeekByPcr(c, nSeekTimeUs);
            break;

        default:
            ret = StreamCacheSeekByPts(c, nSeekTimeUs);
            break;
    }

    return ret;
}

static int64_t StreamCacheSeekByPts(StreamCache* c, int64_t nSeekTimeUs)
{
    if(c->nLastValidPts < nSeekTimeUs)
        return -1;

    CacheNode* pNode;
    for (pNode = c->pPassedHead; pNode != NULL; pNode = pNode->pNext)
    {
        if (pNode->nPts == -1)
            continue;
        if (pNode->nPts > nSeekTimeUs)
            return -1;
        if (StreamCacheIsKeyFrame(c, pNode))
            break;
    }

    if(pNode == NULL)
        return -1;

    //*  find the last node with pts small than nSeekTimeUs,
    //*  find the first node with pts bigger than nSeekTimeUs,
    //*  choose the one with pts more near to nSeekTimeUs as new list head.
    int64_t nLastPtsBefore = -1;
    int64_t nFirstPtsAfter = 0x7fffffffffffffffLL;
    int64_t nCurPts;
    CacheNode *pLastNodeBefore = NULL;
    CacheNode *pFirstNodeAfter = NULL;
    for ( ; pNode != NULL; pNode = pNode->pNext)
    {
        nCurPts = pNode->nPts;
        if (nCurPts == -1 || !StreamCacheIsKeyFrame(c, pNode))
            continue;
        if(nCurPts <= nSeekTimeUs && nCurPts > nLastPtsBefore)
        {
            //* update the first node before.
            nLastPtsBefore  = nCurPts;
            pLastNodeBefore = pNode;
        }
        else if(nCurPts > nSeekTimeUs)
        {
            //* set the last node after.
            nFirstPtsAfter  = nCurPts;
            pFirstNodeAfter = pNode;
            break;
        }
    }

    if (pFirstNodeAfter == NULL && nSeekTimeUs - nLastPtsBefore >= 1000000)
        return -1;

    if ((nSeekTimeUs - nLastPtsBefore) <= (nFirstPtsAfter - nSeekTimeUs))
        pNode = pLastNodeBefore;
    else
        pNode = pFirstNodeAfter;

    adjustList(c, pNode);
    return c->pHead->nPts;
}


static int64_t StreamCacheSeekByPcr(StreamCache* c, int64_t nSeekTimeUs)
{
    CacheNode* pNode = c->pNodeWithLastValidPcr;
    if (pNode == NULL)
        return -1;
    if (c->nLastValidPts - pNode->nPts + c->nLastValidPcr < nSeekTimeUs)
        return -1;

    for (pNode = c->pPassedHead; pNode != NULL; pNode = pNode->pNext)
    {
        if (pNode->nPcr > nSeekTimeUs)
            return -1;
        else if (pNode->nPcr != -1)
            break;
    }

    //*  find the last node with pcr small than nSeekTimeUs,
    //*  find the first node with pcr bigger than nSeekTimeUs,
    //*  choose the one with pcr more near to nSeekTimeUs as new list head.
    int64_t nLastPcrBefore = -1;
    int64_t nFirstPcrAfter = 0x7fffffffffffffffLL;
    int64_t nCurPcr;
    CacheNode *pLastNodeBefore = NULL;
    CacheNode *pFirstNodeAfter = NULL;
    for ( ; pNode != NULL; pNode = pNode->pNext)
    {
        nCurPcr = pNode->nPcr;
        if (nCurPcr > nLastPcrBefore && nCurPcr <= nSeekTimeUs)
        {
            //* update the first node before.
            nLastPcrBefore  = nCurPcr;
            pLastNodeBefore = pNode;
        }
        else if (nCurPcr > nSeekTimeUs)
        {
            //* set the last node after.
            nFirstPcrAfter  = nCurPcr;
            pFirstNodeAfter = pNode;
            break;
        }
    }

    int64_t nPtsBase = pLastNodeBefore->nPcr;
    int64_t nPtsOffset = pLastNodeBefore->nPts;
    int64_t nFoundMappedPts = nPtsBase;
    CacheNode* pNodeFound = pLastNodeBefore;
    pNode = pLastNodeBefore->pNext;
    for ( ; pNode != pFirstNodeAfter; pNode = pNode->pNext)
    {
        if(pNode->nPts == -1 || !StreamCacheIsKeyFrame(c, pNode))
            continue;

        int64_t nMappedPts = pNode->nPts - nPtsOffset + nPtsBase;
        if (nMappedPts <= nSeekTimeUs && nMappedPts > nFoundMappedPts)
        {
            nFoundMappedPts = nMappedPts;
            pNodeFound = pNode;
        }
        else if (nMappedPts > nSeekTimeUs)
        {
            nFirstPcrAfter = nMappedPts;
            pFirstNodeAfter = pNode;
            break;
        }
    }

    logv("nSeekTimeUs %lld, nFoundMappedPts %lld, nFirstPcrAfter %lld",
            nSeekTimeUs, nFoundMappedPts, nFirstPcrAfter);
    /* check if nFirstPcrAfter is more close to nSeekTimeUs */
    if ((nFirstPcrAfter - nSeekTimeUs) < (nSeekTimeUs - nFoundMappedPts))
    {
        nFoundMappedPts = nFirstPcrAfter;
        pNodeFound = pFirstNodeAfter;
    }

    /* Since HLS is not expected to support precise seekto, deviation less than
     * 3 seconds is acceptable.
     */
    if(pFirstNodeAfter == NULL && (nSeekTimeUs - nFoundMappedPts) > 3000000)
        return -1;

    adjustList(c, pNodeFound);
    return nFoundMappedPts;
}


static int StreamCacheSeekByOffset(StreamCache* c, int64_t nSeekOffset)
{
    /* Actually, if nSeekOffset == c->nDataSize, pNodeFound should be the next
     * node after pTail. However, I prefer don't flush cache as many as
     * possible.
     */
    logv("nSeekOffset %lld, c->nPassedDataSize %d", nSeekOffset, c->nPassedDataSize);
    if ((nSeekOffset < -c->nPassedDataSize) || (nSeekOffset > c->nDataSize))
        return -1;

    CacheNode* pNode;
    if (nSeekOffset < 0)
    {
        nSeekOffset += c->nPassedDataSize;
        pNode = c->pPassedHead;
    }
    else
        pNode = c->pHead;

    int offset = 0;
    for ( ; pNode != NULL; pNode = pNode->pNext)
    {
        offset += pNode->nLength;
        /* see the comment at the beginning of this function */
        if (offset >= nSeekOffset)
            break;
    }

    adjustList(c, pNode);
    return 0;
}


static int StreamCacheIsKeyFrame(StreamCache* c, CacheNode* pNode)
{
    if(c->eVideoCodecFormat == VIDEO_CODEC_FORMAT_UNKNOWN)
        return pNode->eMediaType == CDX_MEDIA_AUDIO;

    if(c->eContainerFormat == CDX_PARSER_TS ||
       c->eContainerFormat == CDX_PARSER_BD ||
       c->eContainerFormat == CDX_PARSER_HLS)
    {
        switch (c->eVideoCodecFormat)
        {
            case VIDEO_CODEC_FORMAT_H264:
                return StreamCacheIsH264KeyFrame(pNode);
            case VIDEO_CODEC_FORMAT_MPEG1:
            case VIDEO_CODEC_FORMAT_MPEG2:
                return StreamCacheIsMpeg12KeyFrame(pNode);
            case VIDEO_CODEC_FORMAT_WMV3:
                return StreamCacheIsWMV3KeyFrame(pNode);
            case VIDEO_CODEC_FORMAT_H265:
                return StreamCacheIsH265KeyFrame(pNode);
            default:
                return 0;
        }
    }

    return pNode->nFlags & KEY_FRAME && pNode->eMediaType == CDX_MEDIA_VIDEO;
}


static int StreamCachePlayerCacheTime(Player* p)
{
    int     bHasVideo;
    int     bHasAudio;
    int     nPictureNum;
    int     nFrameDuration;
    int     nPcmDataSize;
    int     nSampleRate;
    int     nChannelCount;
    int     nBitsPerSample;
    int     nStreamDataSize;
    int     nBitrate;
    int64_t nVideoCacheTime;
    int64_t nAudioCacheTime;

    nVideoCacheTime = 0;
    nAudioCacheTime = 0;
    bHasVideo = PlayerHasVideo(p);
    bHasAudio = PlayerHasAudio(p);

    if(bHasVideo == 0 && bHasAudio == 0)
        return 0;

    const int64_t unit = 8 * 1000 * 1000;
    if(bHasVideo)
    {
        nPictureNum     = PlayerGetValidPictureNum(p);
        nFrameDuration  = PlayerGetVideoFrameDuration(p);
        nStreamDataSize = PlayerGetVideoStreamDataSize(p);
        nBitrate        = PlayerGetVideoBitrate(p);

        nVideoCacheTime = nPictureNum*nFrameDuration;

        if(nBitrate > 0)
            nVideoCacheTime += nStreamDataSize * unit / nBitrate;
    }

    if(bHasAudio)
    {
        nPcmDataSize    = PlayerGetAudioPcmDataSize(p);
        nStreamDataSize = PlayerGetAudioStreamDataSize(p);
        nBitrate        = PlayerGetAudioBitrate(p);
        PlayerGetAudioParam(p, &nSampleRate, &nChannelCount, &nBitsPerSample);

        nAudioCacheTime = 0;

        if(nSampleRate != 0 && nChannelCount != 0 && nBitsPerSample != 0)
        {
            nAudioCacheTime += nPcmDataSize * unit /
                (nSampleRate * nChannelCount * nBitsPerSample);
        }

        if(nBitrate > 0)
            nAudioCacheTime += nStreamDataSize * unit/nBitrate;
    }

    return (int)(nVideoCacheTime + nAudioCacheTime)/(bHasVideo + bHasAudio);
}


static int StreamCacheIsMpeg12KeyFrame(CacheNode* pNode)
{
    unsigned int   code;
    unsigned int   pictureType;
    unsigned char* ptr;

    if(pNode->nLength < 6)
        return 0;

    code = 0xffffffff;
    for(ptr = pNode->pData; ptr <= pNode->pData + pNode->nLength - 6;)
    {
        code = code<<8 | *ptr++;
        if (code == 0x01b3 ||   //* sequence header.
            code == 0x01b8)     //* gop header
            return 1;

        if(code == 0x0100)      //* picture header, check picture type.
        {
            pictureType = (ptr[1]>>3) & 0x7;
            if(pictureType == 1)
                return 1;
            else
                return 0;
        }
    }

    return 0;
}


static int StreamCacheIsWMV3KeyFrame(CacheNode* pNode)
{
    unsigned int    code;
    unsigned char*  ptr;

    if(pNode->nLength < 16)
        return 0;

    code = 0xffffffff;

    for (ptr = pNode->pData; ptr <= pNode->pData + pNode->nLength - 16;)
    {
        code = code<<8 | *ptr++;
        if (code == 0x010f) //* sequence header
            return 1;
    }

    return 0;
}


static int StreamCacheIsH264KeyFrame(CacheNode* pNode)
{
    unsigned int   code;
    unsigned int   tmp;
    unsigned char* ptr;

    if(pNode->nLength < 16)
        return 0;

    code = 0xffffffff;

    for (ptr = pNode->pData; ptr <= pNode->pData + pNode->nLength - 16;)
    {
        code = code<<8 | *ptr++;
        tmp = code & 0xffffff1f;
        if (tmp == 0x0107 ||    //* sps
            tmp == 0x0108 ||    //* pps
            tmp == 0x0105)      //* idr
            return 1;
#if 0
        if(tmp == 0x0101)   //* slice NULU, check mbNum==0 and pictureType;
        {
            mbNum = ReadGolomb(...);   //* Ue() not implement here.
            type  = ReadGolomb(...);
            if(mbNum == 0 && (type == 2 || type == 7))
                return 1;
        }
#endif
    }

    return 0;
}


static int StreamCacheIsH265KeyFrame(CacheNode* pNode)
{
    unsigned int   code;
    unsigned char* ptr;
    unsigned int   tmp;

    if(pNode->nLength < 16)
        return 0;

    code = 0xffffffff;

    for (ptr = pNode->pData; ptr <= pNode->pData + pNode->nLength - 16;)
    {
        code = code<<8 | *ptr++;
        if (code == 0x0140 ||   //* vps
            code == 0x0142 ||   //* sps
            code == 0x0144 ||   //* pps
            code == 0x0126 ||   //* key frame
            code == 0x0128 ||   //* key frame
            code == 0x012a)     //* key frame
        {
            if(*ptr == 0x01)
                return 1;
        }
    }

    return 0;
}
