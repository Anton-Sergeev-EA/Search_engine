#include "database.hpp"
#include "documents.hpp"

#include <algorithm>
#include <array>
#include <atomic>
#include <exception>
#include <fstream>
#include <iostream>
#include <stdexcept>
#include <thread>

namespace {

search::IndexedDocument read_document(const std::filesystem::path& path) {
    if (!std::filesystem::is_regular_file(std::filesystem::symlink_status(path))) {
        throw std::runtime_error("Document is no longer a regular file");
    }
    std::ifstream input(path, std::ios::binary);
    if (!input) {
        throw std::runtime_error("Cannot open document");
    }
    std::array<char, 8192> buffer;
    std::string text;
    while (input.read(buffer.data(), buffer.size()) || input.gcount() > 0) {
        text.append(buffer.data(), static_cast<std::size_t>(input.gcount()));
    }
    if (input.bad() || !input.eof()) {
        throw std::runtime_error("Cannot read document");
    }
    return {path, search::count_words(text)};
}

std::vector<search::IndexedDocument> prepare_documents(
    const std::vector<std::filesystem::path>& paths) {
    const auto worker_count = std::min<std::size_t>(
        paths.size(), std::clamp(std::thread::hardware_concurrency(), 1u, 8u));
    std::vector<search::IndexedDocument> documents(paths.size());
    std::vector<std::exception_ptr> errors(worker_count);
    std::atomic_size_t next{0};
    std::atomic_bool failed{false};
    std::vector<std::jthread> workers;
    workers.reserve(worker_count);
    for (std::size_t worker = 0; worker < worker_count; ++worker) {
        workers.emplace_back([&, worker](std::stop_token stop) {
            try {
                while (!stop.stop_requested() && !failed.load()) {
                    const auto index = next.fetch_add(1);
                    if (index >= paths.size()) {
                        break;
                    }
                    documents[index] = read_document(paths[index]);
                }
            } catch (...) {
                errors[worker] = std::current_exception();
                failed.store(true);
            }
        });
    }
    for (auto& worker : workers) {
        worker.join();
    }
    for (const auto& error : errors) {
        if (error) {
            std::rethrow_exception(error);
        }
    }
    return documents;
}

}  // namespace

int main(int argc, char* argv[]) {
    if (argc != 2) {
        std::cerr << "Usage: search_indexer <config.ini>\n";
        return 2;
    }
    try {
        const auto config = search::load_config(argv[1]);
        const auto documents = prepare_documents(search::find_documents(config));
        search::Database database(config.database);
        database.initialize();
        database.replace_index(config.directory, documents);
        std::cout << "Indexed documents: " << documents.size() << '\n';
    } catch (const std::exception&) {
        std::cerr << "Internal error: indexing failed\n";
        return 1;
    }
}
