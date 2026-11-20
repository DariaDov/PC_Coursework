#pragma once
#include <format>
#include <regex>
#include "thread_pool.h"

struct OllamaTask {
    std::string query;
    std::string response;
    std::function<void(OllamaTask&)> callback = nullptr;
};

sockaddr_in ollama_addr = {
    .sin_family = AF_INET,
    .sin_port = htons(11434),
    .sin_addr = {.s_addr = inet_addr("127.0.0.1")}
};

using OllamaThreadPool = ThreadPool<OllamaTask>;
std::string model_name = "hf.co/s-emanuilov/query-expansion-Qwen2.5-7B-GGUF:Q8_0";

OllamaThreadPool ollama_service;

std::string parse_ollama_response(const std::string& response) {
    int start_response_pos = response.find("response\":\"") + 10;
    int end_response_pos = response.find("}\",\"done\":true") - 1;

    std::regex pattern(R"(\\"[^\\"]*\\":)");
    std::string result = std::regex_replace(response.substr(start_response_pos, end_response_pos - start_response_pos + 1), pattern, "");
    return result;
}

int ollama_worker(OllamaTask& task) {
    int sock = socket(AF_INET, SOCK_STREAM, 0);
    if (sock < 0) {
        std::cerr << "Failed to create socket\n";
        return 1;
    }

    if (connect(sock, (struct sockaddr*)&ollama_addr, sizeof(ollama_addr)) < 0) {
        std::cerr << "Connection failed. Is Ollama running on port 11434?\n";
        close(sock);
        return 1;
    }

    std::string json_body = std::format(R"({{
        "model": "{}",
        "prompt": "Expand search query: '{}'",
        "options": {{
            "num_ctx": 32768
        }},
        "format": "json",
        "thinking": false,
        "think": false,
        "keep_alive": "60m",
        "stream": false
    }})", model_name, task.query);

    std::string http_request =
        "POST /api/generate HTTP/1.1\r\n"
        "Host: localhost:11434\r\n"
        "Content-Type: application/json\r\n"
        "Content-Length: " + std::to_string(json_body.length()) + "\r\n"
        "Connection: close\r\n\r\n" + json_body;

    if (send(sock, http_request.c_str(), http_request.length(), 0) < 0) {
        std::cerr << "Failed to send request\n";
        close(sock);
        return 1;
    }

    std::string response;
    char buffer[4096];
    ssize_t bytes_received;
    while ((bytes_received = recv(sock, buffer, sizeof(buffer) - 1, 0)) > 0) {
        buffer[bytes_received] = '\0';
        response += buffer;
    }

    close(sock);

    size_t body_pos = response.find("\r\n\r\n");
    if (body_pos != std::string::npos) {
        std::string headers = response.substr(0, body_pos);
        std::string body = response.substr(body_pos + 4);
        std::cout << "--- HTTP Headers ---\n" << headers << "\n\n";
        std::cout << "--- JSON Response ---\n" << body << "\n";
        task.response = parse_ollama_response(body);
    } else {
        std::cout << response << "\n";
        task.response = "";
    }

    return 0;
}