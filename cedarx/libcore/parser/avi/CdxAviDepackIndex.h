/*
 * Copyright (c) 2008-2016 Allwinner Technology Co. Ltd.
 * All rights reserved.
 *
 * File : CdxAviDepackIndex.h
 * Description : Part of avi parser.
 * History :
 *
 */

#ifndef _CDX_AVI_DEPACK_INDEX_H_
#define _CDX_AVI_DEPACK_INDEX_H_

//ԭ��40�ֽ�, ����4����Ļ���100�ֽ�; 8����Ļ�,180�ֽ�
//            ����           72�ֽ�,            116�ֽ�  120�ֽ�
typedef struct IdxTableItemT
{
    cdx_int32   frameIdx;
    cdx_int64   vidChunkOffset;          // for FFRR.ָ��chunk head
    cdx_int64   vidChunkIndexOffset;     // offset of video chunk index entry, absolue offset!
    cdx_uint32  audPtsArray[MAX_AUDIO_STREAM];
    //����������£�ÿ������Ķ�Ӧ��ʱ���(���ų���ʱ��)�����������ο�p->hasAudio.
    //��ǰ�������±�ο�p->CurItemNumInAudioStreamIndexArray
    //��Ϊ�ǻ���index��Ķ�ȡ��ʽ,���Լ�¼�����֡��PTS֮���һ��audio chunk��Я����PTS.��λ:ms
    cdx_int64   audChunkIndexOffsetArray[MAX_AUDIO_STREAM];
    // offset of audio chunk index entry, absolute offset!
    //�Ǹ�audio frame��Ӧ��index���е�entry�ľ��Ե�ַ
}IdxTableItemT;   //Ϊ����index���ȡ��ʽ��Ƶģ�������˱��entry

typedef struct {
    cdx_int64   ckOffset;       //point to chunk head!
    cdx_int64   ixTblEntOffset; //chunk��Ӧ��index���е�entry���ڵ�λ�ã����Ե�ַ, idx1��entryҲ����
    cdx_int32   chunkBodySize;  //chunk body�Ĵ�С��������chunk head
    cdx_int32   isKeyframe;     //for video chunk
} AVI_CHUNK_POSITION;           //��¼����odml indx entry��ָʾ��chunk����Ϣ��
                                //Ҳ����ָʾidx1���entry

extern cdx_int32 AviGetIndexByNumReadMode1(CdxAviParserImplT *p, cdx_int32 mode,
                cdx_uint32 *frameNum, cdx_uint32 *offSet, cdx_uint64 *position, cdx_int32 *diff);
extern cdx_int32 ReconfigAviReadContextReadMode1(CdxAviParserImplT *p,
                    cdx_uint32 vidTime, cdx_int32 searchDirection, cdx_int32 searchMode);
extern cdx_int32 ConfigNewAudioAviReadContextIndexMode(CdxAviParserImplT *p);
extern cdx_int16 AviReadByIndex(CdxAviParserImplT *p);

//avi_idx1.h
extern cdx_int32 SearchNextIdx1IndexEntry(struct PsrIdx1TableReader *pReader,
    AVI_CHUNK_POSITION *pChunkPos);
extern cdx_int16 AviBuildIdxForIndexMode(CdxAviParserImplT *p);
extern cdx_int32 InitialPsrIdx1TableReader(struct PsrIdx1TableReader *pReader,
                                    AviFileInT *aviIn, cdx_int32 streamIndex);
extern void DeinitialPsrIdx1TableReader(struct PsrIdx1TableReader *pReader);

//avi_odml_indx.h
extern cdx_int32 IsIndxChunkId(FOURCC fcc);
extern cdx_int32 LoadIndxChunk(ODML_SUPERINDEX_READER *pSuperReader, cdx_int32 indxTblEntryIdx);
extern cdx_int32 SearchNextODMLIndexEntry(ODML_SUPERINDEX_READER *pSuperReader,
    AVI_CHUNK_POSITION *pCkPos);
extern cdx_int16 AviBuildIdxForODMLIndexMode(CdxAviParserImplT *p);
extern cdx_int32 InitialPsrIndxTableReader(ODML_SUPERINDEX_READER *pReader,
    AviFileInT *aviIn, cdx_int32 streamIndex);
extern void DeinitialPsrIndxTableReader(ODML_SUPERINDEX_READER *pReader);

#endif  /* _CDX_AVI_DEPACK_INDEX_H_ */

