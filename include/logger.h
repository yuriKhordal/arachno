#ifndef __LOGGER_H__
#define __LOGGER_H__

#include<stdio.h>

enum LOG_LEVEL {
   LOG_DEBUG = 0, LOG_INFO = 1, LOG_WARNING = 2, LOG_ERROR = 4, LOG_NOTHING = 5
};

// #define log_line() fprintf(stderr, "\tAt %s:%s line %d.\n", __FILE__, __func__, __LINE__)
// #define log_line() logff("\tAt %s:%s line %d.\n", __FILE__, __func__, __LINE__)

/** Log the source file, function and line of an error.
 * Userfull for error stack tracing.
*/
#define log_line() __log_line(__FILE__, __func__, __LINE__)

// logf with the appropriate levels.

#define logf_debug(...) logf(LOG_DEBUG, __VA_ARGS__)
#define logf_info(...) logf(LOG_INFO, __VA_ARGS__)
#define logf_warning(...) logf(LOG_WARNING, __VA_ARGS__)
#define logf_error(...) logf(LOG_ERROR, __VA_ARGS__)

/** Set up a logger.
 * @param stream The file/stream to write the logs into.
 * @param level The minimal level of logs to write. Logging a message of a
 * lower level will just not be written out.
 * @param stdstreams Whether to also write into the standard streams,
 * LOG_DEBUG and LOG_INFO into stdout, and LOG_WARNING and LOG_ERROR into stderr.
 * 
*/
void setLogger(FILE *stream, int level, int stdstreams);

/** Set the level of logs to write. Logging a message of a
 * lower level will just not be written out.
 * @param level The minimal level to log.
*/
void setLogLevel(int level);

/** Log a formatted message into the logger with the same formatting syntax and
 * rules as printf.
 * @param level The level of the message to log. If the level is lower than the
 * logger level the message will not be logged.
 * @param format The message to write.
 * @param ... The format values.
*/
void logf(int level, const char format, ...);

/** Force log a formatted message into the logger with the same formatting syntax
 * and rules as printf regardless of logger level.
 * @param format The message to write.
 * @param ... The format values.
*/
void logff(const char format, ...);

/** Log the current errno string after a formatted message into the logger with
 * the same formatting syntax and rules as printf at LOG_ERROR level.
 * Similar to perror().
 * @param format The message to write.
 * @param ... The format values.
*/
void logf_errno(const char format, ...);

void __log_line(const char *file, const char* func, int line);

#endif