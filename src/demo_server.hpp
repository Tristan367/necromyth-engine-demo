#pragma once

#include "physics/physics_world.hpp"
#include "scene/animation_utils.hpp"
#include "scene/scene.hpp"

#include <SDL3/SDL_timer.h>

#include <atomic>
#include <cmath>
#include <cstddef>
#include <cstdint>
#include <iostream>
#include <mutex>
#include <thread>
#include <vector>

class DemoServer {
public:
  explicit DemoServer(engine::Scene &scene, const std::vector<std::uint32_t> &physics_instances,
                      int tick_rate = 60)
      : scene_{scene}, tick_rate_{tick_rate}, physics_(1024) {
    (void)physics_.create_static_box({20.0F, 0.5F, 20.0F}, {0.0F, -0.5F, 0.0F});

    for (std::uint32_t inst_idx : physics_instances) {
      const engine::MeshInstance &inst = scene_.instances()[inst_idx];
      const glm::vec3 pos{inst.model[3]};
      const JPH::BodyID body_id = physics_.create_dynamic_box({0.5F, 0.5F, 0.5F}, pos);
      physics_bodies_.push_back({body_id, inst_idx});
    }

    if (!physics_bodies_.empty())
      std::cout << "Physics: " << physics_bodies_.size()
                << " dynamic cubes, 1 ground plane\n";
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
  std::vector<PhysicsEntry> physics_bodies_;
};
