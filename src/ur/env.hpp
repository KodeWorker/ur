#pragma once

#include <filesystem>
#include <map>
#include <string>

namespace ur {

// Load variables from a .env file and set them in the process environment.
// Each non-blank, non-comment line must be KEY=VALUE.
// Surrounding single or double quotes are stripped from the value.
// Always overwrites existing environment variables.
// Does nothing if the file does not exist.
void load_dotenv(const std::filesystem::path& path = ".env");

// Write key=value pairs into a .env file, preserving existing lines that do
// not match any of the provided keys. Keys not already present are appended.
// Creates the file if it does not exist.
void save_dotenv(const std::filesystem::path& path,
                 const std::map<std::string, std::string>& vars);

int env_int(const char* name, int fallback);

}  // namespace ur
