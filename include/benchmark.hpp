#pragma once

#include "prefix_index.hpp"

#include <array>
#include <cstdint>
#include <functional>
#include <iosfwd>
#include <memory>
#include <optional>
#include <string>
#include <vector>

// Workload: fixed-seed prefixes drawn from the loaded titles, so a given dataset
// and seed always produce the same queries on every platform.
enum class QueryCategory { Short, Medium, Long, Common, Rare, NoMatch };

const char* queryCategoryName(QueryCategory category);

struct BenchmarkQuery {
    std::string prefix; // Already normalized and unique within the workload.
    QueryCategory category = QueryCategory::Short;
};

constexpr std::size_t kMinimumBenchmarkQueries = 100;

// May return fewer than kMinimumBenchmarkQueries for a very small dataset; the
// report flags that case instead of padding the workload with artificial queries.
std::vector<BenchmarkQuery> generateQueryWorkload(const std::vector<Movie>& movies,
                                                  std::uint32_t seed = 3530);

struct BenchmarkConfig {
    std::uint32_t seed = 3530;
    std::size_t result_limit = 10;
    std::size_t build_repetitions = 5;
    std::size_t warmup_passes = 3;
    std::size_t measured_passes = 50;   // Full workload passes at result_limit.
    std::size_t exhaustive_passes = 5;  // Full workload passes with no result cap.
};

struct LatencySummary {
    double mean_us = 0;
    double median_us = 0;
    double p95_us = 0; // Nearest-rank 95th percentile.
};

LatencySummary summarizeLatencies(std::vector<double> samples_us);

// Measurements for one structure across every query and pass of one limit.
struct PassSummary {
    LatencySummary latency;
    double mean_nodes_visited = 0;
    double mean_results = 0;
};

struct AlgorithmReport {
    std::string name;
    std::vector<double> build_ms; // One sample per build repetition.
    double median_build_ms = 0;
    IndexStats stats;
    PassSummary limited;
    PassSummary exhaustive;
    std::size_t scan_mismatches = 0; // Disagreements with the sorted-scan answer.
};

struct CategoryReport {
    QueryCategory category = QueryCategory::Short;
    std::size_t query_count = 0;
    double mean_results = 0;
    std::array<double, 2> mean_us{};
    std::array<double, 2> mean_nodes_visited{};
};

// One row per query, per structure, per limit: the source for the CSV export.
struct QueryRecord {
    std::string prefix;
    QueryCategory category = QueryCategory::Short;
    std::size_t limit = 0;
    std::size_t total_matches = 0; // All matching movies, regardless of the limit.
    std::array<LatencySummary, 2> latency;
    std::array<std::size_t, 2> nodes_visited{};
    std::array<std::size_t, 2> results{};
    bool results_identical = true;
};

// First place where two result lists disagree, with enough context to debug it.
struct ResultMismatch {
    std::string prefix;
    std::size_t limit = 0;
    std::size_t first_count = 0;
    std::size_t second_count = 0;
    std::size_t first_difference = 0;
    std::vector<MovieId> first_ids;  // Up to 10 IDs starting at first_difference.
    std::vector<MovieId> second_ids;
};

std::optional<ResultMismatch> compareResultLists(const std::string& prefix, std::size_t limit,
                                                 const std::vector<MovieId>& first,
                                                 const std::vector<MovieId>& second);

struct BenchmarkReport {
    std::size_t movie_count = 0;
    BenchmarkConfig config;
    std::vector<BenchmarkQuery> queries;
    std::array<AlgorithmReport, 2> algorithms;
    std::vector<CategoryReport> categories;
    std::vector<QueryRecord> query_records;
    std::size_t comparisons = 0;          // Result lists compared across the structures.
    std::size_t cross_mismatches = 0;     // Comparisons where the lists differed.
    std::vector<ResultMismatch> mismatch_details; // Capped; cross_mismatches has the total.
    double timer_tick_ns = 0;             // Smallest observed clock step.
    double timer_overhead_ns = 0;         // Mean cost of a clock read pair (not subtracted).

    bool passed() const;
};

using IndexFactory = std::function<std::unique_ptr<PrefixIndex>()>;

// Runs the full experiment on any two indexes. Throws std::runtime_error for an
// empty dataset or a config with no measured passes.
BenchmarkReport runIndexBenchmark(const IndexFactory& first, const IndexFactory& second,
                                  const std::vector<Movie>& movies,
                                  const BenchmarkConfig& config = {});

// The project experiment: Trie vs Ternary Search Tree.
BenchmarkReport runBenchmark(const std::vector<Movie>& movies, const BenchmarkConfig& config = {});

void printBenchmarkReport(const BenchmarkReport& report, const std::vector<Movie>& movies,
                          std::ostream& out);

// Writes per-query measurements. Creates parent directories; throws on I/O failure.
void writeBenchmarkCsv(const BenchmarkReport& report, const std::string& path);
