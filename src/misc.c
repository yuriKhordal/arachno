#include<stdio.h>
#include<string.h>

#include "misc.h"

int strendswith(const char *str, const char *end) {
	size_t str_len = strlen(str);
	size_t end_len = strlen(end);

	str += str_len - end_len;
	return strcmp(str, end) == 0;
}

char *strtok_nd(char *restrict str, const char *restrict delim) {
	static char *ptr = NULL;
	static char save = '\0';
	// Initialize new string
	if (str != NULL)
		ptr = str;
	// No more tokens
	if (ptr == NULL)
		return NULL;
	
	// Restore delimeter
	if (str == NULL)
		ptr[-1] = save;
	// Skip delimeters
	while (*ptr && strchr(delim, *ptr))
		ptr++;
	// No more tokens
	if (*ptr == '\0')
		return NULL;
	
	char *tok = ptr;
	size_t i;
	for (i = 0; tok[i] != '\0'; i++) {
		if (strchr(delim, tok[i]) != NULL) {
			save = tok[i];
			tok[i] = '\0';
			ptr = &tok[i+1];
			return tok;
		}
	}

	// Last token
	ptr = NULL;
	return tok;
}
