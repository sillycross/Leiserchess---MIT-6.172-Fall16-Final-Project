// Copyright (c) 2015 MIT License by 6.172 Staff

#ifndef UTIL_H
#define UTIL_H

#include <inttypes.h>
#include <unistd.h>
#include <stddef.h>
#include <stdio.h>
#include <time.h>
#include <sys/sysinfo.h>

#ifndef DEBUG_VERBOSE
#define DEBUG_VERBOSE 0
#endif

#if DEBUG_VERBOSE
// DEBUG logging thresh level;
// When calling debug_log, only messages with log_level >= DEBUG_LOG_THRESH get printed
// so "important" debugging messages should get higher level
#define DEBUG_LOG_THRESH 3
#define DEBUG_LOG(arg...) debug_log(arg)
#define WHEN_DEBUG_VERBOSE(ex) ex
#else
#define DEBUG_LOG_THRESH 0
#define DEBUG_LOG(arg...)
#define WHEN_DEBUG_VERBOSE(ex)
#endif  // EVAL_DEBUG_VERBOSE

#if MACPORT
#include "./fasttime.h"
#endif
void debug_log(int log_level, const char *str, ...);
double  milliseconds();
uint64_t myrand();

#endif  // UTIL_H
