#include <iostream>
#include <sys/socket.h>
#include <unistd.h>
#include <thread>
#include "index.h"
#include "network.h"
#include "requests.h"

int sock = socket(AF_INET, SOCK_STREAM, 0);

int main(int, char**){
    if (sock == -1) {
        std::cerr << "Помилка виклику socket" << std::endl;
        return 1;
    }

    int opt = 1;
    setsockopt(sock, SOL_SOCKET, SO_REUSEADDR, &opt, sizeof(opt));

    if (bind(sock, (struct sockaddr *)&serv_addr, sizeof(serv_addr)) == -1) {
        std::cerr << "Помилка виклику bind" << std::endl;
        close(sock);
        return 1;
    }

    if (listen(sock, 1) == -1) {
        std::cerr << "Помилка виклику listen" << std::endl;
        close(sock);
        return 1;
    }

    std::cout << "--Listening--" << std::endl;

    std::vector<std::thread> threads;

    while (true) {
        int new_socket = accept(sock, NULL, NULL);
        if (new_socket != -1) {
            std::cout << "--Server is starting working with client--" << std::endl;
            threads.push_back(std::thread(handle_request, new_socket));
        }
    }

    return 0;
}