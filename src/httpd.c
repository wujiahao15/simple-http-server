#include <pthread.h>
#include "http_functions.h"
#include "logger.h"

int main() {
    // create socket
    int sockfd = http_init();
    logger(INFO, "HTTP server is running on localhost:%d", SERVER_PORT);

    // accept connection from client
    int cs = -1;
    struct sockaddr_in clnt_addr;
    socklen_t addr_size = (socklen_t)sizeof(clnt_addr);

    // create pthread
    while(1) {
        pthread_t newthread;
        if ((cs = accept(sockfd, (struct sockaddr*)&clnt_addr, &addr_size)) < 0)
            logger(ERROR, "accept failed");
        logger(INFO, "Accept a connection from client.");
        if (pthread_create(&newthread, NULL, (void*)accept_request_handler,
                        (void*)(intptr_t)cs) != 0)
            logger(ERROR, "fail to create pthread");
    }
    // close sockets
    close(sockfd);
    return 0;
}