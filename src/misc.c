#include<stdio.h>
#include<string.h>

#include "misc.h"

int strendswith(const char *str, const char *end) {
   size_t str_len = strlen(str);
   size_t end_len = strlen(end);

   str += str_len - end_len;
   return strcmp(str, end) == 0;
}