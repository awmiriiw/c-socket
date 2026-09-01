#ifndef LOGGER_H
#define LOGGER_H

#include <libgen.h>
#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <time.h>

typedef enum {
  LOG_LEVEL_DEBUG,
  LOG_LEVEL_INFO,
  LOG_LEVEL_WARN,
  LOG_LEVEL_ERROR,
  LOG_LEVEL_FATAL
} LogLevel;

void logger_log(LogLevel level, const char *file, int line, const char *fmt,
                ...);
void logger_init(LogLevel level, const char *log_path);

#define __FILENAME__                                                           \
  (strrchr(__FILE__, '/') ? strrchr(__FILE__, '/') + 1 : __FILE__)

#define log_debug(...)                                                         \
  logger_log(LOG_LEVEL_DEBUG, __FILENAME__, __LINE__, __VA_ARGS__)
#define log_info(...)                                                          \
  logger_log(LOG_LEVEL_INFO, __FILENAME__, __LINE__, __VA_ARGS__)
#define log_warn(...)                                                          \
  logger_log(LOG_LEVEL_WARN, __FILENAME__, __LINE__, __VA_ARGS__)
#define log_error(...)                                                         \
  logger_log(LOG_LEVEL_ERROR, __FILENAME__, __LINE__, __VA_ARGS__)
#define log_fatal(...)                                                         \
  logger_log(LOG_LEVEL_FATAL, __FILENAME__, __LINE__, __VA_ARGS__)

#endif
