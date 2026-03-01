from __future__ import annotations

import asyncio
import signal
from typing import Any

import aiosqlite
import typer
from rich import box
from rich.console import Console, Group
from rich.live import Live
from rich.markdown import Markdown
from rich.panel import Panel
from rich.table import Table
from rich.text import Text

from .agent.loop import run as agent_run
from .agent.session import AgentSession
from .config import Settings, get_settings
from .llm.client import LLMClient, Provider
from .memory.db import get_db, init_db
from .memory.session_store import get_session_messages, list_sessions, save_session
from .tools.registry import ToolRegistry

app = typer.Typer(name="ur", help="Agent assisted workflow", no_args_is_help=True)
console = Console()


def _settings() -> Settings:
    s = get_settings()
    s.ensure_dirs()
    return s


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


# ── run ───────────────────────────────────────────────────────────────────────


@app.command()
def run(
    task: str = typer.Argument(..., help="Task for the agent to complete"),
    model: str | None = typer.Option(None, "--model", "-m", help="Override LLM model"),
    no_tools: bool = typer.Option(False, "--no-tools", help="Disable built-in tools"),
) -> None:
    """Run the agent on a single task."""
    asyncio.run(_run(task, _settings(), model, no_tools=no_tools))


async def _run(
    task: str,
    settings: Settings,
    model_override: str | None = None,
    no_tools: bool = False,
) -> None:
    await init_db(settings.db_path)
    model = model_override or settings.model
    client = LLMClient(settings, model=model)
    session = AgentSession.new(task=task, model=model)
    registry = _make_registry(no_tools, settings)

    console.print(f"[dim]session {session.id[:8]}  model={model}[/]")
    console.print()

    try:
        reasoning_acc = ""
        content_acc = ""
        with Live(
            console=console, refresh_per_second=15, vertical_overflow="visible"
        ) as live:
            async for chunk in agent_run(
                session, client, settings.max_iterations, registry=registry
            ):
                if chunk.kind == "tool_call":
                    console.print(f"[dim]⚙ {chunk.text}[/]")
                    continue
                if chunk.kind == "tool_result":
                    preview = chunk.text.split("\n")[0][:120]
                    console.print(f"[dim]🤖 {preview}[/]")
                    continue
                if chunk.kind == "reasoning":
                    reasoning_acc += chunk.text
                else:
                    content_acc += chunk.text
                parts: list[Any] = []
                if reasoning_acc:
                    parts.append(Text(reasoning_acc, style="dim"))
                if content_acc:
                    parts.append(Markdown(content_acc))
                live.update(Group(*parts))
        session.complete()
    except Exception as e:
        session.fail()
        console.print(f"\n[red]Error:[/red] {e}")
        e_lower = str(e).lower()
        if client.provider == Provider.GEMINI and (
            "auth" in e_lower or "api_key" in e_lower
        ):
            console.print(
                "[dim]Set GEMINI_API_KEY in your environment or .env file.[/dim]"
            )
        elif client.provider == Provider.OLLAMA:
            console.print(
                "[dim]Set UR_OLLAMA_BASE_URL in your environment or .env file.[/dim]"
            )
        else:
            console.print(
                "[dim]Set the correct environment variables for the provider.[/dim]"
            )
        raise typer.Exit(1)
    except BaseException:
        session.interrupt()
        raise
    finally:
        await save_session(session, settings.db_path)
        if session.usage:
            console.print()
            console.print(
                f"[dim]tokens in={session.usage.input_tokens}"
                f" out={session.usage.output_tokens}[/]"
            )


# ── chat ──────────────────────────────────────────────────────────────────────


@app.command()
def chat(
    model: str | None = typer.Option(None, "--model", "-m", help="Override LLM model"),
    no_tools: bool = typer.Option(False, "--no-tools", help="Disable built-in tools"),
) -> None:
    """Start an interactive multi-turn chat session."""
    asyncio.run(_chat(_settings(), model, no_tools=no_tools))


async def _chat(
    settings: Settings,
    model_override: str | None = None,
    no_tools: bool = False,
) -> None:
    await init_db(settings.db_path)
    model = model_override or settings.model
    client = LLMClient(settings, model=model)
    session = AgentSession.new(task="", model=model)
    registry = _make_registry(no_tools, settings)

    console.print(
        Panel(
            f"[bold]ur[/bold]  model=[cyan]{model}[/cyan]  "
            f"session=[dim]{session.id[:8]}[/dim]\n"
            "[dim]Ctrl+C or Ctrl+D to exit[/dim]",
            box=box.ROUNDED,
        )
    )

    while session.status == "running":
        # asyncio.run() replaces the SIGINT handler with _cancel_main_task, which
        # only schedules task cancellation and does not raise KeyboardInterrupt.
        # Restore the default handler for the duration of the blocking input() call
        # so that the first Ctrl+C exits immediately.
        _prev_sigint = signal.signal(signal.SIGINT, signal.default_int_handler)
        try:
            task = console.input("\n[bold blue]you[/bold blue] › ")
        except (KeyboardInterrupt, EOFError):
            console.print("\n[dim]Goodbye.[/dim]")
            session.interrupt()
            await save_session(session, settings.db_path)
            break
        finally:
            signal.signal(signal.SIGINT, _prev_sigint)

        if not task.strip():
            continue

        console.print()

        try:
            session.add_user_message(task)
            reasoning_acc = ""
            content_acc = ""
            with Live(
                console=console, refresh_per_second=15, vertical_overflow="visible"
            ) as live:
                async for chunk in agent_run(
                    session, client, settings.max_iterations, registry=registry
                ):
                    if chunk.kind == "tool_call":
                        console.print(f"[dim]⚙ {chunk.text}[/]")
                        continue
                    if chunk.kind == "tool_result":
                        preview = chunk.text.split("\n")[0][:120]
                        console.print(f"[dim]🤖 {preview}[/]")
                        continue
                    if chunk.kind == "reasoning":
                        reasoning_acc += chunk.text
                    else:
                        content_acc += chunk.text
                    parts_chat: list[Any] = []
                    if reasoning_acc:
                        parts_chat.append(Text(reasoning_acc, style="dim"))
                    if content_acc:
                        parts_chat.append(Markdown(content_acc))
                    live.update(Group(*parts_chat))
        except Exception as e:
            session.messages.pop()  # remove orphaned user message from failed turn
            console.print(f"\n[red]Error:[/red] {e}")
            e_lower = str(e).lower()
            if client.provider == Provider.GEMINI and (
                "auth" in e_lower or "api_key" in e_lower
            ):
                console.print(
                    "[dim]Set GEMINI_API_KEY in your environment or .env file.[/dim]"
                )
            elif client.provider == Provider.OLLAMA:
                console.print(
                    "[dim]Set UR_OLLAMA_BASE_URL in your environment"
                    " or .env file.[/dim]"
                )
            else:
                console.print(
                    "[dim]Set the correct environment variables for the provider.[/dim]"
                )
        except BaseException:
            session.interrupt()
            raise
        finally:
            console.print()
            console.print(
                f"[dim]tokens in={session.usage.input_tokens} "
                f"out={session.usage.output_tokens} (session total)[/]"
            )
            await save_session(session, settings.db_path)


# ── history ───────────────────────────────────────────────────────────────────


@app.command()
def history(
    session_id: str | None = typer.Argument(None, help="Session ID to inspect"),
    limit: int = typer.Option(20, "--limit", "-n", help="Number of sessions to show"),
) -> None:
    """List past sessions, or show messages for a specific session."""
    asyncio.run(_history(_settings(), session_id, limit))


async def _history(settings: Settings, session_id: str | None, limit: int) -> None:
    await init_db(settings.db_path)

    if session_id:
        async with get_db(settings.db_path) as db:
            cursor = await db.execute(
                "SELECT id FROM sessions"
                " WHERE SUBSTR(id, 1, LENGTH(?)) = ?"
                " ORDER BY created_at DESC LIMIT 2",
                (session_id, session_id),
            )
            rows: list[aiosqlite.Row] = await cursor.fetchall()  # type: ignore[assignment]
        if not rows:
            console.print(f"[red]No session matching '{session_id}'[/red]")
            raise typer.Exit(1)
        if len(rows) > 1:
            console.print(
                f"[red]Ambiguous prefix '{session_id}' matches multiple sessions[/red]"
            )
            raise typer.Exit(1)
        full_id = rows[0]["id"]
        messages = await get_session_messages(
            full_id, settings.db_path, with_metadata=True
        )
        _role_styles = {
            "user": "bold blue",
            "assistant": "bold green",
            "tool": "bold yellow",
        }
        for msg in messages:
            role_style = _role_styles.get(msg["role"], "white")
            ts = msg.get("created_at", "")[:19]
            console.print(f"[{role_style}]{msg['role']}[/] [{ts}]")

            # Render tool calls requested by an assistant message
            tool_calls = msg.get("tool_calls")
            if tool_calls:
                for tc in tool_calls:  # type: ignore[union-attr]
                    fn_info = tc.get("function", {})
                    tc_name = fn_info.get("name", "?")
                    tc_args = fn_info.get("arguments", "")
                    tc_id = tc.get("id", "?")[:8]
                    console.print(f"[dim]call {tc_id}  {tc_name}({tc_args})[/]")

            # Render metadata for tool result messages
            if msg["role"] == "tool":
                tc_id = str(msg.get("tool_call_id", ""))[:8]
                tc_name = msg.get("name", "")
                console.print(f"[dim]result for {tc_id}  {tc_name}[/]")

            content = msg.get("content")
            if isinstance(content, str) and content:
                console.print(Markdown(content))
            elif content is not None:
                console.print(str(content))
            console.print()
        return

    sessions = await list_sessions(settings.db_path, limit=limit)
    if not sessions:
        console.print(
            '[dim]No sessions yet. Run [bold]ur run "<task>"[/bold] to start.[/dim]'
        )
        return

    table = Table(box=box.ROUNDED, show_header=True)
    table.add_column("ID", style="dim", width=10)
    table.add_column("Task", max_width=52, no_wrap=True)
    table.add_column("Model", style="cyan", width=18)
    table.add_column("Status", width=12)
    table.add_column("Tokens", justify="right", width=8)
    table.add_column("Date", style="dim", width=19)

    status_colors = {"completed": "green", "failed": "red", "interrupted": "yellow"}
    for s in sessions:
        color = status_colors.get(s["status"], "white")
        tokens = s["total_input_tokens"] + s["total_output_tokens"]
        table.add_row(
            s["id"][:8],
            s["task"] or "[dim]chat[/dim]",
            s["model"],
            f"[{color}]{s['status']}[/]",
            str(tokens) if tokens else "—",
            s["created_at"][:19].replace("T", " "),
        )
    console.print(table)


# ── init ──────────────────────────────────────────────────────────────────────


@app.command()
def init() -> None:
    """Initialize ur: create data directories and database."""
    settings = _settings()
    asyncio.run(init_db(settings.db_path))
    console.print(f"[green]✓[/green] data dir : {settings.data_dir}")
    console.print(f"[green]✓[/green] database : {settings.db_path}")
    console.print(f"[green]✓[/green] workspaces: {settings.workspaces_dir}")
    if not settings.gemini_api_key:
        console.print()
        console.print(
            "[yellow]GEMINI_API_KEY is not set.[/yellow] "
            "Add it to your environment or a .env file."
        )


if __name__ == "__main__":
    app()
