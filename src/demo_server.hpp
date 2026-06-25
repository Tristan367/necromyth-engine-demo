#pragma once

#include "physics/physics_world.hpp"
#include "scene/animation_utils.hpp"
#include "scene/scene.hpp"

#include <SDL3/SDL_timer.h>

#include <atomic>
#include <cmath>
#include <cstdint>
#include <iostream>
#include <mutex>
#include <thread>
#include <vector>

class DemoServer {
public:
  explicit DemoServer(engine::Scene &scene, const std::vector<std::uint32_t> &cube_instances,
                      std::uint32_t character_instance, int tick_rate = 60)
      : scene_{scene}, physics_(65536), character_instance_{character_instance} {
    (void)physics_.create_box({25.0F, 0.2F, 25.0F}, {0.0F, -0.2F, 0.0F},
                        JPH::EMotionType::Static, engine::physics::Layers::kNonMoving);

    for (std::uint32_t inst_idx : cube_instances) {
      const engine::MeshInstance &inst = scene_.instances()[inst_idx];
      const glm::vec3 pos{inst.model[3]};
      physics_bodies_.push_back({
          physics_.create_box({0.5F, 0.5F, 0.5F}, pos,
                              JPH::EMotionType::Dynamic, engine::physics::Layers::kMoving,
                              glm::quat(1.0F, 0.0F, 0.0F, 0.0F), 1.0F),
          inst_idx});
    }

    character_ = std::make_unique<engine::physics::Character>(physics_, glm::vec3{0.0F, 5.0F, 0.0F},
                                                                0.5F, 0.8F);

    std::cout << "Physics: " << physics_bodies_.size() << " cubes, ground plane\n";
  }

  ~DemoServer() { stop(); }

  void start() {
    if (running_)
      return;
    running_ = true;
    std::cout << "Localhost server started\n";
    thread_ = std::thread(&DemoServer::loop, this);
  }

  void stop() {
    if (!running_)
      return;
    running_ = false;
    if (thread_.joinable())
      thread_.join();
  }

  [[nodiscard]] auto scene_mutex() -> std::mutex & { return scene_mutex_; }

  // Called from main thread — sets horizontal velocity and jump flag.
  // The server tick reads these atomically and zeroes them.
  void set_input(float forward, float right, bool jump) {
    input_forward_ = forward;
    input_right_ = right;
    input_jump_ = jump;
  }

  [[nodiscard]] auto character_position() -> glm::vec3 {
    std::lock_guard lock(scene_mutex_);
    const float elapsed = static_cast<float>(SDL_GetTicks() - render_state_.write_time_ms);
    const float fraction = std::clamp(elapsed / (1000.0F / 60.0F), 0.0F, 1.0F);
    return glm::mix(render_state_.prev, render_state_.curr, fraction);
  }

private:
  void loop() {
    static constexpr float k_fixed_dt = 1.0F / 60.0F;
    static constexpr float k_max_frame_time = 0.25F;

    std::uint64_t last_time = SDL_GetTicks();
    float accumulator = 0.0F;

    while (running_) {
      const std::uint64_t now = SDL_GetTicks();
      float frame_time = static_cast<float>(now - last_time) / 1000.0F;
      last_time = now;

      if (frame_time > k_max_frame_time)
        frame_time = k_max_frame_time;

      accumulator += frame_time;

      while (accumulator >= k_fixed_dt) {
        {
          std::lock_guard lock(scene_mutex_);
          tick(k_fixed_dt);
        }
        accumulator -= k_fixed_dt;
      }

      SDL_Delay(1);
    }
  }

  void tick(float delta) {
    // Animation update
    for (engine::MeshInstance &instance : scene_.instances()) {
      if (instance.skin_index == engine::k_invalid_skin_index)
        continue;
      if (instance.animation_index >= scene_.animations().size())
        continue;

      const engine::AnimationClip &clip = scene_.animations()[instance.animation_index];
      instance.animation_time += delta * instance.animation_speed;
      if (instance.animation_loop && clip.duration > 0.0F && instance.animation_time > clip.duration)
        instance.animation_time = std::fmod(instance.animation_time, clip.duration);

      if (instance.next_animation_index < scene_.animations().size()) {
        const engine::AnimationClip &next_clip = scene_.animations()[instance.next_animation_index];
        instance.next_animation_time += delta * instance.animation_speed;
        if (instance.animation_loop && next_clip.duration > 0.0F &&
            instance.next_animation_time > next_clip.duration)
          instance.next_animation_time = std::fmod(instance.next_animation_time, next_clip.duration);

        instance.blend_factor += delta / instance.blend_duration;
        if (instance.blend_factor >= 1.0F) {
          instance.animation_index = instance.next_animation_index;
          instance.animation_time = instance.next_animation_time;
          instance.next_animation_index = std::numeric_limits<std::uint32_t>::max();
          instance.blend_factor = 1.0F;
        }
      }
    }

    physics_.step(delta);

    character_->update_ground_velocity();

    glm::vec3 vel = character_->linear_velocity();
    const bool grounded = character_->is_on_ground();

    // Acceleration — always additive, never sets velocity
    const float air_speed = grounded ? 30.0F : 1.5F;
    vel.x += input_forward_ * air_speed * delta;
    vel.z += input_right_ * air_speed * delta;

    // Jump
    if (input_jump_ && grounded)
      vel.y = 6.0F;

    // Gravity (only in air)
    if (!grounded)
      vel.y += -9.81F * delta;

    // Friction/Drag
    float drag = grounded ? 8.0F : 0.5F;
    float t = 1.0F - std::exp(-drag * delta);
    vel.x = std::lerp(vel.x, 0.0F, t);
    vel.z = std::lerp(vel.z, 0.0F, t);

    character_->set_velocity(vel);
    character_->update(delta);

    const glm::vec3 char_pos = character_->position();
    scene_.instance(character_instance_).model = glm::translate(glm::mat4(1.0F), char_pos);

    render_state_.prev = render_state_.curr;
    render_state_.curr = char_pos;
    render_state_.write_time_ms = SDL_GetTicks();

    for (const auto &pb : physics_bodies_)
      physics_.sync_body_to_instance(pb.body_id, scene_.instance(pb.instance_index));
  }

  struct PhysicsEntry {
    JPH::BodyID body_id;
    std::uint32_t instance_index;
  };

  engine::Scene &scene_;
  std::thread thread_;
  std::atomic<bool> running_{false};
  std::mutex scene_mutex_;
  engine::physics::PhysicsWorld physics_;
  std::unique_ptr<engine::physics::Character> character_;
  std::uint32_t character_instance_{};
  std::atomic<float> input_forward_{0.0F};
  std::atomic<float> input_right_{0.0F};
  std::atomic<bool> input_jump_{false};
  std::vector<PhysicsEntry> physics_bodies_;

  struct RenderState {
    glm::vec3 prev{0.0F, 5.0F, 0.0F};
    glm::vec3 curr{0.0F, 5.0F, 0.0F};
    std::uint64_t write_time_ms{};
  } render_state_;
};
