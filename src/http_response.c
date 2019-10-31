#include "http_response.h"
#include "logger.h"
#include <stdlib.h>
#include <string.h>
#include <sys/types.h>
#include <sys/socket.h>

#define MAX_BUFF_SIZE (1 << 10)

void format_and_send_response(int sockfd, const char* msg) {
    char buf[MAX_BUFF_SIZE];
    strcpy(buf, msg);
    logger(DEBUG, "%s", msg);
    strcat(buf, "\r\n");
    send(sockfd, buf, strlen(buf), 0);
}

void http_ok(int client) {
    logger(DEBUG, "sending `ok` response headers");
    // send response back to client
    format_and_send_response(client, "HTTP/1.0 200 OK");
    format_and_send_response(client, SERVER_BASE_STR);
    format_and_send_response(client, "Content-Type: text/html");
    format_and_send_response(client, "");
}

void http_ok_length(int client, char* filename) {
    // TODO: could use filename to determine file type
    (void)filename;
    logger(DEBUG, "sending response headers");
    // send response back to client
    format_and_send_response(client, "HTTP/1.0 200 OK");
    format_and_send_response(client, SERVER_BASE_STR);
    format_and_send_response(client, "Content-Type: text/html");
    format_and_send_response(client, "");
}

void http_not_implemented(int client) {
    logger(DEBUG, "sending `not implement` response");
    // send response back to client
    format_and_send_response(client, "HTTP/1.0 501 Method Not Implemented");
    format_and_send_response(client, SERVER_BASE_STR);
    format_and_send_response(client, "Content-Type: text/html");
    format_and_send_response(client, "");
    char buf[MAX_BUFF_SIZE];
    sprintf(buf, HTML_RESPONSE_FMT, HTML_TITLE_NOT_IMPLEMENT,
            HTML_BODY_NOT_IMPLEMENT);
    format_and_send_response(client, buf);
}

void http_internal_server_error(int client) {
    logger(DEBUG, "sending `internal server error` response headers");
    // send response back to client
    format_and_send_response(client, "HTTP/1.0 500 Internal Server Error");
    format_and_send_response(client, SERVER_BASE_STR);
    format_and_send_response(client, "Content-Type: text/html");
    format_and_send_response(client, "");
    char buf[MAX_BUFF_SIZE];
    sprintf(buf, HTML_RESPONSE_FMT, HTML_TITLE_INTERNAL_ERR,
            HTML_BODY_INTERNAL_ERR);
    format_and_send_response(client, buf);
}

void http_not_found(int client) {
    logger(DEBUG, "sending `404 not found` response headers");
    // send response back to client
    format_and_send_response(client, "HTTP/1.0 404 NOT FOUND");
    format_and_send_response(client, SERVER_BASE_STR);
    format_and_send_response(client, "Content-Type: text/html");
    format_and_send_response(client, "");
    char buf[MAX_BUFF_SIZE];
    sprintf(buf, HTML_RESPONSE_FMT, HTML_TITLE_NOT_FOUND,
            HTML_BODY_NOT_FOUND);
    format_and_send_response(client, buf);
}

void http_forbidden(int client) {
    logger(DEBUG, "sending `forbidden` response headers");
    // send response back to client
    format_and_send_response(client, "HTTP/1.0 403 Forbidden");
    format_and_send_response(client, SERVER_BASE_STR);
    format_and_send_response(client, "Content-Type: text/html");
    format_and_send_response(client, "");
    char buf[MAX_BUFF_SIZE];
    sprintf(buf, HTML_RESPONSE_FMT, HTML_TITLE_FORBIDDEN,
            HTML_BODY_FORBIDDEN);
    format_and_send_response(client, buf);
}

void http_bad_request(int client) {
    logger(DEBUG, "sending `bad request` response headers");
    // send response back to client
    format_and_send_response(client, "HTTP/1.0 400 Bad Request");
    format_and_send_response(client, SERVER_BASE_STR);
    format_and_send_response(client, "Content-Type: text/html");
    format_and_send_response(client, "");
    char buf[MAX_BUFF_SIZE];
    sprintf(buf, HTML_RESPONSE_FMT, HTML_TITLE_BAD_REQUEST,
            HTML_BODY_BAD_REQUEST);
    format_and_send_response(client, buf);
}
