#pragma once

#include "benchmark.hpp"
#include "imdb_loader.hpp"

#include <iosfwd>
#include <optional>
#include <string>

// Terminal front end. Loading happens once in main(); everything here shares that
// one movie vector, so `loaded` must outlive these calls.

enum class AlgorithmChoice { Trie, Tst, Both };

// Accepts "trie", "tst", or "both"; returns nothing for any other value.
std::optional<AlgorithmChoice> parseAlgorithmChoice(const std::string& value);

// Exit codes shared by main() and the one-shot commands.
constexpr int kExitSuccess = 0;
constexpr int kExitFailure = 1;
constexpr int kExitBadArguments = 2;
constexpr int kExitResultsDiffer = 3;

void printLoadSummary(const LoadResult& loaded, std::size_t requested_max, std::ostream& out);

// --search: prints results and returns an exit code. Builds only what it needs.
int runSearchCommand(const LoadResult& loaded, const std::string& prefix,
                     AlgorithmChoice algorithm, std::ostream& out);

// --benchmark: runs the full experiment, optionally writing per-query CSV data.
int runBenchmarkCommand(const LoadResult& loaded, const std::string& csv_path,
                        std::ostream& out, std::ostream& err);

// Prints a finished report. Returns kExitResultsDiffer if any correctness check failed.
int reportBenchmarkOutcome(const BenchmarkReport& report, const std::vector<Movie>& movies,
                           const std::string& csv_path, std::ostream& out);

// Interactive menu. Reads until the user exits or the input stream ends.
int runInteractiveMenu(const LoadResult& loaded, const std::string& data_path,
                       std::istream& in, std::ostream& out);
