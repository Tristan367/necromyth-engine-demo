#pragma once

#include "renderer/frame_overlay.hpp"
#include "scene/scene.hpp"

#include <SDL3/SDL.h>

#include <memory>

namespace engine {
class VulkanContext;
}

namespace app {

class DebugUi {
public:
  DebugUi(SDL_Window *window, engine::VulkanContext &vulkan);
  ~DebugUi();

  DebugUi(const DebugUi &) = delete;
  DebugUi &operator=(const DebugUi &) = delete;

  [[nodiscard]] auto process_event(const SDL_Event &event) -> bool;
  void begin_frame(engine::Scene &scene, float frame_delta_seconds);
  void record_overlay(const engine::FrameOverlayContext &context);

private:
  struct Impl;
  std::unique_ptr<Impl> impl_;
};

} // namespace app
