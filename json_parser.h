# pragma once
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
    value = "";
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

std::string to_json_string(const std::string& text) {
    std::string json_string;

    for (auto c: text) {
        switch(c) {
            case '"'  : json_string.append("\\\""); break;
            case '\\' : json_string.append("\\\\"); break;
            case '\n' : json_string.append("\\n"); break;
            case '\r' : json_string.append("\\r"); break;
            case '\t' : json_string.append("\\t"); break;
            case '\b' : json_string.append("\\b"); break;
            case '\f' : json_string.append("\\f"); break;
            default: json_string.append(1, c); 
        }
    }

    return json_string;
}