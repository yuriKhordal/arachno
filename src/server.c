#include<stdio.h>
#include<stdlib.h>
#include<string.h>
#include<ctype.h>
#include<sys/socket.h>
#include<sys/sendfile.h>
#include<sys/stat.h>
#include<netinet/in.h>
#include<arpa/inet.h>
#include<unistd.h>
#include<sys/wait.h>
#include<fcntl.h>
#include<endian.h>
#include<magic.h>
#include<signal.h>

#include "server.h"
#include "rest.h"
#include "status.h"
#include "misc.h"

magic_t magicfd;
int sockfd;

// Configurations:

struct in_addr cfg_local_ip;
in_port_t cfg_local_port;
char *cfg_default_index = "index.html";
size_t cfg_default_index_len = sizeof("index.html") - 1;
char *cfg_base_path = "html/";
size_t cfg_base_path_len = sizeof("html/") - 1;

_Atomic int stop = 0;
void onCtrlC(int signal) {
	printf("[INFO] Ctrl+C Recieved, Stopping.\n");
	stop = 1;
}

int main(int argc, char *argv[]) {
	// TO BE REPLACED WITH CONFIGS:
	cfg_local_ip.s_addr = htonl(0);
	cfg_local_port = htons(80);

	// Init libmagic (MIME types db).
	magicfd = magic_open(MAGIC_MIME_TYPE);
	if (magicfd == NULL) {
		perror("[ERROR] Failed opening magic file");
		perror_line();
		return EXIT_FAILURE;
	}
	if (magic_load(magicfd, NULL) == -1) {
		fprintf(stderr,
			"[ERROR] Failed loading MIME database: %s\n", magic_error(magicfd)
		);
		perror_line();
		magic_close(magicfd);
		return EXIT_FAILURE;
	}

	// Create and bind socket and start listening to connections.
	sockfd = socket(AF_INET, SOCK_STREAM, 0);
	if (sockfd == -1) {
		perror("[Error] Failed to open a socket");
		perror_line();
		magic_close(magicfd);
		return EXIT_FAILURE;
	}
	printf("[INFO] Socket created succesffully.\n");
	printf("[DEBUG] Socket fd: %d\n", sockfd);
	int reuseaddr = 1;
	if (setsockopt(sockfd, SOL_SOCKET, SO_REUSEADDR, &reuseaddr, sizeof(reuseaddr)) == -1) {
		perror("[Error] Failed to set the socket as reusable");
		perror_line();
		magic_close(magicfd);
		close(sockfd);
		return EXIT_FAILURE;
	}
	const struct sockaddr_in addr = {AF_INET, cfg_local_port, cfg_local_ip};
	if (bind(sockfd, (const struct sockaddr*)&addr, sizeof(addr)) == -1) {
		perror("[Error] Failed to bind the socket");
		perror_line();
		magic_close(magicfd);
		close(sockfd);
		return EXIT_FAILURE;
	}
	printf("[INFO] Socket bound to address %s:%d succesffully.\n", inet_ntoa(cfg_local_ip), ntohs(cfg_local_port));
	if (listen(sockfd, LISTEN_BACKLOG) == -1) {
		perror("[Error] Error while trying to start listening on socket");
		perror_line();
		magic_close(magicfd);
		close(sockfd);
		return EXIT_FAILURE;
	}
	printf("[INFO] Listening mode initiated. Accepting connections.\n");

	const struct sigaction act = {.sa_handler=onCtrlC};
	sigaction(SIGINT, &act, NULL);

	// Start accepting connections.
	while (!stop) {
		struct sockaddr_in clientaddr;
		socklen_t socklen;
		int clientfd = accept(sockfd, &clientaddr, &socklen);
		if (clientfd == -1) {
			perror("[WARN] Failed connection from client");
			perror_line();
			continue;
		}

		// // Fork to handle connection.
		// while (waitpid(-1, NULL, WNOHANG) > 0); // Collect dead children?
		// pid_t pid = fork();
		// // Fork failed, log and let the main process handle the request.
		// if (pid == -1) {
		// 	perror("[WARN] Failed to create a child process to deal with an incomming connection");
		// 	perror_line();
		// 	fprintf(stderr, "\tMain process will handle the connection instead.\n");
		// }
		// // Parent process:
		// else if (pid > 0) {
		// 	printf("[INFO] Created child process with pid %d to handle incomming connection.", pid);
		// 	waitpid(pid, NULL, WNOHANG);
		// 	// ^ Does this tell the kernel to remove the zombie child once it dies?
		// 	// ^ Or just removes it if it already died?
		// 	// ^ Do I need to check for zombies all the time?
		// 	// ^ Or just `waitpid` once and it will be removed when dead?
		// }
		// // Child process:
			

		printf("[INFO] Accepting connection from %s:%d. Data:\n",
			inet_ntoa(clientaddr.sin_addr), clientaddr.sin_port
		);
		char buffer[REQUEST_BUFF_SIZE];
		ssize_t msg_len = recv(clientfd, buffer, REQUEST_BUFF_SIZE, 0);

		if (msg_len == -1) {
			perror("[ERROR] Failed to recieve message");
			perror_line();
		}
		else {
			printf("%s\n[INFO]%ld Bytes.\n", buffer, msg_len);
			handleRequest(clientfd, buffer, REQUEST_BUFF_SIZE);
			printf("[INFO] Connection closed.\n");
		}

		//Is this needed?? (The if, not the close)
		if (close(clientfd) == -1) {
			perror("[Error] close() failed???? Info");
		}
		
		// if (pid == 0) return EXIT_SUCCESS; // Child
	}

	// Destroy/Free resources before exit
	if (close(sockfd) == -1) {
		perror("[Error] close() failed???? Info");
		perror_line();
	}
	magic_close(magicfd);
	return EXIT_SUCCESS;
}

// TODO: If file doesn't exist, still return path.
char * getPath(char *buff, size_t *n, const char *path) {
	// Initialize variables.
	size_t arg_path_len = strlen(path);
	size_t path_len = cfg_base_path_len + arg_path_len + cfg_default_index_len;
	if (n && cfg_base_path_len + arg_path_len + 1 > *n) {
		fprintf(stderr,
			"Buffer size too small for path. Size: %zu, path length: %zu\n",
			*n, cfg_base_path_len + arg_path_len + 1
		);
		perror_line();
		return NULL;
	}

	char real_path[path_len + 1];
	strcpy(real_path, cfg_base_path);
	strcat(real_path, path);

	// Check if file is a dir.
	struct stat st;
	size_t len = cfg_base_path_len + arg_path_len;
	if (stat(real_path, &st) == -1) {
		const size_t html_len = sizeof(".html") - 1;
		if (n && cfg_base_path_len + arg_path_len + html_len + 1 > *n) {
			fprintf(stderr,
				"Buffer size too small for path. Size: %zu, path length: %zu\n",
				*n, path_len
			);
			perror_line();
			return NULL;
		}
		strcat(real_path, ".html");
		len = cfg_base_path_len + arg_path_len + html_len;
	} else if (S_ISDIR(st.st_mode)) {
		if (n && path_len + 1 > *n) {
			fprintf(stderr,
				"Buffer size too small for path. Size: %zu, path length: %zu\n",
				*n, path_len
			);
			perror_line();
			return NULL;
		}
		strcat(real_path, cfg_default_index);
		len = path_len;
	}

	if (stat(real_path, &st) != -1 && S_ISDIR(st.st_mode)) {
		if (n && path_len + 1 > *n) {
			fprintf(stderr,
				"Buffer size too small for path. Size: %zu, path length: %zu\n",
				*n, path_len
			);
			perror_line();
			return NULL;
		}
		strcat(real_path, cfg_default_index);
		len = path_len;
	} else {

	}

	// Write path to buffer.
	if (buff == NULL) {
		buff = calloc(len+1, sizeof(char));
		if (buff == NULL) {
			perror("[ERROR] Failed allocating buffer");
			perror_line();
			return NULL;
		}
	}
	if (n) *n = len;
	strcpy(buff, real_path);

	return buff;
}