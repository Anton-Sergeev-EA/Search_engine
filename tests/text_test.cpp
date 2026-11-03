#include "text.hpp"

#include <iostream>

int main() {
    const search::WordCounts expected{{"привет", 2}, {"мир", 2}, {"hello", 1}};
    if (search::count_words("Привет, МИР!\nПРИВЕТ\tмир—HELLO") != expected) {
        std::cerr << "Unicode tokenization or counting failed\n";
        return 1;
    }

    const std::string longest(32, 'a');
    if (search::tokenize("а да три " + longest + " " + longest + "a") !=
        std::vector<std::string>{"три", longest}) {
        std::cerr << "Word length boundaries failed\n";
        return 1;
    }

    if (search::tokenize("hello\xff" "world") !=
            std::vector<std::string>{"hello", "world"} ||
        !search::tokenize("").empty()) {
        std::cerr << "Invalid UTF-8 or empty input handling failed\n";
        return 1;
    }
}
