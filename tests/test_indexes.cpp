// Runs the Trie and TST side by side through the shared PrefixIndex interface and
// compares both with an independent scan. Numbers in comments match the Phase 2
// test requirements listed in docs/ANALYSIS_NOTES.md.

#include "imdb_loader.hpp"
#include "test_support.hpp"
#include "trie.hpp"
#include "tst.hpp"

#include <iostream>
#include <random>

using namespace test_support;

namespace {
struct Indexes {
    Trie trie;
    TernarySearchTree tst;

    explicit Indexes(const std::vector<Movie>& movies) { rebuild(movies); }

    void rebuild(const std::vector<Movie>& movies) {
        PrefixIndex* all[] = {&trie, &tst};
        for (PrefixIndex* index : all) {
            index->build(movies);
        }
    }
};

// The core assertion: same IDs in the same order as each other and as the scan.
std::vector<MovieId> sameResults(const Indexes& indexes, const std::vector<Movie>& movies,
                                 const std::string& prefix, std::size_t limit = 10) {
    const auto expected = expectedMatches(movies, prefix, limit);
    const auto from_trie = indexes.trie.search(prefix, limit).movie_ids;
    const auto from_tst = indexes.tst.search(prefix, limit).movie_ids;
    const std::string label = "prefix \"" + prefix.substr(0, 60) + "\" limit " + std::to_string(limit);
    check(from_trie == expected, "Trie differs from scan for " + label);
    check(from_tst == expected, "TST differs from scan for " + label);
    return expected;
}

std::vector<Movie> sampleMovies() {
    return loadImdbMovies(std::string(TEST_DATA_DIR) + "/sample_movies.tsv").movies;
}

void testSimpleAndLongPrefixes() { // 1, 2
    const auto movies = sampleMovies();
    const Indexes indexes(movies);
    check(!sameResults(indexes, movies, "star").empty(), "simple prefix finds matches");
    check(sameResults(indexes, movies, "spider man homecoming").size() == 1, "long prefix finds its title");
}

void testNonexistentPrefixes() { // 3
    const auto movies = sampleMovies();
    const Indexes indexes(movies);
    for (const std::string prefix : {"zzz", "star wars", "spider man homecoming 2", "0", "the dark knightly"}) {
        check(sameResults(indexes, movies, prefix).empty(), "no matches for " + prefix);
    }
}

void testEmptyPrefixes() { // 4
    const auto movies = sampleMovies();
    const Indexes indexes(movies);
    for (const std::string prefix : {"", " ", "!!!", "\t-\n", "\xC3\xA9"}) {
        check(sameResults(indexes, movies, prefix).empty(), "empty prefix returns nothing");
        check(indexes.trie.search(prefix).nodes_visited == 0
                  && indexes.tst.search(prefix).nodes_visited == 0, "empty prefix does no node work");
    }
}

void testCaseAndPunctuation() { // 5, 6
    const auto movies = sampleMovies();
    const Indexes indexes(movies);
    const auto reference = sameResults(indexes, movies, "star light");
    check(reference.size() == 5, "reference query");
    for (const std::string prefix : {"STAR LIGHT", "StAr LiGhT", "star-light", "  star...light  ", "star\t\tlight!"}) {
        check(sameResults(indexes, movies, prefix) == reference, "query variant: " + prefix);
    }
    check(sameResults(indexes, movies, "SPIDER-MAN:").size() == 1, "uppercase with punctuation");
}

void testDuplicatesAndOrder() { // 7, 9
    const auto movies = sampleMovies();
    const Indexes indexes(movies);
    // The Phase 1 handoff states these exact IDs.
    check(sameResults(indexes, movies, "star light", 100) == std::vector<MovieId>({5, 3, 4, 7, 6}),
          "duplicate titles stay separate and canonical");
    check(sameResults(indexes, movies, "star") == std::vector<MovieId>({8, 9, 10, 11, 12, 13, 14, 15, 16, 17}),
          "canonical order of a capped result");
}

void testTitleThatPrefixesLongerTitles() { // 8
    const auto movies = sampleMovies();
    const Indexes indexes(movies);
    const auto alien = sameResults(indexes, movies, "alien");
    check(alien.size() == 2 && movies[alien[0]].primary_title == "Alien"
              && movies[alien[1]].primary_title == "Alien 3", "complete title precedes longer title");
    check(sameResults(indexes, movies, "alien 3").size() == 1, "longer title alone");
}

void testResultLimit() { // 10, 16
    std::vector<Movie> movies;
    for (int i = 0; i < 25; ++i) {
        movies.push_back(makeMovie("tt" + std::to_string(i), "matrix " + std::to_string(i), 1990 + i));
    }
    const Indexes indexes(movies);
    check(sameResults(indexes, movies, "matrix").size() == 10, "more matches than the default limit");
    check(indexes.trie.search("matrix").movie_ids.size() == 10
              && indexes.tst.search("matrix").movie_ids.size() == 10, "default argument is ten");
    for (std::size_t limit : {0, 1, 3, 9, 10, 11, 24, 25, 26, 1000}) {
        check(sameResults(indexes, movies, "matrix", limit).size() == std::min<std::size_t>(limit, 25),
              "limit " + std::to_string(limit));
    }
}

void testMissingYears() { // 11
    const std::vector<Movie> movies = {makeMovie("tt3", "Same", 2001), makeMovie("tt2", "Same"),
                                       makeMovie("tt1", "Same"), makeMovie("tt0", "Same", 1950)};
    const Indexes indexes(movies);
    check(sameResults(indexes, movies, "same") == std::vector<MovieId>({2, 1, 3, 0}),
          "missing year (-1) sorts first, then IMDb ID breaks the tie");
}

void testSmallFixtureEveryPrefix() { // 12
    const auto movies = sampleMovies();
    const Indexes indexes(movies);
    check(movies.size() == 22, "fixture size");
    for (const auto& movie : movies) {
        for (std::size_t length = 0; length <= movie.normalized_title.size() + 1; ++length) {
            for (std::size_t limit : {1, 10, 100}) {
                sameResults(indexes, movies, movie.normalized_title.substr(0, length), limit);
            }
        }
    }
}

void testRepeatedQueries() { // 13
    const auto movies = sampleMovies();
    const Indexes indexes(movies);
    const auto trie_first = indexes.trie.search("star");
    const auto tst_first = indexes.tst.search("star");
    const auto trie_stats = indexes.trie.stats();
    const auto tst_stats = indexes.tst.stats();
    for (int i = 0; i < 50; ++i) {
        const auto trie_again = indexes.trie.search("star");
        const auto tst_again = indexes.tst.search("star");
        check(trie_again.movie_ids == trie_first.movie_ids
                  && trie_again.nodes_visited == trie_first.nodes_visited, "Trie repeat");
        check(tst_again.movie_ids == tst_first.movie_ids
                  && tst_again.nodes_visited == tst_first.nodes_visited, "TST repeat");
    }
    check(indexes.trie.stats().node_count == trie_stats.node_count
              && indexes.tst.stats().node_count == tst_stats.node_count, "searches do not grow an index");
}

void testRebuilding() { // 14
    const std::vector<Movie> first = {makeMovie("a", "Avatar"), makeMovie("b", "Brazil")};
    const std::vector<Movie> second = {makeMovie("c", "Casablanca"), makeMovie("d", "Avatar 2")};
    Indexes indexes(first);
    const auto trie_nodes = indexes.trie.stats().node_count;
    const auto tst_nodes = indexes.tst.stats().node_count;
    indexes.rebuild(first);
    check(indexes.trie.stats().node_count == trie_nodes && indexes.tst.stats().node_count == tst_nodes,
          "rebuilding the same data does not accumulate nodes");
    check(sameResults(indexes, first, "avatar").size() == 1, "same data still searchable");
    indexes.rebuild(second);
    check(sameResults(indexes, second, "brazil").empty(), "old titles are gone");
    check(sameResults(indexes, second, "avatar") == std::vector<MovieId>({1}), "IDs refer to the new vector");
    check(sameResults(indexes, second, "casa").size() == 1, "new titles are present");
    indexes.rebuild({});
    check(indexes.trie.search("avatar").movie_ids.empty() && indexes.tst.search("avatar").movie_ids.empty(),
          "empty rebuild clears both");
}

void testStatistics() { // 15
    const auto movies = sampleMovies();
    const Indexes indexes(movies);
    const auto trie_stats = indexes.trie.stats();
    const auto tst_stats = indexes.tst.stats();
    check(trie_stats.estimated_bytes > 0 && tst_stats.estimated_bytes > 0, "positive memory estimates");
    check(tst_stats.node_count > 0, "TST has nodes");
    // Both hold one node per distinct key prefix; only the Trie adds an empty root.
    check(tst_stats.node_count + 1 == trie_stats.node_count, "node counts correspond exactly");
    check(indexes.trie.name() == "Trie" && indexes.tst.name() == "Ternary Search Tree", "names");
    // Visit counts must be plausible: at least the prefix length, and never more
    // than the nodes that exist, since each node is entered at most once per query.
    for (const std::string prefix : {"star", "star light", "alien", "the dark knight"}) {
        const auto length = normalizeTitle(prefix).size();
        check(indexes.trie.search(prefix).nodes_visited >= length, "Trie visits reach the endpoint");
        check(indexes.tst.search(prefix).nodes_visited >= length, "TST visits reach the endpoint");
        check(indexes.tst.search(prefix, 1000).nodes_visited <= tst_stats.node_count, "TST visit upper bound");
        check(indexes.trie.search(prefix, 1000).nodes_visited <= trie_stats.node_count, "Trie visit upper bound");
    }
}

void testRandomizedAgainstScan() {
    std::mt19937 rng(3530);
    std::vector<Movie> movies;
    const std::string alphabet = "abCD09 -!";
    for (std::size_t i = 0; i < 3000; ++i) {
        std::string title;
        const auto length = 1 + rng() % 25;
        for (std::size_t j = 0; j < length; ++j) {
            title.push_back(alphabet[rng() % alphabet.size()]);
        }
        movies.push_back(makeMovie("tt" + std::to_string(i), title, static_cast<int>(rng() % 100) + 1900));
    }
    movies.push_back(makeMovie("same", "Repeat", 2000));
    movies.push_back(makeMovie("same", "Repeat", 2000));
    const Indexes indexes(movies);
    sameResults(indexes, movies, "repeat");
    for (std::size_t i = 0; i < 1000; ++i) {
        const auto& key = movies[rng() % movies.size()].normalized_title;
        sameResults(indexes, movies, key.substr(0, rng() % (key.size() + 2)), rng() % 35);
    }
    check(indexes.tst.stats().node_count + 1 == indexes.trie.stats().node_count, "randomized node counts");
}

void testGenerated150000Records() {
    std::vector<Movie> movies;
    movies.reserve(150000);
    for (std::size_t i = 0; i < 150000; ++i) {
        const auto suffix = std::to_string(1000000 + i).substr(1); // Six digits.
        movies.push_back(makeMovie("tt" + suffix, "Synthetic Movie " + suffix, 1900 + static_cast<int>(i % 126)));
    }
    const Indexes indexes(movies);
    sameResults(indexes, movies, "synthetic");
    sameResults(indexes, movies, "synthetic movie 149999");
    sameResults(indexes, movies, "synthetic movie 150000");
    check(sameResults(indexes, movies, "synthetic", 150001).size() == 150000, "every ID, in order");
}
} // namespace

int main() {
    try {
        testSimpleAndLongPrefixes();
        testNonexistentPrefixes();
        testEmptyPrefixes();
        testCaseAndPunctuation();
        testDuplicatesAndOrder();
        testTitleThatPrefixesLongerTitles();
        testResultLimit();
        testMissingYears();
        testSmallFixtureEveryPrefix();
        testRepeatedQueries();
        testRebuilding();
        testStatistics();
        testRandomizedAgainstScan();
        testGenerated150000Records();
        std::cout << "Passed " << checks << " checks\n";
        return 0;
    } catch (const std::exception& error) {
        std::cerr << "Test failure: " << error.what() << '\n';
        return 1;
    }
}
