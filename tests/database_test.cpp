#include "database.hpp"

#include <fstream>
#include <iostream>
#include <limits>
#include <stdexcept>

namespace {

void require(bool condition) {
    if (!condition) {
        throw std::runtime_error("Database check failed");
    }
}

}  // namespace

int main(int argc, char* argv[]) {
    if (argc != 4) {
        return 2;
    }
    try {
        const auto root = std::filesystem::canonical(argv[3]);
        const search::DatabaseConfig config{argv[1], 5432, "postgres", argv[2], ""};
        search::Database database(config);
        database.initialize();
        database.initialize();
        const auto file = root / "quote' документ.txt";
        std::ofstream(file) << "test";
        database.replace_index(root, {{file, {{"привет", 3}, {"quote'word", 1}}}});
        pqxx::connection connection("host=" + std::string(argv[1]) +
                                    " port=5432 dbname=postgres user=" + argv[2] +
                                    " passfile=/dev/null");
        const auto total = [&connection] {
            pqxx::read_transaction transaction(connection);
            return transaction.exec("SELECT COALESCE(SUM(frequency), 0) FROM document_words")
                [0][0].as<long long>();
        };
        require(total() == 4);
        database.replace_index(root, {{file, {{"мир", 2}}}});
        require(total() == 2);
        for (const auto frequency : {std::uint64_t{0},
                                    std::numeric_limits<std::uint64_t>::max()}) {
            bool failed = false;
            try {
                database.replace_index(root, {{file, {{"привет", frequency}}}});
            } catch (const std::invalid_argument&) {
                failed = true;
            }
            require(failed && total() == 2);
        }
        bool failed = false;
        try {
            database.replace_index(root, {{file, {{"first", 1}}}, {file, {{"second", 2}}}});
        } catch (const pqxx::unique_violation&) {
            failed = true;
        }
        require(failed && total() == 2);

        const auto other = root / "other";
        std::filesystem::create_directory(other);
        const auto other_file = other / "other.txt";
        std::ofstream(other_file) << "other";
        database.replace_index(other, {{other_file, {{"other", 5}}}});
        database.replace_index(root, {});
        require(total() == 5);
        database.replace_index(other, {});
        require(total() == 0);
        pqxx::read_transaction transaction(connection);
        require(transaction.exec("SELECT COUNT(*) FROM words")[0][0].as<int>() == 0);
    } catch (const std::exception&) {
        std::cerr << "Database integration test failed\n";
        return 1;
    }
}
