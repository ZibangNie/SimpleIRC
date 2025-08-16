#ifndef IRCSERVER_H
#define IRCSERVER_H

#include <vector>
#include <map>
#include <string>
#include <netinet/in.h>
#include "Client.h"
#include "Channel.h"

#define PORT 6667
#define BUFFER_SIZE 512

class IRCServer {
private:
    int server_fd;
    std::vector<Client*> clients;
    std::map<std::string, Channel*> channels;

    fd_set readfds;
    int max_sd;

    void setupServerSocket();
    void prepareSelect();
    void handleNewConnections();
    void handleClientMessages();
    void disconnectClient(std::vector<Client*>::iterator& it);
    void disconnectClient(Client* client);
    void processCommand(Client* client, const std::string& command_line);
    void parseCommand(const std::string& line, std::string& command, std::vector<std::string>& params);
    void handleNICK(Client* client, const std::vector<std::string>& params);
    void handleUSER(Client* client, const std::vector<std::string>& params);
    void handlePING(Client* client, const std::vector<std::string>& params);
    void handleJOIN(Client* client, const std::vector<std::string>& params);
    void handlePART(Client* client, const std::vector<std::string>& params);
    void handlePRIVMSG(Client* client, const std::vector<std::string>& params);
    void handleQUIT(Client* client, const std::vector<std::string>& params);
    void handleNOTICE(Client* client, const std::vector<std::string>& params);
    void checkRegistration(Client* client);
    void broadcastToAll(const std::string& message, Client* sender = nullptr);
    bool isValidNickname(const std::string& nick);
    Client* getClientByNickname(const std::string& nickname);

public:
    IRCServer();
    ~IRCServer();
    void start();
};

#endif // IRCSERVER_H
