#ifndef __HTTP_H__
#define __HTTP_H__

#include<stdio.h>

// ==================== Constants/Defines ====================

/** The size of the buffer for recieving requests. */
#define REQUEST_BUFF_SIZE (8*1024)
/** Max size of a response header in this server. */
#define REQUEST_HEADER_SIZE 1024
/** Max size of a response header in this server. */
#define RESPONSE_HEADER_SIZE 1024
/** Max size of an HTTP header in this server. */
#define HTTP_HEADER_SIZE (4*1024)

// ==================== Structs ====================

/** Represents an http method, like GET, POST, DELETE, etc'. */
enum http_method {
	HTTP_GET, HTTP_HEAD, HTTP_POST, HTTP_PUT, HTTP_DELETE, HTTP_CONNECT,
	HTTP_OPTIONS, HTTP_TRACE, HTTP_PATCH
};

/** Represents a version of the http protocol. */
enum http_version {
	HTTP_10, HTTP_11, HTTP_2, HTTP_3
};

enum http_status {
HTTP_STATUS_100 = 100, HTTP_STATUS_101 = 101, HTTP_STATUS_102 = 102, HTTP_STATUS_103 = 103,

HTTP_STATUS_200 = 200, HTTP_STATUS_201 = 201, HTTP_STATUS_202 = 202, HTTP_STATUS_203 = 203,
HTTP_STATUS_204 = 204, HTTP_STATUS_205 = 205, HTTP_STATUS_206 = 206, HTTP_STATUS_207 = 207,
HTTP_STATUS_208 = 208, HTTP_STATUS_226 = 226,

HTTP_STATUS_300 = 300, HTTP_STATUS_301 = 301, HTTP_STATUS_302 = 302, HTTP_STATUS_303 = 303,
HTTP_STATUS_304 = 304, HTTP_STATUS_305 = 305, HTTP_STATUS_306 = 306, HTTP_STATUS_307 = 307,
HTTP_STATUS_308 = 308,

HTTP_STATUS_400 = 400, HTTP_STATUS_401 = 401, HTTP_STATUS_402 = 402, HTTP_STATUS_403 = 403,
HTTP_STATUS_404 = 404, HTTP_STATUS_405 = 405, HTTP_STATUS_406 = 406, HTTP_STATUS_407 = 407,
HTTP_STATUS_408 = 408, HTTP_STATUS_409 = 409, HTTP_STATUS_410 = 410, HTTP_STATUS_411 = 411,
HTTP_STATUS_412 = 412, HTTP_STATUS_413 = 413, HTTP_STATUS_414 = 414, HTTP_STATUS_415 = 415,
HTTP_STATUS_416 = 416, HTTP_STATUS_417 = 417, HTTP_STATUS_418 = 418, HTTP_STATUS_421 = 421,
HTTP_STATUS_422 = 422, HTTP_STATUS_423 = 423, HTTP_STATUS_424 = 424, HTTP_STATUS_425 = 425,
HTTP_STATUS_426 = 426, HTTP_STATUS_428 = 428, HTTP_STATUS_429 = 429, HTTP_STATUS_431 = 431,
HTTP_STATUS_451 = 451,

HTTP_STATUS_500 = 500, HTTP_STATUS_501 = 501, HTTP_STATUS_502 = 502, HTTP_STATUS_503 = 503,
HTTP_STATUS_504 = 504, HTTP_STATUS_505 = 505, HTTP_STATUS_506 = 506, HTTP_STATUS_507 = 507,
HTTP_STATUS_508 = 508, HTTP_STATUS_510 = 510, HTTP_STATUS_511 = 511
};

/** Represents an HTTP header's `name:value` pairs. */
typedef struct http_header {
	/** An array of the http field names. */
	char **fields;
	/** An array of the values of the http fields. */
	char **values;
	/** The amount of http fields in the header. */
	size_t fieldCount;
	/** The size of the `fields` and `values` arrays. */
	size_t arraySize;
} http_header_t;

/**Represents an HTTP request.*/
typedef struct http_request {
	enum http_method method;
	char *path;
	size_t path_len;
	char *query;
	size_t query_len;
	enum http_version version;
	struct http_header header;
	char *body;
	size_t bodysize;
} http_request_t;

typedef struct http_response {
	enum http_version version;
	enum http_status status;
	struct http_header header;
	char *body;
	size_t bodysize;
} http_response_t;

// ==================== Request Functions ====================

/** Read a request from the client, and fill an http_server struct with the
 * recieved data.
 * @return An http_request structure that represents the request.
*/
int readRequest(int clientsock, http_request_t *request);

/** Handle an incoming http request.
 * @param clntsock The socket of the currently connected client that sent the request.
 * @param req The http request string.
 * @param n The size of the request.
*/
void handleRequest(int clntsock, const http_request_t *req);

/** Send a response header to a specified socket.
 * Does not send the http header's `key:value` pairs.
 * @param socket A socket descriptor to send the header to.
 * @param status The status message of the response.
 * @param flags See send().
 * @return The number of bytes sent. On error -1 is returned and errno set to
 * indicate the error.
*/
ssize_t sendResponseHeader(int socket, const char *status, int flags);

/** Send an http header's `key:value` pairs to a specified socket.
 * @param socket A socket descriptor to send the header to.
 * @param header The header values.
 * @param flags See send().
 * @return The number of bytes sent. On error -1 is returned and errno set to
 * indicate the error.
*/
ssize_t sendHeaderValues(int socket, const http_header_t *header, int flags);

// ==================== Header Functions ====================

/**
 * Allocate a new header struct, initialize it, and return a pointer to it.
 * @note Caller takes ownership of the new pointer.
 * @note
 * Must be freed with `headerFree`, not `headerDestroy`. Destroy function
 * only releases memory inside the struct, without the pointer to the struct
 * itself.
 * @exception Return `NULL` if failed to allocate.
 */
http_header_t * headerNew();

/**
 * Initialize a header struct.
 * @note
 * Must be destroyed with `headerDestroy()`, not `headerFree()`. Free function
 * releases the memory used by the header AND the struct header itself, so if
 * the header was allocated on the stack, the behaviour of `headerFree()` would
 * be undefined.
 */
void headerInit(http_header_t *header);

/**
 * Release the memory used by the header struct, but not the struct itself!
 * @note
 * The complementory function to `headerInit`. For pointers allocated by
 * `headerNew`, use `headerFree`.
 */
void headerDestroy(http_header_t *header);

/**
 * Release the memory used by the header struct, and the struct itself.
 * @note
 * The complementory function to `headerNew`. For stack allocated headers
 * initialized by `headerInit`, use `headerDestroy`.
 */
void headerFree(http_header_t *header);

/**
 * Adds a `name:value` pair to a header struct. If a pair with the same name
 * is already in the header struct, replaces the value in the struct with the
 * parameter.
 * The name and value strings are copied as new strings, and the struct
 * owns it's own memory.
 * @returns 0 if successfully added, 1 if replaced existing value, and a
 * negative value on failure.
 * @exception returns (some negative) if one of the parameters is NULL.
 * @exception returns (some negative) on a memry allocation error.
 */
int headerAdd(http_header_t *header, const char *name, const char *value);

/**
 * Searches for a field in a header struct by name, and returns it's index or
 * -1 if the field was not found.
 * @exception Returns -1 if the header or name pointers are `NULL`.
 */
ssize_t headerFind(const http_header_t *header, const char *name);

/**
 * Gets a field in a header struct by name, and returns it's value or
 * NULL if the field was not found.
 * @exception Returns `NULL` if the header or name pointers are `NULL`.
 */
const char* headerGetByName(const http_header_t *header, const char *name);

/**
 * Gets a field in a header struct by index, and returns it's value or
 * NULL if the field was not found.
 * @exception Returns `NULL` if the header pointer is `NULL` or if
 * the index is out of range.
 */
const char* headerGetByIndex(const http_header_t *header, int index);

/**
 * Gets a field in a header struct by index, and returns it's name or
 * NULL if the field was not found.
 * @exception Returns `NULL` if the header pointer is `NULL` or if
 * the index is out of range.
 */
const char* headerGetNameByIndex(const http_header_t *header, int index);

/**
 * Returns the amount of fields in the header struct.
 * @exception Returns -1 if the header pointer is `NULL`.
 */
ssize_t headerLength(const http_header_t *header);

#endif