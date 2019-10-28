#include <stdio.h>
#include <stdlib.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <unistd.h>
#include "logger.h"

#define PORT 12306

int main() {
    // create socket
    int sockfd;
    if ((sockfd = socket(AF_INET, SOCK_STREAM, 0)) < 0) {
        log(ERROR,"create socket failed");
    }
    // prepare for connection
    struct sockaddr_in addr;
    memset(&addr, 0, sizeof(struct sockaddr_in));
    addr.sin_family = AF_INET;
    addr.sin_addr.s_addr = inet_addr("127.0.0.1");
    addr.sin_port = htons(PORT);
    if (connect(sockfd, (struct sockaddr*)&addr, sizeof(addr)) < 0) {
        log(ERROR,"Connection failed in client");
    }

    log(DEBUG, "Server connected.");
    // send message to server
    char snd[] = "Msg from client!";
    if (write(sockfd, snd, sizeof(snd)) < 0) {
        log(ERROR,"write failed");
    }
    log(DEBUG, "send message to server!");
    // receive message from server
    char buff[100];
    if (read(sockfd, buff, 17) < 0) {
        log(ERROR,"read failed");
    }
    log(DEBUG, "%s", buff);
    // close socket
    close(sockfd);
    return 0;
}
