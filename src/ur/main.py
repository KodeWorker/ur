from __future__ import annotations

import asyncio

import aiosqlite
import typer
from rich import box
from rich.console import Console
from rich.markdown import Markdown
from rich.table import Table

from .config import Settings, get_settings
from .memory.db import get_db, init_db
from .memory.session_store import get_session_messages, list_sessions

app = typer.Typer(name="ur", help="Agent assisted workflow", no_args_is_help=True)
console = Console()


def _settings() -> Settings:
    s = get_settings()
    s.ensure_dirs()
    return s


# ── run ───────────────────────────────────────────────────────────────────────


@app.command()
def run(
    task: str = typer.Argument(..., help="Task for the agent to complete"),
    model: str | None = typer.Option(None, "--model", "-m", help="Override LLM model"),
    no_tools: bool = typer.Option(False, "--no-tools", help="Disable built-in tools"),
) -> None:
    """Run the agent on a single task."""
    from .tui import launch_run

    asyncio.run(launch_run(task, _settings(), model, no_tools=no_tools))


# ── chat ──────────────────────────────────────────────────────────────────────


@app.command()
def chat(
    model: str | None = typer.Option(None, "--model", "-m", help="Override LLM model"),
    no_tools: bool = typer.Option(False, "--no-tools", help="Disable built-in tools"),
) -> None:
    """Start an interactive multi-turn chat session."""
    from .tui import launch_chat

    asyncio.run(launch_chat(_settings(), model, no_tools=no_tools))


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
                for tc in tool_calls:
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
