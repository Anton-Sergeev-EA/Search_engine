#include "documents.hpp"

#include <algorithm>
#include <stdexcept>

namespace search {

std::vector<std::filesystem::path> find_documents(const Config& config) {
    if (std::filesystem::is_symlink(config.directory) ||
        !std::filesystem::is_directory(config.directory)) {
        throw std::runtime_error("Document directory must be a directory without a symbolic link");
    }
    std::vector<std::filesystem::path> paths;
    for (const auto& entry : std::filesystem::recursive_directory_iterator(config.directory)) {
        if (entry.is_symlink() || !entry.is_regular_file()) {
            continue;
        }
        auto extension = entry.path().extension().string();
        if (extension.empty()) {
            continue;
        }
        extension.erase(0, 1);
        for (char& character : extension) {
            if (character >= 'A' && character <= 'Z') {
                character = static_cast<char>(character - 'A' + 'a');
            }
        }
        if (std::find(config.extensions.begin(), config.extensions.end(), extension) !=
            config.extensions.end()) {
            paths.push_back(std::filesystem::absolute(entry.path()).lexically_normal());
        }
    }
    std::sort(paths.begin(), paths.end());
    return paths;
}

}  // namespace search
