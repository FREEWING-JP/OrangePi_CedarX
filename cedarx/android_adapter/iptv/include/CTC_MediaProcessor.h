/*
 * Copyright (c) 2008-2016 Allwinner Technology Co. Ltd.
 * All rights reserved.
 *
 * File : CTC_MediaProcessor.h
 * Description : CTC_MediaProcessor
 * History :
 *
 */

#ifndef _CTC_MEDIAPROCESSOR_H_
#define _CTC_MEDIAPROCESSOR_H_

extern "C" {
#include "vformat.h"
#include "aformat.h"
}

#include <gui/Surface.h>

using namespace android;
#define NewInterface (1)

/**
 * @��Ƶ����
 */
typedef struct {
    unsigned short    pid;//pid
    int                nVideoWidth;//��Ƶ���
    int                nVideoHeight;//��Ƶ�߶�
    int                nFrameRate;//֡��
    vformat_t        vFmt;//��Ƶ��ʽ
    unsigned long    cFmt;//�����ʽ
}VIDEO_PARA_T, *PVIDEO_PARA_T;

/**
 * @��Ƶ����
 */
typedef struct {
    unsigned short    pid;            //audio pid
    int                nChannels;        //������
    int                nSampleRate;    //������
#if !NewInterface
    unsigned short    block_align;    //block align
    int             bit_per_sample;    //������
#endif
    aformat_t        aFmt;            //��Ƶ��ʽ
    int                nExtraSize;
    unsigned char*    pExtraData;
}AUDIO_PARA_T, *PAUDIO_PARA_T;

/**
 * @��Ƶ��ʾģʽ
 */
typedef enum {
    IPTV_PLAYER_CONTENTMODE_NULL        = -1,
    IPTV_PLAYER_CONTENTMODE_LETTERBOX,            //Դ�������
    IPTV_PLAYER_CONTENTMODE_FULL,                //ȫ�����
}IPTV_PLAYER_CONTENTMODE_e;

/**
 * @��������
 */
typedef enum {
    IPTV_PLAYER_STREAMTYPE_NULL        = -1,
    IPTV_PLAYER_STREAMTYPE_TS,                    //TS����
    IPTV_PLAYER_STREAMTYPE_VIDEO,                //ES Video����
    IPTV_PLAYER_STREAMTYPE_AUDIO,                //ES Audio����
}IPTV_PLAYER_STREAMTYPE_e;

/**
 * @����״̬
 */
typedef enum {
    IPTV_PLAYER_STATE_OTHER            = -1,
    IPTV_PLAYER_STATE_PLAY,                        //Play State
    IPTV_PLAYER_STATE_PAUSE,                    //Pause State
    IPTV_PLAYER_STATE_STOP,                        //Stop State
}IPTV_PLAYER_STATE_e;

/**
 * @�¼�����
 */
typedef enum {
    IPTV_PLAYER_EVT_FIRST_PTS,                    //�����һ֡
    IPTV_PLAYER_EVT_ABEND,                        //�쳣��ֹ
    IPTV_PLAYER_EVT_MAX,
}IPTV_PLAYER_EVT_e;

/**
 * @brief                 �¼��ص�������ָ��
 * @param evt             �¼�����
 * @param handler           �û����
 * @param param1         �û�����
 * @param param2         �û�����
 **/
typedef void (*IPTV_PLAYER_EVT_CB)(IPTV_PLAYER_EVT_e evt, void *handler,
                        unsigned long param1, unsigned long param2);

/**
 * @CTC_Mediaprocessor
 */
class CTC_MediaProcessor{
public:
    CTC_MediaProcessor(){}
    virtual ~CTC_MediaProcessor(){}
public:
    //ȡ�ò���ģʽ
    virtual int  GetPlayMode()=0;
    //��ʾ����
    virtual int  SetVideoWindow(int x,int y,int width,int height)=0;
    //��ʾ��Ƶ
    virtual int  VideoShow(void)=0;
    //������Ƶ
    virtual int  VideoHide(void)=0;
    /**
     * @brief    ��ʼ����Ƶ����
     * @param    pVideoPara - ��Ƶ��ز���������ES��ʽ����ʱ��pid=0xffff;
     *                        û��video������£�vFmt=VFORMAT_UNKNOWN
     * @return    ��
     **/
    virtual void InitVideo(PVIDEO_PARA_T pVideoPara) = 0;

    /**
     * @brief    ��ʼ����Ƶ����
     * @param    pAudioPara - ��Ƶ��ز���������ES��ʽ����ʱ��pid=0xffff;
     *                         û��audio������£�aFmt=AFORMAT_UNKNOWN
     * @return    ��
     **/
    virtual void InitAudio(PAUDIO_PARA_T pAudioPara) = 0;

    //��ʼ����
    virtual bool StartPlay()=0;
#if NewInterface
    //��ts��д��
    virtual int WriteData(unsigned char* pBuffer, unsigned int nSize) = 0;
#else
    /**
     * @brief    ��ȡnSize��С��buffer
     * @param    type    - ts/video/audio
     *             pBuffer - buffer��ַ�������nSize��С�Ŀռ䣬����Ϊ��Ӧ�ĵ�ַ;�������nSize����NULL
     *             nSize   - ��Ҫ��buffer��С
     * @return    0  - �ɹ�
     *             -1 - ʧ��
     **/
    virtual int    GetWriteBuffer(IPTV_PLAYER_STREAMTYPE_e type,
                    unsigned char** pBuffer, unsigned int *nSize) = 0;

    /**
     * @brief    д�����ݣ���GetWriteBuffer��Ӧʹ��
     * @param    type      - ts/video/audio
     *             pBuffer   - GetWriteBuffer�õ���buffer��ַ
     *             nSize     - д������ݴ�С
     *             timestamp - ES Video/Audio��Ӧ��ʱ���(90KHZ)
     * @return    0  - �ɹ�
     *             -1 - ʧ��
     **/

    virtual int WriteData(IPTV_PLAYER_STREAMTYPE_e type, unsigned char* pBuffer,
                        unsigned int nSize, uint64_t timestamp) = 0;
#endif
    //��ͣ
    virtual bool Pause()=0;
    //��������
    virtual bool Resume()=0;
    //�������
    virtual bool Fast()=0;
    //ֹͣ�������
    virtual bool StopFast()=0;
    //ֹͣ
    virtual bool Stop()=0;
    //��λ
    virtual bool Seek()=0;
    //�趨����
    virtual bool SetVolume(int volume)=0;
    //��ȡ����
    virtual int GetVolume()=0;
#if NewInterface
    //�趨��Ƶ��ʾ����
    virtual bool SetRatio(int nRatio)=0;
#else
    /**
     * @brief    ���û�����ʾ����
     * @param    contentMode - Դ����/ȫ����Ĭ��ȫ��
     * @return    ��
     **/
    virtual void SetContentMode(IPTV_PLAYER_CONTENTMODE_e contentMode) = 0;
#endif
    /**
     * @brief    ��ȡ��ǰ����
     * @param    ��
     * @return    1 - left channel
     *             2 - right channel
     *             3 - stereo
     **/
    virtual int GetAudioBalance() = 0;

    /**
     * @brief    ���ò�������
     * @param    nAudioBanlance - 1.left channel;2.right channle;3.stereo
     * @return    true  - �ɹ�
     *             false - ʧ��
     **/
    virtual bool SetAudioBalance(int nAudioBalance) = 0;
    //��ȡ��Ƶ�ֱ���
    virtual void GetVideoPixels(int& width, int& height)=0;
    virtual bool IsSoftFit()=0;
    virtual void SetEPGSize(int w, int h)=0;
    virtual void SetSurface(Surface *pSurface)=0;

#if NewInterface
    virtual void SwitchAudioTrack(int pid)=0;
#else
    virtual void SwitchAudioTrack(int pid, PAUDIO_PARA_T pAudioPara)=0;
#endif
    //ѡ����Ļ��pid:��ǰѡ�����Ļpid���������Ļ������ӿ���StartPlay֮ǰҲ�����һ�Ρ�
    virtual void SwitchSubtitle(int pid)=0;
    //����Ƶ���������
    virtual void SetProperty(int nType, int nSub, int nValue)=0;
    //��ȡ��ǰ����ʱ��(��λ:����)
    virtual long GetCurrentPlayTime()=0;
    //�뿪Ƶ�������뿪Ƶ��ʱ���øýӿڣ���������ڽ�����һ��Ƶ���ٴ�Start��
    //�����������¼�£�Ȼ����StartPlay�ӿ��м������žͺ�
    virtual void leaveChannel()=0;
    virtual void playerback_register_evt_cb(IPTV_PLAYER_EVT_CB pfunc, void *handler) = 0;
    virtual int  GetAbendNum()=0;

#if !NewInterface
    virtual int GetBufferStatus(long *totalsize, long *datasize) = 0;

    /**
     * @brief    ���Ž���ʱ�Ƿ������һ֡
     * @param    bHoldLastPic - true/falseֹͣ����ʱ�Ƿ������һ֡
     * @return    ��
     **/
    virtual void SetStopMode(bool bHoldLastPic ) = 0;
#endif
};

CTC_MediaProcessor* GetMediaProcessor();
int GetMediaProcessorVersion();

#endif
