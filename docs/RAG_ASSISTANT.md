# Local RAG Assistant (docs/*.md and knowledge/*.md)

A local, offline question-answering tool — the first real piece of the
"conversational design review" / natural-language AI roadmap item (see the
VP checkpoint deck). The same `scripts/rag/` tooling can index and answer
from **either of two separate corpora**, kept in separate Chroma
collections so they never blend:

- **`docs/*.md`** (default collection `aimagnetics_docs`) — questions about
  *this software*: why a field reports `not_evaluated`, how ranking works,
  what the API returns. Ask it *"why is core loss not evaluated for every
  candidate?"* and it cites the actual doc section.
- **`knowledge/*.md`** (collection `magnetics_knowledge`, see below) —
  general power-magnetics engineering knowledge: what an inductor is, why
  ripple current matters, core materials, Steinmetz core loss. This is the
  corpus meant to ground natural-language design conversations, and the one
  worth growing as an Obsidian vault.

**This is local-machine tooling, not part of the deployed app.** It runs
against your own LM Studio server and your own Docker Chroma container —
nothing here touches Vercel, `python/app.py`, or the C++ engine. Everything
in `scripts/rag/` is maintenance/dev tooling, same category as
`scripts/export_real_data.py`.

---

## Architecture

```
a folder of *.md (docs/ or knowledge/)  →  chunking.py (split on headings)
                                         →  LM Studio embeddings
                                         →  ChromaDB (Docker, local)
                                         →  stored chunks + vectors, one collection per corpus

your question  →  LM Studio embeddings  →  Chroma similarity search (that collection)
              →  top-k excerpts  →  LM Studio chat model (grounded prompt)
              →  answer, cited to the file/section it came from
```

Two local services:
- **LM Studio** (`http://127.0.0.1:1234`, OpenAI-compatible REST API) — an
  embedding model (e.g. `nomic-embed-text-v1.5`) for indexing/retrieval, and
  a chat model for answering. Both must be loaded and the Local Server
  running (Developer tab → Local Server → Status: Running).
- **ChromaDB** (`chroma-core/chroma:latest`, Docker, port `8000:8000`) —
  vector storage. Start your container before running either script below
  (`docker start <container name>`, or via Docker Desktop) — a stopped
  container is the most common reason `ingest.py`/`query.py` fail to connect.

**Where Obsidian fits:** Obsidian itself needs no plugin or integration
code here — it's just the editor you use to browse/edit the same markdown
files. Point `ingest.py --docs-dir` at whatever folder you actually opened
as an Obsidian vault (`knowledge/` is the intended one — see below) —
whatever's in that folder is exactly what the assistant knows. There's no
separate "sync into Obsidian" step; the folder *is* the vault.

---

## Setup

```bash
# 1. Start your Chroma Docker container (Docker Desktop, or:)
docker start <your-chroma-container-name>

# 2. In LM Studio: Developer -> Local Server -> Status: Running,
#    with an embedding model and a chat model both loaded

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

## The knowledge/ vault (the real retrieval corpus)

`knowledge/` at the repo root is a curated **magnetics engineering knowledge
base** — 18 notes covering inductor fundamentals, peak/RMS/ripple current,
core materials, air gaps, area-product sizing, turns/AL, winding/AWG, copper
loss, Steinmetz core loss, skin/proximity, thermal, units pitfalls, and an
input-interview guide for the future natural-language-input feature. Open it
as an Obsidian vault (`Open folder as vault` → `knowledge/`) and grow it there
— it, not `docs/`, is what the conversational assistant should answer from.
(`docs/` documents the *software*; `knowledge/` holds the *engineering*.)

Keep the two corpora in separate Chroma collections so they don't blend:

```bash
# index the engineering knowledge vault
python scripts/rag/ingest.py --docs-dir knowledge --collection magnetics_knowledge

# ask it questions
python scripts/rag/query.py --interactive --collection magnetics_knowledge
```

"Learning" = adding/editing a note in Obsidian, then re-running the ingest
command. Nothing updates itself; every fact the assistant can cite is a note
you can open, verify, and delete. Rules for adding notes are in
`knowledge/00_START_HERE.md` — the important one: a wrong note becomes a
confidently-cited wrong answer, so only verified information goes in.

## Performance: pick a model your hardware can actually run

First real end-to-end run (2026-07-21): ingest worked (186 chunks), retrieval
worked, but the answer timed out — LM Studio's log showed Gemma 12B QAT
processing the ~1k-token RAG prompt at **1.4–4.9 tokens/sec**, i.e. running
almost entirely on CPU. The client's timeout has since been raised to 900s so
slow answers finish instead of being cancelled mid-generation, but multi-minute
answers make interactive Q&A miserable. The real fix is a smaller chat model:
a ~3–4B instruct model (e.g. `gemma-3-4b`, `llama-3.2-3b-instruct`) answers
doc questions like these fine and runs several times faster; raising GPU
Offload in LM Studio (if VRAM allows) helps either way. Switch without editing
source:

```bash
python scripts/rag/query.py --interactive --chat-model "your-model-id-here"
```

The embedding model (nomic, 84 MB) is not the bottleneck — embeddings came
back instantly; only chat generation is slow.

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

## Verification status

Confirmed working end-to-end on real hardware: `ingest.py` indexed 186
`docs/*.md` chunks and 94 `knowledge/*.md` chunks into separate Chroma
collections, retrieval returned the correct excerpts, and `query.py`
produced grounded, cited answers for both corpora — including correctly
*refusing* to answer a question outside the active collection instead of
guessing (see "How hallucination is actually being controlled here" above).
The embedding call, the chat call, and the schema/collection-separation
logic have all been exercised against a real LM Studio server and a real
Chroma container, not just reviewed against the API docs.

If a script errors on your machine, the most likely causes, in order: the
Chroma container isn't running (`docker start ...`), LM Studio's Local
Server isn't running or the wrong model name is configured, or the model
you're using needs more `--timeout`/is too large for interactive use (see
"Performance" above). Send the traceback and it can be fixed like any
other bug from there.
