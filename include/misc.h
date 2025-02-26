#ifndef __MISC_H__
#define __MISC_H__

#define perror_line() (fprintf(stderr, "\tAt %s:%s line %d.\n", __FILE__, __func__, __LINE__))

int strendswith(const char *str, const char *end);

/**
 * Like strtok but non-destructive, each consecutive call to strtok_nd restores
 * the previous delimiter that was replaced with '\0' by the previous call,
 * strtok_nd is done with ALL tokens(i.e. after returning NULL), the string will
 * be in it's original state.
 */
char *strtok_nd(char *restrict str, const char *restrict delim);

/**
 * Restore the string tokenized by strtok_nd back to it's original state in
 * case that strtok_nd wasn't done with the string(after strtok_nd is done the
 * string is already back to original state and no manul restoration is needed).
 */
void strtok_restore();

#endif