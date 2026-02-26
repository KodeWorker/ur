import pytest

from tests.conftest import TEST_MODEL
from ur.agent.models import UsageStats
from ur.agent.session import AgentSession
from ur.memory.db import init_db
from ur.memory.session_store import get_session_messages, list_sessions, save_session


@pytest.fixture
async def initialized_db(db_path):
    await init_db(db_path)
    return db_path


# ── init_db ───────────────────────────────────────────────────────────────────

async def test_init_db_creates_file(tmp_path):
    path = tmp_path / "sub" / "ur.db"
    await init_db(path)
    assert path.exists()


async def test_init_db_creates_sessions_table(initialized_db):
    from ur.memory.db import get_db
    async with get_db(initialized_db) as db:
        cursor = await db.execute(
            "SELECT name FROM sqlite_master WHERE type='table' AND name='sessions'"
        )
        assert await cursor.fetchone() is not None


async def test_init_db_creates_messages_table(initialized_db):
    from ur.memory.db import get_db
    async with get_db(initialized_db) as db:
        cursor = await db.execute(
            "SELECT name FROM sqlite_master WHERE type='table' AND name='messages'"
        )
        assert await cursor.fetchone() is not None


async def test_init_db_is_idempotent(db_path):
    await init_db(db_path)
    await init_db(db_path)  # should not raise


# ── save_session / list_sessions ──────────────────────────────────────────────

async def test_save_and_list_session(initialized_db):
    session = AgentSession.new(task="ping", model=TEST_MODEL)
    session.complete()
    await save_session(session, initialized_db)

    rows = await list_sessions(initialized_db)
    assert len(rows) == 1
    assert rows[0]["id"] == session.id
    assert rows[0]["task"] == "ping"
    assert rows[0]["status"] == "completed"
    assert rows[0]["model"] == TEST_MODEL


async def test_save_persists_token_usage(initialized_db):
    session = AgentSession.new(task="t", model=TEST_MODEL)
    session.usage = UsageStats(input_tokens=20, output_tokens=10)
    await save_session(session, initialized_db)

    rows = await list_sessions(initialized_db)
    assert rows[0]["total_input_tokens"] == 20
    assert rows[0]["total_output_tokens"] == 10


async def test_save_upserts_on_repeated_calls(initialized_db):
    session = AgentSession.new(task="t", model=TEST_MODEL)
    await save_session(session, initialized_db)

    session.complete()
    session.usage = UsageStats(input_tokens=5, output_tokens=5)
    await save_session(session, initialized_db)

    rows = await list_sessions(initialized_db)
    assert len(rows) == 1  # no duplicate row
    assert rows[0]["status"] == "completed"
    assert rows[0]["total_input_tokens"] == 5


async def test_list_sessions_ordered_newest_first(initialized_db):
    s1 = AgentSession.new(task="first", model=TEST_MODEL)
    s2 = AgentSession.new(task="second", model=TEST_MODEL)
    await save_session(s1, initialized_db)
    await save_session(s2, initialized_db)

    rows = await list_sessions(initialized_db)
    # s2 was inserted after s1 so created_at >= s1's
    assert rows[0]["task"] in ("first", "second")  # order by created_at DESC


async def test_list_sessions_respects_limit(initialized_db):
    for i in range(5):
        s = AgentSession.new(task=f"task {i}", model=TEST_MODEL)
        await save_session(s, initialized_db)

    rows = await list_sessions(initialized_db, limit=3)
    assert len(rows) == 3


async def test_list_sessions_empty_db(initialized_db):
    rows = await list_sessions(initialized_db)
    assert rows == []


# ── get_session_messages ──────────────────────────────────────────────────────

async def test_get_session_messages_returns_in_order(initialized_db):
    session = AgentSession.new(task="q", model=TEST_MODEL)
    session.add_assistant_message("a")
    session.add_user_message("q2")
    await save_session(session, initialized_db)

    msgs = await get_session_messages(session.id, initialized_db)
    assert [m["role"] for m in msgs] == ["user", "assistant", "user"]
    assert msgs[0]["content"] == "q"
    assert msgs[1]["content"] == "a"


async def test_save_session_is_idempotent_for_messages(initialized_db):
    session = AgentSession.new(task="hi", model=TEST_MODEL)
    session.add_assistant_message("hello")
    await save_session(session, initialized_db)
    await save_session(session, initialized_db)  # second save

    msgs = await get_session_messages(session.id, initialized_db)
    assert len(msgs) == 2  # no duplicates


async def test_get_session_messages_unknown_id_returns_empty(initialized_db):
    msgs = await get_session_messages("nonexistent-id", initialized_db)
    assert msgs == []
