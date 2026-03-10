#pragma once

#include <filesystem>

namespace ur {

// Load variables from a .env file and set them in the process environment.
// Each non-blank, non-comment line must be KEY=VALUE.
// Surrounding single or double quotes are stripped from the value.
// Always overwrites existing environment variables.
// Does nothing if the file does not exist.
void load_dotenv(const std::filesystem::path& path = ".env");

}  // namespace ur
