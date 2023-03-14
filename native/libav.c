#include <jansson.h>
#include <libavcodec/avcodec.h>
#include <libavformat/avformat.h>
#include <libavutil/dict.h>

#include "macros.h"

void *jansson_custom_malloc(size_t s) {
    void *ret = AUDIOFS_MALLOC(s);
    return ret;
}

void jansson_custom_free(void *p) { AUDIOFS_FREE(p); }

void audiofs_libav_setup() {
    // make jansson use logged allocs
    json_set_alloc_funcs(jansson_custom_malloc, jansson_custom_free);
}

char *get_metadate_from_file(char *path) {
    // region variables
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
        return 0;
        avformat_close_input(&fmt_ctx);
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

    if (fmt_ctx->metadata
        && !(av_dict_count(fmt_ctx->metadata) == 1 && av_dict_get(fmt_ctx->metadata, "language", NULL, 0))) {
        while ((tag = av_dict_get(fmt_ctx->metadata, "", tag, AV_DICT_IGNORE_SUFFIX))) {
            json_object_set_new(json_file_metadata, tag->key, json_string(tag->value));
        }
        // Stream metadata:
        for (unsigned int i = 0; i < fmt_ctx->nb_streams; ++i) {
            AVStream *stream = fmt_ctx->streams[i];
            tag              = NULL;

            if (fmt_ctx->streams[i] == NULL || fmt_ctx->streams[i]->codecpar == NULL) { continue; }

            json_object_set_new(json_streams[i], "index", json_integer(fmt_ctx->streams[i]->index));
            json_object_set_new(json_streams[i], "nb_frames", json_integer(fmt_ctx->streams[i]->nb_frames));
            json_object_set_new(json_streams[i], "duration", json_integer(fmt_ctx->streams[i]->duration));
            json_object_set_new(json_streams[i], "time_base_num", json_integer(fmt_ctx->streams[i]->time_base.num));
            json_object_set_new(json_streams[i], "time_base_den", json_integer(fmt_ctx->streams[i]->time_base.den));

            char *layout = malloc(sizeof(char) * 256);
            av_channel_layout_describe(&fmt_ctx->streams[i]->codecpar->ch_layout, layout, 255);
            json_object_set_new(json_streams_codec[i], "ch_layout", json_string(layout));
            json_object_set_new(
                json_streams_codec[i],
                "type",
                json_string(av_get_media_type_string(fmt_ctx->streams[i]->codecpar->codec_type)));
            json_object_set_new(
                json_streams_codec[i],
                "name",
                json_string(avcodec_get_name(fmt_ctx->streams[i]->codecpar->codec_id)));
            json_object_set_new(
                json_streams_codec[i],
                "codec_tag",
                json_integer(fmt_ctx->streams[i]->codecpar->codec_tag));
            json_object_set_new(
                json_streams_codec[i],
                "bit_rate",
                json_integer(fmt_ctx->streams[i]->codecpar->bit_rate));
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
                json_string(avcodec_profile_name(
                    fmt_ctx->streams[i]->codecpar->codec_id,
                    fmt_ctx->streams[i]->codecpar->profile)));
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

    // Close the input file
    avformat_close_input(&fmt_ctx);

    return json_str;
}
