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


int main2(int argc, char const *argv[])
{
   int sockfd = socket(AF_INET, SOCK_STREAM, 0);
   int reuseaddr = 1;
   setsockopt(sockfd, SOL_SOCKET, SO_REUSEADDR, &reuseaddr, sizeof(reuseaddr));
   struct in_addr ip;
   ip.s_addr = htonl(0);
   const struct sockaddr_in addr = {AF_INET, htons(8080), ip};
	bind(sockfd, (const struct sockaddr*)&addr, sizeof(addr));
   listen(sockfd, 1024);

   struct sockaddr_in clientaddr;
	socklen_t socklen;
	int clientfd = accept(sockfd, (struct sockaddr*)&clientaddr, &socklen);

   char *buff = calloc(500, 1);
   memset(buff, '+', 450);
   ssize_t len = recv(clientfd, buff, 437, 0);

   printf("---%s---\nlen: %zd, lastchar: %d\n", buff, len, buff[436]);

   for (int i = 430; i <= 437; i++) {
      printf("\n%d) %d ", i, buff[i]);
   }
      printf("\n");
   return 0;
}
