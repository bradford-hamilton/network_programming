#include "SockAddr.h"
#include "SockServer.h"

int main() {
  const char* port = "3490";
  SockAddr s_addr {port};
  SockServer srv {s_addr};

  while (1) {
    srv.accept_conn();
  }

  return 0;
}
