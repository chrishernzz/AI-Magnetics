#type: ignore
"""
parse_requirements.py

POST /parse-requirements - the natural-language front door. Takes the
engineer's free-text description, extracts a structured (partial)
InductorDesignRequest via the schema-constrained local LLM, and returns it
together with the deterministic validation verdict: blocking errors,
clarifying questions for missing fields (each with the reason it matters),
plausibility warnings, and labeled defaults.

This endpoint never runs the design engine. The frontend fills the normal
form with the extracted values so the engineer confirms (and completes)
the spec before clicking Generate - the human stays the trigger.

Requires LM Studio's local server (127.0.0.1:1234). Where that isn't
reachable (e.g. the Vercel deployment), returns 503 and the frontend
falls back to manual form entry.
"""
from fastapi import APIRouter, HTTPException
from pydantic import BaseModel

from services.requirement_parser import (
    LmStudioUnavailable,
    extract_requirements,
    validate_extraction,
)

router = APIRouter()


class ParseRequest(BaseModel):
    text: str


@router.post("/parse-requirements")
def parse_requirements(request: ParseRequest) -> dict:
    text = request.text.strip()
    if not text:
        raise HTTPException(status_code=422, detail="Describe the inductor you need first.")

    try:
        fields = extract_requirements(text)
    except LmStudioUnavailable as exc:
        raise HTTPException(status_code=503, detail=str(exc))
    except Exception as exc:  # LM Studio up but the call failed (bad model name, etc.)
        raise HTTPException(status_code=502, detail=f"Extraction failed: {exc}")

    verdict = validate_extraction(fields)
    return {"fields": fields, **verdict}
