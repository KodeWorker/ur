from unittest.mock import AsyncMock, MagicMock

import litellm
import pytest

from tests.conftest import (
    TEST_MODEL,
    MockStreamWrapper,
    make_chunk,
    make_tool_call_chunk,
)
from ur.agent.loop import run
from ur.agent.models import StreamChunk
from ur.agent.session import AgentSession
from ur.llm.client import CompletionStream, LLMClient
from ur.tools.registry import ToolRegistry

_MAX_ITER = 5


def _make_stream(tokens: list[str], usage: dict | None = None) -> CompletionStream:
    chunks = [make_chunk(t) for t in tokens]
    if usage:
        chunks[-1] = make_chunk(tokens[-1], usage=usage)
    return CompletionStream(MockStreamWrapper(chunks))


def _make_tool_call_stream(
    tool_call_id: str, name: str, arguments: str
) -> CompletionStream:
    chunks = [make_tool_call_chunk(tool_call_id, name, arguments)]
    return CompletionStream(MockStreamWrapper(chunks))


def _make_registry(name: str, result: str) -> ToolRegistry:
    registry = ToolRegistry()

    async def _fn(**kwargs: str) -> str:
        return result

    registry.register(
        name=name,
        description="test tool",
        parameters={"type": "object", "properties": {}, "required": []},
        fn=_fn,
    )
    return registry


def _make_client(stream: CompletionStream) -> MagicMock:
    client = MagicMock(spec=LLMClient)
    client.stream = AsyncMock(return_value=stream)
    return client


async def test_run_yields_chunks():
    stream = _make_stream(["Hello", " world"])
    session = AgentSession.new(task="hi", model=TEST_MODEL)
    chunks = [c async for c in run(session, _make_client(stream), _MAX_ITER)]

    assert chunks == [
        StreamChunk(kind="content", text="Hello"),
        StreamChunk(kind="content", text=" world"),
    ]


async def test_run_adds_assistant_message_to_session():
    stream = _make_stream(["42"])
    session = AgentSession.new(task="what is 6*7?", model=TEST_MODEL)
    async for _ in run(session, _make_client(stream), _MAX_ITER):
        pass

    assert session.messages[-1]["role"] == "assistant"
    assert session.messages[-1]["content"] == "42"


async def test_run_accumulates_usage():
    stream = _make_stream(["ok"], usage={"prompt_tokens": 8, "completion_tokens": 3})
    session = AgentSession.new(task="t", model=TEST_MODEL)
    async for _ in run(session, _make_client(stream), _MAX_ITER):
        pass

    assert session.usage.input_tokens == 8
    assert session.usage.output_tokens == 3


async def test_run_stops_after_one_iteration_in_phase1():
    """Loop must break after first LLM response (no tool use in Phase 1)."""
    stream = _make_stream(["answer"])
    client = _make_client(stream)
    session = AgentSession.new(task="t", model=TEST_MODEL)
    async for _ in run(session, client, _MAX_ITER):
        pass

    # Only one stream call regardless of max_iterations
    client.stream.assert_called_once()


async def test_run_passes_full_message_history_to_llm():
    captured: list = []

    async def capture_and_return(messages, tools=None):
        captured.extend(messages)
        return _make_stream(["a2"])

    client = MagicMock(spec=LLMClient)
    client.stream = capture_and_return
    session = AgentSession.new(task="q1", model=TEST_MODEL)
    session.add_assistant_message("a1")
    session.add_user_message("q2")

    async for _ in run(session, client, _MAX_ITER):
        pass

    assert len(captured) == 3
    assert captured[-1]["role"] == "user"
    assert captured[-1]["content"] == "q2"


async def test_run_prepends_system_prompt_to_messages_sent_to_llm():
    """system_prompt is prepended to the messages sent to client.stream but
    is not added to session.messages."""
    captured: list = []

    async def capture_and_return(messages, tools=None):
        captured.extend(messages)
        return _make_stream(["reply"])

    client = MagicMock(spec=LLMClient)
    client.stream = capture_and_return
    session = AgentSession.new(task="hello", model=TEST_MODEL)
    initial_session_message_count = len(session.messages)

    async for _ in run(session, client, _MAX_ITER, system_prompt="Be helpful."):
        pass

    # System message is the first element sent to the LLM
    assert captured[0] == {"role": "system", "content": "Be helpful."}

    # The original session messages follow after the system message
    assert captured[1:] == session.messages[:initial_session_message_count]

    # System message is NOT stored in the session
    assert all(m["role"] != "system" for m in session.messages)


async def test_run_handles_empty_response():
    stream = _make_stream([])
    session = AgentSession.new(task="t", model=TEST_MODEL)
    async for _ in run(session, _make_client(stream), _MAX_ITER):
        pass

    assert session.messages[-1]["role"] == "assistant"
    assert session.messages[-1]["content"] == ""


# ── Exception propagation ─────────────────────────────────────────────────────


class _ErroringChunkSource:
    """A raw chunk source that yields one real chunk then raises APIConnectionError."""

    def __aiter__(self):
        return self._iter()

    async def _iter(self):
        yield make_chunk("first token")
        raise litellm.APIConnectionError(
            message="Connection lost",
            llm_provider="gemini",
            model="gemini/gemini-test",
        )


async def test_loop_run_propagates_stream_error():
    """Errors raised while iterating the stream must propagate out of loop.run()."""
    stream = CompletionStream(_ErroringChunkSource())
    client = MagicMock(spec=LLMClient)
    client.stream = AsyncMock(return_value=stream)
    session = AgentSession.new(task="t", model=TEST_MODEL)

    with pytest.raises(litellm.APIConnectionError, match="Connection lost"):
        async for _ in run(session, client, _MAX_ITER):
            pass


# ── Tool dispatch ─────────────────────────────────────────────────────────────


async def test_run_dispatches_single_tool_call():
    """Loop executes a tool call and sends the result back for a second LLM call."""
    call_id = "call_abc"
    tc_stream = _make_tool_call_stream(call_id, "shell", '{"command": "ls"}')
    text_stream = _make_stream(["files: a.txt"])

    client = MagicMock(spec=LLMClient)
    client.stream = AsyncMock(side_effect=[tc_stream, text_stream])
    registry = _make_registry("shell", "a.txt")
    session = AgentSession.new(task="list files", model=TEST_MODEL)

    chunks = [c async for c in run(session, client, _MAX_ITER, registry=registry)]

    # Content from the final text response is yielded
    assert any(c.text == "files: a.txt" for c in chunks)
    # Two LLM calls: one for tool dispatch, one for final reply
    assert client.stream.call_count == 2
    # Message order: user → assistant (tool_calls) → tool result → assistant (text)
    roles = [m["role"] for m in session.messages]
    assert roles == ["user", "assistant", "tool", "assistant"]


async def test_run_appends_tool_result_messages_to_session():
    """Tool result messages have the correct tool_call_id, name and content."""
    call_id = "call_xyz"
    tc_stream = _make_tool_call_stream(call_id, "shell", '{"command": "pwd"}')
    text_stream = _make_stream(["done"])

    client = MagicMock(spec=LLMClient)
    client.stream = AsyncMock(side_effect=[tc_stream, text_stream])
    registry = _make_registry("shell", "/home/user")
    session = AgentSession.new(task="where", model=TEST_MODEL)

    async for _ in run(session, client, _MAX_ITER, registry=registry):
        pass

    tool_msgs = [m for m in session.messages if m["role"] == "tool"]
    assert len(tool_msgs) == 1
    assert tool_msgs[0]["tool_call_id"] == call_id
    assert tool_msgs[0]["name"] == "shell"
    assert tool_msgs[0]["content"] == "/home/user"


async def test_run_tool_error_does_not_propagate():
    """A tool that raises must return an error string — the loop must continue."""
    call_id = "call_err"
    tc_stream = _make_tool_call_stream(call_id, "shell", '{"command": "fail"}')
    text_stream = _make_stream(["I see there was an error"])

    client = MagicMock(spec=LLMClient)
    client.stream = AsyncMock(side_effect=[tc_stream, text_stream])

    registry = ToolRegistry()

    async def _failing(**kwargs: str) -> str:
        raise RuntimeError("execution failed")

    registry.register(
        name="shell",
        description="test",
        parameters={"type": "object", "properties": {}, "required": []},
        fn=_failing,
    )
    session = AgentSession.new(task="test", model=TEST_MODEL)

    # Must not raise
    async for _ in run(session, client, _MAX_ITER, registry=registry):
        pass

    tool_msgs = [m for m in session.messages if m["role"] == "tool"]
    assert len(tool_msgs) == 1
    assert "Error" in tool_msgs[0]["content"]


async def test_run_loops_up_to_max_iterations():
    """Loop terminates at max_iterations even if every response is a tool call."""
    call_count = 0

    async def _always_tool_call(messages, tools=None):
        nonlocal call_count
        call_count += 1
        return _make_tool_call_stream(f"call_{call_count}", "shell", '{"command": "x"}')

    client = MagicMock(spec=LLMClient)
    client.stream = _always_tool_call
    registry = _make_registry("shell", "output")
    max_iter = 3
    session = AgentSession.new(task="loop", model=TEST_MODEL)

    chunks = [c async for c in run(session, client, max_iter, registry=registry)]

    assert call_count == max_iter
    # After exhaustion a content chunk informs the user
    assert any(c.kind == "content" for c in chunks)
    # And a final assistant message is recorded so history is accurate
    assert session.messages[-1]["role"] == "assistant"
    assert "maximum" in session.messages[-1]["content"]


async def test_run_without_registry_stops_after_first_response():
    """registry=None preserves the Phase 1 single-call behavior."""
    stream = _make_stream(["answer"])
    client = _make_client(stream)
    session = AgentSession.new(task="t", model=TEST_MODEL)

    async for _ in run(session, client, _MAX_ITER, registry=None):
        pass

    client.stream.assert_called_once()


# ── confirm_tool allow / deny ─────────────────────────────────────────────────


async def test_run_confirm_tool_allowed_executes_tool():
    """confirm_tool returning None (allowed) lets the tool execute normally."""
    call_id = "call_allow"
    tc_stream = _make_tool_call_stream(call_id, "shell", '{"command": "ls"}')
    text_stream = _make_stream(["files: a.txt"])

    client = MagicMock(spec=LLMClient)
    client.stream = AsyncMock(side_effect=[tc_stream, text_stream])
    registry = _make_registry("shell", "a.txt")
    session = AgentSession.new(task="list", model=TEST_MODEL)

    async def _allow(name: str, args: str) -> str | None:
        return None  # allowed

    chunks = [
        c
        async for c in run(
            session, client, _MAX_ITER, registry=registry, confirm_tool=_allow
        )
    ]

    assert any(c.kind == "tool_result" and "a.txt" in c.text for c in chunks)
    tool_msgs = [m for m in session.messages if m["role"] == "tool"]
    assert tool_msgs[0]["content"] == "a.txt"


@pytest.mark.parametrize(
    "denial_reason, expected_fragment",
    [
        ("", "Tool call denied by user."),
        ("bad idea", "Tool call denied by user with bad idea"),
    ],
)
async def test_run_confirm_tool_denied_records_denial(
    denial_reason: str, expected_fragment: str
) -> None:
    """confirm_tool returning a string (denial) records the correct tool result."""
    call_id = "call_deny"
    tc_stream = _make_tool_call_stream(call_id, "shell", '{"command": "rm -rf /"}')
    text_stream = _make_stream(["understood"])

    client = MagicMock(spec=LLMClient)
    client.stream = AsyncMock(side_effect=[tc_stream, text_stream])
    registry = _make_registry("shell", "should not run")
    session = AgentSession.new(task="deny test", model=TEST_MODEL)

    async def _deny(name: str, args: str) -> str | None:
        return denial_reason

    chunks = [
        c
        async for c in run(
            session, client, _MAX_ITER, registry=registry, confirm_tool=_deny
        )
    ]

    tool_msgs = [m for m in session.messages if m["role"] == "tool"]
    assert len(tool_msgs) == 1
    assert tool_msgs[0]["content"] == expected_fragment
    # tool_call chunk is still yielded so TUI can reset_reasoning
    assert any(c.kind == "tool_call" for c in chunks)
    assert any(c.kind == "tool_result" and expected_fragment in c.text for c in chunks)


async def test_run_unknown_tool_returns_error_string():
    """Calling an unregistered tool writes an error string to the tool result."""
    call_id = "call_unk"
    tc_stream = _make_tool_call_stream(call_id, "unknown_tool", "{}")
    text_stream = _make_stream(["ok"])

    client = MagicMock(spec=LLMClient)
    client.stream = AsyncMock(side_effect=[tc_stream, text_stream])
    registry = ToolRegistry()  # empty — no tools registered
    session = AgentSession.new(task="t", model=TEST_MODEL)

    async for _ in run(session, client, _MAX_ITER, registry=registry):
        pass

    tool_msgs = [m for m in session.messages if m["role"] == "tool"]
    assert len(tool_msgs) == 1
    assert "unknown tool" in tool_msgs[0]["content"]
