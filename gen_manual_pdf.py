#!/usr/bin/env python3
"""Convert manual_claude.md to ClockBox_v3_Manual.pdf using markdown + weasyprint."""

import os, markdown
from weasyprint import HTML, CSS

BASE = os.path.dirname(os.path.abspath(__file__))
MD_FILE  = os.path.join(BASE, "manual_claude.md")
PDF_FILE = os.path.join(BASE, "ClockBox_v3_Manual.pdf")

# ── Read & convert markdown ──────────────────────────────────────────────────
with open(MD_FILE, encoding="utf-8") as f:
    md_text = f.read()

body_html = markdown.markdown(
    md_text,
    extensions=["tables", "fenced_code", "nl2br"],
)

# ── Full HTML document ────────────────────────────────────────────────────────
html_doc = f"""<!DOCTYPE html>
<html lang="en">
<head>
<meta charset="utf-8">
<title>ClockBox v3 Manual</title>
</head>
<body>
{body_html}
</body>
</html>
"""

# ── CSS ───────────────────────────────────────────────────────────────────────
css = CSS(string="""
@page {
    size: A4;
    margin: 20mm 22mm 22mm 22mm;
    @bottom-center {
        content: counter(page) " / " counter(pages);
        font-family: Arial, sans-serif;
        font-size: 9pt;
        color: #888;
    }
}

body {
    font-family: Arial, Helvetica, sans-serif;
    font-size: 10.5pt;
    line-height: 1.55;
    color: #1a1a1a;
}

h1 {
    font-size: 22pt;
    margin-top: 0;
    margin-bottom: 6pt;
    border-bottom: 2px solid #222;
    padding-bottom: 4pt;
}

h2 {
    font-size: 14pt;
    margin-top: 18pt;
    margin-bottom: 4pt;
    border-bottom: 1px solid #ccc;
    padding-bottom: 2pt;
}

h3 {
    font-size: 11.5pt;
    margin-top: 12pt;
    margin-bottom: 3pt;
}

p  { margin: 4pt 0 6pt 0; }

table {
    border-collapse: collapse;
    width: 100%;
    margin: 8pt 0;
    font-size: 10pt;
}

th, td {
    border: 1px solid #bbb;
    padding: 4pt 8pt;
    text-align: left;
}

th {
    background: #f0f0f0;
    font-weight: bold;
}

tr:nth-child(even) td {
    background: #f9f9f9;
}

code {
    font-family: "Courier New", monospace;
    font-size: 9.5pt;
    background: #f4f4f4;
    padding: 0 3pt;
    border-radius: 2pt;
}

pre {
    background: #f4f4f4;
    padding: 8pt;
    border-radius: 3pt;
    font-size: 9pt;
    overflow-wrap: break-word;
}

img {
    max-width: 100%;
    height: auto;
}

hr {
    border: none;
    border-top: 1px solid #ddd;
    margin: 12pt 0;
}

ul, ol {
    margin: 4pt 0 6pt 0;
    padding-left: 20pt;
}

li { margin: 2pt 0; }

strong { font-weight: bold; }
em     { font-style: italic; }
""", base_url=BASE)

# ── Render ────────────────────────────────────────────────────────────────────
print("Rendering PDF...")
HTML(string=html_doc, base_url=BASE).write_pdf(PDF_FILE, stylesheets=[css])
print(f"Saved: {PDF_FILE}")
