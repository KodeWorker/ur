"""Textual TUI for ur — run and chat commands."""

from __future__ import annotations

import asyncio
import logging
from pathlib import Path
from typing import Literal

from rich.markup import escape
from textual import work
from textual.app import App, ComposeResult
from textual.containers import Horizontal, ScrollableContainer, Vertical
from textual.widgets import (
    Button,
    Collapsible,
    Footer,
    Header,
    Input,
    Markdown,
    SelectionList,
    Static,
)
from textual.widgets.selection_list import Selection
from textual.worker import Worker

from .agent.loop import run as agent_run
from .agent.session import AgentSession
from .config import Settings
from .llm.client import LLMClient, Provider
from .memory.db import init_db
from .memory.session_store import save_session
from .tools.registry import ToolRegistry

logger = logging.getLogger(__name__)

Mode = Literal["run", "chat"]


# ── TurnWidget ────────────────────────────────────────────────────────────────


class TurnWidget(Vertical):
    """One user → agent conversational turn.

    The user label is composed statically. All other children — reasoning
    collapsibles, content Markdowns, tool-call/result statics — are mounted
    dynamically as chunks arrive from the streaming worker.

    Each reasoning segment (before the first tool call, between tool calls,
    after the last tool call) gets its own collapsed Collapsible so the user
    can expand each one independently.
    """

    DEFAULT_CSS = """
    TurnWidget {
        height: auto;
        margin-bottom: 1;
        padding: 0 1;
    }
    TurnWidget > Static.user-label {
        text-style: bold;
        color: $accent;
        height: auto;
    }
    TurnWidget .reasoning-text {
        height: auto;
        color: $text-disabled;
    }
    TurnWidget Markdown {
        height: auto;
        margin: 0;
        padding: 0;
    }
    TurnWidget > Collapsible.tool-call {
        height: auto;
        background: $panel;
    }
    TurnWidget Collapsible.tool-call CollapsibleTitle {
        color: $warning;
    }
    TurnWidget Collapsible.tool-call Static.tool-result-text {
        color: $text-disabled;
    }
    TurnWidget > Static.error {
        height: auto;
        color: $error;
    }
    """

    def __init__(self, user_text: str) -> None:
        super().__init__()
        self._user_text = user_text
        self._active_content: Markdown | None = None
        self._active_reasoning: Static | None = None
        self._active_tool_result: Static | None = None

    def compose(self) -> ComposeResult:
        yield Static(
            f"[bold blue]you[/bold blue] › {escape(self._user_text)}",
            markup=True,
            classes="user-label",
        )

    # ── streaming update methods ───────────────────────────────────────────────

    async def append_reasoning(self, text: str) -> None:
        """Append to (or start) the current reasoning Collapsible.

        A new Collapsible is mounted on the first call per reasoning segment.
        Subsequent calls update its Static in-place with the accumulated text.
        Call reset_reasoning() to close the segment; the next call will open
        a fresh Collapsible, giving each segment its own toggle.
        """
        if self._active_reasoning is None:
            static = Static("", classes="reasoning-text")
            await self.mount(Collapsible(static, title="reasoning", collapsed=True))
            self._active_reasoning = static
        self._active_reasoning.update(text)

    def reset_reasoning(self) -> None:
        """Close the current reasoning segment.

        The next append_reasoning call will mount a new Collapsible so that
        reasoning before/between/after tool calls each gets its own toggle.
        """
        self._active_reasoning = None

    async def append_content(self, text: str) -> None:
        """Append to the current content Markdown segment.

        Mounts a new Markdown on the first call per content segment
        (i.e. after widget creation or after add_tool_call resets
        _active_content). The caller passes the fully accumulated
        text for the current segment.
        """
        if self._active_content is None:
            md = Markdown(text)
            self._active_content = md
            await self.mount(md)
        else:
            self._active_content.update(text)

    async def add_tool_call(self, text: str) -> None:
        """Mount a Collapsible for this tool round-trip and reset the content segment.

        The Collapsible title shows the call; add_tool_result fills its body.
        Resetting _active_content means the next append_content call mounts a
        fresh Markdown below this widget, preserving the correct visual order:
        content → tool collapsible → new content.
        """
        self._active_content = None
        result_static = Static("", classes="tool-result-text")
        self._active_tool_result = result_static
        await self.mount(
            Collapsible(
                result_static,
                title=f"⚙ {text}",
                collapsed=True,
                classes="tool-call",
            )
        )

    async def add_tool_result(self, text: str) -> None:
        """Fill the body of the current tool Collapsible with the result."""
        if self._active_tool_result is not None:
            self._active_tool_result.update(text)
            self._active_tool_result = None

    async def add_error(self, text: str) -> None:
        """Append a red error line and reset the content segment."""
        self._active_content = None
        await self.mount(
            Static(f"[red]Error:[/red] {escape(text)}", markup=True, classes="error")
        )


# ── ToolConfirmWidget ─────────────────────────────────────────────────────────


class ToolConfirmWidget(Vertical):
    """Inline tool-call confirmation rendered as a chat message in the scroll pane.

    The worker awaits wait_for_response(); the Input submission resolves it.

    Returns:
      None      — allowed (only "yes" or "YES" exactly)
      ""        — denied with no reason; loop.py produces "Tool call denied by user."
      "<text>"  — denied with reason; loop.py builds
                  f"Tool call denied by user with {text}"
    """

    DEFAULT_CSS = """
    ToolConfirmWidget {
        height: auto;
        margin-bottom: 1;
        padding: 0 1;
        border-left: thick $warning;
    }
    ToolConfirmWidget .confirm-name {
        text-style: bold;
        color: $warning;
        height: auto;
    }
    ToolConfirmWidget .confirm-args {
        color: $text-disabled;
        height: auto;
    }
    ToolConfirmWidget .confirm-prompt {
        color: $text-disabled;
        height: auto;
        margin-top: 1;
    }
    ToolConfirmWidget .confirm-result {
        height: auto;
        margin-top: 1;
    }
    """

    def __init__(self, name: str, args: str) -> None:
        super().__init__()
        self._tool_name = name
        self._tool_args = args
        self._future: asyncio.Future[str | None] | None = None

    def compose(self) -> ComposeResult:
        yield Static(f"⚙ {self._tool_name}", classes="confirm-name", markup=False)
        if self._tool_args and self._tool_args != "{}":
            preview = self._tool_args[:300]
            if len(self._tool_args) > 300:
                preview += "…"
            yield Static(preview, classes="confirm-args", markup=False)
        yield Static(
            "Allow? [dim]yes/YES to allow — ↵ or type a reason + ↵ to deny[/dim]",
            classes="confirm-prompt",
        )
        yield Input(placeholder="reason for denial (↵ to deny)…", id="confirm-input")

    def on_mount(self) -> None:
        self._future = asyncio.get_running_loop().create_future()
        self.query_one("#confirm-input", Input).focus()

    async def on_input_submitted(self, event: Input.Submitted) -> None:
        event.stop()  # prevent bubbling to UrApp.on_input_submitted
        reason = event.value.strip()
        allowed = reason in ("yes", "YES")  # strict match only; anything else is denied
        result: str | None = None if allowed else reason

        await self.query_one(".confirm-prompt").remove()
        await event.input.remove()
        label = (
            "[green]✓ 🤖 allowed[/green]"
            if allowed
            else f"[red]✗ 🤖 denied[/red]: {escape(reason)}"
            if reason
            else "[red]✗ 🤖 denied[/red]"
        )
        await self.mount(Static(label, classes="confirm-result"))

        if self._future is not None and not self._future.done():
            self._future.set_result(result)

    async def wait_for_response(self) -> str | None:
        if self._future is None:
            raise RuntimeError("wait_for_response called before on_mount")
        return await self._future


# ── ToolSettingsWidget ────────────────────────────────────────────────────────


class ToolSettingsWidget(Vertical):
    """Inline tool-settings panel rendered as a chat message in the scroll pane.

    Shows a checklist of all registered tools with their current enabled state.
    Keyboard: Space to toggle, Tab to reach Confirm/Cancel, Enter/Space to activate,
    Escape to cancel from anywhere in the widget.

    Note: the ctrl+enter binding requires kitty keyboard protocol support and will
    not fire in many terminal emulators (tmux, xterm, SSH). Use Tab to reach the
    Confirm button and press Enter/Space as a fully portable alternative.
    """

    BINDINGS = [
        ("escape", "cancel", "Cancel"),
        ("ctrl+enter", "confirm", "Confirm"),
    ]

    DEFAULT_CSS = """
    ToolSettingsWidget {
        height: auto;
        margin-bottom: 1;
        padding: 0 1;
        border-left: thick $accent;
    }
    ToolSettingsWidget .settings-title {
        text-style: bold;
        color: $accent;
        height: auto;
        margin-bottom: 1;
    }
    ToolSettingsWidget SelectionList {
        height: auto;
        max-height: 12;
        border: none;
        padding: 0;
    }
    ToolSettingsWidget .settings-buttons {
        height: 3;
        margin-top: 1;
    }
    ToolSettingsWidget Button {
        margin-right: 1;
    }
    """

    def __init__(self, registry: ToolRegistry) -> None:
        super().__init__()
        self._registry = registry
        self._future: asyncio.Future[None] | None = None

    def compose(self) -> ComposeResult:
        yield Static("configure tools", classes="settings-title", markup=False)
        selections = [
            Selection(name, name, self._registry.is_enabled(name))
            for name in self._registry.names()
        ]
        yield SelectionList(*selections, id="tool-checklist")
        with Horizontal(classes="settings-buttons"):
            yield Button("Confirm", variant="primary", id="btn-confirm")
            yield Button("Cancel", variant="default", id="btn-cancel")

    def on_mount(self) -> None:
        self._future = asyncio.get_running_loop().create_future()
        self.query_one("#tool-checklist", SelectionList).focus()

    def _apply(self) -> None:
        selected: set[str] = set(
            self.query_one("#tool-checklist", SelectionList).selected
        )
        for name in self._registry.names():
            if name in selected:
                self._registry.enable(name)
            else:
                self._registry.disable(name)

    def _close(self) -> None:
        if self._future is not None and not self._future.done():
            self._future.set_result(None)

    def on_button_pressed(self, event: Button.Pressed) -> None:
        if event.button.id == "btn-confirm":
            self._apply()
        self._close()

    def action_confirm(self) -> None:
        self._apply()
        self._close()

    def action_cancel(self) -> None:
        self._close()

    async def wait_for_close(self) -> None:
        if self._future is None:
            raise RuntimeError("wait_for_close called before on_mount")
        await self._future


# ── UrApp ─────────────────────────────────────────────────────────────────────


class UrApp(App[None]):
    """Dual-mode Textual application for `ur run` and `ur chat`."""

    CSS = """
    Screen {
        layout: vertical;
    }
    #scroll {
        height: 1fr;
        padding: 1 2;
    }
    #input-bar {
        height: 3;
    }
    #status {
        height: 1;
        padding: 0 1;
        color: $text-disabled;
    }
    """

    BINDINGS = [
        ("ctrl+c", "request_quit", "Quit"),
        ("ctrl+t", "tool_settings", "Tools"),
        ("ctrl+x", "cancel_stream", "Stop"),
    ]

    def __init__(
        self,
        mode: Mode,
        task: str | None,
        settings: Settings,
        model: str,
        client: LLMClient,
        session: AgentSession,
        registry: ToolRegistry | None,
    ) -> None:
        super().__init__()
        self._mode = mode
        self._run_task = task
        self._settings = settings
        self._model = model
        self._client = client
        self._session = session
        self._tool_registry = registry
        self._current_turn: TurnWidget | None = None
        self._settings_open = False

    def compose(self) -> ComposeResult:
        yield Header(show_clock=False)
        yield ScrollableContainer(id="scroll")
        if self._mode == "chat":
            yield Input(placeholder="Type a message and press Enter…", id="input-bar")
        yield Static("", id="status")
        yield Footer()

    async def on_mount(self) -> None:
        self.title = "ur"
        self.sub_title = f"{self._model}  {self._session.id[:8]}"
        if self._mode == "run":
            if self._run_task is None:
                raise RuntimeError("run mode requires a task string")
            await self._start_turn(self._run_task)
        else:
            self.query_one("#input-bar", Input).focus()

    async def on_input_submitted(self, event: Input.Submitted) -> None:
        text = event.value.strip()
        if not text:
            return
        input_widget = self.query_one("#input-bar", Input)
        input_widget.clear()
        input_widget.disabled = True
        self._session.add_user_message(text)
        await self._start_turn(text)

    async def _start_turn(self, user_text: str) -> None:
        scroll = self.query_one("#scroll", ScrollableContainer)
        turn = TurnWidget(user_text)
        self._current_turn = turn
        await scroll.mount(turn)
        scroll.scroll_end(animate=False)
        self._stream_turn(turn)

    @work(name="stream_turn", exclusive=True)
    async def _stream_turn(self, turn: TurnWidget) -> None:
        """Stream agent chunks into *turn*, updating the TUI as each arrives.

        Session lifecycle:
          - Normal:         session.complete()
          - Exception:      session.fail()   (error shown; post-turn actions still run)
          - BaseException:  session.interrupt() then re-raise (skips post-turn actions)

        save_session and status-bar update happen in the finally block so they
        always execute regardless of which path exits the try/except.
        """
        scroll = self.query_one("#scroll", ScrollableContainer)
        reasoning_acc = ""
        content_acc = ""

        async def _confirm_tool(name: str, args: str) -> str | None:
            # Mount inside the current turn so the confirm widget appears
            # above the tool-call and result lines, not below them.
            widget = ToolConfirmWidget(name, args)
            await turn.mount(widget)
            scroll.scroll_end(animate=False)
            result = await widget.wait_for_response()
            scroll.scroll_end(animate=False)
            return result

        try:
            async for chunk in agent_run(
                self._session,
                self._client,
                self._settings.max_iterations,
                registry=self._tool_registry,
                confirm_tool=_confirm_tool if self._tool_registry is not None else None,
            ):
                if chunk.kind == "reasoning":
                    reasoning_acc += chunk.text
                    await turn.append_reasoning(reasoning_acc)
                elif chunk.kind == "content":
                    if reasoning_acc:
                        turn.reset_reasoning()
                    reasoning_acc = ""
                    content_acc += chunk.text
                    await turn.append_content(content_acc)
                    self.call_after_refresh(scroll.scroll_end, animate=False)
                elif chunk.kind == "tool_call":
                    turn.reset_reasoning()
                    content_acc = ""
                    reasoning_acc = ""
                    await turn.add_tool_call(chunk.text)
                    self.call_after_refresh(scroll.scroll_end, animate=False)
                elif chunk.kind == "tool_result":
                    await turn.add_tool_result(chunk.text)
            if self._mode == "run":
                self._session.complete()
        except Exception as e:
            self._session.fail()
            await turn.add_error(str(e))
            self._show_provider_hint(e)
        except BaseException:
            self._session.interrupt()
            raise
        finally:
            await save_session(self._session, self._settings.db_path)
            self._update_status()
            if self._mode == "chat":
                input_widget = self.query_one("#input-bar", Input)
                input_widget.disabled = False
                input_widget.focus()
            elif self._mode == "run":
                self.call_after_refresh(self.exit)

    def _show_provider_hint(self, e: Exception) -> None:
        """Write a provider-specific hint to the status bar."""
        e_lower = str(e).lower()
        if self._client.provider == Provider.GEMINI and (
            "auth" in e_lower or "api_key" in e_lower
        ):
            hint = "Set GEMINI_API_KEY in your environment or .env file."
        elif self._client.provider == Provider.OLLAMA:
            hint = "Set UR_OLLAMA_BASE_URL in your environment or .env file."
        else:
            hint = "Set the correct environment variables for the provider."
        self.query_one("#status", Static).update(hint)

    def _update_status(self) -> None:
        """Write token usage to the status bar."""
        usage = self._session.usage
        self.query_one("#status", Static).update(
            f"tokens in={usage.input_tokens} out={usage.output_tokens}"
        )

    def on_worker_state_changed(self, event: Worker.StateChanged) -> None:
        self.refresh_bindings()

    def check_action(
        self, action: str, parameters: tuple[object, ...]
    ) -> bool | None:
        streaming = any(
            w.name == "stream_turn" and w.is_running for w in self.workers
        )
        if action == "tool_settings":
            if self._tool_registry is None or self._settings_open or streaming:
                return False
        if action == "cancel_stream" and not streaming:
            return False
        return True

    def action_cancel_stream(self) -> None:
        """Ctrl+X: cancel the running agent stream."""
        for w in self.workers:
            if w.name == "stream_turn" and w.is_running:
                w.cancel()
                break

    def action_tool_settings(self) -> None:
        """Ctrl+T: open the tool enable/disable panel (blocked while streaming)."""
        if self._tool_registry is None or self._settings_open:
            return
        if any(w.is_running for w in self.workers):
            return
        self._settings_open = True
        self._run_tool_settings()

    @work
    async def _run_tool_settings(self) -> None:
        """Worker: mount the settings panel, await close, then restore input."""
        assert self._tool_registry is not None
        if self._mode == "chat":
            self.query_one("#input-bar", Input).disabled = True

        scroll = self.query_one("#scroll", ScrollableContainer)
        widget = ToolSettingsWidget(self._tool_registry)
        await scroll.mount(widget)
        scroll.scroll_end(animate=False)

        try:
            await widget.wait_for_close()
        finally:
            await widget.remove()
            self._settings_open = False
            if self._mode == "chat":
                input_widget = self.query_one("#input-bar", Input)
                input_widget.disabled = False
                input_widget.focus()

    async def action_quit(self) -> None:
        """Textual's built-in quit (command palette, default binding) — delegate."""
        await self.action_request_quit()

    async def action_request_quit(self) -> None:
        """Ctrl+C / Ctrl+D: finalise session status, then exit.

        - streaming in progress → interrupted  (user cut it short)
        - idle when quit        → completed    (conversation ended normally)
        """
        if self._session.status == "running":
            streaming = any(w.is_running for w in self.workers)
            if streaming:
                self._session.interrupt()
            else:
                self._session.complete()
            await save_session(self._session, self._settings.db_path)
        self.exit()


# ── registry helper ───────────────────────────────────────────────────────────


def _make_registry(
    no_tools: bool, settings: Settings, workspace_dir: Path | None = None
) -> ToolRegistry | None:
    if no_tools:
        return None
    try:
        from .tools.builtin import create_default_registry

        registry = create_default_registry(
            truncate_at=settings.tool_builtin_truncate_at,
            max_lines=settings.tool_builtin_max_lines,
            max_search_results=settings.tool_builtin_max_search_results,
            shell_timeout=settings.tool_builtin_shell_timeout,
            http_timeout=settings.tool_builtin_http_timeout,
            browser_timeout=settings.tool_builtin_browser_timeout,
            workspace_dir=workspace_dir,
        )
    except ImportError:
        logger.warning(
            "Built-in tools are unavailable — install the [tools] extra: "
            "pip install 'ur[tools]'"
        )
        registry = ToolRegistry()

    from .tools.plugins import load_plugins

    load_plugins(registry, settings.tools_dir)
    return registry


# ── public entry points ───────────────────────────────────────────────────────


async def launch_run(
    task: str,
    settings: Settings,
    model_override: str | None = None,
    no_tools: bool = False,
) -> None:
    """Entry point for `ur run`. Raises SystemExit(1) on agent failure."""
    await init_db(settings.db_path)
    model = model_override or settings.model
    client = LLMClient(settings, model=model)
    session = AgentSession.new(task=task, model=model)
    workspace_dir = settings.workspaces_dir / session.id
    workspace_dir.mkdir(parents=True, exist_ok=True)
    registry = _make_registry(no_tools, settings, workspace_dir=workspace_dir)

    app = UrApp(
        mode="run",
        task=task,
        settings=settings,
        model=model,
        client=client,
        session=session,
        registry=registry,
    )
    await app.run_async()

    if session.status == "failed":
        raise SystemExit(1)


async def launch_chat(
    settings: Settings,
    model_override: str | None = None,
    no_tools: bool = False,
) -> None:
    """Entry point for `ur chat`."""
    await init_db(settings.db_path)
    model = model_override or settings.model
    client = LLMClient(settings, model=model)
    session = AgentSession.new(task="", model=model)
    # Workspace is created lazily by the file tools on first write; chat sessions
    # that never touch the filesystem produce no directory.
    workspace_dir = settings.workspaces_dir / session.id
    registry = _make_registry(no_tools, settings, workspace_dir=workspace_dir)

    app = UrApp(
        mode="chat",
        task=None,
        settings=settings,
        model=model,
        client=client,
        session=session,
        registry=registry,
    )
    await app.run_async()
