/*
 * Copyright (c) 2008-2016 Allwinner Technology Co. Ltd.
 * All rights reserved.
 *
 * File : CdxAviIdx1.h
 * Description : Part of avi parser.
 * History :
 *
 */

#ifndef _AVI_IDX1_H_
#define _AVI_IDX1_H_
typedef struct AviIndexEntryT
{
    cdx_uint32        ckid;  //�ļ������ֽ�˳�����У���cdx_uint32����,
    cdx_uint32        dwFlags;
    cdx_uint32        dwChunkOffset;        // Position of chunk
    cdx_uint32        dwChunkLength;        // Length of chunk
}AviIndexEntryT;    //idx1���ļ���ʽ

struct PsrIdx1TableReader{
    cdx_int32   leftIdxItem;      //all left idx item number.�ļ���ʣ�µĻ�û�ж���
                                  //reader��filebuffer�ĵ�idx1 entry������
    cdx_int32   bufIdxItem;       //the filebuffer contain the idx item number.
                                  //It is a counter.��ʾfilebuffer�л�ʣ�¶���idx1 entryû�ж�ȡ

    CdxStreamT    *idxFp;          //in common, fp has already point to idx_start.
    cdx_int64   fpPos;             //indicate the location of current idx in file.
    cdx_uint8    *fileBuffer;      //used to store index. length:INDEX_CHUNK_BUFFER_SIZE,
                                   //malloc memory already.
    AviIndexEntryT *pIdxEntry;     //point to filebuffer.
    cdx_int32   streamIdx;         //indicate which stream's chunk, we want to search.
                                   //ָʾ��ȡ�ĸ�stream��idx1��
    cdx_uint32   ckBaseOffset;     //avi file index entry indicate the  chunk offset,
               // ckBaseOffset indicate this chunk'offset's, start address(base on the file start).
    cdx_int32   readEndFlg;        //0:not read end, 1:read end index table.

    //��ȡindex��ʱ�ļ�¼����Щ���Ƕ�idx1��ó���ͳ�����ݡ����Ŷ�����ʱ,����������Щͳ������
    //ʹ��avi_in->uAudioPtsArray[]��3������
    cdx_int32   chunkCounter;     //������,��¼�����õ㿪ʼ���˶��ٸ�chunk
    cdx_int32   chunkSize;        //��ǰ������chunk��body size.
    cdx_int64   chunkIndexOffset; //last audio chunk's index item's offset.
            //����¼��chunk��idx1���е�entry����ʼ��ַ��������ffrrkeyframetable������
    cdx_int64   totalChunkSize;   //before last audio chunk's all audio chunks' total size.
                                  //��¼�����õ㿪ʼ���Ѷ���chunk�����ֽ���
};
//extern cdx_int32 search_next_idx1_index_entry(struct psr_idx1_table_reader *preader,
// AVI_CHUNK_POSITION *pChunkPos);
//extern CDX_S16 AVI_build_idx_for_index_mode(struct FILE_PARSER *p);
//extern cdx_int32 initial_psr_idx1_table_reader(struct psr_idx1_table_reader *preader,
// AVI_FILE_IN *avi_in, cdx_int32 stream_index);
//extern void deinitial_psr_idx1_table_reader(struct psr_idx1_table_reader *preader);
#endif  /* _AVI_IDX1_H_ */

