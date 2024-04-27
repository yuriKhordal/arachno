#include<stdio.h>
#include<sys/file.h>
#include<stdarg.h>
#include<time.h>
#include<errno.h>
#include<string.h>
#include "logger.h"

FILE *logger = NULL;
int logfd = -1;
int log_level = LOG_INFO;
int logtostd = 1;

void setLogger(FILE *stream, int level, int stdstreams) {
   logger = stream;
   if (stream) logfd = fileno(stream);
   else logfd = -1;
   log_level = level;
   logtostd = stdstreams;
}

void setLogLevel(int level) {
   log_level = level;
}

void loggerf(int level, const char *format, ...) {
   if (level < log_level) return;

   va_list args, args2;
   va_start(args, format);
   va_copy(args2, args);

   time_t timer;
   time(&timer);
   char timestr[80] = {0};
   strftime(timestr, 80, "%c", localtime(&timer));

   char *lvlstr = "";
   switch (level) {
   case LOG_DEBUG: lvlstr = "DEBUG"; break;
   case LOG_INFO: lvlstr = "INFO"; break;
   case LOG_WARNING: lvlstr = "WARNING"; break;
   case LOG_ERROR: lvlstr = "ERROR"; break;
   }

   // ========== LOCK stderr ==========
   if (flock(fileno(stderr), LOCK_EX) == -1) {
      // Great idea, can't lock stderr so lets write to it.
      // But I prefer chance for jumbled text that at least shows someting went
      // wrong instead of silently failing.
      fprintf(stderr, "[WARNING %s] Failed to lock stderr: %s\n", timestr, strerror(errno));
   }
   if (logtostd) {
      // ========== LOCK stdout ==========
      if (flock(fileno(stdout), LOCK_EX) == -1) {
         fprintf(stderr, "[WARNING %s] Failed to lock stdout: %s\n", timestr, strerror(errno));
      }
      fprintf(stdout, "[%s %s] ", lvlstr, timestr);
      vfprintf(level < LOG_WARNING ? stdout : stderr, format, args);
      // ========== UNLOCK stdout ==========
      if (flock(fileno(stdout), LOCK_UN) == -1) {
         fprintf(stderr, "[WARNING %s] Failed to unlock stdout: %s\n", timestr, strerror(errno));
      }
   }

   if (logger) {
      // ========== LOCK logger ==========
      if (flock(logfd, LOCK_EX) == -1) {
         fprintf(stderr, "[WARNING %s] Failed to lock log file: %s\n", timestr, strerror(errno));
      }
      fprintf(logger, "[%s %s] ", lvlstr, timestr);
      vfprintf(logger, format, args2);
      // ========== UNLOCK logger ==========
      if (flock(logfd, LOCK_UN) == -1) {
         fprintf(stderr, "[WARNING %s] Failed to lock log file: %s\n", timestr, strerror(errno));
      }
   }
   // ========== UNLOCK stderr ==========
   if (flock(fileno(stderr), LOCK_UN) == -1) {
      fprintf(stderr, "[WARNING %s] Failed to unlock stderr: %s\n", timestr, strerror(errno));
   }
}

void logff(const char *format, ...) {
   va_list args, args2;
   va_start(args, format);
   va_copy(args2, args);

   if (logtostd) {
      // ========== LOCK stdout ==========
      if (flock(fileno(stdout), LOCK_EX) == -1) {
         fprintf(stderr, "[WARNING] Failed to lock stdout: %s\n", strerror(errno));
      }
      vfprintf(stdout, format, args);
      // ========== UNLOCK stdout ==========
      if (flock(fileno(stdout), LOCK_UN) == -1) {
         fprintf(stderr, "[WARNING] Failed to unlock stdout: %s\n", strerror(errno));
      }
   }
   if (logger) {
      // ========== LOCK logger ==========
      if (flock(logfd, LOCK_EX) == -1) {
         fprintf(stderr, "[WARNING] Failed to lock log file: %s\n", strerror(errno));
      }
      vfprintf(logger, format, args2);
      // ========== UNLOCK logger ==========
      if (flock(logfd, LOCK_UN) == -1) {
         fprintf(stderr, "[WARNING] Failed to unlock log file: %s\n", strerror(errno));
      }
   }
}

void logf_errno(const char *format, ...) {
   va_list args, args2;
   va_start(args, format);
   va_copy(args2, args);
   time_t timer;
   time(&timer);
   char timestr[80] = {0};
   strftime(timestr, 80, "%c", localtime(&timer));

   if (logtostd) {
      // ========== LOCK stderr ==========
      if (flock(fileno(stderr), LOCK_EX) == -1) {
         fprintf(stderr, "[WARNING %s] Failed to lock stderr: %s\n", timestr, strerror(errno));
      }
      fprintf(stderr, "[ERROR %s] ", timestr);
      vfprintf(stderr, format, args);
      fprintf(stderr, ": %s\n", strerror(errno));
      // ========== UNLOCK stderr ==========
      if (flock(fileno(stderr), LOCK_UN) == -1) {
         fprintf(stderr, "[WARNING %s] Failed to unlock stderr: %s\n", timestr, strerror(errno));
      }
   }
   if (logger) {
      // ========== LOCK logger ==========
      if (flock(logfd, LOCK_EX) == -1) {
         fprintf(stderr, "[WARNING %s] Failed to lock log file: %s\n", timestr, strerror(errno));
      }
      fprintf(logger, "[ERROR %s] ", timestr);
      vfprintf(logger, format, args2);
      fprintf(logger, ": %s\n", strerror(errno));
      // ========== UNLOCK logger ==========
      if (flock(logfd, LOCK_UN) == -1) {
         fprintf(stderr, "[WARNING %s] Failed to unlock log file: %s\n", timestr, strerror(errno));
      }
   }
}

void __log_line(const char *file, const char* func, int line) {
   if (logger) fprintf(logger, "\tAt %s:%s line %d.\n", file, func, line);
   if (logtostd) fprintf(stderr, "\tAt %s:%s line %d.\n", file, func, line);
}