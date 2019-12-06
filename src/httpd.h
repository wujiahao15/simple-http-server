#ifndef __HTTPD_H__
#define __HTTPD_H__

#include <stdio.h>
#include <stdlib.h> /* atoi() */
#include <string.h> /* strstr() */
#include <sys/stat.h>
#include <sys/types.h>
#include <sys/socket.h>
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
#define MAX_LINE_LEN 1024
#define CHUNK_SIZE 512

struct options {
    int port;
    const char* docroot;
};

static char uri_root[512];
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

static struct options parse_opts(int argc, char** argv);
static void print_usage(FILE* out, const char* prog, int exit_code);
static void do_term(int sig, short events, void* arg);

static const char* guess_content_type(const char* path);
static int display_listen_sock(struct evhttp_bound_socket* handle);
static int handle_post_cb(struct evhttp_request* req, char* whole_path);
static void handle_request_cb(struct evhttp_request* req, void* arg);

void die_most_horribly_from_openssl_error(const char* func);
static struct bufferevent* bevcb(struct event_base* base, void* arg);

/* Try to guess a good content-type for 'path' */
static const char* guess_content_type(const char* path) {
    const char *last_period, *extension;
    const struct table_entry* ent;
    last_period = strrchr(path, '.');
    if (!last_period || strchr(last_period, '/'))
        goto not_found; /* no exension */
    extension = last_period + 1;
    for (ent = &content_type_table[0]; ent->extension; ++ent) {
        if (!evutil_ascii_strcasecmp(ent->extension, extension))
            return ent->content_type;
    }

not_found:
    return "application/misc";
}

static void print_usage(FILE* out, const char* prog, int exit_code) {
    fprintf(out, "usage: %s [-p port_num] <docroot>\n", prog);
    exit(exit_code);
}

static struct options parse_opts(int argc, char** argv) {
    int opt;

    struct options o;
    memset(&o, 0, sizeof(o));

    while ((opt = getopt(argc, argv, "p::h")) != -1) {
        switch (opt) {
            case 'p':
                o.port = atoi(optarg);
                break;
            case 'h':
                print_usage(stdout, argv[0], 0);
                break;
            default:
                fprintf(stderr, "Unknown option %c\n", opt);
                break;
        }
    }

    if (optind >= argc || (argc - optind) > 1) {
        print_usage(stdout, argv[0], 1);
    }

    o.docroot = argv[optind];
    // logger(DEBUG, "port: %d", o.port);
    logger(DEBUG, "Doc root: %s", o.docroot);

    return o;
}

static void do_term(int sig, short events, void* arg) {
    struct event_base* base = (struct event_base*)arg;
    event_base_loopbreak(base);
    printf("\n");
    logger(INFO, "Catch Control+c, Terminating...");
}

static int display_listen_sock(struct evhttp_bound_socket* handle) {
    evutil_socket_t fd;
    struct sockaddr_storage ss;
    ev_socklen_t socklen = sizeof(ss);
    char addrbuf[128];
    void* inaddr;
    const char* addr;
    int got_port = -1;

    fd = evhttp_bound_socket_get_fd(handle);
    memset(&ss, 0, sizeof(ss));
    if (getsockname(fd, (struct sockaddr*)&ss, &socklen)) {
        perror("getsockname() failed");
        return 1;
    }
    // logger(DEBUG, "Bound sockfd: %d", fd);

    if (ss.ss_family == AF_INET) {
        got_port = ntohs(((struct sockaddr_in*)&ss)->sin_port);
        inaddr = &((struct sockaddr_in*)&ss)->sin_addr;
    } else if (ss.ss_family == AF_INET6) {
        got_port = ntohs(((struct sockaddr_in6*)&ss)->sin6_port);
        inaddr = &((struct sockaddr_in6*)&ss)->sin6_addr;
    } else {
        logger(ERROR, "Weird address family %d\n", ss.ss_family);
        return 1;
    }

    addr = evutil_inet_ntop(ss.ss_family, inaddr, addrbuf, sizeof(addrbuf));
    if (addr) {
        logger(DEBUG, "Listening on %s:%d", addr, got_port);
        evutil_snprintf(uri_root, sizeof(uri_root), "http://%s:%d", addr,
                        got_port);
    } else {
        logger(ERROR, "evutil_inet_ntop failed\n");
        return 1;
    }

    return 0;
}

int get_line_from_evbuffer(struct evbuffer* buffer, char* buf) {
    int ret = 0, i = 0, is_a_line = 0;
    char c = '\0', c_b = '\0';

    // read characters one by one, and add them into buf[]
    while ((i < MAX_LINE_LEN - 1) && (c != '\n') &&
           evbuffer_get_length(buffer)) {
        ret = evbuffer_remove(buffer, &c, 1);
        // logger(DEBUG,"ret = %d, char = %c", ret, c);
        if (ret > 0) {
            // stop when encounters "\r\n"
            if ((c_b == '\r') && (c == '\n')) {
                buf[i - 1] = c;
                is_a_line = 1;
                break;
            }
            c_b = c;
            buf[i++] = c;
        } else {
            c = '\n';
        }
    }
    // add '\0' to the end of the string
    buf[i] = '\0';
    return (i + is_a_line);
}

static int handle_post_cb(struct evhttp_request* req, char* whole_path) {
    char cbuf[MAX_LINE_LEN] = {};
    char bound_begin[128] = {};
    char bound_end[128] = {};
    const char* bound_pattern = "boundary=";
    int content_length = 0, received = 0;
    int data_left = -1, total = 0;
    struct evkeyvalq* headers;
    struct evkeyval* header;
    struct evbuffer* buf;
    FILE* fp = NULL;

    /* Parse request header for content-length and boundary string. */
    headers = evhttp_request_get_input_headers(req);
    for (header = headers->tqh_first; header; header = header->next.tqe_next) {
        // logger(DEBUG, "%s: %s", header->key, header->value);
        if (!evutil_ascii_strcasecmp(header->key, "Content-Type")) {
            char* ptr =
                strstr(header->value, bound_pattern) + strlen(bound_pattern);
            logger(DEBUG, "boundary = %s", ptr);
            strncpy(bound_begin, "--", 2);
            strncat(bound_begin, ptr, strlen(ptr));
            strncpy(bound_end, bound_begin, strlen(bound_begin));
            strncat(bound_begin, "\n", 1);
            strncat(bound_end, "--\n", 3);
        } else if (!evutil_ascii_strcasecmp(header->key, "Content-Length")) {
            content_length = atoi(header->value);
            logger(DEBUG, "Content-Length = %d", content_length);
        }
    }

    /* Get content body */
    if (!(fp = fopen(whole_path, "w"))) {
        logger(ERROR, "Error open file to write in POST");
        return -1;
    }
    buf = evhttp_request_get_input_buffer(req);
    //  && evbuffer_get_length(buf)
    while (total != content_length) {
        memset(cbuf, 0, MAX_LINE_LEN);
        received = get_line_from_evbuffer(buf, cbuf);
        total += received;
        if (data_left > 0) {
            data_left -= strlen(cbuf);
            if (data_left < 0)
                cbuf[strlen(cbuf) - 1] = '\0';
            fputs(cbuf, fp);
            cbuf[strlen(cbuf) - 1] = '\0';
            logger(DEBUG, "tofile: %s", cbuf);
            continue;
        }
        // logger(DEBUG, "line: %s", cbuf);
        if (!evutil_ascii_strcasecmp(cbuf, bound_begin)) {
            // logger(DEBUG, "Content body begin.");
        } else if (!evutil_ascii_strcasecmp(cbuf, bound_end)) {
            logger(DEBUG, "received(%d) == content(%d)", total, content_length);
        }
        if (strstr(cbuf, "Content-Type:")) {
            total += get_line_from_evbuffer(buf, cbuf);
            data_left = content_length - total - strlen(bound_end) - 3;
            logger(DEBUG, "Data Length: %d", data_left);
        }
    }
    evhttp_send_reply(req, 200, "OK", NULL);
    fclose(fp);
    return 0;
}

const char* get_req_method_str(struct evhttp_request* req) {
    const char* cmd_type = NULL;
    switch (evhttp_request_get_command(req)) {
        case EVHTTP_REQ_GET:
            cmd_type = "GET";
            break;
        case EVHTTP_REQ_POST:
            cmd_type = "POST";
            break;
        case EVHTTP_REQ_HEAD:
            cmd_type = "HEAD";
            break;
        case EVHTTP_REQ_PUT:
            cmd_type = "PUT";
            break;
        case EVHTTP_REQ_DELETE:
            cmd_type = "DELETE";
            break;
        case EVHTTP_REQ_OPTIONS:
            cmd_type = "OPTIONS";
            break;
        case EVHTTP_REQ_TRACE:
            cmd_type = "TRACE";
            break;
        case EVHTTP_REQ_CONNECT:
            cmd_type = "CONNECT";
            break;
        case EVHTTP_REQ_PATCH:
            cmd_type = "PATCH";
            break;
        default:
            cmd_type = "unknown";
            break;
    }
    // logger(DEBUG, "Received a %s request for %s", cmd_type,
    //    evhttp_request_get_uri(req));
    return cmd_type;
}

void send_data_by_chunk(struct evhttp_request* req,
                        struct evbuffer* evb,
                        char* data,
                        int len) {
    evb = evbuffer_new();
    evbuffer_add(evb, data, len);
    evhttp_send_reply_chunk(req, evb);
    evbuffer_free(evb);
}

static void handle_request_cb(struct evhttp_request* req, void* arg) {
    int fd = -1;
    struct options* opt = (struct options*)arg;
    struct evbuffer* evb = NULL;
    struct evbuffer_file_segment* evb_fs = NULL;
    char* whole_path = NULL;
    char* decoded_path = NULL;
    const char* uri = NULL;
    const char* path = NULL;

    // if ((evhttp_request_get_command(req) != EVHTTP_REQ_POST) &&
    //     (evhttp_request_get_command(req) != EVHTTP_REQ_GET)) {
    //     evhttp_send_error(req, HTTP_NOTIMPLEMENTED, 0);
    //     goto done;
    // }
    /* Handling request starts. */
    const char* type = get_req_method_str(req);
    uri = evhttp_request_get_uri(req);
    logger(DEBUG, "Got a <%s> request for <%s>", type, uri);

    /* Decode the URI */
    struct evhttp_uri* decoded = evhttp_uri_parse(uri);
    if (!decoded) {
        logger(DEBUG, "It's not a good URI. Sending BADREQUEST");
        evhttp_send_error(req, HTTP_BADREQUEST, 0);
        return;
    }

    /* Let's see what path the user asked for. */
    path = evhttp_uri_get_path(decoded);
    logger(DEBUG, "path: %s", path);
    if (!path)
        path = "/";

    /* We need to decode it, to see what path the user really wanted. */
    decoded_path = evhttp_uridecode(path, 0, NULL);
    if (decoded_path == NULL)
        goto err;
    logger(DEBUG, "decoded path: %s", decoded_path);

    /* Don't allow any ".."s in the path, to avoid exposing stuff outside
     * of the docroot.  This test is both overzealous and underzealous:
     * it forbids aceptable paths like "/this/one..here", but it doesn't
     * do anything to prevent symlink following." */
    if (strstr(decoded_path, ".."))
        goto err;

    size_t len = strlen(decoded_path) + strlen(opt->docroot) + 2;
    if (!(whole_path = malloc(len))) {
        perror("malloc");
        goto err;
    }

    int offset = (decoded_path[0] == '/') ? 1 : 0;
    evutil_snprintf(whole_path, len - offset, "%s/%s", opt->docroot,
                    decoded_path + offset);
    logger(DEBUG, "whole path: %s", whole_path);

    if (evhttp_request_get_command(req) == EVHTTP_REQ_POST) {
        if (!handle_post_cb(req, whole_path))
            goto done;
        else
            goto err;
    }

    /* Below is GET handler*/
    struct stat st;
    if (stat(whole_path, &st) < 0) {
        goto err;
    }

    char tmp[MAX_LINE_LEN];
    /* This holds the content we're sending. */
    if (S_ISDIR(st.st_mode)) {
        /* If it's a directory, read the comments and make a little
         * index page */
        DIR* d;
        struct dirent* ent;
        const char* trailing_slash = "";

        if (!strlen(path) || path[strlen(path) - 1] != '/')
            trailing_slash = "/";

        if (!(d = opendir(whole_path)))
            goto err;

        evhttp_add_header(evhttp_request_get_output_headers(req),
                          "Content-Type", "text/html");

        evhttp_send_reply_start(req, HTTP_OK,
                                "Start to send directory by chunk.");

        sprintf(tmp, HTML_BEFORE_BODY, decoded_path, path, trailing_slash,
                decoded_path);
        send_data_by_chunk(req, evb, tmp, strlen(tmp));

        while ((ent = readdir(d))) {
            const char* name = ent->d_name;
            sprintf(tmp, "    <li><a href=\"%s\">%s</a>\n", name, name);
            send_data_by_chunk(req, evb, tmp, strlen(tmp));
        }
        sprintf(tmp, "</ul></body></html>\n");
        send_data_by_chunk(req, evb, tmp, strlen(tmp));

        closedir(d);
    } else {
        /* Otherwise it's a file; add it to the buffer to get
         * sent via sendfile */
        const char* type = guess_content_type(decoded_path);

        if ((fd = open(whole_path, O_RDONLY)) < 0) {
            perror("open");
            goto err;
        }

        if (fstat(fd, &st) < 0) {
            /* Make sure the length still matches, now that we
             * opened the file :/ */
            perror("fstat");
            goto err;
        }

        size_t file_size = st.st_size;
        logger(DEBUG, "File size: %d", (int)file_size);
        evhttp_add_header(evhttp_request_get_output_headers(req),
                          "Content-Type", type);
        evhttp_send_reply_start(req, HTTP_OK, "Start to send file by chunk.");
        for (off_t offset = 0; offset < file_size;) {
            size_t bytesLeft = file_size - offset;
            size_t bytesToRead =
                bytesLeft > CHUNK_SIZE ? CHUNK_SIZE : bytesLeft;
            evb_fs = evbuffer_file_segment_new(fd, offset, bytesToRead, 0);
            evbuffer_add_file_segment(evb, evb_fs, 0, -1);
            evbuffer_file_segment_free(evb_fs);
            // read(fd, tmp, bytesToRead);
            // evbuffer_add_file(evb, fd, offset, bytesToRead);
            offset += bytesToRead;
            // lseek(fd, offset, SEEK_SET);
            logger(DEBUG, "%d data sent.", (int)offset);
            // send_data_by_chunk(req, evb, tmp, strlen(tmp));
        }
        close(fd);
    }

    evhttp_send_reply_end(req);
    logger(DEBUG, "Reply end.");
    // evhttp_send_reply(req, 200, "OK", evb);
    goto done;
err:
    evhttp_send_error(req, 404, "Document was not found");
    if (fd >= 0)
        close(fd);
done:
    if (decoded)
        evhttp_uri_free(decoded);
    if (decoded_path)
        free(decoded_path);
    if (whole_path)
        free(whole_path);
    if (evb)
        evbuffer_free(evb);
}

/**
 * This callback is responsible for creating a new SSL connection
 * and wrapping it in an OpenSSL bufferevent.  This is the way
 * we implement an https server instead of a plain old http server.
 */
static struct bufferevent* bevcb(struct event_base* base, void* arg) {
    struct bufferevent* bev;
    SSL_CTX* server_ctx;
    SSL* client_ctx;

    server_ctx = (SSL_CTX*)arg;
    client_ctx = SSL_new(server_ctx);

    bev = bufferevent_openssl_socket_new(
        base, -1, client_ctx, BUFFEREVENT_SSL_ACCEPTING,
        BEV_OPT_CLOSE_ON_FREE | BEV_OPT_DEFER_CALLBACKS);
    // bufferevent_enable(bev, EV_READ);
    logger(DEBUG, "openssl socket evbuffer created");
    return bev;
}

void die_most_horribly_from_openssl_error(const char* func) {
    logger(ERROR, "%s failed:\n", func);

    /* This is the OpenSSL function that prints the contents of the
     * error stack to the specified file handle. */
    ERR_print_errors_fp(stderr);

    exit(EXIT_FAILURE);
}

#endif