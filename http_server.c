#include "http_server.h"

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

int get_method_from_str(char* buf, char* method) {
    int i = 0;
    while (!isSpace(buf[i]) && (i < MAX_METHOD_LEN - 1)) {
        method[i] = buf[i];
        i++;
    }
    method[i] = '\0';
    return i;
}

char* get_url_from_str(char* buf, char* url, int mode) {
    int i = 0, j = 0;
    while (isSpace(buf[i]) && (i < MAX_LINE_LEN))
        i++;
    while (!isSpace(buf[i]) && (i < MAX_LINE_LEN) && (j < MAX_URL_LEN - 1))
        url[j++] = buf[i++];
    url[j] = '\0';
    // if method is GET, ignore query string when setting url
    char* query_string = NULL;
    if (mode == GET) {
        query_string = url;
        while ((*query_string != '?') && (*query_string != '\0'))
            query_string++;
        if (*query_string == '?') {
            *query_string = '\0';
            query_string++;
        }
    }
    return query_string;
}

void not_found(int client) {
    char buf[1024];
    sprintf(buf, "HTTP/1.0 404 NOT FOUND\r\n");
    send(client, buf, strlen(buf), 0);
    sprintf(buf, SERVER_STRING);
    send(client, buf, strlen(buf), 0);
    sprintf(buf, "Content-Type: text/html\r\n");
    send(client, buf, strlen(buf), 0);
    sprintf(buf, "\r\n");
    send(client, buf, strlen(buf), 0);
    sprintf(buf, "<HTML><TITLE>Not Found</TITLE>\r\n");
    send(client, buf, strlen(buf), 0);
    sprintf(buf, "<BODY><P>The server could not fulfill\r\n");
    send(client, buf, strlen(buf), 0);
    sprintf(buf, "your request because the resource specified\r\n");
    send(client, buf, strlen(buf), 0);
    sprintf(buf, "is unavailable or nonexistent.\r\n");
    send(client, buf, strlen(buf), 0);
    sprintf(buf, "</BODY></HTML>\r\n");
    send(client, buf, strlen(buf), 0);
}

void unimplemented(int client) {
    char buf[1024];
    sprintf(buf, "HTTP/1.0 501 Method Not Implemented\r\n");
    send(client, buf, strlen(buf), 0);
    sprintf(buf, SERVER_STRING);
    send(client, buf, strlen(buf), 0);
    sprintf(buf, "Content-Type: text/html\r\n");
    send(client, buf, strlen(buf), 0);
    sprintf(buf, "\r\n");
    send(client, buf, strlen(buf), 0);
    sprintf(buf, "<HTML><HEAD><TITLE>Method Not Implemented\r\n");
    send(client, buf, strlen(buf), 0);
    sprintf(buf, "</TITLE></HEAD>\r\n");
    send(client, buf, strlen(buf), 0);
    sprintf(buf, "<BODY><P>HTTP request method not supported.\r\n");
    send(client, buf, strlen(buf), 0);
    sprintf(buf, "</BODY></HTML>\r\n");
    send(client, buf, strlen(buf), 0);
}

void send_response_headers(int client, const char* filename) {
    char buf[1024];
    // TODO: could use filename to determine file type
    (void)filename;
    logger(DEBUG, "sending response headers");
    // send response back to client
    strcpy(buf, "HTTP/1.0 200 OK\r\n");
    send(client, buf, strlen(buf), 0);
    logger(DEBUG, "HTTP/1.0 200 OK");
    strcpy(buf, SERVER_STRING);
    send(client, buf, strlen(buf), 0);
    logger(DEBUG, SERVER_BASE_STR);
    sprintf(buf, "Content-Type: text/html\r\n");
    send(client, buf, strlen(buf), 0);
    logger(DEBUG, "Content-Type: text/html");
    strcpy(buf, "\r\n");
    send(client, buf, strlen(buf), 0);
}

void send_file(int client, FILE* fp) {
    char buf[1024];
    fgets(buf, sizeof(buf), fp);
    while (!feof(fp)) {
        send(client, buf, strlen(buf), 0);
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
        send_response_headers(client, filename);
        send_file(client, fp);
    } else
        not_found(client);

    fclose(fp);
}

void recv_file_from_client(int sockfd, char* path) {}

void accept_request_handler(void* arg) {
    int client = (intptr_t)arg;
    int anchor = 0, mode = 0;
    char buf[MAX_LINE_LEN];
    char method[MAX_METHOD_LEN];
    char url[MAX_URL_LEN];
    char path[MAX_PATH_LEN];
    char* query_string = NULL;
    size_t str_len;
    struct stat st;

    logger(DEBUG, "Starting to parse method and url");
    // get the first header of http request,
    // which is in the format of `method url protocol_version\r\n`
    str_len = get_line_from_socket(client, buf);

    // parse method from the first header
    anchor += get_method_from_str(buf, method);
    logger(DEBUG, "Method: %s", method);
    if (!strcasecmp(method, "GET")) {
        mode = GET;
    } else if (!strcasecmp(method, "POST")) {
        mode = POST;
    } else {
        unimplemented(client);
        logger(DEBUG, "Method not implemented.");
        return;
    }

    // parse url from the first header
    query_string = get_url_from_str(buf + anchor, url, mode);
    logger(DEBUG, "url: %s", url);

    // get the file of the main page of html
    sprintf(path, "htmldoc%s", url);
    if (path[strlen(path) - 1] == '/')
        strcat(path, "index.html");
    // logger(DEBUG, "path: %s", path);
    if (stat(path, &st) == -1) {  // get file information failed
        // read and discard the remaining headers
        while ((str_len > 0) && strcmp("\n", buf))
            str_len = get_line_from_socket(client, buf);
        // 404 not found
        not_found(client);
    } else {
        // if path is directory,
        // automatically use index.html under this folder
        if ((st.st_mode & S_IFMT) == S_IFDIR)
            strcat(path, "/index.html");
        logger(DEBUG, "file path: %s", path);
        // if ((st.st_mode & S_IXUSR) || (st.st_mode & S_IXGRP) ||
        //     (st.st_mode & S_IXOTH)) {
        if (mode == GET) {
            send_file_to_client(client, path);
        } else {
            recv_file_from_client(client, path);
        }
        // }
    }

    close(client);
}

int main() {
    // create socket
    int sockfd = http_init();
    logger(INFO, "HTTP server is running on localhost:%d", SERVER_PORT);

    // accept connection from client
    int cs = -1;
    struct sockaddr_in clnt_addr;
    socklen_t addr_size = (socklen_t)sizeof(clnt_addr);
    if ((cs = accept(sockfd, (struct sockaddr*)&clnt_addr, &addr_size)) < 0)
        logger(ERROR, "accept failed");
    logger(INFO, "Accept a connection from client.");

    // create pthread
    pthread_t newthread;
    if (pthread_create(&newthread, NULL, (void*)accept_request_handler,
                       (void*)(intptr_t)cs) != 0)
        logger(ERROR, "fail to create pthread");
    pthread_join(newthread, NULL);
    // close sockets
    close(sockfd);
    return 0;
}
