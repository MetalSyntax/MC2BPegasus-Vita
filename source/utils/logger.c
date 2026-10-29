/*
 * Copyright (C) 2022-2024 Volodymyr Atamanenko
 *
 * This software may be modified and distributed under the terms
 * of the MIT license. See the LICENSE file for details.
 */

#include "utils/logger.h"

#include <psp2/io/fcntl.h>
#include <psp2/io/stat.h>
#include <psp2/io/dirent.h>
#include <psp2/kernel/clib.h>
#include <psp2/kernel/processmgr.h>
#include <psp2/kernel/threadmgr.h>

#include <stdarg.h>
#include <stdbool.h>
#include <stdatomic.h>
#include <stdlib.h>
#include <string.h>

#ifndef DATA_PATH
#define DATA_PATH "ux0:data/moderncombat2/"
#endif

#define LOG_DIR       DATA_PATH "logs"
#define LOG_PREFIX    "port_"
#define LOG_EXT       ".log"
#define LOG_INDEX_MAX 9999

#define COLOR_RED     "\x1B[38;5;196m"
#define COLOR_PINK    "\x1B[38;5;212m"
#define COLOR_ORANGE  "\x1B[38;5;202m"
#define COLOR_BLUE    "\x1B[38;5;32m"
#define COLOR_GREEN   "\x1B[32m"
#define COLOR_CYAN    "\x1B[36m"
#define COLOR_END     "\033[0m"

static SceKernelLwMutexWork _log_mutex;
static atomic_bool _log_mutex_ready = ATOMIC_VAR_INIT(false);

static SceUID _log_fd = -1;
static int    _log_index = -1;
static char   _log_path[256] = "(none)";
static bool   _log_file_tried = false;
static uint64_t _log_t0 = 0;
static uint64_t _log_last_sync = 0;
static unsigned int _log_seq = 0;

#define LOG_SYNC_INTERVAL_US 1000000

static char buffer_a[2048];
static char buffer_b[2048];
static char buffer_c[2048];

/* Every small sceIoWrite to ux0: costs ~8-9 ms on retail hardware, which made
 * boot several seconds slower.  Lines are batched here and written in one go;
 * warnings/errors and port_trace_flush() still hit the card immediately. */
static char _log_pending[32 * 1024];
static size_t _log_pending_len;

static void log_write_pending(void) {
    if (_log_fd >= 0 && _log_pending_len)
        sceIoWrite(_log_fd, _log_pending, _log_pending_len);
    _log_pending_len = 0;
}

static void log_append(const char *line, int len) {
    if (_log_pending_len + (size_t)len > sizeof(_log_pending))
        log_write_pending();
    if ((size_t)len > sizeof(_log_pending)) {
        sceIoWrite(_log_fd, line, len);
        return;
    }
    memcpy(_log_pending + _log_pending_len, line, len);
    _log_pending_len += (size_t)len;
}

static void log_sync_throttled(bool force) {
    if (_log_fd < 0)
        return;
    uint64_t now = sceKernelGetProcessTimeWide();
    if (force || now - _log_last_sync >= LOG_SYNC_INTERVAL_US) {
        log_write_pending();
        sceIoSyncByFd(_log_fd, 0);
        _log_last_sync = now;
    }
}

void log_sync(void) {
    if (_log_fd >= 0) {
        if (atomic_load_explicit(&_log_mutex_ready, memory_order_relaxed))
            sceKernelLockLwMutex(&_log_mutex, 1, NULL);
        log_write_pending();
        sceIoSyncByFd(_log_fd, 0);
        _log_last_sync = sceKernelGetProcessTimeWide();
        if (atomic_load_explicit(&_log_mutex_ready, memory_order_relaxed))
            sceKernelUnlockLwMutex(&_log_mutex, 1);
    }
}

const char *log_current_path(void) {
    return _log_path;
}

static int _log_next_index(void) {
    int max = 0;
    SceUID dfd = sceIoDopen(LOG_DIR);
    if (dfd < 0)
        return 1;

    SceIoDirent entry;
    size_t plen = strlen(LOG_PREFIX);
    size_t elen = strlen(LOG_EXT);

    sceClibMemset(&entry, 0, sizeof(entry));
    while (sceIoDread(dfd, &entry) > 0) {
        const char *name = entry.d_name;
        size_t len = strlen(name);
        if (len <= elen || strcmp(name + len - elen, LOG_EXT) != 0)
            continue;

        int num = 0;
        if (strncmp(name, LOG_PREFIX, plen) == 0) {
            num = atoi(name + plen);
        } else if (strncmp(name, "port", 4) == 0) {
            num = atoi(name + 4);
        }

        if (num > max && num < LOG_INDEX_MAX)
            max = num;
    }
    sceIoDclose(dfd);

    if (max >= LOG_INDEX_MAX)
        return 1;
    return max + 1;
}

static void _log_open_file(void) {
    if (_log_file_tried)
        return;
    _log_file_tried = true;

    _log_t0 = sceKernelGetProcessTimeWide();
    sceIoMkdir(DATA_PATH, 0777);
    sceIoMkdir(LOG_DIR, 0777);

    _log_index = _log_next_index();
    sceClibSnprintf(_log_path, sizeof(_log_path),
                    LOG_DIR "/" LOG_PREFIX "%04d" LOG_EXT, _log_index);

    _log_fd = sceIoOpen(_log_path,
                        SCE_O_WRONLY | SCE_O_CREAT | SCE_O_TRUNC, 0777);
    if (_log_fd < 0) {
        sceClibPrintf("Error: failed to open log file %s: 0x%x\n", _log_path, _log_fd);
    } else {
        sceClibPrintf("=== Modern Combat 2: Logging to %s ===\n", _log_path);
        char header[256];
        int len = sceClibSnprintf(header, sizeof(header),
                                  "=== Modern Combat 2 Vita log #%04d (file: %s) ===\n",
                                  _log_index, _log_path);
        if (len > 0) {
            sceIoWrite(_log_fd, header, len);
            sceIoSyncByFd(_log_fd, 0);
        }
    }
}

int log_init(void) {
    if (!_log_file_tried) {
        _log_open_file();
    }
    return _log_index;
}

static void _ensure_mutex(void) {
    if (!atomic_load_explicit(&_log_mutex_ready, memory_order_relaxed)) {
        int ret = sceKernelCreateLwMutex(&_log_mutex, "log_lock", 0, 0, NULL);
        if (ret < 0) {
            sceClibPrintf("Error: failed to create log mutex: 0x%x\n", ret);
            return;
        }
        atomic_store_explicit(&_log_mutex_ready, true, memory_order_relaxed);
    }
}

void _log_print(int t, const char* fmt, ...) {
    _ensure_mutex();
    sceKernelLockLwMutex(&_log_mutex, 1, NULL);

    if (!_log_file_tried)
        _log_open_file();

    const char *color, *label;
    switch (t) {
        case LT_DEBUG:   color = COLOR_PINK;   label = "• debug  "; break;
        case LT_INFO:    color = COLOR_BLUE;   label = "ℹ info   "; break;
        case LT_WARN:    color = COLOR_ORANGE; label = "⚠ warning"; break;
        case LT_ERROR:   color = COLOR_RED;    label = "⨯ error  "; break;
        case LT_FATAL:   color = COLOR_RED;    label = "! fatal  "; break;
        case LT_SUCCESS: color = COLOR_GREEN;  label = "! success"; break;
        case LT_WAIT:    color = COLOR_CYAN;   label = "… waiting"; break;
        default:
            sceKernelUnlockLwMutex(&_log_mutex, 1);
            return;
    }

    va_list list;

    // Colorized console output
    sceClibSnprintf(buffer_a, sizeof(buffer_a), " %s%s%s  %s\n",
                    color, label, COLOR_END, fmt);
    va_start(list, fmt);
    sceClibVsnprintf(buffer_b, sizeof(buffer_b), buffer_a, list);
    va_end(list);
    sceClibPrintf(buffer_b);

    if (_log_fd >= 0) {
        va_list list2;
        va_start(list2, fmt);
        sceClibVsnprintf(buffer_a, sizeof(buffer_a), fmt, list2);
        va_end(list2);

        uint32_t ms = (uint32_t)((sceKernelGetProcessTimeWide() - _log_t0) / 1000);
        int len = sceClibSnprintf(buffer_c, sizeof(buffer_c),
                                  "[%06u][%6u.%03u][%s] %s\n",
                                  ++_log_seq, ms / 1000, ms % 1000,
                                  label, buffer_a);
        if (len > 0) {
            if ((unsigned int)len >= sizeof(buffer_c))
                len = (int)sizeof(buffer_c) - 1;
            log_append(buffer_c, len);
            log_sync_throttled(t == LT_ERROR || t == LT_FATAL);
        }
    }

    sceKernelUnlockLwMutex(&_log_mutex, 1);
}

void port_trace(const char *format, ...) {
    _ensure_mutex();
    sceKernelLockLwMutex(&_log_mutex, 1, NULL);

    if (!_log_file_tried)
        _log_open_file();

    va_list args;
    va_start(args, format);
    int body_len = sceClibVsnprintf(buffer_a, sizeof(buffer_a), format, args);
    va_end(args);

    if (body_len < 0) {
        sceKernelUnlockLwMutex(&_log_mutex, 1);
        return;
    }

    // Console output
    sceClibPrintf("%s\n", buffer_a);

    // Buffered file output
    if (_log_fd >= 0) {
        uint32_t ms = (uint32_t)((sceKernelGetProcessTimeWide() - _log_t0) / 1000);
        int len = sceClibSnprintf(buffer_b, sizeof(buffer_b),
                                  "[%06u][%6u.%03u] %s\n",
                                  ++_log_seq, ms / 1000, ms % 1000, buffer_a);
        if (len > 0) {
            if ((unsigned int)len >= sizeof(buffer_b))
                len = (int)sizeof(buffer_b) - 1;
            log_append(buffer_b, len);
            log_sync_throttled(false);
        }
    }

    sceKernelUnlockLwMutex(&_log_mutex, 1);
}

void port_trace_flush(void) {
    log_sync();
}
