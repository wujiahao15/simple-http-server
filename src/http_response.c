#include "http_response.h"
#include "logger.h"
#include <stdlib.h>
#include <string.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <sys/stat.h>

#define MAX_BUFF_SIZE (1 << 10)

const char* get_content_type(char* extension) {
    if (!strcmp(extension, "html") || !strcmp(extension, "htm") ||
        !strcmp(extension, "htx"))
        return "text/html";
    else
        return "application/octet-stream";
}

void format_and_send_response(bfevent_t* client, const char* msg) {
    char buf[MAX_BUFF_SIZE];
    strcpy(buf, msg);
    logger(DEBUG, "%s\\r\\n", msg);
    strcat(buf, "\r\n");
    bufferevent_write(client, buf, strlen(buf));
}

void http_ok(bfevent_t* client) {
    logger(DEBUG, "sending `ok` response headers");
    // send response back to client
    format_and_send_response(client, "HTTP/1.1 200 OK");
    format_and_send_response(client, SERVER_BASE_STR);
    format_and_send_response(client, "Content-Type: text/html");
    format_and_send_response(client, "");
}

void http_ok_send_file(bfevent_t* client, int len, char* file_extension) {
    // TODO: could use filename to determine file type
    char buf[MAX_BUFF_SIZE];
    logger(DEBUG, "sending response headers of sending file");
    // send response back to client
    format_and_send_response(client, "HTTP/1.1 200 OK");
    format_and_send_response(client, SERVER_BASE_STR);
    const char *type = get_content_type(file_extension);
    sprintf(buf, "Content-Type: %s", type);
    format_and_send_response(client, buf);
    if (strcmp(type, "text/html")){
        sprintf(buf, "Content-Length: %d", len);
        format_and_send_response(client, buf);
    }
    format_and_send_response(client, "");
}

void http_not_implemented(bfevent_t* client) {
    logger(DEBUG, "sending `not implement` response");
    // send response back to client
    format_and_send_response(client, "HTTP/1.1 501 Method Not Implemented");
    format_and_send_response(client, SERVER_BASE_STR);
    format_and_send_response(client, "Content-Type: text/html");
    format_and_send_response(client, "");
    char buf[MAX_BUFF_SIZE];
    sprintf(buf, HTML_RESPONSE_FMT, HTML_TITLE_NOT_IMPLEMENT,
            HTML_BODY_NOT_IMPLEMENT);
    format_and_send_response(client, buf);
}

void http_internal_server_error(bfevent_t* client) {
    logger(DEBUG, "sending `internal server error` response headers");
    // send response back to client
    format_and_send_response(client, "HTTP/1.1 500 Internal Server Error");
    format_and_send_response(client, SERVER_BASE_STR);
    format_and_send_response(client, "Content-Type: text/html");
    format_and_send_response(client, "");
    char buf[MAX_BUFF_SIZE];
    sprintf(buf, HTML_RESPONSE_FMT, HTML_TITLE_INTERNAL_ERR,
            HTML_BODY_INTERNAL_ERR);
    format_and_send_response(client, buf);
}

void http_not_found(bfevent_t* client) {
    logger(DEBUG, "sending `404 not found` response headers");
    // send response back to client
    format_and_send_response(client, "HTTP/1.1 404 NOT FOUND");
    format_and_send_response(client, SERVER_BASE_STR);
    format_and_send_response(client, "Content-Type: text/html");
    format_and_send_response(client, "");
    char buf[MAX_BUFF_SIZE];
    sprintf(buf, HTML_RESPONSE_FMT, HTML_TITLE_NOT_FOUND, HTML_BODY_NOT_FOUND);
    format_and_send_response(client, buf);
}

void http_forbidden(bfevent_t* client) {
    logger(DEBUG, "sending `forbidden` response headers");
    // send response back to client
    format_and_send_response(client, "HTTP/1.1 403 Forbidden");
    format_and_send_response(client, SERVER_BASE_STR);
    format_and_send_response(client, "Content-Type: text/html");
    format_and_send_response(client, "");
    char buf[MAX_BUFF_SIZE];
    sprintf(buf, HTML_RESPONSE_FMT, HTML_TITLE_FORBIDDEN, HTML_BODY_FORBIDDEN);
    format_and_send_response(client, buf);
}

void http_bad_request(bfevent_t* client) {
    logger(DEBUG, "sending `bad request` response headers");
    // send response back to client
    format_and_send_response(client, "HTTP/1.1 400 Bad Request");
    format_and_send_response(client, SERVER_BASE_STR);
    format_and_send_response(client, "Content-Type: text/html");
    format_and_send_response(client, "");
    char buf[MAX_BUFF_SIZE];
    sprintf(buf, HTML_RESPONSE_FMT, HTML_TITLE_BAD_REQUEST,
            HTML_BODY_BAD_REQUEST);
    format_and_send_response(client, buf);
}
