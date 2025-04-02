#pragma once

#include <string>
#include <cinttypes>
#include <cstdio>
#include <time.h>
#include <cstring>

static void _debuglog_print_timestamp(void) {
    time_t ltime = time(NULL);
#if defined(_WIN32)
    char* str = asctime(localtime(&ltime));
#else
    struct tm ltime2 = { 0 };
    localtime_r(&ltime, &ltime2);
    char* str = asctime(&ltime2);
#endif
    printf("%.*s", (int)strlen(str) - 1, str);
}

static void _debuglog_print_log_type(std::string logType) {
    printf(" [%s] ", logType.c_str());
}

static void _debuglog_print_short_filename(std::string filename) {
    char* last = strrchr((char*)filename.c_str(), '/');
    if (last != NULL) {
        printf("%s: ", last + 1);
    }
    else {
        printf("???: ");
    }
}

static void _debuglog_print_log(std::string logType, std::string filename) {
    _debuglog_print_timestamp();
    _debuglog_print_log_type(logType);
    _debuglog_print_short_filename(filename);
}

#if defined(DISABLE_MODULE_LOG) or !defined(LOGGING)
#define LOG_DEBUG(...)
#define LOG_INFO(...)
#define LOG_ERROR(...)
#else
#define LOG_DEBUG(...) ( _debuglog_print_log("DEBUG", __FILE__), printf(__VA_ARGS__), printf("\n") )
#define LOG_INFO(...)  ( _debuglog_print_log("INFO",  __FILE__), printf(__VA_ARGS__), printf("\n") )
#define LOG_ERROR(...) ( _debuglog_print_log("ERROR", __FILE__), printf(__VA_ARGS__), printf("\n") )
#endif
