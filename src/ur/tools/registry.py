from __future__ import annotations

from collections.abc import Awaitable, Callable
from dataclasses import dataclass
from typing import Any


@dataclass
class ToolSpec:
    fn: Callable[..., Awaitable[str]]
    schema: dict[str, Any]


class ToolRegistry:
    """Registry mapping tool names to async callables and their JSON schemas.

    Public interface: register, get, as_tools_list, names, enable, disable,
    is_enabled, __contains__. Tool dispatch is handled by loop._execute_tool.
    """

    def __init__(self) -> None:
        self._tools: dict[str, ToolSpec] = {}
        self._disabled: set[str] = set()

    def names(self) -> list[str]:
        """Return all registered tool names in registration order."""
        return list(self._tools)

    def enable(self, name: str) -> None:
        """Re-enable a previously disabled tool. No-op if name is unknown."""
        self._disabled.discard(name)

    def disable(self, name: str) -> None:
        """Disable a tool; excluded from as_tools_list() until re-enabled."""
        if name in self._tools:
            self._disabled.add(name)

    def is_enabled(self, name: str) -> bool:
        return name in self._tools and name not in self._disabled

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
        """Return the litellm-compatible tools array, excluding disabled tools."""
        return [
            spec.schema
            for name, spec in self._tools.items()
            if name not in self._disabled
        ]

    def get(self, name: str) -> ToolSpec | None:
        return self._tools.get(name)

    def __contains__(self, name: object) -> bool:
        return name in self._tools
