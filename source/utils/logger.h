/*
 * Copyright (C) 2022-2024 Volodymyr Atamanenko
 *
 * This software may be modified and distributed under the terms
 * of the MIT license. See the LICENSE file for details.
 */

/**
 * @file  logger.h
 * @brief Logging utilities with incremental per-run file support.
 */

#ifndef SOLOADER_LOGGER_H
#define SOLOADER_LOGGER_H

#include <stdbool.h>

#ifdef __cplusplus
extern "C" {
#endif

#define LT_DEBUG   0
#define LT_INFO    1
#define LT_WARN    2
#define LT_ERROR   3
#define LT_FATAL   4
#define LT_SUCCESS 5
#define LT_WAIT    6

#define l_debug(...)   _log_print(LT_DEBUG,   __VA_ARGS__)
#define l_info(...)    _log_print(LT_INFO,    __VA_ARGS__)
#define l_warn(...)    _log_print(LT_WARN,    __VA_ARGS__)
#define l_success(...) _log_print(LT_SUCCESS, __VA_ARGS__)
#define l_wait(...)    _log_print(LT_WAIT,    __VA_ARGS__)
#define l_error(...)   _log_print(LT_ERROR,   __VA_ARGS__)
#define l_fatal(...)   _log_print(LT_FATAL,   __VA_ARGS__)

void _log_print(int t, const char* fmt, ...)
                __attribute__ ((format (printf, 2, 3)));

/**
 * Initialize per-run incremental log file (e.g. ux0:data/moderncombat2/logs/port_0001.log).
 * Increments each run so no logs are overwritten.
 */
int log_init(void);

/** Flushes and commits the log file to storage. */
void log_sync(void);

/** Returns the path of the current log file. */
const char *log_current_path(void);

/**
 * Port trace utility function compatible with all soloader modules.
 * Formats and writes unbuffered to the current incremental log file.
 */
void port_trace(const char *format, ...)
                __attribute__ ((format (printf, 1, 2)));

/**
 * Force flush and sync of port trace logs.
 */
void port_trace_flush(void);

#ifdef __cplusplus
};
#endif

#endif // SOLOADER_LOGGER_H
