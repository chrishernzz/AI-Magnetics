#type: ignore
"""
ingest.py

Reads every docs/*.md file, splits it into heading-scoped chunks
(chunking.py), embeds each chunk with LM Studio's loaded embedding model
(nomic-embed-text-v1.5), and upserts them into a local ChromaDB collection
running in Docker.

This is a maintenance script, not part of the running AIMagnetics app -
same pattern as scripts/export_real_data.py. It only runs against your
local machine's LM Studio server and Docker Chroma container; nothing here
is deployed.

Usage:
    docker start <your chroma container>   # must be running first
    # LM Studio: Developer -> Local Server -> Status: Running, with both
    # text-embedding-nomic-embed-text-v1.5 and a chat model loaded
    pip install -r scripts/rag/requirements.txt
    python scripts/rag/ingest.py

Re-running this is safe - it re-upserts every chunk with a stable ID
(source file + heading path + index), so edited docs get their old chunk
replaced rather than duplicated. Point --docs-dir at your Obsidian vault
instead of docs/ if you keep a separate copy there; whichever folder you
point this at is exactly what the assistant will know about.
"""

from __future__ import annotations

import argparse
import hashlib
from pathlib import Path

import chromadb

from chunking import chunk_markdown_dir
from lm_studio_client import LMStudioClient

REPO_ROOT = Path(__file__).resolve().parents[2]
COLLECTION_NAME = "aimagnetics_docs"
BATCH_SIZE = 16


def chunk_id(chunk) -> str:
    raw = f"{chunk.source_file}::{chunk.heading_path}"
    return hashlib.sha256(raw.encode("utf-8")).hexdigest()[:16]


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--docs-dir", type=Path, default=REPO_ROOT / "docs",
                         help="Folder of .md files to index (default: docs/). "
                              "Point this at your Obsidian vault to index that instead.")
    parser.add_argument("--collection", default=COLLECTION_NAME,
                         help=f"Chroma collection to write into (default: {COLLECTION_NAME}). "
                              "Use a separate collection per corpus - e.g. 'magnetics_knowledge' "
                              "for the knowledge/ vault - so software docs and engineering "
                              "knowledge don't blend in retrieval.")
    parser.add_argument("--chroma-host", default="localhost")
    parser.add_argument("--chroma-port", type=int, default=8000)
    args = parser.parse_args()

    chunks = chunk_markdown_dir(args.docs_dir)
    if not chunks:
        raise SystemExit(f"No .md files found in {args.docs_dir} - nothing to ingest.")
    print(f"Found {len(chunks)} chunks across {len(sorted(args.docs_dir.glob('*.md')))} files in {args.docs_dir}")

    client = LMStudioClient()
    chroma = chromadb.HttpClient(host=args.chroma_host, port=args.chroma_port)
    collection = chroma.get_or_create_collection(args.collection)

    for start in range(0, len(chunks), BATCH_SIZE):
        batch = chunks[start:start + BATCH_SIZE]
        embeddings = client.embed_documents([c.text for c in batch])
        collection.upsert(
            ids=[chunk_id(c) for c in batch],
            embeddings=embeddings,
            documents=[c.text for c in batch],
            metadatas=[{"source_file": c.source_file, "heading_path": c.heading_path} for c in batch],
        )
        print(f"Upserted {start + len(batch)}/{len(chunks)}")

    print(f"Done. Collection '{args.collection}' now has {collection.count()} chunks.")


if __name__ == "__main__":
    main()
