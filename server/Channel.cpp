#include "Channel.h"

Channel::Channel(const std::string& channel_name) : name(channel_name) {}

void Channel::broadcast(const std::string& message, Client* sender) {
    for (auto client : clients) {
        if (client != sender) {
            client->sendmessage(message);
        }
    }
}

void Channel::addClient(Client* client) {
    clients.insert(client);
}

void Channel::removeClient(Client* client) {
    clients.erase(client);
}
