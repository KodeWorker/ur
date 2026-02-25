from __future__ import annotations

import os
from collections.abc import AsyncIterator
from enum import Enum

import litellm

from ..agent.models import Message, UsageStats
from ..config import Settings

# Suppress litellm's verbose startup output
litellm.suppress_debug_info = True
os.environ.setdefault("LITELLM_LOG", "ERROR")


class Provider(str, Enum):
    GEMINI = "gemini"
    OLLAMA = "ollama"
    OTHER = "other"


class LLMClient:
    def __init__(self, settings: Settings) -> None:
        self.settings = settings
        self.provider = self._detect_provider(settings.model)

    @staticmethod
    def _detect_provider(model: str) -> Provider:
        if model.startswith("gemini"):
            return Provider.GEMINI
        if model.startswith("ollama"):
            return Provider.OLLAMA
        return Provider.OTHER

    async def stream(self, messages: list[Message]) -> CompletionStream:
        kwargs: dict = dict(
            model=self.settings.model,
            messages=messages,
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
        self.full_text: str = ""
        self.usage: UsageStats = UsageStats()

    def __aiter__(self) -> AsyncIterator[str]:
        return self._iter()

    async def _iter(self) -> AsyncIterator[str]:
        async for chunk in self._response:
            delta = chunk.choices[0].delta.content or ""
            if delta:
                self.full_text += delta
                yield delta

            # Capture usage when the provider includes it in the stream
            if hasattr(chunk, "usage") and chunk.usage:
                self.usage = UsageStats(
                    input_tokens=getattr(chunk.usage, "prompt_tokens", 0) or 0,
                    output_tokens=getattr(chunk.usage, "completion_tokens", 0) or 0,
                )
