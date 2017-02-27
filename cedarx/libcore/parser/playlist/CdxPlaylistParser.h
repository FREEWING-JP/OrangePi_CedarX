/*
 * Copyright (c) 2008-2016 Allwinner Technology Co. Ltd.
 * All rights reserved.
 *
 * File : CdxPlaylistParser.h
 * Description : Part of play list Parser.
 * History :
 *
 */

#ifndef PLAYLIST_PARSER_H
#define PLAYLIST_PARSER_H
#include "PlaylistParser.h"
#include <pthread.h>
#include <CdxTypes.h>
#include <CdxParser.h>
#include <CdxStream.h>
#include <CdxAtomic.h>

enum CdxParserStatus
{
    CDX_PSR_INITIALIZED,
    CDX_PSR_IDLE,
    CDX_PSR_PREFETCHING,
    CDX_PSR_PREFETCHED,
    CDX_PSR_SEEKING,
    CDX_PSR_READING,
};

typedef struct CdxPlaylistParser
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
    char *playlistUrl;/*����playlist�ĸ���*/
    char *playlistBuf;/*����m3u8�ļ�������*/
    cdx_int64 playlistBufSize;

    Playlist* mPlaylist;
    int curSeqNum;
    cdx_int64 baseTimeUs;

    CdxParserT *child;/*Ϊ��parser����һ����ý���ļ�parser����flv parser*/
    CdxStreamT *childStream;
    CdxParserT *tmpChild;
    CdxStreamT *tmpChildStream;

    CdxMediaInfoT mediaInfo;
    CdxPacketT cdxPkt;
}CdxPlaylistParser;

#endif
