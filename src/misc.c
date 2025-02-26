#include<stdio.h>
#include<string.h>
#include<threads.h>

#include "misc.h"

int strendswith(const char *str, const char *end) {
	size_t str_len = strlen(str);
	size_t end_len = strlen(end);

	str += str_len - end_len;
	return strcmp(str, end) == 0;
}

thread_local char *strtok_ptr = NULL;
thread_local char strtok_save = '\0';

char *strtok_nd(char *restrict str, const char *restrict delim) {
	// Initialize new string
	if (str != NULL)
		strtok_ptr = str;
	// No more tokens
	if (strtok_ptr == NULL)
		return NULL;
	
	// Restore delimeter
	if (str == NULL)
		strtok_ptr[-1] = strtok_save;
	// Skip delimeters
	while (*strtok_ptr && strchr(delim, *strtok_ptr))
		strtok_ptr++;
	// No more tokens
	if (*strtok_ptr == '\0')
		return NULL;
	
	char *tok = strtok_ptr;
	size_t i;
	for (i = 0; tok[i] != '\0'; i++) {
		if (strchr(delim, tok[i]) != NULL) {
			strtok_save = tok[i];
			tok[i] = '\0';
			strtok_ptr = &tok[i+1];
			return tok;
		}
	}

	// Last token
	strtok_ptr = NULL;
	strtok_save = '\0';
	return tok;
}

void strtok_restore() {
	if (strtok_ptr != NULL) {
		strtok_ptr[-1] = strtok_save;
	}
	
	strtok_ptr = NULL;
	strtok_save = '\0';
}
