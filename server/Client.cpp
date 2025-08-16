#include "Client.h"
#include <unistd.h>
#include <cstring>
#include <cstdio>
#include <sys/socket.h>

Client::Client(int socket_fd) : fd(socket_fd), registered(false), disconnecting(false) {}

void Client::sendMessage(const std::string& message) {
    if (send(fd, message.c_str(), message.length(), 0) == -1) {
        perror("Failed to send message");
    }
}
