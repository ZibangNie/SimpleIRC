#ifndef CLIENT_H
#define CLIENT_H

#include <string>

class Client {
public:
    int fd;                        // Socket file descriptor
    std::string nickname;
    std::string username;
    std::string realname;
    std::string hostname;
    bool registered;
    bool disconnecting;

    Client(int socket_fd);
    void sendMessage(const std::string& message);
};

#endif // CLIENT_H
