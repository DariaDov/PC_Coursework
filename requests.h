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

    if (body.size() < content_length) {
        send_http_response(socket, 400, "Bad Request", "Incomplete request body.\n");
        close(socket);
        return;
    }

    std::string boundary_prefix = "boundary=";
    size_t b_pos = header_map.at("content-type").find(boundary_prefix);
    if (b_pos == std::string::npos) {
        send_http_response(socket, 400, "Bad Request", "Missing multipart boundary.\n");
        close(socket);
        return;
    }

    std::string boundary = "--" + header_map.at("content-type").substr(b_pos + boundary_prefix.length());

    std::string param_value;
    std::string file_data;
    size_t pos = 0;

    while ((pos = body.find(boundary, pos)) != std::string::npos) {
        if (body.compare(pos + boundary.length(), 2, "--") == 0) break;

        size_t part_header_end = body.find("\r\n\r\n", pos);
        if (part_header_end == std::string::npos) break;

        std::string headers = body.substr(pos, part_header_end - pos);
        size_t data_start = part_header_end + 4;

        size_t next_boundary = body.find("\r\n" + boundary, data_start);
        if (next_boundary == std::string::npos) break;

        std::string content = body.substr(data_start, next_boundary - data_start);

        std::map<std::string, std::string> b_header_map;
        header_parser(headers, b_header_map);
        int error = 0;
        std::string filename = extract_param(b_header_map["content-disposition"], "filename", error);
        if (error < 0) {
            send_http_response(socket, 400, "Bad Request", "Incorrect value for filename parameter.\n");
            close(socket);
            return;
        }

        std::string name = extract_param(b_header_map["content-disposition"], "name", error);
        if (error < 0) {
            send_http_response(socket, 400, "Bad Request", "Incorrect value for name parameter.\n");
            close(socket);
            return;
        }

        if (!filename.empty()) {
            file_data = content;
        } else if (!name.empty()) {
            param_value = content;
        }

        pos = next_boundary + 2;
    }

    if (file_data.size() == 0 || param_value.size() == 0) {
        send_http_response(socket, 400, "Bad Request", "Missing file or param section.\n");
        close(socket);
        return;
    }

    std::string reply;
    std::ofstream out("TEST.txt", std::ios::binary);
    if (out) {
        out.write(file_data.data(), file_data.size());
        out.close();
        reply += "Saved file:  (" + std::to_string(file_data.size()) + " bytes)\n";
    } else {
        reply += "Error: Failed to write file to disk.\n";
    }

    reply += "Parameter = " + param_value + "\n";

    send_http_response(socket, 200, "OK", reply);
    close(socket);
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