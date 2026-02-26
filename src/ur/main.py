from __future__ import annotations

import asyncio
import sys
from typing import Optional

import typer
from rich import box
from rich.console import Console
from rich.live import Live
from rich.markdown import Markdown
from rich.panel import Panel
from rich.table import Table

from .agent.loop import run as agent_run
from .agent.session import AgentSession
from .config import get_settings
from .memory.db import init_db
from .memory.session_store import get_session_messages, list_sessions, save_session

app = typer.Typer(name="ur", help="Agent assisted workflow", no_args_is_help=True)
console = Console()


def _settings():
    s = get_settings()
    s.ensure_dirs()
    return s


# ── run ───────────────────────────────────────────────────────────────────────

@app.command()
def run(
    task: str = typer.Argument(..., help="Task for the agent to complete"),
    model: Optional[str] = typer.Option(None, "--model", "-m", help="Override LLM model"),
) -> None:
    """Run the agent on a single task."""
    asyncio.run(_run(task, _settings(), model))


async def _run(task: str, settings, model_override: str | None = None) -> None:
    await init_db(settings.db_path)
    model = model_override or settings.model
    session = AgentSession.new(task=task, model=model)

    console.print(f"[dim]session {session.id[:8]}  model={model}[/]")
    console.print()

    try:
        accumulated = ""
        with Live(console=console, refresh_per_second=15, vertical_overflow="visible") as live:
            async for token in agent_run(session, settings):
                accumulated += token
                live.update(Markdown(accumulated))
    except Exception as e:
        session.fail()
        console.print(f"\n[red]Error:[/red] {e}")
        if "auth" in str(e).lower() or "api_key" in str(e).lower():
            console.print("[dim]Set GEMINI_API_KEY in your environment or .env file.[/dim]")
        await save_session(session, settings.db_path)
        raise typer.Exit(1)

    session.complete()
    console.print()
    console.print(
        f"[dim]tokens in={session.usage.input_tokens} out={session.usage.output_tokens}[/]"
    )
    await save_session(session, settings.db_path)


# ── chat ──────────────────────────────────────────────────────────────────────

@app.command()
def chat(
    model: Optional[str] = typer.Option(None, "--model", "-m", help="Override LLM model"),
) -> None:
    """Start an interactive multi-turn chat session."""
    asyncio.run(_chat(_settings(), model))


async def _chat(settings, model_override: str | None = None) -> None:
    await init_db(settings.db_path)
    model = model_override or settings.model
    session = AgentSession.new(task="", model=model)

    console.print(
        Panel(
            f"[bold]ur[/bold]  model=[cyan]{model}[/cyan]  "
            f"session=[dim]{session.id[:8]}[/dim]\n"
            "[dim]Ctrl+C or Ctrl+D to exit[/dim]",
            box=box.ROUNDED,
        )
    )

    while True:
        try:
            task = console.input("\n[bold blue]you[/bold blue] › ")
        except (KeyboardInterrupt, EOFError):
            console.print("\n[dim]Goodbye.[/dim]")
            session.interrupt()
            break

        if not task.strip():
            continue

        session.add_user_message(task)
        console.print()

        try:
            accumulated = ""
            with Live(console=console, refresh_per_second=15, vertical_overflow="visible") as live:
                async for token in agent_run(session, settings):
                    accumulated += token
                    live.update(Markdown(accumulated))
        except Exception as e:
            session.fail()
            console.print(f"\n[red]Error:[/red] {e}")
            if "auth" in str(e).lower() or "api_key" in str(e).lower():
                console.print("[dim]Set GEMINI_API_KEY in your environment or .env file.[/dim]")
            await save_session(session, settings.db_path)
            continue

        console.print()
        console.print(
            f"[dim]tokens in={session.usage.input_tokens} "
            f"out={session.usage.output_tokens} (session total)[/]"
        )

    await save_session(session, settings.db_path)


# ── history ───────────────────────────────────────────────────────────────────

@app.command()
def history(
    session_id: Optional[str] = typer.Argument(None, help="Session ID to inspect"),
    limit: int = typer.Option(20, "--limit", "-n", help="Number of sessions to show"),
) -> None:
    """List past sessions, or show messages for a specific session."""
    asyncio.run(_history(_settings(), session_id, limit))


async def _history(settings, session_id: str | None, limit: int) -> None:
    await init_db(settings.db_path)

    if session_id:
        # Match partial IDs
        sessions = await list_sessions(settings.db_path, limit=1000)
        matched = [s for s in sessions if s["id"].startswith(session_id)]
        if not matched:
            console.print(f"[red]No session matching '{session_id}'[/red]")
            raise typer.Exit(1)
        full_id = matched[0]["id"]
        messages = await get_session_messages(full_id, settings.db_path)
        for msg in messages:
            role_style = "bold blue" if msg["role"] == "user" else "bold green"
            console.print(f"[{role_style}]{msg['role']}[/] [{msg['created_at'][:19]}]")
            console.print(Markdown(msg["content"]))
            console.print()
        return

    sessions = await list_sessions(settings.db_path, limit=limit)
    if not sessions:
        console.print("[dim]No sessions yet. Run [bold]ur run \"<task>\"[/bold] to start.[/dim]")
        return

    table = Table(box=box.ROUNDED, show_header=True)
    table.add_column("ID",     style="dim",   width=10)
    table.add_column("Task",                  max_width=52, no_wrap=True)
    table.add_column("Model",  style="cyan",  width=18)
    table.add_column("Status",               width=12)
    table.add_column("Tokens", justify="right", width=8)
    table.add_column("Date",   style="dim",   width=19)

    status_colors = {"completed": "green", "failed": "red", "interrupted": "yellow"}
    for s in sessions:
        color = status_colors.get(s["status"], "white")
        tokens = s["total_input_tokens"] + s["total_output_tokens"]
        table.add_row(
            s["id"][:8],
            s["task"][:80] or "[dim]chat[/dim]",
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
        console.print("[yellow]GEMINI_API_KEY is not set.[/yellow] "
                      "Add it to your environment or a .env file.")


if __name__ == "__main__":
    app()
