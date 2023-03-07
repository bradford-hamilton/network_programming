#ifndef _SOCK_SERVER_H_
#define _SOCK_SERVER_H_

#include <netdb.h>
#include "SockAddr.h"

typedef int sock_fd;

class SockServer {
public:
  SockServer(SockAddr inet_addr);

  void accept_conn();
private:
  sock_fd m_handle;
  char remote_ip[INET6_ADDRSTRLEN];
};

#endif // _SOCK_SERVER_H_

// Order of system calls for the TCP server:

// getaddrinfo();
// socket();
// bind();
// listen();
// accept()
