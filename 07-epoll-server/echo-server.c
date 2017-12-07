#include <arpa/inet.h>
#include <netinet/in.h>
#include <sys/epoll.h>
#include <sys/socket.h>
#include <sys/types.h>
#include <sys/wait.h>

#include <assert.h>
#include <errno.h>
#include <fcntl.h>
#include <netdb.h>
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
static void remove_socket(int epoll_fd, int fd, int *socket_num);
static int add_socket(int epoll_fd, int fd, int *socket_num);
static int pause_listen(int epoll_fd, int listen_fd);
static int resume_listen(int epoll_fd, int listen_fd);

int main() {

  int listen_fd;
  int stat;
  pid_t pid;
  int flags;

  struct sockaddr_in servaddr;
  socklen_t addr_len;

  listen_fd = socket(AF_INET, SOCK_STREAM, 0);

  bzero(&servaddr, sizeof(servaddr));

  servaddr.sin_family = AF_INET;
  servaddr.sin_addr.s_addr = htonl(INADDR_ANY);

#if USE_ANY_FREE_PORT
  servaddr.sin_port = 0;
#else
  servaddr.sin_port = htons(22000);
#endif

  flags = fcntl(listen_fd, F_GETFL, 0);
  fcntl(listen_fd, F_SETFL, flags | O_NONBLOCK);

  if (bind(listen_fd, (struct sockaddr *)&servaddr, sizeof(servaddr)) < 0) {
    perror("listen failed");
    exit(1);
  }

  addr_len = sizeof(servaddr);
  if (getsockname(listen_fd, (struct sockaddr *)&servaddr, &addr_len) == -1) {
    printf("getsockname() failed");
  }
  printf("listening on %s:%hu \n", inet_ntoa(servaddr.sin_addr),
         ntohs(servaddr.sin_port));

  listen(listen_fd, 10);

  for (int i = 0; i < MAX_WORKER_NUMBER; i++) {
    if ((pid = fork()) == 0) {
      worker_loop(listen_fd);
    } else {
      printf("worker PID %u is started\n", pid);
    }
  }

again:
  while ((pid = waitpid(-1, &stat, 0)) > 0) {
    printf("child %d terminated\n", pid);
    if ((pid = fork()) == 0) {
      worker_loop(listen_fd);
      close(listen_fd);
    } else {
      printf("worker PID %u is started\n", pid);
    }
  }
  if (pid < 0 && errno == EINTR)
    goto again;
  else if (pid < 0)
    perror("waitpid failed");
}

static void worker_loop(int listen_fd) {
  struct sockaddr_in client_addr;
  socklen_t addr_len;
  int comm_fd, epoll_fd;
  fd_set rset;
  int maxfd;
  struct epoll_event events[MAX_SOCKET_PER_WORKER];
  int socket_num = 0, last_num;
  int ret;

  epoll_fd = epoll_create(0xdead);
  if (epoll_fd < 0)
    perror("epoll_create failed");
  assert(epoll_fd > 0);

  add_socket(epoll_fd, listen_fd, &socket_num);

  while (1) {
    ret = epoll_wait(epoll_fd, events, MAX_SOCKET_PER_WORKER, -1);
    if (ret == 0 || (ret == -1 && errno == EINTR)) {
      continue;
    } else if (ret == -1) {
      perror("epoll failed");
      return;
    }

    for (int idx = 0; idx < ret; idx++) {

      assert(events[idx].events & EPOLLIN);

      comm_fd = events[idx].data.fd;
      printf("PID %u FD %d event 0x%08x received (%d / %d)\n", getpid(),
             comm_fd, events[idx].events, idx + 1, ret);

      if (comm_fd == listen_fd) {
        // accept new connections
        addr_len = sizeof(client_addr);
        comm_fd = accept(listen_fd, (struct sockaddr *)&client_addr, &addr_len);
        if (comm_fd == -1) {
          // other process has already accepted the connection.
          continue;
        }

        printf(
           "PID %u is handling %s:%d (%d / %d)\n",
           getpid(), inet_ntoa(client_addr.sin_addr),
           ntohs(client_addr.sin_port),
           socket_num, MAX_SOCKET_PER_WORKER
        );

        // epoll new connections
        assert(-1 != add_socket(epoll_fd, comm_fd, &socket_num));

        if (socket_num >= MAX_SOCKET_PER_WORKER) {
          // connection full
          pause_listen(epoll_fd, listen_fd);
        }

      } else {
        if (str_echo(comm_fd) <= 0) {
          last_num = socket_num;
          remove_socket(epoll_fd, comm_fd, &socket_num);
          close(comm_fd);
          if (last_num == MAX_SOCKET_PER_WORKER &&
              socket_num < MAX_SOCKET_PER_WORKER)
            resume_listen(epoll_fd, listen_fd);
        }
      }
    }
  }
  close(epoll_fd);
}

static void remove_socket(int epoll_fd, int fd, int *socket_num) {
  struct epoll_event ev = {0}; // workaround of the bug in kernels < 2.6.9
  if (*socket_num == 0) {
    return;
  }
  if (0 == epoll_ctl(epoll_fd, EPOLL_CTL_DEL, fd, &ev)) {
    *socket_num -= 1;
    printf("PID %u removes fd %d\n", getpid(), fd);
  } else {
    perror("epoll_ctl remove fd failed");
  }
}

static int add_socket(int epoll_fd, int fd, int *socket_num) {
  struct epoll_event ev = {0};
  if (*socket_num == MAX_SOCKET_PER_WORKER) {
    return -1;
  }
  ev.data.fd = fd;
  ev.events = EPOLLIN;
  if (epoll_ctl(epoll_fd, EPOLL_CTL_ADD, fd, &ev) != 0) {
    perror("epoll_ctl adds fd failed");
    return -1;
  }
  printf("PID %u add fd %d\n", getpid(), fd);
  *socket_num += 1;
  return 0;
}

static int pause_listen(int epoll_fd, int listen_fd) {
  struct epoll_event ev = {0};
  if (epoll_ctl(epoll_fd, EPOLL_CTL_DEL, listen_fd, &ev) != 0) {
    perror("epoll_ctl del listen failed");
    return -1;
  }
  printf("PID %u pause listening\n", getpid());
  return 0;
}

static int resume_listen(int epoll_fd, int listen_fd) {
  struct epoll_event ev = {0};
  ev.data.fd = listen_fd;
  ev.events = EPOLLIN;
  if (epoll_ctl(epoll_fd, EPOLL_CTL_ADD, listen_fd, &ev) != 0) {
    perror("epoll_ctl add listen failed");
    return -1;
  }
  printf("PID %u resumes listening\n", getpid());
  return 0;
}

static int str_echo(int sock_fd) {
  char str[100];
  ssize_t n;

  bzero(str, 100);

again:
  if ((n = read(sock_fd, str, 100)) > 0) {
    printf("PID %u echoing %ld bytes back - %s", getpid(), n, str);
    write(sock_fd, str, n);
  }

  if (n == 0) {
    printf("PID %u EOF met, exit\n", getpid());
  } else if (n < 0 && errno == EINTR) {
    goto again;
  } else if (n < 0) {
    perror("read failed.");
  }
  return n;
}
