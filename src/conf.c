#include<stdio.h>
#include<stdlib.h>
#include<stdint.h>
#include<linux/limits.h>
#include<ctype.h>
#include<string.h>
#include<sys/stat.h>
#include<arpa/inet.h>

#include "conf.h"
#include "misc.h"

struct in_addr cfg_local_ip;
in_port_t cfg_local_port;
char cfg_default_index[FILENAME_MAX];
size_t cfg_default_index_len;
char cfg_base_path[PATH_MAX];
size_t cfg_base_path_len;

int parseline(const char *line, size_t len, const char *path, size_t linecount);

void loadDefaultConfig() {
	// Load defaults.
	cfg_local_ip.s_addr = htonl(0);
#ifdef DEBUG
	cfg_local_port = htons(8080);
#else
	cfg_local_port = htons(80);
#endif
	strcpy(cfg_default_index, "index.html");
	cfg_default_index_len = sizeof("index.html") - 1;
	strcpy(cfg_base_path, ".");
	cfg_base_path_len = sizeof(".") - 1;
}

int loadConfigs() {
	// Load defaults.
	cfg_local_ip.s_addr = htonl(0);
#ifdef DEBUG
	cfg_local_port = htons(8080);
#else
	cfg_local_port = htons(80);
#endif
	strcpy(cfg_default_index, "index.html");
	cfg_default_index_len = sizeof("index.html") - 1;
	strcpy(cfg_base_path, ".");
	cfg_base_path_len = sizeof(".") - 1;

	struct stat st;
	if (stat(CONFIG_PATH_SYS, &st) != -1 && S_ISREG(st.st_mode)) {
		if (loadConfig(CONFIG_PATH_SYS) == -1)
			perror_line();
	}
#ifdef CONFIG_DIR_SYS
	if (stat(CONFIG_DIR_SYS, &st) != -1 && S_ISDIR(st.st_mode)) {
		// Maybe later.
	}
#endif
	if (stat(CONFIG_PATH_USR, &st) != -1 && S_ISREG(st.st_mode)) {
		if (loadConfig(CONFIG_PATH_USR) == -1)
			perror_line();
	}

	return 0;
}

int loadConfig(const char *path) {
	// Open config file.
	FILE *cfg = fopen(path, "r");
	if (cfg == NULL) {
		perror("[WARN] Failed to open config file");
		fprintf(stderr, "\tFile: '%s'. Default values loaded.\n", path);
		perror_line();
		return -1;
	}

	char line[CONFIG_MAX_LINE_SIZE];
	size_t len = 0;
	size_t linecount = 1;
	int ch;
	while ((ch = fgetc(cfg)) != EOF) {
		// Skip comments
		if (ch == '#') {
			while (ch != EOF && ch != '\n') ch = fgetc(cfg);
			if (ch == EOF) break;
		}
		// Check if buffer is full.
		if (len >= CONFIG_MAX_LINE_SIZE) {
			fprintf(stderr, "[WARN] Config file exceeded max line length.\n");
			fprintf(stderr, "\tFile: '%s' line %zu.\n", path, linecount);
			perror_line();
			// Skip to next line.
			len = 0;
			while (ch != EOF && ch != '\n') ch = fgetc(cfg);
			if (ch == EOF) break;
		}
		// Parse line.
		else if (ch == '\n') {
			line[len] = '\0';
			if (parseline(line, len, path, linecount) == -1) {
				perror_line();
			}
			len = 0;
			linecount++;
		}
		// Add to buffer
		else {
			line[len] = (char)ch;
			len++;
		}
	}
	// Did fgetc return EOF because it's the end of the file or because IO error?
	if (ferror(cfg)) {
		perror("[WARN] Failed reading from file");
		fprintf(stderr, "\tFile: '%s'.\n", path);
		perror_line();
	}
	// Parse last line.
	if (parseline(line, len, path, linecount) == -1) {
		perror_line();
	}
	// Close and exit.
	fclose(cfg);
	return 0;
}

int parseline(const char *line, size_t len, const char *path, size_t linecount) {
	char key[CONFIG_MAX_LINE_SIZE];
	char value[CONFIG_MAX_LINE_SIZE];
	size_t keylen, valuelen;
	// Read key.
	size_t start = 0, end = 0;
	while (isspace(line[start]) && start < len) start++;
	// Empty line.
	if (start >= len) return 0;
	end = start + 1;
	while (!isspace(line[end]) && end < len) end++;
	// Key without value.
	if (end >= len) {
		fprintf(stderr,
			"[WARN] Missing `=` separator after '%*s'\n", end-start, line + start
		);
		fprintf(stderr, "\tFile: '%s' line %zu.\n", path, linecount);
		perror_line();
		return -1;
	}
	// Key:
	keylen = end - start;
	strncpy(key, line + start, keylen);
	key[keylen] = '\0';

	// Read separator.
	start = end;
	while (isspace(line[start]) && start < len) start++;
	if (start >= len || line[start] != '=') {
		fprintf(stderr, "[WARN] Missing `=` separator after '%s'.\n", key);
		fprintf(stderr, "\tFile: '%s' line %zu.\n", path, linecount);
		perror_line();
		return -1;
	}
	start++; // < Skip the separator
	while (isspace(line[start]) && start < len) start++;
	if (start >= len) {
		fprintf(stderr, "[WARN] Missing value after '%s'.\n", key);
		fprintf(stderr, "\tFile: '%s' line %zu.\n", path, linecount);
		perror_line();
		return -1;
	}

	// Read value.
	end = len - 1;
	while (isspace(line[end]) && end > start) end--;
	if (end < start) {
		fprintf(stderr, "[WARN] Missing value after '%s'.\n", key);
		fprintf(stderr, "\tFile: '%s' line %zu.\n", path, linecount);
		perror_line();
		return -1;
	}
	valuelen = end - start + 1;
	strncpy(value, line + start, valuelen);
	value[valuelen] = '\0';


	// Parse values:
	if (strcmp(key, "IP") == 0) {
		if (strcmp(value, "localhost") == 0) {
			inet_pton(AF_INET, "127.0.0.1", &cfg_local_ip);
			return 0;
		}

		struct in_addr temp;
		if (inet_pton(AF_INET, value, &temp) == 0) {
			fprintf(stderr, "[WARN] IP is wrong format, failed to parse '%s'.\n", value);
			fprintf(stderr, "\tFile: '%s' line %zu.\n", path, linecount);
			perror_line();
			return -1;
		}
		cfg_local_ip = temp;
	}
	else if (strcmp(key, "Port") == 0) {
		char *end;
		long port = strtol(value, &end, 10);
		if (end == value || port < 0 || port > UINT16_MAX || *end != '\0') {
			fprintf(stderr, "[WARN] Port is wrong format, failed to parse '%s'.\n", value);
			fprintf(stderr, "\tFile: '%s' line %zu.\n", path, linecount);
			perror_line();
			return -1;
		}
		cfg_local_port = htons((in_port_t)port);
	}
	else if (strcmp(key, "Index") == 0) {
		if (valuelen > FILENAME_MAX) {
			fprintf(stderr, "[WARN] Index filename too long. Index: '%s'.\n", value);
			fprintf(stderr, "\tFile: '%s' line %zu.\n", path, linecount);
			perror_line();
			return -1;
		}
		strcpy(cfg_default_index, value);
		cfg_default_index_len = valuelen;
	}
	else if (strcmp(key, "BaseDir") == 0) {
		if (valuelen > PATH_MAX) {
			fprintf(stderr, "[WARN] BaseDir too long. BaseDir: '%s'.\n", value);
			fprintf(stderr, "\tFile: '%s' line %zu.\n", path, linecount);
			perror_line();
			return -1;
		}
		strcpy(cfg_base_path, value);
		cfg_base_path_len = valuelen;
	}

	return 0;
}