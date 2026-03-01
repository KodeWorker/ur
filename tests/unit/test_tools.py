"""Tests for ToolRegistry and built-in tool implementations."""

from __future__ import annotations

import pytest

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
    assert {"shell", "read_file", "write_file", "http_get"} == names
