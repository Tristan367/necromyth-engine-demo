#pragma once

#include "debug_ui.hpp"
#include "fly_camera.hpp"
#include "platform/input_map.hpp"

#include <SDL3/SDL.h>

namespace app {

class InputRouter {
public:
  InputRouter() {
    // Character movement
    input_.register_action("move_forward");
    input_.register_action("move_back");
    input_.register_action("move_left");
    input_.register_action("move_right");
    input_.register_action("jump");
    input_.register_action("sprint");

    // Fly camera
    input_.register_action("fly_forward");
    input_.register_action("fly_back");
    input_.register_action("fly_left");
    input_.register_action("fly_right");
    input_.register_action("fly_up");
    input_.register_action("fly_down");
    input_.register_action("fly_sprint");

    // Keyboard
    input_.bind("move_forward", engine::InputBinding::key_binding(SDL_SCANCODE_W));
    input_.bind("move_back",    engine::InputBinding::key_binding(SDL_SCANCODE_S));
    input_.bind("move_left",    engine::InputBinding::key_binding(SDL_SCANCODE_A));
    input_.bind("move_right",   engine::InputBinding::key_binding(SDL_SCANCODE_D));
    input_.bind("jump",         engine::InputBinding::key_binding(SDL_SCANCODE_SPACE));
    input_.bind("sprint",       engine::InputBinding::key_binding(SDL_SCANCODE_LSHIFT));
    input_.bind("sprint",       engine::InputBinding::key_binding(SDL_SCANCODE_RSHIFT));

    input_.bind("fly_forward",  engine::InputBinding::key_binding(SDL_SCANCODE_W));
    input_.bind("fly_back",     engine::InputBinding::key_binding(SDL_SCANCODE_S));
    input_.bind("fly_left",     engine::InputBinding::key_binding(SDL_SCANCODE_A));
    input_.bind("fly_right",    engine::InputBinding::key_binding(SDL_SCANCODE_D));
    input_.bind("fly_up",       engine::InputBinding::key_binding(SDL_SCANCODE_SPACE));
    input_.bind("fly_down",     engine::InputBinding::key_binding(SDL_SCANCODE_C));
    input_.bind("fly_sprint",   engine::InputBinding::key_binding(SDL_SCANCODE_LSHIFT));
    input_.bind("fly_sprint",   engine::InputBinding::key_binding(SDL_SCANCODE_RSHIFT));
  }

  [[nodiscard]] auto quit_requested() const -> bool { return quit_requested_; }
  [[nodiscard]] auto input_map() -> engine::InputMap & { return input_; }
  [[nodiscard]] auto input_map() const -> const engine::InputMap & { return input_; }

  void process_event(const SDL_Event &event, DebugUi &debug_ui, FlyCameraController &fly_camera) {
    if (event.type == SDL_EVENT_QUIT)
      quit_requested_ = true;

    (void)debug_ui.process_event(event);

    if (event.type == SDL_EVENT_KEY_DOWN && event.key.key == SDLK_ESCAPE && !debug_ui.wants_keyboard())
      fly_camera.toggle_capture();

    if (debug_ui.wants_keyboard() || debug_ui.wants_mouse())
      return;

    fly_camera.handle_event(event);
    input_.process_event(event);
  }

  [[nodiscard]] auto should_update_camera(const DebugUi &debug_ui, const FlyCameraController &fly_camera) const -> bool {
    return fly_camera.capture_active() && !debug_ui.wants_keyboard();
  }

private:
  bool quit_requested_{false};
  engine::InputMap input_;
};

} // namespace app
