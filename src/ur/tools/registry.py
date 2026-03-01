from __future__ import annotations

from collections.abc import Awaitable, Callable
from dataclasses import dataclass
from typing import Any


@dataclass
class ToolSpec:
    fn: Callable[..., Awaitable[str]]
    schema: dict[str, Any]


class ToolRegistry:
    """Registry mapping tool names to async callables and their JSON schemas."""

    def __init__(self) -> None:
        self._tools: dict[str, ToolSpec] = {}

    def register(
        self,
        name: str,
        description: str,
        parameters: dict[str, Any],
        fn: Callable[..., Awaitable[str]],
    ) -> None:
        self._tools[name] = ToolSpec(
            fn=fn,
            schema={
                "type": "function",
                "function": {
                    "name": name,
                    "description": description,
                    "parameters": parameters,
                },
            },
        )

    def as_tools_list(self) -> list[dict[str, Any]]:
        """Return the litellm-compatible tools array."""
        return [spec.schema for spec in self._tools.values()]

    def get(self, name: str) -> ToolSpec | None:
        return self._tools.get(name)

    def __contains__(self, name: object) -> bool:
        return name in self._tools
