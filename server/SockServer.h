#ifndef _SOCK_SERVER_H_
#define _SOCK_SERVER_H_

#include <netdb.h>

typedef int sock_fd;

class SockServer {
public:
  SockServer(const char* port);
  ~SockServer();

  void accept_connections();
private:
  struct addrinfo* m_servinfo;
  sock_fd m_handle;  
  // Master file descriptor list
  fd_set master;
  // Temp file descriptor list for select()
  fd_set read_fds;
  // Maximum file descriptor number
  int fd_max;
  char remote_ip[INET6_ADDRSTRLEN];
};

#endif // _SOCK_SERVER_H_
