#ifndef __MISC_H__
#define __MISC_H__

#define perror_line() (fprintf(stderr, "\tAt %s:%s line %d.\n", __FILE__, __func__, __LINE__))

int strendswith(const char *str, const char *end);

#endif