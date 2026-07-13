#type: ignore
import sys
from pathlib import Path
from fastapi import FastAPI
from fastapi.responses import FileResponse
from fastapi.staticfiles import StaticFiles


sys.path.insert(0, str(Path(__file__).resolve().parent))
frontend_dir = Path(__file__).resolve().parent.parent / "src" / "frontend"

app = FastAPI(
    title="AIMagnetics Python API",
    version="0.1.0",
)
app.mount("/static", StaticFiles(directory=frontend_dir), name="static")


@app.get("/", response_class=FileResponse)
def read_index():
    return FileResponse(frontend_dir / "index.html")


from routes.core_selection import router as core_selection_router
app.include_router(core_selection_router)