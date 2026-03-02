"""Unit tests for the CLI helpers in ur/main.py.

Covers _history (unchanged) and smoke-tests that the run/chat typer
commands delegate to tui.launch_run / tui.launch_chat.
"""

from __future__ import annotations

import logging.handlers
from unittest.mock import AsyncMock, patch

import pytest
import typer

from tests.conftest import TEST_MODEL
from ur.agent.session import AgentSession
from ur.config import Settings
from ur.main import _configure_logging, _history
from ur.memory.db import init_db
from ur.memory.session_store import save_session

# ── _configure_logging ────────────────────────────────────────────────────────


def test_configure_logging_creates_log_file(tmp_settings: Settings) -> None:
    """_configure_logging attaches a RotatingFileHandler writing to logs_dir."""
    import logging

    tmp_settings.ensure_dirs()
    root = logging.getLogger()
    before = list(root.handlers)
    new_handlers: list[logging.Handler] = []
    try:
        _configure_logging(tmp_settings)
        new_handlers = [h for h in root.handlers if h not in before]
        assert any(
            isinstance(h, logging.handlers.RotatingFileHandler) for h in new_handlers
        )
        log_file = tmp_settings.logs_dir / "ur.log"
        logging.getLogger("ur.test").warning("ping")
        assert log_file.exists()
    finally:
        for h in new_handlers:
            h.close()
            root.removeHandler(h)


def test_configure_logging_respects_log_level(tmp_settings: Settings) -> None:
    """Handler level is set from settings.log_level."""
    import logging

    tmp_settings.log_level = "DEBUG"
    tmp_settings.ensure_dirs()
    root = logging.getLogger()
    before = list(root.handlers)
    new_handlers: list[logging.Handler] = []
    try:
        _configure_logging(tmp_settings)
        new_handlers = [h for h in root.handlers if h not in before]
        fh = next(
            h
            for h in new_handlers
            if isinstance(h, logging.handlers.RotatingFileHandler)
        )
        assert fh.level == logging.DEBUG
    finally:
        for h in new_handlers:
            h.close()
            root.removeHandler(h)


# ── run / chat command delegation ─────────────────────────────────────────────


def test_run_command_delegates_to_launch_run(tmp_settings: Settings) -> None:
    """The `run` typer command calls tui.launch_run with the correct arguments."""
    from typer.testing import CliRunner

    from ur.main import app

    runner = CliRunner()
    with patch("ur.tui.launch_run", new_callable=AsyncMock) as mock_launch:
        mock_launch.return_value = None
        with patch("ur.main._settings", return_value=tmp_settings):
            result = runner.invoke(app, ["run", "do something"])

    mock_launch.assert_called_once()
    call_args = mock_launch.call_args
    assert call_args.args[0] == "do something"
    assert result.exit_code == 0


def test_chat_command_delegates_to_launch_chat(tmp_settings: Settings) -> None:
    """The `chat` typer command calls tui.launch_chat with the correct arguments."""
    from typer.testing import CliRunner

    from ur.main import app

    runner = CliRunner()
    with patch("ur.tui.launch_chat", new_callable=AsyncMock) as mock_launch:
        mock_launch.return_value = None
        with patch("ur.main._settings", return_value=tmp_settings):
            result = runner.invoke(app, ["chat"])

    mock_launch.assert_called_once()
    assert result.exit_code == 0


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
