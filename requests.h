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
#include "string_utils.h"

void send_http_response(int client, int status, const std::string& status_text, const std::string& body) {
    std::string response = "HTTP/1.1 " + std::to_string(status) + " " + status_text + "\r\n"
        "Content-Type: text/plain\r\n"
        "Content-Length: " + std::to_string(body.size()) + "\r\n"
        "Connection: close\r\n\r\n" + body;
    send(client, response.c_str(), static_cast<int>(response.size()), 0);
}

void header_parser(const std::string& headers, std::map<std::string, std::string>& header_map) {
    std::istringstream headers_stream(headers);
    std::string line;

    header_map.clear();

    std::getline(headers_stream, line);
    while (std::getline(headers_stream, line)) {
        auto [key, value] = parse_key_value_string(line);
        if (key.empty()) {
            break;
        }
        std::transform(key.begin(), key.end(), key.begin(), [](unsigned char c) { return std::tolower(c); });
        header_map[key] = value;
    }
}

void index_request(int socket, const std::map<std::string, std::string>& header_map, std::string& body) {
    if (!header_map.contains("content-length") || !header_map.contains("content-type")) {
        send_http_response(socket, 400, "Bad Request", "Missing Content-Length or Content-Type.\n");
        close(socket);
        return;
    }

    size_t content_length = std::stoull(header_map.at("content-length"));

    char temp[4096];
    while (body.size() < content_length) {
        int bytes = recv(socket, temp, sizeof(temp), 0);
        if (bytes <= 0) break;
        body.append(temp, bytes);
    }
}

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