from __future__ import annotations

import os
from collections.abc import AsyncIterator
from enum import StrEnum
from typing import Any

import litellm

from ..agent.models import Message, StreamChunk, UsageStats
from ..config import Settings

# Suppress litellm's verbose startup output
litellm.suppress_debug_info = True
os.environ.setdefault("LITELLM_LOG", "ERROR")


class Provider(StrEnum):
    GEMINI = "gemini"
    OLLAMA = "ollama"
    OTHER = "other"


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
    _INTERNAL_KEYS: frozenset[str] = frozenset({"created_at"})

    async def stream(self, messages: list[Message]) -> CompletionStream:
        api_messages = [
            {k: v for k, v in msg.items() if k not in self._INTERNAL_KEYS}
            for msg in messages
        ]
        kwargs: dict[str, Any] = dict(
            model=self.model,
            messages=api_messages,
            stream=True,
        )
        if self.provider == Provider.GEMINI and self.settings.gemini_api_key:
            kwargs["api_key"] = self.settings.gemini_api_key
        elif self.provider == Provider.OLLAMA:
            kwargs["api_base"] = self.settings.ollama_base_url

        response = await litellm.acompletion(**kwargs)
        return CompletionStream(response)


class CompletionStream:
    """Wraps a litellm streaming response. Iterate to get tokens; usage is set after."""

    def __init__(self, response: litellm.CustomStreamWrapper) -> None:
        self._response = response
        self.full_text: str = ""  # content only — stored in session messages
        self.reasoning_text: str = ""  # thinking tokens — display only
        self.usage: UsageStats = UsageStats()

    def __aiter__(self) -> AsyncIterator[StreamChunk]:
        return self._iter()

    async def _iter(self) -> AsyncIterator[StreamChunk]:
        async for chunk in self._response:
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

            # Capture usage when the provider includes it in the stream
            if hasattr(chunk, "usage") and chunk.usage:
                self.usage = UsageStats(
                    input_tokens=getattr(chunk.usage, "prompt_tokens", 0) or 0,
                    output_tokens=getattr(chunk.usage, "completion_tokens", 0) or 0,
                )
