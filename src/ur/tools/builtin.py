"""Built-in tools for the ur agent. Requires the [tools] optional extra."""

from __future__ import annotations

import asyncio
import functools
from pathlib import Path

import httpx

from .registry import ToolRegistry


async def shell(
    command: str,
    max_chars: int = 4000,
    timeout: int = 30,
    cwd: Path | None = None,
) -> str:
    """Run a shell command and return stdout+stderr."""
    try:
        proc = await asyncio.create_subprocess_shell(
            command,
            stdout=asyncio.subprocess.PIPE,
            stderr=asyncio.subprocess.STDOUT,
            cwd=cwd,
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


async def read_file(path: str, max_lines: int = 200, cwd: Path | None = None) -> str:
    """Read a file and return its contents."""
    try:
        import aiofiles  # type: ignore[import-untyped]
        p = Path(path)
        resolved = (cwd / p) if (cwd and not p.is_absolute()) else p
        async with aiofiles.open(resolved) as f:
            lines = await f.readlines()
        if len(lines) > max_lines:
            head = lines[:max_lines]
            head.append(f"\n... ({len(lines) - max_lines} more lines)")
            return "".join(head)
        return "".join(lines)
    except Exception as e:
        return f"Error: {e}"


async def write_file(path: str, content: str, cwd: Path | None = None) -> str:
    """Write content to a file, overwriting any existing content."""
    try:
        import aiofiles
        p = Path(path)
        resolved = (cwd / p) if (cwd and not p.is_absolute()) else p
        async with aiofiles.open(resolved, "w") as f:
            await f.write(content)
        return f"Written {len(content)} bytes to {resolved}"
    except Exception as e:
        return f"Error: {e}"


async def browser_get(url: str, max_chars: int = 4000, timeout: int = 30) -> str:
    """Visit a URL with a headless browser and return rendered page as markdown."""
    try:
        from markdownify import markdownify as to_md
        from playwright.async_api import async_playwright
        async with async_playwright() as p:
            browser = await p.chromium.launch(headless=True)
            try:
                page = await browser.new_page()
                await page.goto(
                    url,
                    wait_until="domcontentloaded",
                    timeout=timeout * 1000,
                )
                html = await page.content()
                text = to_md(html, strip=["script", "style", "head"])
                if len(text) > max_chars:
                    text = (
                        text[:max_chars]
                        + f"\n... (truncated, {len(text) - max_chars} more chars)"
                    )
                return text or "(no content)"
            finally:
                await browser.close()
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


async def web_search(query: str, max_results: int = 10) -> str:
    """Perform a web search and return the top results."""
    try:
        from ddgs import DDGS

        results = list(DDGS().text(query, max_results=max_results))
        if not results:
            return "No results found."
        output = []
        for i, r in enumerate(results, 1):
            title = r.get("title", "No title")
            url = r.get("href", "No URL")
            snippet = r.get("body", "No description").replace("\n", " ")
            output.append(f"{i}. {title}\n   {url}\n   {snippet}")
        return "\n\n".join(output)
    except Exception as e:
        return f"Error: {e}"

def create_default_registry(
    truncate_at: int = 4000,
    max_lines: int = 200,
    max_search_results: int = 10,
    shell_timeout: int = 30,
    http_timeout: int = 10,
    browser_timeout: int = 30,
    workspace_dir: Path | None = None,
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
            },
            "required": ["command"],
        },
        fn=functools.partial(
            shell, max_chars=truncate_at, timeout=shell_timeout, cwd=workspace_dir
        ),
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
        fn=functools.partial(read_file, max_lines=max_lines, cwd=workspace_dir),
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
        fn=functools.partial(write_file, cwd=workspace_dir),
    )

    registry.register(
        name="http_get",
        description=(
            "Fetch a known URL and return the raw response body (HTML, JSON, text). "
            "Use when you already have a URL and the page does not require JavaScript. "
            f"Output truncated at {truncate_at} characters."
        ),
        parameters={
            "type": "object",
            "properties": {
                "url": {"type": "string", "description": "URL to fetch"},
            },
            "required": ["url"],
        },
        fn=functools.partial(http_get, max_chars=truncate_at, timeout=http_timeout),
    )

    registry.register(
        name="browser_get",
        description=(
            "Fetch a known URL using a headless Chromium browser and return the "
            "fully rendered page as markdown (links preserved). "
            "Use when you already have a URL and the page requires JavaScript. "
            f"Output truncated at {truncate_at} characters."
        ),
        parameters={
            "type": "object",
            "properties": {
                "url": {"type": "string", "description": "URL to visit"},
            },
            "required": ["url"],
        },
        fn=functools.partial(
            browser_get, max_chars=truncate_at, timeout=browser_timeout
        ),
    )

    registry.register(
        name="web_search",
        description=(
            "Search the web to discover URLs and summaries when you do not already "
            "have a URL. Returns titles, URLs, and snippets — not full page content. "
            "Follow up with http_get or browser_get to read the actual page. "
            f"Returns up to {max_search_results} results."
        ),
        parameters={
            "type": "object",
            "properties": {
                "query": {"type": "string", "description": "Search query"},
            },
            "required": ["query"],
        },
        fn=functools.partial(web_search, max_results=max_search_results),
    )

    return registry
