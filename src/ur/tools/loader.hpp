#pragma once

#include <memory>
#include <string>
#include <unordered_map>
#include <vector>

#include "cli/context.hpp"
#include "tool.hpp"

namespace ur {

// ---------------------------------------------------------------------------
// Loader — discovers, instantiates, and indexes all available tools.
//
// Lifetime: constructed once in main() (or cmd_run / cmd_chat), lives for
// the duration of the agent run.  All Tool* pointers it returns are valid
// for the Loader's lifetime.
//
// Loading order (later entries override earlier ones on name collision):
//   1. Built-ins registered programmatically in register_builtins()
//   2. Shared-library plugins (*.so / *.dll) from $root/tools/
//   3. Script plugins (executable files) from $root/tools/
//
// Filtering (applied after registration):
//   - tools.json "enabled": false  → excluded
//   - requires_allow_all() && !allow_all  → silently excluded
//   - no_tools  → all excluded
//   - allow_list  → keep only listed names
//   - deny_list   → remove listed names
// ---------------------------------------------------------------------------
class Loader {
 public:
  // paths: provides workspace_root (sandbox) and tools_dir ($root/tools/).
  explicit Loader(const Paths& paths);
  ~Loader();  // dlclose all plugin handles in reverse load order

  // Non-copyable — owns dlopen handles and unique_ptrs.
  Loader(const Loader&) = delete;
  Loader& operator=(const Loader&) = delete;

  // Load tools.json, register built-ins, discover and load plugins,
  // then apply all filters.  Call once before any find() / active_tools().
  //
  // allow_list: comma-separated tool names to keep (empty = keep all)
  // deny_list:  comma-separated tool names to remove (empty = remove none)
  // no_tools:   if true, active_tools() returns empty regardless of manifest
  // allow_all:  if false, tools with requires_allow_all() are excluded
  void load(const std::string& allow_list, const std::string& deny_list,
            bool no_tools, bool allow_all);

  // Look up a tool by name.  Returns nullptr if not found or not active.
  // Called by Executor for each ToolCall in an LLM response.
  Tool* find(const std::string& name) const;

  // All active tools after filtering.
  // Used by Executor to build the "tools" array injected into LLM requests.
  const std::vector<Tool*>& active_tools() const;

 private:
  // Register all compiled-in built-in tools into tools_ and index_.
  // Called first inside load(), before plugin discovery.
  void register_builtins();

  // Parse $root/tools/tools.json (if present) and apply enabled flags
  // and per-tool config (timeout, max_results, …) to registered tools.
  // Missing file is silently ignored — defaults apply.
  void apply_manifest();

  // Scan $root/tools/ for shared-library plugins (*.so / *.dll).
  // For each, dlopen and resolve ur_create_tool / ur_destroy_tool.
  // A plugin whose name collides with an existing entry replaces it.
  void load_shared_plugins();

  // Scan $root/tools/ for executable script plugins (non-library executables).
  // Run each with --describe to obtain name/description/schema.
  // Override semantics same as shared plugins.
  void load_script_plugins();

  // Remove tools that do not pass the active filters (allow_all, allow/deny
  // lists, no_tools).  Populates active_tools_.
  void apply_filters(const std::string& allow_list,
                     const std::string& deny_list, bool no_tools,
                     bool allow_all);

  const Paths& paths_;

  // All registered tools (built-ins + plugins), in registration order.
  // Indexed by name for O(1) lookup; values owned here.
  std::vector<std::unique_ptr<Tool>> tools_;
  std::unordered_map<std::string, Tool*> index_;

  // Subset of index_ that passed all filters — what Executor sees.
  std::vector<Tool*> active_tools_;

  // dlopen handles for shared-library plugins — closed in ~Loader().
  // Parallel to the plugin Tool* entries; stored in load order.
  struct PluginHandle;
  std::vector<std::unique_ptr<PluginHandle>> plugin_handles_;
};

}  // namespace ur
