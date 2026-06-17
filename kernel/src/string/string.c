#include "string.h"
#include "memops.h"
#include "memory/memory.h"

size_t strlen(const char* str){
    size_t size = 0;
    while (*str)
    {
        size++;
        str++;
    }
    return size;
}

int strcmp(const char* s1, const char* s2) {
    while (*s1 && (*s1 == *s2)) {
        s1++;
        s2++;
    }
    return (unsigned char)(*s1) - (unsigned char)(*s2);
}

char *strcpy(char *dest, const char *src) {
    char *ret = dest;
    while ((*dest++ = *src++));
    return ret;
}


char *strncpy(char *dest, const char *src, size_t n) {
    size_t i = 0;
    while (i < n && src[i]) {
        dest[i] = src[i];
        i++;
    }
    while (i < n) {
        dest[i++] = '\0';
    }
    return dest;
}

char *strchr(const char *str, int c) {
    while (*str != '\0') {
        if (*str == c) {
            return (char *)str;
        }
        str++;
    }
    return NULL;
}

char *strrchr(const char *s, int c)
{
    const char *last = NULL;

    while (*s)
    {
        if (*s == (char)c)
            last = s;
        s++;
    }

    if ((char)c == '\0')
        return (char *)s;

    return (char *)last;
}



char* strtok_r(char* str, const char* delim, char** saveptr) {
    if (!str) str = *saveptr;

    while (*str && strchr(delim, *str))str++;

    if (!*str) return NULL;

    char* token_start = str;

    
    while (*str && !strchr(delim, *str)) {
        str++;
    }

    if (*str) {
        *str = '\0';
        *saveptr = str + 1;
    } else {
        *saveptr = str;
    }

    return token_start;
}

char *strdup(const char *s){
    size_t len = strlen(s);
    char* pter = (char*)kmalloc_impl(len+1);
    if(!pter)return NULL;
    memcpy(pter,s,len+1);
    return pter;
}

char *strndup( const char *str, size_t size ){
    size_t stringlen = strlen(str);
    size_t len = size < stringlen? size : stringlen; 
    char* pter = (char*)kmalloc_impl(len+1);
    if(!pter)return NULL;
    memcpy(pter,str,len);
    pter[len] = '\0';
    return pter;
}