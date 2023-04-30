#ifndef NATIVE_RESAMPLER_H
#define NATIVE_RESAMPLER_H

#include "util.h"

#define DITHER_DEFAULT SWR_DITHER_NS_HIGH_SHIBATA

int set_dither_settings(struct decoder_context *ctx, enum SwrDitherType dither, enum SwrEngine resampler);

enum SwrDitherType dither_by_name(const char *dither);
int                get_supported_resampler(const char *resampler);

int setup_swr(struct decoder_context *ctx, enum SwrDitherType dither, enum SwrEngine resampler);
#endif // NATIVE_RESAMPLER_H
