#pragma once

#include <cstdint>
#include <string>
#include <string_view>
#include <unordered_map>
#include <vector>

namespace search {

using WordCounts = std::unordered_map<std::string, std::uint64_t>;

std::vector<std::string> tokenize(std::string_view text);
WordCounts count_words(std::string_view text);

}  // namespace search
