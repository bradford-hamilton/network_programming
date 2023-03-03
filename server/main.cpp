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

// ------------------------------------------------------------------

// Reference addrinfo struct with some details

// struct addrinfo {
//   int              ai_flags;     // AI_PASSIVE, AI_CANONNAME, etc.
//   int              ai_family;    // AF_INET, AF_INET6, AF_UNSPEC
//   int              ai_socktype;  // SOCK_STREAM, SOCK_DGRAM
//   int              ai_protocol;  // use 0 for "any"
//   size_t           ai_addrlen;   // size of ai_addr in bytes
//   struct sockaddr *ai_addr;      // struct sockaddr_in or _in6
//   char            *ai_canonname; // full canonical hostname

//   struct addrinfo *ai_next;      // linked list, next node
// };

// ------------------------------------------------------------------

// Order of system calls for the TCP server:

// getaddrinfo();
// socket();
// bind();
// listen();
// accept()

// ------------------------------------------------------------------

const char* port = "3490";
const int backlog {10}; // Number of connections allowed on the incoming queue when listen()ing.

void sigchld_handler(int s) {
  int saved_errno = errno;

  // waitpid() might overwrite errno, so we save and restore it:
  while (waitpid(-1, NULL, WNOHANG) > 0);

  errno = saved_errno;
}

// get sockaddr, IPv4 or IPv6:
void* get_in_addr(struct sockaddr* sa) {
  if (sa->sa_family == AF_INET) {
    return &(((struct sockaddr_in*)sa)->sin_addr);
  }
  return &(((struct sockaddr_in6*)sa)->sin6_addr);
}

int main() {
  // Listen on sock_fd, new connection on new_fd
  int sock_fd;
  int new_fd;
  struct addrinfo hints;     // We'll set this up and use it to produce servinfo
  struct addrinfo *servinfo; // Will point to the results of getaddrinfo
  struct addrinfo *p;
  struct sockaddr_storage their_addr;
  socklen_t sin_size;
  struct sigaction sa;
  int yes {1};
  char s[INET6_ADDRSTRLEN];
  int rv;

  memset(&hints, 0, sizeof(hints)); // Make sure the struct is empty
  hints.ai_family = AF_UNSPEC;      // Don't care IPv4 or IPv6
  hints.ai_socktype = SOCK_STREAM;  // TCP stream sockets
  hints.ai_flags = AI_PASSIVE;      // Fill in my IP for me

  if ((rv = getaddrinfo(NULL, port, &hints, &servinfo)) != 0) {
    fprintf(stderr, "getaddrinfo: %s\n", gai_strerror(rv));
    return 1;
  }
  // servinfo now points to a linked list of 1 or more struct addrinfos

  // Loop through all the results and bind to the first we can
  for (p = servinfo; p != NULL; p = p->ai_next) {
    // Get the socket file descriptor
    if ((sock_fd = socket(p->ai_family, p->ai_socktype, p->ai_protocol)) == -1) {
      perror("server: socket");
      continue;
    }

    // Set socket options - here we remove the "Address already in use" error message
    if (setsockopt(sock_fd, SOL_SOCKET, SO_REUSEADDR, &yes, sizeof(int)) == -1) {
      perror("setsockopt");
      exit(1);
    }
    
    // Bind the socket to the port we passed in to getaddrinfo().
    // The port number is used by the kernel to match an incoming
    // packet to a certain process’s socket descriptor
    if (bind(sock_fd, p->ai_addr, p->ai_addrlen) == -1) {
      close(sock_fd);
      perror("server: bind");
      continue;
    }

    break;
  }

  freeaddrinfo(servinfo); // All done with this structure

  if (p == NULL) {
    fprintf(stderr, "server: failed to bind\n");
    exit(1);
  }

  // Incoming connections are going to wait in this queue until you accept() them
  if (listen(sock_fd, backlog) == -1) {
    perror("listen");
    exit(1);
  }

  sa.sa_handler = sigchld_handler; // Reap all dead processes
  sigemptyset(&sa.sa_mask);
  sa.sa_flags = SA_RESTART;

  if (sigaction(SIGCHLD, &sa, NULL) == -1) {
    perror("sigaction");
    exit(1);
  }

  printf("server: waiting for connections...\n");

  // Main accept loop
  while (1) {
    sin_size = sizeof(their_addr);
    // The accept() call: someone far far away will try to connect() to your machine on a
    // port that you are listen()ing on. Their connection will be queued up waiting to be
    // accept()ed. You call accept() and you tell it to get the pending connection. It’ll
    // return to you a brand new socket file descriptor to use for this single connection.
    // The original one is still listening for more new connections, and the newly created
    // one is finally ready to send() and recv().
    new_fd = accept(sock_fd, (struct sockaddr*)&their_addr, &sin_size);
    if (new_fd == -1) {
      perror("accept");
      continue;
    }

    inet_ntop(their_addr.ss_family, get_in_addr((struct sockaddr *)&their_addr), s, sizeof(s));
    printf("server: got connection from %s\n", s);

    if (!fork()) { // This is the child process
      close(sock_fd); // Child doesn't need the listener
      if (send(new_fd, "Hello, world!", 13, 0) == -1) {
        perror("send");
      }
      close(new_fd);
      exit(0);
    }

    close(new_fd); // Parent doesn't need this
  }

  return 0;
}
