#include "app.hpp"

#include "format_utils.hpp"
#include "normalize.hpp"
#include "trie.hpp"
#include "tst.hpp"

#include <algorithm>
#include <chrono>
#include <iomanip>
#include <istream>
#include <ostream>
#include <vector>

namespace {
using Clock = std::chrono::steady_clock;

constexpr std::size_t kDisplayLimit = 10;
// One search can take less than a microsecond, close to the clock's resolution,
// so a single reading would be mostly noise. Displayed times use this many runs.
constexpr std::size_t kDisplayTimingRuns = 25;

struct TimedSearch {
    SearchResult result;
    double median_us = 0;
};

double millisecondsSince(Clock::time_point start) {
    return std::chrono::duration<double, std::milli>(Clock::now() - start).count();
}

double timedBuild(PrefixIndex& index, const std::vector<Movie>& movies) {
    const auto start = Clock::now();
    index.build(movies);
    return millisecondsSince(start);
}

TimedSearch timedSearch(const PrefixIndex& index, const std::string& prefix) {
    TimedSearch timed;
    std::vector<double> samples;
    samples.reserve(kDisplayTimingRuns);
    for (std::size_t run = 0; run < kDisplayTimingRuns; ++run) {
        const auto start = Clock::now();
        SearchResult result = index.search(prefix, kDisplayLimit);
        const auto stop = Clock::now();
        samples.push_back(std::chrono::duration<double, std::micro>(stop - start).count());
        timed.result = std::move(result);
    }
    timed.median_us = summarizeLatencies(std::move(samples)).median_us;
    return timed;
}

// Phase 1's display format: Title (Year) [tconst]
std::string describeMovie(const Movie& movie) {
    return movie.primary_title + " ("
        + (movie.year == -1 ? "Unknown year" : std::to_string(movie.year))
        + ") [" + movie.tconst + "]";
}

void printIndexSummary(const PrefixIndex& index, std::ostream& out) {
    const auto stats = index.stats();
    out << index.name() << " nodes: " << stats.node_count << '\n'
        << "Estimated index storage (bytes): " << stats.estimated_bytes << '\n';
}

void printPlainResult(const SearchResult& result, const std::vector<Movie>& movies, std::ostream& out) {
    out << "Matches: " << result.movie_ids.size() << '\n';
    for (MovieId id : result.movie_ids) {
        out << "  " << describeMovie(movies[id]) << '\n';
    }
    out << "Nodes visited: " << result.nodes_visited << '\n';
}

void printRankedResult(const SearchResult& result, const std::vector<Movie>& movies, std::ostream& out) {
    if (result.movie_ids.empty()) {
        out << "  (no matching movies)\n";
        return;
    }
    std::size_t rank = 1;
    for (MovieId id : result.movie_ids) {
        out << std::right << std::setw(4) << rank++ << ". " << describeMovie(movies[id]) << '\n';
    }
}

// Reads one trimmed line. Returns false when the input stream has ended.
bool readLine(std::istream& in, std::string& line) {
    if (!std::getline(in, line)) {
        return false;
    }
    const auto first = line.find_first_not_of(" \t\r");
    if (first == std::string::npos) {
        line.clear();
        return true;
    }
    const auto last = line.find_last_not_of(" \t\r");
    line = line.substr(first, last - first + 1);
    return true;
}

// Holds both indexes for the life of the menu, so searches never rebuild them.
class InteractiveMenu {
public:
    InteractiveMenu(const LoadResult& loaded, const std::string& data_path, std::istream& in, std::ostream& out)
        : loaded_(loaded), movies_(loaded.movies), data_path_(data_path), in_(in), out_(out) {}

    int run() {
        out_ << "\nBuilding both indexes from the " << formatCount(movies_.size()) << " loaded movies...\n";
        trie_build_ms_ = timedBuild(trie_, movies_);
        tst_build_ms_ = timedBuild(tst_, movies_);
        for (;;) {
            printMenu();
            std::string choice;
            if (!readLine(in_, choice)) {
                out_ << "\nEnd of input. Goodbye!\n";
                return kExitSuccess;
            }
            if (choice == "1") {
                searchWith(trie_);
            } else if (choice == "2") {
                searchWith(tst_);
            } else if (choice == "3") {
                compareBoth();
            } else if (choice == "4") {
                runFullBenchmark();
            } else if (choice == "5") {
                printStatistics();
            } else if (choice == "6") {
                out_ << "Goodbye!\n";
                return kExitSuccess;
            } else {
                out_ << "Invalid selection \"" << choice << "\". Please enter a number from 1 to 6.\n";
            }
        }
    }

private:
    void printMenu() {
        const std::string rule(40, '=');
        out_ << '\n' << rule << "\nMOVIE TITLE AUTOCOMPLETE\nLoaded Movies: "
             << formatCount(movies_.size()) << '\n' << rule << '\n'
             << "1. Search with Trie\n"
             << "2. Search with Ternary Search Tree\n"
             << "3. Compare both for a title prefix\n"
             << "4. Run full performance benchmark\n"
             << "5. Display dataset and index statistics\n"
             << "6. Exit\n"
             << "Select an option (1-6): " << std::flush;
    }

    // Returns false if the query cannot be searched or the input has ended.
    bool promptForPrefix(std::string& prefix) {
        out_ << "Enter a movie title prefix: " << std::flush;
        if (!readLine(in_, prefix)) {
            out_ << '\n';
            return false;
        }
        if (normalizeTitle(prefix).empty()) {
            out_ << "That prefix has no letters or digits, so nothing can match it.\n";
            return false;
        }
        return true;
    }

    void searchWith(const PrefixIndex& index) {
        std::string prefix;
        if (!promptForPrefix(prefix)) {
            return;
        }
        const auto timed = timedSearch(index, prefix);
        out_ << "\nAlgorithm: " << index.name() << '\n'
             << "Search key: \"" << normalizeTitle(prefix) << "\"\n"
             << "Showing " << timed.result.movie_ids.size() << " of up to " << kDisplayLimit << " results\n";
        printRankedResult(timed.result, movies_, out_);
        out_ << "Nodes visited: " << formatCount(timed.result.nodes_visited) << '\n'
             << "Query time: " << formatFixed(timed.median_us, 3) << " us (median of "
             << kDisplayTimingRuns << " runs)\n";
    }

    void compareBoth() {
        std::string prefix;
        if (!promptForPrefix(prefix)) {
            return;
        }
        const auto from_trie = timedSearch(trie_, prefix);
        const auto from_tst = timedSearch(tst_, prefix);
        out_ << "\nSearch key: \"" << normalizeTitle(prefix) << "\"\n";
        out_ << '\n' << trie_.name() << " results:\n";
        printRankedResult(from_trie.result, movies_, out_);
        out_ << '\n' << tst_.name() << " results:\n";
        printRankedResult(from_tst.result, movies_, out_);

        constexpr int kLabelWidth = 26;
        constexpr int kValueWidth = 22;
        const auto row = [&](const std::string& label, const std::string& a, const std::string& b) {
            out_ << std::left << std::setw(kLabelWidth) << label << std::right
                 << std::setw(kValueWidth) << a << std::setw(kValueWidth) << b << '\n';
        };
        out_ << '\n';
        row("Metric", trie_.name(), tst_.name());
        out_ << std::string(kLabelWidth + 2 * kValueWidth, '-') << '\n';
        row("Results returned", std::to_string(from_trie.result.movie_ids.size()),
            std::to_string(from_tst.result.movie_ids.size()));
        row("Nodes visited", formatCount(from_trie.result.nodes_visited),
            formatCount(from_tst.result.nodes_visited));
        row("Query time (us)", formatFixed(from_trie.median_us, 3), formatFixed(from_tst.median_us, 3));
        out_ << "Query times are medians of " << kDisplayTimingRuns
             << " runs. A single query is not a benchmark; use option 4 to compare speed.\n";
        if (from_trie.result.movie_ids == from_tst.result.movie_ids) {
            out_ << "Results MATCH: same movies in the same order.\n";
        } else {
            out_ << "Results DO NOT MATCH. This is a bug; please report the prefix above.\n";
        }
    }

    void runFullBenchmark() {
        out_ << "\nRunning the full benchmark. This rebuilds separate indexes and may take a while...\n"
             << std::flush;
        try {
            const auto report = runBenchmark(movies_);
            printBenchmarkReport(report, movies_, out_);
        } catch (const std::exception& error) {
            out_ << "Benchmark could not run: " << error.what() << '\n';
        }
    }

    void printStatistics() {
        constexpr int kLabelWidth = 34;
        constexpr int kValueWidth = 22;
        const auto trie_stats = trie_.stats();
        const auto tst_stats = tst_.stats();
        out_ << "\nDataset: " << data_path_ << '\n'
             << "  Movie records loaded:  " << formatCount(loaded_.records_loaded) << '\n'
             << "  Source rows scanned:   " << formatCount(loaded_.rows_scanned) << '\n'
             << "  Malformed rows skipped: " << formatCount(loaded_.malformed_rows) << "\n\n";
        const auto row = [&](const std::string& label, const std::string& a, const std::string& b) {
            out_ << std::left << std::setw(kLabelWidth) << label << std::right
                 << std::setw(kValueWidth) << a << std::setw(kValueWidth) << b << '\n';
        };
        row("Metric", trie_.name(), tst_.name());
        out_ << std::string(kLabelWidth + 2 * kValueWidth, '-') << '\n';
        row("Nodes", formatCount(trie_stats.node_count), formatCount(tst_stats.node_count));
        row("Estimated index memory", formatMiB(trie_stats.estimated_bytes), formatMiB(tst_stats.estimated_bytes));
        row("Estimated index memory (bytes)", formatCount(trie_stats.estimated_bytes),
            formatCount(tst_stats.estimated_bytes));
        row("Build time at startup (ms)", formatFixed(trie_build_ms_, 1), formatFixed(tst_build_ms_, 1));
        out_ << "Memory is an estimate of index storage (1 MiB = 1,048,576 bytes), not process RAM.\n"
             << "The Trie's node count includes its empty root. Startup build times are single runs;\n"
             << "the benchmark (option 4) reports the median of repeated builds.\n";
    }

    const LoadResult& loaded_;
    const std::vector<Movie>& movies_;
    std::string data_path_;
    std::istream& in_;
    std::ostream& out_;
    Trie trie_;
    TernarySearchTree tst_;
    double trie_build_ms_ = 0;
    double tst_build_ms_ = 0;
};
} // namespace

std::optional<AlgorithmChoice> parseAlgorithmChoice(const std::string& value) {
    if (value == "trie") {
        return AlgorithmChoice::Trie;
    }
    if (value == "tst") {
        return AlgorithmChoice::Tst;
    }
    if (value == "both") {
        return AlgorithmChoice::Both;
    }
    return std::nullopt;
}

void printLoadSummary(const LoadResult& loaded, std::size_t requested_max, std::ostream& out) {
    out << "Loaded records: " << loaded.records_loaded << '\n'
        << "Source rows scanned: " << loaded.rows_scanned << '\n'
        << "Malformed rows skipped: " << loaded.malformed_rows << '\n'
        << "Requested maximum: " << requested_max << '\n';
    if (loaded.records_loaded < requested_max) {
        out << "Reached end of file before requested maximum.\n";
    }
}

int runSearchCommand(const LoadResult& loaded, const std::string& prefix,
                     AlgorithmChoice algorithm, std::ostream& out) {
    const auto& movies = loaded.movies;
    if (algorithm != AlgorithmChoice::Both) {
        Trie trie;
        TernarySearchTree tst;
        PrefixIndex& index = algorithm == AlgorithmChoice::Trie
            ? static_cast<PrefixIndex&>(trie) : static_cast<PrefixIndex&>(tst);
        index.build(movies);
        printIndexSummary(index, out);
        out << "Search prefix: \"" << prefix << "\"\n";
        printPlainResult(index.search(prefix, kDisplayLimit), movies, out);
        return kExitSuccess;
    }

    Trie trie;
    TernarySearchTree tst;
    trie.build(movies);
    tst.build(movies);
    printIndexSummary(trie, out);
    printIndexSummary(tst, out);
    out << "Search prefix: \"" << prefix << "\"\n";
    const auto from_trie = timedSearch(trie, prefix);
    const auto from_tst = timedSearch(tst, prefix);
    const auto print = [&](const PrefixIndex& index, const TimedSearch& timed) {
        out << '\n' << "[" << index.name() << "]\n";
        printPlainResult(timed.result, movies, out);
        out << "Query time: " << formatFixed(timed.median_us, 3) << " us (median of "
            << kDisplayTimingRuns << " runs)\n";
    };
    print(trie, from_trie);
    print(tst, from_tst);
    if (from_trie.result.movie_ids == from_tst.result.movie_ids) {
        out << "\nResults match: yes\n";
        return kExitSuccess;
    }
    out << "\nResults match: NO\n";
    return kExitResultsDiffer;
}

int runBenchmarkCommand(const LoadResult& loaded, const std::string& csv_path,
                        std::ostream& out, std::ostream& err) {
    if (loaded.movies.empty()) {
        err << "Error: cannot benchmark an empty dataset: no eligible movies were loaded\n";
        return kExitFailure;
    }
    out << "\nRunning the full benchmark...\n" << std::flush;
    return reportBenchmarkOutcome(runBenchmark(loaded.movies), loaded.movies, csv_path, out);
}

int reportBenchmarkOutcome(const BenchmarkReport& report, const std::vector<Movie>& movies,
                           const std::string& csv_path, std::ostream& out) {
    printBenchmarkReport(report, movies, out);
    if (!csv_path.empty()) {
        writeBenchmarkCsv(report, csv_path);
        out << "Wrote per-query measurements to " << csv_path << '\n';
    }
    return report.passed() ? kExitSuccess : kExitResultsDiffer;
}

int runInteractiveMenu(const LoadResult& loaded, const std::string& data_path,
                       std::istream& in, std::ostream& out) {
    if (loaded.movies.empty()) {
        out << "The dataset has no eligible movies, so there is nothing to search.\n";
        return kExitSuccess;
    }
    return InteractiveMenu(loaded, data_path, in, out).run();
}
