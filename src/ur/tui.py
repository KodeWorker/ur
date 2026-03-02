"""Textual TUI for ur — run and chat commands."""

from __future__ import annotations

import asyncio
from typing import Literal

from textual import work
from textual.app import App, ComposeResult
from textual.containers import ScrollableContainer, Vertical
from textual.widgets import Collapsible, Footer, Header, Input, Markdown, Static

from .agent.loop import run as agent_run
from .agent.session import AgentSession
from .config import Settings
from .llm.client import LLMClient, Provider
from .memory.db import init_db
from .memory.session_store import save_session
from .tools.registry import ToolRegistry

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
    TurnWidget > Static.tool-call,
    TurnWidget > Static.tool-result {
        height: auto;
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

    def compose(self) -> ComposeResult:
        yield Static(
            f"[bold blue]you[/bold blue] › {self._user_text}",
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
        """Append a dim tool-call line and reset the content segment.

        Resetting _active_content means the next append_content call
        will mount a fresh Markdown below this Static, preserving the
        correct visual order: content → tool-call → new content.
        """
        self._active_content = None
        await self.mount(
            Static(f"[dim]⚙ {text}[/dim]", markup=True, classes="tool-call")
        )

    async def add_tool_result(self, text: str) -> None:
        """Append a dim tool-result preview (first line, ≤120 chars)."""
        preview = text.split("\n")[0][:120]
        await self.mount(
            Static(f"[dim]🤖 {preview}[/dim]", markup=True, classes="tool-result")
        )

    async def add_error(self, text: str) -> None:
        """Append a red error line and reset the content segment."""
        self._active_content = None
        await self.mount(
            Static(f"[red]Error:[/red] {text}", markup=True, classes="error")
        )


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
        border-top: solid $primary;
    }
    #status {
        height: 1;
        padding: 0 1;
        color: $text-disabled;
    }
    """

    BINDINGS = [
        ("ctrl+c", "request_quit", "Quit"),
        ("ctrl+d", "request_quit", "Quit"),
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
            assert self._run_task is not None
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

    @work(exclusive=True)
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
        try:
            async for chunk in agent_run(
                self._session,
                self._client,
                self._settings.max_iterations,
                registry=self._tool_registry,
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
                elif chunk.kind == "tool_call":
                    turn.reset_reasoning()
                    content_acc = ""
                    reasoning_acc = ""
                    await turn.add_tool_call(chunk.text)
                elif chunk.kind == "tool_result":
                    await turn.add_tool_result(chunk.text)
                scroll.scroll_end(animate=False)
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

        # Only reached on normal completion or caught Exception (not BaseException)
        if self._mode == "run":
            await asyncio.sleep(0.1)  # let Textual render the final frame
            self.exit()
        else:
            input_widget = self.query_one("#input-bar", Input)
            input_widget.disabled = False
            input_widget.focus()

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

    async def action_request_quit(self) -> None:
        """Ctrl+C / Ctrl+D: mark session interrupted if still running, then exit."""
        if self._session.status == "running":
            self._session.interrupt()
            await save_session(self._session, self._settings.db_path)
        self.exit()


# ── registry helper ───────────────────────────────────────────────────────────


def _make_registry(no_tools: bool, settings: Settings) -> ToolRegistry | None:
    if no_tools:
        return None
    try:
        from .tools.builtin import create_default_registry

        return create_default_registry(
            truncate_at=settings.tool_builtin_truncate_at,
            max_lines=settings.tool_builtin_max_lines,
        )
    except ImportError:
        return None


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
    registry = _make_registry(no_tools, settings)

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
    registry = _make_registry(no_tools, settings)

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
