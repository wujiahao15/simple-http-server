#include "http_functions.h"
#include "logger.h"

evutil_socket_t http_init() {
    // create socket
    evutil_socket_t httpfd = -1;
    if ((httpfd = socket(PF_INET, SOCK_STREAM, 0)) < 0) {
        logger(ERROR, "failed to create socket");
        return -1;
    }

    // preparation for binding
    struct sockaddr_in name;
    memset(&name, 0, sizeof(name));
    name.sin_family = AF_INET;
    name.sin_port = htons(SERVER_PORT);  // SERVER_PORT is in http_response.h
    name.sin_addr.s_addr = htonl(INADDR_ANY);
    // make socket reuseable
    if (evutil_make_listen_socket_reuseable(httpfd) < 0) {
        logger(ERROR, "setsockfd reuseable failed.");
        return -1;
    }
    // bind socket to ip:port
    if (bind(httpfd, (struct sockaddr*)&name, sizeof(name)) < 0) {
        logger(ERROR, "failed to bind.");
        return -1;
    }
    // listen for connections
    if (listen(httpfd, 5) < 0) {
        logger(ERROR, "fail to listen.");
        return -1;
    }
    // set socket non-blocking
    evutil_make_socket_nonblocking(httpfd);
    return (httpfd);
}

int get_line_from_bufferevent(bfevent_t* bev, char* buf) {
    // param: bev buf
    // return: the length of line
    int ret = 0, i = 0;
    char c = '\0', c_b = '\0';
    // read characters one by one, and add them into buf[]
    while ((i < MAX_LINE_LEN - 1) && (c != '\n')) {
        ret = bufferevent_read(bev, &c, 1);
        // logger(DEBUG,"ret = %d, char = %c", ret, c);
        if (ret > 0) {
            // stop when encounters "\r\n"
            if ((c_b == '\r') && (c == '\n')) {
                buf[i - 1] = c;
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
    return (i);
}

char* get_method_from_str(char* buf, http_headers_t* hdr) {
    int i = 0;
    while (!isSpace(buf[i]) && (i < HTTP_HDR_METHOD_LEN - 1)) {
        hdr->method[i] = buf[i];
        i++;
    }
    hdr->method[i] = '\0';
    if (!strcasecmp(hdr->method, "GET"))
        hdr->mode = GET;
    else if (!strcasecmp(hdr->method, "POST"))
        hdr->mode = POST;
    else
        hdr->mode = NOT_IMPLEMENT;
    return (buf + i);
}

char* get_url_from_str(char* buf, http_headers_t* hdr) {
    int i = 0, j = 0;
    while (isSpace(buf[i]) && (i < MAX_LINE_LEN))
        i++;
    while (!isSpace(buf[i]) && (i < MAX_LINE_LEN) && (j < HTTP_HDR_URL_LEN - 1))
        hdr->url[j++] = buf[i++];
    hdr->url[j] = '\0';
    // if method is GET, ignore query string when setting url
    char* query_string = NULL;
    if (hdr->mode == GET) {
        query_string = hdr->url;
        while ((*query_string != '?') && (*query_string != '\0'))
            query_string++;
        if (*query_string == '?') {
            *query_string = '\0';
            query_string++;
        }
        strcpy(hdr->query, query_string);
    }
    return (buf + i);
}

void get_version_from_str(char* buf, http_headers_t* hdr) {
    int i = 0, j = 0;
    while (isSpace(buf[i]) && (i < MAX_LINE_LEN))
        i++;
    while (!isSpace(buf[i]) && (buf[i] != '\n') && (i < MAX_LINE_LEN) &&
           (j < HTTP_HDR_VERSION_LEN - 1))
        hdr->version[j++] = buf[i++];
    hdr->version[j] = '\0';
}

int get_first_header(bfevent_t* bev, http_headers_t* hdr) {
    char buf[MAX_LINE_LEN];
    // get first line of the header -> `Method URI Version\r\n`
    // size_t recv_bytes = get_line_from_bufferevent(bev, buf);
    get_line_from_bufferevent(bev, buf);
    // parse method
    char* anchor = get_method_from_str(buf, hdr);
    logger(DEBUG, "Method: %s", hdr->method);
    if (hdr->mode == NOT_IMPLEMENT) {
        logger(DEBUG, "Method [%s] not implemented.", hdr->method);
        http_not_implemented(bev);
        return -1;
    }
    // parse url
    anchor = get_url_from_str(anchor, hdr);
    logger(DEBUG, "url: %s", hdr->url);
    // parse version
    get_version_from_str(anchor, hdr);
    logger(DEBUG, "http version: %s", hdr->version);
    return 0;
}

int get_other_headers(bfevent_t* bev, http_headers_t* hdr) {
    char buf[MAX_LINE_LEN];
    char key[MAX_LINE_LEN];
    char value[MAX_LINE_LEN];
    memset(buf, 0, MAX_LINE_LEN);
    memset(key, 0, MAX_LINE_LEN);
    memset(value, 0, MAX_LINE_LEN);
    size_t recv_bytes = 1;
    // while ((recv_bytes > 0) && strcmp("\n", buf)) {
    while ((recv_bytes > 0)) {
        recv_bytes = get_line_from_bufferevent(bev, buf);
        logger(DEBUG, "line: %s", buf);
        int i = 0, j = 0;
        while ((buf[i] != ':') && (i < (MAX_LINE_LEN - 1))) {
            key[i] = buf[i];
            i++;
        }
        key[i++] = '\0';  // avoid ':'
        if (!strcasecmp(key, "Connection")) {
            while (isSpace(buf[i]) && (i < MAX_LINE_LEN))
                i++;
            while ((buf[i] != '\n') && (i < (MAX_LINE_LEN - 1)))
                value[j++] = buf[i++];
            value[j] = '\0';
            if (!strcasecmp(value, "keep-alive")) {
                hdr->alive = 1;
                logger(DEBUG, "Connection need keep alive!");
            }
        } else if (!strcasecmp(key, "Content-Type")) {
            while (isSpace(buf[i]) && (i < MAX_LINE_LEN))
                i++;
            while ((buf[i] != ';') && (i < (MAX_LINE_LEN - 1)))
                value[j++] = buf[i++];
            value[j] = '\0';
            j = 0;
            if (!strcasecmp(value, "multipart/form-data")) {
                while ((isSpace(buf[i]) || buf[i] != '=') && (i < MAX_LINE_LEN))
                    i++;
                i++;
                while ((buf[i] != '\n') && (i < (MAX_LINE_LEN - 1)))
                    hdr->boundary[j++] = buf[i++];
                hdr->boundary[j] = '\0';
                logger(DEBUG, "%s boundary: %s", hdr->method, hdr->boundary);
            }
        } else if (!strcasecmp(key, "Content-Length")) {
            while (isSpace(buf[i]) && (i < MAX_LINE_LEN))
                i++;
            while ((buf[i] != '\n') && (i < (MAX_LINE_LEN - 1)))
                value[j++] = buf[i++];
            value[j] = '\0';
            hdr->length = atoi(value);
            logger(DEBUG, "content length: %d", hdr->length);
        } else
            continue;
    }
    return 0;
}

int parse_http_header(bfevent_t* bev, http_headers_t* hdr) {
    if (get_first_header(bev, hdr) < 0)
        return -1;
    if (get_other_headers(bev, hdr) < 0)
        return -1;
    return 0;
}

void get_current_directory(char* directory, char* cur_dir) {
    int i = strlen(directory) - 1;
    while (i--) {
        if (directory[i] == '/')
            break;
    }
    strcpy(cur_dir, directory + 1 + i);
}

void send_directory_to_client(bfevent_t* client, char* directory) {
    logger(DEBUG, "list directory: %s", directory);
    char cur_dir[MAX_PATH_LEN];
    get_current_directory(directory, cur_dir);

    DIR* dir = NULL;
    if (!(dir = opendir(directory))) {
        if (errno == EACCES) {
            http_forbidden(client);
        } else {
            http_internal_server_error(client);
        }
        return;
    }

    http_ok(client);

    int sz = 0;
    char buffer[MAX_LINE_LEN];
    struct dirent* myDir = NULL;
    sz = sprintf(buffer, "%s", HTML_BEFORE_BODY);
    bufferevent_write(client, buffer, sz);
    logger(DEBUG, "%s", buffer);
    while ((myDir = readdir(dir)) != NULL) {
        // ignore '.' and ".." in current directory, plus '.DS_Store'
        if (strcmp(myDir->d_name, "..") && strcmp(myDir->d_name, ".") &&
            strcmp(myDir->d_name, ".DS_Store")) {
            sz = sprintf(buffer, "<dd>- <a href=\"%s/%s\">%s</a></dd>", cur_dir,
                         myDir->d_name, myDir->d_name);
            bufferevent_write(client, buffer, sz);
            logger(DEBUG, "%s", buffer);
        }
    }
    sz = sprintf(buffer, "%s", HTML_AFTER_BODY);
    bufferevent_write(client, buffer, sz);
    logger(DEBUG, "%s", buffer);
    closedir(dir);
}

void send_file(bfevent_t* client, FILE* fp) {
    char buf[1024];
    fgets(buf, sizeof(buf), fp);
    while (!feof(fp)) {
        // logger(DEBUG, "%s", buf);
        bufferevent_write(client, buf, strlen(buf));
        fgets(buf, sizeof(buf), fp);
    }
}

void get_file_extension(const char* file_name, char* extension) {
    int i = strlen(file_name) - 1;
    while (i--) {
        if (file_name[i] == '.')
            break;
    }
    strcpy(extension, file_name + i + 1);
}

void send_file_to_client(bfevent_t* client, char* file_name) {
    logger(DEBUG, "GET %s", file_name);

    FILE* fp = NULL;
    if ((fp = fopen(file_name, "r")) != NULL) {
        struct stat st;
        if (stat(file_name, &st) == -1) {
            fclose(fp);
            http_internal_server_error(client);
            return;
        }
        char extension[MAX_PATH_LEN];
        get_file_extension(file_name, extension);
        http_ok_send_file(client, st.st_size, extension);
        send_file(client, fp);
    } else
        http_not_found(client);

    fclose(fp);
}

void get_start_and_end_boundary(char* start, char* end, const char* boundary) {
    memset(start, 0, sizeof(char) * MAX_LINE_LEN);
    memset(end, 0, sizeof(char) * MAX_LINE_LEN);
    strcat(start, "--");
    strcat(start, boundary);
    strcat(start, "\n");
    strcat(end, "--");
    strcat(end, boundary);
    strcat(end, "--\n");
}

void recv_file_from_client(bfevent_t* bev, char* path, http_headers_t* hdr) {
    // content-length = len(--boundary)
    //                + len(--boundary--)
    //                + len(Content-Disposition:line)
    //                + len(Content-Type:line)
    //                + len(content-body)
    logger(DEBUG, "receiving file from client.");
    if (hdr->length == 0 && hdr->mode == POST) {
        logger(DEBUG, "Receive file => length == 0");
        http_internal_server_error(bev);
        return;
    }
    FILE* fp = NULL;
    if ((fp = fopen(path, "w")) == NULL) {
        logger(DEBUG, "Failed to create file %s", path);
        http_internal_server_error(bev);
        return;
    }

    // get start boundary and end boundary from header
    char start_bound[MAX_LINE_LEN];
    char end_bound[MAX_LINE_LEN];
    get_start_and_end_boundary(start_bound, end_bound, hdr->boundary);
    logger(DEBUG, "start_bound: %s", start_bound);
    logger(DEBUG, "end_bound: %s", end_bound);
    // receive file
    char buf[MAX_LINE_LEN];
    int bytes_received = 0, is_file_body = 0, count = 0;
    while (bytes_received < hdr->length) {
        bytes_received += get_line_from_bufferevent(bev, buf);
        logger(DEBUG, "bytes received: %d", bytes_received);
        logger(DEBUG, "buf: %s", buf);
        if (bytes_received == 0) {
            if (count ++ == 10) 
                break;
        }
        if (!strcmp(buf, end_bound)) {
            logger(DEBUG, "detect end boundary.");
            break;
        } else if (!strcmp(buf, start_bound)) {
            logger(DEBUG, "detect start boundary.");
            continue;
        } else if (!strcmp(buf, "\n")) {
            is_file_body = 1;
        }
        if (is_file_body)
            fputs(buf, fp);
    }
    http_ok(bev);
    fclose(fp);
}

void get_file_path_on_server(char* path, http_headers_t* hdr) {
    strcpy(path, SERVER_ROOT_DIR);
    strcat(path, hdr->url);
    if (!strcmp("/", hdr->url))
        strcat(path, "index.html");
}

void do_accept_cb(bfevent_t* client, void* arg) {
    // get client socket fd as bufferevent
    struct event_base* base = (struct event_base*)arg;

    // initialize http_headers_t struct
    http_headers_t http_hdr;
    memset(&http_hdr, 0, sizeof(http_headers_t));

    // parse http headers information from sockets
    logger(DEBUG, "Starting to parse method and url");
    if (parse_http_header(client, &http_hdr) < 0) {
        // TODO: add error handling
        return;
    }

    // get the file of the main page of html
    struct stat st;
    char path[MAX_PATH_LEN];
    get_file_path_on_server(path, &http_hdr);
    logger(DEBUG, "access path: %s", path);
    switch (http_hdr.mode) {
        case GET:
            if (stat(path, &st) == -1) {  // get file information failed
                http_not_found(client);   // 404 not found
                return;
            }
            if ((st.st_mode & S_IFMT) == S_IFDIR)
                send_directory_to_client(client, path);
            else if ((st.st_mode & S_IFMT) == S_IFREG)
                send_file_to_client(client, path);
            else
                http_not_found(client);
            break;

        case POST:
            recv_file_from_client(client, path, &http_hdr);
            break;
        default:
            http_not_implemented(client);
            break;
    }
    // FIXME: maybe we need to add sth. here
}