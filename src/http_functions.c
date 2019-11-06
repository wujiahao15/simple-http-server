#include "http_functions.h"
#include "logger.h"

int http_init() {
    // create socket
    int httpfd = -1;
    if ((httpfd = socket(PF_INET, SOCK_STREAM, 0)) < 0)
        logger(ERROR, "failed to create socket");

    // preparation for binding
    int on = 1;
    struct sockaddr_in name;
    memset(&name, 0, sizeof(name));
    name.sin_family = AF_INET;
    name.sin_port = htons(SERVER_PORT);
    name.sin_addr.s_addr = htonl(INADDR_ANY);
    if ((setsockopt(httpfd, SOL_SOCKET, SO_REUSEADDR, &on, sizeof(on))) < 0)
        logger(ERROR, "setsockopt failed");
    if (bind(httpfd, (struct sockaddr*)&name, sizeof(name)) < 0)
        logger(ERROR, "failed to bind");
    if (listen(httpfd, 5) < 0)
        logger(ERROR, "fail to listen");
    return httpfd;
}

int get_line_from_socket(int sockfd, char* buf) {
    int n;
    int i = 0;
    char c = '\0';

    while ((i < MAX_LINE_LEN - 1) && (c != '\n')) {
        n = recv(sockfd, &c, 1, 0);
        // logger(DEBUG, "get char = %c", c);
        if (n > 0) {
            if (c == '\r') {
                n = recv(sockfd, &c, 1, MSG_PEEK);
                if ((n > 0) && (c == '\n'))
                    recv(sockfd, &c, 1, 0);
                else
                    c = '\n';
            }
            buf[i] = c;
            i++;
        } else {
            c = '\n';
        }
    }
    buf[i] = '\0';
    return i;
}

char* get_method_from_str(char* buf, struct http_headers_t* hdr) {
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

char* get_url_from_str(char* buf, struct http_headers_t* hdr) {
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

void get_version_from_str(char* buf, struct http_headers_t* hdr) {
    int i = 0, j = 0;
    while (isSpace(buf[i]) && (i < MAX_LINE_LEN))
        i++;
    while (!isSpace(buf[i]) && (buf[i] != '\n') && (i < MAX_LINE_LEN) &&
           (j < HTTP_HDR_VERSION_LEN - 1))
        hdr->version[j++] = buf[i++];
    hdr->version[j] = '\0';
}

int get_first_header(int sockfd, http_headers_t* hdr) {
    char buf[MAX_LINE_LEN];
    // get first line of the header -> `Method Uri Version\r\n`
    // size_t recv_bytes = get_line_from_socket(sockfd, buf);
    get_line_from_socket(sockfd, buf);
    // parse method
    char* anchor = get_method_from_str(buf, hdr);
    logger(DEBUG, "Method: %s", hdr->method);
    if (hdr->mode == NOT_IMPLEMENT) {
        logger(DEBUG, "Method [%s] not implemented.", hdr->method);
        http_not_implemented(sockfd);
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

int get_other_headers(int sockfd, http_headers_t* hdr) {
    char buf[MAX_LINE_LEN];
    char key[MAX_LINE_LEN];
    char value[MAX_LINE_LEN];
    size_t recv_bytes = 1;
    while ((recv_bytes > 0) && strcmp("\n", buf)) {
        recv_bytes = get_line_from_socket(sockfd, buf);
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

int parse_http_header(int sockfd, http_headers_t* hdr) {
    if (get_first_header(sockfd, hdr) < 0)
        return -1;
    if (get_other_headers(sockfd, hdr) < 0)
        return -1;
    return 0;
}

void send_directory_to_client(int client, char* directory) {
    logger(DEBUG, "list directory: %s", directory);
    int i = strlen(directory) - 1;
    char cur_dir[MAX_PATH_LEN];
    while (i--) {
        if (directory[i] == '/')
            break;
    }
    strcpy(cur_dir, directory + 1 + i);
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
    send(client, buffer, sz, 0);
    logger(DEBUG, "%s", buffer);
    while ((myDir = readdir(dir)) != NULL) {
        // ignore '.' and ".." in current directory
        if (strcmp(myDir->d_name, "..") && strcmp(myDir->d_name, ".") &&
            strcmp(myDir->d_name, ".DS_Store")) {
            sz = sprintf(buffer, "<a href=\"%s/%s\">%s</a><br />", cur_dir,
                         myDir->d_name, myDir->d_name);
            while (send(client, buffer, sz, 0) != sz)
                ;
            logger(DEBUG, "%s", buffer);
        }
    }
    sz = sprintf(buffer, "%s", HTML_AFTER_BODY);
    send(client, buffer, sz, 0);
    logger(DEBUG, "%s", buffer);
    closedir(dir);
}

void send_file(int client, FILE* fp) {
    char buf[1024];
    fgets(buf, sizeof(buf), fp);
    while (!feof(fp)) {
        send(client, buf, strlen(buf), 0);
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

void send_file_to_client(int client, char* file_name) {
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

void recv_file_from_client(int sockfd, char* path, http_headers_t* hdr) {
    logger(DEBUG, "receiving file from client.");
    if (hdr->length == 0 && hdr->mode == POST) {
        logger(DEBUG, "Receive file => length == 0");
        http_internal_server_error(sockfd);
        return;
    }
    FILE* fp = NULL;
    if ((fp = fopen(path, "w")) == NULL) {
        logger(DEBUG, "Failed to create file %s", path);
        http_internal_server_error(sockfd);
        return;
    }
    char buf[MAX_LINE_LEN];
    char start_bound[MAX_LINE_LEN];
    char end_bound[MAX_LINE_LEN];
    int bytes_received = 0, is_file_body = 0;
    strcat(start_bound, "--");
    strcat(start_bound, hdr->boundary);
    strcat(start_bound, "\n");
    strcat(end_bound, "--");
    strcat(end_bound, hdr->boundary);
    strcat(end_bound, "--\n");
    while (bytes_received < hdr->length) {
        bytes_received += get_line_from_socket(sockfd, buf);
        logger(DEBUG, "bytes received: %d", bytes_received)
        if(!strcmp(buf, end_bound)) {
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
    http_ok(sockfd);
    fclose(fp);
}

void get_file_path_on_server(char* path, http_headers_t* hdr) {
    strcpy(path, SERVER_ROOT_DIR);
    if (!strcmp("/", hdr->url)) {
        strcat(path, hdr->url);
        strcat(path, "index.html");
    } else
        strcat(path, hdr->url);
}

void accept_request_handler(void* arg) {
    int client = (intptr_t)arg;
    http_headers_t http_hdr;
    memset(&http_hdr, 0, sizeof(http_headers_t));

    // parse http headers information from sockets
    logger(DEBUG, "Starting to parse method and url");
    if (parse_http_header(client, &http_hdr) < 0) {
        close(client);
        return;
    }

    // get the file of the main page of html
    char path[MAX_PATH_LEN];
    struct stat st;
    get_file_path_on_server(path, &http_hdr);
    logger(DEBUG, "access path: %s", path);
    switch (http_hdr.mode) {
        case GET:
            if (stat(path, &st) == -1) {  // get file information failed
                http_not_found(client);   // 404 not found
                close(client);
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

    close(client);
}
