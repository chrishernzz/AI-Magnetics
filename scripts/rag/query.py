"""
query.py

Answers a question grounded in the docs chunks ingest.py already put into
Chroma: embed the question, retrieve the top-k nearest chunks, and hand
only those chunks (plus the question) to the local chat model - it is
explicitly instructed not to answer from anything outside them. This is
the actual anti-hallucination mechanism here: the model is never asked to
recall facts from its own training, only to read and summarize the
retrieved excerpts, and to say so plainly when the excerpts don't cover
the question.

That instruction is a strong bias, not a guarantee - a local model can
still ignore it. Treat answers as a fast first draft to verify against the
cited doc section, not a substitute for reading it.

Usage:
    python scripts/rag/query.py "why is core loss not evaluated for every candidate?"
    python scripts/rag/query.py --interactive
"""

from __future__ import annotations

import argparse

import chromadb

from lm_studio_client import LMStudioClient

COLLECTION_NAME = "aimagnetics_docs"
TOP_K = 5

SYSTEM_PROMPT = """You are a documentation assistant for the AIMagnetics inductor design engine.

Answer ONLY using the excerpts provided below - never from general knowledge, and never by guessing.
If the excerpts don't contain the answer, say exactly: "I don't have that in the project docs." Do not
speculate past what the excerpts state. When you do answer, cite which excerpt(s) you used by their
[source] tag."""


def format_context(results) -> str:
    blocks = []
    for doc, meta in zip(results["documents"][0], results["metadatas"][0]):
        source = f"{meta['source_file']} > {meta['heading_path']}"
        blocks.append(f"[source: {source}]\n{doc}")
    return "\n\n---\n\n".join(blocks)


def answer(question: str, client: LMStudioClient, collection, top_k: int = TOP_K) -> str:
    query_embedding = client.embed_query(question)
    results = collection.query(query_embeddings=[query_embedding], n_results=top_k)

    if not results["documents"][0]:
        return "No indexed docs yet - run scripts/rag/ingest.py first."

    context = format_context(results)
    user_prompt = f"Excerpts:\n\n{context}\n\n---\n\nQuestion: {question}"
    return client.chat(SYSTEM_PROMPT, user_prompt)


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("question", nargs="?", help="Question to ask. Omit with --interactive for a loop.")
    parser.add_argument("--interactive", action="store_true")
    parser.add_argument("--top-k", type=int, default=TOP_K)
    parser.add_argument("--chroma-host", default="localhost")
    parser.add_argument("--chroma-port", type=int, default=8000)
    parser.add_argument("--chat-model", default=None,
                         help="LM Studio chat model identifier to use (default: the one in lm_studio_client.py). "
                              "Use this to switch to a smaller/faster model without editing source.")
    parser.add_argument("--timeout", type=int, default=None,
                         help="Seconds to wait for LM Studio per request (default 900 - big models on CPU are slow).")
    args = parser.parse_args()

    if not args.question and not args.interactive:
        parser.error("Provide a question, or pass --interactive")

    client_kwargs = {}
    if args.chat_model:
        client_kwargs["chat_model"] = args.chat_model
    if args.timeout:
        client_kwargs["timeout_seconds"] = args.timeout
    client = LMStudioClient(**client_kwargs)
    chroma = chromadb.HttpClient(host=args.chroma_host, port=args.chroma_port)
    try:
        collection = chroma.get_collection(COLLECTION_NAME)
    except Exception as exc:
        raise SystemExit(
            f"Collection '{COLLECTION_NAME}' not found - run scripts/rag/ingest.py first. ({exc})"
        ) from exc

    if args.interactive:
        print("AIMagnetics doc assistant. Ctrl+C to exit.")
        while True:
            try:
                question = input("\n> ")
            except (KeyboardInterrupt, EOFError):
                break
            if not question.strip():
                continue
            print(answer(question, client, collection, args.top_k))
    else:
        print(answer(args.question, client, collection, args.top_k))


if __name__ == "__main__":
    main()
