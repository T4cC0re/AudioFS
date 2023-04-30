//
// Created by t4cc0re on 4/30/23.
//

#ifndef NATIVE_GOLANG_GLUE_H
#define NATIVE_GOLANG_GLUE_H

#include "types.h"
#include <stdlib.h>

// region golang_glue.c
// endregion golang_glue.c

// region libav.c
extern void  audiofs_libav_setup();
extern char *get_metadate_from_file(char *path);
extern char *chromaprint_from_file(const char *path);
// endregion libav.c

// region logbuffer.c
extern void audiofs_log_level_set(int level);
// endregion logbuffer.c

extern audiofs_buffer *test_buffer;

#endif // NATIVE_GOLANG_GLUE_H
