"""
chunking.py

Splits a markdown file into retrieval-sized chunks along its own heading
structure, instead of a naive fixed-character split. This repo's docs are
already organized as short, self-contained sections under ##/### headings
(see docs/*.md), so splitting on headings keeps each chunk topically
coherent - a chunk boundary never lands mid-explanation.

Each chunk carries enough metadata (source file, heading path) to cite
where an answer came from, which is what query.py uses to ground its
answers instead of letting the model improvise.
"""

from __future__ import annotations

import re
from dataclasses import dataclass
from pathlib import Path

HEADING_RE = re.compile(r"^(#{1,6})\s+(.*)$")


@dataclass
class Chunk:
    text: str
    source_file: str
    heading_path: str


def chunk_markdown_file(path: Path) -> list[Chunk]:
    """
    Walks the file top to bottom, starting a new chunk at every heading.
    heading_path is the ">"-joined stack of headings above the chunk (e.g.
    "Formulas > 9. Core Loss > Units"), so a chunk about a subsection still
    carries its parent section's context for the embedding and the citation.
    """
    text = path.read_text(encoding="utf-8")
    lines = text.splitlines()

    chunks: list[Chunk] = []
    stack: list[tuple[int, str]] = []  # (heading level, heading text)
    current_lines: list[str] = []

    def flush():
        body = "\n".join(current_lines).strip()
        if body:
            heading_path = " > ".join(h for _, h in stack) or path.stem
            chunks.append(Chunk(text=body, source_file=str(path), heading_path=heading_path))

    for line in lines:
        match = HEADING_RE.match(line)
        if match:
            flush()
            current_lines = [line]
            level = len(match.group(1))
            heading_text = match.group(2).strip()
            while stack and stack[-1][0] >= level:
                stack.pop()
            stack.append((level, heading_text))
        else:
            current_lines.append(line)

    flush()
    return chunks


def chunk_markdown_dir(directory: Path) -> list[Chunk]:
    all_chunks: list[Chunk] = []
    for path in sorted(directory.glob("*.md")):
        all_chunks.extend(chunk_markdown_file(path))
    return all_chunks
