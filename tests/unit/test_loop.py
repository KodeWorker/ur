from unittest.mock import AsyncMock, MagicMock

import litellm
import pytest

from tests.conftest import TEST_MODEL, MockStreamWrapper, make_chunk
from ur.agent.loop import run
from ur.agent.session import AgentSession
from ur.llm.client import CompletionStream, LLMClient

_MAX_ITER = 5


def _make_stream(tokens: list[str], usage: dict | None = None) -> CompletionStream:
    chunks = [make_chunk(t) for t in tokens]
    if usage:
        chunks[-1] = make_chunk(tokens[-1], usage=usage)
    return CompletionStream(MockStreamWrapper(chunks))


def _make_client(stream: CompletionStream) -> MagicMock:
    client = MagicMock(spec=LLMClient)
    client.stream = AsyncMock(return_value=stream)
    return client


async def test_run_yields_tokens():
    stream = _make_stream(["Hello", " world"])
    session = AgentSession.new(task="hi", model=TEST_MODEL)
    tokens = [t async for t in run(session, _make_client(stream), _MAX_ITER)]

    assert tokens == ["Hello", " world"]


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

    async def capture_and_return(messages):
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
