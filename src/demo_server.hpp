#pragma once

#include "demo_scene.hpp"
#include "physics/physics_world.hpp"
#include "scene/animation_utils.hpp"
#include "scene/scene.hpp"

#include <cmath>
#include <cstdint>
#include <iostream>
#include <vector>

class DemoServer {
public:
  explicit DemoServer(engine::Scene &scene, const std::vector<app::PhysicsObjDesc> &obj_descs,
                      std::uint32_t character_instance, const engine::MeshSource *trimesh_source)
      : scene_{scene}, physics_(65536), character_instance_{character_instance} {
    if (trimesh_source && !trimesh_source->vertices.empty())
      (void)physics_.create_static_mesh(*trimesh_source, glm::vec3(0.0F, -3.0F, 0.0F));
    else
      (void)physics_.create_box({25.0F, 0.2F, 25.0F}, {0.0F, -0.2F, 0.0F},
                          JPH::EMotionType::Static, engine::physics::Layers::kNonMoving);

    for (const app::PhysicsObjDesc &desc : obj_descs) {
      const engine::MeshInstance &inst = scene_.instances()[desc.instance_index];
      const glm::vec3 pos{inst.model[3]};
      JPH::BodyID body_id;

      switch (desc.shape) {
      case app::TestObjShape::Box:
        body_id = physics_.create_box(desc.p1, pos,
                        JPH::EMotionType::Dynamic, engine::physics::Layers::kMoving,
                        glm::quat(1.0F, 0.0F, 0.0F, 0.0F), 1.0F, 0.7F);
        break;
      case app::TestObjShape::Sphere:
        body_id = physics_.add_sphere(desc.p1.x, pos);
        break;
      case app::TestObjShape::Capsule:
        body_id = physics_.add_capsule(desc.p1.x, desc.p2, pos);
        break;
      case app::TestObjShape::TaperedCapsule:
        body_id = physics_.add_tapered_capsule(desc.p1.x, desc.p2, desc.p3, pos);
        break;
      case app::TestObjShape::Cylinder:
        body_id = physics_.add_cylinder(desc.p1.x, desc.p2, pos);
        break;
      case app::TestObjShape::TaperedCylinder:
        body_id = physics_.add_tapered_cylinder(desc.p1.x, desc.p2, desc.p3, pos);
        break;
      }

      physics_bodies_.push_back({body_id, desc.instance_index});
    }

    character_ = std::make_unique<engine::physics::Character>(physics_, glm::vec3{0.0F, 20.0F, 0.0F},
                                                                0.5F, 0.8F);
    character_->set_max_strength(20.0F);
    std::cout << "Physics: " << obj_descs.size() << " objects, "
              << (trimesh_source && !trimesh_source->vertices.empty() ? "trimesh ground" : "ground plane")
              << "\nCharacter at y=20\n";
  }

  [[nodiscard]] auto character_position(float interp_alpha = 0.0F) const -> glm::vec3 {
    const float fraction = std::clamp(interp_alpha, 0.0F, 1.0F);
    return glm::mix(render_state_.prev, render_state_.curr, fraction);
  }

  void tick(float delta, float input_forward, float input_right, bool input_jump) {
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

    // Jolt sample velocity formula (CharacterVirtualTest::HandleInput)
    character_->update_ground_velocity();

    const bool grounded = character_->is_on_ground();
    const glm::vec3 ground_vel = character_->ground_velocity();
    const glm::vec3 current_vel = character_->linear_velocity();
    const glm::vec3 vert_only(0.0F, current_vel.y, 0.0F);

    glm::vec3 new_vel = grounded ? ground_vel : vert_only;
    new_vel.y += -9.81F * delta;  // always

    // Input smoothing (0.25 raw + 0.75 previous — Jolt sample inertia)
    smoothed_input_.x = 0.25F * input_forward * 5.0F + 0.75F * smoothed_input_.x;
    smoothed_input_.z = 0.25F * input_right * 5.0F + 0.75F * smoothed_input_.z;

    const bool player_moving = input_forward != 0.0F || input_right != 0.0F;
    character_->set_allow_sliding(player_moving || !grounded);
    if (grounded || player_moving) {
      new_vel.x += smoothed_input_.x;
      new_vel.z += smoothed_input_.z;
    } else {
      new_vel.x += current_vel.x;  // preserve horizontal in air when idle
      new_vel.z += current_vel.z;
    }

    if (input_jump && grounded)
      new_vel.y = 6.0F;

    // Drag only when actively moving on ground, or always in air
    if (grounded && player_moving) {
      float t = 1.0F - std::exp(-8.0F * delta);
      new_vel.x = std::lerp(new_vel.x, 0.0F, t);
      new_vel.z = std::lerp(new_vel.z, 0.0F, t);
    } else if (!grounded) {
      float t = 1.0F - std::exp(-0.5F * delta);
      new_vel.x = std::lerp(new_vel.x, 0.0F, t);
      new_vel.z = std::lerp(new_vel.z, 0.0F, t);
    }

    character_->set_velocity(new_vel);
    character_->update(delta);

    physics_.step(delta);

    const glm::vec3 char_pos = character_->position();

    render_state_.prev = render_state_.curr;
    render_state_.curr = char_pos;
    render_state_.write_time_ms = SDL_GetTicks();

    for (auto &pb : physics_bodies_) {
      physics_.sync_body_to_instance(pb.body_id, scene_.instance(pb.instance_index));
      // Store for interpolation
      const glm::vec3 p = scene_.instances()[pb.instance_index].model[3];
      pb.prev_pos = pb.curr_pos;
      pb.curr_pos = p;
    }
  }

  void apply_interpolation(float alpha) {
    alpha = std::clamp(alpha, 0.0F, 1.0F);
    for (const auto &pb : physics_bodies_) {
      const glm::vec3 p = glm::mix(pb.prev_pos, pb.curr_pos, alpha);
      glm::mat4 &m = scene_.instance(pb.instance_index).model;
      m[3] = glm::vec4(p.x, p.y, p.z, 1.0F);
    }
    // Character visual cube too
    const glm::vec3 cp = character_position(alpha);
    glm::mat4 &cm = scene_.instance(character_instance_).model;
    cm[3] = glm::vec4(cp.x, cp.y, cp.z, 1.0F);
  }

private:
  struct PhysicsEntry {
    JPH::BodyID body_id;
    std::uint32_t instance_index;
    glm::vec3 prev_pos{0, 0, 0};
    glm::vec3 curr_pos{0, 0, 0};
  };

  struct RenderState {
    glm::vec3 prev{0.0F, 20.0F, 0.0F};
    glm::vec3 curr{0.0F, 20.0F, 0.0F};
    std::uint64_t write_time_ms{};
  };

  engine::Scene &scene_;
  engine::physics::PhysicsWorld physics_;
  std::unique_ptr<engine::physics::Character> character_;
  std::uint32_t character_instance_{};
  RenderState render_state_;
  std::vector<PhysicsEntry> physics_bodies_;
  glm::vec3 smoothed_input_{0, 0, 0};
};
