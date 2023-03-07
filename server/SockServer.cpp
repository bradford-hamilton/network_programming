#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <errno.h>
#include <string.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <netdb.h>
#include <arpa/inet.h>
#include <sys/wait.h>
#include <signal.h>
#include <string>
#include "SockServer.h"
#include "SockAddr.h"

void sigchld_handler(int s)
{
  int saved_errno = errno;

  // waitpid() might overwrite errno, so we save and restore it:
  while (waitpid(-1, NULL, WNOHANG) > 0);

  errno = saved_errno;
}

// get sockaddr, IPv4 or IPv6:
void* get_in_addr(struct sockaddr* sa)
{
  if (sa->sa_family == AF_INET) {
    return &reinterpret_cast<struct sockaddr_in*>(sa)->sin_addr;
  }
  return &reinterpret_cast<struct sockaddr_in6*>(sa)->sin6_addr;
}

SockServer::SockServer(SockAddr inet_addr)
{
  struct addrinfo *p;

  // Loop through all the results and bind to the first we can
  for (p = inet_addr.servinfo(); p != NULL; p = p->ai_next) {
    // Get the socket file descriptor
    if ((m_handle = socket(p->ai_family, p->ai_socktype, p->ai_protocol)) == -1) {
      perror("server: socket");
      continue;
    }

    // Set socket options - here we remove the "Address already in use" error message
    int yes {1};
    if (setsockopt(m_handle, SOL_SOCKET, SO_REUSEADDR, &yes, sizeof(int)) == -1) {
      perror("setsockopt");
      exit(1);
    }

    // Bind the socket to the port we passed in to getaddrinfo().
    // The port number is used by the kernel to match an incoming
    // packet to a certain process’s socket descriptor
    if (bind(m_handle, p->ai_addr, p->ai_addrlen) == -1) {
      close(m_handle);
      perror("server: bind");
      continue;
    }

    break;
  }

  // freeaddrinfo(servinfo); // All done with this structure

  // If we got here, it means we didn't get bound
  if (p == NULL) {
    fprintf(stderr, "server: failed to bind\n");
    exit(1);
  }

  const int backlog {10}; // Number of connections allowed on the incoming queue when listen()ing.
  
  // Incoming connections are going to wait in this queue until you accept() them
  if (listen(m_handle, backlog) == -1) {
    perror("listen");
    exit(1);
  }

  struct sigaction sa;
  sa.sa_handler = sigchld_handler; // Reap all dead processes
  sigemptyset(&sa.sa_mask);
  sa.sa_flags = SA_RESTART;

  if (sigaction(SIGCHLD, &sa, NULL) == -1) {
    perror("sigaction");
    exit(1);
  }

  printf("server: waiting for connections...\n");
}

void SockServer::accept_conn()
{
  struct sockaddr_storage their_addr;
  socklen_t sin_size = sizeof(their_addr);
  // The accept() call: someone far far away will try to connect() to your machine on a
  // port that you are listen()ing on. Their connection will be queued up waiting to be
  // accept()ed. You call accept() and you tell it to get the pending connection. It’ll
  // return to you a brand new socket file descriptor to use for this single connection.
  // The original one is still listening for more new connections, and the newly created
  // one is finally ready to send() and recv().
  int new_fd = accept(m_handle, (struct sockaddr*)&their_addr, &sin_size);
  if (new_fd == -1) {
    perror("accept");
    // continue;
  }

  inet_ntop(their_addr.ss_family, get_in_addr((struct sockaddr*)&their_addr), remote_ip, sizeof(remote_ip));
  printf("server: got connection from %s\n", remote_ip);

  if (!fork()) { // This is the child process
    close(m_handle); // Child doesn't need the listener
    if (send(new_fd, "Hello, world!", 13, 0) == -1) {
      perror("send");
    }
    close(new_fd);
    exit(0);
  }

  close(new_fd); // Parent doesn't need this
}
