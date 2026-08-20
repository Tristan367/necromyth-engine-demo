#pragma once

#include "scene/scene.hpp"

#include <glm/mat4x4.hpp>

#include <cstdint>

namespace app {

// Thin wrapper over engine::InstanceHandle. The handle carries a generation, so
// this stays correct when instances are removed and their slots reused -- which
// the previous bare-index version explicitly did not.
class SceneInstance {
public:
  SceneInstance() = default;

  explicit SceneInstance(engine::InstanceHandle handle) : handle_(handle) {}

  [[nodiscard]] auto handle() const -> engine::InstanceHandle { return handle_; }

  [[nodiscard]] auto valid() const -> bool { return handle_.is_set(); }

  void set_model(engine::Scene &scene, const glm::mat4 &model) const {
    if (engine::MeshInstance *instance = scene.try_instance(handle_))
      instance->model = model;
  }

private:
  engine::InstanceHandle handle_{};
};

} // namespace app
