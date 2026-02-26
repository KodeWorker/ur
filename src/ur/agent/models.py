from __future__ import annotations

from dataclasses import dataclass
from typing import Any, Literal, NotRequired, TypedDict


class UserMessage(TypedDict):
    role: Literal["user"]
    content: str | list[Any]


class AssistantMessage(TypedDict):
    role: Literal["assistant"]
    content: str | list[Any] | None
    tool_calls: NotRequired[list[Any]]


class ToolMessage(TypedDict):
    role: Literal["tool"]
    tool_call_id: str
    content: str


class SystemMessage(TypedDict):
    role: Literal["system"]
    content: str | list[Any]


Message = UserMessage | AssistantMessage | ToolMessage | SystemMessage


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
