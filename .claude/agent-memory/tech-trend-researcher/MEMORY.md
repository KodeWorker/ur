# Tech Trend Researcher Agent Memory

## Evaluations Log

### Vector DB for local-first Python agent (ur project) — 2026-02
**Conclusion:** Keep SQLite as canonical store; add sqlite-vec for semantic search; defer chromadb/lancedb to [memory] extras.
- sqlite-vec: brute-force only (no ANN yet), pure C, zero config, ideal for <500K vectors
- ChromaDB 1.5.x: Rust core rewrite, 4x perf gain, PersistentClient embedded mode, large install (~sentence-transformers pulls onnxruntime/PyTorch)
- LanceDB 0.29.x: Rust, serverless, columnar Lance format, best for ML/multimodal workloads; heavier than sqlite-vec
- Qdrant: requires server process — ruled out for local-first single-user tools
- Key insight: SQLite and vector search are complementary, not competing. SQLite handles structured session data; vector layer handles semantic recall across sessions.
- See: vector-db-research.md for full analysis

## Useful Sources by Domain

### Vector Databases
- Benchmark data: https://thedataquarry.com/blog/vector-db-1/
- ChromaDB changelog: https://www.trychroma.com/changelog
- LanceDB releases: https://github.com/lancedb/lancedb/releases
- sqlite-vec ANN tracking: https://github.com/asg017/sqlite-vec/issues/25

## Technologies Ruled Out (and Why)
- **Qdrant** for local-first single-user: requires separate server process, overkill for developer sandbox
- **pgvector**: requires PostgreSQL, violates local-first/zero-infra constraint
- **sqlite-vss**: deprecated in favor of sqlite-vec (same author)
- **Milvus/Weaviate/Pinecone**: cloud/server-oriented, not appropriate for local-first tools
