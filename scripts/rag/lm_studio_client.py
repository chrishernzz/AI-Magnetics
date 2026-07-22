"""
lm_studio_client.py

Thin wrapper over LM Studio's local REST server (OpenAI-compatible), so
ingest.py and query.py don't each hand-roll the same two HTTP calls. No
official OpenAI SDK dependency - LM Studio's server is simple enough that
raw requests is easier to debug when something's misconfigured (wrong
model name, server not running, etc.) than an SDK's own error wrapping.

Nomic's nomic-embed-text-v1.5 model is trained with asymmetric task
prefixes: document text should be embedded with a "search_document: "
prefix, queries with "search_query: " - using the same prefix for both
measurably hurts retrieval quality. See
https://huggingface.co/nomic-ai/nomic-embed-text-v1.5 - this is why
embed_documents/embed_query are separate functions instead of one.
"""

from __future__ import annotations

import requests

DEFAULT_BASE_URL = "http://127.0.0.1:1234/v1"


class LMStudioClient:
    def __init__(self, base_url: str = DEFAULT_BASE_URL, embedding_model: str = "text-embedding-nomic-embed-text-v1.5",
                 chat_model: str = "google/gemma-4-12b-qat"):
        self.base_url = base_url.rstrip("/")
        self.embedding_model = embedding_model
        self.chat_model = chat_model

    def _post(self, path: str, payload: dict) -> dict:
        try:
            response = requests.post(f"{self.base_url}{path}", json=payload, timeout=120)
        except requests.exceptions.ConnectionError as exc:
            raise RuntimeError(
                f"Could not reach LM Studio at {self.base_url}{path}. Is the local server running "
                f"(LM Studio -> Developer -> Local Server -> Status: Running) and is a model loaded?"
            ) from exc
        response.raise_for_status()
        return response.json()

    def _embed(self, texts: list[str]) -> list[list[float]]:
        body = self._post("/embeddings", {"model": self.embedding_model, "input": texts})
        return [row["embedding"] for row in body["data"]]

    def embed_documents(self, texts: list[str]) -> list[list[float]]:
        return self._embed([f"search_document: {t}" for t in texts])

    def embed_query(self, text: str) -> list[float]:
        return self._embed([f"search_query: {text}"])[0]

    def chat(self, system_prompt: str, user_prompt: str, temperature: float = 0.1) -> str:
        body = self._post(
            "/chat/completions",
            {
                "model": self.chat_model,
                "messages": [
                    {"role": "system", "content": system_prompt},
                    {"role": "user", "content": user_prompt},
                ],
                "temperature": temperature,
            },
        )
        return body["choices"][0]["message"]["content"]
