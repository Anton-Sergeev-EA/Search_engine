#include "documents.hpp"

#include <exception>
#include <iostream>

int main(int argc, char* argv[]) {
    if (argc != 2) {
        std::cerr << "Usage: search_scan <config.ini>\n";
        return 2;
    }
    try {
        const auto config = search::load_config(argv[1]);
        for (const auto& path : search::find_documents(config)) {
            std::cout << path.string() << '\n';
        }
    } catch (const std::exception&) {
        std::cerr << "Cannot load configuration or scan document directory\n";
        return 1;
    }
}
