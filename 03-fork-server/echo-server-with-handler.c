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
void str_echo(int sock_fd);

void sig_child(int signo)
{
	pid_t pid ;
	int stat;
	while((pid = waitpid(-1, &stat , WNOHANG)) > 0)  {
		printf("child %d terminated\n", pid);
	}
	return;
}

int main()
{

    int listen_fd, comm_fd;
    pid_t pid;

    struct sockaddr_in servaddr, client_addr;
    socklen_t addr_len;

	signal(SIGCHLD, sig_child);

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

    while (1) {
        addr_len = sizeof(client_addr);
        if (0 > (comm_fd = accept(listen_fd, (struct sockaddr*) &client_addr, &addr_len)) ) {
            if (errno == EINTR) {
                printf("interrupted, continue\n");
                continue;
            } else {
                perror("accept failed");
            }
        }
        if ((pid = fork()) == 0) {
            close(listen_fd);
            str_echo(comm_fd);
            exit(0);
        } else {
            printf("PID %u is handling %s:%d\n", pid, inet_ntoa(client_addr.sin_addr), ntohs(client_addr.sin_port));
        }
        close(comm_fd);
    }
}

void str_echo(int sock_fd) {
    char str[100];
    ssize_t n;

    bzero( str, 100);

again:
    while ( (n = read(sock_fd,str,100)) > 0) {
        printf("Echoing %ld bytes back - %s",n, str);
        write(sock_fd, str, n);
    }

    if ( n == 0 ) {
        printf("EOF met, exit\n");
        return;
    }
    else if ( n < 0 && errno == EINTR ) {
        goto again;
    }
    else {
        perror("read failed.");
    }
}
