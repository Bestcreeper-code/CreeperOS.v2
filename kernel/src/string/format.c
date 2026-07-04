#include "format.h"
#include "printf/printf.h"
#include "memops.h"

#include <stdint.h>









const char* byte_nb_simplify(uint32_t size_bytes, char* buf, int depth) {
    const uint32_t gib = 1024u * 1024u * 1024u;
    const uint32_t mib = 1024u * 1024u;
    const uint32_t kib = 1024u;

    char *p = buf;
    int written;

    if (size_bytes >= gib) {
        uint32_t v = size_bytes / gib;
        written = sprintf(p, "%uGIB", v);
        p += written;
        size_bytes %= gib;

        if (--depth < 0 || size_bytes == 0) {
            if (size_bytes == 0) return buf;
            *p++ = ' ';
        } else {
            *p++ = ' ';
        }
    }

    if (size_bytes >= mib) {
        uint32_t v = size_bytes / mib;
        written = sprintf(p, "%uMIB", v);
        p += written;
        size_bytes %= mib;

        if (--depth < 0 || size_bytes == 0) {
            if (size_bytes == 0) return buf;
            *p++ = ' ';
        } else {
            *p++ = ' ';
        }
    }

    if (size_bytes >= kib) {
        uint32_t v = size_bytes / kib;
        written = sprintf(p, "%uKIB", v);
        p += written;
        size_bytes %= kib;

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