#pragma once

#include "renderer/vertex.hpp"
#include "scene/mesh_source.hpp"

#include <cstddef>

namespace app {

[[nodiscard]] inline auto make_floor_quad_mesh(float half_extent = 20.0F, float uv_tiles = 8.0F) -> engine::MeshSource {
  static constexpr float white[] = {1.0F, 1.0F, 1.0F};
  static constexpr float up[] = {0.0F, 1.0F, 0.0F};

  const engine::MeshVertex vertices[] = {
      {{-half_extent, 0.0F, -half_extent}, {up[0], up[1], up[2]}, {white[0], white[1], white[2]}, {0.0F, 0.0F}, {}, {}},
      {{half_extent, 0.0F, -half_extent}, {up[0], up[1], up[2]}, {white[0], white[1], white[2]}, {uv_tiles, 0.0F}, {}, {}},
      {{half_extent, 0.0F, half_extent}, {up[0], up[1], up[2]}, {white[0], white[1], white[2]}, {uv_tiles, uv_tiles}, {}, {}},
      {{-half_extent, 0.0F, half_extent}, {up[0], up[1], up[2]}, {white[0], white[1], white[2]}, {0.0F, uv_tiles}, {}, {}},
  };

  static constexpr std::uint32_t indices[] = {0, 2, 1, 0, 3, 2};

  return {
      .vertices = {vertices, vertices + std::size(vertices)},
      .indices = {indices, indices + std::size(indices)},
  };
}

// Vertical quad in the XY plane (normal +Z), bottom edge on Y = 0.
[[nodiscard]] inline auto make_upright_quad_mesh(float width = 1.0F, float height = 1.0F) -> engine::MeshSource {
  static constexpr float white[] = {1.0F, 1.0F, 1.0F};
  static constexpr float normal[] = {0.0F, 0.0F, 1.0F};

  const float half_width = width * 0.5F;

  const engine::MeshVertex vertices[] = {
      {{-half_width, 0.0F, 0.0F}, {normal[0], normal[1], normal[2]}, {white[0], white[1], white[2]}, {0.0F, 1.0F}, {}, {}},
      {{half_width, 0.0F, 0.0F}, {normal[0], normal[1], normal[2]}, {white[0], white[1], white[2]}, {1.0F, 1.0F}, {}, {}},
      {{half_width, height, 0.0F}, {normal[0], normal[1], normal[2]}, {white[0], white[1], white[2]}, {1.0F, 0.0F}, {}, {}},
      {{-half_width, height, 0.0F}, {normal[0], normal[1], normal[2]}, {white[0], white[1], white[2]}, {0.0F, 0.0F}, {}, {}},
  };

  static constexpr std::uint32_t indices[] = {0, 1, 2, 0, 2, 3};

  return {
      .vertices = {vertices, vertices + std::size(vertices)},
      .indices = {indices, indices + std::size(indices)},
  };
}

[[nodiscard]] inline auto make_cube_mesh(float half = 0.5F) -> engine::MeshSource {
  engine::MeshSource mesh;

  auto v = [&](float x, float y, float z, float nx, float ny, float nz, float u, float uv) {
    engine::MeshVertex vert{};
    vert.pos[0] = x * half; vert.pos[1] = y * half; vert.pos[2] = z * half;
    vert.normal[0] = nx; vert.normal[1] = ny; vert.normal[2] = nz;
    vert.color[0] = 1.0F; vert.color[1] = 1.0F; vert.color[2] = 1.0F;
    vert.tex_coord[0] = u; vert.tex_coord[1] = uv;
    return vert;
  };

  auto quad = [&](float nx, float ny, float nz,
                  float x0, float y0, float z0, float x1, float y1, float z1,
                  float x2, float y2, float z2, float x3, float y3, float z3) {
    std::uint32_t base = static_cast<std::uint32_t>(mesh.vertices.size());
    mesh.vertices.push_back(v(x0, y0, z0, nx, ny, nz, 0, 0));
    mesh.vertices.push_back(v(x1, y1, z1, nx, ny, nz, 1, 0));
    mesh.vertices.push_back(v(x2, y2, z2, nx, ny, nz, 1, 1));
    mesh.vertices.push_back(v(x3, y3, z3, nx, ny, nz, 0, 1));
    mesh.indices.push_back(base + 0); mesh.indices.push_back(base + 1); mesh.indices.push_back(base + 2);
    mesh.indices.push_back(base + 0); mesh.indices.push_back(base + 2); mesh.indices.push_back(base + 3);
  };

  // +Y top    (CCW from above)
  quad( 0, 1, 0,  -1, 1,  1,   1, 1,  1,   1, 1, -1,  -1, 1, -1);
  // -Y bottom (CCW from below)
  quad( 0,-1, 0,  -1,-1, -1,   1,-1, -1,   1,-1,  1,  -1,-1,  1);
  // +X right  (CCW from right)
  quad( 1, 0, 0,   1,-1,  1,   1,-1, -1,   1, 1, -1,   1, 1,  1);
  // -X left   (CCW from left)
  quad(-1, 0, 0,  -1,-1, -1,  -1,-1,  1,  -1, 1,  1,  -1, 1, -1);
  // +Z front  (CCW from front)
  quad( 0, 0, 1,  -1, 1,  1,  -1,-1,  1,   1,-1,  1,   1, 1,  1);
  // -Z back   (CCW from back)
  quad( 0, 0,-1,   1, 1, -1,   1,-1, -1,  -1,-1, -1,  -1, 1, -1);

  return mesh;
}

[[nodiscard]] inline auto make_capsule_mesh(float radius, float half_height, int segments = 16, int rings = 8) -> engine::MeshSource {
  engine::MeshSource mesh;

  auto vert = [&](float x, float y, float z, float nx, float ny, float nz) {
    engine::MeshVertex v{};
    v.pos[0] = x; v.pos[1] = y; v.pos[2] = z;
    v.normal[0] = nx; v.normal[1] = ny; v.normal[2] = nz;
    v.color[0] = 1.0F; v.color[1] = 1.0F; v.color[2] = 1.0F;
    return v;
  };

  const float bottom = -half_height - radius;
  const float top = half_height + radius;
  const float two_pi = 2.0F * 3.14159265F;
  const int total_rings = rings * 2 + 2; // rings for each hemisphere + 2 for cylinder ends

  // Generate vertex rings from bottom to top
  for (int ring = 0; ring <= total_rings; ++ring) {
    const float y = bottom + (top - bottom) * static_cast<float>(ring) / static_cast<float>(total_rings);

    float r, ny;
    if (y < -half_height) {
      const float dy = y + half_height;
      r = std::sqrt(radius * radius - dy * dy);
      ny = -dy / radius; // normal Y based on sphere center
    } else if (y > half_height) {
      const float dy = y - half_height;
      r = std::sqrt(radius * radius - dy * dy);
      ny = dy / radius;
    } else {
      r = radius;
      ny = 0.0F;
    }

    for (int seg = 0; seg < segments; ++seg) {
      const float theta = two_pi * static_cast<float>(seg) / static_cast<float>(segments);
      const float x = r * std::cos(theta);
      const float z = r * std::sin(theta);
      const float nx = r > 0.001F ? x / r : 0.0F;
      const float nz = r > 0.001F ? z / r : 0.0F;
      mesh.vertices.push_back(vert(x, y, z, nx, ny, nz));
    }
  }

  // Generate indices (quads between rings)
  for (int ring = 0; ring < total_rings; ++ring) {
    const int base = ring * segments;
    const int next_base = (ring + 1) * segments;
    for (int seg = 0; seg < segments; ++seg) {
      const int curr = base + seg;
      const int next_seg = base + (seg + 1) % segments;
      const int next_curr = next_base + seg;
      const int next_next = next_base + (seg + 1) % segments;
      mesh.indices.push_back(curr);
      mesh.indices.push_back(next_curr);
      mesh.indices.push_back(next_next);
      mesh.indices.push_back(curr);
      mesh.indices.push_back(next_next);
      mesh.indices.push_back(next_seg);
    }
  }

  return mesh;
}

} // namespace app
