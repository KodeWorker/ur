"""Tests for ToolRegistry and built-in tool implementations."""

from __future__ import annotations

import pytest
from pytest_mock import MockerFixture

from ur.tools.registry import ToolRegistry

# ── ToolRegistry ──────────────────────────────────────────────────────────────


def _make_registry() -> ToolRegistry:
    registry = ToolRegistry()

    async def echo(message: str) -> str:
        return message

    registry.register(
        name="echo",
        description="Echo a message back",
        parameters={
            "type": "object",
            "properties": {"message": {"type": "string"}},
            "required": ["message"],
        },
        fn=echo,
    )
    return registry


def test_registry_as_tools_list_returns_correct_schema():
    registry = _make_registry()
    tools = registry.as_tools_list()

    assert len(tools) == 1
    assert tools[0]["type"] == "function"
    assert tools[0]["function"]["name"] == "echo"
    assert tools[0]["function"]["description"] == "Echo a message back"
    assert "message" in tools[0]["function"]["parameters"]["properties"]


def test_registry_contains_registered_tool():
    registry = _make_registry()
    assert "echo" in registry
    assert "missing" not in registry


def test_registry_get_returns_spec_for_known_tool():
    registry = _make_registry()
    spec = registry.get("echo")
    assert spec is not None
    assert spec.schema["function"]["name"] == "echo"


def test_registry_get_returns_none_for_unknown_tool():
    registry = _make_registry()
    assert registry.get("nonexistent") is None


async def test_registry_dispatch_calls_correct_function():
    registry = _make_registry()
    spec = registry.get("echo")
    assert spec is not None
    result = await spec.fn(message="hello world")
    assert result == "hello world"


def test_registry_multiple_tools():
    registry = ToolRegistry()

    async def tool_a() -> str:
        return "a"

    async def tool_b() -> str:
        return "b"

    registry.register("tool_a", "A", {"type": "object", "properties": {}}, tool_a)
    registry.register("tool_b", "B", {"type": "object", "properties": {}}, tool_b)

    tools = registry.as_tools_list()
    names = {t["function"]["name"] for t in tools}
    assert names == {"tool_a", "tool_b"}


# ── Built-in tool smoke tests (skipped if [tools] extra not installed) ────────


@pytest.fixture
def _require_tools() -> None:
    pytest.importorskip("aiofiles", reason="requires [tools] extra")
    pytest.importorskip("httpx", reason="requires [tools] extra")


async def test_builtin_shell_returns_output(_require_tools: None) -> None:
    from ur.tools.builtin import shell

    result = await shell("echo hello")
    assert "hello" in result


async def test_builtin_shell_handles_error(_require_tools: None) -> None:
    from ur.tools.builtin import shell

    result = await shell("exit 1")
    # Should not raise — returns output or empty
    assert isinstance(result, str)


async def test_builtin_read_file_missing_path(_require_tools: None) -> None:
    from ur.tools.builtin import read_file

    result = await read_file("/nonexistent/path/file.txt")
    assert result.startswith("Error:")


async def test_builtin_write_then_read_file(tmp_path, _require_tools: None) -> None:
    from ur.tools.builtin import read_file, write_file

    path = str(tmp_path / "test.txt")
    write_result = await write_file(path, "hello from test")
    assert "Written" in write_result

    read_result = await read_file(path)
    assert "hello from test" in read_result


async def test_builtin_create_default_registry(_require_tools: None) -> None:
    from ur.tools.builtin import create_default_registry

    registry = create_default_registry()
    names = {t["function"]["name"] for t in registry.as_tools_list()}
    assert names == {
        "shell", "read_file", "write_file", "http_get", "browser_get", "web_search"
    }


# ── browser_get tests (skipped if playwright or markdownify not installed) ────


@pytest.fixture
def _require_browser() -> None:
    pytest.importorskip("playwright", reason="requires playwright")
    pytest.importorskip("markdownify", reason="requires markdownify")


def _make_playwright_mocks(mocker: MockerFixture, html: str) -> None:
    """Patch playwright.async_api.async_playwright with a stub returning ``html``."""
    mock_page = mocker.AsyncMock()
    mock_page.content.return_value = html
    mock_browser = mocker.AsyncMock()
    mock_browser.new_page.return_value = mock_page
    mock_p = mocker.AsyncMock()
    mock_p.chromium.launch.return_value = mock_browser

    mock_ap_cm = mocker.AsyncMock()
    mock_ap_cm.__aenter__.return_value = mock_p
    mock_ap_cm.__aexit__.return_value = None
    mocker.patch("playwright.async_api.async_playwright", return_value=mock_ap_cm)


async def test_browser_get_returns_markdown(
    _require_browser: None, mocker: MockerFixture
) -> None:
    from ur.tools.builtin import browser_get

    html = (
        "<html><body>"
        "<h1>Hello</h1>"
        "<p>World <a href='https://example.com'>link</a></p>"
        "</body></html>"
    )
    _make_playwright_mocks(mocker, html)

    result = await browser_get("https://example.com")
    assert isinstance(result, str)
    assert "Hello" in result
    assert "link" in result


async def test_browser_get_truncates_long_output(
    _require_browser: None, mocker: MockerFixture
) -> None:
    from ur.tools.builtin import browser_get

    long_html = "<html><body><p>" + "x" * 5000 + "</p></body></html>"
    _make_playwright_mocks(mocker, long_html)

    max_chars = 100
    result = await browser_get("https://example.com", max_chars=max_chars)
    assert isinstance(result, str)
    assert len(result) <= max_chars + len(f"\n... (truncated, {5000} more chars)")
    assert "truncated" in result


async def test_browser_get_handles_error(
    _require_browser: None, mocker: MockerFixture
) -> None:
    from ur.tools.builtin import browser_get

    mock_page = mocker.AsyncMock()
    mock_page.goto.side_effect = Exception("navigation failed")
    mock_browser = mocker.AsyncMock()
    mock_browser.new_page.return_value = mock_page
    mock_p = mocker.AsyncMock()
    mock_p.chromium.launch.return_value = mock_browser

    mock_ap_cm = mocker.AsyncMock()
    mock_ap_cm.__aenter__.return_value = mock_p
    mock_ap_cm.__aexit__.return_value = None
    mocker.patch("playwright.async_api.async_playwright", return_value=mock_ap_cm)

    result = await browser_get("https://example.com")
    assert result.startswith("Error:")


# ── web_search tests (skipped if duckduckgo-search not installed) ─────────────


@pytest.fixture
def _require_ddg() -> None:
    pytest.importorskip("ddgs", reason="requires ddgs")


def _patch_ddgs(mocker: MockerFixture, results: list[dict[str, str]]) -> None:
    """Patch DDGS so DDGS().text(...) returns *results* without a network call."""
    mock_instance = mocker.MagicMock()
    mock_instance.text.return_value = iter(results)
    mocker.patch("ddgs.DDGS", return_value=mock_instance)


async def test_web_search_returns_formatted_results(
    _require_ddg: None, mocker: MockerFixture
) -> None:
    from ur.tools.builtin import web_search

    _patch_ddgs(mocker, [
        {"title": "Example", "href": "https://example.com", "body": "An example site."},
        {"title": "Test", "href": "https://test.com", "body": "A test site."},
    ])

    result = await web_search("example query")
    assert "Example" in result
    assert "https://example.com" in result
    assert "1." in result
    assert "2." in result


async def test_web_search_no_results(_require_ddg: None, mocker: MockerFixture) -> None:
    from ur.tools.builtin import web_search

    _patch_ddgs(mocker, [])

    result = await web_search("nothing matches this")
    assert result == "No results found."


async def test_web_search_handles_error(
    _require_ddg: None, mocker: MockerFixture
) -> None:
    from ur.tools.builtin import web_search

    mock_instance = mocker.MagicMock()
    mock_instance.text.side_effect = Exception("network error")
    mocker.patch("ddgs.DDGS", return_value=mock_instance)

    result = await web_search("query")
    assert result.startswith("Error:")


# ── Plugin loader ──────────────────────────────────────────────────────────────


from pathlib import Path as _Path  # noqa: E402

from ur.tools.plugins import load_plugins  # noqa: E402


def test_load_plugins_missing_dir_does_not_raise(tmp_path: _Path) -> None:
    registry = ToolRegistry()
    load_plugins(registry, tmp_path / "nonexistent")
    assert registry.as_tools_list() == []


def test_load_plugins_registers_valid_plugin(tmp_path: _Path) -> None:
    (tmp_path / "greet.py").write_text(
        "from ur.tools.registry import ToolRegistry\n"
        "async def _greet(name: str) -> str:\n"
        "    return f'hello {name}'\n"
        "def register(registry: ToolRegistry) -> None:\n"
        "    registry.register(\n"
        "        name='greet', description='Greet someone',\n"
        "        parameters={'type': 'object',"
        " 'properties': {'name': {'type': 'string'}},"
        " 'required': ['name']},\n"
        "        fn=_greet,\n"
        "    )\n"
    )
    registry = ToolRegistry()
    load_plugins(registry, tmp_path)
    assert "greet" in registry


def test_load_plugins_skips_syntax_error(tmp_path: _Path) -> None:
    (tmp_path / "bad.py").write_text("def register(r): pass\ndef broken {{{{")
    registry = ToolRegistry()
    load_plugins(registry, tmp_path)  # must not raise
    assert registry.as_tools_list() == []


def test_load_plugins_skips_missing_register(tmp_path: _Path) -> None:
    (tmp_path / "noregister.py").write_text("# no register fn\n")
    registry = ToolRegistry()
    load_plugins(registry, tmp_path)
    assert registry.as_tools_list() == []


def test_load_plugins_skips_raising_register(tmp_path: _Path) -> None:
    (tmp_path / "exploding.py").write_text(
        "def register(registry):\n    raise RuntimeError('boom')\n"
    )
    registry = ToolRegistry()
    load_plugins(registry, tmp_path)  # must not raise
    assert registry.as_tools_list() == []


def test_load_plugins_alphabetical_last_writer_wins(tmp_path: _Path) -> None:
    stub = (
        "async def _fn() -> str:\n    return '{v}'\n"
        "def register(registry):\n"
        "    registry.register('same', 'from {v}',"
        " {{'type': 'object', 'properties': {{}}}}, _fn)\n"
    )
    (tmp_path / "aaa.py").write_text(stub.format(v="a"))
    (tmp_path / "zzz.py").write_text(stub.format(v="z"))
    registry = ToolRegistry()
    load_plugins(registry, tmp_path)
    spec = registry.get("same")
    assert spec is not None
    assert spec.schema["function"]["description"] == "from z"


def test_load_plugins_can_override_builtin(tmp_path: _Path) -> None:
    async def _original(command: str) -> str:
        return "original"

    registry = ToolRegistry()
    registry.register(
        "shell", "original shell", {"type": "object", "properties": {}}, _original
    )

    (tmp_path / "override.py").write_text(
        "async def _new(command: str) -> str:\n    return 'intercepted'\n"
        "def register(registry):\n"
        "    registry.register('shell', 'overridden shell',\n"
        "        {'type': 'object',"
        " 'properties': {'command': {'type': 'string'}}}, _new)\n"
    )
    load_plugins(registry, tmp_path)
    spec = registry.get("shell")
    assert spec is not None
    assert spec.schema["function"]["description"] == "overridden shell"
