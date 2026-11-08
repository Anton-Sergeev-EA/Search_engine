#include "database.hpp"

#include <algorithm>
#include <iostream>
#include <utility>
#include <boost/asio.hpp>
#include <boost/beast.hpp>

namespace {

namespace net = boost::asio;
namespace beast = boost::beast;
namespace http = beast::http;
using net::ip::tcp;

constexpr std::size_t max_query_bytes = 4096;

std::string execute_query(const search::DatabaseConfig& config, std::string_view query) {
    if (query.size() > max_query_bytes) {
        throw std::invalid_argument("Query is too long");
    }
    auto terms = search::tokenize(query);
    if (terms.empty() || terms.size() > 4) {
        throw std::invalid_argument("Enter one to four search words");
    }
    std::sort(terms.begin(), terms.end());
    terms.erase(std::unique(terms.begin(), terms.end()), terms.end());
    search::Database database(config);
    const auto hits = database.find_matches(terms);
    if (hits.empty()) {
        return "No results\n";
    }
    std::string output;
    for (const auto& hit : hits) {
        output += hit.score + '\t';
        for (const unsigned char byte : hit.path) {
            output += byte < 32 || byte == 127 ? '?' : static_cast<char>(byte);
        }
        output += '\n';
    }
    return output;
}

void console(const search::DatabaseConfig& config) {
    std::string query;
    while (std::cout << "Search> " << std::flush, std::getline(std::cin, query)) {
        try {
            std::cout << execute_query(config, query);
        } catch (const std::invalid_argument& error) {
            std::cout << error.what() << '\n';
        } catch (const std::exception&) {
            std::cout << "Internal search error\n";
        }
    }
}

net::awaitable<void> serve(tcp::acceptor& acceptor, const search::DatabaseConfig& config) {
    for (;;) {
        beast::tcp_stream stream(co_await acceptor.async_accept(net::use_awaitable));
        beast::flat_buffer buffer;
        http::request_parser<http::string_body> parser;
        parser.header_limit(8192);
        parser.body_limit(max_query_bytes);
        boost::system::error_code error;
        stream.expires_after(std::chrono::seconds(5));
        co_await http::async_read(stream, buffer, parser,
                                 net::redirect_error(net::use_awaitable, error));
        if (error && error != http::error::body_limit) {
            continue;
        }
        const auto& request = parser.get();
        http::response<http::string_body> response{http::status::ok, request.version()};
        response.set(http::field::content_type, "text/plain; charset=utf-8");
        response.set(http::field::cache_control, "no-store");
        response.set("X-Content-Type-Options", "nosniff");
        response.keep_alive(false);
        if (error) {
            response.result(http::status::payload_too_large);
            response.body() = "Query is too long\n";
        } else if (request.target() != "/search") {
            response.result(http::status::not_found);
            response.body() = "Not found\n";
        } else if (request.method() != http::verb::post) {
            response.result(http::status::method_not_allowed);
            response.set(http::field::allow, "POST");
            response.body() = "Use POST\n";
        } else {
            try {
                response.body() = execute_query(config, request.body());
            } catch (const std::invalid_argument& exception) {
                response.result(http::status::bad_request);
                response.body() = std::string(exception.what()) + '\n';
            } catch (const std::exception&) {
                response.result(http::status::internal_server_error);
                response.body() = "Internal search error\n";
            }
        }
        response.prepare_payload();
        stream.expires_after(std::chrono::seconds(5));
        co_await http::async_write(stream, response,
                                  net::redirect_error(net::use_awaitable, error));
    }
}

}  // namespace

int main(int argc, char* argv[]) {
    if (argc < 2 || argc > 3 || (argc == 3 && std::string_view(argv[2]) != "--http")) {
        std::cerr << "Usage: search_query <config.ini> [--http]\n";
        return 2;
    }
    try {
        const auto config = search::load_config(argv[1]);
        if (argc == 2) {
            console(config.database);
            return 0;
        }
        net::io_context context(1);
        tcp::acceptor acceptor(context, {net::ip::address_v4::loopback(), config.search_port});
        net::co_spawn(context, serve(acceptor, config.database), [](std::exception_ptr error) {
            if (error) {
                std::rethrow_exception(error);
            }
        });
        std::cout << "Listening on 127.0.0.1:" << config.search_port << std::endl;
        context.run();
    } catch (const std::exception&) {
        std::cerr << "Internal search error\n";
        return 1;
    }
}
