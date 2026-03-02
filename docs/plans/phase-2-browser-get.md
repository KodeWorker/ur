# Plan: Add `browser_get` tool via Playwright

## Context

Phase 2 promised a tool suite: filesystem, code execution, HTTP, browser.
`read_file`, `write_file`, `shell`, and `http_get` are done. `playwright>=1.45`
is already declared in `[tools]` optional deps but has no implementation.
This plan adds a single `browser_get(url)` tool that navigates headlessly with
Playwright and returns the rendered page as **markdown** (links preserved,
best for agent link-following at near-minimum token cost).

---

## Files to change

| File | Change |
|------|--------|
| `pyproject.toml` | Add `markdownify>=0.13` to `[tools]` optional deps |
| `src/ur/tools/builtin.py` | Add `browser_get()` + register in `create_default_registry` |
| `tests/unit/test_tools.py` | Add browser_get tests (skipped without `[tools]`) |

---

## `pyproject.toml`

Add `markdownify>=0.13` to the `[project.optional-dependencies] tools` list.
(`playwright>=1.45` is already there — no change needed for playwright itself.)

Note: after install, user must run once:
```bash
uv run playwright install chromium
```

---

## `src/ur/tools/builtin.py`

### New function (add after `http_get`)

```python
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
```

Follows all established patterns:
- lazy imports inside the function body
- existing mypy override on `ur.tools.builtin` already covers missing imports
- always returns `str`; exceptions become error strings

### Register in `create_default_registry` (append after `http_get` block)

```python
registry.register(
    name="browser_get",
    description=(
        "Visit a URL with a headless Chromium browser (JavaScript rendered) "
        "and return the page as markdown with links preserved. "
        f"Output truncated at {truncate_at} characters."
    ),
    parameters={
        "type": "object",
        "properties": {
            "url": {"type": "string", "description": "URL to visit"},
            "timeout": {
                "type": "integer",
                "description": "Navigation timeout in seconds (default 30)",
                "default": 30,
            },
        },
        "required": ["url"],
    },
    fn=functools.partial(browser_get, max_chars=truncate_at),
)
```

---

## `tests/unit/test_tools.py`

Add at the bottom after existing browser-unrelated tests.
Gate with `pytest.importorskip` for both `playwright` and `markdownify`.

Three tests:
1. `test_browser_get_returns_markdown` — mock `async_playwright` context;
   stub `page.content()` returning simple HTML; assert markdown in result
2. `test_browser_get_truncates_long_output` — stub returning very long HTML;
   assert `len(result) ≤ max_chars + len(suffix)`
3. `test_browser_get_handles_error` — stub `page.goto` raising `Exception`;
   assert result starts with `"Error:"`

---

## Verification

```bash
# Install deps + Chromium binary
uv pip install -e ".[tools,dev]"
uv run playwright install chromium

# Run new tests only
uv run pytest tests/unit/test_tools.py -v -k browser

# Full suite
uv run pytest tests/ -q

# Type + lint
uv run mypy src/ur/tools/builtin.py
uv run ruff check src/ur/tools/builtin.py

# Manual smoke
ur run "visit https://example.com and summarise it"
```

## Status

Implemented in `feat/phase-2`. All tests pass.
