#include "read_file.hpp"

#include <string>

#include "tools/sandbox.hpp"

namespace ur {

std::string ReadFileTool::description() const {
  return "Read the contents of a file inside the workspace. "
         "Returns the file content as a string.";
}

std::string ReadFileTool::input_schema_json() const {
  return R"({"type":"object","properties":{"path":{"type":"string","description":"Path to the file, relative to the workspace root"}},"required":["path"]})";
}

ToolResult ReadFileTool::execute(const std::string& args_json,
                                 const SandboxPolicy& policy) {
  // TODO: implement file read.
  //
  // Steps:
  //   1. Parse args_json with nlohmann::json; extract "path" field.
  //      On parse error or missing field: return {"invalid args: ...", true}.
  //
  //   2. Resolve path relative to policy.workspace_root if not absolute.
  //
  //   3. auto err = Sandbox::validate(path, policy);
  //      if (!err.empty()) return {err, true};
  //
  //   4. Open file with std::ifstream; read entire content.
  //      On open failure: return {"cannot open file: " + path, true}.
  //
  //   5. return {content, false};

  (void)args_json;
  (void)policy;
  return {"not implemented", true};
}

}  // namespace ur
