#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>

#include <netdb.h>
#include <errno.h>
#include <stdio.h>
#include <string.h>
#include <unistd.h>

#define USE_ANY_FREE_PORT 0

int main()
{

    char str[100];
    int socket_fd;
    ssize_t n;

    struct sockaddr_in servaddr;
    socklen_t addr_len;

    socket_fd = socket(AF_INET, SOCK_DGRAM, 0);

    bzero( &servaddr, sizeof(servaddr));

    servaddr.sin_family = AF_INET;
    servaddr.sin_addr.s_addr = htonl(INADDR_ANY);

#if USE_ANY_FREE_PORT
    servaddr.sin_port = 0;
#else
    servaddr.sin_port = htons(22000);
#endif

    bind(socket_fd, (struct sockaddr *) &servaddr, sizeof(servaddr));

    addr_len = sizeof(servaddr);
    if (getsockname(socket_fd, (struct sockaddr *)&servaddr, &addr_len) == -1) {
        printf("getsockname() failed");
    }
    printf("bind to %s:%hu \n", inet_ntoa(servaddr.sin_addr), ntohs(servaddr.sin_port));

    while(1)
    {
        addr_len = sizeof(servaddr);
        bzero( str, 100);

        n = recvfrom(socket_fd, str, 100, 0, (struct sockaddr*)&servaddr, &addr_len);

        if ( n == 0 ) {
            printf("EOF received, exit\n");
            break;
        }
        else if ( n < 0 && errno == EINTR ) {
            continue;
        }
        else if ( n < 0 ) {
            perror("recvfrom failed");
            break;
        }

        printf("Echoing %ld bytes back to %s:%hu - %s",n, inet_ntoa(servaddr.sin_addr), ntohs(servaddr.sin_port), str);

        sendto(socket_fd, str, n, 0, (struct sockaddr*)&servaddr, addr_len);
    }

    close(socket_fd);
}
