// POST /api/index {threads: N}
// POST /api/search (mode=keyword)
// GET /api/stats
// GET /api/health (повертає сатус 200)

#pragma once
#include <iostream>
#include <sstream>
#include <string>
#include <thread>
#include <vector>
#include <sys/socket.h>
#include <unistd.h>
#include <fstream>
#include "string_utils.h"
#include "api_index.h"
#include "api_stats.h"


void handle_request(int socket) {
    std::string buffer;
    char temp[4096];
    size_t header_end = std::string::npos;

    while ((header_end = buffer.find("\r\n\r\n")) == std::string::npos) {
        int bytes = recv(socket, temp, sizeof(temp), 0);
        if (bytes <= 0) {
            close(socket);
            return;
        }
        buffer.append(temp, bytes);
    }

    std::string raw_headers = buffer.substr(0, header_end + 2);
    std::string body = buffer.substr(header_end + 4);

    std::cout << raw_headers << std::endl;

    if (raw_headers.rfind("POST ", 0) != 0 && raw_headers.rfind("GET ", 0) != 0) {
        send_http_response(socket, 405, "Method Not Allowed", "Expected POST or GET request.\\n");
        close(socket);
        return;
    }

    std::map<std::string, std::string> header_map;
    header_parser(raw_headers, header_map);
   
    if (raw_headers.find("/api/index ", 0) != std::string::npos) {
        index_request(socket, header_map, body);
        return;
    }
    else if (raw_headers.find("/api/search ", 0) != std::string::npos) {

    }
    else if (raw_headers.find("/api/stats ", 0) != std::string::npos) {
        stats_request(socket, header_map, body);
        return;
    }
    else if (raw_headers.find("/api/health ", 0) != std::string::npos) {
        send_http_response(socket, 200, "OK", "Server is healthy.\n");
        close(socket);
        return;
    }
    else {
        send_http_response(socket, 404, "Not Found", "Endpoint not found.\n");
        close(socket);
        return;
    }
}