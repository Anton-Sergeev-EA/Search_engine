#include "database.hpp"
#include "documents.hpp"

#include <fstream>
#include <iostream>
#include <iterator>
#include <stdexcept>

int main(int argc, char* argv[]) {
    if (argc != 2) {
        std::cerr << "Usage: search_indexer <config.ini>\n";
        return 2;
    }
    try {
        const auto config = search::load_config(argv[1]);
        std::vector<search::IndexedDocument> documents;
        for (const auto& path : search::find_documents(config)) {
            std::ifstream input(path, std::ios::binary);
            if (!input) {
                throw std::runtime_error("Cannot open document");
            }
            const std::string text{std::istreambuf_iterator<char>(input), {}};
            if (input.bad()) {
                throw std::runtime_error("Cannot read document");
            }
            documents.push_back({path, search::count_words(text)});
        }
        search::Database database(config.database);
        database.initialize();
        database.replace_index(config.directory, documents);
        std::cout << "Indexed documents: " << documents.size() << '\n';
    } catch (const std::exception&) {
        std::cerr << "Internal error: indexing failed\n";
        return 1;
    }
}
