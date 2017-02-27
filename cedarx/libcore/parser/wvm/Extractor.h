/*
 * Copyright 2012, The Android Open Source Project
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *     http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */

#ifndef MEDIA_EXTRACTOR_WRAPPER_H_
#define MEDIA_EXTRACTOR_WRAPPER_H_

#include <media/stagefright/foundation/ABase.h>
#include <media/stagefright/MediaSource.h>
#include <utils/Errors.h>
#include <utils/KeyedVector.h>
#include <utils/RefBase.h>
#include <utils/String8.h>
#include <utils/threads.h>
#include <utils/Vector.h>

#include "sc_interface.h"
namespace android {

struct ABuffer;
struct AMessage;
struct DataSource;
struct MediaBuffer;
struct MediaExtractor;
struct MediaSource;
struct MetaData;

class Extractor{
public:
    enum SampleFlags {
        SAMPLE_FLAG_SYNC        = 1,
        SAMPLE_FLAG_ENCRYPTED   = 2,
    };

    Extractor();

    status_t start();
    status_t pause();
    status_t stop();

    /*Set http data source with uri, deprecated*/
    status_t setDataSource(
            const char *uri,
            const KeyedVector<String8, String8> *headers = NULL,
            void *httpService = NULL);

    /*Set local data source with file descriptor, deprecated*/
    status_t setDataSource(int fd, off64_t offset, off64_t size);

    /*Set data source wrapped in datasource.*/
    status_t setDataSource(const sp<DataSource> &datasource);

    size_t countTracks() const;
    status_t getTrackFormat(size_t index, sp<AMessage> *format) const;
    sp<MetaData> getTrackFormat(size_t index)const ;

    status_t getFileFormat(sp<AMessage> *format) const;

    status_t selectTrack(size_t index);
    status_t unselectTrack(size_t index);

    status_t seekTo(
            int64_t timeUs,
            MediaSource::ReadOptions::SeekMode mode =
                MediaSource::ReadOptions::SEEK_CLOSEST_SYNC);

    status_t advance();
    status_t readSampleData(const sp<ABuffer> &buffer);
    status_t readSampleData(uint8_t *buf, size_t length,
            uint8_t *extraBuf, size_t extraLength, bool secure);

    status_t getSampleTrackIndex(size_t *trackIndex);
    status_t getSampleTime(int64_t *sampleTimeUs);
    status_t getSampleMeta(sp<MetaData> *sampleMeta);
    status_t getSampleSize(size_t *sampleSize);
    bool getCachedDuration(int64_t *durationUs, bool *eos) const;
    void getSourceSize(off64_t *size) const;
    uint32_t getSourceFlags() const;
    bool isWidevine() const;
    void setBufferCount(int32_t count);
    int32_t setBuffers(void *buf, int32_t size);
    virtual ~Extractor();

private:
    enum TrackFlags {
        kIsVorbis       = 1,
    };

    struct TrackInfo {
        sp<MediaSource> mSource;
        size_t mTrackIndex;
        status_t mFinalResult;
        MediaBuffer *mSample;
        int64_t mSampleTimeUs;

        uint32_t mTrackFlags;  // bitmask of "TrackFlags"
        int32_t mCSDIndex;
        List<sp<ABuffer> > mCSD;
    };

    mutable Mutex mLock;

    sp<DataSource> mDataSource;

    sp<MediaExtractor> mImpl;
    bool mIsWidevineExtractor;

    Vector<TrackInfo> mSelectedTracks;
    int64_t mTotalBitrate;  // in bits/sec
    int64_t mDurationUs;
    DrmManagerClient *mDrmManagerClient;
    sp<DecryptHandle> mDecryptHandle;
    int64_t mSeekTimeUs;
    uint32_t mExtractorFlags;
    int32_t mBufferCount;
    Vector<MediaBuffer *> mBuffers;
#if (CONF_ANDROID_MAJOR_VER >= 5)
    sp<IMediaHTTPService> mHTTPService;
#endif
    struct ScMemOpsS* mMemOps;
    ssize_t fetchTrackSamples(
            int64_t seekTimeUs = -1ll,
            MediaSource::ReadOptions::SeekMode mode =
                MediaSource::ReadOptions::SEEK_CLOSEST_SYNC);

    void releaseTrackSamples();

    bool getTotalBitrate(int64_t *bitRate) const;
    void updateDurationAndBitrate();
    void checkDrmStatus(const sp<DataSource>& dataSource);

    DISALLOW_EVIL_CONSTRUCTORS(Extractor);
};

}  // namespace android

#endif  // MEDIA_EXTRACTOR_WRAPPER_H_

