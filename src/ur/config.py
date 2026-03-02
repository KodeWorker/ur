from __future__ import annotations

from functools import lru_cache
from pathlib import Path

from platformdirs import user_data_dir
from pydantic import AliasChoices, Field
from pydantic_settings import BaseSettings, SettingsConfigDict


class Settings(BaseSettings):
    model_config = SettingsConfigDict(
        env_prefix="UR_",
        env_file=".env",
        env_file_encoding="utf-8",
        extra="ignore",
    )

    model: str = Field(default="gemini/gemini-2.5-flash-lite")
    model_think: bool = Field(default=False)
    max_iterations: int = Field(default=20, ge=1)
    data_dir: Path = Field(default_factory=lambda: Path(user_data_dir("ur")))
    log_level: str = Field(default="INFO")

    # Read GEMINI_API_KEY directly (no UR_ prefix) so litellm can also find it
    gemini_api_key: str = Field(
        default="",
        validation_alias=AliasChoices("GEMINI_API_KEY", "UR_GEMINI_API_KEY"),
    )

    # Ollama: base URL of the hosted server (env: UR_OLLAMA_BASE_URL)
    # Use model names like "ollama/llama3.2" or "ollama_chat/qwen2.5"
    ollama_base_url: str = Field(default="http://localhost:11434")

    tool_builtin_truncate_at: int = Field(default=4000)
    tool_builtin_max_lines: int = Field(default=200)
    tool_builtin_max_search_results: int = Field(default=10)

    @property
    def db_path(self) -> Path:
        return self.data_dir / "ur.db"

    @property
    def workspaces_dir(self) -> Path:
        return self.data_dir / "workspaces"

    @property
    def logs_dir(self) -> Path:
        return self.data_dir / "logs"

    @property
    def tools_dir(self) -> Path:
        return self.data_dir / "tools"

    def ensure_dirs(self) -> None:
        self.data_dir.mkdir(parents=True, exist_ok=True)
        self.workspaces_dir.mkdir(parents=True, exist_ok=True)
        self.logs_dir.mkdir(parents=True, exist_ok=True)
        self.tools_dir.mkdir(parents=True, exist_ok=True)

@lru_cache(maxsize=1)
def get_settings() -> Settings:
    return Settings()
