#include "benchmark.hpp"
#include "imdb_loader.hpp"
#include "test_support.hpp"
#include "trie.hpp"
#include "tst.hpp"

#include <array>
#include <iostream>
#include <random>
#include <set>

using namespace test_support;

namespace {
// Realistic enough to contain common words, shared prefixes, and duplicates.
std::vector<Movie> generatedMovies(std::size_t count) {
    static const std::array<const char*, 14> words = {
        "the", "star", "love", "night", "dark", "city", "of", "and", "last",
        "man", "war", "story", "2", "a"};
    std::mt19937 rng(42);
    std::vector<Movie> movies;
    movies.reserve(count);
    for (std::size_t i = 0; i < count; ++i) {
        std::string title;
        const auto length = 1 + rng() % 4;
        for (std::size_t j = 0; j < length; ++j) {
            title += words[rng() % words.size()];
            title += ' ';
        }
        title += std::to_string(rng() % 500);
        movies.push_back(makeMovie("tt" + std::to_string(i), title, 1900 + static_cast<int>(rng() % 120)));
    }
    return movies;
}

void testWorkloadShape() {
    const auto movies = generatedMovies(5000);
    const auto queries = generateQueryWorkload(movies);
    check(queries.size() >= kMinimumBenchmarkQueries, "at least 100 queries");

    std::set<std::string> unique;
    std::array<std::size_t, 6> per_category{};
    for (const auto& query : queries) {
        unique.insert(query.prefix);
        ++per_category[static_cast<std::size_t>(query.category)];
        check(query.prefix == normalizeTitle(query.prefix), "queries are normalized: " + query.prefix);
        const auto matches = expectedMatches(movies, query.prefix, movies.size()).size();
        switch (query.category) {
        case QueryCategory::Short:
            check(query.prefix.size() <= 2 && matches > 0, "short prefix: " + query.prefix);
            break;
        case QueryCategory::Medium:
            check(query.prefix.size() >= 3 && query.prefix.size() <= 5 && matches > 0,
                  "medium prefix: " + query.prefix);
            break;
        case QueryCategory::Long:
            check(query.prefix.size() >= 6 && query.prefix.size() <= 10 && matches > 0,
                  "long prefix: " + query.prefix);
            break;
        case QueryCategory::Common:
            check(matches > 10, "common prefixes exceed the display limit: " + query.prefix);
            break;
        case QueryCategory::Rare:
            check(matches >= 1 && matches <= 3, "rare prefix: " + query.prefix);
            break;
        case QueryCategory::NoMatch:
            check(matches == 0, "no-match prefix: " + query.prefix);
            break;
        }
    }
    check(unique.size() == queries.size(), "queries are unique");
    for (std::size_t count : per_category) {
        check(count > 0, "every category is represented");
    }
}

void testWorkloadIsDeterministic() {
    const auto movies = generatedMovies(2000);
    const auto first = generateQueryWorkload(movies, 3530);
    const auto second = generateQueryWorkload(movies, 3530);
    const auto other_seed = generateQueryWorkload(movies, 3531);
    const auto prefixes = [](const std::vector<BenchmarkQuery>& queries) {
        std::vector<std::string> result;
        for (const auto& query : queries) {
            result.push_back(query.prefix);
        }
        return result;
    };
    check(prefixes(first) == prefixes(second), "same seed, same workload");
    check(prefixes(first) != prefixes(other_seed), "different seed, different workload");
}

void testWorkloadOnSmallDatasets() {
    check(generateQueryWorkload({}).empty(), "empty dataset has no queries");
    const auto fixture = loadImdbMovies(std::string(TEST_DATA_DIR) + "/sample_movies.tsv").movies;
    const auto queries = generateQueryWorkload(fixture);
    check(!queries.empty() && queries.size() < kMinimumBenchmarkQueries,
          "a tiny dataset yields a shorter workload rather than padding");
}

// 20. The same workload must give matching results across both structures.
void testWorkloadMatchesAcrossStructures() {
    const auto movies = generatedMovies(5000);
    Trie trie;
    TernarySearchTree tst;
    trie.build(movies);
    tst.build(movies);
    for (const auto& query : generateQueryWorkload(movies)) {
        for (std::size_t limit : {std::size_t{10}, movies.size()}) {
            const auto expected = expectedMatches(movies, query.prefix, limit);
            check(trie.search(query.prefix, limit).movie_ids == expected, "Trie: " + query.prefix);
            check(tst.search(query.prefix, limit).movie_ids == expected, "TST: " + query.prefix);
        }
    }
}
} // namespace

int main() {
    try {
        testWorkloadShape();
        testWorkloadIsDeterministic();
        testWorkloadOnSmallDatasets();
        testWorkloadMatchesAcrossStructures();
        std::cout << "Passed " << checks << " checks\n";
        return 0;
    } catch (const std::exception& error) {
        std::cerr << "Test failure: " << error.what() << '\n';
        return 1;
    }
}
