from unittest.mock import AsyncMock, patch

import litellm
import pytest

from tests.conftest import (
    TEST_API_KEY,
    MockStreamWrapper,
    make_chunk,
    skip_if_not_gemini,
    skip_if_not_ollama,
)
from ur.agent.models import StreamChunk, UsageStats
from ur.llm.client import CompletionStream, LLMClient, Provider

# ── CompletionStream ──────────────────────────────────────────────────────────


async def test_stream_yields_content_chunks():
    stream = CompletionStream(
        MockStreamWrapper(
            [
                make_chunk("Hello"),
                make_chunk(", "),
                make_chunk("world"),
            ]
        )
    )
    chunks = [c async for c in stream]
    assert chunks == [
        StreamChunk(kind="content", text="Hello"),
        StreamChunk(kind="content", text=", "),
        StreamChunk(kind="content", text="world"),
    ]


async def test_stream_yields_reasoning_chunks():
    stream = CompletionStream(
        MockStreamWrapper(
            [
                make_chunk(None, reasoning="think..."),
                make_chunk("answer"),
            ]
        )
    )
    chunks = [c async for c in stream]
    assert chunks == [
        StreamChunk(kind="reasoning", text="think..."),
        StreamChunk(kind="content", text="answer"),
    ]
    assert stream.reasoning_text == "think..."
    assert stream.full_text == "answer"


async def test_stream_accumulates_full_text_content_only():
    stream = CompletionStream(
        MockStreamWrapper(
            [
                make_chunk(None, reasoning="think"),
                make_chunk("foo"),
                make_chunk("bar"),
            ]
        )
    )
    async for _ in stream:
        pass
    assert stream.full_text == "foobar"
    assert stream.reasoning_text == "think"


async def test_stream_skips_none_content():
    stream = CompletionStream(
        MockStreamWrapper(
            [
                make_chunk(None),
                make_chunk("hi"),
                make_chunk(None),
            ]
        )
    )
    chunks = [c async for c in stream]
    assert chunks == [StreamChunk(kind="content", text="hi")]
    assert stream.full_text == "hi"


async def test_stream_captures_usage_from_chunk():
    stream = CompletionStream(
        MockStreamWrapper(
            [
                make_chunk("text", usage={"prompt_tokens": 10, "completion_tokens": 5}),
            ]
        )
    )
    async for _ in stream:
        pass
    assert stream.usage.input_tokens == 10
    assert stream.usage.output_tokens == 5


async def test_stream_usage_defaults_to_zero_when_absent():
    stream = CompletionStream(MockStreamWrapper([make_chunk("hi")]))
    async for _ in stream:
        pass
    assert stream.usage == UsageStats()


async def test_stream_empty_response():
    stream = CompletionStream(MockStreamWrapper([]))
    assert [c async for c in stream] == []
    assert stream.full_text == ""
    assert stream.reasoning_text == ""


# ── Provider detection ────────────────────────────────────────────────────────


def test_detect_provider_gemini():
    assert LLMClient._detect_provider("gemini/gemini-2.0-flash") == Provider.GEMINI


def test_detect_provider_ollama():
    assert LLMClient._detect_provider("ollama/llama3.2") == Provider.OLLAMA


def test_detect_provider_ollama_chat():
    assert LLMClient._detect_provider("ollama_chat/qwen2.5") == Provider.OLLAMA


def test_detect_provider_other():
    assert LLMClient._detect_provider("openai/gpt-4o") == Provider.OTHER


def test_llm_client_stores_provider_at_init(tmp_settings):
    tmp_settings.model = "gemini/gemini-2.0-flash"
    client = LLMClient(tmp_settings)
    assert client.provider == Provider.GEMINI


def test_llm_client_model_override_sets_correct_provider(tmp_settings):
    client = LLMClient(tmp_settings, model="ollama_chat/qwen2.5")
    assert client.provider == Provider.OLLAMA
    assert client.model == "ollama_chat/qwen2.5"


# ── LLMClient ─────────────────────────────────────────────────────────────────


async def test_llm_client_passes_model_and_messages(tmp_settings):
    with patch("ur.llm.client.litellm.acompletion", new_callable=AsyncMock) as mock_ac:
        mock_ac.return_value = MockStreamWrapper([make_chunk("ok")])
        await LLMClient(tmp_settings).stream([{"role": "user", "content": "hi"}])

    kw = mock_ac.call_args.kwargs
    assert kw["model"] == tmp_settings.model
    assert kw["messages"] == [{"role": "user", "content": "hi"}]
    assert kw["stream"] is True


@skip_if_not_gemini
async def test_llm_client_passes_api_key_when_set(tmp_settings):
    tmp_settings.gemini_api_key = TEST_API_KEY
    with patch("ur.llm.client.litellm.acompletion", new_callable=AsyncMock) as mock_ac:
        mock_ac.return_value = MockStreamWrapper([make_chunk("ok")])
        await LLMClient(tmp_settings).stream([])

    assert mock_ac.call_args.kwargs["api_key"] == TEST_API_KEY


@skip_if_not_gemini
async def test_llm_client_omits_api_key_when_empty(tmp_settings):
    tmp_settings.gemini_api_key = ""
    with patch("ur.llm.client.litellm.acompletion", new_callable=AsyncMock) as mock_ac:
        mock_ac.return_value = MockStreamWrapper([make_chunk("ok")])
        await LLMClient(tmp_settings).stream([])

    assert "api_key" not in mock_ac.call_args.kwargs


async def test_llm_client_returns_completion_stream(tmp_settings):
    with patch("ur.llm.client.litellm.acompletion", new_callable=AsyncMock) as mock_ac:
        mock_ac.return_value = MockStreamWrapper([make_chunk("hi")])
        result = await LLMClient(tmp_settings).stream([])

    assert isinstance(result, CompletionStream)


# ── Ollama ─────────────────────────────────────────────────────────────────────


@skip_if_not_ollama
async def test_ollama_model_passes_api_base(tmp_settings):
    tmp_settings.model = "ollama/llama3.2"
    tmp_settings.ollama_base_url = "http://my-server:11434"

    with patch("ur.llm.client.litellm.acompletion", new_callable=AsyncMock) as mock_ac:
        mock_ac.return_value = MockStreamWrapper([make_chunk("hi")])
        await LLMClient(tmp_settings).stream([])

    assert mock_ac.call_args.kwargs["api_base"] == "http://my-server:11434"


@skip_if_not_ollama
async def test_ollama_chat_model_passes_api_base(tmp_settings):
    tmp_settings.model = "ollama_chat/qwen2.5"
    tmp_settings.ollama_base_url = "http://my-server:11434"

    with patch("ur.llm.client.litellm.acompletion", new_callable=AsyncMock) as mock_ac:
        mock_ac.return_value = MockStreamWrapper([make_chunk("hi")])
        await LLMClient(tmp_settings).stream([])

    assert mock_ac.call_args.kwargs["api_base"] == "http://my-server:11434"


@skip_if_not_gemini
async def test_non_ollama_model_does_not_pass_api_base(tmp_settings):
    tmp_settings.model = "gemini/gemini-2.0-flash"

    with patch("ur.llm.client.litellm.acompletion", new_callable=AsyncMock) as mock_ac:
        mock_ac.return_value = MockStreamWrapper([make_chunk("hi")])
        await LLMClient(tmp_settings).stream([])

    assert "api_base" not in mock_ac.call_args.kwargs


# ── Exception propagation ─────────────────────────────────────────────────────


async def test_llm_client_raises_authentication_error(tmp_settings):
    """AuthenticationError from litellm.acompletion must surface from stream()."""
    auth_error = litellm.AuthenticationError(
        message="Invalid API key",
        llm_provider="gemini",
        model=tmp_settings.model,
    )

    with patch("ur.llm.client.litellm.acompletion", new_callable=AsyncMock) as mock_ac:
        mock_ac.side_effect = auth_error

        with pytest.raises(litellm.AuthenticationError, match="Invalid API key"):
            await LLMClient(tmp_settings).stream([])
