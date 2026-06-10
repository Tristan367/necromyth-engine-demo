#pragma once

#include "debug_ui.hpp"
#include "fly_camera.hpp"

#include <SDL3/SDL.h>

namespace app {

class InputRouter {
public:
  [[nodiscard]] auto quit_requested() const -> bool {
    return quit_requested_;
  }

  void process_event(const SDL_Event &event, DebugUi &debug_ui, FlyCameraController &fly_camera) {
    if (event.type == SDL_EVENT_QUIT)
      quit_requested_ = true;

    (void)debug_ui.process_event(event);

    if (event.type == SDL_EVENT_KEY_DOWN && event.key.key == SDLK_ESCAPE && !debug_ui.wants_keyboard())
      fly_camera.toggle_capture();

    if (debug_ui.wants_keyboard() || debug_ui.wants_mouse())
      return;

    fly_camera.handle_event(event);
  }

  [[nodiscard]] auto should_update_camera(const DebugUi &debug_ui, const FlyCameraController &fly_camera) const -> bool {
    return fly_camera.capture_active() && !debug_ui.wants_keyboard();
  }

private:
  bool quit_requested_{false};
};

} // namespace app
