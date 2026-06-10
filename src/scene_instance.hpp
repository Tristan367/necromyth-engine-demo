#pragma once

#include "scene/scene.hpp"

#include <glm/mat4x4.hpp>

#include <cstdint>

namespace app {

// Stable handle to a MeshInstance index; valid while instances are only appended.
class SceneInstance {
public:
  SceneInstance() = default;

  explicit SceneInstance(std::uint32_t index) : index_(index) {}

  [[nodiscard]] auto valid() const -> bool {
    return index_ != k_invalid;
  }

  void set_model(engine::Scene &scene, const glm::mat4 &model) const {
    if (!valid())
      return;

    scene.instance(index_).model = model;
  }

private:
  static constexpr std::uint32_t k_invalid = UINT32_MAX;
  std::uint32_t index_{k_invalid};
};

} // namespace app
