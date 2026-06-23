#pragma once

#include "renderer/scene_gpu.hpp"
#include "scene/animation_utils.hpp"
#include "scene/scene.hpp"

#include <SDL3/SDL_timer.h>

#include <cmath>
#include <cstddef>
#include <cstdint>

class DemoServer {
public:
  explicit DemoServer(engine::Scene &scene) : scene_{scene} {}

  void update() {
    const std::uint64_t now = SDL_GetTicks();
    const float delta = last_ticks_ > 0
        ? static_cast<float>(now - last_ticks_) / 1000.0F
        : 0.0F;
    last_ticks_ = now;

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

private:
  engine::Scene &scene_;
  std::uint64_t last_ticks_{};
};
