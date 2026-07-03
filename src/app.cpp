#include "app.hpp"

#include "audio/audio_engine.hpp"
#include "debug_ui.hpp"
#include "debug_renderer.hpp"
#include "demo_assets.hpp"
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
#include <stdexcept>
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
  std::vector<app::PhysicsObjDesc> obj_descs_;
  std::uint32_t char_instance_{};
  engine::MeshSource trimesh_source_;
  engine::Scene scene;
  engine::VulkanContext vulkan;
  DemoServer server;
  DebugUi debug_ui;
  std::unique_ptr<DebugLineRenderer> debug_lines_;
  FlyCameraController fly_camera;
  InputRouter input;
  engine::audio::AudioEngine audio;
  std::uint64_t last_frame_counter{};
  bool running{true};
  bool character_mode{false};
  float physics_accumulator_{0.0F};

  explicit Impl(engine::EngineConfig config_in)
      : config(std::move(config_in)),
        window("Necromyth Engine Demo", config.window_width, config.window_height),
        scene(create_demo_scene(&obj_descs_, &char_instance_, &trimesh_source_)),
        vulkan(window.handle(), config, scene),
        server(scene, obj_descs_, char_instance_, &trimesh_source_),
        debug_ui(window.handle(), vulkan) {
    std::signal(SIGINT, on_quit_signal);
    std::signal(SIGTERM, on_quit_signal);
    fly_camera.set_window(window.handle());
    fly_camera.sync_from(scene.camera());
    last_frame_counter = SDL_GetPerformanceCounter();

    vulkan.set_frame_overlay([this](const engine::FrameOverlayContext &context) {
      debug_ui.record_overlay(context);
      if (debug_lines_ && server.debug_active()) {
        debug_lines_->draw(context.command_buffer, vulkan.frame_set_obj(context.frame_index),
                           context.frame_index, server.debug_lines(), context.extent);
      }
    });

    // Debug line renderer (Jolt wireframe overlay)
    debug_lines_ = std::make_unique<DebugLineRenderer>(
        vulkan.device_ref(), vulkan.mem_props(), vulkan.color_fmt(),
        ENGINE_DEBUG_LINE_SPIRV, vulkan.frame_layout_obj());

    std::cout << "Selected GPU: " << vulkan.gpu_name();
    if (config.gpu_device_index)
      std::cout << " (requested index " << *config.gpu_device_index << ')';
    std::cout << "\nEsc: menu / resume fly mode. WASD move, Space/C vertical, Shift sprint.\n";
    std::cout << "Menu: Resume or Quit. Debug panel: shadow toggles, FPS.\n";
    std::cout << "Audio: positional music at animationTest model\n";

    // Audio
    (void)audio.init();
    const auto music_h = audio.load_sound(asset_path("/audio/audioTest.mp3"), /*loop=*/true);
    if (music_h != engine::audio::k_invalid_sound) {
      audio.set_volume(music_h, 0.5F);
      audio.set_position(music_h, glm::vec3{0.0F, 1.5F, 0.0F});
      audio.play(music_h);
    }
  }

  ~Impl() {
    fly_camera.release_capture();
    audio.shutdown();
  }
};

DemoApp::DemoApp(engine::EngineConfig config) : impl_(std::make_unique<Impl>(std::move(config))) {}

DemoApp::~DemoApp() = default;

void DemoApp::run() {
  Impl &impl = *impl_;
  static constexpr float k_fixed_dt = 1.0F / 60.0F;

  while (impl.running) {
    if (g_quit_requested != 0)
      impl.running = false;

    const std::uint64_t now_counter = SDL_GetPerformanceCounter();
    const float delta_seconds =
        static_cast<float>(now_counter - impl.last_frame_counter) / static_cast<float>(SDL_GetPerformanceFrequency());
    impl.last_frame_counter = now_counter;

    // Advance input frame before processing events (edge detection)
    impl.input.input_map().new_frame();

    SDL_Event event{};
    while (SDL_PollEvent(&event)) {
      // Tab: toggle character mode (before InputRouter consumes the key)
      if (event.type == SDL_EVENT_KEY_DOWN && event.key.key == SDLK_TAB &&
          !impl.debug_ui.wants_keyboard()) {
        impl.character_mode = !impl.character_mode;
        impl.fly_camera.set_capture(!impl.character_mode);
        std::cout << (impl.character_mode ? "Character mode\n" : "Fly mode\n");
        continue;
      }

      if (event.type == SDL_EVENT_KEY_DOWN && event.key.key == SDLK_F3 &&
          !impl.debug_ui.wants_keyboard()) {
        impl.server.toggle_debug();
        std::cout << (impl.server.debug_active() ? "Debug wireframes ON\n" : "Debug wireframes OFF\n");
        continue;
      }

      if (event.type == SDL_EVENT_KEY_DOWN && event.key.key == SDLK_R &&
          !impl.debug_ui.wants_keyboard()) {
        impl.server.toggle_bone_override();
        continue;
      }

      if (event.type == SDL_EVENT_KEY_DOWN && event.key.key == SDLK_G &&
          !impl.debug_ui.wants_keyboard()) {
        impl.server.toggle_directional_light();
        continue;
      }

      impl.input.process_event(event, impl.debug_ui, impl.fly_camera);

      if (event.type == SDL_EVENT_MOUSE_BUTTON_DOWN && event.button.button == SDL_BUTTON_LEFT &&
          !impl.debug_ui.wants_mouse()) {
        const glm::vec3 look = impl.fly_camera.forward();
        const glm::vec3 pos = impl.scene.camera().position();
        const std::string hit = impl.server.raycast_all(pos, look);
        if (!hit.empty() && hit != "world")
          std::cout << "hit: " << hit << '\n';
      }

      if (event.type == SDL_EVENT_KEY_DOWN && event.key.key == SDLK_E &&
          !impl.debug_ui.wants_keyboard()) {
        impl.server.toggle_animation();
        continue;
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
      impl.fly_camera.update(impl.scene.camera(), delta_seconds, impl.input.input_map());

    // Read input once per frame
    float input_fwd = 0.0F;
    float input_rgt = 0.0F;
    bool jump = false;

    if (impl.character_mode && !menu_open) {
      impl.fly_camera.update_orientation(delta_seconds);
      const auto &im = impl.input.input_map();

      if (im.strength("move_forward") > 0.0F) input_fwd += 1.0F;
      if (im.strength("move_back") > 0.0F) input_fwd -= 0.7F;
      if (im.strength("move_right") > 0.0F) input_rgt += 0.7F;
      if (im.strength("move_left") > 0.0F) input_rgt -= 0.7F;
      jump = im.just_pressed("jump");

      const glm::vec3 look = impl.fly_camera.forward();
      const glm::vec3 fwd = glm::normalize(glm::vec3(look.x, 0.0F, look.z));
      const glm::vec3 rgt = glm::normalize(glm::cross(fwd, glm::vec3(0.0F, 1.0F, 0.0F)));
      const glm::vec3 world_vel = fwd * input_fwd + rgt * input_rgt;
      input_fwd = world_vel.x;
      input_rgt = world_vel.z;
    }

    // Fixed-timestep physics ticks (max 2 per frame to prevent burst jitter)
    impl.physics_accumulator_ += delta_seconds;
    int ticks = 0;
    while (impl.physics_accumulator_ >= k_fixed_dt && ticks < 2) {
      impl.server.tick(k_fixed_dt, input_fwd, input_rgt, jump);
      update_demo_scene(impl.scene);
      impl.physics_accumulator_ -= k_fixed_dt;
      ++ticks;
    }
    // Always drain residual to prevent jitter accumulation (Godot: subtraction approach)
    impl.physics_accumulator_ = std::fmod(impl.physics_accumulator_, k_fixed_dt);

    // Interpolate all physics bodies to current frame's alpha
    const float interp_alpha = impl.physics_accumulator_ / k_fixed_dt;
    impl.server.apply_interpolation(interp_alpha);

    // Camera follows character (after physics, before render)
    if (impl.character_mode) {
      const glm::vec3 char_pos = impl.server.character_position(interp_alpha);
      const glm::vec3 look_fwd = impl.fly_camera.forward();
      impl.scene.camera().look_at(
          glm::vec3(char_pos.x, char_pos.y + 1.5F, char_pos.z),
          glm::vec3(char_pos.x, char_pos.y + 1.5F, char_pos.z) + look_fwd);
    }

    // Audio listener follows camera
    impl.audio.set_listener(impl.scene.camera().position(), impl.scene.camera().look_direction(),
                            glm::vec3{0.0F, 1.0F, 0.0F});

    // Spotlight follows camera (flashlight)
    if (!impl.scene.spot_lights().empty()) {
      const glm::vec3 cam_pos = impl.scene.camera().position();
      const glm::vec3 cam_fwd = impl.scene.camera().look_direction();
      const glm::vec3 right = glm::normalize(glm::cross(cam_fwd, glm::vec3{0.0F, 1.0F, 0.0F}));
      impl.scene.spot_lights()[0].position = cam_pos + cam_fwd * 0.5F + right * 0.4F;
      // Angle slightly inward toward center and down (like holding a flashlight)
      impl.scene.spot_lights()[0].direction = glm::normalize(cam_fwd - right * 0.3F - glm::vec3{0.0F, 0.15F, 0.0F});
    }

    const UiFrameResult ui = impl.debug_ui.begin_frame(impl.scene, delta_seconds, menu_open);

    if (ui.quit_requested)
      impl.running = false;
    if (ui.resume_requested)
      impl.fly_camera.set_capture(true);

    impl.vulkan.draw_frame(impl.scene);
  }

  impl.vulkan.shutdown();
}

} // namespace app
