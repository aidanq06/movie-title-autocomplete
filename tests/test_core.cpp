#include "movie.hpp"
#include "normalize.hpp"
#include "prefix_index.hpp"
#include "imdb_loader.hpp"
#include "trie.hpp"

#include <algorithm>
#include <chrono>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <numeric>
#include <random>
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

class TempTsv {
public:
    std::filesystem::path directory;
    std::filesystem::path path;

    explicit TempTsv(const std::string& content) {
        const auto stamp = std::chrono::steady_clock::now().time_since_epoch().count();
        for (std::size_t i = 0;; ++i) {
            directory = std::filesystem::temp_directory_path()
                      / ("movie-autocomplete-test-" + std::to_string(stamp) + "-" + std::to_string(i));
            if (std::filesystem::create_directory(directory)) break;
        }
        path = directory / "input.tsv";
        std::ofstream out(path, std::ios::binary);
        out << content;
        if (!out) throw std::runtime_error("cannot write test input");
    }
    ~TempTsv() {
        std::error_code error;
        std::filesystem::remove_all(directory, error);
    }
    TempTsv(const TempTsv&) = delete;
    TempTsv& operator=(const TempTsv&) = delete;
};

void expectLoadError(const std::string& path) {
    bool threw = false;
    try { loadImdbMovies(path); }
    catch (const std::runtime_error&) { threw = true; }
    check(threw, "file/header error must throw: " + path);
}

void testLoader() {
    const std::string fixture = std::string(TEST_DATA_DIR) + "/sample_movies.tsv";
    const auto loaded = loadImdbMovies(fixture);
    check(loaded.movies.size() == 22 && loaded.records_loaded == 22, "eligible fixture count");
    check(loaded.rows_scanned == 29 && loaded.malformed_rows == 1, "row accounting");
    check(loaded.movies.front().tconst == "tt0000001", "source order");
    check(loaded.movies[1].normalized_title == "spider man homecoming", "loader normalization");
    check(loaded.movies[2].primary_title == " Toy Story! ", "preserve original title");
    check(loaded.movies[3].year == -1 && loaded.movies[6].year == -1, "missing and invalid year");
    check(loaded.movies[4].primary_title == loaded.movies[7].primary_title
          && loaded.movies[4].tconst != loaded.movies[7].tconst, "keep separate duplicate titles");
    for (const auto& movie : loaded.movies) {
        check(movie.title_type == "movie" && !movie.normalized_title.empty(), "eligible records only");
        check(movie.tconst != "tt0000011", "filter adult movies");
    }
    const auto limited = loadImdbMovies(fixture, 4);
    check(limited.records_loaded == 4 && limited.rows_scanned == 4, "configurable selection limit");
    check(loadImdbMovies(fixture, 0).rows_scanned == 0, "zero limit");
    TempTsv reordered("\xEF\xBB\xBFstartYear\tprimaryTitle\ttconst\tisAdult\ttitleType\r\n"
                      "2000\tMixed-Case\tttA\t0\tmovie\r\n"
                      "2000\tSkip\tttB\t\\N\tmovie\r\n"
                      "2000\tSkip\t\\N\t0\tmovie\r\n"
                      "2000\tToo\tfew\r\n"
                      "2000\tToo\tttC\t0\tmovie\textra\r\n"
                      "2000\tSkip\tttD\t\tmovie\r\n"
                      "2000\t\xE4\xB8\xAD\tttE\t0\tmovie\r\n");
    const auto shuffled = loadImdbMovies(reordered.path.string());
    check(shuffled.records_loaded == 1 && shuffled.rows_scanned == 7
          && shuffled.malformed_rows == 3, "reordered header, BOM, CRLF, malformed rows");
    check(shuffled.movies[0].normalized_title == "mixed case", "reordered field mapping");
    for (const std::string year : {"\\N", "", "0000", "-1", "200x", "2000x", "999999999999999"}) {
        TempTsv invalid("tconst\ttitleType\tprimaryTitle\tisAdult\tstartYear\n"
                        "ttX\tmovie\tTitle\t0\t" + year + "\n");
        check(loadImdbMovies(invalid.path.string()).movies.at(0).year == -1, "invalid year: " + year);
    }
    for (const std::string header : {"", "tconst\tprimaryTitle\n",
            "tconst\ttitleType\tprimaryTitle\tisAdult\tstartYear\ttconst\n",
            "tconst\ttitleType\tprimaryTitle\tisAdult\tstartYear\t\n"}) {
        TempTsv invalid(header);
        expectLoadError(invalid.path.string());
    }
    TempTsv header_only("tconst\ttitleType\tprimaryTitle\tisAdult\tstartYear\n");
    check(loadImdbMovies(header_only.path.string()).records_loaded == 0, "header-only file");
    expectLoadError((header_only.directory / "missing.tsv").string());
}

Movie makeMovie(const std::string& id, const std::string& title, int year = -1) {
    return {id, title, "movie", year, normalizeTitle(title)};
}

// Independent scan + canonical sort oracle; reusable when the TST is added.
std::vector<MovieId> expectedMatches(const std::vector<Movie>& movies,
                                    const std::string& raw_prefix, std::size_t limit) {
    const auto prefix = normalizeTitle(raw_prefix);
    std::vector<MovieId> ids;
    if (prefix.empty()) return ids;
    for (MovieId id = 0; id < movies.size(); ++id) {
        if (movies[id].normalized_title.compare(0, prefix.size(), prefix) == 0) {
            ids.push_back(id);
        }
    }
    std::stable_sort(ids.begin(), ids.end(), MovieIdLess{movies});
    if (ids.size() > limit) ids.resize(limit);
    return ids;
}

void checkSearch(const PrefixIndex& index, const std::vector<Movie>& movies,
                 const std::string& prefix, std::size_t limit = 10) {
    check(index.search(prefix, limit).movie_ids == expectedMatches(movies, prefix, limit),
          index.name() + " differs from scan oracle for: " + prefix.substr(0, 80));
}

void testTrie() {
    Trie trie;
    check(trie.search("x").movie_ids.empty(), "search before build");
    const auto loaded = loadImdbMovies(std::string(TEST_DATA_DIR) + "/sample_movies.tsv");
    trie.build(loaded.movies);
    const PrefixIndex& index = trie;
    check(index.name() == "Trie", "polymorphic index name");
    check(trie.stats().node_count > 1 && trie.stats().estimated_bytes > 0, "nonempty stats");
    for (const std::string prefix : {"star", "STAR!", "Star Light", "alien", "alien 3",
            "spider-man", "THE", "toy", "missing", "", "!!!", "star light beyond"}) {
        for (std::size_t limit : {std::size_t{0}, std::size_t{1}, std::size_t{10}, std::size_t{100}}) {
            checkSearch(index, loaded.movies, prefix, limit);
        }
    }
    check(trie.search("star").movie_ids.size() == 10, "default result limit is ten");
    check(trie.search("STAR LIGHT", 100).movie_ids == std::vector<MovieId>({5, 3, 4, 7, 6}),
          "duplicate title canonical ordering");
    check(trie.search("").nodes_visited == 0 && trie.search("star", 0).nodes_visited == 0,
          "empty prefix and zero limit do no node work");

    const std::vector<Movie> small = {makeMovie("a", "A"), makeMovie("ab", "AB"),
                                    makeMovie("ac", "AC"), makeMovie("empty", "!!!")};
    trie.build(small);
    check(trie.stats().node_count == 4, "one root plus three character nodes");
    check(trie.search("a", 1).nodes_visited == 2, "stop immediately on terminal limit");
    check(trie.search("a", 10).nodes_visited == 4, "prefix endpoint counted once");
    check(trie.search("ax").nodes_visited == 2, "missing edge counts only reached nodes");
    check(trie.search("z").nodes_visited == 1, "root-only miss");
    checkSearch(trie, small, "a");
    check(trie.search("star").movie_ids.empty(), "rebuild clears old keys");

    const std::string long_title = "Long " + std::string(100000, 'X');
    const std::vector<Movie> long_movies = {makeMovie("long", long_title)};
    trie.build(long_movies);
    checkSearch(trie, long_movies, "long"); // Traverse the deep subtree without recursion.
    checkSearch(trie, long_movies, long_title);
    checkSearch(trie, long_movies, long_title + "x");
    const std::vector<Movie> empty;
    trie.build(empty);
    check(trie.stats().node_count == 1 && trie.search("a").movie_ids.empty(), "empty rebuild");
}

void testTrieAgainstOracle() {
    std::mt19937 rng(3530);
    std::vector<Movie> movies;
    for (std::size_t i = 0; i < 2000; ++i) {
        std::string title;
        const auto length = 1 + rng() % 25;
        const std::string alphabet = "abCD09 -!";
        for (std::size_t j = 0; j < length; ++j) title.push_back(alphabet[rng() % alphabet.size()]);
        movies.push_back(makeMovie("tt" + std::to_string(i), title, static_cast<int>(rng() % 100) + 1900));
    }
    // Include exact comparator ties; their IDs must stay in source order.
    movies.push_back(makeMovie("same", "Repeat", 2000));
    movies.push_back(makeMovie("same", "Repeat", 2000));
    Trie trie;
    trie.build(movies);
    checkSearch(trie, movies, "repeat");
    for (std::size_t i = 0; i < 300; ++i) {
        const auto& key = movies[rng() % movies.size()].normalized_title;
        checkSearch(trie, movies, key.substr(0, rng() % (key.size() + 2)), rng() % 35);
    }
}

void testTrieStress() {
    std::vector<Movie> movies;
    movies.reserve(150000);
    for (std::size_t i = 0; i < 150000; ++i) {
        const auto suffix = std::to_string(1000000 + i).substr(1); // Six digits.
        movies.push_back(makeMovie("tt" + suffix, "Synthetic Movie " + suffix, 1900 + i % 126));
    }
    Trie trie;
    trie.build(movies);
    check(trie.stats().node_count > 150000 && trie.stats().estimated_bytes > 0, "150000-record build");
    checkSearch(trie, movies, "synthetic");
    checkSearch(trie, movies, "synthetic movie 149999");
    checkSearch(trie, movies, "synthetic movie 150000");
    checkSearch(trie, movies, "synthetic", 150001); // Verify every inserted ID, in order.
    std::cout << "Built and verified 150000 synthetic movie records\n";
}
} // namespace

int main() {
    try {
        testNormalization();
        testOrdering();
        testLoader();
        testTrie();
        testTrieAgainstOracle();
        testTrieStress();
        std::cout << "Passed " << checks << " checks\n";
        return 0;
    } catch (const std::exception& error) {
        std::cerr << "Test failure: " << error.what() << '\n';
        return 1;
    }
}
