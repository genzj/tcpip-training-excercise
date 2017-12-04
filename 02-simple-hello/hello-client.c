#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>

#include <netdb.h>
#include <stdio.h>
#include <stdlib.h>
#include <errno.h>
#include <string.h>
#include <unistd.h>

int main()
{

    char str[100];
    int socket_fd;
    ssize_t n;

    struct sockaddr_in addr;
    socklen_t addr_len;

    socket_fd = socket(AF_INET, SOCK_STREAM, 0);

    bzero( &addr, sizeof(addr));

    addr.sin_family = AF_INET;
    addr.sin_addr.s_addr = inet_addr("127.0.0.1");

    addr.sin_port = htons(22000);

    if (-1 == connect(socket_fd, (struct sockaddr *) &addr, sizeof(addr))) {
        perror("connect failed");
        exit(1);
    }

    addr_len = sizeof(addr);
    if (getsockname(socket_fd, (struct sockaddr *)&addr, &addr_len) == -1) {
        perror("getsockname() failed");
    }
    printf("connect from %s:%hu \n", inet_ntoa(addr.sin_addr), ntohs(addr.sin_port));

    write(socket_fd, "hello\n", 6);
    bzero( str, 100);

    if (-1 == (n = read(socket_fd,str,100))){
        perror("read failed");
        exit(1);
    }

    printf("Echoing %ld bytes back - %s",n, str);
    close(socket_fd);

}
