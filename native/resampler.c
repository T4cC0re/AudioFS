
#include "resampler.h"
#include <assert.h>

#include <libswresample/swresample.h>

/**
 * set_dither_settings: set dither and resampler configuration
 *
 * might be called from resampler later.
 *
 * @param ctx       decoder context (is being modified)
 * @param dither    dithering method
 * @param resampler resampling engine
 * @return 0 on success, or an AVERROR code in case of error
 */
int set_dither_settings(struct decoder_context *ctx, enum SwrDitherType dither, enum SwrEngine resampler) {
    int code;

    AUDIOFS_PRINTVAL(ctx, "p");
    AUDIOFS_PRINTVAL(dither, "d");
    AUDIOFS_PRINTVAL(resampler, "d");
    AUDIOFS_PRINTVAL(ctx->swrContext, "p");

    assert(context_ok(ctx));

    if (ctx->swrContext == NULL) { return -1; }

    code = av_opt_set_int(ctx->swrContext, "resampler", (int64_t)resampler, 0);
    if (code < 0) { return code; }

    code = av_opt_set_int(ctx->swrContext, "dither_method", (int64_t)dither, 0);
    if (code < 0) { return code; }

    if (resampler == SWR_ENGINE_SWR) {
        // This cannot be set on SoX
        code = av_opt_set_int(ctx->swrContext, "output_sample_bits", (int64_t)ctx->override_depth, 0);
        if (code < 0) { return code; }
    }

    if (resampler == SWR_ENGINE_SOXR) {
        fprintf(stderr, "Using SoX resampler with 24-bit output. 32-bit output will be clipped to 24-bit.\n");
        // This is a SoXR setting only. 28 = SoX Very High Quality
        code = av_opt_set_int(ctx->swrContext, "precision", (int64_t)28, 0);
        if (code < 0) { return code; }
    }

    return 0;
}

/**
 *
 * @param dither string of the text name of the requested dithering method
 * @return enum SwrEngine of the passed request, or the default dither if an unknown method was requested
 */
enum SwrDitherType dither_by_name(const char *dither) {
    AUDIOFS_PRINTVAL(dither, "s");

    if (NULL == dither) { goto end; }

    if (0 == strcmp(dither, "none")) {
        return SWR_DITHER_NONE;
    } else if (0 == strcmp(dither, "rectangular")) {
        return SWR_DITHER_RECTANGULAR;
    } else if (0 == strcmp(dither, "triangular")) {
        return SWR_DITHER_TRIANGULAR;
    } else if (0 == strcmp(dither, "triangular_hp")) {
        return SWR_DITHER_TRIANGULAR_HIGHPASS;
    } else if (0 == strcmp(dither, "shibata")) {
        return SWR_DITHER_NS_SHIBATA;
    } else if (0 == strcmp(dither, "low_shibata")) {
        return SWR_DITHER_NS_LOW_SHIBATA;
    } else if (0 == strcmp(dither, "high_shibata")) {
        return SWR_DITHER_NS_HIGH_SHIBATA;
    } else if (0 == strcmp(dither, "lipshitz")) {
        return SWR_DITHER_NS_LIPSHITZ;
    } else if (0 == strcmp(dither, "f_weighted")) {
        return SWR_DITHER_NS_F_WEIGHTED;
    } else if (0 == strcmp(dither, "modified_e_weighted")) {
        return SWR_DITHER_NS_MODIFIED_E_WEIGHTED;
    } else if (0 == strcmp(dither, "improved_e_weighted")) {
        return SWR_DITHER_NS_IMPROVED_E_WEIGHTED;
    }
end:
    errorf("Dither: '%s' is unknown.\n", dither);
    return DITHER_DEFAULT;
}

/**
 *
 * @param resampler if null-pointer, a default value is returned
 * @return -1 on error or enum SwrEngine (you have to cast)
 */
int get_supported_resampler(const char *resampler) {
    SwrContext *   swr        = NULL;
    bool           supported  = false;
    enum SwrEngine swr_engine = (enum SwrEngine) - 1;
    int            tmp_level  = 0;

    // If we get a null value, skip all the validation and return SWR.
    if (NULL == resampler) { return SWR_ENGINE_SWR; }

    AUDIOFS_PRINTVAL(resampler, "s");
    swr = swr_alloc();
    AUDIOFS_PRINTVAL(swr, "p");
    if (swr == NULL) {
        errorf("Could not allocate context to check '%s' resampler\n", resampler);
        // No goto, as we don't swr_free() in this case
        return swr_engine;
    }
    AVChannelLayout stereo = (AVChannelLayout)AV_CHANNEL_LAYOUT_STEREO;
    if (0 != swr_alloc_set_opts2(&swr, &stereo, AV_SAMPLE_FMT_S16, 44100, &stereo, AV_SAMPLE_FMT_S16, 44100, 0, NULL)) {
        errorf("Could not set options on context to check '%s' resampler\n", resampler);
        goto end;
    }

    supported = (0 == av_opt_set(swr, "resampler", resampler, 0));
    AUDIOFS_PRINTVAL(supported, "d");
    if (!supported) { goto end; }

    tmp_level = av_log_get_level();
    av_log_set_level(AV_LOG_QUIET);
    supported = (0 == swr_init(swr));
    av_log_set_level(tmp_level);
    AUDIOFS_PRINTVAL(supported, "d");

    if (!supported) {
        errorf("Could not initialize swresample with resampler '%s'\n", resampler);
        goto end;
    }

    if (0 == strcmp(resampler, "soxr")) {
        swr_engine = SWR_ENGINE_SOXR;
    } else {
        swr_engine = SWR_ENGINE_SWR;
    }

end:
    swr_free(&swr);
    return swr_engine;
}

#define return_break        \
    {                       \
        AUDIOFS_BREAKPOINT; \
        return -1;          \
    }

int setup_swr(struct decoder_context *ctx, enum SwrDitherType dither, enum SwrEngine resampler) {
    assert(context_ok(ctx));

    AUDIOFS_PRINTVAL(ctx, "p");
    AUDIOFS_PRINTVAL(dither, "d");
    AUDIOFS_PRINTVAL(resampler, "d");

    if (ctx->swrContext == NULL) { return -1; }

    if (0 != av_opt_set_chlayout(ctx->swrContext, "in_channel_layout", &ctx->channel_layout, 0)) { return_break }
    if (0 != av_opt_set_chlayout(ctx->swrContext, "out_channel_layout", &ctx->override_channel_layout, 0)) {
        return_break
    }
    if (0 != av_opt_set_int(ctx->swrContext, "in_sample_rate", ctx->sample_rate, 0)) { return_break }
    if (0 != av_opt_set_int(ctx->swrContext, "out_sample_rate", ctx->override_sample_rate, 0)) { return_break }
    if (0 != av_opt_set_int(ctx->swrContext, "in_sample_fmt", ctx->sample_fmt, 0)) { return_break }
    if (0 != av_opt_set_int(ctx->swrContext, "out_sample_fmt", ctx->override_sample_fmt, 0)) { return_break }

    debugf("initializing swr_context");
    int ret = swr_init(ctx->swrContext);
    if (ret < 0) {
        errorf("%s", av_err2str(ret));
        AUDIOFS_PRINTVAL(ret, "d");
        errorf("Error while swr_init\n");
        return_break
    }

    return 0;
}
