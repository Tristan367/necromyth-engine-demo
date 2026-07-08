#include "app.hpp"

#include "audio/audio_engine.hpp"
#include "debug_ui.hpp"
#include "debug_renderer.hpp"
#include "demo_assets.hpp"
#include "demo_scene.hpp"
#include "demo_server.hpp"
#include "fly_camera.hpp"
#include "input_router.hpp"
#include "platform/engine_runtime.hpp"
#include "renderer/vulkan_context.hpp"
#include "scene/scene.hpp"

#include <SDL3/SDL_events.h>
#include <SDL3/SDL_timer.h>

#include <iostream>
#include <stdexcept>
#include <utility>
#include <vector>

namespace app {

struct DemoApp::Impl {
  engine::EngineConfig config;
  std::vector<app::PhysicsObjDesc> obj_descs_;
  std::uint32_t char_instance_{};
  engine::MeshSource trimesh_source_;
  engine::Scene scene;
  engine::EngineRuntime runtime;
  DemoServer server;
  DebugUi debug_ui;
  std::unique_ptr<DebugLineRenderer> debug_lines_;
  FlyCameraController fly_camera;
  InputRouter input;
  engine::audio::AudioEngine audio;
  std::uint64_t last_frame_counter{};
  bool running{true};
  bool character_mode{false};
  std::uint32_t snow_emitter_{};

  explicit Impl(engine::EngineConfig config_in)
      : config(std::move(config_in)),
        scene(create_demo_scene(&obj_descs_, &char_instance_, &trimesh_source_)),
        runtime(config, scene),
        server(scene, obj_descs_, char_instance_, &trimesh_source_),
        debug_ui(runtime.window_handle(), runtime.vulkan()) {
    fly_camera.set_window(runtime.window_handle());
    fly_camera.sync_from(scene.camera());
    last_frame_counter = SDL_GetPerformanceCounter();

    runtime.vulkan().set_frame_overlay([this](const engine::FrameOverlayContext &context) {
      debug_ui.record_overlay(context);
      if (debug_lines_ && server.debug_active()) {
        debug_lines_->draw(context.command_buffer, runtime.vulkan().frame_set_obj(context.frame_index),
                           context.frame_index, server.debug_lines(), context.extent);
      }
    });

    debug_lines_ = std::make_unique<DebugLineRenderer>(
        runtime.vulkan().device_ref(), runtime.vulkan().mem_props(), runtime.vulkan().color_fmt(),
        ENGINE_DEBUG_LINE_SPIRV, runtime.vulkan().frame_layout_obj());

    std::cout << "Selected GPU: " << runtime.vulkan().gpu_name();
    if (config.gpu_device_index)
      std::cout << " (requested index " << *config.gpu_device_index << ')';
    std::cout << "\nEsc: menu / resume fly mode. WASD move, Space/C vertical, Shift sprint.\n";
    std::cout << "Menu: Resume or Quit. Debug panel: shadow toggles, FPS.\n";
    std::cout << "Audio: positional music at animationTest model\n";

    (void)audio.init();  // no-op now, auto-initialized in constructor
    const auto music_h = audio.load_sound(asset_path("/audio/audioTest.mp3"), /*loop=*/true);
    if (music_h != engine::audio::k_invalid_sound) {
      audio.set_volume(music_h, 0.5F);
      audio.set_position(music_h, glm::vec3{0.0F, 1.5F, 0.0F});
      audio.play(music_h);
    }

    // Snow particle emitter — spawns particles in a 16m area above the camera
    auto &ps = runtime.vulkan().particle_system();
    snow_emitter_ = ps.add_emitter({
        .position = {0.0F, 20.0F, 0.0F},
        .rate = 2000.0F,
        .on_emit = [](engine::ParticleSystem::Particle &p) {
          p.pos += glm::vec3{(rand() % 2000 - 1000) * 0.016F, 0.0F, (rand() % 2000 - 1000) * 0.016F};
          p.vel = glm::vec3{(rand() % 300 - 150) * 0.003F, -0.8F - (rand() % 100) * 0.004F,
                            (rand() % 300 - 150) * 0.003F};
          p.lifetime = 20.0F;
        },
        .on_update = [](engine::ParticleSystem::Particle &p, float dt) -> bool {
          p.pos += p.vel * dt;
          return p.pos.y > -2.0F;
        },
    });
  }

  ~Impl() {
    fly_camera.release_capture();
  }
};

DemoApp::DemoApp(engine::EngineConfig config) : impl_(std::make_unique<Impl>(std::move(config))) {}

DemoApp::~DemoApp() = default;

void DemoApp::run() {
  Impl &impl = *impl_;
  static constexpr double k_fixed_dt = 1.0 / 60.0;

  impl.runtime.timer().reset();

  while (impl.running) {
    if (engine::EngineRuntime::quit_requested())
      impl.running = false;

    const std::uint64_t now_counter = SDL_GetPerformanceCounter();
    const float delta_seconds =
        static_cast<float>(now_counter - impl.last_frame_counter) / static_cast<float>(SDL_GetPerformanceFrequency());
    impl.last_frame_counter = now_counter;

    impl.input.input_map().new_frame();

    SDL_Event event{};
    while (SDL_PollEvent(&event)) {
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
        impl.runtime.vulkan().mark_framebuffer_resized();
    }

    if (impl.input.quit_requested())
      impl.running = false;

    if (!impl.running)
      break;

    const bool menu_open = !impl.fly_camera.capture_active();

    if (!impl.character_mode && impl.input.should_update_camera(impl.debug_ui, impl.fly_camera))
      impl.fly_camera.update(impl.scene.camera(), delta_seconds, impl.input.input_map());

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

      const auto [fwd, rgt] = impl.scene.camera().horizontal_basis();
      const glm::vec3 world_vel = fwd * input_fwd + rgt * input_rgt;
      input_fwd = world_vel.x;
      input_rgt = world_vel.z;
    }

    const auto sync_result = impl.runtime.timer().advance(delta_seconds);
    for (int i = 0; i < sync_result.physics_steps; ++i) {
      impl.server.tick(static_cast<float>(k_fixed_dt), input_fwd, input_rgt, jump);
      update_demo_scene(impl.scene);
    }

    impl.runtime.vulkan().particle_system().update(delta_seconds);

    const float interp_alpha = static_cast<float>(sync_result.interpolation_fraction);
    impl.server.apply_interpolation(interp_alpha);

    // Camera follows character (after physics, before render)
    if (impl.character_mode) {
      const glm::vec3 char_pos = impl.server.character_position(interp_alpha);
      impl.scene.camera().follow(char_pos, impl.fly_camera.forward());
    }

    // Audio listener follows camera
    impl.audio.set_listener(impl.scene.camera().position(), impl.scene.camera().look_direction(),
                             glm::vec3{0.0F, 1.0F, 0.0F});

    // Point light follows character capsule (above head)
    if (!impl.scene.point_lights().empty()) {
      const glm::vec3 char_pos = impl.server.character_position(interp_alpha);
      impl.scene.point_lights()[0].position = glm::vec3(char_pos.x, char_pos.y + 1.5F, char_pos.z);
    }

    // Spotlight follows camera (flashlight)
    if (!impl.scene.spot_lights().empty()) {
      const glm::vec3 cam_pos = impl.scene.camera().position();
      const glm::vec3 cam_fwd = impl.scene.camera().look_direction();
      const glm::vec3 rgt = impl.scene.camera().right();
      impl.scene.spot_lights()[0].position = cam_pos + cam_fwd * 0.5F + rgt * 0.4F;
      impl.scene.spot_lights()[0].direction = glm::normalize(cam_fwd - rgt * 0.3F - glm::vec3{0.0F, 0.15F, 0.0F});
    }

    const UiFrameResult ui = impl.debug_ui.begin_frame(impl.scene, delta_seconds, menu_open);

    if (ui.quit_requested)
      impl.running = false;
    if (ui.resume_requested)
      impl.fly_camera.set_capture(true);

    impl.runtime.vulkan().draw_frame(impl.scene);
  }

  impl.runtime.shutdown();
}

} // namespace app
