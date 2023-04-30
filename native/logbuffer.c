#include "types.h"

__attribute__((used)) pthread_mutex_t go_log_mutex;
__attribute__((used)) char *          go_log_buffer;
__attribute__((used)) uint32_t        go_log_buffer_len;
__attribute__((used)) int             go_log_level;

__attribute__((used)) void audiofs_log_level_set(int level) { go_log_level = level; }
