# Local Doc Assistant (RAG over docs/*.md)

A local, offline question-answering tool over this project's own
documentation — the first real piece of the "conversational design
review" / natural-language AI roadmap item (see the VP checkpoint deck).
Ask it things like *"why is core loss not evaluated for every
candidate?"* and it answers from the actual `docs/*.md` files, citing
which section it used, instead of guessing.

**This is local-machine tooling, not part of the deployed app.** It runs
against your own LM Studio server and your own Docker Chroma container —
nothing here touches Vercel, `python/app.py`, or the C++ engine. Everything
in `scripts/rag/` is maintenance/dev tooling, same category as
`scripts/export_real_data.py`.

---

## Architecture

```
docs/*.md  →  chunking.py (split on headings)  →  LM Studio embeddings
           →  ChromaDB (Docker, local)          →  stored chunks + vectors

your question  →  LM Studio embeddings  →  Chroma similarity search
              →  top-k doc excerpts  →  LM Studio chat model (grounded prompt)
              →  answer, cited to the doc section it came from
```

Two local services, both already visible in your setup:
- **LM Studio** (`http://127.0.0.1:1234`, OpenAI-compatible REST API) — serving
  `text-embedding-nomic-embed-text-v1.5` for embeddings and
  `google/gemma-4-12b-qat` for chat, both already loaded.
- **ChromaDB** (`chroma-core/chroma:latest`, Docker, port `8000:8000`) — vector
  storage. Your container (`engineeringai-chroma`) currently shows **Exited**
  in Docker Desktop — start it before running either script below.

**Where Obsidian fits:** Obsidian itself needs no plugin or integration
code here — it's just the editor you use to browse/edit the same markdown
files. Point `ingest.py --docs-dir` at whatever folder you actually edit in
Obsidian (your vault, or `docs/` directly if that's what you opened as the
vault) — whatever's in that folder is exactly what the assistant knows.
There's no separate "sync into Obsidian" step; the docs *are* the vault.

---

## Setup

```bash
# 1. Start the Chroma container (Docker Desktop, or:)
docker start engineeringai-chroma

# 2. In LM Studio: Developer -> Local Server -> Status: Running,
#    with text-embedding-nomic-embed-text-v1.5 and a chat model loaded
#    (both already shown as READY in your screenshot - nothing to change)

# 3. Install the two extra Python deps this tooling needs
#    (kept out of python/requirements.txt on purpose - Vercel never needs these)
pip install -r scripts/rag/requirements.txt
```

## Usage

```bash
# Index every docs/*.md file into Chroma (re-run any time docs change)
python scripts/rag/ingest.py

# Ask a one-off question
python scripts/rag/query.py "what happens if I don't supply rippleCurrentPeakToPeakA?"

# Or an interactive loop
python scripts/rag/query.py --interactive
```

To index your Obsidian vault instead of (or in addition to) `docs/`:
```bash
python scripts/rag/ingest.py --docs-dir "C:\path\to\your\vault"
```

---

## How hallucination is actually being controlled here

Two things, stacked:
1. **Retrieval** — the model only ever sees the 5 nearest chunks to your
   question (`query.py`'s `TOP_K`), not the whole doc set and not its own
   training data.
2. **A grounding instruction** (`query.py`'s `SYSTEM_PROMPT`) that tells the
   model to answer only from those chunks and say *"I don't have that in
   the project docs"* when they don't cover the question, instead of
   filling the gap with a plausible-sounding guess.

This is a strong bias, not a hard guarantee — a local model can still
ignore the instruction, especially a quantized 12B model under pressure
from an ambiguous question. Treat answers as a fast first draft to verify
against the cited section (`[source: docs/FILE.md > Heading > Path]`), not
as a substitute for reading it. If you want a harder guarantee later, the
next step up is constrained generation (forcing the model to only emit
spans copied from context) — out of scope for this first pass.

---

## What I could and couldn't verify

This was written and reviewed for correctness against the actual LM
Studio and Chroma REST APIs, but **not run end-to-end** — this remote
session has no network path to `127.0.0.1:1234` or `localhost:8000` on
your machine; both only exist on your local Windows box. Things worth
double-checking on your first real run:
- The exact embedding model name LM Studio expects in the API call
  (`text-embedding-nomic-embed-text-v1.5`, taken from your screenshot) —
  if LM Studio errors on the model field, check its exact string on the
  Local Server page.
- Chroma's Python client version compatibility with `chroma-core/chroma:latest`
  — `chromadb.HttpClient` is the current stable API, but Chroma has changed
  its server API across major versions before.

If either script errors, send me the traceback and I'll fix it from that,
same as any other bug.
