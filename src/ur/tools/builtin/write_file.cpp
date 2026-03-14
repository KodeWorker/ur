#include "write_file.hpp"

#include <string>

#include "tools/sandbox.hpp"

namespace ur {

std::string WriteFileTool::description() const {
  return "Create or overwrite a file inside the workspace. "
         "Parent directories are created automatically if they do not exist.";
}

std::string WriteFileTool::input_schema_json() const {
  return R"({"type":"object","properties":{"path":{"type":"string","description":"Path to the file, relative to the workspace root"},"content":{"type":"string","description":"Text content to write"}},"required":["path","content"]})";
}

ToolResult WriteFileTool::execute(const std::string& args_json,
                                  const SandboxPolicy& policy) {
  // TODO: implement file write.
  //
  // Steps:
  //   1. Parse args_json; extract "path" and "content" fields.
  //      On error: return {"invalid args: ...", true}.
  //
  //   2. Resolve path relative to policy.workspace_root if not absolute.
  //
  //   3. auto err = Sandbox::validate(path, policy);
  //      if (!err.empty()) return {err, true};
  //
  //   4. std::filesystem::create_directories(path.parent_path())
  //      On failure: return {"cannot create directories: ...", true}.
  //
  //   5. Open std::ofstream; write content.
  //      On failure: return {"cannot write file: " + path, true}.
  //
  //   6. return {"written: " + path.string(), false};

  (void)args_json;
  (void)policy;
  return {"not implemented", true};
}

}  // namespace ur
