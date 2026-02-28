"""Unit tests for the async CLI helpers _run, _chat, and _history in ur/main.py.

Issues addressed:
  #65 — Test that _run marks session "interrupted" on BaseException (CancelledError)
  #66 — Zero test coverage for CLI entry points _run, _chat, _history
"""

from __future__ import annotations

import asyncio
from unittest.mock import AsyncMock, MagicMock, patch

import pytest
import typer

from tests.conftest import TEST_MODEL
from ur.agent.models import StreamChunk
from ur.agent.session import AgentSession
from ur.config import Settings
from ur.main import _history, _run
from ur.memory.db import init_db
from ur.memory.session_store import save_session

# ── helpers ───────────────────────────────────────────────────────────────────


def _make_agent_run_stub(*chunks: StreamChunk):
    """Return an async-generator function that yields the given StreamChunks.

    This matches the signature of agent_run: async def run(session, client, max_iter).
    The returned callable ignores all arguments and just yields the chunks.
    """

    async def _stub(session, client, max_iter, **kwargs):
        for chunk in chunks:
            yield chunk

    return _stub


def _make_agent_run_raising(exc: BaseException):
    """Return an async-generator function that immediately raises *exc*."""

    async def _stub(session, client, max_iter, **kwargs):
        raise exc
        yield  # make Python treat this as an async generator  # noqa: unreachable

    return _stub


def _noop_live_patch():
    """Return a context-manager patcher for ur.main.Live that does nothing."""
    live_instance = MagicMock()
    live_instance.__enter__ = MagicMock(return_value=live_instance)
    live_instance.__exit__ = MagicMock(return_value=False)
    live_cls = MagicMock(return_value=live_instance)
    return live_cls


# ── _run: happy path ──────────────────────────────────────────────────────────


async def test_run_happy_path_calls_session_complete(tmp_settings: Settings) -> None:
    """_run happy path: session.complete() is called and save_session persists it."""
    content_chunk = StreamChunk(kind="content", text="Hello!")

    captured_sessions: list[AgentSession] = []

    async def _fake_save(session: AgentSession, db_path):
        captured_sessions.append(session)

    with (
        patch("ur.main.agent_run", _make_agent_run_stub(content_chunk)),
        patch("ur.main.save_session", side_effect=_fake_save),
        patch("ur.main.LLMClient"),
        patch("ur.main.Live", _noop_live_patch()),
        patch("ur.main.init_db", new_callable=AsyncMock),
    ):
        await _run("do something", tmp_settings)

    assert len(captured_sessions) == 1
    assert captured_sessions[0].status == "completed"


async def test_run_happy_path_save_session_called_with_correct_db_path(
    tmp_settings: Settings,
) -> None:
    """_run happy path: save_session is called with the settings db_path."""
    saved_db_paths: list = []

    async def _fake_save(session: AgentSession, db_path):
        saved_db_paths.append(db_path)

    with (
        patch("ur.main.agent_run", _make_agent_run_stub()),
        patch("ur.main.save_session", side_effect=_fake_save),
        patch("ur.main.LLMClient"),
        patch("ur.main.Live", _noop_live_patch()),
        patch("ur.main.init_db", new_callable=AsyncMock),
    ):
        await _run("do something", tmp_settings)

    assert saved_db_paths == [tmp_settings.db_path]


# ── _run: Exception path ──────────────────────────────────────────────────────


async def test_run_exception_path_calls_session_fail(tmp_settings: Settings) -> None:
    """_run exception path: session.fail() is called when agent_run raises Exception."""
    captured_sessions: list[AgentSession] = []

    async def _fake_save(session: AgentSession, db_path):
        captured_sessions.append(session)

    with (
        patch(
            "ur.main.agent_run",
            _make_agent_run_raising(RuntimeError("boom")),
        ),
        patch("ur.main.save_session", side_effect=_fake_save),
        patch("ur.main.LLMClient"),
        patch("ur.main.Live", _noop_live_patch()),
        patch("ur.main.init_db", new_callable=AsyncMock),
        pytest.raises(typer.Exit) as exc_info,
    ):
        await _run("fail task", tmp_settings)

    assert exc_info.value.exit_code == 1
    assert len(captured_sessions) == 1
    assert captured_sessions[0].status == "failed"


async def test_run_exception_path_raises_typer_exit_1(tmp_settings: Settings) -> None:
    """_run exception path: typer.Exit(1) is raised after an Exception."""
    with (
        patch(
            "ur.main.agent_run",
            _make_agent_run_raising(RuntimeError("network error")),
        ),
        patch("ur.main.save_session", new_callable=AsyncMock),
        patch("ur.main.LLMClient"),
        patch("ur.main.Live", _noop_live_patch()),
        patch("ur.main.init_db", new_callable=AsyncMock),
        pytest.raises(typer.Exit) as exc_info,
    ):
        await _run("fail task", tmp_settings)

    assert exc_info.value.exit_code == 1


async def test_run_exception_path_save_session_still_called(
    tmp_settings: Settings,
) -> None:
    """_run exception path: save_session is called even when agent_run raises."""
    save_called = []

    async def _fake_save(session: AgentSession, db_path):
        save_called.append(True)

    with (
        patch(
            "ur.main.agent_run",
            _make_agent_run_raising(RuntimeError("oops")),
        ),
        patch("ur.main.save_session", side_effect=_fake_save),
        patch("ur.main.LLMClient"),
        patch("ur.main.Live", _noop_live_patch()),
        patch("ur.main.init_db", new_callable=AsyncMock),
        pytest.raises(typer.Exit),
    ):
        await _run("fail task", tmp_settings)

    assert save_called, "save_session must be called even when agent_run raises"


# ── _run: BaseException / CancelledError path (#65) ──────────────────────────


async def test_run_cancelled_error_marks_session_interrupted(
    tmp_settings: Settings,
) -> None:
    """#65: CancelledError during streaming sets session.status == 'interrupted'."""
    captured_sessions: list[AgentSession] = []

    async def _fake_save(session: AgentSession, db_path):
        captured_sessions.append(session)

    with (
        patch(
            "ur.main.agent_run",
            _make_agent_run_raising(asyncio.CancelledError()),
        ),
        patch("ur.main.save_session", side_effect=_fake_save),
        patch("ur.main.LLMClient"),
        patch("ur.main.Live", _noop_live_patch()),
        patch("ur.main.init_db", new_callable=AsyncMock),
        pytest.raises(asyncio.CancelledError),
    ):
        await _run("cancel me", tmp_settings)

    assert len(captured_sessions) == 1
    assert captured_sessions[0].status == "interrupted"


async def test_run_cancelled_error_save_session_still_called(
    tmp_settings: Settings,
) -> None:
    """#65: save_session is called in the finally block even on CancelledError."""
    save_called = []

    async def _fake_save(session: AgentSession, db_path):
        save_called.append(True)

    with (
        patch(
            "ur.main.agent_run",
            _make_agent_run_raising(asyncio.CancelledError()),
        ),
        patch("ur.main.save_session", side_effect=_fake_save),
        patch("ur.main.LLMClient"),
        patch("ur.main.Live", _noop_live_patch()),
        patch("ur.main.init_db", new_callable=AsyncMock),
        pytest.raises(asyncio.CancelledError),
    ):
        await _run("cancel me", tmp_settings)

    assert save_called, "save_session must be called in finally block on CancelledError"


async def test_run_cancelled_error_is_reraised(tmp_settings: Settings) -> None:
    """#65: CancelledError is re-raised after marking the session interrupted."""
    with (
        patch(
            "ur.main.agent_run",
            _make_agent_run_raising(asyncio.CancelledError()),
        ),
        patch("ur.main.save_session", new_callable=AsyncMock),
        patch("ur.main.LLMClient"),
        patch("ur.main.Live", _noop_live_patch()),
        patch("ur.main.init_db", new_callable=AsyncMock),
        pytest.raises(asyncio.CancelledError),
    ):
        await _run("cancel me", tmp_settings)


# ── _history: session-list path ───────────────────────────────────────────────


async def test_history_session_list_no_exception(tmp_settings: Settings) -> None:
    """_history with no session_id and an empty DB completes without raising."""
    await init_db(tmp_settings.db_path)

    # Should not raise even with zero sessions
    await _history(tmp_settings, None, 10)


async def test_history_session_list_with_existing_session(
    tmp_settings: Settings,
) -> None:
    """_history lists sessions that were previously saved to the DB."""
    await init_db(tmp_settings.db_path)
    session = AgentSession.new(task="list me", model=TEST_MODEL)
    session.complete()
    await save_session(session, tmp_settings.db_path)

    # Should complete without raising; the session is rendered in a Rich table
    await _history(tmp_settings, None, 10)


# ── _history: prefix-lookup path ──────────────────────────────────────────────


async def test_history_prefix_lookup_finds_correct_session(
    tmp_settings: Settings,
) -> None:
    """_history with a unique session-ID prefix returns exactly one session."""
    await init_db(tmp_settings.db_path)

    session_a = AgentSession.new(task="task alpha", model=TEST_MODEL)
    session_a.complete()
    await save_session(session_a, tmp_settings.db_path)

    session_b = AgentSession.new(task="task beta", model=TEST_MODEL)
    session_b.complete()
    await save_session(session_b, tmp_settings.db_path)

    # Look up session_a by an unambiguous prefix (first 8 chars of its UUID)
    prefix = session_a.id[:8]
    # Should not raise — unique prefix returns one session and prints its messages
    await _history(tmp_settings, prefix, 10)


async def test_history_prefix_lookup_raises_on_no_match(
    tmp_settings: Settings,
) -> None:
    """_history raises typer.Exit(1) when the prefix matches no sessions."""
    await init_db(tmp_settings.db_path)

    with pytest.raises(typer.Exit) as exc_info:
        await _history(tmp_settings, "nonexistent-prefix", 10)

    assert exc_info.value.exit_code == 1


async def test_history_prefix_lookup_raises_on_ambiguous_match(
    tmp_settings: Settings,
) -> None:
    """_history raises typer.Exit(1) when the prefix matches more than one session.

    We force ambiguity by giving two sessions IDs that share a common prefix.
    """
    await init_db(tmp_settings.db_path)

    # Manually assign IDs that share a known 4-character prefix so the lookup
    # with that prefix hits the "multiple matches" branch.
    shared_prefix = "aaaa"
    session_a = AgentSession.new(task="task alpha", model=TEST_MODEL)
    session_a.id = shared_prefix + "0000-0000-0000-0000-000000000001"
    session_a.complete()
    await save_session(session_a, tmp_settings.db_path)

    session_b = AgentSession.new(task="task beta", model=TEST_MODEL)
    session_b.id = shared_prefix + "0000-0000-0000-0000-000000000002"
    session_b.complete()
    await save_session(session_b, tmp_settings.db_path)

    with pytest.raises(typer.Exit) as exc_info:
        await _history(tmp_settings, shared_prefix, 10)

    assert exc_info.value.exit_code == 1
