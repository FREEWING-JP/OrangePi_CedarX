/*
 * Copyright (c) 2008-2016 Allwinner Technology Co. Ltd.
 * All rights reserved.
 *
 * File : CdxStreamErrno.h
 * Description : StreamErrno
 * History :
 *
 */

#ifndef CDX_STREAM_ERRNO_H
#define CDX_STREAM_ERRNO_H

enum CdxStreamErrno
{
    CEDARX_STREAM_ERR_BASE = 0x04670000,
        /*��㶨��ģ�����ÿ��ģ��Ӧ��ͳһ*/
    CEDARX_STREAM_ERR_REGISTER,
    CEDARX_STREAM_ERR_NOT_REGISTER,
    CEDARX_STREAM_ERR_CONN_TIMEOUT,
    CEDARX_STREAM_ERR_PARAMETER_INVAILD,
};

#endif
