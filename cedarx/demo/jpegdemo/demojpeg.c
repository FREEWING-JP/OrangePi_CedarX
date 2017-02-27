/*
 * Copyright (c) 2008-2016 Allwinner Technology Co. Ltd.
 * All rights reserved.
 *
 * File : demojpeg.c
 * Description : demojpeg
 * History :
 *
 */

#include <stdio.h>

#include <cdx_log.h>
#include <vdecoder.h>
#include "memoryAdapter.h"
#include <errno.h>

#define SAVE_RGB (1)
typedef struct VideoFrame
{
    // Intentional public access modifier:
    uint32_t mWidth;
    uint32_t mHeight;
    uint32_t mDisplayWidth;
    uint32_t mDisplayHeight;
    uint32_t mSize;            // Number of bytes in mData
    uint8_t* mData;            // Actual binary data
    int32_t  mRotationAngle;   // rotation angle, clockwise
}VideoFrame;

#if SAVE_RGB
//-------------------------------------------------------------------
 /*
����λͼ�ļ������
          �ṹ���� �� ��
     λͼ�ļ�ͷ (bitmap-file header) BITMAPFILEHEADER bmfh
    λͼ��Ϣͷ (bitmap-information header) BITMAPINFOHEADER bmih
    ��ɫ��(color table) RGBQUAD aColors[]
    ͼ�����������ֽ� BYTE aBitmapBits[]
  */
typedef struct bmp_header
{
    short twobyte           ;//�����ֽڣ�������֤�����Ա�������У��������ַ�����д���ļ���
         //14B
    char bfType[2]          ;//!�ļ�������,��ֵ������0x4D42��Ҳ�����ַ�'BM'
    unsigned int bfSize     ;//!˵���ļ��Ĵ�С�����ֽ�Ϊ��λ
    unsigned int bfReserved1;//��������������Ϊ0
    unsigned int bfOffBits  ;//!˵�����ļ�ͷ��ʼ��ʵ�ʵ�ͼ������֮����ֽڵ�ƫ����������Ϊ14B+sizeof(BMPINFO)
}BMPHEADER;

typedef struct bmp_info
{
         //40B
     unsigned int biSize         ;//!BMPINFO�ṹ����Ҫ������
     int biWidth                 ;//!ͼ��Ŀ�ȣ�������Ϊ��λ
     int biHeight                ;//!ͼ��Ŀ�ȣ�������Ϊ��λ,�����ֵ��������
                            //˵��ͼ���ǵ���ģ������ֵ�Ǹ��������������
     unsigned short biPlanes     ;//!Ŀ���豸˵��λ��������ֵ�����Ǳ���Ϊ1
     unsigned short biBitCount   ;//!������/���أ���ֵΪ1��4��8��16��24����32
     unsigned int biCompression  ;//˵��ͼ������ѹ��������
     #define BI_RGB        0L    //û��ѹ��
     #define BI_RLE8       1L    //ÿ������8���ص�RLEѹ�����룬ѹ����ʽ��2�ֽ���ɣ��ظ����ؼ�������ɫ��������
     #define BI_RLE4       2L    //ÿ������4���ص�RLEѹ�����룬ѹ����ʽ��2�ֽ����
     #define BI_BITFIELDS  3L    //ÿ�����صı�����ָ�������������
     unsigned int biSizeImage    ;//ͼ��Ĵ�С�����ֽ�Ϊ��λ������BI_RGB��ʽʱ��������Ϊ0
     int biXPelsPerMeter         ;//ˮƽ�ֱ��ʣ�������/�ױ�ʾ
     int biYPelsPerMeter         ;//��ֱ�ֱ��ʣ�������/�ױ�ʾ
     unsigned int biClrUsed      ;//λͼʵ��ʹ�õĲ�ɫ���е���ɫ����������Ϊ0�Ļ�����˵��ʹ�����е�ɫ���
     unsigned int biClrImportant ;//��ͼ����ʾ����ҪӰ�����ɫ��������Ŀ�������0����ʾ����Ҫ��
}BMPINFO;

typedef struct tagRGBQUAD
{
    unsigned char rgbBlue;
    unsigned char rgbGreen;
    unsigned char rgbRed;
    unsigned char rgbReserved;
} RGBQUAD;

typedef struct tagBITMAPINFO
{
    BMPINFO    bmiHeader;
    //RGBQUAD    bmiColors[1];
    unsigned int rgb[3];
} BITMAPINFO;

static int get_rgb565_header(int w, int h, BMPHEADER * head, BITMAPINFO * info)
{
    int size = 0;
    if (head && info)
    {
        size = w * h * 2;
        memset(head, 0, sizeof(* head));
        memset(info, 0, sizeof(* info));
         head->bfType[0] = 'B';
         head->bfType[1] = 'M';
         head->bfOffBits = 14 + sizeof(* info);
         head->bfSize = head->bfOffBits + size;
         head->bfSize = (head->bfSize + 3) & ~3;
         size = head->bfSize - head->bfOffBits;

         info->bmiHeader.biSize = sizeof(info->bmiHeader);
         info->bmiHeader.biWidth = w;
         info->bmiHeader.biHeight = -h;
         info->bmiHeader.biPlanes = 1;
         info->bmiHeader.biBitCount = 16;
         info->bmiHeader.biCompression = BI_BITFIELDS;
         info->bmiHeader.biSizeImage = size;

         info->rgb[0] = 0xF800;
         info->rgb[1] = 0x07E0;
         info->rgb[2] = 0x001F;

         logd("rgb565:%dbit,%d*%d,%d\n", info->bmiHeader.biBitCount, w, h, head->bfSize);
     }
     return size;
 }

static int save_bmp_rgb565(FILE* fp, int width, int height, unsigned char* pData)
{
    int success = 0;
    int size = 0;
    BMPHEADER head;
    BITMAPINFO info;
    size = get_rgb565_header(width, height, &head, &info);
    if(size > 0)
    {
        fwrite(head.bfType,1,2,fp);
        fwrite(&head.bfSize,1,4,fp);
        fwrite(&head.bfReserved1,1,4,fp);
        fwrite(&head.bfOffBits,1,4,fp);

        fwrite(&info,1,sizeof(info), fp);
        fwrite(pData,1,size, fp);
        success = 1;
    }
    logd("*****success=%d\n", success);
    return success;
}

#endif

#if 0
static int get_rgb888_header(int w, int h, BMPHEADER * head, BITMAPINFO * info)
{
    int size = 0;
    if (head && info)
    {
        size = w * h * 3;
        memset(head, 0, sizeof(* head));
        memset(info, 0, sizeof(* info));
         head->bfType[0] = 'B';
         head->bfType[1] = 'M';
         head->bfOffBits = 14 + sizeof(* info);
         head->bfSize = head->bfOffBits + size;
         head->bfSize = (head->bfSize + 3) & ~3;
         size = head->bfSize - head->bfOffBits;

         info->bmiHeader.biSize = sizeof(info->bmiHeader);
         info->bmiHeader.biWidth = w;
         info->bmiHeader.biHeight = -h;
         info->bmiHeader.biPlanes = 1;
         info->bmiHeader.biBitCount = 24;
         info->bmiHeader.biCompression = BI_RGB;
         info->bmiHeader.biSizeImage = size;

         logd("rgb888:%dbit,%d*%d,%d\n", info->bmiHeader.biBitCount, w, h, head->bfSize);
     }
     return size;
 }

static int save_bmp_rgb888(FILE* fp, int width, int height, unsigned char* pData)
{
    int success = 0;
    int size = 0;
    BMPHEADER head;
    BITMAPINFO info;
    size = get_rgb888_header(width, height, &head, &info);
    if(size > 0)
    {
        fwrite(head.bfType,1,14,fp);
        fwrite(&info,1,sizeof(info), fp);
        fwrite(pData,1,size, fp);
        success = 1;
    }
    logd("*****success=%d\n", success);
    return success;
}

static int transformPictureMb32ToRGB888(VideoPicture* pPicture, unsigned char* pData,
                                    int nWidth, int nHeight)
{
    unsigned char*   pClipTable;
    unsigned char*   pClip;
    static const int nClipMin = -278;
    static const int nClipMax = 535;

    unsigned short*  pDst = NULL;
    unsigned char*   pSrcY = NULL;
    unsigned char*   pSrcVU = NULL;

    int x = 0;
    int y = 0;
    int nMbWidth = 0;
    int nMbHeight = 0;
    int nVMb = 0;
    int nHMb = 0;
    int yPos = 0;
    int pos = 0;
    int uvPos = 0;

    //* initialize the clip table.
    pClipTable = (unsigned char*)malloc(nClipMax - nClipMin + 1);
    if(pClipTable == NULL)
    {
        loge("can not allocate memory for the clip table, quit.");
        return -1;
    }
    for(x=nClipMin; x<=nClipMax; x++)
    {
        pClipTable[x-nClipMin] = (x<0) ? 0 : (x>255) ? 255 : x;
    }
    pClip = &pClipTable[-nClipMin];

    //* flush cache.
    MemAdapterFlushCache(pPicture->pData0, pPicture->nWidth*pPicture->nHeight);
    MemAdapterFlushCache(pPicture->pData1, pPicture->nHeight*pPicture->nHeight/2);

    pDst  = (unsigned short*)pData;
    logd("+++++ pDst: %p", pDst);
    pSrcY = (unsigned char*)pPicture->pData0;
    pSrcVU  = (unsigned char*)pPicture->pData1;

    nMbWidth = pPicture->nWidth/32;
    nMbHeight = pPicture->nHeight/32;

    for(nVMb=0; nVMb<nMbHeight;nVMb++)
    {
        for(nHMb=0; nHMb<nMbWidth; nHMb++)
        {
            #if 1
            pos = 3*(nVMb*pPicture->nWidth*32+nHMb*32);
            #else
            pos = nVMb*pPicture->nWidth*32+nHMb*32;
            #endif

            for(y=0; y<32; y++)
            {
                yPos = (nVMb*nMbWidth+nHMb)*1024+y*32;
                uvPos = ((nVMb/2)*nMbWidth*1024)+nHMb*1024+(y/2)*32+ (((nVMb%2)==1) ? 512 : 0);
                for(x=0; x<32; x+=2)
                {
                    signed y1 = (signed)pSrcY[yPos+x+0] - 16;
                    signed y2 = (signed)pSrcY[yPos+x+1] - 16;
                    signed u  = (signed)pSrcVU[uvPos+x+0] - 128;
                    signed v  = (signed)pSrcVU[uvPos+x+1] - 128;
                    signed u_b = u * 517;
                    signed u_g = -u * 100;
                    signed v_g = -v * 208;
                    signed v_r = v * 409;
                    signed tmp1 = y1 * 298;
                    signed b1 = (tmp1 + u_b) / 256;
                    signed g1 = (tmp1 + v_g + u_g) / 256;
                    signed r1 = (tmp1 + v_r) / 256;
                    signed tmp2 = y2 * 298;
                    signed b2 = (tmp2 + u_b) / 256;
                    signed g2 = (tmp2 + v_g + u_g) / 256;
                    signed r2 = (tmp2 + v_r) / 256;

                #if 1

                    pDst[pos+0] = pClip[r1];
                    pDst[pos+1] = pClip[g1];
                    pDst[pos+2] = pClip[b1];
                    pDst[pos+3] = pClip[r2];
                    pDst[pos+4] = pClip[g2];
                    pDst[pos+5] = pClip[b2];
                    pos += 6;

                }
                pos += 3*(nMbWidth-1)*32;

                #else
                    unsigned int rgb1 = ((pClip[r1] >> 3) << 11) |
                                        ((pClip[g1] >> 2) << 5)  |
                                        (pClip[b1] >> 3);

                    unsigned int rgb2 = ((pClip[r2] >> 3) << 11) |
                                        ((pClip[g2] >> 2) << 5)  |
                                        (pClip[b2] >> 3);
                    *(unsigned int *)(&pDst[pos]) = (rgb2 << 16) | rgb1;
                    pos += 2;
                }
                pos += (nMbWidth-1)*32;
                #endif
            }
        }
    }

    logd("pos: %d", pos);
    pDst  = (unsigned short*)pData;
    for(y=0; y<pPicture->nTopOffset; y++)
    {
        memset(pDst+y*nWidth, 0, 2*nWidth);
    }
    for(y=pPicture->nBottomOffset; y<nHeight; y++)
    {
        memset(pDst+y*nWidth, 0, 2*nWidth);
    }

    for(y=pPicture->nTopOffset; y<pPicture->nBottomOffset; y++)
    {
        memset(pDst+y*nWidth, 0, 2*pPicture->nLeftOffset);
        memset(pDst+y*nWidth+pPicture->nRightOffset, 0, 2*(nWidth-pPicture->nRightOffset));
    }

#if 1
    FILE* outFp = fopen("/mnt/UDISK/rgb.data", "wb");
    if(outFp != NULL)
    {
        logd("************save_bmp_rgb565\n");
        save_bmp_rgb888(outFp, nWidth, nHeight, pData);
        fwrite(pDst, 1, nWidth*nHeight*3, outFp);
        fclose(outFp);
    }
#endif

    free(pClipTable);

    return 0;
}
#endif

static int transformPictureMb32ToRGB(struct ScMemOpsS* memops,
                                            VideoPicture * pPicture, unsigned char* pData,
                                            int nWidth, int nHeight)
{
    unsigned char*   pClipTable;
    unsigned char*   pClip;
    static const int nClipMin = -278;
    static const int nClipMax = 535;

    unsigned short*  pDst = NULL;
    unsigned char*   pSrcY = NULL;
    unsigned char*   pSrcVU = NULL;

    int x = 0;
    int y = 0;
    int nMbWidth = 0;
    int nMbHeight = 0;
    int nVMb = 0;
    int nHMb = 0;
    int yPos = 0;
    int pos = 0;
    int uvPos = 0;

    //* initialize the clip table.
    pClipTable = (unsigned char*)malloc(nClipMax - nClipMin + 1);
    if(pClipTable == NULL)
    {
        loge("can not allocate memory for the clip table, quit.");
        return -1;
    }
    for(x=nClipMin; x<=nClipMax; x++)
    {
        pClipTable[x-nClipMin] = (x<0) ? 0 : (x>255) ? 255 : x;
    }
    pClip = &pClipTable[-nClipMin];

    //* flush cache.
    CdcMemFlushCache(memops, pPicture->pData0, pPicture->nWidth*pPicture->nHeight);
    CdcMemFlushCache(memops, pPicture->pData1, pPicture->nHeight*pPicture->nHeight/2);

    pDst  = (unsigned short*)pData;
    logd("+++++ pDst: %p", pDst);
    pSrcY = (unsigned char*)pPicture->pData0;
    pSrcVU  = (unsigned char*)pPicture->pData1;

    nMbWidth = pPicture->nWidth/32;
    nMbHeight = pPicture->nHeight/32;

    for(nVMb=0; nVMb<nMbHeight;nVMb++)
    {
        for(nHMb=0; nHMb<nMbWidth; nHMb++)
        {
            pos = nVMb*pPicture->nWidth*32+nHMb*32;
            for(y=0; y<32; y++)
            {
                yPos = (nVMb*nMbWidth+nHMb)*1024+y*32;
                uvPos = ((nVMb/2)*nMbWidth*1024)+nHMb*1024+(y/2)*32+ (((nVMb%2)==1) ? 512 : 0);
                for(x=0; x<32; x+=2)
                {
                    signed y1 = (signed)pSrcY[yPos+x+0] - 16;
                    signed y2 = (signed)pSrcY[yPos+x+1] - 16;
                    signed u  = (signed)pSrcVU[uvPos+x+0] - 128;
                    signed v  = (signed)pSrcVU[uvPos+x+1] - 128;
                    signed u_b = u * 517;
                    signed u_g = -u * 100;
                    signed v_g = -v * 208;
                    signed v_r = v * 409;
                    signed tmp1 = y1 * 298;
                    signed b1 = (tmp1 + u_b) / 256;
                    signed g1 = (tmp1 + v_g + u_g) / 256;
                    signed r1 = (tmp1 + v_r) / 256;
                    signed tmp2 = y2 * 298;
                    signed b2 = (tmp2 + u_b) / 256;
                    signed g2 = (tmp2 + v_g + u_g) / 256;
                    signed r2 = (tmp2 + v_r) / 256;
                    unsigned int rgb1 = ((pClip[r1] >> 3) << 11) |
                                        ((pClip[g1] >> 2) << 5)  |
                                        (pClip[b1] >> 3);

                    unsigned int rgb2 = ((pClip[r2] >> 3) << 11) |
                                        ((pClip[g2] >> 2) << 5)  |
                                        (pClip[b2] >> 3);
                    *(unsigned int *)(&pDst[pos]) = (rgb2 << 16) | rgb1;
                    pos += 2;
                }
                pos += (nMbWidth-1)*32;
            }
        }
    }

    logd("pos: %d", pos);
    pDst  = (unsigned short*)pData;
    for(y=0; y<pPicture->nTopOffset; y++)
    {
        memset(pDst+y*nWidth, 0, 2*nWidth);
    }
    for(y=pPicture->nBottomOffset; y<nHeight; y++)
    {
        memset(pDst+y*nWidth, 0, 2*nWidth);
    }

    for(y=pPicture->nTopOffset; y<pPicture->nBottomOffset; y++)
    {
        memset(pDst+y*nWidth, 0, 2*pPicture->nLeftOffset);
        memset(pDst+y*nWidth+pPicture->nRightOffset, 0, 2*(nWidth-pPicture->nRightOffset));
    }

#if SAVE_RGB
    FILE* outFp = fopen("/mnt/UDISK/rgb.bmp", "wb");
    if(outFp != NULL)
    {
        logd("************save_bmp_rgb565\n");
        save_bmp_rgb565(outFp, nWidth, nHeight, pData);
        fclose(outFp);
    }
#endif

    free(pClipTable);

    return 0;
}

static char * readJpegData(char *path, int *pLen)
{
    FILE *fp = NULL;
    int ret = 0;
    char *data = NULL;

    fp = fopen(path, "rb");
    if(fp == NULL)
    {
        loge("read jpeg file error, errno(%d)", errno);
        return NULL;
    }

    fseek(fp,0,SEEK_END);
    *pLen = ftell(fp);
    rewind(fp);
    data = (char *) malloc (sizeof(char)*(*pLen));

    if(data == NULL)
      {
          loge("malloc memory fail");
          fclose(fp);
          return NULL;
      }

    ret = fread (data,1,*pLen,fp);
    if (ret != *pLen)
    {
        loge("read file fail");
        fclose(fp);
        free(data);
        return NULL;
    }

    if(fp != NULL)
    {
        fclose(fp);
    }
    return data;
}

static int dumpData(char *path, uint8_t *data, int len)
{
    FILE *fp;
    fp = fopen(path, "a+");

    if(fp != NULL)
    {
        logd("dump data '%d'", len);
        fwrite(data, 1, len, fp);
        fclose(fp);
    }
    else
    {
        loge("saving picture open file error, errno(%d)", errno);
        return -1;
    }
    return 0;
}

int main(int argc, char** argv)
{
    int ret;

    VConfig vConfig;
    char *jpegData = NULL;
    int dataLen =0;
    char * uri= NULL;
    VideoDecoder *pVideo;
    VideoPicture *videoPicture = NULL;

    struct ScMemOpsS* memops = MemAdapterGetOpsS();
    if(memops == NULL)
    {
        return -1;
    }
    CdcMemOpen(memops);

    if (argc != 2)
    {
        logd("argc must be '2'");
        return 0;
    }
    uri = argv[1];

    jpegData = readJpegData(uri , &dataLen);
    if (dataLen <= 0  || jpegData == NULL)
   {
        loge("read file fail");
        return 0;
    }

    memset(&vConfig, 0x00, sizeof(VConfig));
    vConfig.bDisable3D = 0;
    vConfig.bDispErrorFrame = 0;
    vConfig.bNoBFrames = 0;
    vConfig.bRotationEn = 0;
    vConfig.bScaleDownEn = 0;
    vConfig.nHorizonScaleDownRatio = 0;
    vConfig.nVerticalScaleDownRatio = 0;
    vConfig.eOutputPixelFormat =PIXEL_FORMAT_YUV_MB32_420;
    vConfig.nDeInterlaceHoldingFrameBufferNum = 0;
    vConfig.nDisplayHoldingFrameBufferNum = 0;
    vConfig.nRotateHoldingFrameBufferNum = 0;
    vConfig.nDecodeSmoothFrameBufferNum = 0;
    vConfig.nVbvBufferSize = 2*1024*1024;
    vConfig.bThumbnailMode = 1;
    vConfig.memops = memops;
    VideoStreamInfo videoInfo;
    memset(&videoInfo, 0x00, sizeof(VideoStreamInfo));
    videoInfo.eCodecFormat = VIDEO_CODEC_FORMAT_MJPEG;

    pVideo = CreateVideoDecoder();
    if(!pVideo)
    {
        logd("create video decoder failed\n");
        return 0;
    }
    logd("create video decoder ok\n");

    if ((InitializeVideoDecoder(pVideo, &videoInfo, &vConfig)) != 0)
    {
        logd("open dev failed,  decode error !\n");
        return 0;
    }
    logd("Initialize video decoder ok\n");

    char *buf, *ringBuf;
    int buflen, ringBufLen;

    if(RequestVideoStreamBuffer(pVideo,
                                dataLen,
                                (char**)&buf,
                                &buflen,
                                (char**)&ringBuf,
                                &ringBufLen,
                                0))
    {
        logd("Request Video Stream Buffer failed\n");
        return 0;
    }
    logd("Request Video Stream Buffer ok\n");

    if(buflen + ringBufLen < dataLen)
    {
        logd("#####Error: request buffer failed, buffer is not enough\n");
        return 0;
    }

    logd("goto to copy Video Stream Data ok!\n");
    // copy stream to video decoder SBM
    if(buflen >= dataLen)
    {
        memcpy(buf,jpegData,dataLen);
    }
    else
    {
        memcpy(buf,jpegData,buflen);
        memcpy(ringBuf,jpegData+buflen,dataLen-buflen);
    }
    logd("Copy Video Stream Data ok!\n");

    VideoStreamDataInfo DataInfo;
    memset(&DataInfo, 0, sizeof(DataInfo));
    DataInfo.pData = buf;
    DataInfo.nLength = dataLen;
    DataInfo.bIsFirstPart = 1;
    DataInfo.bIsLastPart = 1;

    if (SubmitVideoStreamData(pVideo, &DataInfo, 0))
    {
        logd("#####Error: Submit Video Stream Data failed!\n");
        return 0;
    }
    logd("Submit Video Stream Data ok!\n");

    // step : decode stream now
    int endofstream = 0;
    int dropBFrameifdelay = 0;
    int64_t currenttimeus = 0;
    int decodekeyframeonly = 0;

   ret = DecodeVideoStream(pVideo, endofstream, decodekeyframeonly,
                        dropBFrameifdelay, currenttimeus);
    logd("decoder ret is %d",ret);
    switch (ret)
    {
        case VDECODE_RESULT_KEYFRAME_DECODED:
        case VDECODE_RESULT_FRAME_DECODED:
        case VDECODE_RESULT_NO_FRAME_BUFFER:
        {
            ret = ValidPictureNum(pVideo, 0);
            if (ret>= 0)
            {
                videoPicture = RequestPicture(pVideo, 0);
                if (videoPicture == NULL){
                    logd("decoder fail");
                    return 0;
                }
                logd("decoder one pic...");
                logd("pic nWidth is %d,nHeight is %d",videoPicture->nWidth,videoPicture->nHeight);

                VideoFrame jpegData;
                jpegData.mWidth = videoPicture->nWidth;
                jpegData.mHeight = videoPicture->nHeight;
                jpegData.mSize = jpegData.mWidth*jpegData.mWidth*2;
                jpegData.mData = (unsigned char*)malloc(jpegData.mSize);
                if(jpegData.mData == NULL)
                {
                    return -1;
                }

                transformPictureMb32ToRGB(memops, videoPicture, jpegData.mData,
                                            jpegData.mWidth, jpegData.mHeight);

                char path[1024] = "./pic.rgb";
                dumpData(path, (uint8_t *)jpegData.mData, jpegData.mWidth * jpegData.mHeight * 2);
                sync();
            }
            else
            {
               logd("no ValidPictureNum ret is %d",ret);
            }

            break;
        }

        case VDECODE_RESULT_OK:
        case VDECODE_RESULT_CONTINUE:
        case VDECODE_RESULT_NO_BITSTREAM:
        case VDECODE_RESULT_RESOLUTION_CHANGE:
        case VDECODE_RESULT_UNSUPPORTED:
        default:
            logd("video decode Error: %d!\n", ret);
            return 0;
    }

    if (jpegData != NULL)
    {
        free(jpegData);
        jpegData = NULL;
    }

    CdcMemClose(memops);

    return 0;
}

