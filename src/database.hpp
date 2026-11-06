#pragma once

#include "config.hpp"
#include "text.hpp"

#include <pqxx/pqxx>

namespace search {

struct IndexedDocument {
    std::filesystem::path path;
    WordCounts words;
};

class Database {
public:
    explicit Database(const DatabaseConfig& config);
    void initialize();
    void replace_index(const std::filesystem::path& root,
                       const std::vector<IndexedDocument>& documents);

private:
    pqxx::connection connection_;
};

}  // namespace search
