
#include<stdio.h>
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

int readRequest(int clientsock, struct http_request *request) {
	char *buffer = malloc(sizeof(char) * REQUEST_BUFF_SIZE);
	ssize_t msg_len = recv(clientsock, buffer, REQUEST_BUFF_SIZE, 0);

	if (msg_len == -1) {
		logf_errno("Failed to recieve message");
		log_line();
		return -1;
	}
	// printf("%s\n[INFO]%ld Bytes.\n", buffer, msg_len);
	// handleRequest(clientfd, buffer, REQUEST_BUFF_SIZE);
	// printf("[INFO] Connection closed.\n");

	// Find the end of the line.
	size_t i;
	for (i = 0; buffer[i] != '\n' && i < msg_len; i++);
	if (i == msg_len) {
		// No end of line.
		logf_warning("Invalid request syntax. Dropping connection.");
		return -1;
	}
	size_t linelen = i;

	// Fill request line:
	// Method
	for (i = 0; isspace(buffer[i]) && i < linelen; i++);
	if (i == linelen) {
		// Only method???
		logf_warning("Invalid request syntax. Dropping connection.");
		return -1;
	}
	if (strncmp(buffer, "GET", sizeof("GET")) == 0) {
		request->method = HTTP_GET;
	} else if (strncmp(buffer, "HEAD", sizeof("HEAD")) == 0) {
		request->method = HTTP_HEAD;
	} else if (strncmp(buffer, "POST", sizeof("POST")) == 0) {
		request->method = HTTP_POST;
	} else if (strncmp(buffer, "PUT", sizeof("PUT")) == 0) {
		request->method = HTTP_PUT;
	} else if (strncmp(buffer, "DELETE", sizeof("DELETE")) == 0) {
		request->method = HTTP_DELETE;
	} else if (strncmp(buffer, "CONNECT", sizeof("CONNECT")) == 0) {
		request->method = HTTP_CONNECT;
	} else if (strncmp(buffer, "OPTIONS", sizeof("OPTIONS")) == 0) {
		request->method = HTTP_OPTIONS;
	} else if (strncmp(buffer, "TRACE", sizeof("TRACE")) == 0) {
		request->method = HTTP_TRACE;
	} else if (strncmp(buffer, "PATCH", sizeof("PATCH")) == 0) {
		request->method = HTTP_PATCH;
	} else {
		logf_warning("Unknown method %*s. Dropping connection.", i, buffer);
		return -1;
	}

	while (isspace(buffer[i])) i++;
	// Path
	
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
		struct http_header header = {
			.content_length = st.st_size,
			.content_type = mime
		};
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
		struct http_header header = {
			.content_length = st.st_size,
			.content_type = mime
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
	char str[HTTP_HEADER_SIZE];
	int len = 0;
	switch(header->authorization.type) {
	case AUTH_BASIC:
		len += sprintf(str + len, "\nAuthorization: Basic %s", header->authorization.credentials);
	}
	if (header->content_length != -1)
		len += sprintf(str + len, "\nContent-Length: %lu", header->content_length);
	if (header->content_type != NULL)
		len += sprintf(str + len, "\nContent-Type: %s", header->content_type);
	if (header->host != NULL)
		len += sprintf(str + len, "\nHost: %s", header->host);

	return send(socket, str, len, flags);
}

ssize_t sendResponseHeader(int socket, const char *status, int flags) {
	char str[RESPONSE_HEADER_SIZE];
	int len = sprintf(str, "HTTP/1.1 %s", status);
	return send(socket, str, len, flags);
}