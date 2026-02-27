# Issue #10: Message Serialization — Analysis & Fix Plan

## Root cause
`session_store.py` lines 37–41 use `str(msg["content"])` for non-string content.
This produces Python repr, not JSON. Round-trip is broken for list content and None.

## Schema gaps for Phase 2
`messages` table has no `tool_calls`, `tool_call_id`, or `name` columns.
Tool-call data is silently dropped on every save.

## Recommended TypedDict variants (models.py)
```python
ContentBlock = dict[str, Any]
MessageContent = str | list[ContentBlock] | None

class UserMessage(TypedDict):
    role: Literal["user"]
    content: str | list[ContentBlock]

class AssistantMessage(TypedDict, total=False):
    role: Required[Literal["assistant"]]
    content: str | list[ContentBlock] | None
    tool_calls: list[dict[str, Any]]

class ToolMessage(TypedDict):
    role: Literal["tool"]
    tool_call_id: str
    content: str

class SystemMessage(TypedDict):
    role: Literal["system"]
    content: str

Message = UserMessage | AssistantMessage | ToolMessage | SystemMessage
```

## Serialization rule
Always `json.dumps(content)` on write; always `json.loads(content)` on read.
No isinstance branching. `content TEXT NOT NULL DEFAULT 'null'` in schema.

## Schema additions (db.py)
```sql
tool_calls    TEXT,         -- JSON list | NULL
tool_call_id  TEXT,         -- for role='tool'
name          TEXT,         -- tool name
```
Use ALTER TABLE + PRAGMA table_info check in init_db for migration.

## created_at cleanup
Remove from message dicts in session.py (add_user_message, add_assistant_message, new).
Source timestamp at persistence boundary in save_session only.
Strip created_at in _deserialize_message so wire-format dicts are clean.

## Fix sequence
1. Remove created_at from message dicts in session.py
2. Add TypedDicts to models.py; update Message alias
3. ALTER TABLE for new columns in db.py init_db
4. Implement _serialize_message / _deserialize_message in session_store.py
5. Add round-trip tests for AssistantMessage with tool_calls and ToolMessage
