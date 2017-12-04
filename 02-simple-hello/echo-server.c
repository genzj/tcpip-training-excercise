#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>

#include <netdb.h>
#include <stdio.h>
#include <string.h>
#include <unistd.h>

#define USE_ANY_FREE_PORT 0

int main()
{

    char str[100];
    int listen_fd, comm_fd;
    ssize_t n;

    struct sockaddr_in servaddr;
    socklen_t addr_len;

    listen_fd = socket(AF_INET, SOCK_STREAM, 0);

    bzero( &servaddr, sizeof(servaddr));

    servaddr.sin_family = AF_INET;
    servaddr.sin_addr.s_addr = htons(INADDR_ANY);

#if USE_ANY_FREE_PORT
    servaddr.sin_port = 0;
#else
    servaddr.sin_port = htons(22000);
#endif

    bind(listen_fd, (struct sockaddr *) &servaddr, sizeof(servaddr));

    addr_len = sizeof(servaddr);
    if (getsockname(listen_fd, (struct sockaddr *)&servaddr, &addr_len) == -1) {
        printf("getsockname() failed");
    }
    printf("listening on %s:%hu \n", inet_ntoa(servaddr.sin_addr), ntohs(servaddr.sin_port));


    listen(listen_fd, 10);

    comm_fd = accept(listen_fd, (struct sockaddr*) NULL, NULL);

    while(1)
    {
        bzero( str, 100);

        n = read(comm_fd,str,100);
        if (n == 0) {
            printf("EOF met, exit\n");
            break;
        }

        printf("Echoing %ld bytes back - %s",n, str);

        write(comm_fd, str, n);
    }

    close(comm_fd);
    close(listen_fd);
}
