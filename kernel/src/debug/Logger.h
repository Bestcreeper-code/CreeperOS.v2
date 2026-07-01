#ifndef DEBUG_H
#define DEBUG_H

#include <stdbool.h>
#include <stdarg.h>
#include <stddef.h>
#include <stdint.h>

#include "config.h"


#define COM1_PORT 0x3F8  
    



#define Sys_log_Pos(frmt , ...) sys_serial_logf(frmt, __FILE_NAME__, __func__, __LINE__ ,##__VA_ARGS__)

#define Sys_log_NoPos(frmt , ...) sys_serial_logf(frmt, NULL, NULL, 0, ##__VA_ARGS__)

#if !POS_DEBUG_LOGS

#define Sys_log(frmt, ...) Sys_log_Pos(frmt, ##__VA_ARGS__)

#define Sys_Debug(frmt, ...)   Sys_log_Pos("[\e[90mDEBUG\e[0m] "   frmt, ##__VA_ARGS__)
#define Sys_Info(frmt, ...)    Sys_log_Pos("[\e[36mINFO\e[0m] "    frmt, ##__VA_ARGS__)
#define Sys_Success(frmt, ...) Sys_log_Pos("[\e[32mOK\e[0m] " frmt, ##__VA_ARGS__)
#define Sys_Warning(frmt, ...) Sys_log_Pos("[\e[93mWARNING\e[0m] " frmt, ##__VA_ARGS__)
#define Sys_Error(frmt, ...)   Sys_log_Pos("[\e[31mERROR\e[0m] "   frmt, ##__VA_ARGS__)
#define Sys_Fatal(frmt, ...)   Sys_log_Pos("[\e[35mFATAL\e[0m] "   frmt, ##__VA_ARGS__)

#else

#define Sys_log(frmt, ...) Sys_log_NoPos(frmt, ##__VA_ARGS__)

#define Sys_Debug(frmt, ...)   Sys_log_NoPos("[\e[90mDEBUG\e[0m] "   frmt, ##__VA_ARGS__)
#define Sys_Info(frmt, ...)    Sys_log_NoPos("[\e[36mINFO\e[0m] "    frmt, ##__VA_ARGS__)
#define Sys_Success(frmt, ...) Sys_log_NoPos("[\e[32mOK\e[0m] "      frmt, ##__VA_ARGS__)
#define Sys_Warning(frmt, ...) Sys_log_NoPos("[\e[93mWARNING\e[0m] " frmt, ##__VA_ARGS__)
#define Sys_Error(frmt, ...)   Sys_log_NoPos("[\e[31mERROR\e[0m] "   frmt, ##__VA_ARGS__)
#define Sys_Fatal(frmt, ...)   Sys_log_NoPos("[\e[35mFATAL\e[0m] "   frmt, ##__VA_ARGS__)

#endif




#define Sys_Breakpoint() Sys_log("Breakpoint hit at %s:%d\n", __FILE_NAME__, __LINE__); for(;;)
#define Sys_Step_Point() Sys_log("Step Point hit at %s:%d\n", __FILE_NAME__, __LINE__); getc()

void serial_init();

void log_queue_init();
void _log_manager_thread();

void serial_write_char(char c);
void serial_write_string(const char* str);
void serial_log_hex(const char* label, uint32_t val);

void sys_serial_vlogf(const char* format, const char* file,
    const char* func, int line, va_list args);
void sys_serial_logf(const char* frmt, const char* file, const char* func, int line, ...) __attribute__ ((format (printf, 1, 5)));




typedef enum {
    
    ANSI_RESET              = 0,

    
    ANSI_BLACK              = 30,
    ANSI_RED                = 31,
    ANSI_GREEN              = 32,
    ANSI_YELLOW             = 33,
    ANSI_BLUE               = 34,
    ANSI_MAGENTA            = 35,
    ANSI_CYAN               = 36,
    ANSI_WHITE              = 37,

    
    ANSI_BRIGHT_BLACK       = 90,
    ANSI_BRIGHT_RED         = 91,
    ANSI_BRIGHT_GREEN       = 92,
    ANSI_BRIGHT_YELLOW      = 93,
    ANSI_BRIGHT_BLUE        = 94,
    ANSI_BRIGHT_MAGENTA     = 95,
    ANSI_BRIGHT_CYAN        = 96,
    ANSI_BRIGHT_WHITE       = 97,

    
    ANSI_BG_BLACK           = 40,
    ANSI_BG_RED             = 41,
    ANSI_BG_GREEN           = 42,
    ANSI_BG_YELLOW          = 43,
    ANSI_BG_BLUE            = 44,
    ANSI_BG_MAGENTA         = 45,
    ANSI_BG_CYAN            = 46,
    ANSI_BG_WHITE           = 47,

    
    ANSI_BG_BRIGHT_BLACK    = 100,
    ANSI_BG_BRIGHT_RED      = 101,
    ANSI_BG_BRIGHT_GREEN    = 102,
    ANSI_BG_BRIGHT_YELLOW   = 103,
    ANSI_BG_BRIGHT_BLUE     = 104,
    ANSI_BG_BRIGHT_MAGENTA  = 105,
    ANSI_BG_BRIGHT_CYAN     = 106,
    ANSI_BG_BRIGHT_WHITE    = 107
} ansi_color_t;

    
#define ESC_RESET              "\e[0m"

#define ESC_BLACK              "\e[30m"
#define ESC_RED                "\e[31m"
#define ESC_GREEN              "\e[32m"
#define ESC_YELLOW             "\e[33m"
#define ESC_BLUE               "\e[34m"
#define ESC_MAGENTA            "\e[35m"
#define ESC_CYAN               "\e[36m"
#define ESC_WHITE              "\e[37m"

#define ESC_BRIGHT_BLACK       "\e[90m"
#define ESC_BRIGHT_RED         "\e[91m"
#define ESC_BRIGHT_GREEN       "\e[92m"
#define ESC_BRIGHT_YELLOW      "\e[93m"
#define ESC_BRIGHT_BLUE        "\e[94m"
#define ESC_BRIGHT_MAGENTA     "\e[95m"
#define ESC_BRIGHT_CYAN        "\e[96m"
#define ESC_BRIGHT_WHITE       "\e[97m"

#define ESC_BG_BLACK           "\e[40m"
#define ESC_BG_RED             "\e[41m"
#define ESC_BG_GREEN           "\e[42m"
#define ESC_BG_YELLOW          "\e[43m"
#define ESC_BG_BLUE            "\e[44m"
#define ESC_BG_MAGENTA         "\e[45m"
#define ESC_BG_CYAN            "\e[46m"
#define ESC_BG_WHITE           "\e[47m"

#define ESC_BG_BRIGHT_BLACK    "\e[100m"
#define ESC_BG_BRIGHT_RED      "\e[101m"
#define ESC_BG_BRIGHT_GREEN    "\e[102m"
#define ESC_BG_BRIGHT_YELLOW   "\e[103m"
#define ESC_BG_BRIGHT_BLUE     "\e[104m"
#define ESC_BG_BRIGHT_MAGENTA  "\e[105m"
#define ESC_BG_BRIGHT_CYAN     "\e[106m"
#define ESC_BG_BRIGHT_WHITE    "\e[107m"




#endif // DEBUG_H
