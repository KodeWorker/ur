# Plan: Plugin System for Custom Tools

## Context

The README already advertises "drop a .py file in `~/.ur/tools/` to add a
custom tool" but the feature was not implemented. This plan delivers it.

---

## Files changed

| File | Change |
|------|--------|
| `src/ur/config.py` | Add `tools_dir: Path` field (env: `UR_TOOLS_DIR`) |
| `src/ur/tools/plugins.py` | New module — plugin loader |
| `src/ur/tui.py` | Call `load_plugins()` in `_make_registry()` |
| `tests/unit/test_tools.py` | Seven new plugin loader tests |

---

## Plugin format

```python
# ~/.ur/tools/my_tool.py
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
```

- Tool functions must be **async** and return `str`
- Only the `register(registry)` function is called by the loader
- Module-level code runs on import; keep side-effects minimal

---

## Loader behaviour

- Scans `tools_dir` for `*.py` files in **alphabetical order**
- **Last writer wins** on name collisions — plugins can intentionally override builtins
- Three independent guard rails, each logs a warning and skips the file:
  1. Import / syntax error
  2. Missing `register` attribute
  3. `register()` raises
- If `tools_dir` does not exist, returns immediately (no error)
- Plugins load **after** builtins so they can replace any builtin tool

---

## Configuration

| Setting | Default | Env override |
|---------|---------|--------------|
| `tools_dir` | `~/.ur/tools/` | `UR_TOOLS_DIR` |

---

## Status

Implemented. All tests pass.
