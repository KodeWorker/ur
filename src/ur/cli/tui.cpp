#include "tui.hpp"

#include <memory>
#include <string>

// ftxui — included via FetchContent.
// component: ScreenInteractive, Input, Collapsible, Spinner
// dom: elements, text, separator, gauge
// screen: Screen
#include <ftxui/component/component.hpp>
#include <ftxui/component/screen_interactive.hpp>
#include <ftxui/dom/elements.hpp>

namespace ur {

FtxuiTui::FtxuiTui() {
  // TODO: initialise ScreenInteractive (TerminalOutput or FullscreenPrimary).
  //       Build initial component tree:
  //         - Input component bound to an input string + submit callback
  //         - Scrollable chat history container (list of rendered message
  //           elements: user bubbles, assistant bubbles, Collapsible for
  //           reason)
  //         - Spinner element shown/hidden based on spinner_running_ flag
}

FtxuiTui::~FtxuiTui() {
  // TODO: post an ExitLoopClosure event to the ScreenInteractive if it is
  //       still running, then join the event-loop thread if any.
}

std::string FtxuiTui::read_input() {
  // TODO: block the ftxui event loop (screen.Loop(root_component)) until the
  //       user presses Enter. The Input component's on_enter callback stores
  //       the submitted string and posts an event to unblock the loop.
  //       Return "" on Ctrl-C or /exit.
  return {};
}

void FtxuiTui::print_response(const std::string& content) {
  // TODO: append a new assistant message Element to the chat history list;
  //       call screen.PostEvent(Event::Custom) to trigger a re-render.
  (void)content;
}

void FtxuiTui::print_reasoning(const std::string& reasoning) {
  // TODO: if reasoning is empty, return.
  //       Otherwise prepend a ftxui Collapsible component (collapsed by
  //       default, title "thinking…") containing the reasoning text above
  //       the corresponding response element; trigger re-render.
  (void)reasoning;
}

void FtxuiTui::start_spinner() {
  // TODO: set spinner_running_ = true; start a background thread that posts
  //       Event::Custom at ~100 ms intervals to advance the spinner frame
  //       counter; the renderer reads spinner_running_ + frame to draw the
  //       spinner Element (e.g. ftxui::spinner(7, frame)).
}

void FtxuiTui::stop_spinner() {
  // TODO: set spinner_running_ = false; join the spinner thread; post one
  //       more Event::Custom to clear the spinner from the screen.
}

std::unique_ptr<Tui> make_tui() { return std::make_unique<FtxuiTui>(); }

}  // namespace ur
