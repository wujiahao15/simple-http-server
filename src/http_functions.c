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

int get_method_from_str(char* buf, struct http_headers_t* hdr) {
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
    return i;
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
    }
    return query_string;
}

void send_directory_to_client(int client, char* directory) {
    logger(DEBUG, "list directory: %s", directory);
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

    int sz = 0, w = 0;
    char buffer[MAX_LINE_LEN];
    struct dirent* myDir = NULL;
    sz = sprintf(buffer, HTML_BEFORE_BODY "\r\n");
    while(send(client, buffer, sz, 0) != (int)strlen(buffer));
    logger(DEBUG, HTML_BEFORE_BODY);
    while ((myDir = readdir(dir)) != NULL) {
        // skip '.' and ".." in current directory
        if (strcmp(myDir->d_name, "..") && strcmp(myDir->d_name, ".")) {
            sz = sprintf(buffer, "<a href=\"%s/%s\">%s</a><br/>\r\n", directory,
                         myDir->d_name, myDir->d_name);
            logger(DEBUG, "<a href=\"%s/%s\">%s</a><br/>", directory,
                   myDir->d_name, myDir->d_name);
            while(send(client, buffer, sz, 0) != (int)strlen(buffer));
        }
    }
    sz = sprintf(buffer, HTML_AFTER_BODY "\r\n");
    while(send(client, buffer, sz, 0) != (int)strlen(buffer));
    logger(DEBUG, HTML_AFTER_BODY);
    sz = sprintf(buffer, "\r\n");
    while(send(client, buffer, sz, 0) != (int)strlen(buffer));
    closedir(dir);
}

void send_file(int client, FILE* fp) {
    char buf[1024];
    fgets(buf, sizeof(buf), fp);
    while (!feof(fp)) {
        send(client, buf, strlen(buf), 0);
        logger(DEBUG, "%s", buf);
        fgets(buf, sizeof(buf), fp);
    }
}

void send_file_to_client(int client, char* filename) {
    FILE* fp = NULL;
    int numchars = 1;
    char buf[1024];

    // read and discard the remaining headers
    buf[0] = 'A';
    buf[1] = '\0';
    while ((numchars > 0) && strcmp("\n", buf)) {
        numchars = get_line_from_socket(client, buf);
        // logger(DEBUG, "read and discard headers");
    }

    if ((fp = fopen(filename, "r")) != NULL) {
        http_ok(client);
        send_file(client, fp);
    } else
        http_not_found(client);

    fclose(fp);
}

void recv_file_from_client(int sockfd, char* path) {}

void accept_request_handler(void* arg) {
    int client = (intptr_t)arg;

    logger(DEBUG, "Starting to parse method and url");
    // get the first header of http request,
    // which is in the format of `method url protocol_version\r\n`
    char buf[MAX_LINE_LEN];
    size_t recv_bytes = get_line_from_socket(client, buf);

    // parse method from the first header
    struct http_headers_t http_hdr = {};
    int anchor = get_method_from_str(buf, &http_hdr);
    if (http_hdr.mode == NOT_IMPLEMENT) {
        logger(DEBUG, "Method [%s] not implemented.", http_hdr.method);
        http_not_implemented(client);
        return;
    }
    logger(DEBUG, "Method: %s", http_hdr.method);

    // parse url from the first header
    char* query_string = get_url_from_str(buf + anchor, &http_hdr);
    logger(DEBUG, "url: %s", http_hdr.url);

    // get the file of the main page of html
    char path[MAX_PATH_LEN];
    if (!strcmp("/", http_hdr.url))
        sprintf(path, "htdocs");
    else
        strcpy(path, http_hdr.url);
    // if (path[strlen(path) - 1] == '/')
    //     strcat(path, "index.html");
    // logger(DEBUG, "path: %s", path);
    struct stat st;
    if (stat(path, &st) == -1) {  // get file information failed
        // read and discard the remaining headers
        while ((recv_bytes > 0) && strcmp("\n", buf))
            recv_bytes = get_line_from_socket(client, buf);
        // 404 not found
        http_not_found(client);
    } else {
        logger(DEBUG, "access path: %s", path);
        // if path is directory,
        // automatically use index.html under this folder
        if ((st.st_mode & S_IFMT) == S_IFDIR) {
            send_directory_to_client(client, path);
        } else {
            // if ((st.st_mode & S_IXUSR) || (st.st_mode & S_IXGRP) ||
            //     (st.st_mode & S_IXOTH)) {
            if (http_hdr.mode == GET) {
                // if (show_dir)
                // send_directory_to_client(client, path);
                // else
                send_file_to_client(client, path);
            } else {
                recv_file_from_client(client, path);
            }
        }
        // }
    }

    close(client);
}
