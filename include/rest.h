#ifndef __REST_H__
#define __REST_H__

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

/** Represents an HTTP header's `name:value` pairs. */
struct http_header {
	/** An array of the http field names. */
	char **fields;
	/** An array of the values of the http fields. */
	char **values;
	/** The amount of http fields in the header. */
	size_t fieldCount;
};

/**Represents an HTTP request.*/
struct http_request {
	enum http_method method;
	char *path;
	size_t path_len;
	char *query;
	size_t query_len;
	enum http_version version;
	struct http_header header;
	char *body;
	size_t bodysize;
};

// ==================== Functions ====================

/** Read a request from the client, and fill an http_server struct with the
 * recieved data.
 * @return An http_request structure that represents the request.
*/
int readRequest(int clientsock, struct http_request *request);

/** Handle an incoming http request.
 * @param clntsock The socket of the currently connected client that sent the request.
 * @param req The http request string.
 * @param n The size of the request.
*/
void handleRequest(int clntsock, const char *req, size_t n);
/** Handle a GET request.
 * @param clntsock The socket of the currently connected client that sent the request.
 * @param req The GET request string.
 * @param n The size of the request.
*/
void handleGetRequest(int clntsock, const char *req, size_t n);

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
ssize_t sendHeaderValues(int socket, const struct http_header *header, int flags);

/** Checks wheather a header fields list has a specified field.
 * @param list The list of fields. The list is assumed to be sorted
 * alphabetically with the names in lower case.
 * @param name The name of the field to search for, case irrelevant.
 * @return True(1) if the field exists, False(0) otherwise.
*/
int headerHasField(const struct http_header *list, const char *name);

/** Returns the value of a field from a header fields list.
 * @param list The list of fields.
 * @param name The name of the field to search for.
 * @return A pointer to the value of the field, or NULL if the field is not found.
*/
const char *headerGetFieldValue(const struct http_header *list, const char *name);

#endif