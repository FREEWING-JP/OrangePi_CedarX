/*
 * Copyright (c) 2008-2016 Allwinner Technology Co. Ltd.
 * All rights reserved.
 *
 * File : CdxAviFileIn.h
 * Description : Part of avi parser.
 * History :
 *
 */

#ifndef _CDX_AVI_FILE_IN_H_
#define _CDX_AVI_FILE_IN_H_

/*******************************************************************************
struct name: _AVI_FILE_IN_
Note:   This struct is used to store stuff chunk from file.
*******************************************************************************/
typedef struct _AVI_FILE_IN_    //��AVI_DEPACK����������������
{
    CdxStreamT        *fp;//used to read video chunk.����ǻ���index���ȡ��ʽ������Ҫ��3���ļ�ָ��.
    CdxStreamT        *audFp;    //used to read audio chunk
    CdxStreamT        *idxFp;    //used to read index table.
    AviChunkT         *avih;     //avi file header����Ҫmalloc,���ǳ��Ȳ���
                                 //ȫ�����ƣ�ԭʼ�ļ������ݡ��������޸����ݣ���ʹ����
                                 //��avih->dwMicroSecPerFrame

    cdx_int32           nStream;    //��¼AVI�ļ�����������Ƶ��������MainAviHeader�����ݡ�
    AviStreamInfoT      *sInfo[MAX_STREAM]; //malloc�����ģ�ͨ��get_next_stream_info(),
                                            //һ��SInfo��Ӧһ��LIST strl��ȫ������,���±�ž���
                                            //stream��index����������ԭʼ�ļ������ݣ�
                                            //��ʹ��Ϊ�������Դ��󣬲����޸�
    AudioStreamInfoT    audInfoArray[MAX_AUDIO_STREAM]; //�����������Ƶ��Ϣ������ʵ�ʡ�
    SubStreamInfoT      subInfoArray[MAX_SUBTITLE_STREAM];
    cdx_int8            hasIdx1;    //idx1 index table exsit,����ʾidx1���Ƿ���ڶ�������������
                                    //ֻҪ���쳣,has_idx1��0.
    cdx_int8            hasIndx;    //indx index table exsit (Open_DML AVI),ֻҪ��odml��ʽ��
                                    //��һ����1�������0����ʾ��AVI1.0��ʽ

    // readmode & idx_style decide the AVI parsing style and FFRR.
    enum READ_CHUNK_MODE    readmode;   //enum READ_CHUNK_MODE, READ_CHUNK_SEQUENCE
    enum USE_INDEX_STYLE    idxStyle;

    cdx_int32       moviStart;     //fileoffset,after "movi", ֻ��RIFF AVI���ֵ�movi_start,
                                   //RIFF AVIX�����ǣ�����CDX_S32�㹻
    cdx_int32       moviEnd;       //fileoffset,point to next byte of last_movi_byte.
                                   //ֻ��RIFF AVI���ֵ�movi_end�� RIFF AVIX�����ǣ� ����CDX_S32�㹻
    cdx_uint32      *idx1Buf;      //old:frame_count, offset, audio_time, (audio chunk offset),
                                   //two type key frame index table,(1)3 items,(2)ref to
                                   //IDX_TABLE_ITEM. idx1_buf�ǽ�FFRRKeyframeTable��buffer��
    cdx_uint32      *idx1BufEnd;   //point to the next byte of idx1's last byte.
    cdx_int32       idx1Total;     //total idx1 items in idx1 chunk��idx1ר��
    cdx_int32       idx1Start;     //offset from the file start.ָ���һ��idx1_entry��λ�á�
    //cdx_int32       key_frame_index;  //current nearest key frame index,
                                        //index in keyframe_table items.���key_frame�ڹؼ�֡����
                                        //��index�������ܵ�frame index���е�index��
    //cdx_int32       last_key_frame_index;   //last decode key frame index when ff/rr
    cdx_int32       indexInKeyfrmTbl;    //current nearest key frame index, index in
                                        //keyframe_table items.���key_frame�ڹؼ�֡����
                                        //��index�������ܵ�frame index���е�index��
    cdx_int32       lastIndexInKeyfrmTbl;//last decode key frame index when ff/rr��
                                         //Ҳ������һ�ξ�����entry����keyframe table�е����,
                                         //ÿ��״̬ת��ʱ,��SetProcMode()->AVI_IoCtrl()�г�ʼ��Ϊ-1
    cdx_int32       ckBaseOffset;  //idx1��ָʾ��chunk offset value has two cases:
                                   //(1)from movi start,(2)from file start.
                                   //We base on file start to calc the start address.
    //cdx_int32       index_count;            //total items in created index buffer(idx1_buf).
    cdx_int32       indexCountInKeyfrmTbl;//total items in created index buffer(idx1_buf).
    cdx_int32       noPreLoad;            //audio before video, means pre_load.
    cdx_int32       drmFlag;
    cdx_uint64      fileSize;         //right now, we use 32bits for file length
    AviChunkT       dataChunk;        //for store chunk data.һ�����MAX_CHUNK_BUF_SIZE��С��
    //�ڴ������buffer��������OpenMediaFile()�л��AVI_release_data_chunk_buf()�ͷŵ�����ʡ�ڴ�

    //CDX_S64       aud_chunk_counter;   //calc audio chunk count, not include current audio chunk
    //CDX_S64       aud_chunk_total_size;//calc current audio chunk total size, not include
                                         //current audio chunk size

    //˳��ʽ��, ��GetNextChunkInfo()�У�����һ��chunkͷ֮��,���ۼ�ͳ�ƣ�
    //����������ľ�����һ��audio chunk����ʼPTS��.
    //˳��ʽ��index��ʽ������3������.��ΪGetNextChunkInfo()��ͳ��audio����ʱ��Ĵ����ǹ��õġ�
    //indexģʽ��index reader���Լ���ͳ�Ʊ���������������.
    //��4��������avi_inͳ��ʱ���õģ�״̬ת��ʱ������
    cdx_int32       frameIndex;            //frame idx in all frames.
    //��GetNextChunkInfo()�л�����, ״̬ת��ʱ
    //setprocmode()->__reconfig_avi_read_context_readmode1()�����ã�һ���Ǵӵ�һ֡����
    cdx_int32       frameIndex1;           //for Video stream 1
    cdx_int32       frameIndex2;           //for Video stream 2
    cdx_int64       nAudChunkCounterArray[MAX_AUDIO_STREAM];  //calc audio chunk count, not include
    // current audio chunk��˳��ģʽ�£���һ���Ǵ��ļ�ͷ��ʼ�����ģ�
    // ��FRRR->PLAY֮�󣬶�λ�ĵط���ʼ��
    cdx_int64       nAudFrameCounterArray[MAX_AUDIO_STREAM];  //����ͬ��.VBR��ƵӦ����֡Ϊ��λ����,
    //һ��chunk��һ������һ֡.����nAudChunkCounterArray�ľ�ȷ���
    cdx_int64       nAudChunkTotalSizeArray[MAX_AUDIO_STREAM];//calc current audio chunk total
    //size, not include current audio chunk size, ˳��ģʽ�£�
    //��һ���Ǵ��ļ�ͷ��ʼ�����ģ���FRRR->PLAY֮�󣬶�λ�ĵط���ʼ��
    cdx_uint32      uBaseAudioPtsArray[MAX_AUDIO_STREAM]; //pts of the audio who is follow
    //video key frame,��׼ʱ��,��parse������,audio_pts�ļ����Ի�׼ʱ����϶��������ݵĳ���
    //ʱ��Ϊ׼,˳��ʽ��index��ʽ�¶����������
    //for FFRR, after search key frame, reset avsync!�����ּ����µ�����:��ʱ��¼��ǰaudio chunk
    //��PTS���Ա��ڻ����������ȷ�������������ʱ��
    struct PsrIdx1TableReader    vidIdx1Reader;   //use idx_fp, use vid_idx_data_chunk
    struct PsrIdx1TableReader    audIdx1Reader;   //use idx_fp, use aud_idx_data_chunk

    ODML_SUPERINDEX_READER  vidIndxReader;   //use idx_fp, use vid_idx_data_chunk
    ODML_SUPERINDEX_READER  audIndxReader;   //use idx_fp, use aud_idx_data_chunk

    cdx_char        *vopPrivInf; //vop private information, for some video format
                                 //to store private header information,e.g:H264,
                                 //VC1.��Ҫʱ��malloc�ڴ棬Ȼ��ָ�븳ֵ��p->vFormat.pCodecExtraData
    cdx_uint32     isNetworkStream;
} AviFileInT;

extern cdx_int32 AviMallocDataChunkBuf(AviFileInT *pAviIn);
extern cdx_int32 AviReleaseDataChunkBuf(AviFileInT *aviIn);
extern cdx_int16 GetNextChunk(CdxStreamT *fp, AviChunkT *chunk);
extern cdx_int16 AviReaderGetStreamInfo(AviFileInT *aviFile, AviStreamInfoT *sInfo,
    cdx_int32 index);
cdx_int32 CalcAviAudioChunkPts(AudioStreamInfoT *pAudioStreamInfo, cdx_int64 totalSize,
    cdx_int32 totalNum);
cdx_int64 CalcAviSubChunkPts(cdx_uint8 *buf, cdx_int64 *duration);
extern cdx_int16 GetNextChunkHead(CdxStreamT *pf, AviChunkT *chunk, cdx_uint32 *length);

#endif  /* _CDX_AVI_FILE_IN_H_ */

