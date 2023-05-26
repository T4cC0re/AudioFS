#include <chromaprint.h>
#include <jansson.h>
#include <libavcodec/avcodec.h>
#include <libavformat/avformat.h>
#include <libavutil/dict.h>
#include <libavutil/samplefmt.h>
#include <sys/mman.h>
#include <sys/stat.h>

#include "custom_avio.h"
#include "macros.h"
#include "resampler.h"
#include "util.h"

// defined in transcode.c
extern audiofs_avio_handle *do_transcode(
    const char *          from_path,
    AVFormatContext *     from_context,
    const char *          to,
    const AVOutputFormat *oformat,
    const char *          format_name);

audiofs_buffer *test_buffer;

__attribute__((hot)) void *jansson_custom_malloc(size_t s) {
    void *ret = AUDIOFS_MALLOC(s);
    return ret;
}

__attribute__((hot)) void jansson_custom_free(void *p) { AUDIOFS_FREE(p); }

__attribute__((used)) void audiofs_libav_setup() {
    test_buffer = audiofs_buffer_alloc(5);
    memcpy(test_buffer->data, "test", 4);

    // make jansson use logged allocs
    json_set_alloc_funcs(jansson_custom_malloc, jansson_custom_free);
}

/**
 * detect_dsd: detect a DSD stream by context
 *
 * The passed struct is modified.
 *
 * INTERNAL
 *
 * @param ctx   pointer to decoder_context
 */
static void detect_dsd(struct decoder_context *ctx) {
    AUDIOFS_PRINTVAL(ctx, "p");

    if (!context_ok(ctx) || !ctx->avCodecContext) { return; }

    uint32_t bitrate_per_channel = ctx->sample_rate * ((uint32_t)ctx->depth) / MAX(ctx->channel_layout.nb_channels, 1);
    switch (ctx->avCodecContext->codec_id) {
        case AV_CODEC_ID_DSD_LSBF_PLANAR:
        case AV_CODEC_ID_DSD_MSBF_PLANAR:
            bitrate_per_channel /= 2;
            // fallthrough
        case AV_CODEC_ID_DSD_LSBF:
        case AV_CODEC_ID_DSD_MSBF:
            if (bitrate_per_channel % 48000 == 0) {
                ctx->dsd = (u_int16_t)(bitrate_per_channel / 48000);
            } else {
                ctx->dsd = (u_int16_t)(bitrate_per_channel / 44100);
            }

            if (ctx->dsd % 64 != 0 || ctx->dsd < 64) { ctx->dsd = 0; }

            break;
        default:
            ctx->dsd = 0;
    }
}

/**
 * fix_depth: fixes sample depth of a context
 *
 * The passed struct is modified.
 *
 * INTERNAL
 *
 * @param ctx   pointer to decoder_context
 */
static void fix_depth(struct decoder_context *ctx) {
    AUDIOFS_PRINTVAL(ctx, "p");

    if (!context_ok(ctx) || !ctx->avCodec) { return; }

    switch (ctx->avCodec->id) {
        case AV_CODEC_ID_PCM_S24LE:
        case AV_CODEC_ID_PCM_S24BE:
        case AV_CODEC_ID_PCM_S24LE_PLANAR:
            // There is no AV_CODEC_ID_PCM_S24BE_PLANAR in FFMPEG
            ctx->depth               = 24;
            ctx->override_sample_fmt = AV_SAMPLE_FMT_S32;
            break;
        default:
            if (ctx->depth == 0) { ctx->depth = sample_bits_by_format(ctx->sample_fmt, 0); }
            break;
    }
}

/**
 * change_target_format: Change the target format to be resampled to (if
 * required).
 *
 * @param ctx           pointer to decoder_context
 * @param target_fmt    force a target format (-1/AV_SAMPLE_FMT_NONE to keep
 * existing). NOTE: planar audio will always be changed no packed
 * @param target_depth  force a sample depth (0 to keep existing)
 * @param target_rate   force a rate (0 to keep existing)
 * @param forceStereo   force stereo (false to keep channel layout)
 *
 * @return int          negative on error
 */
int change_target_format(
    struct decoder_context *ctx,
    enum AVSampleFormat     target_fmt,
    u_int8_t                target_depth,
    int32_t                 target_rate,
    bool                    forceStereo) {
    if (!context_ok(ctx)) { return -1; }

    AUDIOFS_PRINTVAL(ctx, "p");
    AUDIOFS_PRINTVAL(target_fmt, "d");
    AUDIOFS_PRINTVAL(target_depth, "d");
    AUDIOFS_PRINTVAL(target_rate, "d");
    AUDIOFS_PRINTVAL(forceStereo, "d");
    //    AUDIOFS_PRINTVAL(ctx->channel_layout, PRIu64);
    AUDIOFS_PRINTVAL(ctx->depth, "d");
    AUDIOFS_PRINTVAL(ctx->sample_fmt, "d");
    AUDIOFS_PRINTVAL(ctx->sample_rate, "d");
    AUDIOFS_PRINTVAL(ctx->swrContext, "p");

    ctx->override_channel_layout = ctx->channel_layout;
    ctx->override_channels       = ctx->channel_layout.nb_channels;
    ctx->override_depth          = ctx->depth;
    ctx->override_sample_fmt     = ctx->sample_fmt;
    ctx->override_sample_rate    = ctx->sample_rate;

    if (av_sample_fmt_is_planar(ctx->override_sample_fmt)) {
        ctx->override_sample_fmt = av_get_alt_sample_fmt(ctx->override_sample_fmt, false);
    }

    if (target_fmt != AV_SAMPLE_FMT_NONE && av_sample_fmt_is_planar(target_fmt)) {
        target_fmt = av_get_alt_sample_fmt(target_fmt, false);
    }

    // This will override the target_fmt. If the input format or target_depth
    // was floating point (and the depth allows) the target_fmt will be floating
    // point again
    if (target_depth > 0) {
        ctx->override_depth = target_depth;
        switch (ctx->override_depth) {
            case 64:
                target_fmt = AV_SAMPLE_FMT_DBL;
                break;
            case 32:
                if (target_fmt == AV_SAMPLE_FMT_DBL || ctx->override_sample_fmt == AV_SAMPLE_FMT_DBL
                    || target_fmt == AV_SAMPLE_FMT_FLT || ctx->override_sample_fmt == AV_SAMPLE_FMT_FLT) {
                    // If the input was supposed to be floating point, make this
                    // flt32
                    target_fmt = AV_SAMPLE_FMT_FLT;
                } else {
                    // Otherwise s32, please
                    target_fmt = AV_SAMPLE_FMT_S32;
                }
                break;
            case 24:
                // Despite only 24-bits being used, libav will handle it in
                // 32-bit with the upper 8 bits == 0x00
                target_fmt = AV_SAMPLE_FMT_S32;
                break;
            case 16:
            default:
                target_fmt = AV_SAMPLE_FMT_S16;
                break;
            case 8:
                target_fmt = AV_SAMPLE_FMT_U8;
                break;
        }
    }

    if (target_rate > 0) {
        ctx->override_sample_rate = target_rate;
    } else {
        ctx->override_sample_rate = ctx->sample_rate;
    }

    if (forceStereo && ctx->channel_layout.nb_channels != 2) {
        ctx->override_channels       = 2;
        ctx->override_channel_layout = (AVChannelLayout)AV_CHANNEL_LAYOUT_STEREO_DOWNMIX;
    }

    if (target_fmt != AV_SAMPLE_FMT_NONE) {
        // Do not convert formats, but do change the fmt to non-planar in every
        // case.
        ctx->override_sample_fmt = av_get_alt_sample_fmt(target_fmt, false);
    }

    ctx->override_depth = sample_bits_by_format(ctx->override_sample_fmt, ctx->override_depth);

    if (0 == av_channel_layout_compare(&ctx->override_channel_layout, &ctx->channel_layout)
        || ctx->override_channels != ctx->channel_layout.nb_channels || ctx->override_depth != ctx->depth
        || ctx->override_sample_fmt != ctx->sample_fmt || ctx->override_sample_rate != ctx->sample_rate) {
        if (ctx->swrContext == NULL) {
            ctx->swrContext = swr_alloc();
            if (ctx->swrContext == NULL) {
                errorf("Unable to allocate swr context\n");
                return -1;
            }
        }
    } else {
        if (ctx->swrContext != NULL) { swr_free(&ctx->swrContext); }
    }

    AUDIOFS_PRINTVAL(ctx->override_channels, "d");
    //    AUDIOFS_PRINTVAL(ctx->channel_layout, PRIu64);
    AUDIOFS_PRINTVAL(ctx->override_depth, "d");
    AUDIOFS_PRINTVAL(ctx->override_sample_fmt, "d");
    AUDIOFS_PRINTVAL(ctx->override_sample_rate, "d");
    AUDIOFS_PRINTVAL(ctx->swrContext, "p");
    AUDIOFS_PRINTVAL(ctx->swrContext != NULL, "d");

    return 0;
}

__attribute__((used)) __attribute__((hot)) __attribute__((warn_unused_result)) char *
get_metadate_from_file(char *path) {
    // region variables
    infof("getting metadata from '%s'", path);
    AVFormatContext *  fmt_ctx = avformat_alloc_context();
    AVDictionaryEntry *tag     = NULL;
    int                ret;
    json_t *           json                  = json_object();
    json_t *           json_file             = json_object();
    json_t *           json_file_format      = json_object();
    json_t *           json_file_metadata    = json_object();
    json_t *           json_streams_array    = json_array();
    json_t **          json_streams          = AUDIOFS_CALLOC(fmt_ctx->nb_streams, sizeof(json_t *));
    json_t **          json_streams_metadata = AUDIOFS_CALLOC(fmt_ctx->nb_streams, sizeof(json_t *));
    json_t **          json_streams_codec    = AUDIOFS_CALLOC(fmt_ctx->nb_streams, sizeof(json_t *));
    // endregion variables

    // Everything is now allocated. Let's open the file!
    ret = avformat_open_input(&fmt_ctx, path, NULL, NULL);
    if (ret < 0) { return 0; }

    // Retrieve the stream information
    ret = avformat_find_stream_info(fmt_ctx, NULL);
    if (ret < 0) {
        avformat_close_input(&fmt_ctx);
        return 0;
    }

    // region JSON setup
    json_object_set_new(json, "file", json_file);
    json_object_set_new(json_file, "metadata", json_file_metadata);
    json_object_set_new(json_file, "format", json_file_format);
    json_object_set(json, "streams", json_streams_array);
    for (unsigned int i = 0; i < fmt_ctx->nb_streams; ++i) {
        // Add entries to json array, so we can easily manipulate them later.
        json_streams[i]          = json_object();
        json_streams_metadata[i] = json_object();
        json_streams_codec[i]    = json_object();
        json_object_set(json_streams[i], "metadata", json_streams_metadata[i]);
        json_object_set(json_streams[i], "codec", json_streams_codec[i]);
        json_array_append(json_streams_array, json_streams[i]);
    }
    // endregion JSON setup

    // region json_file_format
    json_object_set_new(json_file_format, "name", json_string(fmt_ctx->iformat->name));
    json_object_set_new(json_file_format, "long_name", json_string(fmt_ctx->iformat->long_name));
    json_object_set_new(json_file_format, "mime_type", json_string(fmt_ctx->iformat->mime_type));
    json_object_set_new(json_file_format, "flags", json_integer(fmt_ctx->iformat->flags));
    // endregion json_file_format

    // region json_file
    json_object_set_new(json_file, "start_time", json_integer(fmt_ctx->start_time));
    json_object_set_new(json_file, "duration", json_integer(fmt_ctx->duration));
    json_object_set_new(json_file, "bit_rate", json_integer(fmt_ctx->bit_rate));
    // endregion json_file

    AUDIOFS_PRINTVAL(fmt_ctx, "p");
    AUDIOFS_PRINTVAL(fmt_ctx->metadata, "p");
    while ((tag = av_dict_get(fmt_ctx->metadata, "", tag, AV_DICT_IGNORE_SUFFIX))) {
        json_object_set_new(json_file_metadata, tag->key, json_string(tag->value));
    }

    // Stream metadata:
    for (unsigned int i = 0; i < fmt_ctx->nb_streams; ++i) {
        AVStream *stream = fmt_ctx->streams[i];
        tag              = NULL;

        AUDIOFS_PRINTVAL(fmt_ctx->streams[i]->codecpar, "p");
        if (fmt_ctx->streams[i] == NULL || fmt_ctx->streams[i]->codecpar == NULL) { continue; }
        if (fmt_ctx->streams[i]->codecpar->codec_type == AVMEDIA_TYPE_AUDIO) {
            // TODO: Make do_transcode handle a passed stream ID.
            /// Hook into transcoding pipeline
            audiofs_avio_handle *avio_handle = do_transcode(NULL, fmt_ctx, "memory", NULL, "chromaprint");
            int                  handle      = audiofs_avio_get_handle(avio_handle);
            struct stat          s;
            int                  status;
            char *               mapped;
            status = fstat(handle, &s); // TODO: Check return

            /* Memory-map the file. */
            mapped = mmap(0, s.st_size + 1, PROT_READ | PROT_WRITE, MAP_PRIVATE, handle, 0); // TODO check pointer
            mapped[s.st_size] = '\0';

            infof("Chromaprint: %s\n", mapped);

            json_object_set_new(json_streams[i], "chromaprint", json_string(mapped));

            // Close the AVIO buffer:
            munmap(mapped, s.st_size + 1);
            audiofs_avio_close(&avio_handle);
        }

        json_object_set_new(json_streams[i], "index", json_integer(fmt_ctx->streams[i]->index));
        json_object_set_new(json_streams[i], "nb_frames", json_integer(fmt_ctx->streams[i]->nb_frames));
        json_object_set_new(json_streams[i], "duration", json_integer(fmt_ctx->streams[i]->duration));
        json_object_set_new(json_streams[i], "time_base_num", json_integer(fmt_ctx->streams[i]->time_base.num));
        json_object_set_new(json_streams[i], "time_base_den", json_integer(fmt_ctx->streams[i]->time_base.den));

        char *layout = AUDIOFS_CALLOC(256, sizeof(char));
        av_channel_layout_describe(&fmt_ctx->streams[i]->codecpar->ch_layout, layout, 255);
        json_object_set_new(json_streams_codec[i], "ch_layout", json_string(layout));
        AUDIOFS_FREE(layout);
        json_object_set_new(
            json_streams_codec[i],
            "type",
            json_string(av_get_media_type_string(fmt_ctx->streams[i]->codecpar->codec_type)));
        json_object_set_new(
            json_streams_codec[i],
            "name",
            json_string(avcodec_get_name(fmt_ctx->streams[i]->codecpar->codec_id)));
        json_object_set_new(json_streams_codec[i], "codec_tag", json_integer(fmt_ctx->streams[i]->codecpar->codec_tag));
        json_object_set_new(json_streams_codec[i], "bit_rate", json_integer(fmt_ctx->streams[i]->codecpar->bit_rate));
        json_object_set_new(
            json_streams_codec[i],
            "bits_per_coded_sample",
            json_integer(fmt_ctx->streams[i]->codecpar->bits_per_coded_sample));
        json_object_set_new(
            json_streams_codec[i],
            "bits_per_raw_sample",
            json_integer(fmt_ctx->streams[i]->codecpar->bits_per_raw_sample));
        json_object_set_new(json_streams_codec[i], "profile", json_integer(fmt_ctx->streams[i]->codecpar->profile));
        json_object_set_new(json_streams_codec[i], "level", json_integer(fmt_ctx->streams[i]->codecpar->level));
        json_object_set_new(json_streams_codec[i], "width", json_integer(fmt_ctx->streams[i]->codecpar->width));
        json_object_set_new(json_streams_codec[i], "height", json_integer(fmt_ctx->streams[i]->codecpar->height));
        json_object_set_new(json_streams_codec[i], "profile", json_integer(fmt_ctx->streams[i]->codecpar->profile));
        json_object_set_new(
            json_streams_codec[i],
            "bits_per_sample",
            json_integer(av_get_exact_bits_per_sample(fmt_ctx->streams[i]->codecpar->codec_id)));
        json_object_set_new(
            json_streams_codec[i],
            "profile_name",
            json_string(
                avcodec_profile_name(fmt_ctx->streams[i]->codecpar->codec_id, fmt_ctx->streams[i]->codecpar->profile)));
        json_object_set_new(
            json_streams_codec[i],
            "sample_rate",
            json_integer(fmt_ctx->streams[i]->codecpar->sample_rate));
        json_object_set_new(
            json_streams_codec[i],
            "frame_size",
            json_integer(fmt_ctx->streams[i]->codecpar->frame_size));
        json_object_set_new(
            json_streams_codec[i],
            "block_align",
            json_integer(fmt_ctx->streams[i]->codecpar->block_align));
        json_object_set_new(
            json_streams_codec[i],
            "nb_channels",
            json_integer(fmt_ctx->streams[i]->codecpar->ch_layout.nb_channels));

        // TODO fmt_ctx->streams[i]->side_data
        // TODO AVPacket 	attached_pic

        // Retrieve the stream metadata
        while ((tag = av_dict_get(stream->metadata, "", tag, AV_DICT_IGNORE_SUFFIX))) {
            // Add the metadata to the JSON object
            json_object_set_new(json_streams_metadata[i], tag->key, json_string(tag->value));
        }
    }

    // TODO: Dump to audiofs_buffer and return pointer to THAT.
    char *json_str = json_dumps(json, JSON_COMPACT | JSON_SORT_KEYS);

    // Free the JSON objects
    for (unsigned int i = 0; i < fmt_ctx->nb_streams; ++i) {
        json_decref(json_streams[i]);
        json_decref(json_streams_metadata[i]);
        json_decref(json_streams_codec[i]);
    }
    json_decref(json_file_metadata);
    json_decref(json_file_format);
    json_decref(json_file);
    json_decref(json);

    AUDIOFS_FREE(json_streams);
    AUDIOFS_FREE(json_streams_metadata);
    AUDIOFS_FREE(json_streams_codec);

    // Close the input file
    avformat_close_input(&fmt_ctx);

    return json_str;
}
