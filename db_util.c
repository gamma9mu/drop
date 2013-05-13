#include <string.h>
#include "db_util.h"

void
normalize_key(const char *key) {
    char *space = strchr(key, ' ');
    if (space) {
        *space = '\0';
    }
}

