# Memory Layer: SQLite vs Vector Database

**Date:** 2026-02-26
**Question:** Should `ur` switch from SQLite to a vector database for agent memory?

---

## Short Answer

**No — don't replace SQLite. Add vector search on top as an optional layer.**

---

## Why SQLite Stays

SQLite handles what it's good at: ordered message history, session metadata, structured queries (`session_id`, date ranges, usage stats). No vector DB replaces this.

## When Vector Search Actually Helps

1. **Cross-session recall** — "How did I solve X last week?" — semantic search over past sessions
2. **Long-context compression** — inject only the K most relevant past turns instead of full history (saves tokens)
3. **Knowledge base RAG** — index files/docs and retrieve relevant chunks at query time
4. **User preference memory** — "this project uses ruff, not black"

These answer fundamentally different questions than SQLite — they are not competing.

---

## Recommended Architecture

Two-layer memory:

| Layer | Tool | When |
|---|---|---|
| System of record | SQLite (existing) | Always |
| Semantic index | `sqlite-vec` (lightweight) | Core or near-zero-cost add |
| Power users | ChromaDB (already in `[memory]` extras) | Opt-in only |

### sqlite-vec

- 100 KB C extension — lives in the same `ur.db` file
- No new infrastructure; works with existing `aiosqlite`
- Fine for single-user use (<200K vectors); brute-force KNN is fast enough at that scale
- **Limitation:** no ANN index yet (tracking: [sqlite-vec #25](https://github.com/asg017/sqlite-vec/issues/25)) — revisit if corpus grows large

### ChromaDB

- Already correctly placed in `[memory]` extras
- 200–500 MB install (pulls `sentence-transformers`/PyTorch) — should stay opt-in, not in core
- Consider tightening the pin to `chromadb>=1.0` (stable Rust rewrite shipped 2025)

### LanceDB

- Technically excellent (Rust, serverless, bi-weekly releases)
- Best if `ur` ever grows toward multimodal memory (images, audio from tools)
- Overkill for a text-only agent at this stage

---

## Data Flow for Memory

```
Session completes
  → Extract key facts/summaries (LLM call or rule-based)
  → Embed them (local model or LLM embed API)
  → Write to vec_memories (sqlite-vec or Chroma)

New session starts
  → Embed the task
  → Query vec_memories for top-5 similar past facts
  → Inject as system prompt prefix: "From past sessions: ..."
  → Proceed with loop.run() unchanged
```

Injection point: `AgentSession.new()` — `loop.py` does not need to change.

### sqlite-vec schema sketch

```python
# In db.py — extended init_db
import sqlite_vec

async def init_db(db_path: Path) -> None:
    async with aiosqlite.connect(db_path) as db:
        await db.enable_load_extension(True)
        await db.load_extension(sqlite_vec.loadable_path())
        await db.executescript(SCHEMA + VEC_SCHEMA)
        await db.commit()

VEC_SCHEMA = """
CREATE VIRTUAL TABLE IF NOT EXISTS vec_memories USING vec0(
    session_id TEXT,
    embedding FLOAT[384]
);
"""
```

Query at session start:
```sql
SELECT session_id, distance
FROM vec_memories
WHERE embedding MATCH ?
ORDER BY distance
LIMIT 5;
```

### VectorStore protocol (for [memory] extras)

```python
# src/ur/memory/vector_store.py
from typing import Protocol

class VectorStore(Protocol):
    async def add(self, id: str, text: str, metadata: dict) -> None: ...
    async def search(self, query: str, k: int = 5) -> list[dict]: ...
```

Two implementations: `SqliteVecStore` (zero-dependency) and `ChromaStore` (opt-in).

---

## Risks and Watch-outs

- **sqlite-vec ANN gap:** Brute-force scan degrades at hundreds of thousands of entries. Monitor [issue #25](https://github.com/asg017/sqlite-vec/issues/25).
- **ChromaDB API stability:** Breaking changes between 0.x and 1.x (Rust rewrite). Tighten `pyproject.toml` pin to `chromadb>=1.0`.
- **Embedding model cold start:** sentence-transformers downloads ~90 MB on first use — document this clearly.
- **Extension loading on macOS:** `enable_load_extension(True)` is disabled by default in Homebrew Python. Needs testing and documentation.

---

## Decision

- **Keep SQLite** as the foundation (system of record)
- **Add `sqlite-vec`** for lightweight semantic memory with zero new infrastructure
- **Keep ChromaDB in `[memory]` extras** — correctly placed; tighten pin to `>=1.0`
- **Defer LanceDB** — revisit if multimodal memory becomes a requirement
