#ifndef __REST_H__
#define __REST_H__

#include<stdio.h>

// ==================== Constants/Defines ====================

/**The size of the buffer for recieving requests.*/
#define REQUEST_BUFF_SIZE (8*1024)
/**Max size of a response header in this server.*/
#define REQUEST_HEADER_SIZE 1024
/**Max size of a response header in this server.*/
#define RESPONSE_HEADER_SIZE 1024
/**Max size of an HTTP header in this server.*/
#define HTTP_HEADER_SIZE (4*1024)

// ==================== Structs ====================

/**Represents an HTTP header's `key:value` pairs.*/
struct http_header {
	struct {
		enum {AUTH_NONE = 0, AUTH_BASIC/*, AUTH_DIGEST*/ } type;
		const char *const credentials;
	} authorization;

	const char *content_type;
	unsigned long content_length;

	const char *host;
	// TODO: FINISH.
};

// ==================== Functions ====================

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

#endif