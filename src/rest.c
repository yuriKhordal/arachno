
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

#include "rest.h"
#include "server.h"
#include "conf.h"
#include "status.h"
#include "misc.h"
#include "logger.h"


int cnt = 0;
size_t ttlsize = 0;

void * __mylloc(size_t size) {
	void *p = malloc(size);
	cnt++;
	ttlsize += size;
	return p;
}

int rcnt = 0;
size_t rttlsize = 0;

void * __myrlloc(void *p, size_t size) {
	void *p2 = realloc(p, size);
	rcnt++;
	rttlsize += size;
	return p2;
}

#define malloc(X) __mylloc(X)
#define realloc(p, n) __myrlloc(p, n)

int readRequestStartLine(char *line, struct http_request *request);
int readRequestHeader(char *line, struct http_request *request, size_t n);

int readRequest(int clientsock, struct http_request *request) {
	size_t buffsize = REQUEST_BUFF_SIZE;
	char *buffer = malloc(sizeof(char) * (buffsize + 1));
	if (buffer == NULL) {
		logf_errno("Failed to allocate buffer");
		log_line();
		return -1;
	}
	buffer[buffsize] = '\0';
	ssize_t msg_len = recv(clientsock, buffer, buffsize, 0);

	if (msg_len == -1) {
		logf_errno("Failed to recieve message");
		log_line();
		free(buffer);
		return -1;
	}

	// Find the end of the line.
	char *lineend = strstr(buffer, "\r\n");
	if (lineend == NULL) {
		// No end of line or line too long.
		logf_warning("Invalid request syntax. Dropping connection.\n");
		log_line();
		free(buffer);
		return -1;
	}
	// size_t linelen = i;

	// Fill request line:
	*lineend = '\0';
	if (readRequestStartLine(buffer, request) == -1) {
		log_line();
		free(buffer);
		return -1;
	}
	*lineend = '\r';

	// Recieve all header fields and count them;
	request->header.fieldCount = 0;
	char *header = lineend + 2;
	while (1) {
		char *temp = strstr(header, "\r\n");
		// End of message, recieve next.
		if (temp == NULL) {
			buffsize *= 2;
			char *rbuffer = realloc(buffer, sizeof(char) * buffsize);
			if (rbuffer == NULL) {
				logf_errno("Failed to reallocate buffer");
				log_line();
				free(buffer);
				free(request->path);
				if (request->query) free(request->query);
				return -1;
			}
			buffer = rbuffer;
			ssize_t new_msg_len = recv(clientsock, buffer + msg_len, buffsize - msg_len, 0);
			if (new_msg_len == - 1) {
				logf_errno("Failed to recieve message");
				log_line();
				free(buffer);
				free(request->path);
				if (request->query) free(request->query);
				return -1;
			}
			msg_len += new_msg_len;
			continue;
		}

		while (isspace(header[0]) && header[0] != '\r') header++;
		if (header == temp) {
			break;
		}
		header = temp + 2;
		request->header.fieldCount++;
	}
	char *body = header;

	// Fill headers:
	request->header.fields = malloc(sizeof(char*) * request->header.fieldCount);
	if (request->header.fields == NULL) {
		logf_errno("Failed to allocate header fields");
		log_line();
		free(buffer);
		free(request->path);
		if (request->query) free(request->query);
		return -1;
	}
	
	request->header.values = malloc(sizeof(char*) * request->header.fieldCount);
	if (request->header.values == NULL) {
		logf_errno("Failed to allocate header fields");
		log_line();
		free(buffer);
		free(request->path);
		if (request->query) free(request->query);
		free(request->header.fields);
		return -1;
	}
	
	size_t count = 0;
	header = lineend + 2;
	while (isspace(header[0])) header++;
	
	while (header < body) {
		char *end = strstr(header, "\r\n");
		*end = '\0';
		if (readRequestHeader(header, request, count) == -1) {
			log_line();
			free(buffer);
			free(request->path);
			if (request->query) free(request->query);
			for (size_t i = 0; i < count; i++) {
				free(request->header.fields[i]);
				free(request->header.values[i]);
			}
			free(request->header.fields);
			free(request->header.values);
			return -1;
		}
		*end = '\r';
		
		header = end + 2;
		while (isspace(header[0])) header++;
		count++;
	}

	// Read Data.
	body += 2; // Skip the \r\n delimeter.
	int found = 0;
	for (size_t i = 0; i < request->header.fieldCount; i++) {
		if (strcmp(request->header.fields[i], "Content-Length") == 0) {
			// TODO: Check validity of string before converting.
			request->bodysize = atol(request->header.values[i]);
			found = 1;
			break;
		}
	}
	if (!found) request->bodysize = 0;

	if (request->bodysize > 0) {
		size_t header_size = (body - buffer) * sizeof(char);
		size_t current_body_size = msg_len - header_size;
		if (request->bodysize > current_body_size) {
			buffsize += request->bodysize - current_body_size;
			char * rbuff = realloc(buffer, buffsize);
			if (rbuff == NULL) {
				logf_errno("Failed to reallocate buffer");
				log_line();
				free(buffer);
				free(request->path);
				if (request->query) free(request->query);
				for (size_t i = 0; i < count; i++) {
					free(request->header.fields[i]);
					free(request->header.values[i]);
				}
				free(request->header.fields);
				free(request->header.values);
				return -1;
			}
			buffer = rbuff;
			ssize_t new_msg_len = recv(clientsock, buffer + msg_len,
				request->bodysize - current_body_size, 0
			);
			if (new_msg_len == -1) {
				logf_errno("Failed to recieve message");
				log_line();
				free(buffer);
				free(request->path);
				if (request->query) free(request->query);
				for (size_t i = 0; i < count; i++) {
					free(request->header.fields[i]);
					free(request->header.values[i]);
				}
				free(request->header.fields);
				free(request->header.values);
				return -1;
			}	
		}
		// TODO: CONTINUE SOMEWHERE HERE

		request->body = malloc(sizeof(char) * (request->bodysize + 1));
		strncpy(request->body, body, request->bodysize);
		request->body[request->bodysize] = '\0';
	} else {
		request->body = NULL;
	}

	logf_debug(buffer);
	free(buffer);
	return 0;
}

int readRequestStartLine(char *line, struct http_request *request) {
	while (isspace(*line)) line++;
	size_t i;
	// Method
	for (i = 0; !isspace(line[i]) && line[i] != '\0'; i++);
	if (line[i] == '\0') {
		// Missing path and version.
		logf_warning("Invalid request start line syntax:\n%s\nDropping connection.\n", line);
		log_line();
		return -1;
	}

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
		return -1;
	}

	while (isspace(line[i])) i++;
	// Path
	line += i;
	for (i = 0; !isspace(line[i]) && line[i] != '?' && line[i] != '\0'; i++);
	if (line[i] == '\0') {
		// Missing http version.
		logf_warning("Invalid request start line syntax:\n%s\nDropping connection.\n", line);
		log_line();
		return -1;
	}
	request->path_len = i;
	request->path = malloc(sizeof(char) * (i + 1));
	if (request->path == NULL) {
		logf_errno("Failed to allocate path string");
		log_line();
		return -1;
	}
	strncpy(request->path, line, i);
	request->path[i] = '\0';

	line += i;
	// Query String
	if (line[0] == '?') {
		line++;
		for (i = 0; !isspace(line[i]) && line[i] != '\0'; i++);
		if (line[i] == '\0') {
			// Missing http version.
			logf_warning("Invalid request start line syntax:\n%s\nDropping connection.\n", line);
			log_line();
			free(request->path);
			return -1;
		}
		
		request->query_len = i;
		request->query = malloc(sizeof(char) * (i + 1));
		if (request->query == NULL) {
			logf_errno("Failed to allocate query string");
			log_line();
			free(request->path);
			return -1;
		}
		strncpy(request->query, line, i);
		request->query[i] = '\0';
	} else {
		request->query = NULL;
		request->query_len = 0;
	}

	while (isspace(line[i])) i++;
	line += i;
	
	// HTTP/Version
	if (strncmp(line, "HTTP/1.0", sizeof("HTTP/1.0")) == 0) {
		request->version = HTTP_10;
	} else if (strncmp(line, "HTTP/1.1", sizeof("HTTP/1.1")) == 0) {
		request->version = HTTP_11;
	} else if (strncmp(line, "HTTP/2", sizeof("HTTP/2")) == 0) {
		request->version = HTTP_2;
		logf_warning("HTTP/2 requests are unsupported. Dropping connection.\n", line);
		log_line();
		free(request->path);
		if (request->query) free(request->query);
		return -1;
	} else if (strncmp(line, "HTTP/3", sizeof("HTTP/3")) == 0) {
		request->version = HTTP_3;
		logf_warning("HTTP/3 requests are unsupported. Dropping connection.\n", line);
		log_line();
		free(request->path);
		if (request->query) free(request->query);
		return -1;
	} else {
		logf_warning("Unknown http version: %s. Dropping connection.\n", line);
		log_line();
		free(request->path);
		if (request->query) free(request->query);
		return -1;
	}

	return 0;
}

int readRequestHeader(char *line, struct http_request *request, size_t n) {
	char *orig = line;

	char *idx = strchr(line, ':');
	if (idx == NULL) {
		logf_warning("Failed to parse header line:%s\n"
			"Missing ':', dropping connection.", orig
		);
		log_line();
		return -1;
	}
	
	while (isspace(line[0])) line++;
	size_t field_len = idx - line;
	while (isspace(line[field_len - 1]) && field_len > 0) field_len--;
	if (field_len == 0) {
		logf_warning("Failed to parse header line:%s\n"
			"Empty field name, dropping connection.", orig
		);
		log_line();
		return -1;
	}
	
	idx++;
	while (isspace(idx[0])) idx++;
	size_t value_len = strlen(idx);
	while (isspace(idx[value_len - 1]) && value_len > 0) value_len--;
	if (value_len == 0) {
		logf_warning("Failed to parse header line:%s\n"
			"Empty field value, dropping connection.", orig
		);
		log_line();
		return -1;
	}

	request->header.fields[n] = malloc(sizeof(char) * (field_len + 1));
	if (request->header.fields[n] == NULL) {
		logf_errno("Failed to allocate header");
		log_line();
		return -1;
	}
	request->header.values[n] = malloc(sizeof(char) * (value_len + 1));
	if (request->header.values[n] == NULL) {
		logf_errno("Failed to allocate header");
		log_line();
		return -1;
	}
	
	strncpy(request->header.fields[n], line, field_len);
	request->header.fields[n][field_len] = '\0';
	strncpy(request->header.values[n], idx, value_len);
	request->header.values[n][value_len] = '\0';

	return 0;
}







void handleRequest(int clntsock, const char *req, size_t n) {
	// Get request method.
	size_t method_len = 0;
	while (req[method_len] != '\0' && !isspace(req[method_len]) && method_len < n) method_len++;
	if (method_len == n) return; // no idea what to do. Garbage data???
	if (req[method_len] == '\0') return; // no idea what to do. Garbage data???
	char method[method_len+1]; //Request method.
	strncpy(method, req, method_len);
	method[method_len] = '\0';
	for (size_t i = 0; i < method_len; i++) method[i] = toupper(method[i]);

	// Check method.
	if (strcmp(method, "GET") == 0) {
		handleGetRequest(clntsock, req, n);
	} else {
		printf("[WARN] Unknown method %s.\n", method);
		perror_line();
	}
}

void handleGetRequest(int clntsock, const char *req, size_t n) {
	//Skip until the resource path.
	while (*req != '\0' && *req != '/') {
		req++;
		n--;
	}
	if (*req == '\0') return; //TODO: Something?

	// Get request path.
	size_t path_len = 0;
	// while(req[path_len] != '\0' && !isspace(req[path_len]) && req[path_len] != '?' && req[path_len] != '#' && path_len < n)
	// 	path_len++;
	while (path_len < n) {
		if (req[path_len] == '\0' || isspace(req[path_len])) break;
		if (req[path_len] == '?' || req[path_len] == '#') break;
		path_len++;
	}
	char req_path[path_len + 1];
	strncpy(req_path, req, path_len);
	req_path[path_len] = '\0';
	printf("[INFO] Request path is '%s'.\n", req_path);

	// Resolve path.
	char path[cfg_base_path_len + path_len + cfg_default_index_len + 1];
	if (getPath(path, NULL, req_path) == NULL) {
		perror_line();
		sendResponseHeader(clntsock, STATUS_500_INTERNAL_SERVER_ERROR, 0);
		return;
	}
	struct stat st;
	if (stat(path, &st) == -1 || !S_ISREG(st.st_mode)) {
		sendResponseHeader(clntsock, STATUS_404_NOT_FOUND, 0);
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
		sendResponseHeader(clntsock, STATUS_500_INTERNAL_SERVER_ERROR, 0);
		return;
	}

	// Send non html file:
	if (strcmp(mime, "text/html") != 0) {
		char st_size[21]={0}; sprintf(st_size, "%ld", st.st_size);
		char *fields[2] = { "Content-Type", "Content-Length"};
		char *values[2] = { (char*)mime, st_size };
		struct http_header header = {
			.fieldCount = 2,
			.fields = fields,
			.values = values
		};
		// struct http_header header = {
		// 	.content_length = st.st_size,
		// 	.content_type = mime
		// };
		printf("[DEBUG] Sending Response...\n");
		sendResponseHeader(clntsock, STATUS_200_OK, 0);
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
		sendResponseHeader(clntsock, STATUS_500_INTERNAL_SERVER_ERROR, 0);
		return;
	}
	// Static file
	if (strcmp(buff, htmlf) != 0) {
		char st_size[21]={0}; sprintf(st_size, "%ld", st.st_size);
		char *fields[2] = { "Content-Type", "Content-Length"};
		char *values[2] = { (char*)mime, st_size };
		struct http_header header = {
			.fieldCount = 2,
			.fields = fields,
			.values = values
		};
		printf("[DEBUG] Static html file...\n");
		printf("[DEBUG] Sending Response...\n");
		sendResponseHeader(clntsock, STATUS_200_OK, 0);
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

ssize_t sendHeaderValues(int socket, const struct http_header *header, int flags) {
	size_t header_size = 0;
	for (size_t i = 0; i < header->fieldCount; i++) {
		header_size += strlen(header->fields[i]);
		header_size += strlen(header->values[i]);
		header_size += sizeof(": \r\n") - 1;
	}
	header_size += sizeof("\r\n");

	char str[header_size];
	char *sptr = str;
	for (size_t i = 0; i < header->fieldCount; i++) {
		sptr += sprintf(str, "%s: %s\r\n", header->fields[i], header->values[i]);
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