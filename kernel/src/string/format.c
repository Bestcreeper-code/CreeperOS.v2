#include "format.h"
#include "printf/printf.h"
#include "memops.h"

#include <stdint.h>









const char* byte_nb_simplify(uint32_t size_bytes, char* buf, int depth) {
    const uint32_t GiB = 1024u * 1024u * 1024u;
    const uint32_t MiB = 1024u * 1024u;
    const uint32_t KiB = 1024u;

    char *p = buf;
    int written;

    if (size_bytes >= GiB) {
        uint32_t v = size_bytes / GiB;
        written = sprintf(p, "%uGiB", v);
        p += written;
        size_bytes %= GiB;

        if (--depth < 0 || size_bytes == 0) {
            if (size_bytes == 0) return buf;
            *p++ = ' ';
        } else {
            *p++ = ' ';
        }
    }

    if (size_bytes >= MiB) {
        uint32_t v = size_bytes / MiB;
        written = sprintf(p, "%uMiB", v);
        p += written;
        size_bytes %= MiB;

        if (--depth < 0 || size_bytes == 0) {
            if (size_bytes == 0) return buf;
            *p++ = ' ';
        } else {
            *p++ = ' ';
        }
    }

    if (size_bytes >= KiB) {
        uint32_t v = size_bytes / KiB;
        written = sprintf(p, "%uKiB", v);
        p += written;
        size_bytes %= KiB;

        if (--depth < 0 || size_bytes == 0) {
            if (size_bytes == 0) return buf;
            *p++ = ' ';
        } else {
            *p++ = ' ';
        }
    }

    sprintf(p, "%uB", size_bytes);
    return buf;
}