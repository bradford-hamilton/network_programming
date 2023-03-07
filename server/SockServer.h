#ifndef _SOCK_SERVER_H_
#define _SOCK_SERVER_H_

#include <netdb.h>
#include "SockAddr.h"

typedef int sock_fd;

class SockServer {
public:
  SockServer(SockAddr sock_addr);

  void accept_connections();
private:
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
