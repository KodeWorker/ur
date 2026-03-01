from __future__ import annotations

import json
import os
import uuid
from collections.abc import AsyncGenerator, AsyncIterator
from enum import StrEnum
from typing import Any, ClassVar

import litellm
import ollama

from ..agent.models import Message, StreamChunk, UsageStats
from ..config import Settings

# Suppress litellm's verbose startup output
litellm.suppress_debug_info = True
os.environ.setdefault("LITELLM_LOG", "ERROR")


class Provider(StrEnum):
    GEMINI = "gemini"
    OLLAMA = "ollama"
    OTHER = "other"


def _to_ollama_messages(messages: list[dict[str, Any]]) -> list[dict[str, Any]]:
    """Convert OpenAI-format messages to Ollama format.

    Ollama requires tool_calls[].function.arguments to be a dict, whereas the
    OpenAI wire format (and our session store) uses a JSON string.
    """
    result = []
    for msg in messages:
        tool_calls = msg.get("tool_calls")
        if not tool_calls:
            result.append(msg)
            continue
        converted_tcs = []
        for tc in tool_calls:
            fn = tc.get("function") or {}
            args = fn.get("arguments", {})
            if isinstance(args, str):
                try:
                    args = json.loads(args)
                except json.JSONDecodeError:
                    args = {}
            converted_tcs.append({**tc, "function": {**fn, "arguments": args}})
        result.append({**msg, "tool_calls": converted_tcs})
    return result


class LLMClient:
    def __init__(self, settings: Settings, model: str | None = None) -> None:
        self.settings = settings
        self.model = model or settings.model
        self.provider = self._detect_provider(self.model)

    @staticmethod
    def _detect_provider(model: str) -> Provider:
        prefix = model.split("/")[0]
        if prefix == "gemini":
            return Provider.GEMINI
        if prefix in {"ollama", "ollama_chat"}:
            return Provider.OLLAMA
        return Provider.OTHER

    # Keys that are internal metadata and must never be sent to the LLM API
    _INTERNAL_KEYS: ClassVar[frozenset[str]] = frozenset({"created_at"})

    async def stream(
        self,
        messages: list[Message],
        tools: list[dict[str, Any]] | None = None,
    ) -> CompletionStream:
        api_messages = [
            {k: v for k, v in msg.items() if k not in self._INTERNAL_KEYS}
            for msg in messages
        ]

        if self.provider == Provider.OLLAMA:
            model_name = self.model.split("/", 1)[-1]
            client = ollama.AsyncClient(host=self.settings.ollama_base_url)
            response = await client.chat(
                model=model_name,
                messages=_to_ollama_messages(api_messages),
                tools=tools or [],
                stream=True,
                think=self.settings.model_think or None,
            )
            return OllamaCompletionStream(response)

        kwargs: dict[str, Any] = dict(
            model=self.model,
            messages=api_messages,
            stream=True,
        )
        if tools:
            kwargs["tools"] = tools
        if self.provider == Provider.GEMINI and self.settings.gemini_api_key:
            kwargs["api_key"] = self.settings.gemini_api_key

        response = await litellm.acompletion(**kwargs)
        return CompletionStream(response)


class CompletionStream:
    """Wraps a litellm streaming response. Iterate to get tokens; usage is set after."""

    def __init__(self, response: litellm.CustomStreamWrapper) -> None:
        self._response = response
        self.full_text: str = ""  # content only — stored in session messages
        self.reasoning_text: str = ""  # thinking tokens — display only
        self.usage: UsageStats = UsageStats()
        self.tool_calls: list[dict[str, Any]] = []

    @property
    def has_tool_calls(self) -> bool:
        return bool(self.tool_calls)

    def __aiter__(self) -> AsyncIterator[StreamChunk]:
        return self._iter()

    async def _iter(self) -> AsyncGenerator[StreamChunk, None]:
        tool_calls_acc: dict[int, dict[str, Any]] = {}

        async for chunk in self._response:
            if not chunk.choices:
                continue
            delta = chunk.choices[0].delta

            # Reasoning/thinking tokens (e.g. DeepSeek-R1 via Ollama)
            reasoning = getattr(delta, "reasoning_content", None) or ""
            if reasoning:
                self.reasoning_text += reasoning
                yield StreamChunk(kind="reasoning", text=reasoning)

            content = delta.content or ""
            if content:
                self.full_text += content
                yield StreamChunk(kind="content", text=content)

            # Tool-call delta accumulation — fragments arrive index-by-index
            for tc_delta in getattr(delta, "tool_calls", None) or []:
                idx: int = tc_delta.index
                if idx not in tool_calls_acc:
                    tool_calls_acc[idx] = {
                        "id": tc_delta.id,
                        "type": "function",
                        "function": {
                            "name": tc_delta.function.name,
                            "arguments": "",
                        },
                    }
                args_fragment: str = tc_delta.function.arguments or ""
                tool_calls_acc[idx]["function"]["arguments"] += args_fragment

            # Capture usage when the provider includes it in the stream
            if hasattr(chunk, "usage") and chunk.usage:
                self.usage = UsageStats(
                    input_tokens=getattr(chunk.usage, "prompt_tokens", 0) or 0,
                    output_tokens=getattr(chunk.usage, "completion_tokens", 0) or 0,
                )

        self.tool_calls = [tool_calls_acc[i] for i in sorted(tool_calls_acc)]


class OllamaCompletionStream(CompletionStream):
    """Wraps an ollama async streaming response with native tool-call support."""

    def __init__(self, response: AsyncIterator[ollama.ChatResponse]) -> None:
        self._ollama_response = response
        self.full_text = ""
        self.reasoning_text = ""
        self.usage: UsageStats = UsageStats()
        self.tool_calls: list[dict[str, Any]] = []

    def __aiter__(self) -> AsyncIterator[StreamChunk]:
        return self._iter_ollama()

    async def _iter_ollama(self) -> AsyncGenerator[StreamChunk, None]:
        async for chunk in self._ollama_response:
            msg = chunk.message

            thinking: str = msg.thinking or ""
            if thinking:
                self.reasoning_text += thinking
                yield StreamChunk(kind="reasoning", text=thinking)

            content: str = msg.content or ""
            if content:
                self.full_text += content
                yield StreamChunk(kind="content", text=content)

            for tc in msg.tool_calls or []:
                args = tc.function.arguments or {}
                self.tool_calls.append(
                    {
                        "id": f"call_{uuid.uuid4().hex[:8]}",
                        "type": "function",
                        "function": {
                            "name": tc.function.name,
                            "arguments": json.dumps(dict(args)),
                        },
                    }
                )
