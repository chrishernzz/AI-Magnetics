# Magnetics Knowledge Vault

This folder is the **retrieval knowledge base** for the AIMagnetics assistant —
open it as an Obsidian vault (`Open folder as vault` → this `knowledge/` folder)
and treat it as the single source of truth the local LLM answers from. It is
deliberately separate from `docs/` (which documents how the *software* works);
this vault holds the *magnetics engineering* knowledge itself.

## How this vault becomes the assistant's brain

```
you edit notes here (Obsidian)
        ↓ (just files on disk - Obsidian talks to nothing)
python scripts/rag/ingest.py --docs-dir knowledge --collection magnetics_knowledge
        ↓ (chunks each note by heading, embeds via LM Studio)
ChromaDB stores the chunks
        ↓
python scripts/rag/query.py --collection magnetics_knowledge "your question"
```

The assistant can only cite what is in these notes. **Adding a note and
re-running ingest is how the system "learns."** Nothing updates by itself.

## Rules for adding notes (these keep the assistant honest)

1. **Only verified information.** A wrong note becomes a confidently-cited
   wrong answer. If you're not sure, don't write it — or mark it clearly:
   `UNVERIFIED:` at the start of the line.
2. **Use headings.** The ingest pipeline splits notes at `##`/`###` boundaries,
   so each section should stand alone as a retrievable fact.
3. **State units explicitly, every time.** Units mistakes are the #1 real bug
   class this project has hit (see [[units-and-pitfalls]]).
4. **Say where a fact came from** when it's from a datasheet, McLyman, or a
   measurement — future-you will need to know whether to trust it.
5. **One topic per note.** Retrieval works best when a chunk is about one thing.
