#include<stdio.h>
#include<stdlib.h>
#include<string.h>
#include<ctype.h>
#include<time.h>
#include<limits.h>
#include<linux/limits.h>
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
#include<libgen.h>
#include<dlfcn.h>
#include<argp.h>
#include<errno.h>

#include "server.h"
#include "http.h"
#include "status.h"
#include "conf.h"
#include "misc.h"
#include "logger.h"
#include "errors.h"

// TODO: Move to other place.
struct htmlf_format_list {
	struct kvpair *values;
	size_t length;
};
struct cardf {
	char *path;
	struct htmlf_format_list format_list;
};
#include<wctype.h>
union valuef_t {
	int c;
	wint_t lc;
	char *s;
	wchar_t *ls;
	long d;
	unsigned long u;
	double f;
	// void *p;
	struct cardf cardf;
};
struct kvpair {
	char *key;
	union valuef_t value;
};

magic_t magicfd;
int sockfd;

int staticFileHandler(int sock, const struct http_request *req);
int tryExt(int sock, const struct http_request *req);
int send404(int sock, const struct http_request *req);
int initArachno();
void destroyArachno();
void sendError(int arcErr, int sock);

struct {
	/** An array of request handlers. */
	on_request *arr;
	/** The amount of request handlers in the array. */
	size_t count;
	/** The capacity the array. */
	size_t arr_size;
} requestHandlers = {NULL, 0, 0};

char *fofPath = NULL;

_Atomic int stop = 0;
void onCtrlC(int signal) {
	printf("[INFO] SIGINT(Ctrl+C) Recieved, Stopping.\n");
	stop = 1;
}

typedef int (*init_func_t)(int argc, char *argv[]);
typedef void (*destroy_func_t)();

typedef struct args {
	char *dir;
	char *app_path;
	
	int app_argc;
	char **app_argv;
} args_t;

error_t argp_opt(int key, char *value, struct argp_state *state);

args_t parseArgs(int argc, char *argv[]) {
	struct args args = {
		.dir = NULL,
		.app_argc = 0, .app_argv = NULL,
		.app_path = NULL
	};
	struct argp_option options[] = {
		{"dir", 'd', "directory", 0, "Specify the working directory for the app. Default is the directory of the app .so file", 0},
		{ 0 }
	};
	struct argp argp = {
		.options = options,
		.parser = argp_opt,
		.args_doc = "app",
		.doc = "TODO: Add",
		.children = NULL,
		.help_filter = NULL,
		.argp_domain = NULL
	};

	argp_parse(&argp, argc, argv, ARGP_IN_ORDER, NULL, &args);
	return args;
}

error_t argp_opt(int key, char *value, struct argp_state *state) {
	args_t *args = state->input;

	// App argument, finish parsing.
	if (key == ARGP_KEY_ARG) {
		args->app_argc = state->argc - state->next + 1;
		args->app_argv = state->argv + state->next - 1;
		args->app_path = value;
		state->next = state->argc;
		return 0;
	}
	if (key == ARGP_KEY_NO_ARGS) {
		fprintf(stderr, "%s: Missing app argument.\n", state->name);
		argp_help(state->root_argp, stderr, ARGP_HELP_STD_ERR, state->name);
		exit(1);
	}

	// --dir
	if (key == 'd') {
		args->dir = value;
	}

	return 0;
}

int main(int argc, char *argv[]) {
	struct args args = parseArgs(argc, argv);

	// Loading project .so:
	void *lib;
	init_func_t arachno_init;
	init_func_t arachno_start;
	destroy_func_t arachno_stop;
	destroy_func_t arachno_destroy;

	lib = dlopen(args.app_path, RTLD_LAZY | RTLD_GLOBAL);
	if (lib == NULL) {
		fprintf(stderr, "Couldn't open: %s\n", dlerror());
		return EXIT_FAILURE;
	}

	arachno_start = dlsym(lib, "arachno_start");
	if (arachno_start == NULL) {
		fprintf(stderr, "[Warning] Couldn't load arachno_start: %s\n", dlerror());
	}
	arachno_init = dlsym(lib, "arachno_init");
	arachno_stop = dlsym(lib, "arachno_stop");
	arachno_destroy = dlsym(lib, "arachno_destroy");

	// Change directory
	if (args.dir) {
		if (chdir(args.dir) == -1) {
			fprintf(stderr, "arachno: Failed to change directory to '%s': ", args.dir);
			perror("");
			dlclose(lib);
			return EXIT_FAILURE;
		}
	} else {
		char dir[PATH_MAX];
		char *res = realpath(args.app_path, dir);
		if (res == NULL) {
			fprintf(stderr, "arachno: Can't resolve '%s': ", args.app_path);
			perror("");
			dlclose(lib);
			return EXIT_FAILURE;
		}
		chdir(dirname(dir));
	}
	
	if (loadConfigs() == -1) log_line();
	
	if (arachno_init) {
		int res = arachno_init(argc, argv);
		if (res != ARC_NO_ERRORS) {
			logf_error("arachno_init failed with error code %d\n", res);
			return EXIT_FAILURE;
		}
	}

	if (initArachno() == -1) {
		logf_error("Failed to initialize arachno.\n");
		log_line();
		return EXIT_FAILURE;
	}
	
	// TODO: Load post-init method.
	if (arachno_start) {
		int res = arachno_start(argc, argv);
		if (res != ARC_NO_ERRORS) {
			logf_error("arachno_start failed with error code %d\n", res);
			return EXIT_FAILURE;
		}
	}

	// =============== TEMP===============
	// registerRequest(staticFileHandler);
	// registerRequest(tryExt);
	// registerRequest(send404);
	// =============== TEMP===============

	// Start listening to requests.
	if (listen(sockfd, LISTEN_BACKLOG) == -1) {
		logf_errno("Error while trying to start listening on socket");
		log_line();
		magic_close(magicfd);
		close(sockfd);
		return EXIT_FAILURE;
	}
	logf_info("Listening mode initiated. Accepting connections.\n");

	struct sigaction act = { .sa_handler = onCtrlC };
	sigaction(SIGINT, &act, NULL);

	// Start accepting connections.
	while (!stop) {
		struct sockaddr_in clientaddr;
		socklen_t socklen = sizeof(clientaddr);
		int clientfd = accept(sockfd, (struct sockaddr*)&clientaddr, &socklen);
		if (clientfd == -1) {
			if (stop) break;
			logf_errno("Failed connection from client");
			log_line();
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
			
		logf_debug("Accepting connection from %s:%d.\n",
			inet_ntoa(clientaddr.sin_addr), clientaddr.sin_port
		);

		http_request_t req;
		requestInit(&req);
		int err = readRequest(clientfd, &req);
		if (err < 0) {
			log_line();
			sendError(err, clientfd);
			if (close(clientfd) == -1) {
				logf_errno("close() failed???? Info");
				log_line();
			}
			continue;
		}

		int handledResult = 0;
		for (int i = 0; i < requestHandlers.count && handledResult != 1; i++) {
			handledResult = requestHandlers.arr[i](clientfd, &req);
		}
		if (handledResult <= 0) {
			sendError(handledResult, clientfd);
			send(clientfd, "HTTP/1.1 "STATUS_500_STR"\r\n\r\n", 40, 0);
		}

		//Is this needed?? (The if, not the close)
		if (close(clientfd) == -1) {
			logf_errno("close() failed???? Info");
			log_line();
		}

		requestDestroy(&req);

		// if (pid == 0) return EXIT_SUCCESS; // Child
	}

	if (arachno_stop) {
		arachno_stop();
	}
	destroyArachno();
	if (arachno_destroy) {
		arachno_destroy();
	}
	dlclose(lib);
	return EXIT_SUCCESS;
}

int initArachno() {
	const int FAIL = -1, SUCCESS = 0;

	// Init request handler list.
	requestHandlers.count = 0;
	requestHandlers.arr_size = 16;
	requestHandlers.arr = calloc(requestHandlers.arr_size, sizeof(on_request));
	if (requestHandlers.arr == NULL) {
		logf_errno("Failed to allocate request handlers");
		log_line();
		return FAIL;
	}

	// Init libmagic (MIME types db).
	magicfd = magic_open(MAGIC_MIME_TYPE);
	if (magicfd == NULL) {
		logf_errno("Failed opening magic file");
		log_line();
		free(requestHandlers.arr);
		return FAIL;
	}
	if (magic_load(magicfd, NULL) == -1) {
		logf_error("Failed loading MIME database: %s\n", magic_error(magicfd));
		log_line();
		free(requestHandlers.arr);
		magic_close(magicfd);
		return FAIL;
	}

	// Create and bind a network socket.
	sockfd = socket(AF_INET, SOCK_STREAM, 0);
	if (sockfd == -1) {
		logf_errno("Failed to open a socket");
		log_line();
		free(requestHandlers.arr);
		magic_close(magicfd);
		return FAIL;
	}
	logf_info("Socket created succesffully.\n");
	logf_debug("Socket fd: %d\n", sockfd);

	int reuseaddr = 1;
	if (setsockopt(sockfd, SOL_SOCKET, SO_REUSEADDR, &reuseaddr, sizeof(reuseaddr)) == -1) {
		logf_errno("Failed to set the socket as reusable");
		log_line();
		free(requestHandlers.arr);
		magic_close(magicfd);
		close(sockfd);
		return FAIL;
	}

	const struct sockaddr_in addr = {AF_INET, cfg_local_port, cfg_local_ip};
	if (bind(sockfd, (const struct sockaddr*)&addr, sizeof(addr)) == -1) {
		logf_errno("Failed to bind the socket");
		log_line();
		free(requestHandlers.arr);
		magic_close(magicfd);
		close(sockfd);
		return FAIL;
	}
	logf_info("Socket bound to address %s:%d succesffully.\n", inet_ntoa(cfg_local_ip), ntohs(cfg_local_port));
	
	return SUCCESS;
}

void destroyArachno() {
	if (close(sockfd) == -1) {
		logf_errno("[Error] close() failed???? Info");
		log_line();
	}
	magic_close(magicfd);
	free(requestHandlers.arr);
	if (fofPath) free(fofPath);
}

// ==================== Request Handlers ====================

int registerRequest(on_request func) {
	if (requestHandlers.count == requestHandlers.arr_size) {
		on_request *temp = realloc(requestHandlers.arr, sizeof(on_request) * requestHandlers.arr_size * 2);
		if (temp == NULL) {
			return ARC_ERR_ALLOC;
		}
		requestHandlers.arr = temp;
		requestHandlers.arr_size *= 2;
	}

	requestHandlers.arr[requestHandlers.count] = func;
	requestHandlers.count++;
	return 0;
}

void unregisterRequest(on_request func) {
	size_t i;
	for (i = 0; i < requestHandlers.count; i++) {
		if (requestHandlers.arr[i] == func) break;
	}
	if (i == requestHandlers.count) return;

	requestHandlers.count--;
	memmove(requestHandlers.arr + i, requestHandlers.arr + i + 1,
		(requestHandlers.count - i) * sizeof(on_request)
	);
}

int serveStaticFiles() {
	return registerRequest(staticFileHandler);
}

int set404Page(const char *path) {
	char *temp = strdup(path);
	if (temp == NULL) {
		logf_errno("%s: Failed to allocate path string", __func__);
		log_line();
		return ARC_ERR_ALLOC;
	}
	
	int err = registerRequest(send404);
	if (err != ARC_NO_ERRORS) {
		logf_errno("%s: Failed to register 404 handler", __func__);
		log_line();
		return err;
	}
	
	fofPath = temp;
	return ARC_NO_ERRORS;
}

int staticFileHandler(int sock, const struct http_request *req) {
	char *path = resolvePath(NULL, req->path);
	if (path == NULL) return 0;

	int err = serveFile(sock, path, NULL);
	if (err < 0) {
		log_line();
	}

	free(path);
	if (err == 0)
		return 1;
	return err;
}

int tryExt(int sock, const struct http_request *req) {
	char *resolved = NULL;
	char path[req->path_len + 20];
	strcpy(path, req->path);

	char *ext[] = {".htm", ".html", ".htmlf"};
	size_t len = sizeof(ext)/sizeof(char*);
	for (int i = 0; i < len; i++) {
		strcpy(path + req->path_len, ext[i]);
		resolved = resolvePath(NULL, path);
		if (resolved != NULL)
			break;
	}

	if (resolved == NULL)
		return 0;
	
	int err = serveFile(sock, resolved, NULL);
	if (err < 0) {
		log_line();
	}

	free(resolved);
	if (err == 0)
		return 1;
	return err;
}

int send404(int sock, const struct http_request *req) {
	char *path = resolvePath(NULL, fofPath);
	if (path == NULL) {
		return -1;
	}

	int err = serveFile(sock, path, NULL);
	if (err < 0) {
		log_line();
	}

	if (err == ARC_NO_ERRORS)
		return 1;
	return err;
}

// Rest

// TODO: If file doesn't exist, still return path.
char * resolvePath(char *buff, const char *path) {
	// Initialize variables.
	const size_t arg_path_len = strlen(path);
	size_t path_len = cfg_base_path_len + 1 + arg_path_len + 1 + cfg_default_index_len;

	char real_path[path_len + 1];
	// strcpy(real_path, cfg_base_path);
	strcpy(real_path, cfg_base_path);
	if (real_path[cfg_base_path_len-1] != '/')
		strcpy(real_path + cfg_base_path_len, "/");
	// strcpy(real_path + cfg_base_path_len + 1, path);
	strcat(real_path + cfg_base_path_len, path);
	// real_path[cfg_base_path_len + 1 + arg_path_len] = '\0';

	// Check if file is a dir.
	struct stat st;

	if (stat(real_path, &st) == -1) {
		return NULL;
	}
	
	if (S_ISDIR(st.st_mode)) {
		strcat(real_path, "/");
		strcat(real_path, cfg_default_index);
	}

	char *temp = realpath(real_path, buff);
	return temp;
}

int serveFile(int sock, const char *path, const struct http_response *res) {
	struct stat st;
	stat(path, &st);

	char *ext = strrchr(path, '.');

	const char *mime = magic_file(magicfd, path);
	if (strcmp(mime, "text/plain") == 0 && ext != NULL) {
		if (strcmp(ext, ".html") == 0) mime = "text/html";
		else if (strcmp(ext, ".htm") == 0) mime = "text/html";
		else if (strcmp(ext, ".css") == 0) mime = "text/css";
		else if (strcmp(ext, ".js") == 0) mime = "text/javascript";
		else if (strcmp(ext, ".json") == 0) mime = "application/json";
	}

	time_t timer = time(NULL);
	struct tm tm = *gmtime(&timer);
	char hdate[30]; strftime(hdate, 30, "%a, %d %b %Y %T GMT", &tm);

	if (res != NULL) {
		logf_error("ERROR: res is not NULL, UNIMPLIMENED YET!\n");
		log_line();
		return -1;
	}

	char response[200] = {0};
	size_t size = sprintf(response,
		"HTTP/1.1 200 OK\r\n"
		"Content-Length: %ld\r\n"
		"Content-Type: %s\r\n"
		"Date: %s\r\n"
		"Server: arachno\r\n"
		"\r\n", st.st_size, mime, hdate
	);

	FILE *file = fopen(path, "r");
	if (file == NULL) {
		logf_errno("Failed to open '%s'", path);
		log_line();
		return -1;
	}

	if (send(sock, response, size, 0) == -1) {
		logf_errno("Failed to send response headers");
		log_line();
		fclose(file);
		return -1;
	}

	if (sendfile(sock, fileno(file), NULL, st.st_size) == -1) {
		logf_errno("Failed to send '%s' file", path);
		log_line();
		fclose(file);
		return -1;
	}

	fclose(file);
	return 0;
}

void sendError(int arcErr, int sock) {
	const char *status;
	switch(arcErr) {
	case ARC_ERR_SYNTAX:
		status = STATUS_400_STR; break;
	case ARC_ERR_NO_LENGTH:
		status = STATUS_411_STR; break;
	case ARC_ERR_UNSUPPORTED_HTTP_VER:
		status = STATUS_505_STR; break;
	case ARC_ERR_NOT_IMPLEMENTED:
		status = STATUS_501_STR; break;
	default:
		status = STATUS_500_STR; break;
	}

	char response[1024];
	int bytes = sprintf(response,
		"HTTP/1.1 %s\r\n\r\n"
		"<h1 style='color:red;text-align:center;border-bottom:1px solid red'>%s</h1>\r\n",
		status, status
	);
	send(sock, response, bytes, 0);
}