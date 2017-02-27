/*
* Copyright (c) 2008-2016 Allwinner Technology Co. Ltd.
* All rights reserved.
*
* File : CdxHlsParser.h
* Description :
* History :
*   Author  : Kewei Han
*   Date    : 2014/10/08
*/

#ifndef HLS_PARSER_H
#define HLS_PARSER_H
#include <stdint.h>
#include <pthread.h>
#include <CdxTypes.h>
#include <CdxParser.h>
#include <CdxStream.h>
#include <CdxAtomic.h>
#include <semaphore.h>

#define MaxNumBandwidthItems (16)
enum CdxParserStatus {
    CDX_PSR_INITIALIZED,
    CDX_PSR_IDLE,
    CDX_PSR_PREFETCHING,
    CDX_PSR_PREFETCHED,
    CDX_PSR_SEEKING,
    CDX_PSR_READING,
};

enum RefreshState {
    INITIAL_MINIMUM_RELOAD_DELAY,
    FIRST_UNCHANGED_RELOAD_ATTEMPT,
    SECOND_UNCHANGED_RELOAD_ATTEMPT,
    THIRD_UNCHANGED_RELOAD_ATTEMPT,
    FOURTH_UNCHANGED_RELOAD_ATTEMPT,
    FIFTH_UNCHANGED_RELOAD_ATTEMPT,
    SIXTH_UNCHANGED_RELOAD_ATTEMPT
};

typedef struct SelectTrace SelectTrace;
struct SelectTrace {
    MediaType        mType;
    AString          *groupID;/*������Trace������һ��MediaGroup*/
    unsigned char    mNum;/*������Trace����MediaGroup�е���һ��MediaItem*/
};

typedef struct MediaPlaylistItemInf MediaPlaylistItemInf;
struct MediaPlaylistItemInf {
    PlaylistItem  *item;
    cdx_int64     size;
    int           givenPcr;
};

struct PeriodicTask {
    sem_t        semStop;
    pthread_t    tid;
    int          enable;
    sem_t        semTimeshift;
};

typedef struct {
    CdxParserT base;
    enum CdxParserStatus status;
    int mErrno;
    cdx_uint32 flags;/*ʹ�ܱ�־*/

    int forceStop;          /* for forceStop()*/
    pthread_mutex_t statusLock;
    pthread_cond_t cond;

    CdxStreamT *cdxStream;/*��Ӧ�����ɸ�CdxHlsParser��m3u8�ļ�*/
    CdxDataSourceT cdxDataSource;/*��;1:playlist�ĸ��£���;2:child��*/
    char *m3u8Url;/*����playlist�ĸ���*/
    char *m3u8Buf;/*����m3u8�ļ�������*/
    cdx_int64 m3u8BufSize;

    Playlist* mPlaylist;
    pthread_rwlock_t rwlockPlaylist;
    int isMasterParser;

    ParserCallback callback;
    void *pUserData;

    //void *father;
    //int (*cbFunc)(void *);
    int update;
    union {
        struct {
            int64_t mLastPlaylistFetchTimeUs;
            enum RefreshState refreshState;
            uint8_t mMD5[16];
            int curSeqNum;
            cdx_int64 baseTimeUs;
            MediaPlaylistItemInf curItemInf;
            PlaylistItem *cipherReference;

            //int64_t durationUs;
        }media;
        struct {
            int bandwidthSortIndex[MaxNumBandwidthItems];
            int bandwidthCount;
            int curBandwidthIndex;
            int preBandwidthIndex;
            SelectTrace selectTrace[3];/*��¼��ǰ��ѡ�Ĺ�������ڹ����ѡ����л�*/
            int curSeqNum[3];/*ѡ����ʱ���������ǰ��SeqNum*/
        }master;
    } u;

    CdxParserT *child[3];/*Ϊ��parser����һ����hls parser����ý���ļ�parser����TS parser*/
    CdxStreamT *childStream[3];
    cdx_uint64 streamPts[3];
    int prefetchType;

    CdxMediaInfoT mediaInfo;
    CdxPacketT cdxPkt;
//    struct StreamCacheStateS cacheState;
    AesStreamExtraDataT aesCipherInf;
    int ivIsAppointed;
    char *aesUri;

    struct StreamSeekPos streamSeekPos;
    ExtraDataContainerT extraDataContainer;

    int mPlayQuality;
    int curDownloadObject;
    char *curDownloadUri;
    int mIsTimeShift;

    //* YUNOS UUID
    char mYunOSUUID[64];

    int refreshFaild;
    struct PeriodicTask PeriodicTask;

    int timeShiftLastSeqNum;
    int64_t ptsShift;

    int64_t streamOpenTimeout;
}CdxHlsParser;

#endif
