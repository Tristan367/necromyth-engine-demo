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
  explicit DemoServer(engine::Scene &scene, int tick_rate = 60)
      : scene_{scene}, tick_rate_{tick_rate}, physics_(1024) {
    setup_physics_test();
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
  void setup_physics_test() {
    // Ground plane matching rendering floor (20x20 quad at y=0)
    physics_.create_static_box({20.0F, 0.5F, 20.0F}, {0.0F, -0.5F, 0.0F});

    const float half = 0.5F;
    const std::array<glm::vec3, 3> positions = {
        glm::vec3{0.0F, 2.0F, 0.0F},
        glm::vec3{0.5F, 4.0F, 0.3F},
        glm::vec3{-0.4F, 6.0F, -0.2F},
    };

    for (const glm::vec3 &pos : positions) {
      const std::uint32_t mesh_idx = scene_.add_mesh(create_cube_mesh(half));
      const JPH::BodyID body_id =
          physics_.create_dynamic_box({half, half, half}, pos);

      const std::uint32_t inst_idx = scene_.add_instance({
          .mesh_index = mesh_idx,
          .texture_index = 0,
          .model = glm::translate(glm::mat4(1.0F), pos),
          .layer = engine::RenderLayer::Opaque,
      });

      physics_bodies_.push_back({body_id, inst_idx});
    }

    if (!physics_bodies_.empty())
      std::cout << "Physics: " << physics_bodies_.size() << " dynamic cubes, 1 ground plane\n";
  }

  [[nodiscard]] static auto create_cube_mesh(float half) -> engine::MeshSource {
    engine::MeshSource mesh;

    auto v = [&](float x, float y, float z, float nx, float ny, float nz, float u, float v) {
      engine::MeshVertex vert{};
      vert.pos[0] = x * half; vert.pos[1] = y * half; vert.pos[2] = z * half;
      vert.normal[0] = nx; vert.normal[1] = ny; vert.normal[2] = nz;
      vert.color[0] = 1.0F; vert.color[1] = 1.0F; vert.color[2] = 1.0F;
      vert.tex_coord[0] = u; vert.tex_coord[1] = v;
      return vert;
    };

    auto face = [&](std::uint32_t base, float nx, float ny, float nz,
                    float x0, float y0, float z0, float x1, float y1, float z1,
                    float x2, float y2, float z2, float x3, float y3, float z3) {
      mesh.vertices.push_back(v(x0, y0, z0, nx, ny, nz, 0, 0));
      mesh.vertices.push_back(v(x1, y1, z1, nx, ny, nz, 1, 0));
      mesh.vertices.push_back(v(x2, y2, z2, nx, ny, nz, 1, 1));
      mesh.vertices.push_back(v(x3, y3, z3, nx, ny, nz, 0, 1));
      mesh.indices.push_back(base + 0); mesh.indices.push_back(base + 1); mesh.indices.push_back(base + 2);
      mesh.indices.push_back(base + 0); mesh.indices.push_back(base + 2); mesh.indices.push_back(base + 3);
    };

    // +Y (top, CCW), -Y (bottom, CW reversed), +X (right, CCW), -X (left, CCW), +Z (front CW reversed), -Z (back CW reversed)
    face(0,  0, 1, 0,  -1, 1,  1,   1, 1,  1,   1, 1, -1,  -1, 1, -1);
    face(4,  0,-1, 0,  -1,-1,  1,   1,-1,  1,   1,-1, -1,  -1,-1, -1);
    face(8,  1, 0, 0,   1,-1,  1,   1,-1, -1,   1, 1, -1,   1, 1,  1);
    face(12,-1, 0, 0,  -1,-1, -1,  -1, 1, -1,  -1, 1,  1,  -1,-1,  1);
    face(16, 0, 0, 1,  -1, 1,  1,  -1,-1,  1,   1,-1,  1,   1, 1,  1);
    face(20, 0, 0,-1,   1, 1, -1,   1,-1, -1,  -1,-1, -1,  -1, 1, -1);

    return mesh;
  }

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
    // Advance animations
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

    // Step physics
    physics_.step(delta);

    // Sync physics bodies to mesh instances
    for (const PhysicsBody &pb : physics_bodies_)
      physics_.sync_body_to_instance(pb.body_id, scene_.instance(pb.instance_index));
  }

  struct PhysicsBody {
    JPH::BodyID body_id;
    std::uint32_t instance_index;
  };

  engine::Scene &scene_;
  std::thread thread_;
  std::atomic<bool> running_{false};
  std::mutex scene_mutex_;
  int tick_rate_;
  engine::physics::PhysicsWorld physics_;
  std::vector<PhysicsBody> physics_bodies_;
};
