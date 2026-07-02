#pragma once

#include "demo_scene.hpp"
#include "debug_renderer.hpp"
#include "physics/hitbox_manager.hpp"
#include "physics/physics_world.hpp"
#include "scene/animation_state_machine.hpp"
#include "scene/animation_utils.hpp"
#include "scene/scene.hpp"

#include <Jolt/Physics/Collision/CastResult.h>
#include <Jolt/Physics/Collision/RayCast.h>

#include <glm/gtc/quaternion.hpp>

#include <cmath>
#include <cstdint>
#include <iostream>
#include <memory>
#include <unordered_map>
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

    constexpr float k_default_char_radius = 0.5F;
    constexpr float k_default_char_height = 0.8F;
    float char_radius = k_default_char_radius;
    float char_height = k_default_char_height;

    for (const engine::SkeletonAsset &skel : scene_.skeletons()) {
      if (skel.body_collider) {
        const engine::BodyColliderDef &def = *skel.body_collider;
        if (def.shape == engine::BodyColliderDef::Shape::Capsule) {
          char_radius = def.radius;
          char_height = def.half_height * 2.0F;
        }
      }
      if (!skel.hitboxes.empty()) {
        auto mgr = std::make_unique<engine::physics::HitboxManager>(physics_, skel);
        hitbox_managers_[static_cast<std::uint32_t>(
            std::distance(scene_.skeletons().data(), &skel))] = std::move(mgr);
      }
    }

    character_ = std::make_unique<engine::physics::Character>(physics_, glm::vec3{0.0F, 20.0F, 0.0F},
                                                                char_radius, char_height);
    character_->set_max_strength(20.0F);
    std::cout << "Physics: " << obj_descs.size() << " objects, "
              << (trimesh_source && !trimesh_source->vertices.empty() ? "trimesh ground" : "ground plane")
              << "\nCharacter at y=20\n"
              << "Hitbox managers: " << hitbox_managers_.size() << "\n";

    for (engine::MeshInstance &inst : scene_.instances())
      if (inst.skin_index != engine::k_invalid_skin_index)
        inst.joint_overrides = &joint_overrides_;

    // State machine: Wriggle/idle ↔ Writhe/walk
    anim_state_.add_state({"idle", scene_.animations().size() - 2, true});
    anim_state_.add_state({"walk", scene_.animations().size() - 1, true});
    anim_state_.add_transition({"idle", "walk", "speed", engine::AnimConditionOp::Greater, 0.01F, 0.2F});
    anim_state_.add_transition({"walk", "idle", "speed", engine::AnimConditionOp::Less, 0.01F, 0.3F});
    anim_state_.start("idle");
  }

  [[nodiscard]] auto character_position(float interp_alpha = 0.0F) const -> glm::vec3 {
    const float fraction = std::clamp(interp_alpha, 0.0F, 1.0F);
    return glm::mix(render_state_.prev, render_state_.curr, fraction);
  }

  void toggle_debug() { debug_enabled_ = !debug_enabled_; }
  [[nodiscard]] auto debug_active() const -> bool { return debug_enabled_; }
  [[nodiscard]] auto debug_lines() const -> const std::vector<JoltDebugRenderer::Line> & {
    return debug_renderer_.lines();
  }

  void toggle_animation() {
    const bool split = false; // split disabled while using state machine
    for (engine::MeshInstance &instance : scene_.instances()) {
      if (instance.skin_index == engine::k_invalid_skin_index) continue;
      if (anim_state_.current() == "idle")
        anim_state_.travel(instance, "walk", split);
      else
        anim_state_.travel(instance, "idle", split);
    }
  }

  void toggle_bone_override() {
    bone_override_active_ = !bone_override_active_;
    if (!bone_override_active_)
      joint_overrides_.clear();
  }

  void update_bone_override() {
    if (!bone_override_active_)
      return;
    const float t = static_cast<float>(SDL_GetTicks()) * 0.001f;
    joint_overrides_[10] = engine::BoneTRS{
        .translation = glm::vec3{0.0F, std::sin(t * 3.0F) * 0.3F, 0.0F},
        .rotation = glm::angleAxis(std::sin(t * 10.0F) * 1.5F, glm::vec3{1.0F, 0.0F, 0.0F}),
    };
  }

  [[nodiscard]] auto raycast_hitbox(const glm::vec3 &origin, const glm::vec3 &dir) -> std::string {
    JPH::RRayCast ray{JPH::RVec3(origin.x, origin.y, origin.z),
                       JPH::Vec3(dir.x, dir.y, dir.z)};
    JPH::RayCastResult hit;
    const JPH::SpecifiedBroadPhaseLayerFilter bp_filter{engine::physics::BroadPhaseLayers::kHitbox};
    if (physics_.physics_system().GetNarrowPhaseQuery().CastRay(ray, hit, bp_filter))
      for (auto &[skin_idx, mgr] : hitbox_managers_)
        if (auto *name = mgr->find_name(hit.mBodyID))
          return *name;
    return "";
  }

  [[nodiscard]] auto raycast_all(const glm::vec3 &origin, const glm::vec3 &dir) -> std::string {
    JPH::RRayCast ray{JPH::RVec3(origin.x, origin.y, origin.z),
                       JPH::Vec3(dir.x, dir.y, dir.z)};
    JPH::RayCastResult hit;
    if (physics_.physics_system().GetNarrowPhaseQuery().CastRay(ray, hit)) {
      for (auto &[skin_idx, mgr] : hitbox_managers_)
        if (auto *name = mgr->find_name(hit.mBodyID))
          return *name;
      return "world";
    }
    return "";
  }

  void tick(float delta, float input_forward, float input_right, bool input_jump) {
    const float speed =
        std::sqrt(input_forward * input_forward + input_right * input_right);
    anim_state_.set_param("speed", speed);

    for (engine::MeshInstance &instance : scene_.instances()) {
      if (instance.skin_index == engine::k_invalid_skin_index)
        continue;
      if (instance.animation_index >= scene_.animations().size())
        continue;

      anim_state_.tick(delta, instance, scene_.animations());

      if (instance.next_animation_index < scene_.animations().size()) {
        const engine::AnimationClip &next_clip = scene_.animations()[instance.next_animation_index];
        instance.next_animation_time += delta * instance.animation_speed;
        if (instance.animation_loop && next_clip.duration > 0.0F &&
            instance.next_animation_time > next_clip.duration)
          instance.next_animation_time = std::fmod(instance.next_animation_time, next_clip.duration);

        if (!instance.secondary_joints || instance.secondary_joints->empty()) {
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
      // Body position = CoM. Shift mesh to geometric center.
      const JPH::Vec3 com = physics_.shape_center_of_mass(pb.body_id);
      if (!com.IsNearZero()) {
        const JPH::Quat rot = physics_.body_interface().GetRotation(pb.body_id);
        const JPH::Vec3 offset = -com;  // geometric center = CoM - CoM_offset
        const JPH::Vec3 world_off = rot * offset;
        scene_.instance(pb.instance_index).model[3].x += world_off.GetX();
        scene_.instance(pb.instance_index).model[3].y += world_off.GetY();
        scene_.instance(pb.instance_index).model[3].z += world_off.GetZ();
      }
      // Store for interpolation
      const glm::vec3 p = scene_.instances()[pb.instance_index].model[3];
      pb.prev_pos = pb.curr_pos;
      pb.curr_pos = p;
    }

    if (debug_enabled_) {
      debug_renderer_.clear();
      for (auto &pb : physics_bodies_) {
        JPH::BodyLockRead lock(physics_.physics_system().GetBodyLockInterface(), pb.body_id);
        if (!lock.Succeeded()) continue;
        const JPH::Shape *s = lock.GetBody().GetShape();
        if (s->GetSubType() == JPH::EShapeSubType::Mesh) continue;
        s->Draw(&debug_renderer_, lock.GetBody().GetWorldTransform(),
                JPH::Vec3::sReplicate(1.0F), JPH::Color::sGreen, false, true);
      }
      // Character
      JPH::Ref<JPH::CapsuleShape> cs(new JPH::CapsuleShape(0.4f, 0.5f));
      const glm::vec3 cp = character_->position();
      cs->Draw(&debug_renderer_,
               JPH::RMat44::sTranslation(JPH::RVec3(cp.x, cp.y, cp.z)),
               JPH::Vec3::sReplicate(1.0F), JPH::Color::sRed, false, true);
      // Hitbox bodies (already in Jolt, just needs to be drawn)
      for (auto &[skin_idx, mgr] : hitbox_managers_) {
        for (const auto &hb : mgr->hitbox_bodies()) {
          JPH::BodyLockRead lock(physics_.physics_system().GetBodyLockInterface(), hb.body_id);
          if (!lock.Succeeded()) continue;
          const JPH::Shape *s = lock.GetBody().GetShape();
          s->Draw(&debug_renderer_, lock.GetBody().GetWorldTransform(),
                  JPH::Vec3::sReplicate(1.0F), JPH::Color::sYellow, false, true);
        }
      }
    }

    update_bone_override();
    update_hitboxes();
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
  void update_hitboxes() {
    for (const engine::MeshInstance &instance : scene_.instances()) {
      if (instance.skin_index >= scene_.skeletons().size() ||
          instance.animation_index >= scene_.animations().size())
        continue;

      auto it = hitbox_managers_.find(instance.skin_index);
      if (it == hitbox_managers_.end()) continue;

      const engine::SkeletonAsset &skel = scene_.skeletons()[instance.skin_index];
      std::vector<glm::mat4> bone_worlds_local;
      std::vector<glm::mat4> unused_joint_matrices;
      engine::compute_joint_matrices_for_instance(
          skel, instance, scene_.animations(),
          unused_joint_matrices, &bone_worlds_local);

      for (glm::mat4 &bw : bone_worlds_local)
        bw = instance.model * bw;

      it->second->update(skel, bone_worlds_local);
    }
  }

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
  std::unordered_map<std::uint32_t, std::unique_ptr<engine::physics::HitboxManager>> hitbox_managers_;
  std::vector<std::uint32_t> secondary_joints_{2, 3, 4, 5, 6, 7, 8, 9, 10};
  std::unordered_map<std::uint32_t, engine::BoneTRS> joint_overrides_;
  bool bone_override_active_{false};
  engine::AnimStateMachine anim_state_;
  glm::vec3 smoothed_input_{0, 0, 0};
  JoltDebugRenderer debug_renderer_;
  bool debug_enabled_{false};
};
