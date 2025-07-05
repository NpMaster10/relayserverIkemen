#include <iostream>
#include <thread>
#include <vector>
#include <mutex>
#include <queue>
#include <csignal>
#include <cstring>
#include <unistd.h>
#include <netinet/in.h>
#include <sys/socket.h>

#define PORT 7500

std::queue<int> pending_clients;
std::mutex queue_mutex;
bool running = true;

void shutdown_server(int) {
    running = false;
    std::cout << "\nShutting down server..." << std::endl;
}

void relay_data(int src, int dst) {
    char buffer[4096];
    ssize_t bytes;

    while ((bytes = recv(src, buffer, sizeof(buffer), 0)) > 0) {
        if (send(dst, buffer, bytes, 0) < 0) break;
    }

    shutdown(src, SHUT_RD);
    shutdown(dst, SHUT_WR);
    close(src);
    close(dst);
}

void relay_pair(int client1, int client2) {
    // Send host/guest flags
    send(client1, "\x01", 1, 0); // host
    send(client2, "\x00", 1, 0); // guest

    std::thread t1(relay_data, client1, client2);
    std::thread t2(relay_data, client2, client1);
    t1.detach();
    t2.detach();
}

void client_handler(int client_sock) {
    std::lock_guard<std::mutex> lock(queue_mutex);
    if (pending_clients.empty()) {
        std::cout << "Client " << client_sock << " is waiting for a pair." << std::endl;
        pending_clients.push(client_sock);
    } else {
        int partner = pending_clients.front();
        pending_clients.pop();
        std::cout << "Pairing client " << client_sock << " with " << partner << std::endl;
        relay_pair(partner, client_sock);
    }
}

int main() {
    signal(SIGINT, shutdown_server);
    signal(SIGTERM, shutdown_server);

    int server_fd = socket(AF_INET, SOCK_STREAM, 0);
    if (server_fd < 0) {
        perror("socket failed");
        return 1;
    }

    int opt = 1;
    setsockopt(server_fd, SOL_SOCKET, SO_REUSEADDR, &opt, sizeof(opt));

    sockaddr_in address {};
    address.sin_family = AF_INET;
    address.sin_addr.s_addr = INADDR_ANY;
    address.sin_port = htons(PORT);

    if (bind(server_fd, (struct sockaddr*)&address, sizeof(address)) < 0) {
        perror("bind failed");
        return 1;
    }

    if (listen(server_fd, 10) < 0) {
        perror("listen failed");
        return 1;
    }

    std::cout << "Relay server listening on port " << PORT << "..." << std::endl;

    while (running) {
        sockaddr_in client_addr {};
        socklen_t client_len = sizeof(client_addr);
        int client_sock = accept(server_fd, (struct sockaddr*)&client_addr, &client_len);
        if (client_sock < 0) {
            if (running) perror("accept");
            break;
        }
        std::cout << "Accepted connection: " << client_sock << std::endl;
        std::thread(client_handler, client_sock).detach();
    }

    close(server_fd);
    return 0;
}
