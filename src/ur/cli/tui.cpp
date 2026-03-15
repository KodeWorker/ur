#include "tui.hpp"

#include <algorithm>
#include <atomic>
#include <chrono>
#include <condition_variable>
#include <cstdio>
#include <cstdlib>
#include <functional>
#include <memory>
#include <mutex>
#include <sstream>
#include <string>
#include <thread>
#include <utility>
#include <vector>

// ftxui — included via FetchContent.
#include <ftxui/component/component.hpp>
#include <ftxui/component/component_base.hpp>
#include <ftxui/component/screen_interactive.hpp>
#include <ftxui/dom/elements.hpp>
#include <ftxui/screen/box.hpp>

#include "env.hpp"

namespace ur {

// ---------------------------------------------------------------------------
// Helpers
// ---------------------------------------------------------------------------

static std::string env_or(const char* name, const char* fallback) {
  const char* v = std::getenv(name);
  return (v && v[0]) ? v : fallback;
}

// Render text respecting embedded \n characters, wrapping each line with
// ftxui::paragraph so long lines still wrap at the terminal boundary.
static ftxui::Element render_text(const std::string& text) {
  ftxui::Elements elems;
  std::istringstream stream(text);
  std::string line;
  while (std::getline(stream, line)) {
    elems.push_back(ftxui::paragraph(line));
  }
  if (elems.empty()) return ftxui::text("");
  return ftxui::vbox(std::move(elems));
}

// ---------------------------------------------------------------------------
// Scroller — mouse-wheel-scrollable wrapper for a child component.
// Tracks a virtual cursor (selected_) and uses yframe to scroll the viewport.
// Auto-scrolls to the bottom when scroll_to_end_ is set.
// ---------------------------------------------------------------------------

class ScrollerBase : public ftxui::ComponentBase {
 public:
  explicit ScrollerBase(ftxui::Component child) { Add(std::move(child)); }

  // Call from the UI thread to jump to the bottom on the next render.
  void ScrollToEnd() { scroll_to_end_ = true; }

 private:
  ftxui::Element Render() override {
    if (ChildAt(0)->ChildCount() == 0)
      return ftxui::text("") | ftxui::yflex | ftxui::reflect(box_);
    ftxui::Element background = ChildAt(0)->Render();
    background->ComputeRequirement();
    size_ = background->requirement().min_y;
    if (scroll_to_end_) {
      selected_ = size_;
      scroll_to_end_ = false;
    }
    selected_ = std::max(0, std::min(size_ - 1, selected_));
    return ftxui::dbox({
               std::move(background),
               ftxui::vbox({
                   ftxui::text("") |
                       ftxui::size(ftxui::HEIGHT, ftxui::EQUAL, selected_),
                   ftxui::text("") | ftxui::focus,
               }),
           }) |
           ftxui::vscroll_indicator | ftxui::yframe | ftxui::yflex |
           ftxui::reflect(box_);
  }

  bool OnEvent(ftxui::Event event) override {
    if (event.is_mouse() && box_.Contain(event.mouse().x, event.mouse().y))
      TakeFocus();

    // Let children handle non-scroll events first (e.g. Collapsible toggle).
    bool is_scroll =
        (event.is_mouse() &&
         (event.mouse().button == ftxui::Mouse::WheelUp ||
          event.mouse().button == ftxui::Mouse::WheelDown)) ||
        event == ftxui::Event::ArrowUp || event == ftxui::Event::ArrowDown ||
        event == ftxui::Event::PageUp || event == ftxui::Event::PageDown ||
        event == ftxui::Event::Home || event == ftxui::Event::End;
    if (!is_scroll && ComponentBase::OnEvent(event)) return true;

    int old = selected_;
    if (event == ftxui::Event::ArrowUp ||
        (event.is_mouse() && event.mouse().button == ftxui::Mouse::WheelUp))
      selected_--;
    if (event == ftxui::Event::ArrowDown ||
        (event.is_mouse() && event.mouse().button == ftxui::Mouse::WheelDown))
      selected_++;
    if (event == ftxui::Event::PageUp) selected_ -= box_.y_max - box_.y_min;
    if (event == ftxui::Event::PageDown) selected_ += box_.y_max - box_.y_min;
    if (event == ftxui::Event::Home) selected_ = 0;
    if (event == ftxui::Event::End) selected_ = size_;
    selected_ = std::max(0, std::min(size_ - 1, selected_));
    return selected_ != old;
  }

  bool Focusable() const override { return true; }

  int selected_ = 0;
  int size_ = 0;
  bool scroll_to_end_ = false;
  ftxui::Box box_;
};

// ---------------------------------------------------------------------------
// NonFocusable — wraps a component so Tab skips it.
// Mouse clicks and rendering still work normally.
// ---------------------------------------------------------------------------

class NonFocusableBase : public ftxui::ComponentBase {
 public:
  explicit NonFocusableBase(ftxui::Component child) { Add(std::move(child)); }
  bool Focusable() const override { return false; }
  ftxui::Element Render() override { return ChildAt(0)->Render(); }
  bool OnEvent(ftxui::Event event) override {
    return ChildAt(0)->OnEvent(event);
  }
};

static ftxui::Component NonFocusable(ftxui::Component child) {
  return ftxui::Make<NonFocusableBase>(std::move(child));
}

// ---------------------------------------------------------------------------
// Impl
// ---------------------------------------------------------------------------

struct FtxuiTui::Impl {
  // --- Tab ---
  int tab_index = 0;
  std::vector<std::string> tab_labels{"Session", "System Prompt", "Tools",
                                      "Options"};

  // --- Session ---
  std::string input_content;
  int input_cursor = 0;
  std::string submitted_line;

  struct HistoryEntry {
    std::string role;  // "user" | "assistant" | "reason" | "error"
    std::shared_ptr<bool>
        open;  // non-null for "reason"; stable addr for Collapsible
    ftxui::Component component;  // added to history_container once
  };
  std::vector<HistoryEntry> history;

  int prompt_tokens = 0;
  int ctx_len = 0;
  std::string status_hint;

  // --- System Prompt ---
  mutable std::mutex system_prompt_mu;
  std::string system_prompt;

  // --- Options (pre-loaded from env at startup) ---
  std::string opt_base_url;
  std::string opt_api_key;
  std::string opt_model;
  std::string opt_conn_timeout;
  std::string opt_read_timeout;
  std::string opt_num_persona;
  std::string opt_search_base_url;

  // --- Streaming ---
  // Non-null while a print_response_chunk() / print_reasoning_chunk()
  // sequence is in progress. Renderer lambdas capture these shared_ptrs so
  // they always render the latest text without rebuilding the component tree.
  std::shared_ptr<std::string> streaming_content;
  std::shared_ptr<std::string> streaming_reasoning;
  std::shared_ptr<bool> streaming_reason_open;

  // --- Spinner ---
  std::atomic<bool> spinner_running{false};
  std::atomic<int> spinner_frame{0};
  std::thread spinner_thread;

  // --- Submit sync (read_input blocks here between turns) ---
  std::mutex submit_mu;
  std::condition_variable submit_cv;
  bool submitted = false;
  bool exit_requested = false;
  std::thread loop_thread;

  // --- ftxui (screen must be last — non-movable after construction) ---
  ftxui::ScreenInteractive screen;
  ftxui::Component root;
  ftxui::Component history_container;
  ScrollerBase* scroller = nullptr;  // raw ptr into the scroller component

  Impl() : screen(ftxui::ScreenInteractive::Fullscreen()) {}

  // Execute fn in the event loop if running; otherwise execute directly.
  // Direct execution is safe before read_input() starts the loop thread
  // (only the main thread is running at that point).
  void post_or_direct(std::function<void()> fn) {
    if (loop_thread.joinable()) {
      screen.Post(std::move(fn));
    } else {
      fn();
    }
  }
};

// ---------------------------------------------------------------------------
// Constructor
// ---------------------------------------------------------------------------

FtxuiTui::FtxuiTui() : impl_(std::make_unique<Impl>()) {
  // Load Options from environment.
  impl_->opt_base_url = env_or("UR_LLM_BASE_URL", "http://localhost:8080");
  impl_->opt_api_key = env_or("UR_LLM_API_KEY", "");
  impl_->opt_model = env_or("UR_LLM_MODEL", "");
  impl_->opt_conn_timeout = env_or("UR_LLM_CONNECTION_TIMEOUT", "10");
  impl_->opt_read_timeout = env_or("UR_LLM_READ_TIMEOUT", "0");
  impl_->opt_num_persona = env_or("UR_NUM_PERSONA", "0");
  impl_->opt_search_base_url =
      env_or("UR_SEARCH_BASE_URL", "http://localhost:8888");

  // Stable raw pointer for lambdas — safe because Impl is owned by this object.
  Impl* d = impl_.get();

  // ── Session tab ───────────────────────────────────────────────────────────

  d->history_container = ftxui::Container::Vertical({});

  auto input_style = [](ftxui::InputState s) {
    return std::move(s.element) | ftxui::bgcolor(ftxui::Color::GrayDark) |
           ftxui::color(ftxui::Color::White);
  };

  auto input_option = ftxui::InputOption::Default();
  input_option.transform = input_style;
  input_option.cursor_position = &d->input_cursor;
  input_option.on_enter = [d] {
    std::string line = d->input_content;
    d->input_content.clear();
    d->input_cursor = 0;
    if (!line.empty()) {
      // Notify the main thread (blocked in read_input). The event loop keeps
      // running so the screen stays live for the next turn.
      std::lock_guard<std::mutex> lock(d->submit_mu);
      d->submitted_line = line;
      d->submitted = true;
      d->submit_cv.notify_one();
    }
  };
  auto input_component = ftxui::Input(
      &d->input_content, "Type a message or /command…", input_option);

  // Tab key autocomplete for slash commands.
  static const std::vector<std::string> kSlashCommands{
      "/help",  "/exit",    "/compact",     "/clear",
      "/title", "/persona", "/load-prompt", "/save-prompt"};
  input_component = ftxui::CatchEvent(input_component, [d](ftxui::Event e) {
    if (e != ftxui::Event::Tab) return false;
    if (d->input_content.empty() || d->input_content[0] != '/') return false;
    std::vector<std::string> matches;
    std::copy_if(kSlashCommands.begin(), kSlashCommands.end(),
                 std::back_inserter(matches), [&](const std::string& cmd) {
                   return cmd.rfind(d->input_content, 0) == 0;
                 });
    if (matches.empty()) return true;
    if (matches.size() == 1) {
      d->input_content = matches[0];
      d->input_cursor = static_cast<int>(d->input_content.size());
      return true;
    }
    // Multiple matches: complete to longest common prefix.
    std::string prefix = matches[0];
    for (const auto& m : matches) {
      size_t i = 0;
      while (i < prefix.size() && i < m.size() && prefix[i] == m[i]) ++i;
      prefix.resize(i);
    }
    d->input_content = prefix;
    d->input_cursor = static_cast<int>(d->input_content.size());
    return true;
  });

  auto scroller_owned = ftxui::Make<ScrollerBase>(d->history_container);
  d->scroller = scroller_owned.get();
  auto scroller =
      std::shared_ptr<ftxui::ComponentBase>(std::move(scroller_owned));

  auto session_container =
      ftxui::Container::Vertical({scroller, input_component});

  // Spinner frames (Braille dots).
  static const std::vector<std::string> kSpinnerFrames{"⠋", "⠙", "⠹", "⠸", "⠼",
                                                       "⠴", "⠦", "⠧", "⠇", "⠏"};

  auto session_tab = ftxui::Renderer(session_container, [d, scroller,
                                                         input_component] {
    // Status footer.
    std::string status;
    if (d->prompt_tokens > 0) {
      status = "context: " + std::to_string(d->prompt_tokens);
      if (d->ctx_len > 0) status += "/" + std::to_string(d->ctx_len);
      status += " tokens";
    }
    if (!d->status_hint.empty()) {
      if (!status.empty()) status += "  |  ";
      status += d->status_hint;
    }

    // Spinner element.
    ftxui::Element spinner_elem = ftxui::text("");
    if (d->spinner_running) {
      int f = d->spinner_frame.load() % static_cast<int>(kSpinnerFrames.size());
      spinner_elem = ftxui::text(" " + kSpinnerFrames[f] + " thinking…") |
                     ftxui::color(ftxui::Color::Yellow);
    }

    return ftxui::vbox({
        scroller->Render(),
        ftxui::separator(),
        ftxui::hbox({ftxui::text("> "), input_component->Render() | ftxui::flex,
                     spinner_elem}),
        ftxui::separator(),
        ftxui::text(status) | ftxui::dim,
    });
  });

  // ── System Prompt tab ─────────────────────────────────────────────────────

  auto sysprompt_option = ftxui::InputOption::Default();
  sysprompt_option.multiline = true;
  sysprompt_option.transform = input_style;
  auto sysprompt_input =
      ftxui::Input(&d->system_prompt, "Enter system prompt…", sysprompt_option);

  auto sysprompt_tab = ftxui::Renderer(sysprompt_input, [sysprompt_input] {
    return ftxui::vbox({
        ftxui::text("System Prompt") | ftxui::bold,
        ftxui::separator(),
        sysprompt_input->Render() | ftxui::flex | ftxui::border,
        ftxui::separator(),
        ftxui::text(
            "Edits take effect on the next turn.  "
            "Use /save-prompt <path> or /load-prompt <path> to persist.") |
            ftxui::dim,
    });
  });

  // ── Tools tab (Phase 4 placeholder) ──────────────────────────────────────

  auto tools_tab = ftxui::Renderer([] {
    return ftxui::vbox({
        ftxui::text("Tools") | ftxui::bold,
        ftxui::separator(),
        ftxui::text("No tools loaded.  "
                    "Add plugins to $root/tools/ and restart.") |
            ftxui::dim,
    });
  });

  // ── Options tab ───────────────────────────────────────────────────────────

  // Shared save callback — writes all 5 fields to .env on Enter.
  auto save_opts = [d] {
    save_dotenv(".env", {{"UR_LLM_BASE_URL", d->opt_base_url},
                         {"UR_LLM_API_KEY", d->opt_api_key},
                         {"UR_LLM_MODEL", d->opt_model},
                         {"UR_LLM_CONNECTION_TIMEOUT", d->opt_conn_timeout},
                         {"UR_LLM_READ_TIMEOUT", d->opt_read_timeout},
                         {"UR_NUM_PERSONA", d->opt_num_persona},
                         {"UR_SEARCH_BASE_URL", d->opt_search_base_url}});
  };

  auto make_opt = [&](std::string* val, const char* placeholder) {
    ftxui::InputOption opt;
    opt.multiline = false;
    opt.transform = input_style;
    opt.on_enter = save_opts;
    return ftxui::Input(val, placeholder, opt);
  };

  auto opt_url_input = make_opt(&d->opt_base_url, "http://localhost:8080");
  auto opt_key_input = make_opt(&d->opt_api_key, "(empty for local servers)");
  auto opt_model_input = make_opt(&d->opt_model, "(server default)");
  auto opt_conn_input = make_opt(&d->opt_conn_timeout, "10");
  auto opt_read_input = make_opt(&d->opt_read_timeout, "0");
  auto opt_num_persona_input = make_opt(&d->opt_num_persona, "0");
  auto opt_search_base_url_input =
      make_opt(&d->opt_search_base_url, "http://localhost:8888");

  auto options_container = ftxui::Container::Vertical({
      opt_url_input,
      opt_key_input,
      opt_model_input,
      opt_conn_input,
      opt_read_input,
      opt_num_persona_input,
      opt_search_base_url_input,
  });

  auto options_tab = ftxui::Renderer(
      options_container,
      [opt_url_input, opt_key_input, opt_model_input, opt_conn_input,
       opt_read_input, opt_num_persona_input, opt_search_base_url_input] {
        // Fixed-width label helper.
        constexpr int kLabelWidth = 28;
        auto row = [kLabelWidth](const char* label, ftxui::Component c) {
          return ftxui::hbox({
              ftxui::text(label) |
                  ftxui::size(ftxui::WIDTH, ftxui::EQUAL, kLabelWidth),
              c->Render() | ftxui::flex,
          });
        };
        return ftxui::vbox({
            ftxui::text("Options") | ftxui::bold,
            ftxui::separator(),
            row("UR_LLM_BASE_URL", opt_url_input),
            row("UR_LLM_API_KEY", opt_key_input),
            row("UR_LLM_MODEL", opt_model_input),
            row("UR_LLM_CONNECTION_TIMEOUT", opt_conn_input),
            row("UR_LLM_READ_TIMEOUT", opt_read_input),
            row("UR_NUM_PERSONA", opt_num_persona_input),
            row("UR_SEARCH_BASE_URL", opt_search_base_url_input),
            ftxui::separator(),
            ftxui::text(
                "Press Enter on any field to save all options to .env.") |
                ftxui::dim,
        });
      });

  // ── Tab bar + root ────────────────────────────────────────────────────────

  auto tab_opt = ftxui::MenuOption::HorizontalAnimated();
  tab_opt.entries_option.transform = [](const ftxui::EntryState& s) {
    auto e = ftxui::text(s.label) | ftxui::hcenter | ftxui::flex;
    if (s.active) e = e | ftxui::bold;
    if (s.focused) e = e | ftxui::inverted;
    return e;
  };
  auto tab_toggle = ftxui::Menu(&d->tab_labels, &d->tab_index, tab_opt);

  auto tab_content = ftxui::Container::Tab(
      {session_tab, sysprompt_tab, tools_tab, options_tab}, &d->tab_index);

  auto layout = ftxui::Container::Vertical({tab_toggle, tab_content});

  d->root = ftxui::Renderer(layout, [tab_toggle, tab_content] {
    return ftxui::vbox({
        tab_toggle->Render() | ftxui::xflex,
        ftxui::separator(),
        tab_content->Render() | ftxui::flex,
    });
  });
}

// ---------------------------------------------------------------------------
// Destructor
// ---------------------------------------------------------------------------

FtxuiTui::~FtxuiTui() {
  if (!impl_) return;
  // Unblock any read_input() waiting on submit_cv.
  {
    std::lock_guard<std::mutex> lock(impl_->submit_mu);
    impl_->exit_requested = true;
    impl_->submit_cv.notify_all();
  }
  // Stop spinner before loop (spinner posts events; must join first).
  impl_->spinner_running = false;
  if (impl_->spinner_thread.joinable()) impl_->spinner_thread.join();
  // Exit the event loop and join its thread.
  impl_->screen.ExitLoopClosure()();
  if (impl_->loop_thread.joinable()) impl_->loop_thread.join();
}

// ---------------------------------------------------------------------------
// Remaining method stubs (read_input, print_*, set_status, spinner)
// ---------------------------------------------------------------------------

std::string FtxuiTui::read_input() {
  Impl* d = impl_.get();

  // Start the event loop in a background thread on the first call.
  // The loop runs for the lifetime of the session; read_input() just waits
  // for each submitted line via submit_cv.
  if (!d->loop_thread.joinable()) {
    d->loop_thread = std::thread([d] {
      // Set terminal window title.
      std::printf("\033]0;UR::CHAT\007");
      std::fflush(stdout);
      d->screen.Loop(d->root);
      // Restore terminal title on exit.
      std::printf("\033]0;\007");
      std::fflush(stdout);
      // Loop exited externally (Ctrl-C or ExitLoopClosure).
      std::lock_guard<std::mutex> lock(d->submit_mu);
      d->exit_requested = true;
      d->submit_cv.notify_one();
    });
  }

  std::unique_lock<std::mutex> lock(d->submit_mu);
  d->submit_cv.wait(lock, [d] { return d->submitted || d->exit_requested; });

  if (d->exit_requested) return "";

  std::string result = std::move(d->submitted_line);
  d->submitted = false;
  return result;
}

void FtxuiTui::print_user(const std::string& content) {
  Impl* d = impl_.get();
  // Finalize any prior stream before starting a new turn.
  d->streaming_content = nullptr;
  d->streaming_reasoning = nullptr;
  d->streaming_reason_open = nullptr;
  std::string text = content;
  auto component = ftxui::Renderer([text] {
    return ftxui::vbox({
               ftxui::text("user: ") | ftxui::bold |
                   ftxui::color(ftxui::Color::Cyan),
               render_text(text),
           }) |
           ftxui::xflex;
  });
  d->post_or_direct([d, component]() {
    d->history.push_back({"user", nullptr, component});
    d->history_container->Add(component);
    d->scroller->ScrollToEnd();
    d->screen.PostEvent(ftxui::Event::Custom);
  });
}

void FtxuiTui::print_response(const std::string& content) {
  Impl* d = impl_.get();
  std::string text = content;
  auto component = ftxui::Renderer([text] {
    return ftxui::vbox({
               ftxui::text("assistant: ") | ftxui::bold |
                   ftxui::color(ftxui::Color::Green),
               render_text(text),
           }) |
           ftxui::xflex;
  });
  d->post_or_direct([d, component]() {
    d->history.push_back({"assistant", nullptr, component});
    d->history_container->Add(component);
    d->scroller->ScrollToEnd();
    d->screen.PostEvent(ftxui::Event::Custom);
  });
}

void FtxuiTui::print_response_chunk(const std::string& chunk) {
  Impl* d = impl_.get();
  if (!d->streaming_content) {
    // First chunk: create the shared buffer and add a live component.
    d->streaming_content = std::make_shared<std::string>();
    auto buf = d->streaming_content;
    auto component = ftxui::Renderer([buf] {
      return ftxui::vbox({
                 ftxui::text("assistant: ") | ftxui::bold |
                     ftxui::color(ftxui::Color::Green),
                 render_text(*buf),
             }) |
             ftxui::xflex;
    });
    d->screen.Post([d, component]() {
      d->history.push_back({"assistant", nullptr, component});
      d->history_container->Add(component);
      d->scroller->ScrollToEnd();
      d->screen.PostEvent(ftxui::Event::Custom);
    });
  }
  // Append chunk to shared buffer and redraw.
  auto buf = d->streaming_content;
  d->screen.Post([d, buf, chunk]() {
    *buf += chunk;
    d->scroller->ScrollToEnd();
    d->screen.PostEvent(ftxui::Event::Custom);
  });
}

void FtxuiTui::print_reasoning(const std::string& reasoning) {
  if (reasoning.empty()) return;
  Impl* d = impl_.get();
  std::string text = reasoning;
  // shared_ptr keeps the bool alive and its address stable even if history
  // vector reallocates. Collapsible holds the raw bool* safely.
  auto open = std::make_shared<bool>(false);
  auto inner = ftxui::Renderer([text] {
    return render_text(text) | ftxui::color(ftxui::Color::GrayDark) |
           ftxui::xflex;
  });
  auto collapsible =
      NonFocusable(ftxui::Collapsible("thinking…", inner, open.get()));
  d->post_or_direct([d, collapsible, open]() {
    d->history.push_back({"reason", open, collapsible});
    d->history_container->Add(collapsible);
    d->scroller->ScrollToEnd();
    d->screen.PostEvent(ftxui::Event::Custom);
  });
}

void FtxuiTui::print_reasoning_chunk(const std::string& chunk) {
  Impl* d = impl_.get();
  if (!d->streaming_reasoning) {
    // First chunk: create shared buffers and a live collapsible component.
    d->streaming_reasoning = std::make_shared<std::string>();
    d->streaming_reason_open = std::make_shared<bool>(false);
    auto buf = d->streaming_reasoning;
    auto open = d->streaming_reason_open;
    auto inner = ftxui::Renderer([buf] {
      return render_text(*buf) | ftxui::color(ftxui::Color::GrayDark) |
             ftxui::xflex;
    });
    auto collapsible =
        NonFocusable(ftxui::Collapsible("thinking…", inner, open.get()));
    d->screen.Post([d, collapsible, open]() {
      d->history.push_back({"reason", open, collapsible});
      d->history_container->Add(collapsible);
      d->scroller->ScrollToEnd();
      d->screen.PostEvent(ftxui::Event::Custom);
    });
  }
  auto buf = d->streaming_reasoning;
  d->screen.Post([d, buf, chunk]() {
    *buf += chunk;
    d->scroller->ScrollToEnd();
    d->screen.PostEvent(ftxui::Event::Custom);
  });
}

void FtxuiTui::print_error(const std::string& msg) {
  Impl* d = impl_.get();
  std::string text = msg;
  auto component = ftxui::Renderer([text] {
    return ftxui::text("error: " + text) | ftxui::color(ftxui::Color::Red);
  });
  d->post_or_direct([d, component]() {
    d->history.push_back({"error", nullptr, component});
    d->history_container->Add(component);
    d->scroller->ScrollToEnd();
    d->screen.PostEvent(ftxui::Event::Custom);
  });
}

void FtxuiTui::set_status(int prompt_tokens, int ctx_len,
                          const std::string& hint) {
  Impl* d = impl_.get();
  // Post to the UI thread — avoids data races on the string members.
  d->screen.Post([d, prompt_tokens, ctx_len, hint]() {
    d->prompt_tokens = prompt_tokens;
    d->ctx_len = ctx_len;
    d->status_hint = hint;
  });
}

void FtxuiTui::start_spinner() {
  Impl* d = impl_.get();
  d->spinner_running = false;
  if (d->spinner_thread.joinable()) d->spinner_thread.join();
  d->spinner_running = true;
  d->spinner_frame = 0;
  d->spinner_thread = std::thread([d] {
    while (d->spinner_running) {
      std::this_thread::sleep_for(std::chrono::milliseconds(100));
      ++d->spinner_frame;
      d->screen.PostEvent(ftxui::Event::Custom);
    }
  });
}

void FtxuiTui::stop_spinner() {
  Impl* d = impl_.get();
  d->spinner_running = false;
  if (d->spinner_thread.joinable()) d->spinner_thread.join();
  // One final redraw to clear the spinner from the screen.
  d->screen.PostEvent(ftxui::Event::Custom);
}

std::string FtxuiTui::system_prompt() const {
  std::lock_guard<std::mutex> lock(impl_->system_prompt_mu);
  return impl_->system_prompt;
}

void FtxuiTui::set_system_prompt(const std::string& prompt) {
  std::lock_guard<std::mutex> lock(impl_->system_prompt_mu);
  impl_->system_prompt = prompt;
}

#ifdef _WIN32
#include <io.h>
bool FtxuiTui::is_interactive() const {
  return _isatty(_fileno(stdin)) && _isatty(_fileno(stdout));
}
#else
#include <unistd.h>
bool FtxuiTui::is_interactive() const {
  return isatty(STDIN_FILENO) && isatty(STDOUT_FILENO);
}
#endif

std::unique_ptr<Tui> make_tui() { return std::make_unique<FtxuiTui>(); }

}  // namespace ur
