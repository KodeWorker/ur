"""Built-in tools for the ur agent. Requires the [tools] optional extra."""

from __future__ import annotations

import asyncio
import functools

import httpx

from .registry import ToolRegistry


async def shell(command: str, max_chars: int = 4000, timeout: int = 30) -> str:
    """Run a shell command and return stdout+stderr."""
    try:
        proc = await asyncio.create_subprocess_shell(
            command,
            stdout=asyncio.subprocess.PIPE,
            stderr=asyncio.subprocess.STDOUT,
        )
        try:
            stdout, _ = await asyncio.wait_for(proc.communicate(), timeout=timeout)
        except TimeoutError:
            proc.kill()
            return f"Error: command timed out after {timeout}s"
        output = stdout.decode(errors="replace")
        if len(output) > max_chars:
            output = (
                output[:max_chars]
                + f"\n... (truncated, {len(output) - max_chars} more chars)"
            )
        return output or "(no output)"
    except Exception as e:
        return f"Error: {e}"


async def read_file(path: str, max_lines: int = 200) -> str:
    """Read a file and return its contents."""
    try:
        import aiofiles  # type: ignore[import-untyped]

        async with aiofiles.open(path) as f:
            lines = await f.readlines()
        if len(lines) > max_lines:
            head = lines[:max_lines]
            head.append(f"\n... ({len(lines) - max_lines} more lines)")
            return "".join(head)
        return "".join(lines)
    except Exception as e:
        return f"Error: {e}"


async def write_file(path: str, content: str) -> str:
    """Write content to a file, overwriting any existing content."""
    try:
        import aiofiles  # type: ignore[import-untyped]

        async with aiofiles.open(path, "w") as f:
            await f.write(content)
        return f"Written {len(content)} bytes to {path}"
    except Exception as e:
        return f"Error: {e}"


async def http_get(url: str, max_chars: int = 4000, timeout: int = 10) -> str:
    """Fetch a URL and return the response body text."""
    try:
        async with httpx.AsyncClient() as client:
            response = await client.get(url, timeout=timeout, follow_redirects=True)
            text = response.text
            if len(text) > max_chars:
                text = (
                    text[:max_chars]
                    + f"\n... (truncated, {len(text) - max_chars} more chars)"
                )
            return text
    except Exception as e:
        return f"Error: {e}"


def create_default_registry(
    truncate_at: int = 4000,
    max_lines: int = 200,
) -> ToolRegistry:
    """Return a ToolRegistry pre-populated with all built-in tools."""
    registry = ToolRegistry()

    registry.register(
        name="shell",
        description=(
            "Run a shell command and return stdout+stderr combined. "
            f"Output truncated at {truncate_at} characters."
        ),
        parameters={
            "type": "object",
            "properties": {
                "command": {"type": "string", "description": "Shell command to run"},
                "timeout": {
                    "type": "integer",
                    "description": "Timeout in seconds (default 30)",
                    "default": 30,
                },
            },
            "required": ["command"],
        },
        fn=functools.partial(shell, max_chars=truncate_at),
    )

    registry.register(
        name="read_file",
        description=(
            "Read a file and return its contents. "
            f"Truncated at max_lines lines (default {max_lines})."
        ),
        parameters={
            "type": "object",
            "properties": {
                "path": {
                    "type": "string",
                    "description": "Absolute or relative file path",
                },
                "max_lines": {
                    "type": "integer",
                    "description": "Maximum lines to return (default 200)",
                    "default": 200,
                },
            },
            "required": ["path"],
        },
        fn=functools.partial(read_file, max_lines=max_lines),
    )

    registry.register(
        name="write_file",
        description="Write content to a file, overwriting any existing content.",
        parameters={
            "type": "object",
            "properties": {
                "path": {
                    "type": "string",
                    "description": "Absolute or relative file path",
                },
                "content": {"type": "string", "description": "Content to write"},
            },
            "required": ["path", "content"],
        },
        fn=write_file,
    )

    registry.register(
        name="http_get",
        description=(
            "Fetch a URL with an HTTP GET request and return the response body text. "
            f"Output truncated at {truncate_at} characters."
        ),
        parameters={
            "type": "object",
            "properties": {
                "url": {"type": "string", "description": "URL to fetch"},
                "timeout": {
                    "type": "integer",
                    "description": "Timeout in seconds (default 10)",
                    "default": 10,
                },
            },
            "required": ["url"],
        },
        fn=functools.partial(http_get, max_chars=truncate_at),
    )

    return registry
