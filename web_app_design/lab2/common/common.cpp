#include "common.h"

#include <unistd.h>   // read, write
#include <sys/socket.h>

bool send_all(int fd, const void* buffer, std::size_t len) {
    const char* buf = static_cast<const char*>(buffer);
    std::size_t sent = 0;
    while (sent < len) {
        ssize_t n = ::send(fd, buf + sent, len - sent, 0);
        if (n <= 0) {
            return false;
        }
        sent += static_cast<std::size_t>(n);
    }
    return true;
}

bool recv_all(int fd, void* buffer, std::size_t len) {
    char* buf = static_cast<char*>(buffer);
    std::size_t recvd = 0;
    while (recvd < len) {
        ssize_t n = ::recv(fd, buf + recvd, len - recvd, 0);
        if (n <= 0) {
            return false;   // 断开或出错
        }
        recvd += static_cast<std::size_t>(n);
    }
    return true;
}