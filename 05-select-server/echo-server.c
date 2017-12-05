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
#define MAX_SOCKET_PER_WORKER 3

#define NO_FD (-1)

static int str_echo(int sock_fd);
static void worker_loop(int listen_fd);
static int fill_fd_set(int fd_table[MAX_SOCKET_PER_WORKER], fd_set *set, int include_listen);
static void init_fd_table(int fd_table[MAX_SOCKET_PER_WORKER]);
static void remove_socket(int fd_table[MAX_SOCKET_PER_WORKER], int fd, int* socket_num);
static int add_socket(int fd_table[MAX_SOCKET_PER_WORKER], int fd, int* socket_num);

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
    servaddr.sin_addr.s_addr = htons(INADDR_ANY);

#if USE_ANY_FREE_PORT
    servaddr.sin_port = 0;
#else
    servaddr.sin_port = htons(22000);
#endif

    if (bind(listen_fd, (struct sockaddr *) &servaddr, sizeof(servaddr)) < 0) {
        perror("listen failed");
        exit(1);
    }

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
            close(listen_fd);
        } else {
            printf("worker PID %u is started\n", pid);
        }
	}
    if (pid < 0 && errno == EINTR) goto again;
    else if (pid < 0) perror("waitpid failed");
}

static void worker_loop(int listen_fd) {
    struct sockaddr_in client_addr;
    socklen_t addr_len;
    int comm_fd;
    fd_set rset;
	int maxfd;
    int fd_table[MAX_SOCKET_PER_WORKER];
    int socket_num = 0;
    int ret;

    init_fd_table(fd_table);
    add_socket(fd_table, listen_fd, &socket_num);


    while (1) {
        maxfd = fill_fd_set(fd_table, &rset, (socket_num < MAX_SOCKET_PER_WORKER));
        ret = select(maxfd + 1, &rset, NULL, NULL, NULL);
        if (ret == 0 || (ret == -1 && errno == EINTR)) {
            continue;
        }
        else if (ret == -1) {
            perror("select failed");
            return;
        }

        if (FD_ISSET(listen_fd, &rset)) {
            addr_len = sizeof(client_addr);
            comm_fd = accept(listen_fd, (struct sockaddr*) &client_addr, &addr_len);
            if (-1 == add_socket(fd_table, comm_fd, &socket_num)) {
                printf("PID %u is full, close new connection %s:%d\n", getpid(), inet_ntoa(client_addr.sin_addr), ntohs(client_addr.sin_port));
                close(comm_fd);
            } else {
                printf("PID %u is handling %s:%d (%d / %d)\n", getpid(), inet_ntoa(client_addr.sin_addr), ntohs(client_addr.sin_port), socket_num, MAX_SOCKET_PER_WORKER);
            }
        }

        for (int idx = 1; idx < MAX_SOCKET_PER_WORKER; ++idx) {
            comm_fd = fd_table[idx];
            if (FD_ISSET(comm_fd, &rset)) {
                printf("PID %u FD %d is readable\n", getpid(), comm_fd);
                if (str_echo(comm_fd) <= 0) {
                    close(comm_fd);
                    remove_socket(fd_table, comm_fd, &socket_num);
                }
            }
        }
    }
}

static void init_fd_table(int fd_table[MAX_SOCKET_PER_WORKER]) {
    for (int idx = 0; idx < MAX_SOCKET_PER_WORKER; ++idx) {
        fd_table[idx] = NO_FD;
    }
}

static void remove_socket(int fd_table[MAX_SOCKET_PER_WORKER], int fd, int* socket_num) {
    if (*socket_num == 0) {
        return ;
    }
    for (int idx = 0; idx < MAX_SOCKET_PER_WORKER; ++idx) {
        if (fd_table[idx] == fd) {
            fd_table[idx] = NO_FD;
            *socket_num -= 1;
        }
    }
}

static int add_socket(int fd_table[MAX_SOCKET_PER_WORKER], int fd, int* socket_num) {
    if (*socket_num == MAX_SOCKET_PER_WORKER) {
        return -1;
    }
    for (int idx = 0; idx < MAX_SOCKET_PER_WORKER; ++idx) {
        if (fd_table[idx] == NO_FD) {
            fd_table[idx] = fd;
            *socket_num += 1;
            return idx;
        }
    }
    return -1;
}

static int fill_fd_set(int fd_table[MAX_SOCKET_PER_WORKER], fd_set *set, int include_listen) {
    int maxfd = -1;
    FD_ZERO(set);
    for (int idx = include_listen ? 0 : 1; idx < MAX_SOCKET_PER_WORKER; ++idx) {
        if (fd_table[idx] > maxfd) maxfd = fd_table[idx];
        FD_SET(fd_table[idx], set);
    }
    return maxfd;
}

static int str_echo(int sock_fd) {
    char str[100];
    ssize_t n;

    bzero( str, 100);

again:
    if ( (n = read(sock_fd,str,100)) > 0) {
        printf("PID %u echoing %ld bytes back - %s", getpid(), n, str);
        write(sock_fd, str, n);
    }

    if ( n == 0 ) {
        printf("PID %u EOF met, exit\n", getpid());
    }
    else if ( n < 0 && errno == EINTR ) {
        goto again;
    }
    else if ( n < 0 ){
        perror("read failed.");
    }
    return n;
}
