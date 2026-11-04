# Movie Title Autocomplete

COP3530 Project 2, Group 123: **Aidan Quach, Hana Elbatouty, Krish Gupta**.

Type the start of a movie title and get matching movies back. The program answers
the same query with two data structures written from scratch, a **Trie** and a
**Ternary Search Tree (TST)**, on the same 150,000 IMDb movie records. It then
compares them on construction time, query time, node visits, and memory, and
checks that they return identical results.

| Part | Owner | Contents |
| --- | --- | --- |
| Phase 1 | Aidan Quach | C++17 foundation, IMDb loader, normalization, Trie, first tests ([handoff](docs/PHASE1_HANDOFF.md)) |
| Phase 2 | Krish Gupta | TST, benchmark, terminal interface, integration tests, documentation |

Design notes and measured results for the report are in
[docs/ANALYSIS_NOTES.md](docs/ANALYSIS_NOTES.md).

## Quick start

You need a C++17 compiler and CMake 3.20 or newer. There are no other dependencies.

```bash
cmake -S . -B build -DCMAKE_BUILD_TYPE=Release
```

```bash
cmake --build build -j
```

```bash
ctest --test-dir build --output-on-failure
```

```bash
./build/movie_autocomplete --data data/sample_movies.tsv
```

That last command opens the menu on a small synthetic sample in the repository,
so it works before you download anything. For real results, get the IMDb data below.

**Windows:** CLion's bundled MinGW and CMake work. Add its `mingw\bin` folder to
`PATH` so the programs can find the C++ runtime, and pass `-G Ninja` to the first
command. With Visual Studio's generator, use `cmake --build build --config Release`
and `ctest --test-dir build -C Release`, and the program is at
`build\Release\movie_autocomplete.exe`. Run `chcp 65001` first so accented titles
display correctly.

## Get the IMDb dataset

The loader reads the **decompressed** official `title.basics.tsv`. The file is about
1.1 GB and is ignored by Git. Do not commit it.

```bash
curl -L --fail https://datasets.imdbws.com/title.basics.tsv.gz -o data/title.basics.tsv.gz
```

```bash
date -u +"%Y-%m-%dT%H:%M:%SZ" > data/DOWNLOAD_DATE.txt
```

```bash
gzip -dc data/title.basics.tsv.gz > data/title.basics.tsv
```

IMDb refreshes this file daily, so a selection is only reproducible for one
snapshot. Record the download date and a SHA-256 digest (`sha256sum`, or
`shasum -a 256` on macOS) alongside any results you report.

## Using the program

### Interactive menu

```bash
./build/movie_autocomplete --data data/title.basics.tsv
```

```text
========================================
MOVIE TITLE AUTOCOMPLETE
Loaded Movies: 150,000
========================================
1. Search with Trie
2. Search with Ternary Search Tree
3. Compare both for a title prefix
4. Run full performance benchmark
5. Display dataset and index statistics
6. Exit
```

The data loads once and both indexes are built once at startup, then stay in memory
for repeated searches. Searches show up to ten movies with their years and IMDb IDs.

### One-shot commands

```bash
./build/movie_autocomplete --data data/sample_movies.tsv --search "star" --algorithm both
```

```bash
./build/movie_autocomplete --data data/title.basics.tsv --max-movies 150000 --benchmark
```

| Option | Meaning |
| --- | --- |
| `--data PATH` | Decompressed IMDb-format TSV file (required except for `--help`) |
| `--max-movies N` | Select up to N eligible movies. Default 150000; 0 is allowed |
| `--search PREFIX` | Print up to ten matches and exit |
| `--algorithm NAME` | Structure for `--search`: `trie` (default), `tst`, or `both` |
| `--benchmark` | Run the full benchmark and exit |
| `--csv FILE` | With `--benchmark`, also write per-query measurements to FILE |
| `--help` | Print usage |

With `--data` alone the menu starts. With no arguments the program prints usage.

Exit codes: `0` success, `1` file or I/O failure, `2` invalid arguments, `3` the two
structures returned different results.

### Demo walkthrough

For a recorded demo, after downloading the data:

1. Start the menu with the command above. Point out `Loaded Movies: 150,000`.
2. Option 1, prefix `star`: Trie results with years and node visits.
3. Option 2, prefix `STAR!`: the same movies from the TST. Case and punctuation do not matter.
4. Option 3, prefix `alien`: side-by-side comparison ending in `Results MATCH`.
5. Option 3, prefix `hamlet`: 20 movies share this exact title, so all ten results are separate
   films called Hamlet, ordered by year.
6. Option 5: node counts and estimated memory.
7. Option 4: the full benchmark, which takes a few seconds on the 150,000 records.

## How matching works

`normalizeTitle` lowercases ASCII letters, keeps `a-z` and `0-9`, turns every other
byte into a separator, collapses repeated separators to one space, and trims the ends.
Titles and queries use the same function, so `Spider-Man: Homecoming` is stored as
`spider man homecoming`, and `SPIDER man!` finds it. Matching is by character prefix.
A query with no letters or digits returns no results.

Results are ordered by normalized title, then original title, then year (unknown years
first), then IMDb ID, comparing bytes rather than locale rules. Exact ties keep input
order. Each result is an index into one shared `std::vector<Movie>`, so movies with
identical titles stay separate and neither index copies any movie strings.

### Which rows are selected

The loader takes the **first 150,000 eligible rows in file order**: `titleType` exactly
`movie`, `isAdult` exactly `0`, a real primary title, and at least one letter or digit
after normalization. Titles are not deduplicated. Wrong-width rows and rows without an
IMDb ID count as malformed and are skipped. Years must be four digits above zero;
anything else becomes unknown.

IMDb's file is roughly in ID order, so the first 150,000 lean old. See
[Known limitations](#known-limitations).

## The two data structures

Both implement the `PrefixIndex` interface (`build`, `search`, `stats`, `name`), keep
nodes in one vector linked by index, and sort movies at each terminal with the same
comparator.

**Trie** ([src/trie.cpp](src/trie.cpp)). Each node stands for one prefix and owns a
sorted vector of `(character, child)` edges plus its terminal movie IDs. Finding a child
is a binary search of that vector. Enumeration visits a node's movies, then its children
in order. An explicit stack avoids recursion on long titles.

**Ternary Search Tree** ([src/tst.cpp](src/tst.cpp)). Each node holds one character and
three inline links. To follow a character, compare it with the node's: go left if smaller,
right if larger, and through the middle link to the next position if equal. Left and
right links are a small binary search tree of the characters possible at that position.
Search finds the node for the last prefix character, emits its movies, and walks only its
middle subtree in order: left, this node's movies, middle, right. The endpoint's own left
and right links are never followed, because they hold other characters and so do not
share the prefix. An explicit stack avoids recursion here as well.

Both structures end up with one node per distinct title prefix. The Trie adds one
empty root, so its node count is always the TST's plus one.

## Benchmark

```bash
./build/movie_autocomplete --data data/title.basics.tsv --max-movies 150000 --benchmark --csv results/benchmark.csv
```

The `results/` folder is ignored by Git. Build in Release mode for meaningful timings.

- **Queries.** 130 unique prefixes from a fixed seed (3530): 20 short (1-2 characters),
  30 medium (3-5), 30 long (6-10), 15 most common, 15 rare (3 or fewer matches), and
  20 checked to match nothing. Selection uses raw `mt19937` output rather than standard
  library distributions, so the same data gives the same queries on any platform.
- **Construction.** Five fresh builds each, from the same in-memory vector, alternating
  which structure goes first. Loading and normalization are excluded because they happen
  beforehand, and destroying the index is not timed. Reported as the median.
- **Queries, timed.** Three untimed warm-up passes, then 50 measured passes at the limit
  of ten in a shuffled order, alternating which structure goes first. Then 5 passes with
  no limit, to show the cost of enumerating every match. Only the search call is timed.
- **Correctness.** In every pass, every result list is compared between the structures,
  and each is compared with an independent answer from a sorted scan. Any mismatch prints
  the prefix and the first differing movies, marks the run `FAILED`, and exits with code 3.

### What the metrics mean

| Metric | Definition |
| --- | --- |
| Construction time | Median wall-clock time of `build()`, in milliseconds |
| Nodes allocated | Nodes in the structure. The Trie's count includes its empty root |
| Estimated index memory | Allocated capacity of the node vector and each node's dynamic vectors, in MiB (1 MiB = 1,048,576 bytes). This is an **estimate of index storage, not process RAM**. It leaves out the shared movie records and allocator overhead, and includes unused vector capacity, so it varies by standard library |
| Query time | Pooled over every sample of every query, so each query counts equally. Mean, median, and nearest-rank 95th percentile, in microseconds. Clock read overhead is reported but not subtracted |
| Nodes visited | Nodes entered per query, summed over the descent and the enumeration. **These are not equal units of work**: the Trie's binary search over a child vector counts as one visit, while each sibling a TST passes through counts as its own visit |
| Mismatches | Result lists that differed from the sorted scan. Must be 0 |

### Results

Measured on 150,000 movies from the 2026-11-04 snapshot, on an Intel Core Ultra 9 185H
laptop with 32 GB of RAM, Windows 11, GCC 13.1, Release build. The table shows one of
three runs; the others and the Linux and MSVC results are in the
[analysis notes](docs/ANALYSIS_NOTES.md#measured-results). Your numbers will differ.

| Metric | Trie | Ternary Search Tree |
| --- | --- | --- |
| Construction time, median of 5 | 366.4 ms | 229.2 ms |
| Nodes allocated | 1,395,441 | 1,395,440 |
| Estimated index memory | 119.00 MiB | 113.17 MiB |
| Mean query time, limit 10 | 3.940 us | 2.797 us |
| Median query time, limit 10 | 3.700 us | 2.600 us |
| 95th percentile query time, limit 10 | 8.400 us | 6.200 us |
| Mean nodes visited, limit 10 | 66.9 | 75.8 |
| Mismatches against sorted scan | 0 | 0 |

These results compare **these two implementations**. Our Trie uses compact sorted child
vectors, not the textbook fixed array of children, and that choice matters. The analysis
notes explain why this should not be read as "TSTs are faster than Tries".

## Tests

```bash
ctest --test-dir build --output-on-failure
```

| CTest name | Covers |
| --- | --- |
| `core` | Phase 1: normalization, ordering, loader, Trie, 150,000 generated records |
| `tst` | TST branching, ordering, duplicates, limits, 100,000-character titles |
| `indexes` | Trie, TST, and an independent scan agree across every required scenario |
| `benchmark` | Workload generation, statistics, CSV, and detection of deliberately wrong results |
| `cli` | Every command-line path, error handling, and a scripted run through the whole menu |
| `full_dataset` | The real 150,000 records. Reports `Skipped` unless the dataset is available |

To include the real data, place it at `data/title.basics.tsv` or point to it:

```bash
MOVIE_DATASET=/path/to/title.basics.tsv ctest --test-dir build --output-on-failure
```

To run under AddressSanitizer and UndefinedBehaviorSanitizer with GCC or Clang:

```bash
cmake -S . -B build-sanitize -DCMAKE_BUILD_TYPE=Debug -DCMAKE_CXX_FLAGS="-fsanitize=address,undefined -fno-omit-frame-pointer" -DCMAKE_EXE_LINKER_FLAGS="-fsanitize=address,undefined"
```

```bash
cmake --build build-sanitize -j
```

```bash
ctest --test-dir build-sanitize --output-on-failure
```

## Known limitations

- **ASCII matching only.** There is no Unicode case folding or transliteration, so
  `café noir` is stored as `caf noir`, and a title with no ASCII letters or digits is left out.
- **The selection is old.** Taking the first 150,000 eligible rows gives mostly pre-2000
  films (median year 1968 in our snapshot) and no popularity ranking. It meets the data
  size requirement but is not a sample of all movies, so recent titles often will not appear.
- **Prefix search only.** It matches the start of a title, so `knight` will not find
  `The Dark Knight`.
- **Timings are from one laptop.** Runs vary, especially means and rare categories. The
  Windows clock has a 100 ns step, which is coarse for the fastest queries of under a microsecond.
- **Memory is an estimate.** It is sensitive to vector growth policy: the same data gives
  119.00 / 113.17 MiB with libstdc++ and 94.68 / 85.25 MiB with MSVC.
- **Platform coverage.** See [what was validated](docs/ANALYSIS_NOTES.md#validation) for
  exactly which compilers and operating systems ran which tests.

## Data attribution and licensing

IMDb supplies the official dataset at
[title.basics.tsv.gz](https://datasets.imdbws.com/title.basics.tsv.gz). See the
[IMDb non-commercial dataset documentation](https://developer.imdb.com/non-commercial-datasets/),
[non-commercial licensing information](https://help.imdb.com/article/imdb/general-information/can-i-use-imdb-data-in-my-software/G5JTRESSHJBBHTGX),
and [IMDb conditions of use](https://www.imdb.com/conditions). Use of IMDb data is
governed by IMDb's terms. This repository does not redistribute the official dataset or
grant a license to it. The IDs and titles in `data/sample_movies.tsv` are synthetic test
records, not an IMDb extract.

Attribution for use of the official data: Information courtesy of
[IMDb](https://www.imdb.com). Used with permission.

The original course instructions are preserved in
[docs/COURSE_TEMPLATE.md](docs/COURSE_TEMPLATE.md) and `resources/`.
