# Submission preparation

This folder prepares the report and submission requirements for COP3530 Project 2b. It does not change the C++ program.

- `REPORT_DRAFT.md`: editable report source, divided into five planned pages.
- `build_report.py`: PDF generator with page-count, 12-point-font, and horizontal-margin checks.
- `SUBMISSION_CHECKLIST.md`: detailed rubric coverage, pending team inputs, and Canvas/feedback steps.
- Generated report: `../output/pdf/movie_autocomplete_report_draft.pdf`.

The report is a **draft**, not ready to submit. Proposal comparison, personal reflection, any unconfirmed noncoding contributions, and the actual YouTube URL need completion. Recording is deferred for now. Keep every role and experience truthful.

## Rebuild the PDF

The authoring helper uses Python with `reportlab` and `pymupdf`. These are report-generation tools, not C++ application dependencies. Install them in a local virtual environment, then run from the repository root:

```sh
python3 submission/build_report.py
```

The generator creates the PDF under `output/pdf/` and page previews plus a machine-check report under ignored `tmp/pdfs/report/`. It uses 12-point Times New Roman on macOS when available, or built-in Times as a portable fallback. All headings, table cells, and page numbers remain 12 point. Visually inspect all five page previews after editing; successful text checks alone do not establish a clean layout.

The explicit page dividers in the Markdown keep the current draft to five pages. Adding text can overflow a page, so rerun the helper and shorten prose or reorganize content rather than shrinking type. References and headings count toward the five-page limit.

The Windows measurement table reproduces the run documented in `docs/ANALYSIS_NOTES.md`. The added November 5 macOS checks validate correctness on a separate dataset download and do not replace or mix with those timings.

Once the missing content is complete, change the draft wording and filenames, rerender and inspect, then link the final PDF and YouTube recording from the root README through a reviewed PR. Submit the public repository URL in Canvas and finish the separate peer/group feedback deliverables.
