#include "SockServer.h"

int main() {
  const char* port = "3490";
  SockServer srv {port};

  while (1) {
    srv.accept_connections();
  }

  return 0;
}
