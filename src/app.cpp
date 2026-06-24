#include "app.hpp"

#include "debug_ui.hpp"
#include "demo_scene.hpp"
#include "demo_server.hpp"
#include "fly_camera.hpp"
#include "input_router.hpp"

#include "platform/sdl_window.hpp"
#include "renderer/vulkan_context.hpp"
#include "scene/scene.hpp"

#include <SDL3/SDL_events.h>
#include <SDL3/SDL_keyboard.h>
#include <SDL3/SDL_timer.h>

#include <csignal>
#include <iostream>
#include <mutex>
#include <utility>
#include <vector>

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
  std::vector<std::uint32_t> cube_indices_;
  std::uint32_t char_instance_{};
  engine::Scene scene;
  engine::VulkanContext vulkan;
  DemoServer server;
  DebugUi debug_ui;
  FlyCameraController fly_camera;
  InputRouter input;
  std::uint64_t last_frame_counter{};
  bool running{true};
  bool character_mode{false};

  explicit Impl(engine::EngineConfig config_in)
      : config(std::move(config_in)),
        window("Necromyth Engine Demo", config.window_width, config.window_height),
        scene(create_demo_scene(&cube_indices_, &char_instance_)),
        vulkan(window.handle(), config, scene),
        server(scene, cube_indices_, char_instance_),
        debug_ui(window.handle(), vulkan) {
    std::signal(SIGINT, on_quit_signal);
    std::signal(SIGTERM, on_quit_signal);
    fly_camera.set_window(window.handle());
    fly_camera.sync_from(scene.camera());
    last_frame_counter = SDL_GetPerformanceCounter();

    vulkan.set_frame_overlay([this](const engine::FrameOverlayContext &context) {
      debug_ui.record_overlay(context);
    });

    std::cout << "Selected GPU: " << vulkan.gpu_name();
    if (config.gpu_device_index)
      std::cout << " (requested index " << *config.gpu_device_index << ')';
    std::cout << "\nEsc: menu / resume fly mode. WASD move, Space/C vertical, Shift sprint.\n";
    std::cout << "Menu: Resume or Quit. Debug panel: shadow toggles, FPS.\n";

    server.start();
  }

  ~Impl() {
    server.stop();
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

    const std::uint64_t now_counter = SDL_GetPerformanceCounter();
    const float delta_seconds =
        static_cast<float>(now_counter - impl.last_frame_counter) / static_cast<float>(SDL_GetPerformanceFrequency());
    impl.last_frame_counter = now_counter;

    SDL_Event event{};
    while (SDL_PollEvent(&event)) {
      impl.input.process_event(event, impl.debug_ui, impl.fly_camera);

      if (event.type == SDL_EVENT_KEY_DOWN && event.key.key == SDLK_E &&
          !impl.debug_ui.wants_keyboard()) {
        std::lock_guard lock(impl.server.scene_mutex());
        toggle_demo_animation(impl.scene);
      }

      if (event.type == SDL_EVENT_KEY_DOWN && event.key.key == SDLK_TAB &&
          !impl.debug_ui.wants_keyboard()) {
        impl.character_mode = !impl.character_mode;
        std::cout << (impl.character_mode ? "Character mode\n" : "Fly mode\n");
      }

      if (event.type == SDL_EVENT_WINDOW_RESIZED ||
          event.type == SDL_EVENT_WINDOW_PIXEL_SIZE_CHANGED)
        impl.vulkan.mark_framebuffer_resized();
    }

    if (impl.input.quit_requested())
      impl.running = false;

    if (!impl.running)
      break;

    const bool menu_open = !impl.fly_camera.capture_active();

    if (!impl.character_mode && impl.input.should_update_camera(impl.debug_ui, impl.fly_camera))
      impl.fly_camera.update(impl.scene.camera(), delta_seconds);

    if (impl.character_mode && !menu_open) {
      const bool *keys = SDL_GetKeyboardState(nullptr);
      float input_fwd = 0.0F;
      float input_rgt = 0.0F;
      bool jump = false;

      if (keys[SDL_SCANCODE_W]) input_fwd += 1.0F;
      if (keys[SDL_SCANCODE_S]) input_fwd -= 1.0F;
      if (keys[SDL_SCANCODE_D]) input_rgt += 1.0F;
      if (keys[SDL_SCANCODE_A]) input_rgt -= 1.0F;
      if (keys[SDL_SCANCODE_SPACE]) jump = true;

      // Normalize input so diagonal isn't faster
      float mag = std::sqrt(input_fwd * input_fwd + input_rgt * input_rgt);
      if (mag > 1.0F) { input_fwd /= mag; input_rgt /= mag; }

      // Convert local input to world-space velocity via camera forward
      const glm::vec3 look = impl.fly_camera.forward();
      const glm::vec3 fwd = glm::normalize(glm::vec3(look.x, 0.0F, look.z));
      const glm::vec3 rgt = glm::normalize(glm::cross(fwd, glm::vec3(0.0F, 1.0F, 0.0F)));
      const glm::vec3 world_vel = fwd * input_fwd + rgt * input_rgt;

      impl.server.set_input(world_vel.x, world_vel.z, jump);
    }

    if (impl.character_mode) {
      const glm::vec3 look_fwd = impl.fly_camera.forward();
      const glm::vec3 char_pos = impl.scene.instances()[impl.char_instance_].model[3];
      impl.scene.camera().look_at(
          glm::vec3(char_pos.x, char_pos.y + 1.5F, char_pos.z),
          glm::vec3(char_pos.x, char_pos.y + 1.5F, char_pos.z) + look_fwd);
    }

    {
      std::lock_guard lock(impl.server.scene_mutex());
      update_demo_scene(impl.scene);
    }

    const UiFrameResult ui = impl.debug_ui.begin_frame(impl.scene, delta_seconds, menu_open);

    if (ui.quit_requested)
      impl.running = false;
    if (ui.resume_requested)
      impl.fly_camera.set_capture(true);

    {
      std::lock_guard lock(impl.server.scene_mutex());
      impl.vulkan.draw_frame(impl.scene);
    }
  }

  impl.vulkan.shutdown();
}

} // namespace app
