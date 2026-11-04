#include "movie.hpp"
#include "normalize.hpp"
#include "prefix_index.hpp"
#include "imdb_loader.hpp"

#include <algorithm>
#include <chrono>
#include <filesystem>
#include <fstream>
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
} // namespace

int main() {
    try {
        testNormalization();
        testOrdering();
        testLoader();
        std::cout << "Passed " << checks << " checks\n";
        return 0;
    } catch (const std::exception& error) {
        std::cerr << "Test failure: " << error.what() << '\n';
        return 1;
    }
}
