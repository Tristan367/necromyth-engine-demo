#pragma once

#include "demo_scene.hpp"
#include "fly_camera.hpp"

#include "engine_config.hpp"
#include "platform/sdl_window.hpp"
#include "renderer/vulkan_context.hpp"
#include "scene/scene.hpp"

#include <SDL3/SDL_events.h>
#include <SDL3/SDL_timer.h>

#include <csignal>
#include <iostream>
#include <utility>

namespace app {

namespace {

volatile std::sig_atomic_t g_quit_requested = 0;

void on_quit_signal(int) {
  g_quit_requested = 1;
}

} // namespace

class DemoApp {
public:
  explicit DemoApp(engine::EngineConfig config = engine::engine_config_from_environment())
      : config_(std::move(config)),
        window_(config_.window_title, config_.window_width, config_.window_height),
        scene_(create_demo_scene()),
        vulkan_(window_.handle(), config_, scene_) {
    std::signal(SIGINT, on_quit_signal);
    std::signal(SIGTERM, on_quit_signal);
    fly_camera_.set_window(window_.handle());
    fly_camera_.sync_from(scene_.camera());
    last_ticks_ = SDL_GetTicks();
    std::cout << "Selected GPU: " << vulkan_.gpu_name();
    if (config_.gpu_device_index)
      std::cout << " (requested index " << *config_.gpu_device_index << ')';
    std::cout << "\nFly camera: click to look, Esc to release, WASD move, Q/E vertical, Shift sprint\n";
  }

  ~DemoApp() {
    fly_camera_.release_capture();
  }

  void run() {
    while (running_) {
      if (g_quit_requested != 0)
        running_ = false;

      const std::uint64_t now_ticks = SDL_GetTicks();
      const float delta_seconds = static_cast<float>(now_ticks - last_ticks_) / 1000.0F;
      last_ticks_ = now_ticks;

      SDL_Event event{};
      while (SDL_PollEvent(&event)) {
        if (event.type == SDL_EVENT_QUIT)
          running_ = false;
        else if (event.type == SDL_EVENT_KEY_DOWN && event.key.key == SDLK_ESCAPE && !fly_camera_.capture_active())
          running_ = false;
        else if (event.type == SDL_EVENT_WINDOW_RESIZED)
          vulkan_.mark_framebuffer_resized();
        else
          fly_camera_.handle_event(event);
      }

      if (!running_)
        break;

      fly_camera_.update(scene_.camera(), delta_seconds);
      update_demo_scene(scene_);
      vulkan_.draw_frame(scene_);
    }
  }

private:
  engine::EngineConfig config_;
  engine::SdlContext sdl_;
  engine::SdlWindow window_;
  engine::Scene scene_;
  engine::VulkanContext vulkan_;
  FlyCameraController fly_camera_;
  std::uint64_t last_ticks_{};
  bool running_{true};
};

} // namespace app
