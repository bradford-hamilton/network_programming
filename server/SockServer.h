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
  fd_set master;    // master file descriptor list
  fd_set read_fds;  // temp file descriptor list for select()
  int fd_max;       // maximum file descriptor number
  char remote_ip[INET6_ADDRSTRLEN];
};

#endif // _SOCK_SERVER_H_
