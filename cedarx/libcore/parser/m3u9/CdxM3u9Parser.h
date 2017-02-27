/*
 * Copyright (c) 2008-2016 Allwinner Technology Co. Ltd.
 * All rights reserved.
 *
 * File : CdxM3u9Parser.h
 * Description : Part of m3u9 parser.
 * History :
 *
 */

#ifndef M3U9_PARSER_H
#define M3U9_PARSER_H
#include "M3U9Parser.h"
#include <pthread.h>
#include <CdxTypes.h>
#include <CdxParser.h>
#include <CdxStream.h>
#include <CdxAtomic.h>

#define _SAVE_VIDEO_STREAM (0)
#define _SAVE_AUDIO_STREAM (0)

enum CdxParserStatus
{
    CDX_PSR_INITIALIZED,
    CDX_PSR_IDLE,
    CDX_PSR_PREFETCHING,
    CDX_PSR_PREFETCHED,
    CDX_PSR_SEEKING,
    CDX_PSR_READING,
};

typedef struct CdxM3u9Parser
{
    CdxParserT base;
    enum CdxParserStatus status;
    int mErrno;
    cdx_uint32 flags;/*ʹ�ܱ�־*/
    
    int forceStop;          /* for forceStop()*/
    pthread_mutex_t statusLock;
    pthread_cond_t cond;
    
    CdxStreamT *cdxStream;/*��Ӧ�����ɸ�CdxHlsParser��m3u8�ļ�*/
    CdxDataSourceT cdxDataSource;/*��;1:playlist�ĸ��£���;2:child��*/
    char *m3u9Url;/*����playlist�ĸ���*/
    char *m3u9Buf;/*����m3u8�ļ�������*/
    cdx_int64 m3u9BufSize;
    
    Playlist* mPlaylist;
    int curSeqNum;
    cdx_int64 baseTimeUs;

    CdxParserT *child;/*Ϊ��parser����һ����ý���ļ�parser����flv parser*/
    CdxStreamT *childStream;

    CdxMediaInfoT mediaInfo;
    CdxPacketT cdxPkt;

#if _SAVE_VIDEO_STREAM
    FILE* fpVideoStream;
#endif
#if _SAVE_AUDIO_STREAM
    FILE* fpAudioStream;
#endif
#if _SAVE_AUDIO_STREAM || _SAVE_VIDEO_STREAM
    char url[256];
#endif
}CdxM3u9Parser;

#endif
