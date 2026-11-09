#pragma once

#include <cstdint>
#include <filesystem>
#include <string>
#include <vector>

namespace search {

struct DatabaseConfig {
    std::string host;
    std::uint16_t port;
    std::string name;
    std::string user;
    std::string password;
};

struct Config {
    DatabaseConfig database;
    std::filesystem::path directory;
    std::vector<std::string> extensions;
    std::uint16_t search_port = 8080;
    std::string search_address = "127.0.0.1";
};

Config load_config(const std::filesystem::path& path);

}  // namespace search
