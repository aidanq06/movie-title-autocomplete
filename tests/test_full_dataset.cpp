// Requirement 19: the full 150,000-record workload on the real IMDb file.
// IMDb's data is not in the repository, so this test skips itself (exit code 77,
// reported by CTest as "Skipped") unless the file is available. It looks at the
// MOVIE_DATASET environment variable first, then at data/title.basics.tsv.

#ifdef _MSC_VER
#define _CRT_SECURE_NO_WARNINGS // Reading one variable with std::getenv is fine here.
#endif

#include "benchmark.hpp"
#include "imdb_loader.hpp"
#include "test_support.hpp"
#include "trie.hpp"
#include "tst.hpp"

#include <cstdlib>
#include <filesystem>
#include <iostream>
#include <random>

using namespace test_support;

namespace {
constexpr int kSkipped = 77;
constexpr std::size_t kTargetMovies = 150000;

std::string datasetPath() {
    if (const char* configured = std::getenv("MOVIE_DATASET")) {
        return configured;
    }
    return std::string(TEST_DATA_DIR) + "/title.basics.tsv";
}

void testFullDataset(const std::string& path) {
    const auto loaded = loadImdbMovies(path, kTargetMovies);
    const auto& movies = loaded.movies;
    check(loaded.records_loaded == kTargetMovies && movies.size() == kTargetMovies, "exactly 150,000 records");
    check(loaded.rows_scanned >= kTargetMovies, "rows were scanned to find them");
    for (const auto& movie : movies) {
        check(movie.title_type == "movie", "only movies are selected: " + movie.tconst);
        check(!movie.tconst.empty() && movie.tconst != "\\N", "every record has an IMDb ID");
        check(movie.normalized_title == normalizeTitle(movie.primary_title), "normalized key: " + movie.tconst);
        check(!movie.normalized_title.empty(), "nonempty key: " + movie.tconst);
    }

    Trie trie;
    TernarySearchTree tst;
    trie.build(movies);
    tst.build(movies);
    check(tst.stats().node_count + 1 == trie.stats().node_count, "node counts correspond on real data");

    // Every indexed movie must be findable from its own full key, by both structures.
    std::size_t limit_checked = 0;
    for (MovieId id = 0; id < movies.size(); ++id) {
        const auto from_trie = trie.search(movies[id].normalized_title, movies.size());
        const auto from_tst = tst.search(movies[id].normalized_title, movies.size());
        check(from_trie.movie_ids == from_tst.movie_ids, "structures differ on " + movies[id].normalized_title);
        check(std::find(from_tst.movie_ids.begin(), from_tst.movie_ids.end(), id) != from_tst.movie_ids.end(),
              "movie not found from its own key: " + movies[id].tconst);
        ++limit_checked;
    }
    check(limit_checked == kTargetMovies, "every record verified");

    // A sample compared with a full scan of all 150,000 records.
    std::mt19937 rng(3530);
    for (int i = 0; i < 200; ++i) {
        const auto& key = movies[rng() % movies.size()].normalized_title;
        const auto prefix = key.substr(0, 1 + rng() % key.size());
        checkAgainstScan(trie, movies, prefix, 10);
        checkAgainstScan(tst, movies, prefix, 10);
    }

    BenchmarkConfig config;
    config.build_repetitions = 1;
    config.warmup_passes = 1;
    config.measured_passes = 5;
    config.exhaustive_passes = 1;
    const auto report = runBenchmark(movies, config);
    check(report.queries.size() >= kMinimumBenchmarkQueries, "at least 100 unique queries");
    check(report.movie_count == kTargetMovies, "benchmark ran on all records");
    check(report.passed(), "benchmark correctness checks passed");
    std::cout << "Verified " << path << ": " << movies.size() << " movies, " << trie.stats().node_count
              << " Trie nodes, " << report.queries.size() << " benchmark queries\n";
}
} // namespace

int main() {
    const auto path = datasetPath();
    if (!std::filesystem::exists(path)) {
        std::cout << "Skipped: no IMDb dataset at " << path << ". See the README to download it.\n";
        return kSkipped;
    }
    try {
        testFullDataset(path);
        std::cout << "Passed " << checks << " checks\n";
        return 0;
    } catch (const std::exception& error) {
        std::cerr << "Test failure: " << error.what() << '\n';
        return 1;
    }
}
