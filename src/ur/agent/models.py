from __future__ import annotations

from dataclasses import dataclass
from typing import Any

# Messages are plain dicts in litellm/OpenAI format:
#   {"role": "user"|"assistant"|"system", "content": str | list}
Message = dict[str, Any]


@dataclass
class UsageStats:
    input_tokens: int = 0
    output_tokens: int = 0

    @property
    def total_tokens(self) -> int:
        return self.input_tokens + self.output_tokens

    def add(self, other: UsageStats | None) -> None:
        if other:
            self.input_tokens += other.input_tokens
            self.output_tokens += other.output_tokens
