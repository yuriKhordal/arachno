#ifndef __SERVER_H__
#define __SERVER_H__

#include<stdio.h>
#include<magic.h>
#include<netinet/in.h>

// ==================== Constants/Defines ====================

#define LISTEN_BACKLOG 1024

// ==================== Variables ====================

/**A magic cookie pointer, for getting mime types from files.*/
extern magic_t magicfd;

/**The socket the server is listening on.*/
extern int sockfd;

// Configurations:
// NOTE: Will be read from .cfg file in in the future.
/**Local ip of the server to listen on.*/
extern struct in_addr cfg_local_ip;
/**Local port of the server to listen on.*/
extern in_port_t cfg_local_port;
/**The default index file for when url path is a dir.*/
extern char *cfg_default_index;
/**Length of cfg_default_index.*/
extern size_t cfg_default_index_len;
/**The base dir of the website.*/
extern char *cfg_base_path;
/**Length of cfg_base_path.*/
extern size_t cfg_base_path_len;

// ==================== Functions ====================

/** Finds the actual path of a file from a URL path.
 * Prepends the website's base dir and if the file is a directory, appends the index file.
 * Does not guarantee that the file actually exists(For 404 reasons).
 * For example "/" could return "www/html/index.html".
 * @param buff (Optional) A buffer into which to write the path.
 * If `NULL`, a new string will be allocated and returned.
 * @param n (Optional) As input, the size of the buffer,
 * as output, the amount of characters written into buffer/allocated string.
 * If `NULL`, the size of the buffer won't be checked for being too short,
 * and the length of the resolved path won't be written into `n`.
 * @param path The url path to resolve into a "real" path.
 * @return Returns the resolved path string. If `buff` was not `NULL`,
 * returns `buff`, if `buff` was `NULL` returns a newly allocated string.
 * On error returns `NULL` and prints an error message to stderr.
 * @exception If the size of the buffer is smaller than the resolved path.
 * @exception If allocating a new string failed.
 */
char * getPath(char *buff, size_t *n, const char *path);

#endif