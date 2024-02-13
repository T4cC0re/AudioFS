/*
 * Copyright (c) 2010 Nicolas George
 * Copyright (c) 2011 Stefano Sabatini
 * Copyright (c) 2014 Andrey Utkin
 * Copyright (c) 2023 Hendrik 'T4cC0re' Meyer
 *
 * Permission is hereby granted, free of charge, to any person obtaining a copy
 * of this software and associated documentation files (the "Software"), to deal
 * in the Software without restriction, including without limitation the rights
 * to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
 * copies of the Software, and to permit persons to whom the Software is
 * furnished to do so, subject to the following conditions:
 *
 * The above copyright notice and this permission notice shall be included in
 * all copies or substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL
 * THE AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
 * LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
 * OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN
 * THE SOFTWARE.
 */

/**
 * This file is based on the `transcode.c` example from FFmpeg, modified for use in AudioFS.
 */

#include "custom_avio.h"
#include "util.h"
#include <libavcodec/avcodec.h>
#include <libavfilter/buffersink.h>
#include <libavfilter/buffersrc.h>
#include <libavformat/avformat.h>
#include <libavformat/avio.h>
#include <libavutil/channel_layout.h>
#include <libavutil/mem.h>
#include <libavutil/opt.h>
#include <libavutil/pixdesc.h>

static AVFormatContext *ifmt_ctx;
static AVFormatContext *ofmt_ctx;
typedef struct FilteringContext {
    AVFilterContext *buffersink_ctx;
    AVFilterContext *buffersrc_ctx;
    AVFilterGraph *  filter_graph;

    AVPacket *enc_pkt;
    AVFrame * filtered_frame;
} FilteringContext;
static FilteringContext *filter_ctx;

typedef struct StreamContext {
    AVCodecContext *dec_ctx;
    AVCodecContext *enc_ctx;

    AVFrame *dec_frame;
} StreamContext;
static StreamContext *stream_ctx;

static int open_input_file_with_format_context(AVFormatContext *ctx) {
    int          ret;
    unsigned int i;

    if (ctx == NULL) {
        errorf("No FormatContext provided");
        return -1; // TODO Better code
    }
    ifmt_ctx = ctx;

    stream_ctx = av_malloc(sizeof(*stream_ctx));
    if (!stream_ctx) { return AVERROR(ENOMEM); }

    for (i = 0; i < ifmt_ctx->nb_streams; i++) {
        AVStream *stream = ifmt_ctx->streams[i];
        if (stream->codecpar->codec_type != AVMEDIA_TYPE_AUDIO) { continue; }
        const AVCodec * dec = avcodec_find_decoder(stream->codecpar->codec_id);
        AVCodecContext *codec_ctx;
        if (!dec) {
            errorf("Failed to find decoder for stream #%u\n", i);
            return AVERROR_DECODER_NOT_FOUND;
        }
        codec_ctx = avcodec_alloc_context3(dec);
        if (!codec_ctx) {
            errorf("Failed to allocate the decoder context for stream #%u\n", i);
            return AVERROR(ENOMEM);
        }
        ret = avcodec_parameters_to_context(codec_ctx, stream->codecpar);
        if (ret < 0) {
            errorf(
                "Failed to copy decoder parameters to input decoder context "
                "for stream #%u\n",
                i);
            return ret;
        }
        /* Reencode video & audio and remux subtitles etc. */
        if (codec_ctx->codec_type == AVMEDIA_TYPE_VIDEO || codec_ctx->codec_type == AVMEDIA_TYPE_AUDIO) {
            if (codec_ctx->codec_type == AVMEDIA_TYPE_VIDEO) {
                codec_ctx->framerate = av_guess_frame_rate(ifmt_ctx, stream, NULL);
            }
            /* Open decoder */
            ret = avcodec_open2(codec_ctx, dec, NULL);
            if (ret < 0) {
                errorf("Failed to open decoder for stream #%u\n", i);
                return ret;
            }
        }
        stream_ctx->dec_ctx = codec_ctx;

        stream_ctx->dec_frame = av_frame_alloc();
        if (!stream_ctx->dec_frame) { return AVERROR(ENOMEM); }
    }

    av_dump_format(ifmt_ctx, 0, "input", 0);
    return 0;
}

/**
 * @deprecated
 * @param filename
 * @return
 */
__attribute__((deprecated)) static int open_input_file(const char *filename) {
    int ret;
    ifmt_ctx = NULL;
    if ((ret = avformat_open_input(&ifmt_ctx, filename, NULL, NULL)) < 0) {
        errorf("Cannot open input file\n");
        return ret;
    }

    if ((ret = avformat_find_stream_info(ifmt_ctx, NULL)) < 0) {
        errorf("Cannot find stream information\n");
        return ret;
    }
    ret = open_input_file_with_format_context(ifmt_ctx);
    return ret;
}

/**
 * Allocate an AVFormatContext for an output format.
 * avformat_free_context() can be used to free the context and
 * everything allocated by the framework within it.
 *
 * @param ctx           pointee is set to the created format context,
 *                      or to NULL in case of failure
 * @param oformat       format to use for allocating the context, if NULL
 *                      format_name and filename are used instead
 * @param format_name   the name of output format to use for allocating the
 *                      context, if NULL filename is used instead
 * @param filename      the name of the filename to use for allocating the
 *                      context, may be NULL
 *
 * @return  >= 0 in case of success, a negative AVERROR code in case of
 *          failure
 */
static int open_output_file(const AVOutputFormat *oformat, const char *format_name, const char *filename) {
    AVStream *      out_stream = NULL;
    AVStream *      in_stream = NULL;
    AVCodecContext *dec_ctx = NULL, *enc_ctx = NULL;
    const AVCodec * encoder = NULL;
    int             ret = 0;
    unsigned int    i = 0;

    ofmt_ctx = NULL;
    avformat_alloc_output_context2(&ofmt_ctx, oformat, format_name, filename);
    if (!ofmt_ctx) {
        errorf("Could not create output context\n");
        return AVERROR_UNKNOWN;
    }

    for (i = 0; i < ifmt_ctx->nb_streams; i++) {
        in_stream = ifmt_ctx->streams[i];
        if (in_stream->codecpar->codec_type == AVMEDIA_TYPE_AUDIO) {
            out_stream = avformat_new_stream(ofmt_ctx, NULL);
            if (!out_stream) {
                errorf("Failed allocating output stream\n");
                return AVERROR_UNKNOWN;
            }
        } else {
            continue;
        }

        dec_ctx = stream_ctx->dec_ctx;

        if (dec_ctx->codec_type == AVMEDIA_TYPE_AUDIO) {
            encoder = avcodec_find_encoder_by_name("pcm_s16be");
            if (!encoder) {
                fatalf("Necessary encoder not found\n");
                return AVERROR_INVALIDDATA;
            }
            enc_ctx = avcodec_alloc_context3(encoder);
            if (!enc_ctx) {
                fatalf("Failed to allocate the encoder context\n");
                return AVERROR(ENOMEM);
            }

            /* In this example, we transcode to same properties (picture size,
             * sample rate etc.). These properties can be changed for output
             * streams easily using filters */
            if (dec_ctx->codec_type == AVMEDIA_TYPE_AUDIO) {
                //            if (dec_ctx->codec_type == AVMEDIA_TYPE_VIDEO) {
                //                enc_ctx->height = dec_ctx->height;
                //                enc_ctx->width = dec_ctx->width;
                //                enc_ctx->sample_aspect_ratio = dec_ctx->sample_aspect_ratio;
                //                /* take first format from list of supported formats */
                //                if (encoder->pix_fmts)
                //                    enc_ctx->pix_fmt = encoder->pix_fmts[0];
                //                else
                //                    enc_ctx->pix_fmt = dec_ctx->pix_fmt;
                //                /* video time_base can be set to whatever is handy and supported by encoder */
                //                enc_ctx->time_base = av_inv_q(dec_ctx->framerate);
                //            } else {
                enc_ctx->sample_rate = 44100;

                ret = av_channel_layout_copy(&enc_ctx->ch_layout, &dec_ctx->ch_layout);
                if (ret < 0) { return ret; }
                /* take first format from list of supported formats */
                enc_ctx->sample_fmt = encoder->sample_fmts[0];
                enc_ctx->time_base  = (AVRational){1, enc_ctx->sample_rate};
            }

            if (ofmt_ctx->oformat->flags & AVFMT_GLOBALHEADER) { enc_ctx->flags |= AV_CODEC_FLAG_GLOBAL_HEADER; }

            /* Third parameter can be used to pass settings to encoder */
            ret = avcodec_open2(enc_ctx, encoder, NULL);
            if (ret < 0) {
                errorf("Cannot open video encoder for stream #%u\n", i);
                return ret;
            }
            ret = avcodec_parameters_from_context(out_stream->codecpar, enc_ctx);
            if (ret < 0) {
                errorf("Failed to copy encoder parameters to output stream #%u\n", i);
                return ret;
            }

            out_stream->time_base = enc_ctx->time_base;
            stream_ctx->enc_ctx   = enc_ctx;
        } else if (dec_ctx->codec_type == AVMEDIA_TYPE_UNKNOWN) {
            fatalf("Elementary stream #%d is of unknown type, cannot proceed\n", i);
            return AVERROR_INVALIDDATA;
        } else {
            //            /* if this stream must be remuxed */
            //            ret = avcodec_parameters_copy(out_stream->codecpar, in_stream->codecpar);
            //            if (ret < 0) {
            //                errorf( "Copying parameters for stream #%u failed\n", i);
            //                return ret;
            //            }
            //            out_stream->time_base = in_stream->time_base;
        }
    }
    av_dump_format(ofmt_ctx, 0, filename, 1);

    if (!(ofmt_ctx->oformat->flags & AVFMT_NOFILE)) {
        void *handle = audiofs_avio_open(filename);
        if (handle == NULL) {
            errorf("Could not open output file");
            return -1;
        }
        unsigned char *buffer = av_malloc(4096);
        AVIOContext *  avio_ctx
            = avio_alloc_context(buffer, 4096, 1, handle, &audiofs_avio_read, &audiofs_avio_write, &audiofs_avio_seek);
        ofmt_ctx->pb = avio_ctx;
        if (ret < 0) {
            errorf("Could not open output file '%s'", filename);
            return ret;
        }
    }

    /* init muxer, write output file header */
    ret = avformat_write_header(ofmt_ctx, NULL);
    if (ret < 0) {
        errorf("Error occurred when opening output file\n");
        return ret;
    }

    return 0;
}

static int
init_filter(FilteringContext *fctx, AVCodecContext *dec_ctx, AVCodecContext *enc_ctx, const char *filter_spec) {
    char             args[512];
    int              ret            = 0;
    const AVFilter * buffersrc      = NULL;
    const AVFilter * buffersink     = NULL;
    AVFilterContext *buffersrc_ctx  = NULL;
    AVFilterContext *buffersink_ctx = NULL;
    AVFilterInOut *  outputs        = avfilter_inout_alloc();
    AVFilterInOut *  inputs         = avfilter_inout_alloc();
    AVFilterGraph *  filter_graph   = avfilter_graph_alloc();

    if (!outputs || !inputs || !filter_graph) {
        ret = AVERROR(ENOMEM);
        goto end;
    }

    //    if (dec_ctx->codec_type == AVMEDIA_TYPE_VIDEO) {
    //        buffersrc = avfilter_get_by_name("buffer");
    //        buffersink = avfilter_get_by_name("buffersink");
    //        if (!buffersrc || !buffersink) {
    //            errorf( "filtering source or sink element not found\n");
    //            ret = AVERROR_UNKNOWN;
    //            goto end;
    //        }
    //
    //        snprintf(args, sizeof(args),
    //                 "video_size=%dx%d:pix_fmt=%d:time_base=%d/%d:pixel_aspect=%d/%d",
    //                 dec_ctx->width, dec_ctx->height, dec_ctx->pix_fmt,
    //                 dec_ctx->time_base.num, dec_ctx->time_base.den,
    //                 dec_ctx->sample_aspect_ratio.num,
    //                 dec_ctx->sample_aspect_ratio.den);
    //
    //        ret = avfilter_graph_create_filter(&buffersrc_ctx, buffersrc, "in",
    //                                           args, NULL, filter_graph);
    //        if (ret < 0) {
    //            errorf( "Cannot create buffer source\n");
    //            goto end;
    //        }
    //
    //        ret = avfilter_graph_create_filter(&buffersink_ctx, buffersink, "out",
    //                                           NULL, NULL, filter_graph);
    //        if (ret < 0) {
    //            errorf( "Cannot create buffer sink\n");
    //            goto end;
    //        }
    //
    //        ret = av_opt_set_bin(buffersink_ctx, "pix_fmts",
    //                             (uint8_t*)&enc_ctx->pix_fmt, sizeof(enc_ctx->pix_fmt),
    //                             AV_OPT_SEARCH_CHILDREN);
    //        if (ret < 0) {
    //            errorf( "Cannot set output pixel format\n");
    //            goto end;
    //        }
    //    } else
    if (dec_ctx->codec_type == AVMEDIA_TYPE_AUDIO) {
        char buf[64];
        buffersrc  = avfilter_get_by_name("abuffer");
        buffersink = avfilter_get_by_name("abuffersink");
        if (!buffersrc || !buffersink) {
            errorf("filtering source or sink element not found\n");
            ret = AVERROR_UNKNOWN;
            goto end;
        }
        av_channel_layout_describe(&enc_ctx->ch_layout, buf, sizeof(buf));
        snprintf(
            args,
            sizeof(args),
            "time_base=%d/%d:sample_rate=%d:sample_fmt=%s:channel_layout=%s",
            dec_ctx->time_base.num,
            dec_ctx->time_base.den,
            dec_ctx->sample_rate,
            av_get_sample_fmt_name(dec_ctx->sample_fmt),
            buf);
        ret = avfilter_graph_create_filter(&buffersrc_ctx, buffersrc, "in", args, NULL, filter_graph);
        if (ret < 0) {
            errorf("Cannot create audio buffer source\n");
            goto end;
        }

        ret = avfilter_graph_create_filter(&buffersink_ctx, buffersink, "out", NULL, NULL, filter_graph);
        if (ret < 0) {
            errorf("Cannot create audio buffer sink\n");
            goto end;
        }

        ret = av_opt_set_bin(
            buffersink_ctx,
            "sample_fmts",
            (uint8_t *)&enc_ctx->sample_fmt,
            sizeof(enc_ctx->sample_fmt),
            AV_OPT_SEARCH_CHILDREN);
        if (ret < 0) {
            errorf("Cannot set output sample format\n");
            goto end;
        }

        av_channel_layout_describe(&enc_ctx->ch_layout, buf, sizeof(buf));
        ret = av_opt_set(buffersink_ctx, "ch_layouts", buf, AV_OPT_SEARCH_CHILDREN);
        if (ret < 0) {
            errorf("Cannot set output channel layout\n");
            goto end;
        }

        ret = av_opt_set_bin(
            buffersink_ctx,
            "sample_rates",
            (uint8_t *)&enc_ctx->sample_rate,
            sizeof(enc_ctx->sample_rate),
            AV_OPT_SEARCH_CHILDREN);
        if (ret < 0) {
            errorf("Cannot set output sample rate\n");
            goto end;
        }
    } else {
        ret = AVERROR_UNKNOWN;
        goto end;
    }

    /* Endpoints for the filter graph. */
    outputs->name       = av_strdup("in");
    outputs->filter_ctx = buffersrc_ctx;
    outputs->pad_idx    = 0;
    outputs->next       = NULL;

    inputs->name       = av_strdup("out");
    inputs->filter_ctx = buffersink_ctx;
    inputs->pad_idx    = 0;
    inputs->next       = NULL;

    if (!outputs->name || !inputs->name) {
        ret = AVERROR(ENOMEM);
        goto end;
    }

    if ((ret = avfilter_graph_parse_ptr(filter_graph, filter_spec, &inputs, &outputs, NULL)) < 0) { goto end; }

    if ((ret = avfilter_graph_config(filter_graph, NULL)) < 0) { goto end; }

    /* Fill FilteringContext */
    fctx->buffersrc_ctx  = buffersrc_ctx;
    fctx->buffersink_ctx = buffersink_ctx;
    fctx->filter_graph   = filter_graph;

end:
    avfilter_inout_free(&inputs);
    avfilter_inout_free(&outputs);

    return ret;
}

static int init_filters(void) {
    const char * filter_spec = NULL;
    unsigned int i = 0;
    int          ret = 0;
    filter_ctx = av_malloc(sizeof(*filter_ctx));
    if (!filter_ctx) { return AVERROR(ENOMEM); }

    for (i = 0; i < ifmt_ctx->nb_streams; i++) {
        if (ifmt_ctx->streams[i]->codecpar->codec_type != AVMEDIA_TYPE_AUDIO) { continue; }

        filter_ctx->buffersrc_ctx  = NULL;
        filter_ctx->buffersink_ctx = NULL;
        filter_ctx->filter_graph   = NULL;

        filter_spec = "anull"; /* passthrough (dummy) filter for audio */
        ret         = init_filter(filter_ctx, stream_ctx->dec_ctx, stream_ctx->enc_ctx, filter_spec);
        if (ret) { return ret; }

        filter_ctx->enc_pkt = av_packet_alloc();
        if (!filter_ctx->enc_pkt) { return AVERROR(ENOMEM); }

        filter_ctx->filtered_frame = av_frame_alloc();
        if (!filter_ctx->filtered_frame) { return AVERROR(ENOMEM); }

        return (int)i;
    }
    return -1; // No stream found
}

static int encode_write_frame(int stream_index, int flush) {
    StreamContext *   stream     = stream_ctx;
    FilteringContext *filter     = filter_ctx;
    AVFrame *         filt_frame = flush ? NULL : filter->filtered_frame;
    AVPacket *        enc_pkt    = filter->enc_pkt;
    int               ret = 0;

    tracef("Encoding frame\n");
    /* encode filtered frame */
    av_packet_unref(enc_pkt);

    ret = avcodec_send_frame(stream->enc_ctx, filt_frame);

    if (ret < 0) { return ret; }

    while (ret >= 0) {
        ret = avcodec_receive_packet(stream->enc_ctx, enc_pkt);

        if (ret == AVERROR(EAGAIN) || ret == AVERROR_EOF) { return 0; }

        /* prepare packet for muxing */
        enc_pkt->stream_index = stream_index;
        av_packet_rescale_ts(enc_pkt, stream->enc_ctx->time_base, ofmt_ctx->streams[stream_index]->time_base);

        tracef("Muxing frame\n");
        /* mux encoded frame */
        ret = av_interleaved_write_frame(ofmt_ctx, enc_pkt);
    }

    return ret;
}

static int filter_encode_write_frame(AVFrame *frame, int stream_index) {
    FilteringContext *filter = filter_ctx;
    int               ret = 0;

    //    infof("Pushing decoded frame to filters\n");
    /* push the decoded frame into the filtergraph */
    ret = av_buffersrc_add_frame_flags(filter->buffersrc_ctx, frame, 0);
    if (ret < 0) {
        errorf("Error while feeding the filtergraph\n");
        return ret;
    }

    /* pull filtered frames from the filtergraph */
    while (1) {
        //        infof("Pulling filtered frame from filters\n");
        ret = av_buffersink_get_frame(filter->buffersink_ctx, filter->filtered_frame);
        if (ret < 0) {
            /* if no more frames for output - returns AVERROR(EAGAIN)
             * if flushed and no more frames for output - returns AVERROR_EOF
             * rewrite retcode to 0 to show it as normal procedure completion
             */
            if (ret == AVERROR(EAGAIN) || ret == AVERROR_EOF) { ret = 0; }
            break;
        }

        filter->filtered_frame->pict_type = AV_PICTURE_TYPE_NONE;
        ret                               = encode_write_frame(stream_index, 0);
        av_frame_unref(filter->filtered_frame);
        if (ret < 0) { break; }
    }

    return ret;
}

static int flush_encoder(int stream_index) {
    if (!(stream_ctx->enc_ctx->codec->capabilities & AV_CODEC_CAP_DELAY)) { return 0; }

    infof("Flushing stream #%u encoder\n", stream_index);
    return encode_write_frame(stream_index, 1);
}

audiofs_avio_handle *do_transcode(
    const char *          from_path,
    AVFormatContext *     from_context,
    const char *          to,
    const AVOutputFormat *oformat,
    const char *          format_name) {
    volatile int ret = 0;
    AVPacket *   packet = NULL;
    int          stream_index;
    int          selected_stream = 0;

    if (from_path != NULL) {
        if ((ret = open_input_file(from_path)) < 0) { goto end; }
    } else {
        if ((ret = open_input_file_with_format_context(from_context)) < 0) { goto end; }
    }
    if ((ret = open_output_file(oformat, format_name, to)) < 0) { goto end; }
    if ((ret = init_filters()) < 0) {
        goto end;
    } else {
        selected_stream = ret;
    }
    if (!(packet = av_packet_alloc())) { goto end; }

    /* read all packets */
    while (1) {
        if ((ret = av_read_frame(ifmt_ctx, packet)) < 0) { break; }

        if (packet->stream_index != selected_stream) { continue; }
        stream_index = packet->stream_index;
        tracef("Demuxer gave frame of stream_index %u\n", stream_index);

        if (filter_ctx->filter_graph) {
            StreamContext *stream = stream_ctx;

            tracef("Going to reencode&filter the frame\n");

            av_packet_rescale_ts(packet, ifmt_ctx->streams[stream_index]->time_base, stream->dec_ctx->time_base);
            ret = avcodec_send_packet(stream->dec_ctx, packet);
            if (ret < 0) {
                errorf("Decoding failed\n");
                break;
            }

            while (ret >= 0) {
                ret = avcodec_receive_frame(stream->dec_ctx, stream->dec_frame);
                if (ret == AVERROR_EOF || ret == AVERROR(EAGAIN)) {
                    break;
                } else if (ret < 0) {
                    goto end;
                }

                stream->dec_frame->pts = stream->dec_frame->best_effort_timestamp;
                ret                    = filter_encode_write_frame(stream->dec_frame, stream_index);
                if (ret < 0) { goto end; }
            }
        } else {
            /* remux this frame without reencoding */
            av_packet_rescale_ts(
                packet,
                ifmt_ctx->streams[stream_index]->time_base,
                ofmt_ctx->streams[stream_index]->time_base);

            ret = av_interleaved_write_frame(ofmt_ctx, packet);
            if (ret < 0) { goto end; }
        }
        av_packet_unref(packet);
    }

    /* flush filter */
    if (filter_ctx->filter_graph) {
        ret = filter_encode_write_frame(NULL, selected_stream);
        if (ret < 0) {
            errorf("Flushing filter failed\n");
            goto end;
        }
    }

    /* flush encoder */
    ret = flush_encoder(selected_stream);
    if (ret < 0) {
        errorf("Flushing encoder failed\n");
        goto end;
    }

    av_write_trailer(ofmt_ctx);
end:
    av_packet_free(&packet);
    avcodec_free_context(&stream_ctx->dec_ctx);
    avcodec_free_context(&stream_ctx->enc_ctx);
    if (filter_ctx && filter_ctx->filter_graph) {
        avfilter_graph_free(&filter_ctx->filter_graph);
        av_packet_free(&filter_ctx->enc_pkt);
        av_frame_free(&filter_ctx->filtered_frame);
    }

    av_frame_free(&stream_ctx->dec_frame);
    av_free(stream_ctx);

    av_free(filter_ctx);
    if (from_path != NULL) {
        // Only free them if they were allocated here!

        avformat_close_input(&ifmt_ctx);
    }
    audiofs_avio_handle *handle = NULL;
    if (ofmt_ctx && !(ofmt_ctx->oformat->flags & AVFMT_NOFILE)) {
        // Extract out file handle, so the caller can read the transcoded data.
        avio_flush(ofmt_ctx->pb);
        handle = ofmt_ctx->pb->opaque;
        infof("AudioFS AVIO handle: %p\n", handle);
        infof("extracted shared memory file handle: %p\n", handle);
        infof("memory backed?: %d\n", audiofs_avio_is_memory_backed(handle));
    }
    avformat_free_context(ofmt_ctx);

    if (ret < 0) {
        errorf("Error occurred: %s\n", av_err2str(ret));
        return NULL;
    }

    return handle;
}
#ifndef AUDIOFS_CGO

int main(int argc, char **argv) {
    if (argc != 3) {
        errorf("Usage: %s <input file> <output file>\n", argv[0]);
        return 1;
    }

    return do_transcode(argv[1], argv[2], NULL, NULL, "aiff");
}
#endif
