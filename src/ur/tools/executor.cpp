#include "executor.hpp"

#include <string>
#include <vector>

#include "loader.hpp"
#include "log/logger.hpp"
#include "memory/crypto.hpp"
#include "memory/database.hpp"

namespace ur {

Executor::Executor(Loader& loader, const SandboxPolicy& policy, Database& db,
                   Logger& logger)
    : loader_(loader), policy_(policy), db_(db), logger_(logger) {}

std::vector<ToolResult> Executor::execute_all(
    const std::vector<ToolCall>& calls, const std::string& session_id,
    std::function<bool(const ToolCall&)> audit_cb) {
  std::vector<ToolResult> results;
  results.reserve(calls.size());

  for (const auto& call : calls) {
    // TODO: implement per-call routing.
    //
    // Steps for each call:
    //
    // 1. Resolve tool:
    //      Tool* tool = loader_.find(call.name);
    //      If nullptr: results.push_back({"unknown tool: " + call.name, true});
    //                  continue;
    //
    // 2. Human audit (skip when policy_.allow_all):
    //      bool approved = policy_.allow_all || audit_cb(call);
    //      If !approved:
    //        persist_audit(session_id, "rejected: " + call.name + " | " +
    //        call.args_json); results.push_back({"rejected by user", true});
    //        continue;
    //      persist_audit(session_id, "approved: " + call.name + " | " +
    //      call.args_json);
    //
    // 3. Execute:
    //      results.push_back(execute_one(tool, call));
    //
    // Errors from execute_one() are already wrapped in ToolResult{msg, true}
    // by the tool — no additional try/catch needed here.

    (void)call;
    (void)session_id;
    (void)audit_cb;
    results.push_back({"not implemented", true});
  }

  return results;
}

ToolResult Executor::execute_one(Tool* tool, const ToolCall& call) {
  // TODO: call tool->execute() and log the outcome.
  //
  // Steps:
  //   1. logger_.info("executing tool: " + call.name);
  //   2. ToolResult result = tool->execute(call.args_json, policy_);
  //   3. if (result.is_error)
  //        logger_.error("tool " + call.name + " failed: " + result.content);
  //      else
  //        logger_.info("tool " + call.name + " ok");
  //   4. return result;

  (void)tool;
  (void)call;
  return {"not implemented", true};
}

void Executor::persist_audit(const std::string& session_id,
                             const std::string& content) {
  // TODO: persist a tool_audit message to the database.
  //
  // Steps:
  //   const int64_t now = static_cast<int64_t>(std::time(nullptr));
  //   db_.insert_message(generate_id(), session_id, "tool_audit", content,
  //   now);
  //
  // Wrap in try/catch — audit persistence failure must not abort execution.

  (void)session_id;
  (void)content;
}

}  // namespace ur
