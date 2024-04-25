#include<stdio.h>

union test1 {
   int d;
   unsigned u;
   char c;
   char *s;
   size_t zu;
};

int main(int argc, char const *argv[])
{
   union test1 t1 = {.d=-10};
   union test1 t2 = {.u=1 << (sizeof(unsigned)*8) - 1};
   union test1 t3 = {.c='h'};
   union test1 t4 = {.s="Hello"};
   union test1 t5 = {.zu=15};

   printf("t1 = %d\n", t1);
   printf("t2 = %u\n", t2);
   printf("t3 = %c\n", t3);
   printf("t4 = %s\n", t4);
   printf("t4 = %zu\n", t5);

   return 0;
}
