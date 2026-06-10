#include "app.hpp"

#include "debug_ui.hpp"
#include "demo_scene.hpp"
#include "fly_camera.hpp"

#include "platform/sdl_window.hpp"
#include "renderer/vulkan_context.hpp"
#include "scene/scene.hpp"

#include <SDL3/SDL_events.h>
#include <SDL3/SDL_timer.h>

#include <imgui.h>

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

struct DemoApp::Impl {
  engine::EngineConfig config;
  engine::SdlContext sdl;
  engine::SdlWindow window;
  engine::Scene scene;
  engine::VulkanContext vulkan;
  DebugUi debug_ui;
  FlyCameraController fly_camera;
  std::uint64_t last_ticks{};
  bool running{true};

  explicit Impl(engine::EngineConfig config_in)
      : config(std::move(config_in)),
        window(config.window_title, config.window_width, config.window_height),
        scene(create_demo_scene()),
        vulkan(window.handle(), config, scene),
        debug_ui(window.handle(), vulkan) {
    std::signal(SIGINT, on_quit_signal);
    std::signal(SIGTERM, on_quit_signal);
    fly_camera.set_window(window.handle());
    fly_camera.sync_from(scene.camera());
    last_ticks = SDL_GetTicks();

    vulkan.set_frame_overlay([this](const engine::FrameOverlayContext &context) {
      debug_ui.record_overlay(context);
    });

    std::cout << "Selected GPU: " << vulkan.gpu_name();
    if (config.gpu_device_index)
      std::cout << " (requested index " << *config.gpu_device_index << ')';
    std::cout << "\nFly camera: click to look, Esc to release, WASD move, Space/C vertical, Shift sprint\n";
    std::cout << "Debug UI: ImGui overlay (shadow toggles, FPS)\n";
  }

  ~Impl() {
    fly_camera.release_capture();
  }
};

DemoApp::DemoApp(engine::EngineConfig config) : impl_(std::make_unique<Impl>(std::move(config))) {}

DemoApp::~DemoApp() = default;

void DemoApp::run() {
  Impl &impl = *impl_;

  while (impl.running) {
    if (g_quit_requested != 0)
      impl.running = false;

    const std::uint64_t now_ticks = SDL_GetTicks();
    const float delta_seconds = static_cast<float>(now_ticks - impl.last_ticks) / 1000.0F;
    impl.last_ticks = now_ticks;

    SDL_Event event{};
    while (SDL_PollEvent(&event)) {
      if (event.type == SDL_EVENT_QUIT)
        impl.running = false;

      (void)impl.debug_ui.process_event(event);

      if (event.type == SDL_EVENT_WINDOW_RESIZED)
        impl.vulkan.mark_framebuffer_resized();

      if (ImGui::GetIO().WantCaptureKeyboard)
        continue;

      if (event.type == SDL_EVENT_KEY_DOWN && event.key.key == SDLK_ESCAPE && !impl.fly_camera.capture_active())
        impl.running = false;
      else
        impl.fly_camera.handle_event(event);
    }

    if (!impl.running)
      break;

    if (!ImGui::GetIO().WantCaptureKeyboard)
      impl.fly_camera.update(impl.scene.camera(), delta_seconds);

    update_demo_scene(impl.scene);
    impl.debug_ui.begin_frame(impl.scene, delta_seconds);
    impl.vulkan.draw_frame(impl.scene);
  }

  impl.vulkan.shutdown();
}

} // namespace app
