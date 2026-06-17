#pragma once
#include <stddef.h>


size_t strlen(const char* str);
int strcmp(const char* s1, const char* s2);

char* strchr(const char* str, int c);
char* strrchr(const char* s, int c);

char* strcpy(char* dest, const char* src);
char* strncpy(char* dest, const char* src, size_t n);

char *strdup(const char* s); 
char *strndup(const char* str, size_t size );

char* strtok_r(char* str, const char* delim, char** saveptr);