"""Plugin loader for user-defined tools.

Drop a .py file in the tools directory (Settings.tools_dir, derived from
data_dir) and
define a top-level register() function:

    from ur.tools.registry import ToolRegistry

    async def my_tool(input: str) -> str:
        return f"result: {input}"

    def register(registry: ToolRegistry) -> None:
        registry.register(
            name="my_tool",
            description="Does something custom",
            parameters={
                "type": "object",
                "properties": {"input": {"type": "string"}},
                "required": ["input"],
            },
            fn=my_tool,
        )

Tool functions must be async and return str.
Plugins are loaded alphabetically after builtins; last writer wins on name
collisions, so a plugin can intentionally override a builtin.
"""

from __future__ import annotations

import importlib.util
import logging
import sys
from pathlib import Path
from types import ModuleType

from .registry import ToolRegistry

logger = logging.getLogger(__name__)


def load_plugins(registry: ToolRegistry, tools_dir: Path) -> None:
    """Scan *tools_dir* for *.py files and call register(registry) in each.

    Safe to call when tools_dir does not exist — returns immediately.
    Files are processed in alphabetical order for deterministic load sequence.
    """
    if not tools_dir.is_dir():
        return

    for path in sorted(tools_dir.glob("*.py")):
        _load_one(registry, path)


def _load_one(registry: ToolRegistry, path: Path) -> None:
    module_name = f"ur._plugin.{path.stem}"
    try:
        spec = importlib.util.spec_from_file_location(module_name, path)
        if spec is None or spec.loader is None:
            logger.warning("Plugin %s: could not build module spec — skipped", path)
            return
        module: ModuleType = importlib.util.module_from_spec(spec)
        sys.modules[module_name] = module
        spec.loader.exec_module(module)
    except Exception:
        logger.warning("Plugin %s: import failed — skipped", path, exc_info=True)
        sys.modules.pop(module_name, None)
        return

    register_fn = getattr(module, "register", None)
    if register_fn is None:
        logger.warning("Plugin %s: no register() function — skipped", path)
        return
    if not callable(register_fn):
        logger.warning("Plugin %s: register is not callable — skipped", path)
        return

    try:
        register_fn(registry)
    except Exception:
        logger.warning("Plugin %s: register() raised — skipped", path, exc_info=True)
