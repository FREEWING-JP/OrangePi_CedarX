/*
 * Copyright (c) 2008-2016 Allwinner Technology Co. Ltd.
 * All rights reserved.
 *
 * File : CdxAtomic.h
 * Description : Atomic
 * History :
 *
 */

#ifndef CDX_ATOMIC_H
#define CDX_ATOMIC_H

#include <CdxTypes.h>

#ifdef __cplusplus
extern "C" {
#endif

typedef struct {
    volatile cdx_ssize counter;
} cdx_atomic_t;

/*���ü���+ 1 �����ؽ������ֵ*/
static inline cdx_int32 CdxAtomicInc(cdx_atomic_t *ref)
{
    return __sync_add_and_fetch(&ref->counter, 1L);
}

static inline cdx_int32 CdxAtomicAdd(cdx_atomic_t *ref, cdx_ssize val)
{
    return __sync_add_and_fetch(&ref->counter, val);
}

/*���ü���- 1 �����ؽ������ֵ*/
static inline cdx_int32 CdxAtomicDec(cdx_atomic_t *ref)
{
    return __sync_sub_and_fetch(&ref->counter, 1L);
}

static inline cdx_int32 CdxAtomicSub(cdx_atomic_t *ref, cdx_ssize val)
{
    return __sync_sub_and_fetch(&ref->counter, val);
}

/*�������ü���Ϊval����������ǰ��ֵ*/
static inline cdx_int32 CdxAtomicSet(cdx_atomic_t *ref, cdx_ssize val)
{
    return __sync_lock_test_and_set(&ref->counter, val);
}

/*��ȡ���ü�����ֵ*/
static inline cdx_int32 CdxAtomicRead(cdx_atomic_t *ref)
{
    return __sync_or_and_fetch(&ref->counter, 0L);
}

static inline cdx_bool CdxAtomicCAS(cdx_atomic_t *ref, cdx_ssize oldVal, cdx_ssize newVal)
{
    return __sync_bool_compare_and_swap(&ref->counter, oldVal, newVal);
}

#ifdef __cplusplus
}
#endif

#endif
