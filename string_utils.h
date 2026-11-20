#pragma once

#include <string>
#include <algorithm>
#include <cctype>

void remove_whitespace(std::string& str) {
    str.erase(std::remove_if(str.begin(), str.end(), ::isspace), str.end());
}

std::pair<std::string, std::string> parse_key_value_string(std::string& str, const char delimiter = ':') {
    remove_whitespace(str);
    size_t pos = str.find(delimiter);

    if (pos != std::string::npos) {
        std::string key = str.substr(0, pos);
        std::string value = str.substr(pos + 1);
        return std::make_pair(key, value);
    }
    return std::make_pair(std::string(), std::string());
}

std::string word_extractor(const std::string& str, std::string::size_type& pos) {
    // skip initial delimiters
    while (pos < str.size() && !std::isalpha(str[pos]) && !std::isdigit(str[pos])) pos++;

    // collect word characters
    std::string word;
    while (pos < str.size() && (std::isalpha(str[pos]) || std::isdigit(str[pos]))) {
	    word += str[pos];
	    pos++;
    }
    return word;
}