#include "documents.hpp"

#include <chrono>
#include <fstream>
#include <iostream>
#include <stdexcept>

namespace {

void require(bool condition) {
    if (!condition) {
        throw std::runtime_error("Configuration or document discovery check failed");
    }
}

void write_config(const std::filesystem::path& path, const std::string& tail,
                  const std::string& port = "5432",
                  const std::string& extensions = "TXT, .md,txt") {
    std::ofstream output(path);
    output << "[database]\nhost=localhost\nport=" << port
           << "\nname=test\nuser=test\npassword=\n[indexer]\n"
              "directory=documents\nextensions="
           << extensions << "\n" << tail;
    if (!output) {
        throw std::runtime_error("Cannot write test fixture");
    }
}

bool rejected(const std::filesystem::path& path) {
    try {
        static_cast<void>(search::load_config(path));
        return false;
    } catch (const std::runtime_error&) {
        return true;
    }
}

}  // namespace

int main() {
    try {
        const auto root = std::filesystem::current_path() /
                          ("config-fixture-" + std::to_string(
                              std::chrono::steady_clock::now().time_since_epoch().count()));
        std::filesystem::create_directories(root / "documents" / "nested");
        const auto path = root / "settings.ini";
        write_config(path, "");
        const auto config = search::load_config(path);
        require(config.database.port == 5432 && config.database.password.empty());
        require(config.extensions == std::vector<std::string>{"txt", "md"});
        require(config.directory == root / "documents");
        require(search::find_documents(config).empty());
        std::ofstream(root / "documents" / "a.TXT") << "hello";
        std::ofstream(root / "documents" / "nested" / "b.md") << "world";
        std::ofstream(root / "documents" / "ignored.csv") << "ignored";
        std::filesystem::create_directory(root / "documents" / "folder.txt");
        std::filesystem::create_symlink(root / "documents" / "a.TXT",
                                        root / "documents" / "link.txt");
        std::filesystem::create_directory_symlink(root / "documents",
                                                  root / "documents" / "nested" / "loop");
        require(search::find_documents(config) ==
                std::vector<std::filesystem::path>{root / "documents" / "a.TXT",
                                                  root / "documents" / "nested" / "b.md"});
        for (const auto& port : {"0", "65536", "-1", "5432x", "999999999999999"}) {
            write_config(path, "", port);
            require(rejected(path));
        }
        for (const auto& tail : {"extensions=txt\n", "[unknown]\n", "broken\n",
                                 "password=hidden\n"}) {
            write_config(path, tail);
            require(rejected(path));
        }
        for (const auto& extensions : {"txt,", ",txt", "txt,,md", "../txt", "*", "."}) {
            write_config(path, "", "5432", extensions);
            require(rejected(path));
        }
        write_config(path, "", "65535");
        require(search::load_config(path).database.port == 65535);
        require(rejected(root / "missing.ini"));
        {
            std::ofstream output(path);
            output << "[database]\nhost=localhost\n";
        }
        require(rejected(path));
        auto missing = config;
        missing.directory = root / "missing";
        bool failed = false;
        try {
            static_cast<void>(search::find_documents(missing));
        } catch (const std::exception&) {
            failed = true;
        }
        require(failed);
    } catch (const std::exception& error) {
        std::cerr << error.what() << '\n';
        return 1;
    }
}
