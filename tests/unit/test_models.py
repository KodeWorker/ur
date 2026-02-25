from ur.agent.models import UsageStats


def test_defaults():
    stats = UsageStats()
    assert stats.input_tokens == 0
    assert stats.output_tokens == 0


def test_total_tokens():
    stats = UsageStats(input_tokens=100, output_tokens=50)
    assert stats.total_tokens == 150


def test_add():
    a = UsageStats(input_tokens=10, output_tokens=5)
    b = UsageStats(input_tokens=20, output_tokens=15)
    a.add(b)
    assert a.input_tokens == 30
    assert a.output_tokens == 20


def test_add_none_is_noop():
    stats = UsageStats(input_tokens=7, output_tokens=3)
    stats.add(None)
    assert stats.input_tokens == 7
    assert stats.output_tokens == 3


def test_add_zero():
    stats = UsageStats(input_tokens=5, output_tokens=5)
    stats.add(UsageStats())
    assert stats.total_tokens == 10
