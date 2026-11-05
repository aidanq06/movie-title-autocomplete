#include "app.hpp"
#include "benchmark.hpp"
#include "imdb_loader.hpp"
#include "test_support.hpp"
#include "trie.hpp"
#include "tst.hpp"

#include <algorithm>
#include <array>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <random>
#include <set>
#include <sstream>

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

BenchmarkConfig fastConfig() {
    BenchmarkConfig config;
    config.build_repetitions = 2;
    config.warmup_passes = 1;
    config.measured_passes = 5;
    config.exhaustive_passes = 1;
    return config;
}

void testHonestRunPasses() {
    const auto movies = generatedMovies(5000);
    const auto report = runBenchmark(movies, fastConfig());
    check(report.passed() && report.cross_mismatches == 0, "real indexes agree");
    check(report.movie_count == 5000 && report.queries.size() >= kMinimumBenchmarkQueries, "report covers the workload");
    check(report.comparisons == report.queries.size() * 6, "every query compared in every pass");
    check(report.algorithms[0].name == "Trie" && report.algorithms[1].name == "Ternary Search Tree", "names");
    for (const auto& algorithm : report.algorithms) {
        check(algorithm.build_ms.size() == 2 && algorithm.median_build_ms > 0, "build samples");
        check(algorithm.stats.node_count > 0 && algorithm.stats.estimated_bytes > 0, "memory stats");
        check(algorithm.limited.latency.mean_us > 0 && algorithm.limited.mean_nodes_visited > 0, "query stats");
        check(algorithm.limited.mean_results <= 10, "limited pass respects the limit");
        check(algorithm.exhaustive.mean_results >= algorithm.limited.mean_results, "no cap returns at least as many");
    }
    check(report.query_records.size() == report.queries.size() * 2, "one record per query per limit");
    check(!report.categories.empty(), "category summaries");
}

// A wrapper that answers correctly except for the fault it is told to inject.
class FaultyIndex : public PrefixIndex {
public:
    enum class Fault { ReverseOrder, DropLast, WrongMovie, Missing };

    explicit FaultyIndex(Fault fault) : fault_(fault) {}

    void build(const std::vector<Movie>& movies) override { trie_.build(movies); }
    IndexStats stats() const override { return trie_.stats(); }
    std::string name() const override { return "Faulty"; }

    SearchResult search(const std::string& raw_prefix, std::size_t limit) const override {
        SearchResult result = trie_.search(raw_prefix, limit);
        if (result.movie_ids.size() >= 2) {
            switch (fault_) {
            case Fault::ReverseOrder:
                std::reverse(result.movie_ids.begin(), result.movie_ids.end());
                break;
            case Fault::DropLast:
                result.movie_ids.pop_back();
                break;
            case Fault::WrongMovie:
                result.movie_ids.back() = result.movie_ids.back() + 1000000;
                break;
            case Fault::Missing:
                result.movie_ids.clear();
                break;
            }
        }
        return result;
    }

private:
    Trie trie_;
    Fault fault_;
};

// 17. The benchmark must catch intentionally wrong result lists.
void testBenchmarkDetectsMismatches() {
    const auto movies = generatedMovies(2000);
    const IndexFactory good = [] { return std::make_unique<Trie>(); };
    for (auto fault : {FaultyIndex::Fault::ReverseOrder, FaultyIndex::Fault::DropLast,
                       FaultyIndex::Fault::WrongMovie, FaultyIndex::Fault::Missing}) {
        const IndexFactory bad = [fault] { return std::make_unique<FaultyIndex>(fault); };
        const auto report = runIndexBenchmark(good, bad, movies, fastConfig());
        check(!report.passed(), "fault must fail the benchmark");
        check(report.cross_mismatches > 0, "fault counted across structures");
        check(report.algorithms[0].scan_mismatches == 0, "the correct index stays clean");
        check(report.algorithms[1].scan_mismatches > 0, "the faulty index is identified");
        check(!report.mismatch_details.empty() && !report.mismatch_details.front().prefix.empty(),
              "the failing prefix is recorded");
        std::ostringstream printed;
        printBenchmarkReport(report, movies, printed);
        check(printed.str().find("FAILED") != std::string::npos, "report states the failure");
        check(printed.str().find("prefix \"" + report.mismatch_details.front().prefix + "\"") != std::string::npos,
              "report names the failing prefix");
        check(printed.str().find("PASSED") == std::string::npos, "report does not claim success");
        std::ostringstream command_output;
        check(reportBenchmarkOutcome(report, movies, "", command_output) == kExitResultsDiffer,
              "command-line mode exits nonzero on a mismatch");
    }
    std::ostringstream honest_output;
    check(reportBenchmarkOutcome(runBenchmark(movies, fastConfig()), movies, "", honest_output) == kExitSuccess,
          "command-line mode exits zero when results agree");

    // Both agree with each other but not with the data: only the scan can catch this.
    const IndexFactory bad_a = [] { return std::make_unique<FaultyIndex>(FaultyIndex::Fault::ReverseOrder); };
    const auto shared_bug = runIndexBenchmark(bad_a, bad_a, movies, fastConfig());
    check(shared_bug.cross_mismatches == 0 && !shared_bug.passed(), "a shared bug still fails");
}

void testCompareResultLists() {
    using Ids = std::vector<MovieId>;
    check(!compareResultLists("a", 10, Ids{1, 2}, Ids{1, 2}), "equal lists");
    check(!compareResultLists("a", 10, Ids{}, Ids{}), "equal empty lists");
    const auto order = compareResultLists("a", 10, Ids{1, 2, 3}, Ids{1, 3, 2});
    check(order && order->first_difference == 1 && order->first_ids == Ids({2, 3}), "order difference");
    const auto length = compareResultLists("a", 10, Ids{1, 2}, Ids{1, 2, 3});
    check(length && length->first_difference == 2 && length->first_ids.empty()
              && length->second_ids == Ids({3}) && length->second_count == 3, "length difference");
}

void testLatencySummary() {
    const auto single = summarizeLatencies({5.0});
    check(single.mean_us == 5.0 && single.median_us == 5.0 && single.p95_us == 5.0, "one sample");
    const auto even = summarizeLatencies({4.0, 1.0, 3.0, 2.0});
    check(even.mean_us == 2.5 && even.median_us == 2.5 && even.p95_us == 4.0, "even count, unsorted");
    std::vector<double> hundred;
    for (int i = 1; i <= 100; ++i) {
        hundred.push_back(static_cast<double>(i));
    }
    const auto many = summarizeLatencies(hundred);
    check(many.p95_us == 95.0 && many.median_us == 50.5, "nearest-rank percentile");
    check(summarizeLatencies({}).mean_us == 0.0, "no samples");
}

void testEmptyDatasetAndBadConfig() {
    const auto throws = [](const std::vector<Movie>& movies, const BenchmarkConfig& config) {
        try {
            runBenchmark(movies, config);
        } catch (const std::runtime_error&) {
            return true;
        }
        return false;
    };
    check(throws({}, fastConfig()), "empty dataset is rejected");
    BenchmarkConfig no_passes = fastConfig();
    no_passes.measured_passes = 0;
    check(throws(generatedMovies(100), no_passes), "zero measured passes is rejected");
}

void testCsvOutput() {
    const auto movies = generatedMovies(1000);
    const auto report = runBenchmark(movies, fastConfig());
    const auto directory = std::filesystem::temp_directory_path()
        / ("movie-benchmark-test-" + std::to_string(std::random_device{}()));
    const auto path = directory / "nested" / "queries.csv";
    writeBenchmarkCsv(report, path.string());
    std::ifstream input(path);
    std::string line;
    std::getline(input, line);
    check(line.rfind("prefix,category,limit,", 0) == 0, "CSV header");
    std::size_t rows = 0;
    while (std::getline(input, line)) {
        ++rows;
    }
    check(rows == report.query_records.size() * 2, "one CSV row per record per structure");
    input.close();
    std::error_code ignored;
    std::filesystem::remove_all(directory, ignored);
}
} // namespace

int main() {
    try {
        testWorkloadShape();
        testWorkloadIsDeterministic();
        testWorkloadOnSmallDatasets();
        testWorkloadMatchesAcrossStructures();
        testHonestRunPasses();
        testBenchmarkDetectsMismatches();
        testCompareResultLists();
        testLatencySummary();
        testEmptyDatasetAndBadConfig();
        testCsvOutput();
        std::cout << "Passed " << checks << " checks\n";
        return 0;
    } catch (const std::exception& error) {
        std::cerr << "Test failure: " << error.what() << '\n';
        return 1;
    }
}
