"""Unit tests for src/ur/tui.py — TurnWidget and UrApp lifecycle."""

from __future__ import annotations

import asyncio
from unittest.mock import AsyncMock, MagicMock, patch

import pytest

from tests.conftest import TEST_MODEL
from ur.agent.models import StreamChunk
from ur.agent.session import AgentSession
from ur.config import Settings
from ur.tui import TurnWidget, UrApp

# ── helpers ───────────────────────────────────────────────────────────────────


def _make_agent_stub(*chunks: StreamChunk):
    """Async-generator stub that yields the given chunks then stops."""

    async def _stub(*args, **kwargs):  # type: ignore[no-untyped-def]
        for chunk in chunks:
            yield chunk

    return _stub


def _make_agent_raising(exc: BaseException):
    """Async-generator stub that immediately raises *exc*."""

    async def _stub(*args, **kwargs):  # type: ignore[no-untyped-def]
        raise exc
        yield  # make Python treat this as an async generator  # noqa: RET508

    return _stub


def _make_urapp(
    tmp_settings: Settings,
    mode: str = "run",
    task: str = "test task",
) -> tuple[UrApp, AgentSession]:
    """Build an UrApp with a mocked LLMClient. Returns (app, session)."""
    session = AgentSession.new(task=task if mode == "run" else "", model=TEST_MODEL)
    client = MagicMock()
    client.provider = MagicMock()
    app = UrApp(
        mode=mode,  # type: ignore[arg-type]
        task=task if mode == "run" else None,
        settings=tmp_settings,
        model=TEST_MODEL,
        client=client,
        session=session,
        registry=None,
    )
    return app, session


# ── TurnWidget — tested inside a minimal scaffold app ────────────────────────
# We use a simple App with just a ScrollableContainer so that on_mount
# doesn't start any agent worker (avoiding the double-shutdown issue that
# occurs when UrApp in run-mode calls self.exit() inside run_test()).


class _ScaffoldApp(UrApp):
    """UrApp variant that suppresses the automatic agent turn in on_mount."""

    async def on_mount(self) -> None:
        # Skip UrApp's on_mount; just set metadata
        self.title = "test"


async def test_turn_widget_append_reasoning_updates_static(
    tmp_settings: Settings,
) -> None:
    """append_reasoning updates the Static inside the Collapsible."""
    from textual.widgets import Static

    session = AgentSession.new(task="t", model=TEST_MODEL)
    app = _ScaffoldApp(
        mode="run",
        task="t",
        settings=tmp_settings,
        model=TEST_MODEL,
        client=MagicMock(),
        session=session,
        registry=None,
    )
    with patch("ur.tui.save_session", new_callable=AsyncMock):
        async with app.run_test() as pilot:
            turn = TurnWidget("hello")
            await app.query_one("#scroll").mount(turn)
            await pilot.pause()

            await turn.append_reasoning("thinking hard")
            await pilot.pause()

            static = turn.query_one(".reasoning-text", Static)
            assert "thinking hard" in str(static._Static__content)


async def test_turn_widget_append_content_mounts_markdown(
    tmp_settings: Settings,
) -> None:
    """First append_content call mounts a Markdown widget."""
    from textual.widgets import Markdown

    session = AgentSession.new(task="t", model=TEST_MODEL)
    app = _ScaffoldApp(
        mode="run",
        task="t",
        settings=tmp_settings,
        model=TEST_MODEL,
        client=MagicMock(),
        session=session,
        registry=None,
    )
    with patch("ur.tui.save_session", new_callable=AsyncMock):
        async with app.run_test() as pilot:
            turn = TurnWidget("hello")
            await app.query_one("#scroll").mount(turn)
            await pilot.pause()

            assert len(turn.query(Markdown)) == 0
            await turn.append_content("# Hello")
            await pilot.pause()

            assert len(turn.query(Markdown)) == 1


async def test_turn_widget_tool_call_resets_content_segment(
    tmp_settings: Settings,
) -> None:
    """add_tool_call resets _active_content so next content gets a fresh Markdown."""
    from textual.widgets import Markdown

    session = AgentSession.new(task="t", model=TEST_MODEL)
    app = _ScaffoldApp(
        mode="run",
        task="t",
        settings=tmp_settings,
        model=TEST_MODEL,
        client=MagicMock(),
        session=session,
        registry=None,
    )
    with patch("ur.tui.save_session", new_callable=AsyncMock):
        async with app.run_test() as pilot:
            turn = TurnWidget("hello")
            await app.query_one("#scroll").mount(turn)
            await pilot.pause()

            # Run the three mounts as a separate task, mirroring how _stream_turn
            # works as a @work worker; pilot.pause() pumps the event loop between
            # each mount so Textual can process the DOM changes.
            completed: asyncio.Event = asyncio.Event()

            async def _sequence() -> None:
                await turn.append_content("before tool")
                await turn.add_tool_call("shell(ls)")
                await turn.append_content("after tool")
                completed.set()

            asyncio.ensure_future(_sequence())
            for _ in range(20):
                await pilot.pause()
                if completed.is_set():
                    break

            assert completed.is_set(), "mount sequence did not complete"
            # Two separate Markdown widgets: one before, one after the tool call
            assert len(turn.query(Markdown)) == 2


# ── ToolConfirmWidget ────────────────────────────────────────────────────────


@pytest.mark.parametrize(
    "input_value, expected_result",
    [
        ("yes", None),
        ("YES", None),
        ("", ""),
        ("not today", "not today"),
    ],
)
async def test_tool_confirm_widget_resolves_future(
    input_value: str,
    expected_result: str | None,
) -> None:
    """ToolConfirmWidget resolves its future with the correct value for each input.

    Tests the allow/deny logic in isolation by mocking DOM operations so the
    widget does not need to be mounted in a running app.
    """
    import asyncio

    from ur.tui import ToolConfirmWidget

    widget = ToolConfirmWidget("shell", '{"command": "ls"}')
    widget._future = asyncio.get_event_loop().create_future()

    mock_event = MagicMock()
    mock_event.stop = MagicMock()
    mock_event.value = input_value
    mock_event.input = MagicMock()
    mock_event.input.remove = AsyncMock()

    mock_prompt = MagicMock()
    mock_prompt.remove = AsyncMock()

    with (
        patch.object(widget, "query_one", return_value=mock_prompt),
        patch.object(widget, "mount", new_callable=AsyncMock),
    ):
        await widget.on_input_submitted(mock_event)

    assert widget._future.done()
    assert widget._future.result() == expected_result


# ── UrApp lifecycle — run mode ────────────────────────────────────────────────
# Use run_async(headless=True) instead of run_test() to avoid the
# double-shutdown error that occurs when the app calls self.exit() during a
# run_test() session (Textual cleans up _registry on exit(), and run_test()'s
# finally also calls _shutdown(), causing TypeError on the second pass).


async def test_urapp_run_mode_session_complete_on_success(
    tmp_settings: Settings,
) -> None:
    """session.complete() is called after successful streaming."""
    content = StreamChunk(kind="content", text="done")
    app, session = _make_urapp(tmp_settings, mode="run", task="task")

    with (
        patch("ur.tui.agent_run", _make_agent_stub(content)),
        patch("ur.tui.save_session", new_callable=AsyncMock),
    ):
        await asyncio.wait_for(app.run_async(headless=True), timeout=10.0)

    assert session.status == "completed"


async def test_urapp_run_mode_session_fail_on_exception(
    tmp_settings: Settings,
) -> None:
    """session.fail() is called when agent_run raises a regular Exception."""
    app, session = _make_urapp(tmp_settings, mode="run", task="task")

    with (
        patch("ur.tui.agent_run", _make_agent_raising(RuntimeError("boom"))),
        patch("ur.tui.save_session", new_callable=AsyncMock),
    ):
        await asyncio.wait_for(app.run_async(headless=True), timeout=10.0)

    assert session.status == "failed"


async def test_urapp_run_mode_save_session_always_called(
    tmp_settings: Settings,
) -> None:
    """save_session is called in the finally block on both success and failure."""
    for stub in [
        _make_agent_stub(StreamChunk(kind="content", text="ok")),
        _make_agent_raising(RuntimeError("fail")),
    ]:
        save_calls: list[object] = []

        async def _fake_save(session: object, db_path: object) -> None:
            save_calls.append(session)

        app, _ = _make_urapp(tmp_settings, mode="run", task="task")
        with (
            patch("ur.tui.agent_run", stub),
            patch("ur.tui.save_session", side_effect=_fake_save),
        ):
            await asyncio.wait_for(app.run_async(headless=True), timeout=10.0)

        assert len(save_calls) >= 1, "save_session must be called"


async def test_urapp_run_mode_session_interrupted_on_ctrl_c(
    tmp_settings: Settings,
) -> None:
    """Ctrl+C (action_request_quit) marks the session interrupted."""
    app, session = _make_urapp(tmp_settings, mode="run", task="task")

    async def _slow_stub(*args, **kwargs):  # type: ignore[no-untyped-def]
        await asyncio.sleep(10)
        yield StreamChunk(kind="content", text="never")

    async def autopilot(pilot: object) -> None:
        import asyncio as _asyncio

        await _asyncio.sleep(0.2)
        from textual.pilot import Pilot as _Pilot

        assert isinstance(pilot, _Pilot)
        await pilot.press("ctrl+c")

    with (
        patch("ur.tui.agent_run", _slow_stub),
        patch("ur.tui.save_session", new_callable=AsyncMock),
    ):
        await asyncio.wait_for(
            app.run_async(headless=True, auto_pilot=autopilot), timeout=10.0
        )

    assert session.status == "interrupted"


# ── UrApp lifecycle — chat mode ───────────────────────────────────────────────


async def test_urapp_chat_mode_ctrl_c_idle_completes_session(
    tmp_settings: Settings,
) -> None:
    """Ctrl+C in chat mode with no stream running marks session completed."""
    app, session = _make_urapp(tmp_settings, mode="chat")

    async def autopilot(pilot: object) -> None:
        import asyncio as _asyncio

        await _asyncio.sleep(0.2)
        from textual.pilot import Pilot as _Pilot

        assert isinstance(pilot, _Pilot)
        await pilot.press("ctrl+c")

    with patch("ur.tui.save_session", new_callable=AsyncMock):
        await asyncio.wait_for(
            app.run_async(headless=True, auto_pilot=autopilot), timeout=10.0
        )

    assert session.status == "completed"


async def test_urapp_chat_mode_turn_completes_successfully(
    tmp_settings: Settings,
) -> None:
    """Chat message is processed; session.complete() not called (stays running)."""
    content = StreamChunk(kind="content", text="response text")
    app, session = _make_urapp(tmp_settings, mode="chat")

    turn_processed: list[bool] = []

    async def _stub(*args, **kwargs):  # type: ignore[no-untyped-def]
        turn_processed.append(True)
        yield content

    async def autopilot(pilot: object) -> None:
        import asyncio as _asyncio

        await _asyncio.sleep(0.2)
        from textual.pilot import Pilot as _Pilot

        assert isinstance(pilot, _Pilot)
        # Type a message and submit
        await pilot.press("h", "i")
        await pilot.press("enter")
        # Wait for the worker to process the turn
        await _asyncio.sleep(1.0)
        # Exit
        await pilot.press("ctrl+c")

    with (
        patch("ur.tui.agent_run", _stub),
        patch("ur.tui.save_session", new_callable=AsyncMock),
    ):
        await asyncio.wait_for(
            app.run_async(headless=True, auto_pilot=autopilot), timeout=10.0
        )

    assert turn_processed, "agent_run was called at least once"
    # Quit after stream finished → completed (not interrupted)
    assert session.status == "completed"
