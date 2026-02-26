from __future__ import annotations

import uuid
from dataclasses import dataclass, field
from datetime import datetime, timezone
from typing import Literal

from .models import Message, UsageStats


@dataclass
class AgentSession:
    id: str = field(default_factory=lambda: str(uuid.uuid4()))
    task: str = ""
    messages: list[Message] = field(default_factory=list)
    created_at: datetime = field(default_factory=lambda: datetime.now(timezone.utc))
    status: Literal["running", "completed", "failed", "interrupted"] = "running"
    usage: UsageStats = field(default_factory=UsageStats)
    model: str = ""

    @classmethod
    def new(cls, task: str, model: str) -> AgentSession:
        session = cls(task=task, model=model)
        if task:
            session.messages.append({"role": "user", "content": task})
        return session

    def add_user_message(self, content: str) -> None:
        self.messages.append({"role": "user", "content": content})

    def add_assistant_message(self, content: str) -> None:
        self.messages.append({"role": "assistant", "content": content})

    def complete(self) -> None:
        self.status = "completed"

    def fail(self) -> None:
        self.status = "failed"

    def interrupt(self) -> None:
        self.status = "interrupted"
