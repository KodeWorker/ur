from __future__ import annotations

from collections.abc import AsyncIterator

from ..config import Settings
from ..llm.client import LLMClient
from .session import AgentSession


async def run(session: AgentSession, settings: Settings) -> AsyncIterator[str]:
    """
    Core agentic loop. Yields tokens as they stream from the LLM.

    Expects session.messages to already contain the latest user message.
    Updates session with the assistant response after each turn.

    Phase 1: single LLM call, no tool use.
    Phase 2+: will iterate up to max_iterations, dispatching tool calls.
    """
    client = LLMClient(settings)

    for _ in range(settings.max_iterations):
        stream = await client.stream(session.messages)

        async for token in stream:
            yield token

        session.add_assistant_message(stream.full_text)
        session.usage.add(stream.usage)

        # Phase 1: no tool use — stop after first response
        # Phase 2 will check for tool_use stop reason and dispatch tools here
        break
