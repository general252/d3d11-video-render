#pragma once

#include <stdio.h>
#include <stdarg.h>
#include <Windows.h>

// _check_log 当v为true时打印数据
static bool _log_check(bool v, const char* fmt, ...) {
    if (v) {
        va_list vl;

        va_start(vl, fmt);
        vprintf(fmt, vl);
        va_end(vl);
    }

    return v;
}

static bool _log_check_hr(long hr, const char* fmt, ...) {
    if (hr < 0) {
        va_list vl;

        va_start(vl, fmt);
        vprintf(fmt, vl);
        va_end(vl);


        const int BUFFER_LEN = 256;
        char buffer[BUFFER_LEN] = { 0 };

        FormatMessageA(FORMAT_MESSAGE_IGNORE_INSERTS | FORMAT_MESSAGE_FROM_SYSTEM,
            NULL, hr, 0, buffer, BUFFER_LEN, NULL);
        printf(buffer);
    }

    return hr < 0;
}

#define LOG(format, ...) { fprintf(stderr, "[%s:%d] " format, __FUNCTION__ , __LINE__, ##__VA_ARGS__); }

#define LOG_CHECK_(v, format, ...) ( _log_check(v, "[%s:%d] " format, __FUNCTION__ , __LINE__, ##__VA_ARGS__) )

#define LOG_CHECK_HR(hr, format, ...) ( _log_check_hr(hr, "[%s:%d] " format, __FUNCTION__ , __LINE__, ##__VA_ARGS__) )
