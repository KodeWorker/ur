from __future__ import annotations

import json
from collections.abc import AsyncGenerator
from typing import Any

from ..llm.client import LLMClient
from ..tools.registry import ToolRegistry
from .models import StreamChunk
from .session import AgentSession


async def run(
    session: AgentSession,
    client: LLMClient,
    max_iterations: int,
    system_prompt: str | None = None,
    registry: ToolRegistry | None = None,
) -> AsyncGenerator[StreamChunk, None]:
    """
    Core agentic loop. Yields tokens as they stream from the LLM.

    Expects session.messages to already contain the latest user message.
    Updates session with the assistant response after each turn.

    When a registry is provided, dispatches tool calls and loops until the
    model produces a final text response or max_iterations is exhausted.
    When registry is None, behaves identically to Phase 1 (single LLM call).
    """
    tools_list = registry.as_tools_list() if registry else None
    messages = session.messages
    if system_prompt:
        messages = [{"role": "system", "content": system_prompt}, *messages]

    for _ in range(max_iterations):
        stream = await client.stream(messages, tools=tools_list)

        async for chunk in stream:
            yield chunk

        session.usage.add(stream.usage)

        if stream.has_tool_calls and registry is not None:
            # Record the assistant turn that requested tool calls
            session.add_assistant_tool_call_message(
                stream.tool_calls, content=stream.full_text or None
            )
            # Execute each requested tool and record results
            for tc in stream.tool_calls:
                result = await _execute_tool(registry, tc)
                tc_id: str = tc.get("id") or ""
                tc_name: str = (tc.get("function") or {}).get("name") or ""
                session.add_tool_result_message(tc_id, tc_name, result)
            # Rebuild messages for next iteration
            messages = session.messages
            if system_prompt:
                messages = [{"role": "system", "content": system_prompt}, *messages]
        else:
            # Final text response (or no registry) — record and stop
            session.add_assistant_message(stream.full_text)
            break


async def _execute_tool(registry: ToolRegistry, tool_call: dict[str, Any]) -> str:
    """Execute a single tool call; always returns a string (errors included)."""
    fn_info: dict[str, Any] = tool_call.get("function") or {}
    name: str = fn_info.get("name") or ""
    args_str: str = fn_info.get("arguments") or "{}"
    spec = registry.get(name)
    if spec is None:
        return f"Error: unknown tool '{name}'"
    try:
        args: dict[str, Any] = json.loads(args_str)
        result = await spec.fn(**args)
        return str(result)
    except Exception as e:
        return f"Error executing tool '{name}': {e}"
