#include "../include/logger.h"

static LogLevel current_level;
static FILE *log_file = NULL;

static void create_log_directory(const char *path) {
  char *path_copy = strdup(path);
  if (path_copy == NULL) {
    perror("strdup failed");
    return;
  }
  char *dir = dirname(path_copy);
  if (dir && strcmp(dir, ".") != 0) {
    mkdir(dir, 0755);
  }
  free(path_copy);
}

void logger_init(LogLevel level, const char *log_path) {
  current_level = level;
  if (log_path) {
    create_log_directory(log_path);
    log_file = fopen(log_path, "a");
    if (!log_file) {
      perror("Failed to open log file");
    }
  }
}

static const char *level_to_string(LogLevel level) {
  switch (level) {
  case LOG_LEVEL_DEBUG:
    return "DEBUG";
  case LOG_LEVEL_INFO:
    return "INFO";
  case LOG_LEVEL_WARN:
    return "WARN";
  case LOG_LEVEL_ERROR:
    return "ERROR";
  case LOG_LEVEL_FATAL:
    return "FATAL";
  }
  return "UNKNOWN";
}

void logger_log(LogLevel level, const char *file, int line, const char *fmt,
                ...) {
  if (level < current_level) {
    return;
  }

  time_t t = time(NULL);
  struct tm *lt = localtime(&t);
  char time_str[20];
  strftime(time_str, sizeof(time_str), "%Y-%m-%d %H:%M:%S", lt);

  char log_message[1024];
  va_list args;
  va_start(args, fmt);
  vsnprintf(log_message, sizeof(log_message), fmt, args);
  va_end(args);

  const char *level_str = level_to_string(level);

  fprintf(stderr, "[%s] [%-5s] [%s:%d] %s\n", time_str, level_str, file, line,
          log_message);

  if (log_file) {
    fprintf(log_file, "[%s] [%-5s] [%s:%d] %s\n", time_str, level_str, file,
            line, log_message);
    fflush(log_file);
  }

  if (level == LOG_LEVEL_FATAL) {
    if (log_file) {
      fclose(log_file);
    }
    exit(EXIT_FAILURE);
  }
}
