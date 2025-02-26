#include<stdlib.h>
#include<string.h>
#include "strbuff.h"
#include "logger.h"
#include "errors.h"

size_t strbuffsize = 0;

void arcSetDefaultStrBuffSize(size_t buffsize) {
   strbuffsize = buffsize;
}

int arcStrBuffInit(arc_str_buffer_t *buff, size_t size) {
   ARC_CHECK_NULL(buff, ARC_ERR_NULL)

   if (size == 0) {
      size = strbuffsize;
   }
   
   buff->buffer = malloc(sizeof(char) * (size+1));
   buff->len = 0;
   buff->totalSize = size;

   if (buff->buffer == NULL) {
      logf_errno("%s: Failed to allocate buffer", __func__);
      return ARC_ERR_ALLOC;
   }

   buff->buffer[0] = '\0';
   return ARC_NO_ERRORS;
}

char *arcStrBuffDupe(const arc_str_buffer_t *buff) {
   ARC_CHECK_NULL(buff, NULL)
   return strdup(buff->buffer);
}

char *arcStrBuffSubstr(const arc_str_buffer_t *buff, long start, size_t length) {
   ARC_CHECK_NULL(buff, NULL)
   ARC_CHECK_RANGE(start, -buff->len, buff->len-1, NULL)
   ARC_CHECK_RANGE(length, 0, buff->len, NULL)
   
   if (start < 0) start += buff->len;
   if (length == 0) length = buff->len - start;

   char *str = malloc((length + 1) * sizeof(char));
   if (str == NULL) {
      logf_errno("%s: Failed to allocate substring", __func__);
      log_line();
      return NULL;
   }
   
   strncpy(str, buff->buffer + start, length);
   str[length] = '\0';

   return str;
}

int arcStrBuffReserve(arc_str_buffer_t *buff, size_t bytes) {
   ARC_CHECK_NULL(buff, ARC_ERR_NULL)
   if (bytes <= buff->totalSize) {
      return ARC_NO_ERRORS;
   }

   size_t size = buff->totalSize + 1;
   while (size < bytes) {
      size *= 2;
   }

   char *rbuff = realloc(buff->buffer, size * sizeof(char));
   if (rbuff == NULL) {
      logf_errno("%s: Failed to reallocate buffer", __func__);
      log_line();
      return ARC_ERR_ALLOC;
   }
   buff->buffer = rbuff;
   buff->totalSize = size - 1;

   return ARC_NO_ERRORS;
}

int arcStrBuffAppend(arc_str_buffer_t *buff, const char *str) {
   ARC_CHECK_NULL(buff, ARC_ERR_NULL)
   if (str == NULL) { // Do nothing on null string
      return ARC_NO_ERRORS;
   }

   size_t n = strlen(str);
   int err = arcStrBuffReserve(buff, buff->len + n);
   if (err != ARC_NO_ERRORS) {
      log_line();
      return err;
   }

   strcpy(buff->buffer + buff->len, str);
   buff->len += n;

   return ARC_NO_ERRORS;
}

void arcStrBuffDestroy(arc_str_buffer_t *buff) {
   ARC_CHECK_NULL(buff,)
   if (buff->buffer != NULL) {
      free(buff->buffer);
   }
   
   buff->buffer = NULL;
   buff->len = 0;
   buff->totalSize = 0;
}
