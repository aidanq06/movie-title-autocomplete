#include "movie.hpp"
#include "normalize.hpp"
#include "prefix_index.hpp"

#include <algorithm>
#include <iostream>
#include <stdexcept>

namespace {
std::size_t checks = 0;

void check(bool condition, const std::string& message) {
    ++checks;
    if (!condition) {
        throw std::runtime_error(message);
    }
}

void testNormalization() {
    check(normalizeTitle("THE DARK KNIGHT") == "the dark knight", "ASCII case");
    check(normalizeTitle("Spider-Man: Homecoming") == "spider man homecoming", "punctuation");
    check(normalizeTitle(" Toy Story! ") == "toy story", "trim spaces");
    check(normalizeTitle("A\t\tB...  123") == "a b 123", "collapse and digits");
    check(normalizeTitle("").empty(), "empty title");
    check(normalizeTitle(" \t!\r\n").empty(), "unsupported title");
    check(normalizeTitle("caf\xC3\xA9 noir") == "caf noir", "UTF-8 bytes become separators");
    check(normalizeTitle(std::string("A\0B", 3)) == "a b", "embedded null byte");
    const std::string long_title(100000, 'A');
    check(normalizeTitle(long_title) == std::string(100000, 'a'), "long normalization");
}

void testOrdering() {
    std::vector<Movie> movies = {
        {"tt4", "star", "movie", 2000, "star"},
        {"tt3", "Star!", "movie", 2000, "star"},
        {"tt2", "Star!", "movie", -1, "star"},
        {"tt1", "Star!", "movie", -1, "star"},
        {"tt0", "Alpha", "movie", 2020, "alpha"}
    };
    std::vector<MovieId> ids = {0, 1, 2, 3, 4};
    std::stable_sort(ids.begin(), ids.end(), MovieIdLess{movies});
    check(ids == std::vector<MovieId>({4, 3, 2, 1, 0}), "canonical title/year/id ordering");
    check(!movieLess(movies[0], movies[0]), "comparator strictness");
}
} // namespace

int main() {
    try {
        testNormalization();
        testOrdering();
        std::cout << "Passed " << checks << " checks\n";
        return 0;
    } catch (const std::exception& error) {
        std::cerr << "Test failure: " << error.what() << '\n';
        return 1;
    }
}
