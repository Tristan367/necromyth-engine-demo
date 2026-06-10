#pragma once

#include "renderer/frame_overlay.hpp"
#include "scene/scene.hpp"

#include <SDL3/SDL.h>

#include <memory>

namespace engine {
class VulkanContext;
}

namespace app {

struct UiFrameResult {
  bool quit_requested{};
  bool resume_requested{};
};

class DebugUi {
public:
  DebugUi(SDL_Window *window, engine::VulkanContext &vulkan);
  ~DebugUi();

  DebugUi(const DebugUi &) = delete;
  DebugUi &operator=(const DebugUi &) = delete;

  [[nodiscard]] auto process_event(const SDL_Event &event) -> bool;
  [[nodiscard]] auto wants_keyboard() const -> bool;
  [[nodiscard]] auto wants_mouse() const -> bool;
  [[nodiscard]] auto begin_frame(engine::Scene &scene, float frame_delta_seconds, bool menu_open) -> UiFrameResult;
  void record_overlay(const engine::FrameOverlayContext &context);

private:
  struct Impl;
  std::unique_ptr<Impl> impl_;
};

} // namespace app
