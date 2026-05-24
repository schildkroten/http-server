/*
 * TODO:
 *  - Add the ability to specify a directory to run the server in
 *  - When not able to generate a response send 501 NOT IMPLEMENTED
 *    ERROR instead of closing the connection
 *  - Implement more kinds of requests
 *  - Make sure its memory safe
*/

#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <string.h>
#include <errno.h>
#include <signal.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <sys/wait.h>
#include <netdb.h>
#include <arpa/inet.h>
#include <netinet/in.h>
#include "lib/parser.h"

#define PORT "8080"
#define BACKLOG 10

void handle_client(int client_fd) {
  char buffer[1024];
  int bytes_recived;

  while ((bytes_recived = recv(client_fd, buffer, sizeof(buffer), 0)) != 0) {
    if (bytes_recived == -1) {
      fprintf(stderr, "recv: %s\n", strerror(errno));
      close(client_fd);
      exit(1);
    }

    printf("RECIVED\n\n%.*s\n", bytes_recived, buffer);

    http_request_token_list *request;
    if (parse_http_request(buffer, &request) == -1) {
      fprintf(stderr, "parse_http_request failed\n");
      close(client_fd);
      exit(1);
    }

    char *response;
    if (generate_response(request, &response) == -1) {
      fprintf(stderr, "generate_response failed\n");
      close(client_fd);
      exit(1);
    }

    printf("\nRESPONSE\n\n");
    printf("%s\n", response);

    int total = 0;
    int bytes_left = strlen(response);
    int bytes_sent;

    while (bytes_left > 0) {
      bytes_sent = send(client_fd, response + total, bytes_left, 0);
      if (bytes_left == -1) {
        fprintf(stderr, "send: %s\n", strerror(errno));
        close(client_fd);
        exit(1);
      }
      total += bytes_sent;
      bytes_left -= bytes_sent;
    }

    free(response);

    memset(buffer, 0, sizeof(buffer));

    printf("\nREQUEST PROCCESED\n");
  }
}

void sigchild_handler(int s) {
  (void)s;

  int saved_errno = errno;
  
  while (waitpid(-1, NULL, WNOHANG) > 0);

  errno = saved_errno;
}

void *get_in_addr(struct sockaddr *sa) {
  if (sa->sa_family == AF_INET) {
    return &(((struct sockaddr_in*)sa)->sin_addr);
  }

  return &(((struct sockaddr_in6*)sa)->sin6_addr);
}

int main(int argc, char *argv[]) {
  int serv_fd, client_fd;

  struct addrinfo hints, *servinfo, *p;
  struct sockaddr_storage client_addr;
  socklen_t sin_size;

  struct sigaction sa;

  char s[INET6_ADDRSTRLEN];

  int yes = 1;
  int rv;

  memset(&hints, 0, sizeof(hints));
  hints.ai_family = AF_UNSPEC;
  hints.ai_socktype = SOCK_STREAM;
  hints.ai_flags = AI_PASSIVE;

  if ((rv = getaddrinfo(NULL, PORT, &hints, &servinfo)) != 0) {
    fprintf(stderr, "getaddrinfo: %s\n", gai_strerror(rv));
    exit(1);
  }

  for (p = servinfo; p != NULL; p = p->ai_next) {
    if ((serv_fd = socket(p->ai_family, p->ai_socktype, p->ai_protocol)) == -1) {
      fprintf(stderr, "socket: %s\n", strerror(errno));
      continue;
    }

    if (setsockopt(serv_fd, SOL_SOCKET, SO_REUSEADDR, &yes, sizeof(int)) == -1) {
      fprintf(stderr, "setsockopt: %s\n", strerror(errno));
      exit(1);
    }

    if (bind(serv_fd, p->ai_addr, p->ai_addrlen) == -1) {
      close(serv_fd);
      fprintf(stderr, "bind: %s\n", strerror(errno));
      continue;
    }

    break;
  }

  freeaddrinfo(servinfo);

  if (p == NULL) {
    fprintf(stderr, "server: failed to bind\n");
    exit(1);
  }

  if (listen(serv_fd, BACKLOG) == -1) {
    fprintf(stderr, "listen: %s\n", strerror(errno));
    exit(1);
  }

  sa.sa_handler = sigchild_handler;
  sigemptyset(&sa.sa_mask);
  sa.sa_flags = SA_RESTART;
  if (sigaction(SIGCHLD, &sa, NULL) == -1) {
    fprintf(stderr, "sigaction: %s\n", strerror(errno));
    exit(1);
  }

  printf("server: waiting for connections...\n");

  while (1) {
    sin_size = sizeof(client_addr);
    client_fd = accept(serv_fd, (struct sockaddr *)&client_addr, &sin_size);

    if (client_fd == -1) {
      fprintf(stderr, "accept: %s\n", strerror(errno));
      continue;
    }

    inet_ntop(client_addr.ss_family, get_in_addr((struct sockaddr *)&client_addr), s, sizeof(s));
    printf("server: got connection from %s\n", s);

    if (!fork()) {
      close(serv_fd);

      handle_client(client_fd);

      close(client_fd);
      exit(0);
    }

    close(client_fd);
  }

  return 0;
}
