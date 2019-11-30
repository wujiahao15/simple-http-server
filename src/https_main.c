#include "http_functions.h"
#include "logger.h"

void event_cb(struct bufferevent* bev, short event, void* arg);
void accept_cb(int fd, short events, void* arg);

int main() {
    // create socket
    evutil_socket_t httpd = http_init();
    logger(INFO, "HTTP server is running on localhost:%d", SERVER_PORT);

    // create a base event
    struct event_base* base = event_base_new();

    // create an event for accepting connections
    struct event* listener =
        event_new(base, httpd, EV_READ | EV_PERSIST, accept_cb, base);
    // add listener to the base
    event_add(listener, NULL);

    event_base_dispatch(base);
    // event_base_free(base);
    return 0;
}

void event_cb(struct bufferevent* bev, short event, void* arg) {
    if (event & BEV_EVENT_EOF) {
        logger(INFO, "connection closed");
    } else if (event & BEV_EVENT_ERROR) {
        logger(INFO, "some other error");
    }
    bufferevent_free(bev);
}

void accept_cb(int fd, short events, void* arg) {
    struct sockaddr_in client;
    socklen_t len = sizeof(client);

    evutil_socket_t sockfd;
    if ((sockfd = accept(fd, (struct sockaddr*)&client, &len)) < 0) {
        logger(ERROR, "accept() failed");
        return;
    }
    evutil_make_socket_nonblocking(sockfd);

    logger(INFO, "Accept a client: %d", sockfd);

    struct event_base* base = (struct event_base*)arg;

    struct bufferevent* bev =
        bufferevent_socket_new(base, sockfd, BEV_OPT_CLOSE_ON_FREE);
    bufferevent_setcb(bev, do_accept_cb, NULL, event_cb, arg);

    bufferevent_enable(bev, EV_READ | EV_PERSIST | EV_WRITE);
}