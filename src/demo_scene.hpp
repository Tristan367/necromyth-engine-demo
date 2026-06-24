#pragma once

#include "scene/scene.hpp"

#include <cstdint>
#include <vector>

namespace app {

void populate_demo_scene(engine::Scene &scene);
[[nodiscard]] auto create_demo_scene(std::vector<std::uint32_t> *out_physics_indices = nullptr) -> engine::Scene;
void update_demo_scene(engine::Scene &scene);
void toggle_demo_animation(engine::Scene &scene);

} // namespace app
