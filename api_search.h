#pragma once
#include <vector>
#include <algorithm>
#include <format>
#include "json_parser.h"
#include "data.h"
#include "ollama_service.h"

#define MAX_SEARCH_RESULT 10
#define TAG_SCORE_MUL 2

using ScoreMap = std::map<int, int>;

ScoreMap get_score(const StringSet& words, const IndexMap& index) {
    std::map<int, int> scores;

    for (auto word: words) {
        auto w_index = index.at(word);
        for (int i = 0; i < w_index.size(); i++) {
            scores[w_index[i]]++;
        }
    }

    return scores;
}

ScoreMap score_merge(const ScoreMap& tag_score, const ScoreMap& text_score) {
    auto score = text_score;

    for (const auto& [key, value]: tag_score) {
        score[key] += value * TAG_SCORE_MUL;
    }

    return score;
}

std::vector<std::pair<int, int>> search(const std::string& query) {
    StringSet words;
    size_t pos = 0;

    while (true) {
        std::string word = word_extractor(query, pos);
        if (word.empty()) break;
        words.insert(word);
    }

    auto tag_score = get_score(words, tag_index);
    auto text_score = get_score(words, text_index);

    auto score_merged = score_merge(tag_score, text_score);

    std::vector<std::pair<int, int>> scores(score_merged.begin(), score_merged.end());

    std::sort(scores.begin(), scores.end(), [](const auto& a, const auto& b) {
        return a.second > b.second;
    });

    return scores;
}

void search_request(int socket, const KeyValueMap& header_map, std::string& body) {
    if (!header_map.contains(CONTENT_LENGH) || !header_map.contains(CONTENT_TYPE)) {
        send_http_response(socket, 400, "Bad Request", "Missing Content-Length or Content-Type.\n");
        close(socket);
        return;
    }

    size_t content_length = std::stoull(header_map.at(CONTENT_LENGH));

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

    std::cout << body << std::endl;

    KeyValueMap params;
    std::string key, value;
    size_t comma_pos = body.find(','); 
    size_t open_bracket_pos = body.find('{');
    size_t close_bracket_pos = body.find('}');

    parse_json_row(&body, open_bracket_pos + 1, comma_pos - 1, key, value);
    params[key] = value;
    parse_json_row(&body, comma_pos + 1, close_bracket_pos - 1, key, value);
    params[key] = value;

    std::cout << params["query"] << " " << params["mode"] << std::endl;

    auto start = std::chrono::steady_clock::now();
    
    std::mutex mtx;
    std::condition_variable cv;

    OllamaTask ollama_task;
    ollama_task.query = params["query"];
    std::string expansion;

    ollama_task.callback = [&](OllamaTask& task) {
        {
            std::lock_guard<std::mutex> lock(mtx);
            expansion = task.response;
        }
        cv.notify_one();
    };

    ollama_service.enqueue(ollama_task);

    std::unique_lock<std::mutex> lock(mtx);
    cv.wait(lock);

    auto scores = search(params["query"] + " " + expansion);

    auto end = std::chrono::steady_clock::now();
    auto dur = std::chrono::duration_cast<std::chrono::milliseconds>(end - start).count();


    std::string json_output = "{ \"results\": [\n";

    for (int i = 0; i < std::min((size_t)MAX_SEARCH_RESULT, scores.size()); i++) {
        // { "results": [{"doc_id": "...", "score": 0.95, "snippet": "..."}], "time_ms": 56 }
        json_output.append(std::format("{{\n\"doc_id\": \"{}\",\n\"score\": {},\n\"snippet\": \"{}\"\n}}\n", 
            scores[i].first,
            scores[i].second,
            to_json_string(books[scores[i].first].text.substr(0, 500))
        ));
    }

    json_output.append(std::format("],\n\"time_ms\": {}\n}}\n", dur));

    send_http_response(socket, 200, "OK", json_output, APPLICATION_JSON);
    close(socket);
}