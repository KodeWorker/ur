#include "bash.hpp"

#include <string>

namespace ur {

BashTool::BashTool(int timeout_seconds) : timeout_seconds_(timeout_seconds) {}

std::string BashTool::description() const {
  return "Execute a shell command and return its stdout and stderr output. "
         "Only available with --allow-all.";
}

std::string BashTool::input_schema_json() const {
  return R"({"type":"object","properties":{"command":{"type":"string","description":"Shell command to execute"}},"required":["command"]})";
}

ToolResult BashTool::execute(const std::string& args_json,
                             const SandboxPolicy& policy) {
  // TODO: implement shell command execution.
  //
  // Steps:
  //   1. Parse args_json; extract "command" field.
  //      On error: return {"invalid args: ...", true}.
  //
  //   2. (policy.allow_all is guaranteed true by Loader filtering, but
  //      double-check as a safety net:)
  //      if (!policy.allow_all) return {"bash requires --allow-all", true};
  //
  //   3. Execute the command with a timeout.
  //      Recommended approach (POSIX): popen() for simplicity in Phase 4.
  //      Timeout enforcement is best-effort — see plan/phase4.md.
  //      Capture stdout + stderr (redirect stderr to stdout in command string,
  //      e.g. append "2>&1", or use separate pipe pairs with fork/exec).
  //
  //   4. Read all output into a string.
  //
  //   5. Check exit code:
  //      - exit 0: return {output, false}
  //      - non-zero: return {"exit " + code + ": " + output, true}
  //
  // Note: pclose() returns the exit status — use WEXITSTATUS() on POSIX.

  (void)args_json;
  (void)policy;
  return {"not implemented", true};
}

}  // namespace ur
