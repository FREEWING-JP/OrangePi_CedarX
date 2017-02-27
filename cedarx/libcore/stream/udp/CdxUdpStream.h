/*
 * Copyright (c) 2008-2016 Allwinner Technology Co. Ltd.
 * All rights reserved.
 *
 * File : CdxUdpStream.h
 * Description : UdpStream
 * History :
 *
 */

#ifndef UDP_STREAM_H
#define UDP_STREAM_H
#include <pthread.h>
#include <CdxTypes.h>
#include <CdxStream.h>
#include <CdxAtomic.h>
#include <CdxUrl.h>

#include <arpa/inet.h>

#define BANDWIDTH_ARRAY_SIZE (100)

enum CdxStreamStatus
{
    STREAM_IDLE,
    STREAM_CONNECTING,
    STREAM_SEEKING,
    STREAM_READING,
};

typedef struct BandwidthEntry
{
    cdx_int64 downloadTimeCost;
    cdx_int32 downloadSize;
}BandwidthEntryT;

typedef struct CdxUdpStream
{
    CdxStreamT base;
    cdx_uint32 attribute;
    enum CdxStreamStatus status;
    cdx_int32 ioState;
    pthread_mutex_t lock;
    pthread_cond_t cond;
    int forceStop;
    cdx_int32 exitFlag;/*��ҪUdpDownloadThread�˳�*/
    int downloadThreadExit;/*UdpDownloadThread�Ѿ��˳�*/

    cdx_char *sourceUri;/*��dataSource��õ�ԭʼ��uri*/
    CdxUrlT* url; /*scheme,port,etc*/
    CdxStreamProbeDataT probeData;
    int socketFd;
    int isMulticast;/*�Ƿ�ಥ*/
    struct ip_mreq multicast;
    pthread_t threadId;/*UdpDownloadThread*/

    cdx_uint8 *bigBuf;
    cdx_uint32 bufSize;
    cdx_uint32 validDataSize;
    cdx_uint32 writePos;/*���Ҫд���λ��*/
    cdx_uint32 readPos;/*���Ҫ��ȡ��λ��*/
    cdx_uint32 endPos;/*���Ҫ��ȡ���ݵĽ�ֹλ��, 0��ʾ��û�б�����*/
    cdx_int64 accumulatedDownload;/*�ۼ�����������*/
    pthread_mutex_t bufferMutex;

 //���ƴ���֮��
    BandwidthEntryT mBWArray[BANDWIDTH_ARRAY_SIZE];
    cdx_int32 mBWWritePos;
    cdx_int32 mBWCount;
    cdx_int64 mBWTotalTimeCost;
    cdx_int32 mBWTotalSize;

}CdxUdpStream;

#endif
