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

  // Clear master and temp sets
  FD_ZERO(&master);
  FD_ZERO(&read_fds);

  // Add the m_handle listener to the master set
  FD_SET(m_handle, &master);

  // Keep track of the biggest file descriptor, which so far, is this one
  fd_max = m_handle;

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

void SockServer::accept_connections()
{
  read_fds = master; // Copy it
  if (select(fd_max+1, &read_fds, NULL, NULL, NULL) == -1) {
    perror("select");
    exit(1);
  }

  char buf[256];  // Buffer for client data
  int n_bytes;

  // Run through existing connections looking for data to read
  for (auto i = 0; i <= fd_max; i++) {
    // We found one:
    if (FD_ISSET(i, &read_fds)) {
      if (i == m_handle) {
        // Handle new connection:
        struct sockaddr_storage remote_addr;
        socklen_t addr_len = sizeof(remote_addr);
        
        int new_fd = accept(m_handle, (struct sockaddr*)&remote_addr, &addr_len);
        if (new_fd == -1) {
          perror("accept");
        } else {
          FD_SET(new_fd, &master); // Add to master set
          if (new_fd > fd_max) {   // Keep track of the max
            fd_max = new_fd;
          }
          printf("selectserver: new connection from %s on socket %d\n", inet_ntop(remote_addr.ss_family, get_in_addr((struct sockaddr*)&remote_addr), remote_ip, INET6_ADDRSTRLEN), new_fd);
        }
      } else {
        // Handle data from client
        if ((n_bytes = recv(i, buf, sizeof(buf), 0)) <= 0) {
          // Got error or connection closed by client
          if (n_bytes == 0) {
            // Connection closed
            printf("selectserver: socket %d hung up\n", i);
          } else {
            perror("recv");
          }
          close(i); // Buh-bye
          FD_CLR(i, &master); // Remove from master set
        } else {
          // Got some data from the client
          for (auto j = 0; j <= fd_max; j++) {
            // Send to everyone except the listener and ourselves
            if (FD_ISSET(j, &master)) {
              if (j != m_handle && j != i) {
                if (send(j, buf, n_bytes, 0) == -1) {
                  perror("send");
                }
              }
            }
          }
        }
      }
    }
  }
}
