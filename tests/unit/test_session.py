from tests.conftest import TEST_MODEL
from ur.agent.session import AgentSession


def test_new_with_task_adds_user_message():
    s = AgentSession.new(task="hello", model=TEST_MODEL)
    assert len(s.messages) == 1
    assert s.messages[0]["role"] == "user"
    assert s.messages[0]["content"] == "hello"
    assert s.task == "hello"
    assert s.model == TEST_MODEL


def test_new_with_empty_task_has_no_messages():
    s = AgentSession.new(task="", model=TEST_MODEL)
    assert s.messages == []


def test_add_user_message():
    s = AgentSession.new(task="", model=TEST_MODEL)
    s.add_user_message("ping")
    assert s.messages[-1]["role"] == "user"
    assert s.messages[-1]["content"] == "ping"


def test_add_assistant_message():
    s = AgentSession.new(task="hi", model=TEST_MODEL)
    s.add_assistant_message("hello back")
    assert len(s.messages) == 2
    assert s.messages[-1]["role"] == "assistant"
    assert s.messages[-1]["content"] == "hello back"


def test_add_assistant_tool_call_message():
    s = AgentSession.new(task="hi", model=TEST_MODEL)
    s.add_assistant_tool_call_message({"name": "test", "arguments": "{}"})
    assert len(s.messages) == 2
    assert s.messages[-1]["role"] == "assistant"
    assert s.messages[-1]["tool_call"] == {"name": "test", "arguments": "{}"}
    assert s.messages[-1]["content"] is None


def test_add_assistant_tool_call_message_with_content():
    s = AgentSession.new(task="hi", model=TEST_MODEL)
    s.add_assistant_tool_call_message(
        {"name": "test", "arguments": "{}"}, content="hello back"
    )
    assert len(s.messages) == 2
    assert s.messages[-1]["role"] == "assistant"
    assert s.messages[-1]["tool_call"] == {"name": "test", "arguments": "{}"}
    assert s.messages[-1]["content"] == "hello back"


def test_status_transitions():
    s = AgentSession.new(task="t", model=TEST_MODEL)
    assert s.status == "running"

    s.complete()
    assert s.status == "completed"

    s.fail()
    assert s.status == "failed"

    s.interrupt()
    assert s.status == "interrupted"


def test_each_session_has_unique_id():
    a = AgentSession.new(task="t", model=TEST_MODEL)
    b = AgentSession.new(task="t", model=TEST_MODEL)
    assert a.id != b.id


def test_conversation_history_preserved():
    s = AgentSession.new(task="q1", model=TEST_MODEL)
    s.add_assistant_message("a1")
    s.add_user_message("q2")
    s.add_assistant_message("a2")
    assert [m["role"] for m in s.messages] == ["user", "assistant", "user", "assistant"]
