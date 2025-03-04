#include<stdio.h>
#include<string.h>

#include "arachno/server.h"
#include "arachno/conf.h"
#include "arachno/logger.h"

int staticFileHandler(int sock, const struct http_request *req);
int tryExt(int sock, const struct http_request *req);
int send404(int sock, const struct http_request *req);

int arachno_init(int argc, const char *argv[]) {
   setLogger(NULL, LOG_DEBUG, 1);
   return 0;
}

int arachno_start(int argc, const char *argv[]) {

   serveStaticFiles();
	registerRequest(tryExt);
   set404Page("./FoF.html");

   return 0;
}

void arachno_stop() {
   printf("Destroying...\n");
}

void arachno_destroy() {
   printf("Destroyed...\n");
}