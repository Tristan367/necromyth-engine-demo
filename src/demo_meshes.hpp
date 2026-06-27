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

[[nodiscard]] inline auto make_box_mesh(float hx, float hy, float hz) -> engine::MeshSource {
  engine::MeshSource mesh;

  auto v = [&](float x, float y, float z, float nx, float ny, float nz) {
    engine::MeshVertex vert{};
    vert.pos[0] = x * hx; vert.pos[1] = y * hy; vert.pos[2] = z * hz;
    vert.normal[0] = nx; vert.normal[1] = ny; vert.normal[2] = nz;
    vert.color[0] = 1.0F; vert.color[1] = 1.0F; vert.color[2] = 1.0F;
    return vert;
  };

  auto quad = [&](float nx, float ny, float nz,
                  float x0, float y0, float z0, float x1, float y1, float z1,
                  float x2, float y2, float z2, float x3, float y3, float z3) {
    std::uint32_t base = static_cast<std::uint32_t>(mesh.vertices.size());
    mesh.vertices.push_back(v(x0, y0, z0, nx, ny, nz));
    mesh.vertices.push_back(v(x1, y1, z1, nx, ny, nz));
    mesh.vertices.push_back(v(x2, y2, z2, nx, ny, nz));
    mesh.vertices.push_back(v(x3, y3, z3, nx, ny, nz));
    mesh.indices.push_back(base + 0); mesh.indices.push_back(base + 1); mesh.indices.push_back(base + 2);
    mesh.indices.push_back(base + 0); mesh.indices.push_back(base + 2); mesh.indices.push_back(base + 3);
  };

  quad( 0, 1, 0,  -1, 1,  1,   1, 1,  1,   1, 1, -1,  -1, 1, -1);
  quad( 0,-1, 0,  -1,-1, -1,   1,-1, -1,   1,-1,  1,  -1,-1,  1);
  quad( 1, 0, 0,   1,-1,  1,   1,-1, -1,   1, 1, -1,   1, 1,  1);
  quad(-1, 0, 0,  -1,-1, -1,  -1,-1,  1,  -1, 1,  1,  -1, 1, -1);
  quad( 0, 0, 1,  -1, 1,  1,  -1,-1,  1,   1,-1,  1,   1, 1,  1);
  quad( 0, 0,-1,   1, 1, -1,   1,-1, -1,  -1,-1, -1,  -1, 1, -1);

  return mesh;
}

inline auto make_cube_mesh(float half = 0.5F) -> engine::MeshSource {
  return make_box_mesh(half, half, half);
}

[[nodiscard]] inline auto make_sphere_mesh(float radius, int rings = 12, int segments = 16) -> engine::MeshSource {
  engine::MeshSource mesh;

  auto v = [&](float x, float y, float z) {
    engine::MeshVertex vert{};
    vert.pos[0] = x; vert.pos[1] = y; vert.pos[2] = z;
    vert.normal[0] = x / radius; vert.normal[1] = y / radius; vert.normal[2] = z / radius;
    vert.color[0] = 1.0F; vert.color[1] = 1.0F; vert.color[2] = 1.0F;
    return vert;
  };

  const float pi = 3.14159265F;
  const float two_pi = 2.0F * pi;

  for (int ring = 0; ring <= rings; ++ring) {
    float phi = pi * static_cast<float>(ring) / static_cast<float>(rings);
    float y = radius * std::cos(phi);
    float r = radius * std::sin(phi);

    for (int seg = 0; seg <= segments; ++seg) {
      float theta = two_pi * static_cast<float>(seg) / static_cast<float>(segments);
      mesh.vertices.push_back(v(r * std::cos(theta), y, r * std::sin(theta)));
    }
  }

  for (int ring = 0; ring < rings; ++ring) {
    int base = ring * (segments + 1);
    int next = (ring + 1) * (segments + 1);
    for (int seg = 0; seg < segments; ++seg) {
      mesh.indices.push_back(base + seg);
      mesh.indices.push_back(next + seg + 1);
      mesh.indices.push_back(next + seg);
      mesh.indices.push_back(base + seg);
      mesh.indices.push_back(base + seg + 1);
      mesh.indices.push_back(next + seg + 1);
    }
  }

  return mesh;
}

[[nodiscard]] inline auto make_capsule_mesh(
    float top_radius, float bottom_radius, float half_height,
    int segments = 16, int rings = 8) -> engine::MeshSource {
  engine::MeshSource mesh;

  auto v = [&](float x, float y, float z, float nx, float ny, float nz) {
    engine::MeshVertex vert{};
    vert.pos[0] = x; vert.pos[1] = y; vert.pos[2] = z;
    vert.normal[0] = nx; vert.normal[1] = ny; vert.normal[2] = nz;
    vert.color[0] = 1.0F; vert.color[1] = 1.0F; vert.color[2] = 1.0F;
    return vert;
  };

  const float bottom = -half_height - bottom_radius;
  const float top = half_height + top_radius;
  const float two_pi = 2.0F * 3.14159265F;
  const int total_rings = rings * 2 + 2;

  for (int ring = 0; ring <= total_rings; ++ring) {
    const float y = bottom + (top - bottom) * static_cast<float>(ring) / static_cast<float>(total_rings);

    float r, ny;
    if (y < -half_height) {
      const float dy = y + half_height;
      r = std::sqrt(bottom_radius * bottom_radius - dy * dy);
      ny = -dy / bottom_radius;
    } else if (y > half_height) {
      const float dy = y - half_height;
      r = std::sqrt(top_radius * top_radius - dy * dy);
      ny = dy / top_radius;
    } else {
      float t = (y + half_height) / (2.0F * half_height);
      r = bottom_radius + (top_radius - bottom_radius) * t;
      ny = 0.0F;
    }

    for (int seg = 0; seg < segments; ++seg) {
      const float theta = two_pi * static_cast<float>(seg) / static_cast<float>(segments);
      const float x = r * std::cos(theta);
      const float z = r * std::sin(theta);
      const float nx = r > 0.001F ? x / r : 0.0F;
      const float nz = r > 0.001F ? z / r : 0.0F;
      mesh.vertices.push_back(v(x, y, z, nx, ny, nz));
    }
  }

  for (int ring = 0; ring < total_rings; ++ring) {
    const int base = ring * segments;
    const int next_base = (ring + 1) * segments;
    for (int seg = 0; seg < segments; ++seg) {
      const int curr = base + seg;
      const int next_seg = base + (seg + 1) % segments;
      const int next_curr = next_base + seg;
      const int next_next = next_base + (seg + 1) % segments;
      mesh.indices.push_back(curr);
      mesh.indices.push_back(next_next);
      mesh.indices.push_back(next_curr);
      mesh.indices.push_back(curr);
      mesh.indices.push_back(next_seg);
      mesh.indices.push_back(next_next);
    }
  }

  return mesh;
}

inline auto make_capsule_mesh(float radius, float half_height,
                               int segments = 16, int rings = 8) -> engine::MeshSource {
  return make_capsule_mesh(radius, radius, half_height, segments, rings);
}

[[nodiscard]] inline auto make_cylinder_mesh(float top_radius, float bottom_radius, float half_height,
                                              int segments = 16) -> engine::MeshSource {
  engine::MeshSource mesh;

  auto v = [&](float x, float y, float z, float nx, float ny, float nz) {
    engine::MeshVertex vert{};
    vert.pos[0] = x; vert.pos[1] = y; vert.pos[2] = z;
    vert.normal[0] = nx; vert.normal[1] = ny; vert.normal[2] = nz;
    vert.color[0] = 1.0F; vert.color[1] = 1.0F; vert.color[2] = 1.0F;
    return vert;
  };

  const float two_pi = 2.0F * 3.14159265F;

  // Body vertices: bottom ring + top ring
  for (int seg = 0; seg <= segments; ++seg) {
    const float theta = two_pi * static_cast<float>(seg) / static_cast<float>(segments);
    const float xb = bottom_radius * std::cos(theta);
    const float zb = bottom_radius * std::sin(theta);
    const float xt = top_radius * std::cos(theta);
    const float zt = top_radius * std::sin(theta);
    mesh.vertices.push_back(v(xb, -half_height, zb, xb / bottom_radius, 0, zb / bottom_radius));
    mesh.vertices.push_back(v(xt, half_height, zt, xt / top_radius, 0, zt / top_radius));
  }

  // Body quads
  for (int seg = 0; seg < segments; ++seg) {
    int b0 = seg * 2, b1 = (seg + 1) * 2;
    mesh.indices.push_back(b0); mesh.indices.push_back(b1 + 1); mesh.indices.push_back(b1);
    mesh.indices.push_back(b0); mesh.indices.push_back(b0 + 1); mesh.indices.push_back(b1 + 1);
  }

  // Bottom cap
  int bottom_center = static_cast<int>(mesh.vertices.size());
  mesh.vertices.push_back(v(0, -half_height, 0, 0, -1, 0));
  for (int seg = 0; seg < segments; ++seg) {
    int a = seg * 2, b = (seg + 1) * 2;
    mesh.indices.push_back(bottom_center);
    mesh.indices.push_back(a);
    mesh.indices.push_back(b);
  }

  // Top cap
  int top_center = static_cast<int>(mesh.vertices.size());
  mesh.vertices.push_back(v(0, half_height, 0, 0, 1, 0));
  for (int seg = 0; seg < segments; ++seg) {
    int a = seg * 2 + 1, b = (seg + 1) * 2 + 1;
    mesh.indices.push_back(top_center);
    mesh.indices.push_back(b);
    mesh.indices.push_back(a);
  }

  return mesh;
}

} // namespace app
