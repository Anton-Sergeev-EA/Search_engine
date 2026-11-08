#include "database.hpp"

#include <limits>
#include <stdexcept>

namespace search {
namespace {

std::string quote_option(const std::string& value) {
    if (value.find('\0') != std::string::npos) {
        throw std::invalid_argument("Invalid database setting");
    }
    std::string quoted = "'";
    for (const char character : value) {
        if (character == '\\' || character == '\'') {
            quoted += '\\';
        }
        quoted += character;
    }
    return quoted + "'";
}

std::string connection_options(const DatabaseConfig& config) {
    return "host=" + quote_option(config.host) + " port=" + std::to_string(config.port) +
           " dbname=" + quote_option(config.name) + " user=" + quote_option(config.user) +
           " password=" + quote_option(config.password) +
           " connect_timeout=5 client_encoding=UTF8 passfile=/dev/null";
}

}  // namespace

Database::Database(const DatabaseConfig& config) : connection_(connection_options(config)) {}

std::vector<SearchHit> Database::find_matches(const std::vector<std::string>& terms) {
    if (terms.empty() || terms.size() > 4) {
        throw std::invalid_argument("Expected one to four search terms");
    }
    auto parameters = terms;
    parameters.resize(4);
    pqxx::read_transaction transaction(connection_);
    transaction.exec("SET LOCAL statement_timeout = '5s'");
    const auto rows = transaction.exec_params(R"(
        WITH matches AS (
            SELECT d.path, SUM(dw.frequency) AS score
            FROM documents d
            JOIN document_words dw ON dw.document_id = d.id
            JOIN words w ON w.id = dw.word_id
            WHERE w.word IN ($1, $2, $3, $4) AND w.word <> ''
            GROUP BY d.id
            HAVING COUNT(*) = $5
        )
        SELECT path, MAX(score) AS score
        FROM matches
        GROUP BY path
        ORDER BY score DESC, path COLLATE "C"
        LIMIT 10
    )", parameters[0], parameters[1], parameters[2], parameters[3], terms.size());
    std::vector<SearchHit> hits;
    for (const auto& row : rows) {
        hits.push_back({row[0].as<std::string>(), row[1].as<std::string>()});
    }
    return hits;
}

void Database::initialize() {
    pqxx::work transaction(connection_);
    transaction.exec_params("SELECT pg_advisory_xact_lock($1)", 830164925);
    transaction.exec(R"(
        CREATE TABLE IF NOT EXISTS documents (
            id BIGINT GENERATED ALWAYS AS IDENTITY PRIMARY KEY,
            root TEXT NOT NULL,
            path TEXT NOT NULL,
            UNIQUE (root, path)
        );
        CREATE TABLE IF NOT EXISTS words (
            id BIGINT GENERATED ALWAYS AS IDENTITY PRIMARY KEY,
            word TEXT NOT NULL UNIQUE
        );
        CREATE TABLE IF NOT EXISTS document_words (
            document_id BIGINT NOT NULL REFERENCES documents(id) ON DELETE CASCADE,
            word_id BIGINT NOT NULL REFERENCES words(id),
            frequency BIGINT NOT NULL CHECK (frequency > 0),
            PRIMARY KEY (document_id, word_id)
        );
        CREATE INDEX IF NOT EXISTS document_words_word_id ON document_words(word_id)
    )");
    transaction.commit();
}

void Database::replace_index(const std::filesystem::path& root,
                             const std::vector<IndexedDocument>& documents) {
    pqxx::work transaction(connection_);
    transaction.exec_params("SELECT pg_advisory_xact_lock($1)", 830164925);
    const auto root_name = std::filesystem::canonical(root).string();
    transaction.exec_params("DELETE FROM documents WHERE root = $1", root_name);
    for (const auto& document : documents) {
        const auto path = std::filesystem::canonical(document.path);
        const auto relative = path.lexically_relative(root_name);
        if (relative.empty() || relative.is_absolute() || *relative.begin() == "..") {
            throw std::invalid_argument("Document is outside index directory");
        }
        const auto id = transaction.exec_params(
            "INSERT INTO documents(root, path) VALUES ($1, $2) RETURNING id",
            root_name, path.string())[0][0].as<long long>();
        for (const auto& [word, frequency] : document.words) {
            if (frequency == 0 ||
                frequency > static_cast<std::uint64_t>(std::numeric_limits<long long>::max())) {
                throw std::invalid_argument("Invalid word frequency");
            }
            const auto word_id = transaction.exec_params(
                "INSERT INTO words(word) VALUES ($1) "
                "ON CONFLICT (word) DO UPDATE SET word = EXCLUDED.word RETURNING id",
                word)[0][0].as<long long>();
            transaction.exec_params(
                "INSERT INTO document_words(document_id, word_id, frequency) VALUES ($1, $2, $3)",
                id, word_id, frequency);
        }
    }
    transaction.exec(
        "DELETE FROM words WHERE NOT EXISTS "
        "(SELECT 1 FROM document_words WHERE word_id = words.id)");
    transaction.commit();
}

}  // namespace search
