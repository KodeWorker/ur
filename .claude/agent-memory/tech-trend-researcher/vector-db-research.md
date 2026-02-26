# Vector DB Research: Local-First Python Agent Memory (2026-02)

## Project: ur (local-first AI agent sandbox, Python 3.12+)

## Conclusion
Dual-layer architecture: SQLite (canonical, structured) + sqlite-vec (semantic recall).
ChromaDB remains correct as the [memory] extras choice for users who want richer embedding features.
LanceDB is the strongest alternative if ML/multimodal is ever needed.

## Candidate Status (Feb 2026)

| Tool          | Version  | Architecture     | ANN?              | Install cost       | Local-first? |
|---------------|----------|-----------------|-------------------|--------------------|--------------|
| sqlite-vec    | 0.1.x    | SQLite extension | No (brute-force)  | ~100KB C lib       | Yes          |
| ChromaDB      | 1.5.1    | Embedded/Server  | HNSW              | Heavy (~300MB w/ ST)| Yes (PersistentClient) |
| LanceDB       | 0.29.2   | Embedded (Rust)  | IVF+HNSW          | Medium (~50MB)     | Yes          |
| Qdrant        | 1.x      | Client-Server    | HNSW              | Requires daemon    | No           |

## Architecture Pattern for ur
```
SQLite (ur.db)
  sessions table      — structured session metadata, usage stats
  messages table      — ordered conversation turns (current session context)

sqlite-vec (same ur.db file via extension)
  vec_memories table  — semantic index over distilled facts/summaries

[memory] extras (optional)
  ChromaDB            — richer embedding features, custom models, collections
```

## When Vector Search Actually Helps Agents
1. Cross-session recall: "last time I worked on X..." — needs semantic search over historical sessions
2. Long-context compression: inject only the K most-relevant past turns rather than full history
3. Knowledge base grounding: search a document/codebase corpus beyond current session
4. Tool result deduplication: avoid re-fetching semantically identical web results
5. User preference recall: retrieve how user solved similar problems in previous sessions

## When Vector Search Does NOT Help
- Single-session conversations with small context windows (just pass all messages)
- Structured lookups by session ID, timestamp, status (SQLite wins here)
- Usage stats, token counts, session metadata (purely relational)

## sqlite-vec Key Facts
- Author: Alex Garcia (@asg017), same as sqlite-vss
- sqlite-vss is deprecated; sqlite-vec is the successor
- Pure C, no dependencies, installs via pip: `pip install sqlite-vec`
- Works with aiosqlite by loading the extension before queries
- Brute-force KNN only — fine for <500K vectors (>>enough for single-user agent)
- ANN (HNSW/IVF) is on the roadmap but not yet shipped as of Feb 2026
- Python usage: `sqlite_vec.load(conn)` then SQL: `SELECT * FROM vec_memories WHERE embedding MATCH ? ORDER BY distance LIMIT 5`

## ChromaDB Key Facts (1.5.x, Feb 2026)
- Rust-core rewrite in 2025, 4x faster writes/queries
- PersistentClient mode = fully embedded, zero infrastructure
- Default embedding: all-MiniLM-L6-v2 via sentence-transformers (auto-downloads ~90MB model)
- Total install with sentence-transformers pulls onnxruntime or PyTorch — adds 200-500MB
- Mindshare declining (14.4% -> 8.9% in vector DB category) but still dominant for Python prototypes
- Correct choice for [memory] extras: power users who want semantic search with custom models

## LanceDB Key Facts (0.29.x, Feb 2026)
- Pure Rust, serverless, columnar Lance format
- Releases every ~2 weeks, very active
- Best fit: ML pipelines, multimodal (images/audio), Arrow/pandas integration
- Overkill for text-only agent memory but excellent engineering quality
- Would be correct choice if ur ever needs multimodal memory
