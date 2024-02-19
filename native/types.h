
#ifndef NATIVE_TYPES_H
#define NATIVE_TYPES_H

#include <libavcodec/avcodec.h>
#include <libavformat/avformat.h>
#include <libavutil/avutil.h>
#include <libavutil/opt.h>
#include <libswresample/swresample.h>
#include <pthread.h>
#include <stdint.h>
#include <sys/types.h>

#define _AUDIOFS_CONTEXT_MAGIC_A 0xd0d0cafe
#define _AUDIOFS_CONTEXT_MAGIC_B 0xdeadbeef

typedef struct audiofs_buffer {
    uint32_t        cookie;
    void           *data;
    uint64_t        len;
    pthread_mutex_t lock;
    void           *self;
    pthread_mutex_t used_outside_audiofs; // Lock this mutex when used outside of C code
} audiofs_buffer;

typedef struct decoder_context {
    int                 cookieA;
    AVFormatContext    *avFormatContext;
    AVStream           *avStream;
    AVCodecContext     *avCodecContext;
    AVCodec            *avCodec;
    SwrContext         *swrContext;
    AVDictionary       *avDictionary;
    u_int16_t           dsd;
    enum AVSampleFormat sample_fmt;
    int32_t             sample_rate;
    u_int8_t            depth;
    // int32_t             channels; // Replaced via channel_layout.nb_channels
    AVChannelLayout     channel_layout;
    enum AVSampleFormat override_sample_fmt;
    int32_t             override_sample_rate;
    u_int8_t            override_depth;
    int32_t             override_channels;
    AVChannelLayout     override_channel_layout;
    enum SwrDitherType  dither;
    enum SwrEngine      resampler;
    //    struct mqa_context *mqa_context;
    int cookieB;
} decoder_context;

#endif // NATIVE_TYPES_H
