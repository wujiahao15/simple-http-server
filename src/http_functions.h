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
// for reading directory
#include <sys/types.h>
#include <dirent.h>

#include "http_response.h"

// http header params
#define HTTP_HDR_METHOD_LEN 10
#define HTTP_HDR_URL_LEN (1 << 10)
#define HTTP_HDR_VERSION_LEN 10

// process params
#define MAX_PATH_LEN 512
#define MAX_LINE_LEN 1024

// html strings
#define HTML_BEFORE_BODY                      \
    "<html>"                                  \
    "<title>WUW Web Server Main Page</title>" \
    "<body>"                                  \
    "<p>Welcome to Wu Jiahao's web server!</p>"
#define HTML_AFTER_BODY                                      \
    "<form method=\"POST\" enctype=\"multipart/form-data\">" \
    "<input type=\"file\">"                                  \
    "<button type=\"submit\">upload</button>"                \
    "</form>"                                                \
    "</body>"                                                \
    "</html>"

// function define
#define isSpace(x) isspace((int)(x))

// enums
enum http_method {
    NOT_IMPLEMENT = -1,
    GET,
    POST,
    /*
       the below methods
       are not implemented.
    */
    HEAD,
    PUT,
    DELETE,
    OPTIONS,
    TRACE,
    CONNECT,
    OTHERS
};

// http header struct
typedef struct http_headers_t {
    char version[HTTP_HDR_VERSION_LEN];
    char method[HTTP_HDR_METHOD_LEN];
    char url[HTTP_HDR_URL_LEN];
    int mode;
} http_headers_t;

// function declarations
int http_init();
void accept_request_handler(void* arg);
int get_line_from_socket(int sockfd, char* buf);
int get_method_from_str(char* buf, struct http_headers_t* hdr);
char* get_url_from_str(char* buf, struct http_headers_t* hdr);
void send_file(int client, FILE* fp);
void send_directory_to_client(int client, char* directory);
void send_file_to_client(int sockfd, char* path);
void recv_file_from_client(int sockfd, char* path);

#endif