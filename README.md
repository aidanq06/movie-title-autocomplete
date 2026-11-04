# Movie Title Autocomplete

COP3530 Project 2 — Group 123: **Aidan Quach, Hana Elbatouty, Krish Gupta**.

Find movies from the beginning of a title. The completed project will compare a
manually implemented **Trie** and **Ternary Search Tree (TST)** on the same
150,000 IMDb movie records. Phase 1, implemented by Aidan, provides the C++17
foundation, streaming IMDb loader, normalization, manual Trie, terminal search,
and tests. Krish's Phase 2 will add the TST, comparison menu, and benchmarks.

## Build and test

Use a C++17 compiler and CMake 3.20+ for the commands below. There are no external
C++ dependencies or testing frameworks.

```sh
cmake -S . -B build -DCMAKE_BUILD_TYPE=Release
cmake --build build -j
ctest --test-dir build --output-on-failure
```

CTest runs `core` (450 checks, including 150,000 generated records) and `cli`
(successful searches and invalid-input behavior). Tests also run under AddressSanitizer
and UndefinedBehaviorSanitizer; see the [handoff](docs/PHASE1_HANDOFF.md) for commands
and the actual validation results. The stress dataset is generated in memory.

## Run the committed sample

```sh
./build/movie_autocomplete --help
./build/movie_autocomplete --data data/sample_movies.tsv --search "star"
./build/movie_autocomplete --data data/sample_movies.tsv --search "star light"
./build/movie_autocomplete --data data/sample_movies.tsv --max-movies 4 --search "star"
```

The synthetic sample has 29 data rows and selects 22 movies. `star` returns ten
matches; `star light` returns five distinct records, including duplicate titles
and unknown years. Output includes the loaded count, scanned/malformed rows,
Trie nodes, estimated index storage, titles, years, IMDb IDs, and nodes visited.
`Unknown year` represents `-1`. Output is deterministic on a given build and dataset;
storage bytes depend on the toolchain and platform.

`--max-movies` defaults to 150000 and accepts nonnegative integers (including zero).
An empty normalized prefix gives zero matches. With `--data` and no `--search`,
the program loads/builds and prints how to run a search. With no arguments it
prints usage. Exit codes: 0 success/help, 1 file/header/I/O failure, 2 invalid arguments.

## Download IMDb locally

The loader accepts the **decompressed** official `title.basics.tsv` file.
Do not commit the download or generated benchmark data.

```sh
mkdir -p data
curl -L --fail https://datasets.imdbws.com/title.basics.tsv.gz -o data/title.basics.tsv.gz
date -u +"%Y-%m-%dT%H:%M:%SZ" > data/DOWNLOAD_DATE.txt
gzip -dc data/title.basics.tsv.gz > data/title.basics.tsv
./build/movie_autocomplete --data data/title.basics.tsv --search "star"
```

Run the timestamp/decompression steps after a successful download. Record the
timestamp and a SHA-256 digest with the final experiment; on macOS use
`shasum -a 256 data/title.basics.tsv.gz`, or on Linux use `sha256sum`.
IMDb refreshes these files daily, so a selection is reproducible for a fixed
download, not necessarily across different download dates.

**Phase 1 data provenance (2026-11-04):** the committed fixture is synthetic and
was created on this date. No official IMDb dataset was downloaded or tested in
this session, so there is no official download date or claim of a 150,000-row
IMDb run. The 150,000-record validation used synthetic movies. The commands above
record the real date when you download the official data.

### Selection and parsing

The loader reads one line at a time and finds `tconst`, `titleType`, `primaryTitle`,
`isAdult`, and `startYear` by header name. It selects the **first 150,000 eligible
records in file order** by default: exactly `movie`, exactly `isAdult == "0"`,
a nonempty/non-`\N` primary title, and a nonempty normalized key. No titles are
deduplicated. Missing IMDb IDs and wrong-width rows are skipped and counted as
malformed; missing files, missing/duplicate/empty header names, and read errors
raise exceptions. CRLF and an optional header UTF-8 BOM are supported.

Years must be four ASCII digits with a value above zero; missing or invalid years
become `-1`. A short input returns every eligible movie available and the CLI
reports that it reached EOF before the requested maximum. Only the selected
records and the current source line are retained, not the entire IMDb file.

## Matching and order

`normalizeTitle` lowercases ASCII A–Z, keeps a–z and 0–9, replaces every other byte
with a space, collapses spaces, and trims the result. Titles and prefixes use the
same function. For example, `Spider-Man: Homecoming` becomes
`spider man homecoming`. Matching is character-prefix matching on these keys.
Trailing separators disappear: `star ` searches the same key as `star`.

Results use ascending normalized title, then original primary title, then year
(`-1` first), then IMDb ID. String ordering is bytewise, not locale-sensitive.
Exact comparator ties retain input order. Each result is a `MovieId` index into
the shared `std::vector<Movie>`, so identical titles with different IDs stay separate.

The Trie stores nodes in a vector, sorted sparse child links in each node, and
sorted terminal movie IDs. It uses no map or library Trie. Iterative depth-first
traversal stops as soon as the result limit is reached. `estimated_bytes` counts
allocated node/child/terminal vector storage, **not process RAM**.

## Phase 2 integration and limitations

Start with [docs/PHASE1_HANDOFF.md](docs/PHASE1_HANDOFF.md) for exact APIs, lifetime
rules, statistics conventions, completed validation, and Krish's remaining work.
Both structures must build against the same immutable movie vector, and that
vector must remain alive and at the same address for the duration of index use.

Full Unicode matching/transliteration, popularity ranking, the TST, side-by-side
timings, final interactive comparison menu, performance report, and video are
outside this phase. The included fixture is too small for the course's final
100,000-point requirement; the final experiment must use 150,000 official records.
Only macOS/AppleClang was exercised here; the implementation uses standard C++17
and CMake for macOS and Linux. No Linux test run is claimed.

## Data attribution and licensing

IMDb supplies the official dataset at
[title.basics.tsv.gz](https://datasets.imdbws.com/title.basics.tsv.gz).
See the [IMDb non-commercial dataset documentation](https://developer.imdb.com/non-commercial-datasets/),
[non-commercial licensing information](https://help.imdb.com/article/imdb/general-information/can-i-use-imdb-data-in-my-software/G5JTRESSHJBBHTGX),
and [IMDb conditions of use](https://www.imdb.com/conditions).
Use of IMDb data is governed by IMDb's applicable terms; this repository does not
redistribute the official dataset or grant a license to it. The sample IDs/titles
are synthetic test records and are not an IMDb data extract.

Attribution for use of the official data: Information courtesy of
[IMDb](https://www.imdb.com). Used with permission.

The original course collaboration instructions are preserved in
[docs/COURSE_TEMPLATE.md](docs/COURSE_TEMPLATE.md); original resources remain under
`resources/`. Phase 1 work is tracked in issues #1–#4 and branch
`feature/aidan-foundation-trie`. Preserve the incremental commits when merging so
the contribution history remains visible for course review.
