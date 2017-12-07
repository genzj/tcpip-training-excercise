#include <sys/types.h>
#include <sys/socket.h>
#include <sys/wait.h>
#include <netinet/in.h>
#include <arpa/inet.h>

#include <netdb.h>
#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#define USE_ANY_FREE_PORT 0
#define MAX_WORKER_NUMBER 4
void str_echo(int sock_fd);
void worker_loop(int listen_fd);

int main()
{

    int listen_fd;
	int stat;
    pid_t pid;

    struct sockaddr_in servaddr;
    socklen_t addr_len;

    listen_fd = socket(AF_INET, SOCK_STREAM, 0);

    bzero( &servaddr, sizeof(servaddr));

    servaddr.sin_family = AF_INET;
    servaddr.sin_addr.s_addr = htonl(INADDR_ANY);

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

    for (int i = 0; i < MAX_WORKER_NUMBER; i++) {
        if ((pid = fork()) == 0) {
            worker_loop(listen_fd);
        } else {
            printf("worker PID %u is started\n", pid);
        }
    }

again:
	while((pid = waitpid(-1, &stat , 0)) > 0)  {
		printf("child %d terminated\n", pid);
        if ((pid = fork()) == 0) {
            worker_loop(listen_fd);
        } else {
            printf("worker PID %u is started\n", pid);
        }
	}
    if (pid < 0 && errno == EINTR) goto again;
    else if (pid < 0) perror("waitpid failed");
}

void worker_loop(int listen_fd) {
    struct sockaddr_in client_addr;
    socklen_t addr_len;
    int comm_fd;

    while (1) {
        addr_len = sizeof(client_addr);
        comm_fd = accept(listen_fd, (struct sockaddr*) &client_addr, &addr_len);
        printf("PID %u is handling %s:%d\n", getpid(), inet_ntoa(client_addr.sin_addr), ntohs(client_addr.sin_port));
        str_echo(comm_fd);
        close(comm_fd);
    }
}

void str_echo(int sock_fd) {
    char str[100];
    ssize_t n;

    bzero( str, 100);

again:
    while ( (n = read(sock_fd,str,100)) > 0) {
        printf("PID %u echoing %ld bytes back - %s", getpid(), n, str);
        write(sock_fd, str, n);
    }

    if ( n == 0 ) {
        printf("PID %u EOF met, exit\n", getpid());
        return;
    }
    else if ( n < 0 && errno == EINTR ) {
        goto again;
    }
    else {
        perror("read failed.");
    }
}
