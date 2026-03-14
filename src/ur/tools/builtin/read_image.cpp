#include "read_image.hpp"

#include <string>

#include "tools/sandbox.hpp"

namespace ur {

std::string ReadImageTool::description() const {
  return "Validate and return an image file path for use by a vision-capable "
         "model. "
         "The path must be inside the workspace. "
         "Only available with --allow-all and a vision-capable model.";
}

std::string ReadImageTool::input_schema_json() const {
  return R"({"type":"object","properties":{"path":{"type":"string","description":"Path to the image file (JPEG, PNG, etc.), relative to the workspace root"}},"required":["path"]})";
}

ToolResult ReadImageTool::execute(const std::string& args_json,
                                  const SandboxPolicy& policy) {
  // TODO: implement image path validation.
  //
  // Steps:
  //   1. Parse args_json; extract "path" field.
  //      On error: return {"invalid args: ...", true}.
  //
  //   2. Resolve path relative to policy.workspace_root if not absolute.
  //
  //   3. auto err = Sandbox::validate(path, policy);
  //      if (!err.empty()) return {err, true};
  //
  //   4. Check the file exists and is a regular file:
  //      if (!std::filesystem::is_regular_file(path))
  //        return {"not a file: " + path.string(), true};
  //
  //   5. Optionally validate extension (.jpg, .jpeg, .png, .webp, .gif).
  //      Log a warning but do not reject — the model decides what it accepts.
  //
  //   6. return {path.string(), false};
  //      (The absolute path string is what the LLM passes to the server.)

  (void)args_json;
  (void)policy;
  return {"not implemented", true};
}

}  // namespace ur
