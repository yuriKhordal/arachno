#include <stddef.h>
#ifndef __STR_BUFF_H__
#define __STR_BUFF_H__

/*
 * Note to self:
 * Add functions as needed, do not add functions before you know they're
 * needed "just in case".
 */

/** The default starter buffer size if size was not specified, and if the
 * default was not overriden with arcSetDefaultStrBuffSize.
 */
#define ARC_STR_BUFF_DEFAULT_SIZE 128

/** Represents an auto-resizing string buffer. */
typedef struct arc_str_buffer {
   /** The actual string buffer */
   char *buffer;
   /** The length of the current string(excluding the null terminator). */
   size_t len;
   /** The allocated size of the string(excluding the null terminator, so -1) */
   size_t totalSize;
} arc_str_buffer_t;

#if defined(ARC_NO_PREFIX) || defined(ARC_STR_BUFF_NO_PREFIX)
   #define str_buffer arc_str_buffer
   typedef arc_str_buffer_t str_buffer_t;

   #define setDefaultStrBuffSize(buffsize) arcSetDefaultStrBuffSize(buffsize)
   #define strBuffInit(buff, size) arcStrBuffInit(buff, size)
   #define strBuffGetPtr(buff) arcStrBuffGetPtr(buff)
   #define strBuffDupe(buff) arcStrBuffDupe(buff)
   #define strBuffSubstr(buff, start, length) arcStrBuffSubstr(buff, start, length)
   #define strBuffReserve(buff, size) arcStrBuffReserve(buff, size)
   #define strBuffAppend(buff, str) arcStrBuffAppend(buff, str)
   #define strBuffDestroy(buff) arcStrBuffDestroy(buff)
#endif

/**
 * Set the starter size for arc_str_buffer when initialized with size 0
 * (default size), default value is ARC_STR_BUFF_DEFAULT_SIZE.
 * @note Does not check for size 0.
 */
void arcSetDefaultStrBuffSize(size_t buffsize);

/**
 * Initialize a buffer object, allocating a new string of size `size` (not
 * including the null terminator).
 * @return 0 on succeccfull intialization, a negative error code on failure.
 * @exception ARC_ERR_ALLOC - on failure to allocate a string.
 * @exception ARC_ERR_NULL - if buff param is null.
 */
int arcStrBuffInit(arc_str_buffer_t *buff, size_t size);

/** The buffer's string pointer. */
#define arcStrBuffPtr(buff) (buff->buffer)

/**
 * Return a newly allocated string which is a copy of the buffer's string.
 * The string's allocated size is the buffer's length(+1 for null terminator).
 * @note Ownership of the string is transfered to the caller.
 * @note Currently just uses strdup on the buffer.
 * @exception Returns NULL on allocation error.
 * @exception Returns NULL on a NULL parameter.
 */
char *arcStrBuffDupe(const arc_str_buffer_t *buff);

/**
 * Return a newly allocated string which is a substring of the buffer's string,
 * from a starting index and of a certain length. If the starting index is
 * negative, count from the end of the string. If the length is 0, copy until
 * the end of the string.
 * 
 * Examples: For buffer "Hello World!"
 * arcStrBuffSubstr(buff, 3, 4) -> "lo W"
 * arcStrBuffSubstr(buff, -5, 3) -> "orl"
 * arcStrBuffSubstr(buff, 2, 0) -> "llo World!"
 * arcStrBuffSubstr(buff, -6, 0) -> "World!"
 * 
 * @note Ownership of the string is transfered to the caller.
 * @exception Returns NULL on allocation error.
 * @exception Returns NULL on a NULL parameter.
 */
char *arcStrBuffSubstr(const arc_str_buffer_t *buff, long start, size_t length);

/**
 * Resize the internal buffer to at least `size` bytes.
 * @return 0 on succeccfull intialization, a negative error code on failure.
 * @exception ARC_ERR_ALLOC - on failure to reallocate a string.
 * @exception ARC_ERR_NULL - if buff param is null.
 */
int arcStrBuffReserve(arc_str_buffer_t *buff, size_t size);

/**
 * Append str to the buffer.
 * @return 0 on succeccfull intialization, a negative error code on failure.
 * @exception ARC_ERR_ALLOC - on failure to reallocate a string.
 * @exception ARC_ERR_NULL - if buff param is null.
 */
int arcStrBuffAppend(arc_str_buffer_t *buff, const char *str);

/** Free the buffer's string. */
void arcStrBuffDestroy(arc_str_buffer_t *buff);

#endif