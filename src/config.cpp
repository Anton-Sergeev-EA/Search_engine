#include "config.hpp"

#include <algorithm>
#include <charconv>
#include <fstream>
#include <map>
#include <stdexcept>
#include <string_view>

namespace search {
namespace {

std::string trim(std::string_view text) {
    const auto first = text.find_first_not_of(" \t\r");
    if (first == std::string_view::npos) {
        return {};
    }
    return std::string(text.substr(first, text.find_last_not_of(" \t\r") - first + 1));
}

std::uint16_t parse_port(const std::string& text, const char* setting) {
    unsigned int port = 0;
    const auto end = text.data() + text.size();
    const auto result = std::from_chars(text.data(), end, port);
    if (result.ec != std::errc{} || result.ptr != end || port == 0 || port > 65535) {
        throw std::runtime_error(std::string(setting) + " must be between 1 and 65535");
    }
    return static_cast<std::uint16_t>(port);
}

}  // namespace

Config load_config(const std::filesystem::path& path) {
    std::ifstream input(path);
    if (!input) {
        throw std::runtime_error("Cannot open configuration");
    }
    std::map<std::string, std::string> values;
    std::string section;
    std::string line;
    std::size_t number = 0;
    const std::vector<std::string> allowed{
        "database.host", "database.port", "database.name", "database.user",
        "database.password", "indexer.directory", "indexer.extensions"};
    while (std::getline(input, line)) {
        ++number;
        line = trim(line);
        if (line.empty() || line.front() == ';' || line.front() == '#') {
            continue;
        }
        if (line.front() == '[' && line.back() == ']') {
            section = trim(std::string_view(line).substr(1, line.size() - 2));
            if (section != "database" && section != "indexer" && section != "search") {
                throw std::runtime_error("Unknown configuration section at line " +
                                         std::to_string(number));
            }
            continue;
        }
        const auto equal = line.find('=');
        if (equal == std::string::npos) {
            throw std::runtime_error("Invalid configuration at line " + std::to_string(number));
        }
        const auto key = section + "." + trim(std::string_view(line).substr(0, equal));
        if ((std::find(allowed.begin(), allowed.end(), key) == allowed.end() && key != "search.port") ||
            values.contains(key)) {
            throw std::runtime_error("Unknown or duplicate setting at line " +
                                     std::to_string(number));
        }
        values.emplace(key, trim(std::string_view(line).substr(equal + 1)));
    }
    if (input.bad()) {
        throw std::runtime_error("Cannot read configuration");
    }
    for (const auto& key : allowed) {
        if (!values.contains(key) || (values.at(key).empty() && key != "database.password")) {
            throw std::runtime_error("Missing configuration setting " + key);
        }
    }
    Config config{{values.at("database.host"), parse_port(values.at("database.port"), "Database port"),
                   values.at("database.name"), values.at("database.user"),
                   values.at("database.password")},
                  values.at("indexer.directory"), {}};
    if (values.contains("search.port")) {
        config.search_port = parse_port(values.at("search.port"), "Search port");
    }
    if (config.directory.is_relative()) {
        config.directory = std::filesystem::absolute(path).parent_path() / config.directory;
    }
    const auto& extensions = values.at("indexer.extensions");
    std::size_t start = 0;
    do {
        const auto comma = extensions.find(',', start);
        auto extension = trim(std::string_view(extensions).substr(
            start, comma == std::string::npos ? comma : comma - start));
        if (!extension.empty() && extension.front() == '.') {
            extension.erase(0, 1);
        }
        if (extension.empty() ||
            extension.find_first_not_of("abcdefghijklmnopqrstuvwxyzABCDEFGHIJKLMNOPQRSTUVWXYZ0123456789") !=
                std::string::npos) {
            throw std::runtime_error("Invalid file extension");
        }
        for (char& character : extension) {
            if (character >= 'A' && character <= 'Z') {
                character = static_cast<char>(character - 'A' + 'a');
            }
        }
        if (std::find(config.extensions.begin(), config.extensions.end(), extension) ==
            config.extensions.end()) {
            config.extensions.push_back(extension);
        }
        if (comma == std::string::npos) {
            break;
        }
        start = comma + 1;
    } while (true);
    return config;
}

}  // namespace search
