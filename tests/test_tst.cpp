#include "test_support.hpp"
#include "trie.hpp"
#include "tst.hpp"

#include <iostream>
#include <random>
#include <utility>

using namespace test_support;

namespace {
void testEmptyIndex() {
    TernarySearchTree tst;
    check(tst.name() == "Ternary Search Tree", "index name");
    check(tst.stats().node_count == 0 && tst.stats().estimated_bytes == 0, "unbuilt stats");
    tst.build({});
    check(tst.stats().node_count == 0, "empty build has no nodes");
}

void testNodeCounts() {
    TernarySearchTree tst;
    const std::vector<Movie> movies = {makeMovie("a", "A"), makeMovie("ab", "AB"),
                                       makeMovie("ac", "AC"), makeMovie("empty", "!!!")};
    tst.build(movies);
    check(tst.stats().node_count == 3, "one node per distinct nonempty prefix");
    check(tst.stats().estimated_bytes > 0, "estimated bytes");

    // Each TST node stands for one distinct nonempty key prefix, exactly like a
    // non-root Trie node, whatever order the titles were inserted in.
    Trie trie;
    const std::vector<Movie> titles = {makeMovie("1", "cat"), makeMovie("2", "cap"),
                                       makeMovie("3", "car"), makeMovie("4", "cat"),
                                       makeMovie("5", "dog"), makeMovie("6", "cats")};
    trie.build(titles);
    tst.build(titles);
    check(tst.stats().node_count + 1 == trie.stats().node_count, "same prefixes as the Trie");
}

void testInsertSurvivesReallocation() {
    // Many sibling and deep inserts force nodes_ to reallocate during insertion.
    std::vector<Movie> movies;
    for (int i = 0; i < 5000; ++i) {
        movies.push_back(makeMovie("tt" + std::to_string(i), "movie " + std::to_string(i * 7919)));
    }
    Trie trie;
    TernarySearchTree tst;
    trie.build(movies);
    tst.build(movies);
    check(tst.stats().node_count + 1 == trie.stats().node_count, "no nodes lost on growth");
}

std::vector<MovieId> idsFor(const TernarySearchTree& tst, const std::string& prefix,
                            std::size_t limit = 100) {
    return tst.search(prefix, limit).movie_ids;
}

void testLeftMiddleRightBranching() {
    TernarySearchTree tst;
    // b is inserted first, so it is the root; a goes left and c goes right.
    const std::vector<Movie> balanced = {makeMovie("b", "b"), makeMovie("a", "a"), makeMovie("c", "c")};
    tst.build(balanced);
    check(tst.search("b").nodes_visited == 1, "root matched directly");
    check(tst.search("a").nodes_visited == 2 && idsFor(tst, "a") == std::vector<MovieId>({1}),
          "left branch");
    check(tst.search("c").nodes_visited == 2 && idsFor(tst, "c") == std::vector<MovieId>({2}),
          "right branch");
    check(tst.search("0").nodes_visited == 2 && idsFor(tst, "0").empty(), "miss below left leaf");
    check(tst.search("d").nodes_visited == 2 && idsFor(tst, "d").empty(), "miss below right leaf");

    // Ascending inserts form a chain of right links, so c is reached third.
    const std::vector<Movie> chain = {makeMovie("a", "a"), makeMovie("b", "b"), makeMovie("c", "c")};
    tst.build(chain);
    check(tst.search("c").nodes_visited == 3, "right chain depth follows insertion order");

    // One title is a chain of middle links.
    const std::vector<Movie> word = {makeMovie("abc", "abc")};
    tst.build(word);
    check(tst.search("abc").nodes_visited == 3, "middle links advance the position");
    check(tst.search("a").nodes_visited == 3, "enumeration walks the middle subtree");
    check(tst.search("ax").nodes_visited == 2 && idsFor(tst, "ax").empty(), "miss after middle link");

    // Third characters branch left (p) then right (r) below t, but sort p, r, t.
    const std::vector<Movie> cats = {makeMovie("t", "cat"), makeMovie("p", "cap"), makeMovie("r", "car")};
    tst.build(cats);
    check(idsFor(tst, "ca") == std::vector<MovieId>({1, 2, 0}), "in-order across branches");
    check(tst.search("ca").nodes_visited == 5, "descent plus each subtree node once");
}

void testOnlyPrefixMatchesAreReturned() {
    TernarySearchTree tst;
    const std::vector<Movie> movies = {makeMovie("m", "mid"), makeMovie("a", "alpha"),
                                       makeMovie("z", "zulu"), makeMovie("mi", "mix"),
                                       makeMovie("mo", "mod")};
    tst.build(movies);
    // The endpoint for "mi" has a sibling path to "mod"; alpha/zulu hang off the root.
    check(idsFor(tst, "mi") == std::vector<MovieId>({0, 3}), "siblings of the endpoint excluded");
    check(idsFor(tst, "m") == std::vector<MovieId>({0, 3, 4}), "root siblings excluded");
    check(idsFor(tst, "mid") == std::vector<MovieId>({0}), "exact title only");
    check(idsFor(tst, "midd").empty(), "prefix longer than the title");
    check(idsFor(tst, "b").empty(), "character between existing siblings");
}

void testTitleThatPrefixesOthers() {
    TernarySearchTree tst;
    const std::vector<Movie> movies = {makeMovie("3", "Alien 3"), makeMovie("r", "Aliens"),
                                       makeMovie("1", "Alien"), makeMovie("a", "Ali")};
    tst.build(movies);
    check(idsFor(tst, "ali") == std::vector<MovieId>({3, 2, 0, 1}), "shorter keys come first");
    check(idsFor(tst, "alien") == std::vector<MovieId>({2, 0, 1}), "terminal at the endpoint");
    check(idsFor(tst, "ali", 1) == std::vector<MovieId>({3}), "endpoint terminal honors limit");
}

void testDuplicateTitlesAreKept() {
    TernarySearchTree tst;
    // Canonical order: normalized title, original title, year (-1 first), tconst.
    const std::vector<Movie> movies = {makeMovie("tt3", "Star Light", 2001),
                                       makeMovie("tt1", "Star Light", 2001),
                                       makeMovie("tt2", "Star Light"),
                                       makeMovie("tt5", "STAR LIGHT", 1999),
                                       makeMovie("tt4", "Star-Light", 1980)};
    tst.build(movies);
    check(idsFor(tst, "star light") == std::vector<MovieId>({3, 2, 1, 0, 4}), "canonical duplicates");
    check(tst.stats().node_count == 10, "duplicates share one key path");
    // Exact comparator ties stay in input order.
    const std::vector<Movie> ties = {makeMovie("same", "Repeat", 2000), makeMovie("same", "Repeat", 2000)};
    tst.build(ties);
    check(idsFor(tst, "repeat") == std::vector<MovieId>({0, 1}), "stable exact ties");
}

void testInsertionOrderDoesNotChangeResults() {
    std::mt19937 rng(3530);
    std::vector<Movie> movies;
    const std::string alphabet = "abcdexyz0189 ";
    for (int i = 0; i < 400; ++i) {
        std::string title;
        const auto length = 1 + rng() % 8;
        for (std::size_t j = 0; j < length; ++j) {
            title.push_back(alphabet[rng() % alphabet.size()]);
        }
        movies.push_back(makeMovie("tt" + std::to_string(i), title, 1900 + static_cast<int>(rng() % 100)));
    }
    TernarySearchTree tst;
    for (int round = 0; round < 5; ++round) {
        tst.build(movies);
        for (const std::string prefix : {"a", "b", "x", "0", "1 ", "ab", "z y", "8"}) {
            checkAgainstScan(tst, movies, prefix, 1000);
        }
        // Fisher-Yates with raw engine output, so every platform shuffles the same way.
        for (std::size_t i = movies.size() - 1; i > 0; --i) {
            std::swap(movies[i], movies[rng() % (i + 1)]);
        }
    }
}

void testLimitStopsTraversal() {
    TernarySearchTree tst;
    std::vector<Movie> movies = {makeMovie("a", "a"), makeMovie("ab", "ab"), makeMovie("ac", "ac")};
    tst.build(movies);
    check(tst.search("a", 1).nodes_visited == 1, "stop on the endpoint's own terminal");
    check(tst.search("a", 2).nodes_visited == 2, "stop as soon as the second match appears");
    check(tst.search("a", 10).nodes_visited == 3, "whole subtree when under the limit");
    check(idsFor(tst, "a", 0).empty() && tst.search("a", 0).nodes_visited == 0, "zero limit");

    movies.clear();
    for (int i = 0; i < 20000; ++i) {
        movies.push_back(makeMovie("tt" + std::to_string(i), "same prefix " + std::to_string(i)));
    }
    tst.build(movies);
    const auto bounded = tst.search("same prefix", 10);
    check(bounded.movie_ids.size() == 10, "limit returns ten");
    check(bounded.nodes_visited < 200, "bounded query leaves most of the subtree unvisited");
    check(tst.search("same prefix", 100000).nodes_visited > tst.stats().node_count - 20,
          "unbounded query reaches the whole subtree");
}

void testVeryLongTitleUsesNoRecursion() {
    TernarySearchTree tst;
    const std::string long_title = "Long " + std::string(100000, 'X');
    const std::vector<Movie> movies = {makeMovie("long", long_title),
                                       makeMovie("branch", long_title.substr(0, 50000) + "A")};
    tst.build(movies);
    checkAgainstScan(tst, movies, "long");
    checkAgainstScan(tst, movies, long_title);
    checkAgainstScan(tst, movies, long_title + "x");
}

void testEmptyAndNormalizedQueries() {
    TernarySearchTree tst;
    check(tst.search("x").movie_ids.empty() && tst.search("x").nodes_visited == 0, "before build");
    const std::vector<Movie> movies = {makeMovie("s", "Spider-Man: Homecoming"), makeMovie("n", "!!!")};
    tst.build(movies);
    check(idsFor(tst, "SPIDER man").size() == 1, "case and punctuation fold together");
    check(idsFor(tst, "  spider---man  ").size() == 1, "separators collapse");
    check(idsFor(tst, "").empty() && tst.search("").nodes_visited == 0, "empty prefix");
    check(idsFor(tst, "!!!").empty() && tst.search("!!!").nodes_visited == 0, "punctuation only");
    check(tst.stats().node_count == std::string("spider man homecoming").size(), "empty keys skipped");
    tst.build({});
    check(tst.stats().node_count == 0 && idsFor(tst, "spider").empty(), "rebuild clears old keys");
}
} // namespace

int main() {
    try {
        testEmptyIndex();
        testNodeCounts();
        testInsertSurvivesReallocation();
        testLeftMiddleRightBranching();
        testOnlyPrefixMatchesAreReturned();
        testTitleThatPrefixesOthers();
        testDuplicateTitlesAreKept();
        testInsertionOrderDoesNotChangeResults();
        testLimitStopsTraversal();
        testVeryLongTitleUsesNoRecursion();
        testEmptyAndNormalizedQueries();
        std::cout << "Passed " << checks << " checks\n";
        return 0;
    } catch (const std::exception& error) {
        std::cerr << "Test failure: " << error.what() << '\n';
        return 1;
    }
}
