#ifndef __HTTP_RESPONSE_H__
#define __HTTP_RESPONSE_H__

// libevent
#include <event.h>
#include <event2/bufferevent.h>

/* server information */
#define SERVER_PORT 12306
#define SERVER_BASE_STR "Server: WUW"

/* 
    http error strings
 */
// the format of the html response
#define HTML_RESPONSE_FMT "<html><title>%s</title><body><p>%s</p></body></html>"
// 400 bad request
#define HTML_TITLE_BAD_REQUEST "400 Bad Request"
#define HTML_BODY_BAD_REQUEST "Your browser sent a bad request"
// 403 forbidden
#define HTML_TITLE_FORBIDDEN "403 Forbidden"
#define HTML_BODY_FORBIDDEN "You don't have access to this content"
// 404 not found
#define HTML_TITLE_NOT_FOUND "404 Not Found"
#define HTML_BODY_NOT_FOUND "The file you specified is unavailable"
// 500 internal server error
#define HTML_TITLE_INTERNAL_ERR "500 Internal Server Error"
#define HTML_BODY_INTERNAL_ERR "Internal Server Error"
// 501 method not implemented
#define HTML_TITLE_NOT_IMPLEMENT "501 Method Not Implemented"
#define HTML_BODY_NOT_IMPLEMENT "HTTP request method not supported"

// typedef struct bufferevent as bfevent_t;
typedef struct bufferevent bfevent_t;

/* 
    declarations of functions
 */
void http_ok(bfevent_t* bev);
void http_ok_send_file(bfevent_t* bev, int len, char *extension);
void http_not_found(bfevent_t* bev);

void http_not_implemented(bfevent_t* bev);
void http_bad_request(bfevent_t* bev);

void http_forbidden(bfevent_t* bev);
void http_internal_server_error(bfevent_t* bev);

const char* get_content_type(char *extension);

#endif