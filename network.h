#pragma once

#include <arpa/inet.h>
#include <algorithm>
#include <sstream>
#include <map>
#include "data.h"
#include "string_utils.h"

#define MY_PORT 5000
#define CONTENT_LENGH "content-length"
#define CONTENT_TYPE "content-type"
#define MULTYPART_BOUNDARY_PREFIX "boundary="
#define CONTENT_DISPOSITION "content-disposition"
#define TEXT_PLAIN "text/plain"
#define APPLICATION_JSON "application/json"

sockaddr_in serv_addr = {
    .sin_family = AF_INET,
    .sin_port = htons(MY_PORT),
    .sin_addr = {.s_addr = inet_addr("127.0.0.1")}
};

void send_http_response(int client, int status, const std::string& status_text, const std::string& body, std::string content_type=TEXT_PLAIN) {
    std::string response = "HTTP/1.1 " + std::to_string(status) + " " + status_text + "\r\n"
        "Content-Type: " + content_type + "\r\n"
        "Content-Length: " + std::to_string(body.size()) + "\r\n"
        "Connection: close\r\n\r\n" + body;
    send(client, response.c_str(), static_cast<int>(response.size()), 0);
}

void header_parser(const std::string& headers, KeyValueMap& header_map) {
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

std::string extract_param(const std::string& header, std::string param, int& error) {
    param = param + "=\"";
    size_t start = header.find(param);
    if (start == std::string::npos) {
        error = 0;
        return "";
    }
    start += param.length();
    size_t end = header.find_first_of("\";", start);
    if (end == std::string::npos || header[end] == ';') {
        error = -1;
        return "";
    }
    return header.substr(start, end - start);
}