from __future__ import annotations

import uuid
from dataclasses import dataclass, field
from datetime import UTC, datetime
from typing import Any, Literal

from .models import Message, UsageStats


def _now_iso() -> str:
    return datetime.now(UTC).isoformat()


@dataclass
class AgentSession:
    id: str = field(default_factory=lambda: str(uuid.uuid4()))
    task: str = ""
    messages: list[Message] = field(default_factory=list)
    created_at: datetime = field(default_factory=lambda: datetime.now(UTC))
    status: Literal["running", "completed", "failed", "interrupted"] = "running"
    usage: UsageStats = field(default_factory=UsageStats)
    model: str = ""

    @classmethod
    def new(cls, task: str, model: str) -> AgentSession:
        session = cls(task=task, model=model)
        if task:
            session.messages.append(
                {"role": "user", "content": task, "created_at": _now_iso()}
            )
        return session

    def add_user_message(self, content: str) -> None:
        self.messages.append(
            {"role": "user", "content": content, "created_at": _now_iso()}
        )

    def add_assistant_message(self, content: str) -> None:
        self.messages.append(
            {"role": "assistant", "content": content, "created_at": _now_iso()}
        )

    def add_assistant_tool_call_message(
        self, tool_calls: list[dict[str, Any]], content: str | None = None
    ) -> None:
        self.messages.append(
            {
                "role": "assistant",
                "tool_calls": tool_calls,
                "content": content,
                "created_at": _now_iso(),
            }
        )

    def add_tool_result_message(
        self, tool_call_id: str, name: str, content: str
    ) -> None:
        self.messages.append(
            {
                "role": "tool",
                "tool_call_id": tool_call_id,
                "name": name,
                "content": content,
                "created_at": _now_iso(),
            }
        )

    def complete(self) -> None:
        self.status = "completed"

    def fail(self) -> None:
        self.status = "failed"

    def interrupt(self) -> None:
        self.status = "interrupted"
