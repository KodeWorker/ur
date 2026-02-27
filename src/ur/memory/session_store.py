from __future__ import annotations

import json
from datetime import UTC, datetime
from pathlib import Path
from typing import Any

import aiosqlite

from ..agent.models import Message
from ..agent.session import AgentSession
from .db import get_db


def _serialize_message(
    msg: Message,
) -> tuple[str, str | None, str | None, str | None]:
    content = json.dumps(msg.get("content"))
    tool_calls_raw = msg.get("tool_calls")
    tool_calls = json.dumps(tool_calls_raw) if tool_calls_raw is not None else None
    return content, tool_calls, msg.get("tool_call_id"), msg.get("name")


def _deserialize_message(
    row: aiosqlite.Row, *, keep_metadata: bool = False
) -> dict[str, Any]:
    d = dict(row)

    raw_content = d.pop("content", None)
    try:
        content = json.loads(raw_content) if raw_content is not None else None
    except (json.JSONDecodeError, TypeError):
        content = raw_content  # legacy plain-string row
    d["content"] = content

    raw_tool_calls = d.pop("tool_calls", None)
    if raw_tool_calls is not None:
        try:
            d["tool_calls"] = json.loads(raw_tool_calls)
        except (json.JSONDecodeError, TypeError) as exc:
            raise ValueError(
                f"Corrupt tool_calls JSON in message row: {raw_tool_calls!r}"
            ) from exc

    # Drop None-valued optional fields to keep clean OpenAI wire format
    for key in ("tool_call_id", "name"):
        if d.get(key) is None:
            d.pop(key, None)

    # Strip DB-internal metadata unless caller explicitly requests it
    if not keep_metadata:
        d.pop("created_at", None)

    return d


async def save_session(session: AgentSession, db_path: Path) -> None:
    async with get_db(db_path) as db:
        await db.execute(
            """
            INSERT INTO sessions
                (id, task, model, status, created_at,
                 total_input_tokens, total_output_tokens)
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
            content, tool_calls, tool_call_id, name = _serialize_message(msg)
            created_at = msg.get("created_at", datetime.now(UTC).isoformat())
            await db.execute(
                "INSERT INTO messages"
                " (session_id, role, content,"
                " tool_calls, tool_call_id, name, created_at)"
                " VALUES (?, ?, ?, ?, ?, ?, ?)",
                (
                    session.id,
                    msg["role"],
                    content,
                    tool_calls,
                    tool_call_id,
                    name,
                    created_at,
                ),
            )


async def list_sessions(db_path: Path, limit: int = 20) -> list[dict[str, Any]]:
    async with get_db(db_path) as db:
        cursor = await db.execute(
            "SELECT * FROM sessions ORDER BY created_at DESC, rowid DESC LIMIT ?",
            (limit,),
        )
        rows = await cursor.fetchall()
        return [dict(row) for row in rows]


async def get_session_messages(
    session_id: str, db_path: Path, *, with_metadata: bool = False
) -> list[dict[str, Any]]:
    async with get_db(db_path) as db:
        cursor = await db.execute(
            "SELECT role, content, tool_calls, tool_call_id, name, created_at"
            " FROM messages WHERE session_id = ? ORDER BY id",
            (session_id,),
        )
        rows = await cursor.fetchall()
        return [_deserialize_message(row, keep_metadata=with_metadata) for row in rows]
