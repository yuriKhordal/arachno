#include<stdio.h>
#include<stdlib.h>
#include<string.h>
#include<ctype.h>
#include<unistd.h>
#include<fcntl.h>
#include<sys/socket.h>
#include<sys/sendfile.h>
#include<sys/stat.h>
#include<sys/types.h>

#include "http.h"
#include "server.h"
#include "conf.h"
#include "status.h"
#include "misc.h"
#include "logger.h"
#include "errors.h"

// TODO: SEND 400 BAD REQUEST OR 411 LENGTH REQUIRED ON ERRORS

// First line of the request: METHOD /path?query
int readRequestLine(char *line, http_request_t *request);
int parseQString(char *qstr, http_query_t *query);
int readRequestHeader(char *line, http_header_t *header, size_t n);
ssize_t readAndCountHeaders(char **buffer, size_t *buffsize, ssize_t *msg_len, int clientsock);
int readBody(
	http_request_t *request, char **buffer, size_t buffsize,
	ssize_t msg_len, char *body, int clientsock
);

// ==================== Requset Functions ====================

int readRequest(int clientsock, http_request_t *request) {
	int err;
	// Initialize buffer.
	size_t buffsize = REQUEST_BUFF_SIZE;
	char *buffer = malloc(sizeof(char) * (buffsize + 1));
	if (buffer == NULL) {
		logf_errno("Failed to allocate buffer");
		log_line();
		return ARC_ERR_ALLOC;
	}
	buffer[buffsize] = '\0';

	// Recieve message.
	ssize_t msg_len = recv(clientsock, buffer, buffsize, 0);
	if (msg_len == -1) {
		logf_errno("Failed to recieve message");
		log_line();
		free(buffer);
		return ARC_ERR_SOCK;
	}
	buffer[msg_len] = '\0';

	// =============== Request Line ===============
	// Find the end of the request line.
	char *lineend = strstr(buffer, "\r\n");
	if (lineend == NULL) {
		// No end of line or line too long.
		logf_warning("Invalid request syntax. Dropping connection.\n");
		log_line();
		free(buffer);
		return ARC_ERR_SYNTAX;
	}

	// Read and parse request line into `request` struct.
	*lineend = '\0';
	if ((err = readRequestLine(buffer, request)) < 0) {
		log_line();
		free(buffer);
		return err;
	}
	*lineend = '\r';

	// =============== Headers ===============

	// Recieve all header fields and count them
	// But don't read/save them yet, only count
	ssize_t headCount = readAndCountHeaders(&buffer, &buffsize, &msg_len, clientsock);
	if (headCount < 0) {
		log_line();
		free(buffer);
		free(request->path);
		arcQueryDestroy(&request->query);
		return (int)headCount;
	}
	
	// Fill header list:
	char *header = lineend + 2;
	for (int i = 0; i < headCount; i++) {
		char *end = strstr(header, "\r\n");
		*end = '\0';
		if ((err = readRequestHeader(header, &request->headers, i)) < 0) {
			log_line();
			free(buffer);
			free(request->path);
			arcQueryDestroy(&request->query);
			headerDestroy(&request->headers);
			return err;
		}
		*end = '\r';
		
		header = end + 2;
	}

	// =============== Body ===============

	char *body = header + 2; // Skip the \r\n delimeter.
	err = readBody(request, &buffer, buffsize, msg_len, body, clientsock);
	if (err < 0) {
		log_line();
		free(buffer);
		free(request->path);
		arcQueryDestroy(&request->query);
		headerDestroy(&request->headers);
		return err;
	}

	// =============== Old Read Body ===============
	// const char *contentLength = headerGetByName(&request->headers, "Content-Length");
	// if (contentLength) {
	// 	// TODO: Check validity of string before converting.
	// 	request->bodysize = atol(contentLength);
	// } else {
	// 	request->bodysize = 0;
	// }
	//
	// // Read Data.
	// if (request->bodysize > 0) {
	// 	size_t header_size = body - buffer;
	// 	size_t current_body_size = msg_len - header_size;
	// 	// If not all body has been read.
	// 	if (request->bodysize > current_body_size) {
	// 		size_t diff = request->bodysize - current_body_size;
	// 		buffsize += diff;
	// 		char * rbuff = realloc(buffer, (buffsize+1) * sizeof(char));
	// 		if (rbuff == NULL) {
	// 			logf_errno("Failed to reallocate buffer");
	// 			log_line();
	// 			free(buffer);
	// 			free(request->path);
	// 			if (request->query) free(request->query);
	// 			headerDestroy(&request->headers);
	// 			return -1;
	// 		}
	// 		buffer = rbuff;
	// 		ssize_t new_msg_len = recv(clientsock, buffer + msg_len, diff, 0);
	// 		if (new_msg_len == -1) {
	// 			logf_errno("Failed to recieve message");
	// 			log_line();
	// 			free(buffer);
	// 			free(request->path);
	// 			if (request->query) free(request->query);
	// 			headerDestroy(&request->headers);
	// 			return -1;
	// 		}
	// 		msg_len += new_msg_len;
	// 		buffer[msg_len] = '\0';
	// 	}
	//
	// 	request->body = malloc(sizeof(char) * (request->bodysize + 1));
	// 	strncpy(request->body, body, request->bodysize);
	// 	request->body[request->bodysize] = '\0';
	// } else {
	// 	request->body = NULL;
	// }

	logf_debug("Data:\n%s\n", buffer);
	free(buffer);
	return 0;
}

int readRequestLine(char *line, http_request_t *request) {
	while (isspace(*line) && *line != '\0') line++;
	if (*line == '\0') {
		// Empty request line.
		logf_warning("Empty request line!\nDropping connection.\n", line);
		log_line();
		return ARC_ERR_SYNTAX;
	}

	size_t i;
	// Method
	for (i = 0; !isspace(line[i]); i++) {}
	if (strncmp(line, "GET", i) == 0) {
		request->method = HTTP_GET;
	} else if (strncmp(line, "HEAD", i) == 0) {
		request->method = HTTP_HEAD;
	} else if (strncmp(line, "POST", i) == 0) {
		request->method = HTTP_POST;
	} else if (strncmp(line, "PUT", i) == 0) {
		request->method = HTTP_PUT;
	} else if (strncmp(line, "DELETE", i) == 0) {
		request->method = HTTP_DELETE;
	} else if (strncmp(line, "CONNECT", i) == 0) {
		request->method = HTTP_CONNECT;
	} else if (strncmp(line, "OPTIONS", i) == 0) {
		request->method = HTTP_OPTIONS;
	} else if (strncmp(line, "TRACE", i) == 0) {
		request->method = HTTP_TRACE;
	} else if (strncmp(line, "PATCH", i) == 0) {
		request->method = HTTP_PATCH;
	} else {
		logf_warning("Unknown method %*s. Dropping connection.\n", i, line);
		log_line();
		return ARC_ERR_SYNTAX;
	}

	// Skip space
	while (isspace(line[i]) && line[i] != '\0') i++;
	if (line[i] == '\0') {
		// Missing path and version.
		logf_warning("Invalid request line: Missing path."
			"\n%s\nDropping connection.\n", line);
		log_line();
		return ARC_ERR_SYNTAX;
	}

	// Path
	char *path = line + i;
	for (i=0; !isspace(path[i]) && path[i] != '?' && path[i] != '\0'; i++) {}
	if (path[i] == '\0') {
		// Missing http version.
		logf_warning("Invalid request line: Missing HTTP version.\n"
			"%s\nDropping connection.\n", line);
		log_line();
		return ARC_ERR_SYNTAX;
	}
	request->path_len = i;
	request->path = malloc(sizeof(char) * (i + 1));
	if (request->path == NULL) {
		logf_errno("Failed to allocate path string");
		log_line();
		return ARC_ERR_ALLOC;
	}
	strncpy(request->path, path, i);
	request->path[i] = '\0';

	// Query String
	// char *query = path + i;
	// i = 0;
	// if (query[0] == '?') {
	// 	query++;
	// 	// for (i = 0; !isspace(query[i]) && query[i] != '\0'; i++) {}
	// 	while (!isspace(query[i]) && query[i] != '\0') i++;
	// 	request->query_len = i;
	// 	request->query = malloc(sizeof(char) * (i + 1));
	// 	if (request->query == NULL) {
	// 		logf_errno("Failed to allocate query string");
	// 		log_line();
	// 		free(request->path);
	// 		return ARC_ERR_ALLOC;
	// 	}
	// 	strncpy(request->query, query, i);
	// 	request->query[i] = '\0';
	// } else {
	// 	request->query = NULL;
	// 	request->query_len = 0;
	// }
	char *query = path + i;
	int err = parseQString(query, &request->query);
	if (err < 0) {
		log_line();
		free(request->path);
		return err;
	}

	// HTTP Version
	char *version = query + request->query.str_len;
	if (request->query.str_len > 0) version++; // +1 for the '?'
	while (isspace(*version) && *version != '\0') version++;
	if (*version == '\0') {
		// Missing http version.
		logf_warning("Invalid request line: Missing HTTP version.\n"
			"%s\nDropping connection.\n", line);
		log_line();
		free(request->path);
		arcQueryDestroy(&request->query);
		return ARC_ERR_SYNTAX;
	}
	
	if (strncmp(version, "HTTP/1.0", sizeof("HTTP/1.0")) == 0) {
		request->version = HTTP_10;
	} else if (strncmp(version, "HTTP/1.1", sizeof("HTTP/1.1")) == 0) {
		request->version = HTTP_11;
	} else if (strncmp(version, "HTTP/2", sizeof("HTTP/2")) == 0) {
		request->version = HTTP_2;
		logf_warning("HTTP/2 requests are unsupported.\n"
			"%s\nDropping connection.\n", line);
		log_line();
		free(request->path);
		arcQueryDestroy(&request->query);
		return ARC_ERR_UNSUPPORTED_HTTP_VER;
	} else if (strncmp(version, "HTTP/3", sizeof("HTTP/3")) == 0) {
		request->version = HTTP_3;
		logf_warning("HTTP/3 requests are unsupported.\n"
			"%s\nDropping connection.\n", line);
		log_line();
		free(request->path);
		arcQueryDestroy(&request->query);
		return ARC_ERR_UNSUPPORTED_HTTP_VER;
	} else {
		logf_warning("Unknown http version: %s.\n"
			"%s\nDropping connection.\n", version, line);
		log_line();
		free(request->path);
		arcQueryDestroy(&request->query);
		return ARC_ERR_SYNTAX;
	}

	return 0;
}

int parseQString(char *qstr, http_query_t *query) {
	// No query string
	if (qstr[0] != '?') {
		return 0;
	}
	// Empty query
	if (isspace(qstr[1]) || qstr[1] == '\0') {
		logf_warning("%s: Bad query string syntax: Nothing after '?'.\n"
			"Query: %s.\n", __func__, qstr);
		log_line();
		return ARC_ERR_SYNTAX;
	}

	// Allocate and copy query string:
	size_t len;
	for (len = 0; !isspace(qstr[len]) && qstr[len] != '\0'; len++) {}
	query->str_len = len;
	query->str = malloc(sizeof(char) * (len + 1));
	if (query->str == NULL) {
		logf_errno("%s: Failed to allocate query string", __func__);
		log_line();
		return ARC_ERR_ALLOC;
	}
	strncpy(query->str, qstr, len);
	query->str[len] = '\0';

	// Parse query string
	char* param = strtok_nd(query->str + 1, "&");
	while (param != NULL) {
		char *key = param;
		char *value = strchr(param, '=');
		
		//error
		if (value == NULL) {
			logf_warning("%s: Bad query string syntax: Missing '=' for param %s.\n"
			"Query: %s.\n", __func__, key, query->str);
			log_line();
			free(query->str);
			arcMapDestroy(&query->params);
			return ARC_ERR_SYNTAX;
		}

		// Split key and value into "two" strings.
		*(value++) = '\0';
		// error
		int err = arcMapPut(&query->params, key, value);
		if (err < 0) {
			log_line();
			free(query->str);
			arcMapDestroy(&query->params);
			return err;
		} 

		// Return string to original state and go to next param
		*(--value) = '=';
		param = strtok_nd(NULL, "&");
	}

	return 0;
}

int readRequestHeader(char *line, http_header_t *header, size_t n) {
	char *field = line;
	while (isspace(*field)) field++;

	char *sep = strchr(field, ':');
	if (sep == NULL) {
		logf_warning("Failed to parse header line:%s\n"
			"Missing ':', dropping connection.\n", line
		);
		log_line();
		return ARC_ERR_SYNTAX;
	}
	
	size_t field_len = sep - field;
	while (isspace(field[field_len - 1]) && field_len > 0) field_len--;
	if (field_len == 0) {
		logf_warning("Failed to parse header line:%s\n"
			"Empty field name, dropping connection.\n", line
		);
		log_line();
		return ARC_ERR_SYNTAX;
	}
	
	char *value = sep + 1;
	while (isspace(*value)) value++;
	size_t value_len = strlen(value);
	while (isspace(value[value_len - 1]) && value_len > 0) value_len--;
	if (value_len == 0) {
		logf_warning("Failed to parse header line:%s\n"
			"Empty field value, dropping connection.\n", line
		);
		log_line();
		return ARC_ERR_SYNTAX;
	}

	char ftmp = field[field_len];
	char vtmp = value[value_len];
	field[field_len] = '\0';
	value[value_len] = '\0';

	int err = headerAdd(header, field, value);
	if (err < 0) {
		log_line();
		return err;
	}

	field[field_len] = ftmp;
	value[value_len] = vtmp;
	return 0;
}

ssize_t readAndCountHeaders(char **buffer, size_t *buffsize, ssize_t *msg_len, int clientsock) {
	// Recieve all header fields and count them
	// But don't read/save them yet, only count
	ssize_t count = 0;
	char *header = strstr(*buffer, "\r\n") + 2; // skip first line
	while (1) {
		char *temp = strstr(header, "\r\n");
		// End of message, recieve next.
		if (temp == NULL) {
			*buffsize *= 2;
			char *rbuffer = realloc(*buffer, sizeof(char) * (*buffsize + 1));
			if (rbuffer == NULL) {
				logf_errno("Failed to reallocate buffer");
				log_line();
				return ARC_ERR_ALLOC;
			}
			*buffer = rbuffer;
			(*buffer)[*buffsize] = '\0';
			ssize_t new_msg_len = recv(clientsock, buffer + *msg_len, *buffsize - *msg_len, 0);
			if (new_msg_len == - 1) {
				logf_errno("Failed to recieve message");
				log_line();
				return ARC_ERR_SOCK;
			}
			*msg_len += new_msg_len;
			(*buffer)[*msg_len] = '\0';
			continue;
		}

		// while (isspace(header[0]) && header[0] != '\r') header++;
		if (header == temp) {
			break;
		}
		header = temp + 2;
		count++;
	}

	return count;
}

int readBody(
	http_request_t *request, char **buffer, size_t buffsize,
ssize_t msg_len, char *body, int clientsock) {
	const char *contentLength = headerGetByName(&request->headers, "Content-Length");
	if (contentLength) {
		// TODO: Check validity of string before converting.
		request->bodysize = atol(contentLength);
	} else {
		request->bodysize = 0;
	}

	// Read Data.
	if (request->bodysize > 0) {
		size_t header_size = body - *buffer;
		size_t current_body_size = msg_len - header_size;
		// If not all body has been read.
		if (request->bodysize > current_body_size) {
			size_t diff = request->bodysize - current_body_size;
			buffsize += diff;
			char * rbuff = realloc(*buffer, (buffsize+1) * sizeof(char));
			if (rbuff == NULL) {
				logf_errno("%s: Failed to reallocate buffer", __func__);
				log_line();
				return ARC_ERR_ALLOC;
			}
			*buffer = rbuff;
			ssize_t new_msg_len = recv(clientsock, *buffer + msg_len, diff, 0);
			if (new_msg_len == -1) {
				logf_errno("%s: Failed to recieve message", __func__);
				log_line();
				return ARC_ERR_SOCK;
			}
			msg_len += new_msg_len;
			(*buffer)[msg_len] = '\0';
		}

		request->body = malloc(sizeof(char) * (request->bodysize + 1));
		if (request->body == NULL) {
			logf_errno("%s: Failed to allocate body", __func__);
			log_line();
			return ARC_ERR_ALLOC;
		}
		strncpy(request->body, body, request->bodysize);
		request->body[request->bodysize] = '\0';
	} else {
		request->body = NULL;
	}

	return 0;
}

// ==================== Misc ====================

void arcQueryDestroy(http_query_t *query) {
	if (query->str_len > 0) {
		free(query->str);
		arcMapDestroy(&query->params);
	}
}

void requestInit(http_request_t *request) {
	request->body = NULL;
	request->bodysize = 0;
	arcMapInit(&request->headers);
	request->method = HTTP_GET;
	request->path = NULL;
	request->path_len = 0;
	arcMapInit(&request->query.params);
	request->query.str = NULL;
	request->query.str_len = 0;
	request->version = HTTP_10;
}

void requestDestroy(http_request_t *request) {
	if (request->body)
		free(request->body);
	arcMapDestroy(&request->headers);
	if (request->path)
		free(request->path);
	if (request->query.str)
		free(request->query.str);
	arcMapDestroy(&request->query.params);
}

// ==================== Legacy Functions ====================

void handleRequest(int clntsock, const http_request_t *req) {
	// Resolve path.
	char path[cfg_base_path_len + req->path_len + cfg_default_index_len + 1];
	if (resolvePath(path, req->path) == NULL) {
		perror_line();
		sendResponseHeader(clntsock, STATUS_500_STR, 0);
		return;
	}
	struct stat st;
	if (stat(path, &st) == -1 || !S_ISREG(st.st_mode)) {
		sendResponseHeader(clntsock, STATUS_404_STR, 0);
		// possible send 404 page.
		return;
	}
	// Get mime:
	const char *mime = magic_file(magicfd, path);
	if (strcmp(mime, "text/plain") == 0) {
		if (strendswith(path, ".css")) mime = "text/css";
		else if (strendswith(path, ".js")) mime = "text/javascript";
	}

	printf("[INFO] Resolved path is '%s'.\n", path);
	printf("[INFO] MIME type is '%s'.\n", mime);

	// Open file:
	int fd = open(path, O_RDONLY);
	if (fd == -1) {
		perror("[ERROR] Failed opening file");
		perror_line();
		fprintf(stderr, "\tFile: %s\n", path);
		sendResponseHeader(clntsock, STATUS_500_STR, 0);
		return;
	}

	// Send non html file:
	if (strcmp(mime, "text/html") != 0) {
		char st_size[21]={0}; sprintf(st_size, "%ld", st.st_size);
		char *fields[2] = { "Content-Type", "Content-Length"};
		char *values[2] = { (char*)mime, st_size };
		http_header_t header = {
			.elementCount = 2,
			.keys = fields,
			.values = values
		};
		// http_header_t header = {
		// 	.content_length = st.st_size,
		// 	.content_type = mime
		// };
		printf("[DEBUG] Sending Response...\n");
		sendResponseHeader(clntsock, STATUS_200_STR, 0);
		sendHeaderValues(clntsock, &header, 0);
		send(clntsock, "\n\n", 2, 0);
		sendfile(clntsock, fd, NULL, st.st_size);
		close(fd);
		printf("[DEBUG] Response sent...\n");
		return;
	}

	// Check if file is static or htmlf file.
	static const char htmlf[] = "<htmlf>";
	static const size_t htmlf_len = sizeof(htmlf)-1;
	char buff[htmlf_len+1];
	buff[htmlf_len] = '\0';
	if (read(fd, buff, htmlf_len) == -1) {
		perror("[ERROR] Failed to read file");
		perror_line();
		fprintf(stderr, "\tFile: %s\n", path);
		close(fd);
		sendResponseHeader(clntsock, STATUS_500_STR, 0);
		return;
	}
	// Static file
	if (strcmp(buff, htmlf) != 0) {
		char st_size[21]={0}; sprintf(st_size, "%ld", st.st_size);
		char *fields[2] = { "Content-Type", "Content-Length"};
		char *values[2] = { (char*)mime, st_size };
		http_header_t header = {
			.elementCount = 2,
			.keys = fields,
			.values = values
		};
		printf("[DEBUG] Static html file...\n");
		printf("[DEBUG] Sending Response...\n");
		sendResponseHeader(clntsock, STATUS_200_STR, 0);
		sendHeaderValues(clntsock, &header, 0);
		send(clntsock, "\r\n\r\n", 4, 0);
		lseek(fd, 0, SEEK_SET);
		sendfile(clntsock, fd, NULL, st.st_size);
		close(fd);
		printf("[DEBUG] Response sent...\n");
		return;
	}
	// Parse htmlf file
	printf("[DEBUG] Htmlf file...\n");
	close(fd);
	FILE *file = fopen(path, "r");
	// Give me a fucking break, later.
	fclose(file);
}

void handleGetRequest(int clntsock, const char *req, size_t n) {
	
}

ssize_t sendHeaderValues(int socket, const http_header_t *header, int flags) {
	size_t header_size = 0;
	for (size_t i = 0; i < header->elementCount; i++) {
		header_size += strlen(header->keys[i]);
		header_size += strlen(header->values[i]);
		header_size += sizeof(": \r\n") - 1;
	}
	header_size += sizeof("\r\n");

	char str[header_size];
	char *sptr = str;
	for (size_t i = 0; i < header->elementCount; i++) {
		sptr += sprintf(str, "%s: %s\r\n", header->keys[i], header->values[i]);
	}
	sptr += sprintf(str, "\r\n");
	// TODO: check that this is actually the end of str, and not an array overflow.
	*sptr = '\0';

	return send(socket, str, (sptr - str), flags);
}

ssize_t sendResponseHeader(int socket, const char *status, int flags) {
	char str[RESPONSE_HEADER_SIZE];
	int len = sprintf(str, "HTTP/1.1 %s", status);
	return send(socket, str, len, flags);
}