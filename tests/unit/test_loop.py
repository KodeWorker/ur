from unittest.mock import AsyncMock, MagicMock, patch

import pytest

from tests.conftest import TEST_MODEL, MockStreamWrapper, make_chunk
from ur.agent.loop import run
from ur.agent.models import UsageStats
from ur.agent.session import AgentSession
from ur.llm.client import CompletionStream


def _make_stream(tokens: list[str], usage: dict | None = None) -> CompletionStream:
    chunks = [make_chunk(t) for t in tokens]
    if usage:
        chunks[-1] = make_chunk(tokens[-1], usage=usage)
    return CompletionStream(MockStreamWrapper(chunks))


async def test_run_yields_tokens(tmp_settings):
    stream = _make_stream(["Hello", " world"])
    with patch("ur.agent.loop.LLMClient") as MockClient:
        MockClient.return_value.stream = AsyncMock(return_value=stream)
        session = AgentSession.new(task="hi", model=TEST_MODEL)
        tokens = [t async for t in run(session, tmp_settings)]

    assert tokens == ["Hello", " world"]


async def test_run_adds_assistant_message_to_session(tmp_settings):
    stream = _make_stream(["42"])
    with patch("ur.agent.loop.LLMClient") as MockClient:
        MockClient.return_value.stream = AsyncMock(return_value=stream)
        session = AgentSession.new(task="what is 6*7?", model=TEST_MODEL)
        async for _ in run(session, tmp_settings):
            pass

    assert session.messages[-1]["role"] == "assistant"
    assert session.messages[-1]["content"] == "42"
    assert "created_at" in session.messages[-1]


async def test_run_accumulates_usage(tmp_settings):
    stream = _make_stream(["ok"], usage={"prompt_tokens": 8, "completion_tokens": 3})
    with patch("ur.agent.loop.LLMClient") as MockClient:
        MockClient.return_value.stream = AsyncMock(return_value=stream)
        session = AgentSession.new(task="t", model=TEST_MODEL)
        async for _ in run(session, tmp_settings):
            pass

    assert session.usage.input_tokens == 8
    assert session.usage.output_tokens == 3


async def test_run_stops_after_one_iteration_in_phase1(tmp_settings):
    """Loop must break after first LLM response (no tool use in Phase 1)."""
    stream = _make_stream(["answer"])
    with patch("ur.agent.loop.LLMClient") as MockClient:
        MockClient.return_value.stream = AsyncMock(return_value=stream)
        session = AgentSession.new(task="t", model=TEST_MODEL)
        async for _ in run(session, tmp_settings):
            pass

    # Only one stream call regardless of max_iterations
    MockClient.return_value.stream.assert_called_once()


async def test_run_passes_full_message_history_to_llm(tmp_settings):
    captured: list = []

    async def capture_and_return(messages):
        captured.extend(messages)
        return _make_stream(["a2"])

    with patch("ur.agent.loop.LLMClient") as MockClient:
        MockClient.return_value.stream = capture_and_return
        session = AgentSession.new(task="q1", model=TEST_MODEL)
        session.add_assistant_message("a1")
        session.add_user_message("q2")

        async for _ in run(session, tmp_settings):
            pass

    assert len(captured) == 3
    assert captured[-1]["role"] == "user"
    assert captured[-1]["content"] == "q2"
