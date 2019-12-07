#ifndef __HTTPD_H__
#define __HTTPD_H__

/**
 * @file httpd.h
 * @brief File that implements the main functions of https server.
 * @author Wu Jiahao
 * @version evhttp-version
 * @date 2019/12/06
 */

#include <stdio.h> /* printf() */
#include <stdlib.h> /* atoi() */
#include <string.h> /* strstr(), memeset() */
#include <sys/stat.h>
#include <sys/types.h>
#include <sys/socket.h> /* socket() */
#include <fcntl.h>  /* open() */
#include <unistd.h> /* close() */
#include <signal.h> /* for signal handlers */
#include <dirent.h> /* for directory entries */
#include <netinet/in.h>

#include <event2/event.h>
#include <event2/http.h>
#include <event2/listener.h>
#include <event2/buffer.h>
#include <event2/util.h>
#include <event2/keyvalq_struct.h>

#include <event2/bufferevent_ssl.h>
#include <openssl/ssl.h>
#include <openssl/err.h>
#include <openssl/rand.h> /* for RAND_poll() */

#include "logger.h" /* for logging information */

/// html content before the list of files when sending the directory
#define HTML_BEFORE_BODY         \
    "<!DOCTYPE html>\n"          \
    "<html>\n <head>\n"          \
    "  <meta charset='utf-8'>\n" \
    "  <title>%s</title>\n"      \
    "  <base href='%s%s'>\n"     \
    " </head>\n"                 \
    " <body>\n"                  \
    "  <h1>%s</h1>\n"            \
    "  <ul>\n"

/// The maximum length of a line
#define MAX_LINE_LEN 1024

/// The chunk size of sending data back to client
#define CHUNK_SIZE 512

/**
 * @struct options
 * Structure to hold basic running information of https server.
 */
struct options {
    int port;             ///< the port number of which the server binds to
    const char* docroot;  ///< the directory which contains the main html files of the server.
};

/// char array for output server information. (e.g. https://127.0.0.1:12306)
static char uri_root[512];

/// table_entry array to hold the map of extension to content type.
static const struct table_entry {
    const char* extension;
    const char* content_type;
} content_type_table[] = {
    {"txt", "text/plain"},
    {"c", "text/plain"},
    {"h", "text/plain"},
    {"html", "text/html"},
    {"htm", "text/htm"},
    {"css", "text/css"},
    {"gif", "image/gif"},
    {"jpg", "image/jpeg"},
    {"jpeg", "image/jpeg"},
    {"png", "image/png"},
    {"pdf", "application/pdf"},
    {"ps", "application/postscript"},
    {NULL, NULL},
};

/**
 * Parse arguments from command line, e.g. bind port and html docroot.
 *
 * @param argc argument count from command line
 * @param argv argument vector from command line
 * @return struct options
 * @see struct options
 */
struct options parse_opts(int argc, char** argv);

/**
 * Print the usage of the https server.
 *
 * @param out FILE pointer to the output file(stdout, stderr...)
 * @param prog a char pointer to the name of the program
 * @param exit_code exit code of the program
 */
void print_usage(FILE* out, const char* prog, int exit_code);

/**
 * Catch the ctrl+c signal and exit.
 *
 * @param sig signal number
 * @param events events
 * @param arg user defined context
 */
void do_term(int sig, short events, void* arg);

/**
 * Display the server information: binded ip address and port number.
 *
 * @param handle the handle of binded socket
 * @return 0 if successful, or 1 if some errors occur.
 */
int display_listen_sock(struct evhttp_bound_socket* handle);

/**
 * Find the most suitable content type of the given file extension.
 *
 * @param path the path to the file
 * @return const char pointer to the corresponding content type.
 */
const char* guess_content_type(const char* path);

/**
 * Generate method strings from the enum values of method.
 *
 * @param req the coming request
 * @return const char pointer to the method.
 * @see struct evhttp_request.
 */
const char* get_req_method_str(struct evhttp_request* req);

/**
 * Parse a line from the given evbuffer.
 *
 * Assume the max line length is MAX_LINE_LEN. If we do not
 * encounter "\r\n" which marks the end of one line when reaching the
 * maximum length of the line, we will stop and return the read data.
 *
 * @param buffer the evbuffer struct which holds the incoming data
 * @param buf the data buffer which stores the received data
 * @return the number of chars read from the evbuffer.
 * @see MAX_LINE_LEN
 */
int get_line_from_evbuffer(struct evbuffer* buffer, char* buf);

/**
 * Callback function when the server accept a new connection.
 *
 * @param req the coming request in the format of struct evhttp_request*
 * @param arg user defined context
 * @see struct evhttp_request.
 */
void handle_request_cb(struct evhttp_request* req, void* arg);

/**
 * Function to handle the POST request.
 *
 * @param req the coming request in the format of struct evhttp_request*
 * @param whole_path the whole path to store file
 * @return 0 if success, 1 if some errors occurr.
 * @see struct evhttp_request.
 */
int handle_post_helper(struct evhttp_request* req, char* whole_path);

/**
 * Function to handle the GET request.
 *
 * @param req the coming request in the format of struct evhttp_request*
 * @param path the whole path to get the file
 * @param whole_path the whole path to get the file
 * @param decoded_path the whole path to get the file
 * @return 0 if success, 1 if some errors occurr.
 * @see struct evhttp_request.
 */
int handle_get_helper(struct evhttp_request* req,
                             const char* path,
                             char* whole_path,
                             char* decoded_path);

/**
 * Send data by chunk back to the client.
 *
 * @param req the request struct which send response to client
 * @param data data to be sent
 * @param len the length of the data
 * @see struct evhttp_request.
 */
void send_data_by_chunk(struct evhttp_request* req, char* data, int len);

/**
 * Send file by chunk back to the client.
 *
 * @param req the request struct which send response to client
 * @param fd the file descriptor
 * @param offset from which to read file
 * @param len how much to read
 * @see struct evhttp_request.
 */
void send_file_by_chunk(struct evhttp_request* req,
                        int fd,
                        int64_t offset,
                        int64_t len);

/**
 * Bufferevent callback function.
 *
 * In order to supply openssl bufferevent.
 *
 * @param base the request struct which send response to client
 * @param arg user defined context, here is (SSL_CTX*)server_ctx
 * @return a pointer to struct bufferevent.
 * @see struct bufferevent.
 */
struct bufferevent* bevcb(struct event_base* base, void* arg);

#endif