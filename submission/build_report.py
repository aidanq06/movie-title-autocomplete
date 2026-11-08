#!/usr/bin/env python3
"""Render the editable draft to a five-page, 12-point PDF.

Requirements: reportlab, pymupdf (render/QA only).
Run: python3 submission/build_report.py
Outputs: output/pdf/movie_autocomplete_report_draft.pdf
         tmp/pdfs/report/page-N.png and qa.json
"""
from __future__ import annotations

import html
import json
import re
from pathlib import Path

import fitz
from reportlab.lib import colors
from reportlab.lib.enums import TA_CENTER, TA_LEFT, TA_RIGHT
from reportlab.lib.styles import ParagraphStyle
from reportlab.pdfbase import pdfmetrics
from reportlab.pdfbase.ttfonts import TTFont
from reportlab.platypus import (
    BaseDocTemplate, Frame, KeepTogether, PageBreak, PageTemplate,
    Paragraph, Spacer, Table, TableStyle,
)

ROOT = Path(__file__).resolve().parents[1]
SOURCE = ROOT / "submission" / "REPORT_DRAFT.md"
OUTPUT = ROOT / "output" / "pdf" / "movie_autocomplete_report_draft.pdf"
QA = ROOT / "tmp" / "pdfs" / "report"


def register_fonts():
    # Built-in Times is a portable fallback. Both options remain exactly 12 pt.
    font_root = Path("/System/Library/Fonts/Supplemental")
    candidates = {
        "ReportTimes": "Times New Roman.ttf",
        "ReportTimesBold": "Times New Roman Bold.ttf",
        "ReportTimesItalic": "Times New Roman Italic.ttf",
        "ReportTimesBoldItalic": "Times New Roman Bold Italic.ttf",
    }
    if all((font_root / name).exists() for name in candidates.values()):
        for family, name in candidates.items():
            pdfmetrics.registerFont(TTFont(family, str(font_root / name)))
        pdfmetrics.registerFontFamily("ReportTimes", normal="ReportTimes",
            bold="ReportTimesBold", italic="ReportTimesItalic", boldItalic="ReportTimesBoldItalic")
        return "ReportTimes", "ReportTimesBold"
    return "Times-Roman", "Times-Bold"


NORMAL, BOLD = register_fonts()
BODY = ParagraphStyle("Body", fontName=NORMAL, fontSize=12, leading=14,
    spaceAfter=6, textColor=colors.black, alignment=TA_LEFT)
HEADING = ParagraphStyle("Heading", parent=BODY, fontName=BOLD,
    spaceBefore=5, spaceAfter=5, keepWithNext=True)
TITLE = ParagraphStyle("Title", parent=HEADING, alignment=TA_CENTER,
    spaceBefore=0, spaceAfter=5)
CELL = ParagraphStyle("Cell", parent=BODY, leading=13, spaceAfter=0)
CELL_HEAD = ParagraphStyle("CellHead", parent=CELL, fontName=BOLD)
CELL_NUMBER = ParagraphStyle("CellNumber", parent=CELL, alignment=TA_RIGHT)


def inline(text):
    text = html.escape(text, quote=False)
    text = re.sub(r"\*\*(.+?)\*\*", r"<b>\1</b>", text)
    # URLs are readable as well as clickable; long ones may wrap naturally.
    text = re.sub(r"https?://[^\s<;]+", lambda m:
        '<link href="' + m.group(0).replace('"', '&quot;') + '">' + m.group(0) + '</link>', text)
    return text


def table(lines):
    rows = [[c.strip() for c in line.strip().strip("|").split("|")] for line in lines]
    rows = [row for row in rows if not all(re.fullmatch(r"[-: ]+", c) for c in row)]
    widths = [189, 279] if len(rows[0]) == 2 else [276, 96, 96]
    data = [[Paragraph(inline(value), CELL_HEAD if i == 0 else
                       CELL_NUMBER if len(row) == 3 and j > 0 else CELL)
             for j, value in enumerate(row)] for i, row in enumerate(rows)]
    result = Table(data, colWidths=widths, hAlign="LEFT", repeatRows=1)
    result.setStyle(TableStyle([
        ("BACKGROUND", (0, 0), (-1, 0), colors.HexColor("#EEEEEE")),
        ("GRID", (0, 0), (-1, -1), 0.5, colors.HexColor("#D9D9D9")),
        ("VALIGN", (0, 0), (-1, -1), "MIDDLE"),
        ("LEFTPADDING", (0, 0), (-1, -1), 6),
        ("RIGHTPADDING", (0, 0), (-1, -1), 6),
        ("TOPPADDING", (0, 0), (-1, -1), 5),
        ("BOTTOMPADDING", (0, 0), (-1, -1), 5),
    ]))
    return KeepTogether([result, Spacer(1, 6)])


def section_flowables(text):
    lines = text.strip().splitlines()
    result = []
    i = 0
    while i < len(lines):
        line = lines[i].strip()
        if not line:
            i += 1
        elif line.startswith("|"):
            end = i
            while end < len(lines) and lines[end].strip().startswith("|"):
                end += 1
            result.append(table(lines[i:end]))
            i = end
        elif line.startswith("# "):
            result.append(Paragraph(inline(line[2:]), TITLE))
            i += 1
        elif line.startswith("## "):
            result.append(Paragraph(inline(line[3:]), HEADING))
            i += 1
        else:
            end = i + 1
            while end < len(lines) and lines[end].strip() and not lines[end].startswith(("#", "|")):
                end += 1
            result.append(Paragraph(inline(" ".join(x.strip() for x in lines[i:end])), BODY))
            i = end
    return result


def footer(canvas, doc):
    canvas.saveState()
    canvas.setFont(NORMAL, 12)
    canvas.setFillColor(colors.black)
    canvas.drawString(72, 39, "DRAFT - team inputs pending")
    canvas.drawRightString(540, 39, str(doc.page))
    canvas.restoreState()


def build():
    OUTPUT.parent.mkdir(parents=True, exist_ok=True)
    QA.mkdir(parents=True, exist_ok=True)
    sections = SOURCE.read_text().split("<!-- PAGE BREAK -->")
    if len(sections) != 5:
        raise ValueError("The editable source must have exactly five sections/pages")
    doc = BaseDocTemplate(str(OUTPUT), pagesize=(612, 792),
        leftMargin=72, rightMargin=72, topMargin=54, bottomMargin=58,
        title="Movie Title Autocomplete report draft", author="Group 123")
    frame = Frame(72, 58, 468, 680, leftPadding=0, rightPadding=0,
        topPadding=0, bottomPadding=0)
    doc.addPageTemplates(PageTemplate(id="Report", frames=[frame], onPage=footer))
    story = []
    for i, section in enumerate(sections):
        if i:
            story.append(PageBreak())
        story.extend(section_flowables(section))
    doc.build(story)

    pdf = fitz.open(OUTPUT)
    sizes = set()
    pages = []
    for number, page in enumerate(pdf, 1):
        spans = [span for block in page.get_text("dict")["blocks"]
                 for line in block.get("lines", []) for span in line.get("spans", [])]
        sizes.update(round(span["size"], 3) for span in spans if span["text"].strip())
        page.get_pixmap(matrix=fitz.Matrix(1.5, 1.5), alpha=False).save(QA / f"page-{number}.png")
        pages.append({"page": number, "characters": len(page.get_text()),
                      "links": len(page.get_links())})
        for span in spans:
            if span["text"].strip() and (span["bbox"][0] < 71 or span["bbox"][2] > 541):
                raise RuntimeError(f"Text crosses horizontal margins on page {number}: {span['text']}")
    result = {"page_count": len(pdf), "font_sizes_pt": sorted(sizes), "pages": pages,
              "pdf": str(OUTPUT), "status": "draft", "visual_review": "required"}
    (QA / "qa.json").write_text(json.dumps(result, indent=2))
    print(json.dumps(result, indent=2))
    if len(pdf) != 5:
        raise RuntimeError(f"Expected 5 pages, got {len(pdf)}; reduce content, never shrink below 12 pt")
    if sizes != {12.0}:
        raise RuntimeError(f"Expected only 12-point text, got {sizes}")


if __name__ == "__main__":
    build()
