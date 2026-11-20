#pragma once

#include <format>

void stats_request(int socket, const KeyValueMap& header_map, std::string& body) {
    std::string response_body = std::format(
        "{{ \"documents\": {}, \"index_build_time_ms\": {}, \"threads_used\": {} }}", 
        index_stat.documents, 
        index_stat.index_build_time_ms,
        index_stat.threads_used
    );

    send_http_response(socket, 200, "OK", response_body, APPLICATION_JSON);
    close(socket);
}
