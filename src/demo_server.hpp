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
      : scene_{scene}, tick_rate_{tick_rate}, physics_(65536),
        character_instance_{character_instance} {
    (void)physics_.create_box({25.0F, 0.2F, 25.0F}, {0.0F, -0.2F, 0.0F},
                        JPH::EMotionType::Static, engine::physics::Layers::kNonMoving);

    for (std::uint32_t inst_idx : cube_instances) {
      const engine::MeshInstance &inst = scene_.instances()[inst_idx];
      const glm::vec3 pos{inst.model[3]};
      const JPH::BodyID body_id = physics_.create_box({0.5F, 0.5F, 0.5F}, pos,
                                                       JPH::EMotionType::Dynamic,
                                                       engine::physics::Layers::kMoving,
                                                       glm::quat(1.0F, 0.0F, 0.0F, 0.0F),
                                                       1.0F);
      physics_bodies_.push_back({body_id, inst_idx});
    }

    character_ = std::make_unique<engine::physics::Character>(physics_, glm::vec3{0.0F, 5.0F, 0.0F});

    std::cout << "Physics: " << physics_bodies_.size() << " cubes, ground plane\nCharacter at y=5\n";
  }

  ~DemoServer() { stop(); }

  void start() {
    if (running_)
      return;
    running_ = true;
    std::cout << "Localhost server started (" << tick_rate_ << " Hz tick rate)\n";
    thread_ = std::thread(&DemoServer::loop, this);
  }

  void stop() {
    if (!running_)
      return;
    running_ = false;
    if (thread_.joinable())
      thread_.join();
    std::cout << "Localhost server stopped\n";
  }

  [[nodiscard]] auto scene_mutex() -> std::mutex & { return scene_mutex_; }

  void set_character_velocity(const glm::vec3 &velocity) {
    char_velocity_[0] = velocity.x;
    char_velocity_[1] = velocity.y;
    char_velocity_[2] = velocity.z;
  }

  [[nodiscard]] auto character_position() const -> glm::vec3 {
    return {char_position_[0].load(), char_position_[1].load(), char_position_[2].load()};
  }

private:
  void loop() {
    std::uint64_t last_tick = SDL_GetTicks();
    const std::uint64_t tick_interval_ms = 1000 / static_cast<std::uint64_t>(tick_rate_);

    while (running_) {
      const std::uint64_t now = SDL_GetTicks();
      const float delta = static_cast<float>(now - last_tick) / 1000.0F;
      last_tick = now;

      {
        std::lock_guard lock(scene_mutex_);
        tick(delta);
      }

      const std::uint64_t elapsed = SDL_GetTicks() - now;
      if (elapsed < tick_interval_ms)
        SDL_Delay(static_cast<std::uint32_t>(tick_interval_ms - elapsed));
    }
  }

  void tick(float delta) {
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

    character_->set_velocity(
        glm::vec3{char_velocity_[0].exchange(0.0F), char_velocity_[1].exchange(0.0F), char_velocity_[2].exchange(0.0F)});
    character_->update(delta);

    const glm::vec3 char_pos = character_->position();
    scene_.instance(character_instance_).model = glm::translate(glm::mat4(1.0F), char_pos);

    char_position_[0] = char_pos.x;
    char_position_[1] = char_pos.y;
    char_position_[2] = char_pos.z;

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
  int tick_rate_;
  engine::physics::PhysicsWorld physics_;
  std::unique_ptr<engine::physics::Character> character_;
  std::uint32_t character_instance_{};
  std::atomic<float> char_velocity_[3]{{0.0F}, {0.0F}, {0.0F}};
  std::atomic<float> char_position_[3]{{0.0F}, {5.0F}, {0.0F}};
  std::vector<PhysicsEntry> physics_bodies_;
};
