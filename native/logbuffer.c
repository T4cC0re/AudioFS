#include "types.h"

pthread_mutex_t go_log_mutex;
char *          go_log_buffer;
uint32_t        go_log_buffer_len;
int             go_log_level;

void audiofs_log_level_set(int level) { go_log_level = level; }
