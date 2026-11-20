#pragma once
#include <iostream>
#include <sstream>
#include <string>
#include <thread>
#include <vector>
#include <cstring>
#include <sys/socket.h>
#include <unistd.h>
#include <set>
#include <map>
#include <ranges>
#include "string_utils.h"
#include "network.h"
#include "data.h"
#include "thread_pool.h"

struct IndexTask {
    size_t start_pos;
    size_t end_pos;
    const std::string* dataset;

    IndexTask() {}
    IndexTask(const IndexTask& src): start_pos(src.start_pos), end_pos(src.end_pos), dataset(src.dataset) {}
    IndexTask(IndexTask&& src): start_pos(src.start_pos), end_pos(src.end_pos), dataset(src.dataset) {}
    IndexTask(size_t start_pos, size_t end_pos, const std::string* dataset): start_pos(start_pos), end_pos(end_pos), dataset(dataset) {}

    constexpr IndexTask& operator=(const IndexTask& src) {
        this->start_pos = src.start_pos;
        this->end_pos = src.end_pos;
        this->dataset = src.dataset;
        return *this;
    }
};

int parse_json_row_key(const std::string* dataset, std::string& key, const size_t from, const size_t colon_id) {
    size_t key_start_id = from;
    auto key_end_id = colon_id - 1;
   
    // skip initial spaces
    while (key_start_id < (key_end_id - 1) && (*dataset)[key_start_id] == ' ') key_start_id++;
    
    // next should be a quote
    if ((*dataset)[key_start_id] != '"') return -1;
   
    // skip trailing spaces
    while (key_end_id > (key_start_id + 1) && (*dataset)[key_end_id] == ' ') key_end_id--;
   
    // next should be a quote
    if ((*dataset)[key_end_id] != '"') return -1;
    key = dataset->substr(key_start_id + 1, key_end_id - key_start_id - 1);
    return 0;
}

int parse_json_row_value(const std::string* dataset, std::string& value, const size_t colon_id, const size_t to) {
    auto value_start_id = colon_id + 1;
    auto value_end_id = to;

    // skip initial spaces
    while (value_start_id < (value_end_id - 1) && (*dataset)[value_start_id] == ' ') value_start_id++;
    
    // skip trailing spaces, \n, \r
    while (
	    value_end_id > (value_start_id + 1) && ((*dataset)[value_end_id] == ' ' || (*dataset)[value_end_id] == '\r' || (*dataset)[value_end_id] == '\n')
    ) value_end_id--;
    
    // skip possible trailing ","
    if ((*dataset)[value_end_id] == ',') value_end_id--;
    
    // skip other possible spaces
    while (value_end_id > (value_start_id + 1) && (*dataset)[value_end_id] == ' ') value_end_id--;
    
    // the value is quoted incorrectly
    if ((*dataset)[value_start_id] == '"' 
	    && (*dataset)[value_end_id] != '"' || (*dataset)[value_start_id] != '"' && (*dataset)[value_end_id] == '"'
    ) return -1;
    
    // return result immediately if it is not quoted
    if ((*dataset)[value_start_id] != '"') {
	    value = (*dataset).substr(value_start_id, value_end_id - value_start_id + 1);
	    return 0;
    }

    // resolve all escapes in quoted string
    value_start_id++;
    value_end_id--;

    for (auto i = value_start_id; i <= value_end_id; i++) {
        char rc = (*dataset)[i];
        if (rc == '\\') { // resolve escaped characters
            switch ((*dataset)[i+1]) {
            case '"' : rc = '"';  i++; break;
            case '\\': rc = '\\'; i++; break;
            case '/' : rc = '/';  i++; break;
            case 'n' : rc = '\n'; i++; break;
            case 'r' : rc = '\r'; i++; break;
            case 't' : rc = '\t'; i++; break;
            case 'b' : rc = '\b'; i++; break;
            case 'f' : rc = '\f'; i++; break;
            }
        }

        value.push_back(rc);
    }
    return 0;
};

int parse_json_row(const std::string* dataset, const size_t from, const size_t to, std::string& key, std::string& value) {
    if (from >= to) return -1;
    
    // find colon separator
    auto colon_id = dataset->find(':', from);
    if (colon_id > to) return -1;
    
    // getting the key
    if (parse_json_row_key(dataset, key, from, colon_id) == -1) return -1;
    
    // getting the value
    if (parse_json_row_value(dataset, value, colon_id, to) == -1) return -1;
    return 0;
}

int dataset_parsing(const IndexTask& task) {
    size_t from = task.start_pos;
    Data data_item;
    while(true) {
        auto nl_pos = task.dataset->find('\n', from);
        if (nl_pos > task.end_pos) break;

        std::string key;
        std::string value;
        if (parse_json_row(task.dataset, from, nl_pos, key, value) == -1) {
           break;
        }

        from = nl_pos + 1;
        if (key == "title") data_item.title = std::move(value);
        else if (key == "author") data_item.author = std::move(value);
        else if (key == "year") data_item.year = std::move(value);
        else if (key == "page_count") data_item.page_count = std::move(value);
        else if (key == "full_text") data_item.text = std::move(value);
    }
    
    return books.push_back(std::move(data_item)) - 1;
}

std::string word_extractor(const std::string& str, std::string::size_type& pos) {
    // skip initial delimiters
    while (pos < str.size() && !std::isalpha(str[pos]) && !std::isdigit(str[pos])) pos++;\

    // collect word characters
    std::string word;
    while (pos < str.size() && (std::isalpha(str[pos]) || std::isdigit(str[pos]))) {
	    word += str[pos];
	    pos++;
    }
    return word;
};

void string_indexer(const std::size_t doc_id, const std::string& str, const StopWords& stop_words, IndexMap& index) {
    std::set<std::string> word_list;
    std::string word;
    std::string::size_type word_pos = 0;
    while(true) {
        word = std::move(word_extractor(str, word_pos));
        if (word_pos >= str.size()) break;
        if (word.length() < 3) continue;

        // to lower case
        std::transform(word.begin(), word.end(), word.begin(), [](unsigned char c){ return std::tolower(c); });

        // skip stop words
        if (stop_words.contains(word)) continue;
        word_list.insert(word);
    }
    
	index.push_back(word_list, doc_id);
}

void dataset_indexer(const size_t pos) {
    Data book = books[pos];
    std::string tag = book.author + " " + book.year + " " + book.title;
    string_indexer(pos, tag, stop_words, tag_index);
    string_indexer(pos, book.text, stop_words, text_index);
}

int index_thread_worker(const IndexTask& task) {
    size_t pos = dataset_parsing(task);
    dataset_indexer(pos);
    return 0;
}

using IndexThreadPool = ThreadPool<IndexTask>;

int index_task_producer(const std::string& file_data, IndexThreadPool& thread_pool, DataArray& data) {
    size_t start_pos = 0;
    int cnt = 0;

    while (true) {
        start_pos = file_data.find('{', start_pos);
        if (start_pos == std::string::npos) {
            return cnt;
        }

        start_pos += 2;

        size_t end_pos = file_data.find("},\n", start_pos);
        if (end_pos == std::string::npos) {
            end_pos = file_data.find("}\n", start_pos);
            if (end_pos == std::string::npos) {
                return cnt;
            }
        }
        end_pos--;

        IndexTask task(start_pos, end_pos, &file_data);
        thread_pool.enqueue(task);
        cnt++;
        start_pos = end_pos + 1;
    }
}

void index_request(int socket, const std::map<std::string, std::string>& header_map, std::string& body) {
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

    size_t b_pos = header_map.at(CONTENT_TYPE).find(MULTYPART_BOUNDARY_PREFIX);
    if (b_pos == std::string::npos) {
        send_http_response(socket, 400, "Bad Request", "Missing multipart boundary.\n");
        close(socket);
        return;
    }

    std::string boundary = "--" + header_map.at(CONTENT_TYPE).substr(b_pos + std::strlen(MULTYPART_BOUNDARY_PREFIX));

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
        std::string filename = extract_param(b_header_map[CONTENT_DISPOSITION], "filename", error);
        if (error < 0) {
            send_http_response(socket, 400, "Bad Request", "Incorrect value for filename parameter.\n");
            close(socket);
            return;
        }

        std::string name = extract_param(b_header_map[CONTENT_DISPOSITION], "name", error);
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

    //
    IndexThreadPool t_pool;

    int threads = std::stoull(param_value);
    t_pool.initialize(threads, index_thread_worker);

    auto start = std::chrono::steady_clock::now();
    books.clear();
    int tasks_cnt = index_task_producer(file_data, t_pool, books);

    while (t_pool.tasks_complited < tasks_cnt) {
        std::this_thread::sleep_for(std::chrono::milliseconds(100));
    }

    auto end = std::chrono::steady_clock::now();
    auto dur = std::chrono::duration_cast<std::chrono::milliseconds>(end - start).count();

    send_http_response(socket, 200, "OK", "");
    close(socket);

    text_index.print_stats(); //

    index_stat.documents = books.size();
    index_stat.index_build_time_ms = dur;
    index_stat.threads_used = threads;

    std::cout << dur << std::endl;
}