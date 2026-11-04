#include "normalize.hpp"

std::string normalizeTitle(const std::string& input) {
    std::string output;
    output.reserve(input.size());
    bool pending_space = false;
    for (unsigned char ch : input) {
        if (ch >= 'A' && ch <= 'Z') {
            ch = static_cast<unsigned char>(ch + ('a' - 'A'));
        }
        if ((ch >= 'a' && ch <= 'z') || (ch >= '0' && ch <= '9')) {
            if (pending_space && !output.empty()) {
                output.push_back(' ');
            }
            output.push_back(static_cast<char>(ch));
            pending_space = false;
        } else {
            pending_space = true;
        }
    }
    return output;
}
