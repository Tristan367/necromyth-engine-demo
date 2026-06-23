#pragma once

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

class DemoServer {
public:
  explicit DemoServer(engine::Scene &scene, int tick_rate = 60)
      : scene_{scene}, tick_rate_{tick_rate} {}

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
  }

  engine::Scene &scene_;
  std::thread thread_;
  std::atomic<bool> running_{false};
  std::mutex scene_mutex_;
  int tick_rate_;
};
