#include <iostream>
#include <array>
#include <iomanip>
#include <unistd.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netdb.h>
#include <stdio.h>
#include <string.h>

int main() {
  std::array<char, 1024> hostname{};
  gethostname(hostname.data(), 1023);

  std::cout << "hostname: " << hostname.data() << std::endl;

  int status = 0;
  struct addrinfo hints{};
  struct addrinfo *result = nullptr;
  struct addrinfo *addr_cursor = nullptr;
  hints.ai_family = AF_UNSPEC;
  hints.ai_socktype = SOCK_STREAM;
  hints.ai_flags = AI_CANONNAME;

  status = getaddrinfo(hostname.data(), nullptr, &hints, &result);

  if (status) {
    std::string message("Failed to resolve local hostname to discover IP: ");
    message.append(gai_strerror(status));
    std::cout << message << std::endl;
    return 1;
  }

  for (addr_cursor = result; addr_cursor != nullptr; addr_cursor = addr_cursor->ai_next) {
    if (strlen(addr_cursor->ai_canonname) > 0) {
      std::string c_host(addr_cursor->ai_canonname);
      freeaddrinfo(result);
      std::cout << "canonical hostname: " << c_host << std::endl;
      return 0;
    }
  }

  freeaddrinfo(result);
  std::cout << "canonical hostname not found" << std::endl;

  return 0;
}
