#pragma once

namespace engine {
class Scene;
}

namespace app {

void populate_demo_scene(engine::Scene &scene);
[[nodiscard]] auto create_demo_scene() -> engine::Scene;
void update_demo_scene(engine::Scene &scene);

} // namespace app
