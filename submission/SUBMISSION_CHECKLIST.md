# Project 2b submission preparation

Group 123: Aidan Quach, Hana Elbatouty, Krish Gupta.

Deadline from the supplied Canvas assignment: **Friday, November 6, 2026, 11:59 p.m.** Check the Canvas course timezone. The submission counts for the whole group.

## What to submit

The group deliverable asks for the public repository URL:

https://github.com/aidanq06/movie-title-autocomplete

Make the report PDF easy to find in the repository and link it from the README when final. The PDF must include both the repository URL and the YouTube video URL. Check the Canvas submission controls for any additional attachment requirement; the supplied instructions explicitly request the repository URL.

| Component | Rubric weight | Current status |
| --- | --- | --- |
| Code and functionality | 2 points | Implemented and merged; verified on 150,000 IMDb movies |
| Report PDF | 2 points | Draft being prepared; team inputs and video URL still needed |
| Video walkthrough | 2 points | Deferred at Aidan's request; record/upload later |
| Individual peer feedback | Separate 1% of grade | Every member completes it, including self-evaluation |
| Group feedback | Separate 1% of grade | Complete together and submit once for the group |

The assignment prose contains inconsistent report percentages; the displayed grading rubric allocates 2 points each to code, report, and video. Cover every listed report prompt rather than relying on the percentage typo.

## Report requirements

Maximum **five pages total**, including headings, links, and references. Use **12-point type**. Do not add a separate cover page outside the limit. The assignment states a 20% penalty for each extra page.

| Required content | Planned location | State |
| --- | --- | --- |
| Team name | Page 1 | Group 123 used as the default |
| Team members and GitHub usernames | Page 1 | Aidan/Krish verified; Hana has no coding role and username is not supplied |
| Repository URL | Page 1 | Available |
| YouTube demo URL | Page 1 | Pending recording and upload |
| Problem | Page 1 | Drafted from implemented project |
| Motivation | Page 1 | Drafted from implemented project |
| Features implemented | Page 1 | Verified against current main |
| Description of data | Page 1 | Source, filters, size, and snapshot selection documented |
| Tools, languages, APIs, libraries | Page 2 | Standard C++17, CMake, CTest, standard library, IMDb TSV |
| Algorithms implemented | Page 2 | Manual Trie and TST |
| Additional structures/algorithms | Page 2 | Vectors, binary search, sorting, iterative DFS, benchmark query set and oracle |
| Responsibility and roles | Page 2 | Coding roles verified; noncoding contributions require confirmation |
| Changes since proposal and rationale | Page 3 | Original proposal is needed to confirm differences |
| Worst-case complexity of major features | Page 3 | Include normalization, streaming load, construction, search, and auxiliary algorithms |
| Group experience | Page 5 | Team-authored reflection needed |
| Challenges | Page 5 | Technical challenges can be described; personal experience requires confirmation |
| What to change in project/workflow | Page 5 | Team confirmation needed |
| What each member learned | Page 5 | Each member must provide their own takeaway |
| References | Page 5 | Dataset and implementation sources included |

The rubric summarizes 15 prompts, while the detailed list has more individually listed items. This checklist covers the entire detailed list.

## Evidence already verified

- Phase 1 PR #5 and Phase 2 PR #11 are merged into main.
- Reviewed main commit: c40a9c0eb4273ffe69e9c8eda23833d31c1e061e.
- On 2026-11-05, all six CTest targets passed in Release on macOS arm64 / AppleClang 21.
- All six passed under AddressSanitizer and UndefinedBehaviorSanitizer, including the official 150,000-record dataset test.
- Full benchmark: 130 unique prefixes, 7,150 cross-structure comparisons, zero cross-structure or sorted-scan mismatches.
- The 2026-11-05 review download has SHA-256 24c5b3e2b193a2b4800832822b931b28277a79cdbca44e14027eedc71da23f14. Keep this separate from Krish's 2026-11-04 benchmark snapshot.
- Published benchmark figures in the report come from the documented Windows GCC Release run in docs/ANALYSIS_NOTES.md. Do not mix measurements across platforms or snapshots.

## Team inputs still needed

1. Original proposal, or a summary of its promised features and expectations, to confirm what changed.
2. Each member's real experience, challenges, learning, and preferred workflow changes.
3. Any noncoding contributions to include in the roles section. Do not invent responsibilities or coding credit. Aidan has requested that Hana not be added to the coding repository.
4. Final YouTube URL once the video is uploaded. Keep it accessible as Public or Unlisted and test it signed out.

## Video for later

Keep the final recording below five minutes; aim for approximately 4 minutes 30 seconds. The assignment allows one presenter or a group recording. Show the live application and its actual outputs rather than only slides.

Cover startup with 150,000 movies, Trie search, TST search, case/punctuation handling, duplicate titles and unknown years, a no-match/empty query, side-by-side agreement, statistics, the full benchmark, and the CSV command. Show a small fixture briefly if it is easier to demonstrate unknown years. Do not claim recent titles are present in the first-150,000 selection.

After uploading to YouTube, put the real URL in the report and README. A repository URL or a local terminal executable does not replace the required video URL. No hosted website is required for this terminal project.

## Final checks before Canvas

- [ ] Replace every pending team input and remove all draft labels.
- [ ] Verify roles against actual contributions; do not manufacture commits or alter authorship.
- [ ] Rebuild the PDF and confirm at most five pages and 12-point type throughout.
- [ ] Open both links in the PDF; confirm the repository is public and video accessible signed out.
- [ ] Watch the uploaded video and confirm duration is under five minutes and all essential features are shown.
- [ ] Place the final PDF in the repository and link it from README through a reviewed PR.
- [ ] Confirm the code builds using README commands and the real-data test runs rather than skips.
- [ ] Submit the public repository URL in Canvas before the deadline and check the submission receipt.
- [ ] Each teammate completes the separate peer feedback, including themselves.
- [ ] Submit the separate group feedback once for the group.

The current preparation does not submit anything to Canvas, record/upload a video, or complete feedback on anyone's behalf.
