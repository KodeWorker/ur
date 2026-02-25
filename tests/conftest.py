"""Shared fixtures for all tests."""
from __future__ import annotations

import os
from pathlib import Path
from unittest.mock import MagicMock

import pytest

from ur.config import Settings

# Read from env so values can be overridden per run (set in [tool.pytest.ini_options] env)
TEST_MODEL = os.environ.get("UR_MODEL", "gemini/gemini-test")
TEST_API_KEY = os.environ.get("GEMINI_API_KEY", "gm-test-key")

# Provider-specific skip markers — decorate tests that only apply to one provider.
# Switch provider with:  UR_MODEL=ollama_chat/qwen2.5 pytest
skip_if_not_gemini = pytest.mark.skipif(
    not TEST_MODEL.startswith("gemini"),
    reason=f"Gemini-only test (current UR_MODEL={TEST_MODEL!r})",
)
skip_if_not_ollama = pytest.mark.skipif(
    not TEST_MODEL.startswith("ollama"),
    reason=f"Ollama-only test (current UR_MODEL={TEST_MODEL!r})",
)


# ── Settings fixture ──────────────────────────────────────────────────────────

@pytest.fixture
def tmp_settings(tmp_path: Path) -> Settings:
    """Settings wired to a temp directory so no real files are touched."""
    return Settings(
        model=TEST_MODEL,
        gemini_api_key=TEST_API_KEY,
        max_iterations=5,
        data_dir=tmp_path / "ur_data",
    )


@pytest.fixture
def db_path(tmp_path: Path) -> Path:
    return tmp_path / "test.db"


# ── litellm chunk helpers ─────────────────────────────────────────────────────

def make_chunk(content: str | None, usage: dict | None = None) -> MagicMock:
    """Build a mock litellm streaming chunk."""
    chunk = MagicMock()
    chunk.choices[0].delta.content = content
    if usage:
        chunk.usage = MagicMock(
            prompt_tokens=usage.get("prompt_tokens", 0),
            completion_tokens=usage.get("completion_tokens", 0),
        )
    else:
        chunk.usage = None
    return chunk


class MockStreamWrapper:
    """Async-iterable stand-in for litellm.CustomStreamWrapper."""

    def __init__(self, chunks: list) -> None:
        self._chunks = chunks

    def __aiter__(self):
        return self._iter()

    async def _iter(self):
        for chunk in self._chunks:
            yield chunk
