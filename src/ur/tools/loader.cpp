#include "loader.hpp"

#include <algorithm>
#include <filesystem>
#include <iterator>
#include <memory>
#include <string>
#include <vector>

#include "builtin/bash.hpp"
#include "builtin/read_file.hpp"
#include "builtin/read_image.hpp"
#include "builtin/web_fetch.hpp"
#include "builtin/web_search.hpp"
#include "builtin/write_file.hpp"
#include "dl_compat.hpp"

namespace ur {

// ---------------------------------------------------------------------------
// PluginHandle — RAII wrapper around a dlopen handle + destroy function.
// Ensures the tool object is destroyed before the library is unloaded.
// ---------------------------------------------------------------------------
struct Loader::PluginHandle {
  DlHandle handle = nullptr;
  Tool* tool = nullptr;
  void (*destroy)(Tool*) = nullptr;

  ~PluginHandle() {
    if (tool && destroy) destroy(tool);
    if (handle) dl_close(handle);
  }
};

// ---------------------------------------------------------------------------

Loader::Loader(const Paths& paths) : paths_(paths) {}

Loader::~Loader() = default;  // PluginHandle destructors close handles

void Loader::load(const std::string& allow_list, const std::string& deny_list,
                  bool no_tools, bool allow_all) {
  register_builtins();
  apply_manifest();
  load_shared_plugins();
  load_script_plugins();
  apply_filters(allow_list, deny_list, no_tools, allow_all);
}

Tool* Loader::find(const std::string& name) const {
  // Only search active_tools_ — tools filtered out are not callable.
  auto it = std::find_if(active_tools_.begin(), active_tools_.end(),
                         [&](const Tool* t) { return t->name() == name; });
  return it != active_tools_.end() ? *it : nullptr;
}

const std::vector<Tool*>& Loader::active_tools() const { return active_tools_; }

// ---------------------------------------------------------------------------

void Loader::register_builtins() {
  // TODO: instantiate each built-in and add to tools_ + index_.
  //
  // Pattern for each built-in:
  //   auto t = std::make_unique<ReadFileTool>();
  //   index_[t->name()] = t.get();
  //   tools_.push_back(std::move(t));
  //
  // Order determines registration priority — later built-ins do NOT override
  // earlier ones (plugins do).  Registration order here is informational.
  //
  // Built-ins to register:
  //   ReadFileTool, WriteFileTool, BashTool,
  //   WebSearchTool, WebFetchTool, ReadImageTool
  //
  // Note: WebSearchTool should check for UR_SEARCH_BASE_URL at construction
  // time and skip registration (with a std::cerr warning) if absent.
}

void Loader::apply_manifest() {
  // TODO: parse $root/tools/tools.json with nlohmann/json.
  //
  // For each entry:
  //   - "enabled": false  → erase from index_ and remove from tools_
  //   - tool-specific config fields (timeout, max_results) →
  //     cast Tool* to the concrete type and call configure(entry) if present,
  //     OR store config in a side map keyed by name for the tool to read.
  //
  // Missing file: return silently (all defaults apply).
  // Malformed JSON: log warning, continue with defaults.
  //
  // Path: paths_.tools / "tools.json"
}

void Loader::load_shared_plugins() {
  // TODO: scan paths_.tools for *.so (Linux/macOS) or *.dll (Windows).
  //
  // For each candidate file:
  //   1. DlHandle h = dl_open(path.c_str())
  //      On failure: log warning ("failed to load plugin: " + path), continue.
  //   2. Resolve "ur_create_tool" and "ur_destroy_tool" via dl_sym.
  //      On missing symbol: log warning, dl_close(h), continue.
  //   3. Call ur_create_tool() to get a Tool*.
  //   4. If index_ already contains tool->name(): remove the existing entry
  //      from tools_ (override semantics — plugin wins over built-in).
  //   5. Store the Tool* + handle in a PluginHandle, push to plugin_handles_.
  //   6. Add to index_[tool->name()].
  //      Note: do NOT add to tools_ (lifetime managed by PluginHandle).
}

void Loader::load_script_plugins() {
  // TODO: scan paths_.tools for executable files that are not shared libs.
  //
  // Detection heuristic:
  //   - std::filesystem::status(p).permissions() has execute bit set
  //   - extension is not ".so" / ".dll" / ".dylib"
  //
  // For each candidate:
  //   1. Run: <path> --describe
  //      Capture stdout, parse as JSON: {name, description, input_schema}
  //      On failure or non-zero exit: log warning, skip.
  //   2. Construct a ScriptTool (wraps path + cached descriptor).
  //   3. Override semantics same as shared plugins.
  //
  // ScriptTool::execute() runs the script with args_json on stdin and reads
  // {"content": "...", "is_error": false} from stdout.
  // Timeout: from manifest "timeout" field if present, else 30 seconds.
  // (Timeout enforcement is best-effort — see plan/phase4.md.)
}

void Loader::apply_filters(const std::string& allow_list,
                           const std::string& deny_list, bool no_tools,
                           bool allow_all) {
  // TODO: populate active_tools_ from index_ after applying all filters.
  //
  // Filter order:
  //   1. no_tools == true  → active_tools_ stays empty, return.
  //   2. !allow_all && tool->requires_allow_all()  → exclude silently.
  //   3. allow_list non-empty  → keep only tools whose name appears in list.
  //   4. deny_list non-empty   → remove tools whose name appears in list.
  //
  // allow_list / deny_list are comma-separated strings; split on ','.
  // Trim whitespace around each name before comparing.

  (void)allow_list;
  (void)deny_list;
  (void)no_tools;
  (void)allow_all;

  // Temporary passthrough — replace with real filter logic.
  std::transform(tools_.begin(), tools_.end(),
                 std::back_inserter(active_tools_),
                 [](const std::unique_ptr<Tool>& t) { return t.get(); });
}

}  // namespace ur
