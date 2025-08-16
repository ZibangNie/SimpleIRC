#include "IRCServer.h"
#include <iostream>
#include <unistd.h>
#include <arpa/inet.h>
#include <algorithm>
#include <cstring>
#include <sys/socket.h>
#include <sys/select.h>
#include <cerrno>
#include <netdb.h>

IRCServer::IRCServer() : server_fd(0), max_sd(0) {}

IRCServer::~IRCServer() {
    for (auto client : clients) {
        close(client->fd);
        delete client;
    }
    for (auto& pair : channels) {
        delete pair.second;
    }
    if (server_fd > 0) {
        close(server_fd);
    }
}

void IRCServer::start() {
    setupServerSocket();
    std::cout << "IRC Server started, listening on port " << PORT << std::endl;

    while (true) {
        prepareSelect();
        int activity = select(max_sd + 1, &readfds, NULL, NULL, NULL);

        if (activity < 0 && errno != EINTR) {
            perror("select error");
            break;
        }

        handleNewConnections();
        handleClientMessages();
    }
}

void IRCServer::setupServerSocket() {
    struct addrinfo hints, *res, *p;
    int yes = 1;
    int rv;

    memset(&hints, 0, sizeof hints);
    hints.ai_family = AF_INET6; // Use AF_INET6 for IPv6
    hints.ai_socktype = SOCK_STREAM;
    hints.ai_flags = AI_PASSIVE; // Use my IP

    if ((rv = getaddrinfo(NULL, std::to_string(PORT).c_str(), &hints, &res)) != 0) {
        std::cerr << "getaddrinfo: " << gai_strerror(rv) << std::endl;
        exit(EXIT_FAILURE);
    }

    // Loop through all the results and bind to the first we can
    for(p = res; p != NULL; p = p->ai_next) {
        if ((server_fd = socket(p->ai_family, p->ai_socktype, p->ai_protocol)) == -1) {
            perror("socket");
            continue;
        }

        // Allow both IPv4 and IPv6
        if (p->ai_family == AF_INET6) {
            if (setsockopt(server_fd, IPPROTO_IPV6, IPV6_V6ONLY, &yes, sizeof(int)) == -1) {
                perror("setsockopt");
                exit(EXIT_FAILURE);
            }
        }

        if (setsockopt(server_fd, SOL_SOCKET, SO_REUSEADDR, &yes, sizeof(int)) == -1) {
            perror("setsockopt");
            exit(EXIT_FAILURE);
        }

        if (bind(server_fd, p->ai_addr, p->ai_addrlen) == -1) {
            close(server_fd);
            perror("bind");
            continue;
        }

        break;
    }

    if (p == NULL)  {
        std::cerr << "Failed to bind" << std::endl;
        exit(EXIT_FAILURE);
    }

    freeaddrinfo(res); // All done with this structure

    if (listen(server_fd, SOMAXCONN) == -1) {
        perror("listen");
        exit(EXIT_FAILURE);
    }

    max_sd = server_fd;
}

void IRCServer::prepareSelect() {
    FD_ZERO(&readfds);
    FD_SET(server_fd, &readfds);
    max_sd = server_fd;

    for (auto client : clients) {
        int sd = client->fd;
        if (sd > 0) {
            FD_SET(sd, &readfds);
        }
        if (sd > max_sd) {
            max_sd = sd;
        }
    }
}

void IRCServer::handleNewConnections() {
    if (FD_ISSET(server_fd, &readfds)) {
        struct sockaddr_storage remoteaddr; // Generic address structure
        socklen_t addrlen = sizeof remoteaddr;

        int new_socket = accept(server_fd, (struct sockaddr *)&remoteaddr, &addrlen);
        if (new_socket == -1) {
            perror("accept");
            return;
        }

        // Create new client
        Client* new_client = new Client(new_socket);

        // Get the remote IP address
        char remoteIP[INET6_ADDRSTRLEN];
        void *addr;
        if (remoteaddr.ss_family == AF_INET) {
            struct sockaddr_in *s = (struct sockaddr_in *)&remoteaddr;
            addr = &(s->sin_addr);
        } else { // AF_INET6
            struct sockaddr_in6 *s = (struct sockaddr_in6 *)&remoteaddr;
            addr = &(s->sin6_addr);
        }
        inet_ntop(remoteaddr.ss_family, addr, remoteIP, sizeof remoteIP);
        new_client->hostname = remoteIP;

        clients.push_back(new_client);

        std::cout << "New connection, socket fd: " << new_socket
                  << ", IP: " << new_client->hostname << std::endl;

        std::string welcome = ":miniircd NOTICE AUTH :Welcome to miniircd!\r\n";
        new_client->sendMessage(welcome);
    }
}

void IRCServer::handleClientMessages() {
    char buffer[BUFFER_SIZE + 1];

    for (auto it = clients.begin(); it != clients.end();) {
        Client* client = *it;
        int sd = client->fd;

        if (FD_ISSET(sd, &readfds)) {
            int valread = recv(sd, buffer, BUFFER_SIZE, 0);

            if (valread <= 0) {
                // Client disconnected or error
                std::cout << "Client disconnected, fd: " << sd << std::endl;
                client->disconnecting = true;
            } else {
                buffer[valread] = '\0';
                std::string data(buffer);

                // Handle multiple commands separated by \r\n
                size_t pos = 0;
                std::string delimiter = "\r\n";
                while ((pos = data.find(delimiter)) != std::string::npos) {
                    std::string line = data.substr(0, pos);
                    data.erase(0, pos + delimiter.length());
                    processCommand(client, line);

                    if (client->disconnecting) {
                        // Client marked for disconnection
                        break;
                    }
                }
            }
        }

        if (client->disconnecting) {
            // Remove client from all channels
            for (auto it_channel = channels.begin(); it_channel != channels.end();) {
                Channel* channel = it_channel->second;
                channel->removeClient(client);

                // Notify other channel members
                std::string part_msg = ":" + client->nickname + " PART " + channel->name + "\r\n";
                channel->broadcast(part_msg, client);

                // If channel is empty, delete it
                if (channel->clients.empty()) {
                    delete channel;
                    it_channel = channels.erase(it_channel);
                } else {
                    ++it_channel;
                }
            }

            // Remove client
            close(sd);
            delete client;
            it = clients.erase(it);
        } else {
            ++it;
        }
    }
}

void IRCServer::disconnectClient(std::vector<Client*>::iterator& it) {
    Client* client = *it;
    std::cout << "Client disconnected, fd: " << client->fd << ", nickname: " << client->nickname << std::endl;

    // Notify other clients
    std::string quit_msg = ":" + client->nickname + " QUIT :Client disconnected\r\n";
    broadcastToAll(quit_msg, client);

    // Remove from all channels
    for (auto& pair : channels) {
        pair.second->removeClient(client);
        // Delete empty channels
        if (pair.second->clients.empty()) {
            delete pair.second;
            channels.erase(pair.first);
        }
    }

    close(client->fd);
    delete client;
    it = clients.erase(it);
}

void IRCServer::disconnectClient(Client* client) {
    client->disconnecting = true;
}

void IRCServer::processCommand(Client* client, const std::string& command_line) {
    if (command_line.empty()) return;

    std::string command;
    std::vector<std::string> params;
    parseCommand(command_line, command, params);

    if (command == "NICK") {
        handleNICK(client, params);
    } else if (command == "USER") {
        handleUSER(client, params);
    } else if (command == "PING") {
        handlePING(client, params);
    } else if (command == "JOIN") {
        handleJOIN(client, params);
    } else if (command == "PRIVMSG") {
        handlePRIVMSG(client, params);
    } else if (command == "PART") {
        handlePART(client, params);
    } else if (command == "QUIT") {
        handleQUIT(client, params);
    } else if (command == "NOTICE") {
        handleNOTICE(client, params);
    } else {
        std::string unknown = ":miniircd 421 " + client->nickname + " " + command + " :Unknown command\r\n";
        client->sendMessage(unknown);
    }
}

void IRCServer::parseCommand(const std::string& line, std::string& command, std::vector<std::string>& params) {
    size_t pos = 0;
    std::string token;
    std::string s = line;
    std::string prefix;

    // Skip leading spaces
    s.erase(0, s.find_first_not_of(" "));

    // Check for prefix
    if (!s.empty() && s[0] == ':') {
        pos = s.find(' ');
        if (pos != std::string::npos) {
            prefix = s.substr(1, pos - 1);
            s.erase(0, pos + 1);
        } else {
            // Malformed message; no command after prefix
            return;
        }
    }

    // Skip leading spaces
    s.erase(0, s.find_first_not_of(" "));

    // Extract command
    pos = s.find(' ');
    if (pos != std::string::npos) {
        command = s.substr(0, pos);
        s.erase(0, pos + 1);
    } else {
        command = s;
        return;
    }

    // Extract parameters
    while (!s.empty()) {
        // Skip leading spaces
        s.erase(0, s.find_first_not_of(" "));
        if (s.empty()) break;

        if (s[0] == ':') {
            params.push_back(s.substr(1));
            break;
        }

        pos = s.find(' ');
        if (pos != std::string::npos) {
            token = s.substr(0, pos);
            s.erase(0, pos + 1);
        } else {
            token = s;
            s.clear();
        }
        params.push_back(token);
    }
}

void IRCServer::handleNICK(Client* client, const std::vector<std::string>& params) {
    if (params.empty()) {
        std::string error = ":miniircd 431 * :No nickname given\r\n";
        client->sendMessage(error);
        return;
    }

    std::string nick = params[0];

    // Validate nickname format
    if (!isValidNickname(nick)) {
        std::string error = ":miniircd 432 * " + nick + " :Erroneous nickname\r\n";
        client->sendMessage(error);
        return;
    }

    // Check if nickname is already in use
    for (auto c : clients) {
        if (c->nickname == nick && c != client) {
            std::string error = ":miniircd 433 * " + nick + " :Nickname is already in use\r\n";
            client->sendMessage(error);
            return;
        }
    }

    // Notify others if nickname changes
    if (!client->nickname.empty() && client->registered) {
        std::string nick_change = ":" + client->nickname + " NICK :" + nick + "\r\n";
        broadcastToAll(nick_change, client);
    }

    client->nickname = nick;
    checkRegistration(client);
}

void IRCServer::handleUSER(Client* client, const std::vector<std::string>& params) {
    if (params.size() < 4) {
        std::string error = ":miniircd 461 * USER :Not enough parameters\r\n";
        client->sendMessage(error);
        return;
    }

    if (client->registered) {
        std::string error = ":miniircd 462 * :You may not reregister\r\n";
        client->sendMessage(error);
        return;
    }

    client->username = params[0];
    client->hostname = params[1]; // Typically the hostname
    client->realname = params[3];
    checkRegistration(client);
}

void IRCServer::handlePING(Client* client, const std::vector<std::string>& params) {
    if (params.empty()) {
        std::string error = ":miniircd 409 " + client->nickname + " :No origin specified\r\n";
        client->sendMessage(error);
        return;
    }
    std::string response = ":" + client->nickname + " PONG miniircd :" + params[0] + "\r\n";
    client->sendMessage(response);
}

void IRCServer::handleJOIN(Client* client, const std::vector<std::string>& params) {
    if (params.empty()) {
        std::string error = ":miniircd 461 " + client->nickname + " JOIN :Not enough parameters\r\n";
        client->sendMessage(error);
        return;
    }

    std::string channel_name = params[0];
    if (channel_name[0] != '#') {
        std::string error = ":miniircd 476 " + client->nickname + " " + channel_name + " :Invalid channel name\r\n";
        client->sendMessage(error);
        return;
    }

    if (channels.find(channel_name) == channels.end()) {
        channels[channel_name] = new Channel(channel_name);
    }

    Channel* channel = channels[channel_name];

    if (channel->clients.find(client) != channel->clients.end()) {
        // User is already in the channel
        return;
    }

    channel->addClient(client);

    std::string join_msg = ":" + client->nickname + " JOIN :" + channel_name + "\r\n";
    channel->broadcast(join_msg);

    // Send channel topic (not set in this implementation)
    std::string topic = ":miniircd 332 " + client->nickname + " " + channel_name + " :No topic is set\r\n";
    client->sendMessage(topic);

    // Send current user list
    std::string names = ":miniircd 353 " + client->nickname + " = " + channel_name + " :";
    for (auto c : channel->clients) {
        names += c->nickname + " ";
    }
    names += "\r\n";
    client->sendMessage(names);

    // End of NAMES list
    std::string end_names = ":miniircd 366 " + client->nickname + " " + channel_name + " :End of /NAMES list.\r\n";
    client->sendMessage(end_names);
}

void IRCServer::handlePART(Client* client, const std::vector<std::string>& params) {
    if (params.empty()) {
        std::string error = ":miniircd 461 " + client->nickname + " PART :Not enough parameters\r\n";
        client->sendMessage(error);
        return;
    }

    std::string channel_name = params[0];
    auto it = channels.find(channel_name);
    if (it == channels.end()) {
        std::string error = ":miniircd 403 " + client->nickname + " " + channel_name + " :No such channel\r\n";
        client->sendMessage(error);
        return;
    }

    Channel* channel = it->second;
    if (channel->clients.find(client) == channel->clients.end()) {
        std::string error = ":miniircd 442 " + client->nickname + " " + channel_name + " :You're not on that channel\r\n";
        client->sendMessage(error);
        return;
    }

    channel->removeClient(client);
    std::string part_msg = ":" + client->nickname + " PART " + channel_name + "\r\n";
    channel->broadcast(part_msg);

    if (channel->clients.empty()) {
        delete channel;
        channels.erase(it);
    }
}

void IRCServer::handlePRIVMSG(Client* client, const std::vector<std::string>& params) {
    if (params.size() < 2) {
        std::string error = ":miniircd 461 " + client->nickname + " PRIVMSG :Not enough parameters\r\n";
        client->sendMessage(error);
        return;
    }

    std::string target = params[0];
    std::string message = params[1];

    if (message.empty()) {
        std::string error = ":miniircd 412 " + client->nickname + " :No text to send\r\n";
        client->sendMessage(error);
        return;
    }

    // Message to channel
    if (target[0] == '#') {
        auto it = channels.find(target);
        if (it == channels.end()) {
            std::string error = ":miniircd 401 " + client->nickname + " " + target + " :No such nick/channel\r\n";
            client->sendMessage(error);
            return;
        }

        Channel* channel = it->second;
        if (channel->clients.find(client) == channel->clients.end()) {
            std::string error = ":miniircd 442 " + client->nickname + " " + target + " :You're not on that channel\r\n";
            client->sendMessage(error);
            return;
        }

        std::string msg = ":" + client->nickname + " PRIVMSG " + target + " :" + message + "\r\n";
        channel->broadcast(msg, client);
    }
    // Message to user
    else {
        Client* target_client = getClientByNickname(target);

        if (!target_client) {
            std::string error = ":miniircd 401 " + client->nickname + " " + target + " :No such nick/channel\r\n";
            client->sendMessage(error);
            return;
        }

        std::string msg = ":" + client->nickname + " PRIVMSG " + target + " :" + message + "\r\n";
        target_client->sendMessage(msg);
    }
}

void IRCServer::handleQUIT(Client* client, const std::vector<std::string>& params) {
    std::string quit_msg = ":" + client->nickname + " QUIT :Quit";
    if (!params.empty()) {
        quit_msg += " :" + params[0];
    }
    quit_msg += "\r\n";

    // Notify everyone
    broadcastToAll(quit_msg, client);

    // Mark client for disconnection instead of deleting immediately
    client->disconnecting = true;
}

void IRCServer::handleNOTICE(Client* client, const std::vector<std::string>& params) {
    if (params.size() < 2) {
        return; // NOTICE does not return errors
    }

    std::string target = params[0];
    std::string message = params[1];

    if (message.empty()) {
        return;
    }

    // Notice to channel
    if (target[0] == '#') {
        auto it = channels.find(target);
        if (it == channels.end()) {
            return;
        }

        Channel* channel = it->second;
        if (channel->clients.find(client) == channel->clients.end()) {
            return;
        }

        std::string msg = ":" + client->nickname + " NOTICE " + target + " :" + message + "\r\n";
        channel->broadcast(msg, client);
    }
    // Notice to user
    else {
        Client* target_client = getClientByNickname(target);

        if (!target_client) {
            return;
        }

        std::string msg = ":" + client->nickname + " NOTICE " + target + " :" + message + "\r\n";
        target_client->sendMessage(msg);
    }
}

void IRCServer::checkRegistration(Client* client) {
    if (client->registered) return;

    if (!client->nickname.empty() && !client->username.empty()) {
        client->registered = true;
        std::string welcome = ":miniircd 001 " + client->nickname + " :Welcome to the mini IRC server\r\n";
        client->sendMessage(welcome);

        // Send MOTD (Message of the Day)
        std::string motd_start = ":miniircd 375 " + client->nickname + " :- miniircd Message of the day - \r\n";
        std::string motd = ":miniircd 372 " + client->nickname + " :- Welcome to the mini IRC server!\r\n";
        std::string motd_end = ":miniircd 376 " + client->nickname + " :End of /MOTD command.\r\n";
        client->sendMessage(motd_start + motd + motd_end);
    }
}

void IRCServer::broadcastToAll(const std::string& message, Client* sender) {
    for (auto client : clients) {
        if (client != sender) {
            client->sendMessage(message);
        }
    }
}

bool IRCServer::isValidNickname(const std::string& nick) {
    if (nick.empty() || nick.length() > 9) return false;
    if (!isalpha(nick[0])) return false;
    for (char c : nick) {
        if (!isalnum(c) && c != '-' && c != '_') return false;
    }
    return true;
}

Client* IRCServer::getClientByNickname(const std::string& nickname) {
    for (auto client : clients) {
        if (client->nickname == nickname) {
            return client;
        }
    }
    return nullptr;
}
