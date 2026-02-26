from __future__ import annotations

from datetime import datetime, timezone
from pathlib import Path
from typing import Any

from ..agent.session import AgentSession
from .db import get_db


async def save_session(session: AgentSession, db_path: Path) -> None:
    async with get_db(db_path) as db:
        await db.execute(
            """
            INSERT INTO sessions
                (id, task, model, status, created_at, total_input_tokens, total_output_tokens)
            VALUES (?, ?, ?, ?, ?, ?, ?)
            ON CONFLICT(id) DO UPDATE SET
                status               = excluded.status,
                total_input_tokens   = excluded.total_input_tokens,
                total_output_tokens  = excluded.total_output_tokens
            """,
            (
                session.id,
                session.task,
                session.model,
                session.status,
                session.created_at.isoformat(),
                session.usage.input_tokens,
                session.usage.output_tokens,
            ),
        )
        # Clear and re-insert messages so repeated saves stay idempotent
        await db.execute("DELETE FROM messages WHERE session_id = ?", (session.id,))
        for msg in session.messages:
            content = msg["content"] if isinstance(msg["content"], str) else str(msg["content"])
            # Use message's created_at if present, otherwise use current time
            created_at = msg.get("created_at", datetime.now(timezone.utc).isoformat())
            await db.execute(
                "INSERT INTO messages (session_id, role, content, created_at) VALUES (?, ?, ?, ?)",
                (session.id, msg["role"], content, created_at),
            )
        await db.commit()


async def list_sessions(db_path: Path, limit: int = 20) -> list[dict[str, Any]]:
    async with get_db(db_path) as db:
        cursor = await db.execute(
            "SELECT * FROM sessions ORDER BY created_at DESC, rowid DESC LIMIT ?",
            (limit,),
        )
        rows = await cursor.fetchall()
        return [dict(row) for row in rows]


async def get_session_messages(session_id: str, db_path: Path) -> list[dict[str, Any]]:
    async with get_db(db_path) as db:
        cursor = await db.execute(
            "SELECT role, content, created_at FROM messages WHERE session_id = ? ORDER BY id",
            (session_id,),
        )
        rows = await cursor.fetchall()
        return [dict(row) for row in rows]
