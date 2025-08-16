#ifndef CHANNEL_H
#define CHANNEL_H

#include <string>
#include <set>
#include "Client.h"

class Channel {
public:
    std::string name;
    std::set<Client*> clients;

    Channel(const std::string& channel_name);
    void broadcast(const std::string& message, Client* sender = nullptr);
    void addClient(Client* client);
    void removeClient(Client* client);
};

#endif // CHANNEL_H
