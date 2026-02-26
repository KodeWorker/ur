from pathlib import Path
from unittest.mock import patch

import pytest

import ur.config as config_module
from ur.config import Settings, get_settings
from tests.conftest import skip_if_not_gemini, skip_if_not_ollama


@pytest.fixture(autouse=True)
def reset_settings_singleton():
    """Ensure the module-level singleton is cleared between tests."""
    config_module._settings = None
    yield
    config_module._settings = None


def test_defaults(tmp_path):
    s = Settings(data_dir=tmp_path)
    assert s.model == "gemini/gemini-2.5-flash-lite"
    assert s.max_iterations == 20
    assert s.log_level == "INFO"
    assert s.gemini_api_key == "gm-test-key"


def test_db_path_property(tmp_path):
    s = Settings(data_dir=tmp_path)
    assert s.db_path == tmp_path / "ur.db"


def test_workspaces_dir_property(tmp_path):
    s = Settings(data_dir=tmp_path)
    assert s.workspaces_dir == tmp_path / "workspaces"


def test_logs_dir_property(tmp_path):
    s = Settings(data_dir=tmp_path)
    assert s.logs_dir == tmp_path / "logs"


def test_ensure_dirs_creates_directories(tmp_path):
    s = Settings(data_dir=tmp_path / "new_dir")
    s.ensure_dirs()
    assert s.data_dir.is_dir()
    assert s.workspaces_dir.is_dir()
    assert s.logs_dir.is_dir()


def test_ensure_dirs_is_idempotent(tmp_path):
    s = Settings(data_dir=tmp_path)
    s.ensure_dirs()
    s.ensure_dirs()  # should not raise


def test_model_override(tmp_path):
    s = Settings(data_dir=tmp_path, model="openai/gpt-4o")
    assert s.model == "openai/gpt-4o"


def test_get_settings_returns_singleton(tmp_path):
    with patch("ur.config.Settings", return_value=Settings(data_dir=tmp_path)):
        first = get_settings()
        second = get_settings()
    assert first is second


@skip_if_not_gemini
def test_gemini_api_key_from_env(tmp_path, monkeypatch):
    monkeypatch.setenv("GEMINI_API_KEY", "gm-live-key")
    s = Settings(data_dir=tmp_path)
    assert s.gemini_api_key == "gm-live-key"


@skip_if_not_ollama
def test_ollama_base_url_default(tmp_path):
    s = Settings(data_dir=tmp_path)
    assert s.ollama_base_url == "http://localhost:11434"


@skip_if_not_ollama
def test_ollama_base_url_override(tmp_path, monkeypatch):
    monkeypatch.setenv("UR_OLLAMA_BASE_URL", "http://my-server:11434")
    s = Settings(data_dir=tmp_path)
    assert s.ollama_base_url == "http://my-server:11434"
