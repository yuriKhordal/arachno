#ifndef __SERVER_H__
#define __SERVER_H__

#include<stdio.h>
#include<magic.h>

// ==================== Constants/Defines ====================

struct http_request;
struct http_response;
struct htmlf_context;

#define LISTEN_BACKLOG 1024

/** A function pointer to an on_request event handler function that resolves an event.
 * @param sock The socket of the client who sent the request.
 * @param req A pointer to a struct containing the details of the request.
 * @return Return to mark the request as resolved, 0 to let the request
 * fall-through to the next handler, and a negative number to stop due to error.
*/
typedef int(*on_request)(int, const struct http_request *);
// typedef int(*on_request)(int sock, const struct http_request *req);

typedef int(*htmlf_load)(int sock, const struct htmlf_context *ctx);

// ==================== Variables ====================

/**A magic cookie pointer, for getting mime types from files.*/
extern magic_t magicfd;

/**The socket the server is listening on.*/
extern int sockfd;

// ==================== Functions ====================

/** Set up arachno to serve static files.
 * @return ARC_NO_ERRORS On success, a negative error code on failure.
 */
int serveStaticFiles();

/**
 * Setup a request handler to handle 404 errors by serving the file at `path`.
 * @param path A path to an html or htmlf file.
 * @return ARC_NO_ERRORS On success, a negative error code on failure.
 */
int set404Page(const char *path);

/** Register a new request handler.
 * On an incoming request, it will be passed over to the registered handlers
 * in the same order as registered, and the return value of each handler will
 * dictate whether the request will be passed to the next handler or whether
 * the request will be considered as responded to.
 * @param func A function pointer to the handler.
 * @return ARC_NO_ERRORS On success, a negative error code on failure.
 */
int registerRequest(on_request func);

/** Remove a previously registered request handler.
 * @param func A function pointer to the handler.
 */
void unregisterRequest(on_request func);

/** Finds the actual path of a file from a URL path.
 * Prepends the website's base dir and if the file is a directory, appends the index file.
 * Does not guarantee that the file actually exists(For 404 reasons).
 * For example "/" could return "www/html/index.html".
 * @param buff (Optional) A buffer into which to write the path.
 * If `NULL`, a new string will be allocated and returned.
 * @param path The url path to resolve into a "real" path.
 * @return Returns the resolved path string. If `buff` was not `NULL`,
 * returns `buff`, if `buff` was `NULL` returns a newly allocated string.
 * On error returns `NULL` and prints an error message to stderr.
 * @exception If allocating a new string failed.
 */
char * resolvePath(char *buff, const char *path);

// res can be NULL for "default" response structure.
int serveFile(int sock, const char *path, const struct http_response *res);


#endif