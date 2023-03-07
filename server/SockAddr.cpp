#include <stdio.h>
#include <string.h>
#include <sys/socket.h>
#include <netdb.h>
#include "SockAddr.h"

// Set up local server on given port
SockAddr::SockAddr(const char* port)
{
  struct addrinfo hints;

  memset(&hints, 0, sizeof(hints)); // Make sure the struct is empty
  hints.ai_family = AF_UNSPEC;      // Don't care IPv4 or IPv6
  hints.ai_socktype = SOCK_STREAM;  // TCP stream sockets
  hints.ai_flags = AI_PASSIVE;      // Fill in my IP for me

  int rv = getaddrinfo(nullptr, port, &hints, &m_servinfo);
  if (rv != 0) {
    fprintf(stderr, "getaddrinfo: %s\n", gai_strerror(rv));
    // return 1;
  }
  // servinfo now points to a linked list of 1 or more struct addrinfos
}

// TODO: come back and figure out what to do/makes sense here so that this
// can't release resources a SockServer is using.
SockAddr::~SockAddr()
{
  freeaddrinfo(m_servinfo);
}

struct addrinfo* SockAddr::servinfo() const
{
  return m_servinfo;
}

