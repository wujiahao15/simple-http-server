#ifndef __HTTP_SERVER_H__
#define __HTTP_SERVER_H__

#include <arpa/inet.h>
#include <ctype.h>
#include <netinet/in.h>
#include <sys/stat.h>
#include <sys/socket.h>
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <unistd.h>
#include <pthread.h>
#include "logger.h"

#define SERVER_PORT 12306
#define SERVER_BASE_STR "Server: WUW"
#define SERVER_STRING SERVER_BASE_STR "\r\n"
#define isSpace(x) isspace((int)(x))
#define MAX_METHOD_LEN 15
#define MAX_URL_LEN 255
#define MAX_PATH_LEN 512
#define MAX_LINE_LEN 1024

enum http_method { GET = 1, POST, OTHERS };

int http_init();
void accept_request_handler(void* arg);
int get_line_from_socket(int sockfd, char* buf);
int get_method_from_str(char* buf, char* method);
char* get_url_from_str(char* buf, char* url, int mode);
void send_file(int client, FILE* fp);
void send_file_to_client(int sockfd, char* path);
void recv_file_from_client(int sockfd, char* path);
void send_response_headers(int sockfd, const char* filename);
void not_found(int client);
void unimplemented(int client);

#endif