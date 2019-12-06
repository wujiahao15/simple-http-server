#ifndef __LOG_H__
#define __LOG_H__

/**
 * @file logger.h
 * @brief File that logs information of INFO, DEBUG and ERROR.
 * @author Wu Jiahao
 * @version evhttp-version
 * @date 2019/12/06
 */

#include <stdio.h>
#include <errno.h>
#include <string.h>

#define LOG_DEBUG

/// logger level (DEBUG, INFO, WARNING and ERROR)
enum log_level { DEBUG = 0, INFO, WARNING, ERROR };

/// default logger level
static enum log_level this_log_level = DEBUG;

/// logger level string corresponding to their enum
static const char* log_level_str[] = {"DEBUG", "INFO", "WARNING", "ERROR"};

#ifdef LOG_DEBUG
/// logger core function
#define log_it(output, fmt, level_str, ...)                                 \
    fprintf(output, "[%s:%u] %s: " fmt "\n", __FILE__, __LINE__, level_str, \
            ##__VA_ARGS__);
#else
#define log_it(output, fmt, level_str, ...)                         \
    if (strcmp(level_str, "DEBUG")) {                               \
        fprintf(output, "%s: " fmt "\n", level_str, ##__VA_ARGS__); \
    }
#endif

/// logger main function
#define logger(level, fmt, ...)                                       \
    do {                                                              \
        if (level == ERROR)                                           \
            log_it(stderr, fmt, log_level_str[level], ##__VA_ARGS__); \
        if (level < ERROR && level >= this_log_level)                 \
            log_it(stdout, fmt, log_level_str[level], ##__VA_ARGS__); \
    } while (0);

#endif
