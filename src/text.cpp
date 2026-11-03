#include "text.hpp"

#include <limits>
#include <stdexcept>
#include <unicode/locid.h>
#include <unicode/uchar.h>
#include <unicode/unistr.h>
#include <unicode/utf8.h>

namespace search {

std::vector<std::string> tokenize(std::string_view text) {
    if (text.size() > std::numeric_limits<std::int32_t>::max()) {
        throw std::length_error("UTF-8 input exceeds ICU size limit");
    }

    std::vector<std::string> words;
    icu::UnicodeString word;
    int length = 0;
    const auto flush = [&] {
        if (length >= 3 && length <= 32) {
            word.toLower(icu::Locale::getRoot());
            word.toUTF8String(words.emplace_back());
        }
        word.remove();
        length = 0;
    };

    const auto* bytes = reinterpret_cast<const std::uint8_t*>(text.data());
    const auto size = static_cast<std::int32_t>(text.size());
    for (std::int32_t offset = 0; offset < size;) {
        UChar32 character;
        U8_NEXT(bytes, offset, size, character);
        if (character < 0 || !u_isalpha(character)) {
            flush();
        } else if (length <= 32) {
            ++length;
            if (length <= 32) {
                word.append(character);
            }
        }
    }
    flush();
    return words;
}

WordCounts count_words(std::string_view text) {
    WordCounts counts;
    for (const auto& word : tokenize(text)) {
        ++counts[word];
    }
    return counts;
}

}  // namespace search
