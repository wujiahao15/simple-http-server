#include <stdio.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <unistd.h>
#include "logger.h"

#define PORT 12306

int main(int argc, char* argv[]) {
    // create socket
    int sockfd;
    if ((sockfd = socket(AF_INET, SOCK_STREAM, 0)) < 0) {
        perror("create socket failed");
        exit(1);
    }
    // prepare for connection
    struct sockaddr_in addr;
    memset(&addr, 0, sizeof(struct sockaddr_in));
    addr.sin_family = AF_INET;
    addr.sin_addr.s_addr = inet_addr("127.0.0.1");
    addr.sin_port = htons(PORT);
    if (connect(sockfd, (struct sockaddr*)&addr, sizeof(addr)) < 0) {
        perror("Connection failed in client");
        exit(1);
    }

    log(DEBUG, "Server connected.");
    // send message to server
    char snd[] = "Msg from client!";
    if (write(sockfd, snd, sizeof(snd)) < 0) {
        perror("write failed");
        exit(1);
    }
    log(DEBUG, "send message to server!");
    // receive message from server
    char buff[100];
    if (read(sockfd, buff, 17) < 0) {
        perror("read failed");
        exit(1);
    }
    log(DEBUG, "%s", buff);
    // close socket
    close(sockfd);
    return 0;
}
