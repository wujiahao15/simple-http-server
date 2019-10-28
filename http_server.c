#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <unistd.h>
#include <arpa/inet.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include "logger.h"

#define SERVER_PORT 12306

int main() {
    // create socket
    int sockfd;
    if ((sockfd = socket(AF_INET, SOCK_STREAM, 0)) < 0)
        log(ERROR, "Creating socket failed.");

    // bind ip and port
    struct sockaddr_in serv_addr;
    memset(&serv_addr, 0, sizeof(serv_addr));
    serv_addr.sin_family = AF_INET;
    serv_addr.sin_addr.s_addr = inet_addr("127.0.0.1");
    serv_addr.sin_port = htons(SERVER_PORT);
    if (bind(sockfd, (struct sockaddr*)&serv_addr, sizeof(serv_addr)))
        log(ERROR, "bind failed");

    // listen and wait for connection from client
    listen(sockfd, 3);
    log(DEBUG, "Http server is listening port: %d", SERVER_PORT);

    // accept connection from client
    int cs;
    struct sockaddr_in clnt_addr;
    socklen_t addr_size = (socklen_t)sizeof(clnt_addr);
    if ((cs = accept(sockfd, (struct sockaddr*)&clnt_addr, &addr_size)) < 0)
        log(ERROR, "accept failed");
    log(DEBUG, "Accept a connection from client.");
    // read data from client
    char buff[100];
    if (read(cs, buff, 17) < 0)
        log(ERROR, "read failed");
    log(DEBUG, "%s", buff);

    // send data to client
    char str[] = "Msg from server!";
    if (write(cs, str, sizeof(str)) < 0)
        log(ERROR, "Write failed");

    // close sockets
    close(cs);
    close(sockfd);
    return 0;
}
