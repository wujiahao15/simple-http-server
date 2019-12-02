#ifndef __HTTPD_H__
#define __HTTPD_H__

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <fcntl.h>
#include <unistd.h>       /* for getopt */
#include <signal.h>       /* for signal handlers */
#include <dirent.h>       /* for directory entries */
#include <event2/event.h> /* for event */
#include <event2/http.h>  /* for evhttp components */
#include <event2/listener.h>
#include <event2/buffer.h>
#include <event2/util.h>
#include <event2/keyvalq_struct.h> /* for evhttp_header */
#include <netinet/in.h>

#include "logger.h" /* for logging information */

struct options {
    int port;
    const char* docroot;
};

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
static void dump_request_cb(struct evhttp_request* req, void* arg);
static void handle_request_cb(struct evhttp_request* req, void* arg);
static void send_document_cb(struct evhttp_request* req, void* arg);

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
    logger(DEBUG, "port: %d", o.port);
    logger(DEBUG, "Doc root: %s", o.docroot);

    return o;
}

static void do_term(int sig, short events, void* arg) {
    struct event_base* base = (struct event_base*)arg;
    event_base_loopbreak(base);
    logger(ERROR, "Got %i, Terminating\n", sig);
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
    logger(DEBUG, "bound sockfd: %d", fd);

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
        logger(DEBUG, "Listening on %s:%d\n", addr, got_port);
        // evutil_snprintf(uri_root, sizeof(uri_root), "http://%s:%d",
        // addr,got_port);
    } else {
        logger(ERROR, "evutil_inet_ntop failed\n");
        return 1;
    }

    return 0;
}

static void dump_request_cb(struct evhttp_request* req, void* arg) {
    const char* cmdtype;
    struct evkeyvalq* headers;
    struct evkeyval* header;
    struct evbuffer* buf;

    switch (evhttp_request_get_command(req)) {
        case EVHTTP_REQ_GET:
            cmdtype = "GET";
            break;
        case EVHTTP_REQ_POST:
            cmdtype = "POST";
            break;
        case EVHTTP_REQ_HEAD:
            cmdtype = "HEAD";
            break;
        case EVHTTP_REQ_PUT:
            cmdtype = "PUT";
            break;
        case EVHTTP_REQ_DELETE:
            cmdtype = "DELETE";
            break;
        case EVHTTP_REQ_OPTIONS:
            cmdtype = "OPTIONS";
            break;
        case EVHTTP_REQ_TRACE:
            cmdtype = "TRACE";
            break;
        case EVHTTP_REQ_CONNECT:
            cmdtype = "CONNECT";
            break;
        case EVHTTP_REQ_PATCH:
            cmdtype = "PATCH";
            break;
        default:
            cmdtype = "unknown";
            break;
    }

    logger(DEBUG, "Received a %s request for %s\nHeaders:\n", cmdtype,
           evhttp_request_get_uri(req));

    headers = evhttp_request_get_input_headers(req);
    for (header = headers->tqh_first; header; header = header->next.tqe_next) {
        logger(DEBUG, "  %s: %s\n", header->key, header->value);
    }

    buf = evhttp_request_get_input_buffer(req);
    puts("Input data: <<<");
    while (evbuffer_get_length(buf)) {
        int n;
        char cbuf[128];
        n = evbuffer_remove(buf, cbuf, sizeof(cbuf));
        if (n > 0)
            (void)fwrite(cbuf, 1, n, stdout);
    }
    puts(">>>");

    evhttp_send_reply(req, 200, "OK", NULL);
}

const char* print_method(struct evhttp_request* req) {
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
    logger(DEBUG, "Received a %s request for %s\nHeaders:\n", cmd_type,
           evhttp_request_get_uri(req));
    return cmd_type;
}

static void handle_request_cb(struct evhttp_request* req, void* arg) {
    const char* method = print_method(req);
    if (evutil_ascii_strcasecmp(method, "POST") &&
        evutil_ascii_strcasecmp(method, "GET")) {
        // not implemented
        return;
    }

    // parse uri from http header
    const char* uri = evhttp_request_get_uri(req);
    logger(DEBUG, "Got a %s request for <%s>", method, uri);

    // decode the uri
    struct evhttp_uri* decoded = evhttp_uri_parse(uri);
    // logger(DEBUG, "uri: %s", decoded);
    if (!decoded) {
        logger(DEBUG, "It's not a good URI. Sending BADREQUEST");
        evhttp_send_error(req, HTTP_BADREQUEST, 0);
        return;
    }
}

static void send_document_cb(struct evhttp_request* req, void* arg) {
    int fd = -1;
    struct options* opt = (struct options*)arg;
    struct evbuffer* evb = NULL;
    char* whole_path = NULL;
    char* decoded_path = NULL;
    const char* uri = NULL;
    const char* path = NULL;

    if (evhttp_request_get_command(req) != EVHTTP_REQ_GET) {
        dump_request_cb(req, arg);
        return;
    }

    uri = evhttp_request_get_uri(req);
    logger(DEBUG, "Got a GET request for <%s>", uri);

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
    evutil_snprintf(whole_path, len, "%s/%s", opt->docroot, decoded_path);

    struct stat st;
    if (stat(whole_path, &st) < 0) {
        goto err;
    }

    /* This holds the content we're sending. */
    evb = evbuffer_new();

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

        evbuffer_add_printf(evb,
                            "<!DOCTYPE html>\n"
                            "<html>\n <head>\n"
                            "  <meta charset='utf-8'>\n"
                            "  <title>%s</title>\n"
                            "  <base href='%s%s'>\n"
                            " </head>\n"
                            " <body>\n"
                            "  <h1>%s</h1>\n"
                            "  <ul>\n",
                            decoded_path, /* XXX html-escape this. */
                            path,         /* XXX html-escape this? */
                            trailing_slash,
                            decoded_path /* XXX html-escape this */);

        while ((ent = readdir(d))) {
            const char* name = ent->d_name;
            evbuffer_add_printf(evb, "    <li><a href=\"%s\">%s</a>\n", name,
                                name); /* XXX escape this */
        }
        evbuffer_add_printf(evb, "</ul></body></html>\n");

        closedir(d);

        evhttp_add_header(evhttp_request_get_output_headers(req),
                          "Content-Type", "text/html");
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
        evhttp_add_header(evhttp_request_get_output_headers(req),
                          "Content-Type", type);
        evbuffer_add_file(evb, fd, 0, st.st_size);
    }

    evhttp_send_reply(req, 200, "OK", evb);
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

#endif