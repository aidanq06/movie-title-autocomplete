# Phase 1 handoff to Krish Gupta

**Project:** Movie Title Autocomplete — Comparing a Trie and a Ternary Search Tree

**Course/team:** COP3530 Project 2, Group 123 — Aidan Quach, Hana Elbatouty, Krish Gupta

**Phase 1 owner:** Aidan Quach

**Validation date:** 2026-11-04

**Branch:** `feature/aidan-foundation-trie`

Phase 1 supplies a working C++17 library, IMDb loader, manual Trie, minimal CLI,
synthetic fixture, and dependency-free tests. No TST, comparison engine, final
interactive menu, final performance report, or video is implemented. There are
no known unresolved Phase 1 code defects after the checks below.

## Files and build targets

| File | Responsibility |
| --- | --- |
| `CMakeLists.txt` | C++17, `movie_core` library, `movie_autocomplete`, CTest `core`/`cli` |
| `include/movie.hpp` | Movie records, ID/result/stat types, shared ordering |
| `include/prefix_index.hpp` | Stable polymorphic contract for both indexes |
| `include/normalize.hpp`, `src/normalize.cpp` | Canonical ASCII normalization |
| `include/imdb_loader.hpp`, `src/imdb_loader.cpp` | Streaming header-driven TSV loader |
| `include/trie.hpp`, `src/trie.cpp` | Manual node-indexed Trie, prefix traversal, stats |
| `src/main.cpp` | Minimal Phase 1 argument parsing and single-query CLI |
| `tests/test_core.cpp` | Core, loader, Trie, oracle, and 150,000-record stress checks |
| `tests/test_cli.cmake` | CLI output/order, deterministic results, errors, limits |
| `data/sample_movies.tsv` | Small synthetic IMDb-format fixture, intentionally imperfect |
| `README.md` | Build/run/download/provenance/attribution/limitations |
| `.gitignore` | Build output, raw downloads, generated datasets, benchmark output, local junk |
| `docs/COURSE_TEMPLATE.md`, `resources/` | Preserved original course template material |

`movie_core` exports `include/` publicly. Add `src/ternary_search_tree.cpp` to this
target when implementing the TST. No package manager or external C++ dependency
is required. CMake enforces C++17 without compiler extensions. Use CMake 3.20+
for the documented `ctest --test-dir` commands.

## Exact public shared API

These are global names, not namespaced. Retain these signatures for integration.

```cpp
// include/movie.hpp
using MovieId = std::size_t;

struct Movie {
    std::string tconst;
    std::string primary_title;
    std::string title_type;
    int year = -1;
    std::string normalized_title;
};

struct SearchResult {
    std::vector<MovieId> movie_ids;
    std::size_t nodes_visited = 0;
};

struct IndexStats {
    std::size_t node_count = 0;
    std::size_t estimated_bytes = 0;
};

inline bool movieLess(const Movie& lhs, const Movie& rhs);
struct MovieIdLess {
    const std::vector<Movie>& movies;
    bool operator()(MovieId lhs, MovieId rhs) const;
};

// include/prefix_index.hpp
class PrefixIndex {
public:
    virtual ~PrefixIndex() = default;
    virtual void build(const std::vector<Movie>& movies) = 0;
    virtual SearchResult search(const std::string& raw_prefix,
                                std::size_t limit = 10) const = 0;
    virtual IndexStats stats() const = 0;
    virtual std::string name() const = 0;
};

// include/normalize.hpp
std::string normalizeTitle(const std::string& input);

// include/imdb_loader.hpp
struct LoadResult {
    std::vector<Movie> movies;
    std::size_t rows_scanned = 0;
    std::size_t records_loaded = 0;
    std::size_t malformed_rows = 0;
};
LoadResult loadImdbMovies(const std::string& path,
                          std::size_t maxMovies = 150000);

// include/trie.hpp
class Trie : public PrefixIndex {
public:
    void build(const std::vector<Movie>& movies) override;
    SearchResult search(const std::string& raw_prefix,
                        std::size_t limit = 10) const override;
    IndexStats stats() const override;
    std::string name() const override; // Returns "Trie".
    // Private node storage/insertion omitted from this API excerpt.
};
```

`MovieId` is always an index into the original shared vector; never a copy of a
movie, IMDb numeric suffix, or position in a sorted replacement vector.
`build()` consumes already-normalized movie records. The loader prepares them;
manually created records must set `normalized_title = normalizeTitle(primary_title)`.
Empty keys are skipped by the Trie. Rebuilding replaces the prior index.

**Lifetime requirement:** keep the vector alive, unmodified, and at the same
address while either index is used. Do not sort, append, clear, move, or mutate it
after building. Indexes may store a pointer to this vector. The Trie itself only
stores IDs and does not need a persistent movie pointer after sorting terminals,
but result IDs still refer to that vector. A `MovieIdLess` instance also holds a
reference and must not outlive it. `search()` is const and has only local scratch
state. Do not call `build()` concurrently with queries.

## Loading and selection

Source: <https://datasets.imdbws.com/title.basics.tsv.gz>

Documentation: <https://developer.imdb.com/non-commercial-datasets/>

The loader accepts a decompressed UTF-8 TSV and streams it with `std::getline`.
It maps the five required header names (`tconst`, `titleType`, `primaryTitle`,
`isAdult`, `startYear`), so reordered headers and extra named columns work.
It tolerates CRLF and an optional UTF-8 BOM at the start of the header. Empty or
duplicate header names, missing required names, missing/unreadable files, and
stream read errors throw `std::runtime_error` with the path and cause.
Even a zero-record request validates the file and header.

For each data line, increment `rows_scanned`, excluding the header. Wrong-width
rows (including blank lines) and empty/`\N` IMDb IDs increment `malformed_rows`
and are skipped. All other rejected rows are filters rather than malformed rows.
Retain only rows with exactly `titleType == "movie"`, exactly `isAdult == "0"`,
nonempty/non-`\N` primary titles, and nonempty normalized keys. Do not deduplicate.
Keep the first `maxMovies` eligible rows in original order and stop immediately
at the limit. A zero limit scans zero data rows. `records_loaded` always equals
`movies.size()`. EOF before the limit is valid; the CLI explicitly reports it.

`startYear` must be exactly four ASCII digits and greater than zero; otherwise
use `-1`, including `\N`, empty strings, signs, suffixes, and oversized integers.
Original title bytes are preserved for display. Other unused column contents are
not validated or copied into records. The parser retains just the current line,
string views into that line, and selected movie records. Initial record capacity
is capped at 150,000, even for larger requested maxima; it can grow normally.

The fixture was created on **2026-11-04** and contains 29 source rows, 22 eligible
records, and one malformed row. It includes non-movies, adult/missing/empty titles,
punctuation, case variants, duplicate titles with distinct IDs, and missing/invalid
years. It is synthetic, not an extract from IMDb.

**Official dataset download date: not applicable in this session.** No official
download or official 150,000-record run occurred. Use the README download commands
and record the actual date in ignored `data/DOWNLOAD_DATE.txt` along with a SHA-256
digest when preparing the final comparison. IMDb refreshes the source daily;
preserve the exact snapshot for reproducible results across teammates. The loader
has no artificial restriction below 150,000; the synthetic stress test verifies
the Trie at that size, not the number of eligible rows in a downloaded snapshot.

## Normalization, ordering, and duplicates

`normalizeTitle` maps ASCII uppercase to lowercase, retains only ASCII a–z and
0–9, and treats every other byte as a separator. Consecutive separators collapse
to one space; leading/trailing separators disappear. This is locale independent.
Apply it once to each title during loading and to each raw prefix inside each
index's `search()`. Examples:

```text
THE DARK KNIGHT          -> the dark knight
Spider-Man: Homecoming   -> spider man homecoming
 Toy Story!              -> toy story
```

Unsupported UTF-8 bytes become separators; there is no Unicode case folding or
transliteration. For example, `café noir` becomes `caf noir`. A title consisting
only of unsupported characters is excluded. An empty normalized prefix or a
zero limit returns no IDs and zero visits. A trailing space in a prefix is
trimmed, so `star ` and `star` have the same behavior.

Use `movieLess`/`MovieIdLess` rather than inventing another ranking:

1. `normalized_title`, ascending.
2. Original `primary_title`, ascending bytewise.
3. `year`, ascending (`-1` sorts before known years).
4. `tconst`, ascending bytewise.

Traverse different normalized keys lexicographically. At a terminal containing
multiple IDs, use `std::stable_sort(ids.begin(), ids.end(), MovieIdLess{movies})`.
Stable sorting preserves input order when all four fields tie. Do not add a
popularity ranking or deduplicate by title, key, or year. A key that is itself a
complete title precedes its longer descendants (for example, `alien` before
`alien 3`). Both indexes must return exactly the same IDs in exactly this order.

For the fixture, `search("star light", 100).movie_ids` is `{5, 3, 4, 7, 6}`.
The default `search("star")` returns IDs `{8, 9, 10, 11, 12, 13, 14, 15, 16, 17}`.
These are shared-vector indices after filtering, not source row numbers.

## Trie implementation and statistics conventions

Each node owns `vector<pair<char, size_t>> children` and
`vector<MovieId> terminal_ids`. All nodes live in one `vector<Node>`, root at 0.
Child vectors are kept in ascending character order using binary search and
insertion. Node growth uses indices and reacquires references after reallocation.
No map, unordered map, fixed 128/256-pointer arrays, copied movie strings, or
external Trie implementation is used.

Search normalizes the prefix, follows its edges, then visits terminals before
children in an iterative lexicographic DFS. Explicit frames avoid recursion
overflow for long keys. Results stop immediately at the requested limit. No
full-subtree sorting or collecting all matches happens during a bounded query.

**Nodes visited:** a nonempty, positive-limit search counts the root once, each
successfully reached prefix node once, and each subsequently entered descendant
once. The prefix endpoint is not counted twice. A failed child lookup does not
count a nonexistent node; an immediate miss counts the root only. Comparisons
inside a child-vector binary search are not separate node visits. Empty-prefix
and zero-limit searches count zero. This metric reflects actual structure work;
TST counts may differ, but it must document what it counts, including low/high
comparisons and equal links. Keep instrumentation local to each query.

**Index storage estimate:**

```text
nodes_.capacity() * sizeof(Node)
+ sum(node.children.capacity() * sizeof(pair<char, size_t>))
+ sum(node.terminal_ids.capacity() * sizeof(MovieId))
```

`node_count` means constructed nodes (`nodes_.size()`), not backing capacity.
An empty/unbuilt Trie contains one root. `estimated_bytes` includes unused
allocated capacity and vector control objects inside each Node, and excludes
the outer Trie object, shared movies/strings, allocator bookkeeping, stack,
query scratch space, and returned results. It is **estimated index storage, not
exact process RAM or measured peak memory**. Apply the same capacity/owned-storage
principle to the TST, document its formula, and avoid double-counting shared movies.
The sample on the tested platform has 75 nodes and 7,656 estimated bytes;
byte estimates may differ across standard libraries or architectures.

With alphabet size A (at most 37 for valid keys), total normalized title length L,
new edges E, and terminal multiplicities k, construction is
O(L log A + E * A + sum(k log k)), including sorted-vector shifts for new edges.
The alphabet bound keeps the per-edge shift cost small. Query work is O(raw prefix bytes +
normalized prefix length * log A + visited subtree nodes + returned IDs).
DFS scratch storage is O(subtree depth). `stats()` scans all allocated nodes;
do not put it inside a timed query loop in Phase 2.

## Validation actually executed

Environment: macOS arm64, AppleClang **21.0.0 (clang-2100.1.1.101)**,
CMake **4.3.1**, C++17. No Linux test run or official IMDb benchmark is claimed.
All commands below ran successfully from the repository root on 2026-11-04:

```sh
cmake -S . -B build -DCMAKE_BUILD_TYPE=Release
cmake --build build -j
ctest --test-dir build --output-on-failure
./build/test_core
./build/movie_autocomplete --data data/sample_movies.tsv --search star
./build/movie_autocomplete --data data/sample_movies.tsv --search 'star light'
./build/movie_autocomplete --data data/sample_movies.tsv --max-movies 4 --search star
cmake --version
c++ --version
```

Results: **2/2 CTest targets passed** (`core`, `cli`); direct `test_core` reported
**450 checks passed**, including a generated 150,000-movie build and a full
comparison of all 150,000 returned IDs with a sorted scan oracle. Sample `star`
returned ten matches and 17 node visits. Sample `star light` returned five matches
and 11 node visits. Limiting the loader to four records scanned four rows and
returned one `star` match, with 11 visits. These are correctness observations,
not final performance benchmark measurements.

Sanitizer commands also ran successfully:

```sh
cmake -S . -B build-sanitize -DCMAKE_BUILD_TYPE=Debug -DCMAKE_CXX_FLAGS='-fsanitize=address,undefined -fno-omit-frame-pointer' -DCMAKE_EXE_LINKER_FLAGS='-fsanitize=address,undefined'
cmake --build build-sanitize -j
ctest --test-dir build-sanitize --output-on-failure
```

Results: **2/2 passed**, including the CLI and 150,000-record stress check, with
no reported AddressSanitizer or UndefinedBehaviorSanitizer errors. macOS leak
sanitizer coverage is not claimed.

Coverage includes normalization/case/punctuation/digits/UTF-8 bytes/empty input,
filtering and missing fields, year validation, reordered/BOM/CRLF headers,
missing files and malformed headers/rows, distinct duplicate titles, exact
canonical ordering, prefix misses, arbitrary result limits including zero and
ten, terminal-before-descendant ordering, repeat builds, empty indexes, exact
node-visit examples, and positive statistics. Randomized differential checks use
a fixed `mt19937` seed of 3530 and 2,002 records. A 100,005-character title tests
both long-prefix descent and deep subtree traversal. No huge fixture is committed.

## Krish's Phase 2 work

1. Add `include/ternary_search_tree.hpp` and `src/ternary_search_tree.cpp` with a
   manual `TernarySearchTree : public PrefixIndex`. Implement `build`, `search`,
   `stats`, and `name` with the signatures above. A TST node should represent
   its own character and low/equal/high links, with terminal ID collections;
   choose the storage details yourself. No existing library TST.
2. Update `CMakeLists.txt` to compile the TST into `movie_core`. Keep the loader,
   MovieId contract, normalizer, and comparator unchanged unless the team agrees
   on and documents an API change.
3. Build both indexes from the **same** loaded vector. Inside TST search,
   normalize the raw prefix, locate its endpoint, emit its terminal IDs first,
   then traverse its equal subtree in lexicographic order. Do not search the
   endpoint's low/high siblings as prefix descendants. Sort duplicate terminals
   with the shared stable comparator and honor early limits.
4. Extend `tests/test_core.cpp` or add `tests/test_indexes.cpp`. Existing
   `expectedMatches` and `checkSearch(const PrefixIndex&, ...)` helpers illustrate
   a reusable correctness oracle. Compare exact ID sequences from Trie, TST,
   and the oracle for fixture/randomized/long/missing/duplicate cases before
   measuring performance.
5. Add the final interactive comparison menu by extending/replacing `src/main.cpp`
   and updating CLI tests as needed. Keep useful command-line runs available for
   reproducible queries.
6. Add benchmark sources/headers (for example, `include/benchmark.hpp` and
   `src/benchmark.cpp`). Measure build time, query time, node visits, and the
   documented index storage estimate. Exclude loader/display/`stats()` work from
   timed queries, use the same raw queries and limits for both indexes, and
   verify equal results. Decide repetitions, clock, query set, and reporting in
   Phase 2; no final benchmark engine is supplied here.
7. Download the official snapshot, record date and digest, confirm the CLI reports
   150,000 loaded records, and run the required experiment on both structures.
   Keep raw data and temporary results out of Git. Add the final critical
   analysis/report and presentation/video materials in coordination with the team.

Example integration (after implementing the TST):

```cpp
const auto loaded = loadImdbMovies("data/title.basics.tsv", 150000);
Trie trie;
TernarySearchTree tst;
PrefixIndex* indexes[] = {&trie, &tst};
for (PrefixIndex* index : indexes) {
    index->build(loaded.movies);
    const SearchResult result = index->search("star", 10);
    // Display by indexing loaded.movies[result.movie_ids[i]].
}
// loaded is declared first, so it outlives both index objects.
```

## Git workflow and next checkout

Issues #1–#4 track the foundation, loader, Trie, and CLI/handoff respectively and
are assigned to Aidan (`aidanq06`). Each tested milestone was committed and
published before work on the next feature. Commit subjects use lowercase
conventional prefixes and issue references. The original `main` history and
course resources are preserved. Review the Phase 1 PR and merge with a regular
merge commit (rather than squash) to retain incremental contribution history.

This execution environment blocks writing `.git` in the supplied workspace, and
the shell's `gh` credentials are invalid. GitHub publication therefore uses the
connected GitHub app's Git Data API with normal parent-linked commits and
non-forced branch updates. No force push, merge, or rewriting of others' work
was performed. A normal clone outside this restricted session uses standard Git:

```sh
git clone https://github.com/aidanq06/movie-title-autocomplete.git
cd movie-title-autocomplete
git switch feature/aidan-foundation-trie
cmake -S . -B build -DCMAKE_BUILD_TYPE=Release
cmake --build build -j
ctest --test-dir build --output-on-failure
```

After the PR merges, Krish should clone or update `main`, then create his own
feature branch before starting Phase 2:

```sh
git switch main
git pull --ff-only origin main
git switch -c feature/krish-tst-comparison
```

The Git checkout commands above are handoff instructions, not claimed successful
shell commands in this restricted session. There is no outstanding Phase 1 code
blocker. Remaining validation is the official IMDb snapshot and Linux portability;
the remaining product work belongs to Phase 2 as listed above.
