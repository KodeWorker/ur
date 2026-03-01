"""Built-in tools for the ur agent. Requires the [tools] optional extra."""

from __future__ import annotations

import asyncio

import aiofiles  # type: ignore[import-untyped]
import httpx

from .registry import ToolRegistry

_TRUNCATE_AT = 4000


async def shell(command: str, timeout: int = 30) -> str:
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
        if len(output) > _TRUNCATE_AT:
            output = (
                output[:_TRUNCATE_AT] + f"\n... (truncated, {len(output)} chars total)"
            )
        return output or "(no output)"
    except Exception as e:
        return f"Error: {e}"


async def read_file(path: str, max_lines: int = 200) -> str:
    """Read a file and return its contents."""
    try:
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
        async with aiofiles.open(path, "w") as f:
            await f.write(content)
        return f"Written {len(content)} bytes to {path}"
    except Exception as e:
        return f"Error: {e}"


async def http_get(url: str, timeout: int = 10) -> str:
    """Fetch a URL and return the response body text."""
    try:
        async with httpx.AsyncClient() as client:
            response = await client.get(url, timeout=timeout, follow_redirects=True)
            text = response.text
            if len(text) > _TRUNCATE_AT:
                text = (
                    text[:_TRUNCATE_AT] + f"\n... (truncated, {len(text)} chars total)"
                )
            return text
    except Exception as e:
        return f"Error: {e}"


def create_default_registry() -> ToolRegistry:
    """Return a ToolRegistry pre-populated with all built-in tools."""
    registry = ToolRegistry()

    registry.register(
        name="shell",
        description=(
            "Run a shell command and return stdout+stderr combined. "
            f"Output truncated at {_TRUNCATE_AT} characters."
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
        fn=shell,
    )

    registry.register(
        name="read_file",
        description=(
            "Read a file and return its contents. "
            "Truncated at max_lines lines (default 200)."
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
        fn=read_file,
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
            f"Output truncated at {_TRUNCATE_AT} characters."
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
        fn=http_get,
    )

    return registry
