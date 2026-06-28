#pragma once

#include <Jolt/Jolt.h>
#include <Jolt/Renderer/DebugRenderer.h>

#include "renderer/buffer.hpp"
#include "renderer/graphics_pipeline.hpp"

#include <vulkan/vulkan_raii.hpp>

#include <glm/vec3.hpp>

#include <array>
#include <cstddef>
#include <cstdint>
#include <cstring>
#include <optional>
#include <vector>

#include <atomic>

namespace {

struct BatchImpl final : public JPH::RefTargetVirtual {
  JPH_OVERRIDE_NEW_DELETE
  JPH::Array<JPH::DebugRenderer::Triangle> triangles;
  void AddRef() override { ++ref_; }
  void Release() override { if (--ref_ == 0) delete this; }
private:
  std::atomic<JPH::uint32> ref_ = 0;
};

} // namespace

class JoltDebugRenderer : public JPH::DebugRenderer {
public:
  JoltDebugRenderer() { Initialize(); }
  void clear() { lines_.clear(); NextFrame(); }

  struct Line { float from[3], to[3]; uint32_t color; };
  [[nodiscard]] auto lines() const -> const std::vector<Line> & { return lines_; }

  void DrawLine(JPH::RVec3Arg f, JPH::RVec3Arg t, JPH::ColorArg c) override {
    lines_.push_back({{f.GetX(), f.GetY(), f.GetZ()}, {t.GetX(), t.GetY(), t.GetZ()}, c.GetUInt32()});
  }

  void DrawTriangle(JPH::RVec3Arg v1, JPH::RVec3Arg v2, JPH::RVec3Arg v3,
                    JPH::ColorArg c, ECastShadow) override {
    DrawLine(v1, v2, c);
    DrawLine(v2, v3, c);
    DrawLine(v3, v1, c);
  }

  void DrawText3D(JPH::RVec3Arg, const std::string_view &, JPH::ColorArg, float) override {}

  Batch CreateTriangleBatch(const Triangle *triangles, int count) override {
    auto *b = new BatchImpl;
    if (triangles && count > 0)
      b->triangles.assign(triangles, triangles + count);
    return b;
  }

  Batch CreateTriangleBatch(const Vertex *verts, int vcount,
                            const std::uint32_t *indices, int icount) override {
    auto *b = new BatchImpl;
    if (verts && vcount > 0 && indices && icount > 0) {
      b->triangles.resize(icount / 3);
      for (size_t t = 0; t < b->triangles.size(); ++t) {
        auto &tri = b->triangles[t];
        tri.mV[0] = verts[indices[t * 3 + 0]];
        tri.mV[1] = verts[indices[t * 3 + 1]];
        tri.mV[2] = verts[indices[t * 3 + 2]];
      }
    }
    return b;
  }

  void DrawGeometry(JPH::RMat44Arg m, const JPH::AABox &, float,
                    JPH::ColorArg color, const GeometryRef &geo,
                    ECullMode, ECastShadow, EDrawMode draw_mode) override {
    if (draw_mode != EDrawMode::Wireframe) return;
    const LOD &lod = geo->mLODs.back();  // lowest detail LOD
    auto *batch = static_cast<const BatchImpl *>(lod.mTriangleBatch.GetPtr());
    if (!batch) return;
    for (const auto &tri : batch->triangles) {
      JPH::RVec3 v0 = m * JPH::Vec3(tri.mV[0].mPosition);
      JPH::RVec3 v1 = m * JPH::Vec3(tri.mV[1].mPosition);
      JPH::RVec3 v2 = m * JPH::Vec3(tri.mV[2].mPosition);
      DrawLine(v0, v1, color);
      DrawLine(v1, v2, color);
      DrawLine(v2, v0, color);
    }
  }

private:
  std::vector<Line> lines_;
};

class DebugLineRenderer {
public:
  struct Vertex { float pos[3]; float color[4]; };

  static constexpr std::uint32_t k_frames_in_flight = 2;

  DebugLineRenderer(vk::raii::Device &dev, vk::PhysicalDeviceMemoryProperties mem_props,
                    vk::Format c_fmt,
                    std::string_view spirv, vk::DescriptorSetLayout frame_layout)
      : dev_(dev), mem_props_(mem_props) {
    auto code = engine::read_spirv_file(spirv);
    auto vs = engine::create_shader_module(dev, code);
    auto fs = engine::create_shader_module(dev, code);

    pip_layout_ = vk::raii::PipelineLayout(dev, {.setLayoutCount = 1, .pSetLayouts = &frame_layout});

    const vk::VertexInputBindingDescription bind{
        .binding = 0, .stride = sizeof(Vertex), .inputRate = vk::VertexInputRate::eVertex};
    const vk::VertexInputAttributeDescription attrs[] = {{.location = 0, .binding = 0, .format = vk::Format::eR32G32B32Sfloat, .offset = offsetof(Vertex, pos)}, {.location = 1, .binding = 0, .format = vk::Format::eR32G32B32A32Sfloat, .offset = offsetof(Vertex, color)}};

    const vk::PipelineVertexInputStateCreateInfo vi{
        .vertexBindingDescriptionCount = 1, .pVertexBindingDescriptions = &bind,
        .vertexAttributeDescriptionCount = 2, .pVertexAttributeDescriptions = attrs};
    const vk::PipelineInputAssemblyStateCreateInfo ia{.topology = vk::PrimitiveTopology::eLineList};
    const vk::PipelineRasterizationStateCreateInfo rs{.polygonMode = vk::PolygonMode::eFill,
        .cullMode = vk::CullModeFlagBits::eNone, .frontFace = vk::FrontFace::eCounterClockwise, .lineWidth = 1.0f};
    const vk::PipelineMultisampleStateCreateInfo ms{.rasterizationSamples = vk::SampleCountFlagBits::e1};  // overlay renders into resolved 1-sample image
    const vk::PipelineDepthStencilStateCreateInfo ds{.depthTestEnable = VK_FALSE,
        .depthWriteEnable = VK_FALSE};
    const vk::PipelineColorBlendAttachmentState bl{.colorWriteMask =
        vk::ColorComponentFlagBits::eR | vk::ColorComponentFlagBits::eG |
        vk::ColorComponentFlagBits::eB | vk::ColorComponentFlagBits::eA};
    const vk::PipelineColorBlendStateCreateInfo cb{
        .attachmentCount = 1, .pAttachments = &bl};
    const vk::DynamicState dyn_states[]{vk::DynamicState::eViewport, vk::DynamicState::eScissor};
    const vk::PipelineDynamicStateCreateInfo dyn{
        .dynamicStateCount = 2, .pDynamicStates = dyn_states};

    const vk::PipelineViewportStateCreateInfo vp{.viewportCount = 1, .scissorCount = 1};

    const vk::PipelineShaderStageCreateInfo stages[]{
        {.stage = vk::ShaderStageFlagBits::eVertex, .module = *vs, .pName = "vertMain"},
        {.stage = vk::ShaderStageFlagBits::eFragment, .module = *fs, .pName = "fragMain"}};

    vk::raii::PipelineCache cache(dev, {});
    vk::StructureChain<vk::GraphicsPipelineCreateInfo, vk::PipelineRenderingCreateInfo> chain{
      {.stageCount=2, .pStages=stages, .pVertexInputState=&vi, .pInputAssemblyState=&ia, .pViewportState=&vp,
       .pRasterizationState=&rs, .pMultisampleState=&ms, .pDepthStencilState=&ds,
       .pColorBlendState=&cb, .pDynamicState=&dyn,
       .layout=*pip_layout_, .renderPass=nullptr},
      {.colorAttachmentCount=1, .pColorAttachmentFormats=&c_fmt, .depthAttachmentFormat=vk::Format::eUndefined}};
    pip_ = vk::raii::Pipeline(dev, cache, chain.get<vk::GraphicsPipelineCreateInfo>());
  }

  void draw(vk::raii::CommandBuffer &cmd, vk::DescriptorSet frame_set,
            std::uint32_t frame_idx,
            const std::vector<JoltDebugRenderer::Line> &lines, vk::Extent2D extent) {
    if (lines.empty() || extent.width == 0 || extent.height == 0) return;

    cmd.setViewport(0, vk::Viewport{0, 0, (float)extent.width, (float)extent.height, 0, 1});
    cmd.setScissor(0, vk::Rect2D{{0, 0}, extent});
    std::vector<Vertex> verts(lines.size() * 2);
    for (size_t i = 0; i < lines.size(); ++i) {
      auto &v0 = verts[i*2], &v1 = verts[i*2+1];
      v0.pos[0]=lines[i].from[0]; v0.pos[1]=lines[i].from[1]; v0.pos[2]=lines[i].from[2];
      v1.pos[0]=lines[i].to[0]; v1.pos[1]=lines[i].to[1]; v1.pos[2]=lines[i].to[2];
      float r=((lines[i].color>>24)&0xFF)/255.f, g=((lines[i].color>>16)&0xFF)/255.f;
      float b=((lines[i].color>>8)&0xFF)/255.f, a=(lines[i].color&0xFF)/255.f;
      v0.color[0]=r; v0.color[1]=g; v0.color[2]=b; v0.color[3]=a;
      v1.color[0]=r; v1.color[1]=g; v1.color[2]=b; v1.color[3]=a;
    }
    auto sz = verts.size() * sizeof(Vertex);
    auto &vb = vb_[frame_idx];
    auto &mem = mem_[frame_idx];
    auto &cur_size = vb_size_[frame_idx];
    if (cur_size < sz) {
      vb.reset(); mem.reset();
      vb.emplace(dev_, vk::BufferCreateInfo{.size=sz, .usage=vk::BufferUsageFlagBits::eVertexBuffer});
      auto req = vb->getMemoryRequirements();
      auto mem_type = engine::detail::find_memory_type(mem_props_, req.memoryTypeBits,
          vk::MemoryPropertyFlagBits::eHostVisible|vk::MemoryPropertyFlagBits::eHostCoherent);
      mem.emplace(dev_, vk::MemoryAllocateInfo{.allocationSize=req.size, .memoryTypeIndex=mem_type});
      vb->bindMemory(**mem, 0);
      cur_size = sz;
    }
    std::memcpy(mem->mapMemory(0, sz), verts.data(), sz);
    mem->unmapMemory();
    cmd.bindPipeline(vk::PipelineBindPoint::eGraphics, *pip_);
    cmd.bindDescriptorSets(vk::PipelineBindPoint::eGraphics, *pip_layout_, 0, frame_set, nullptr);
    cmd.bindVertexBuffers(0, **vb, {0});
    cmd.draw(static_cast<uint32_t>(verts.size()), 1, 0, 0);
  }

private:
  vk::raii::Device &dev_;
  vk::PhysicalDeviceMemoryProperties mem_props_;
  vk::raii::PipelineLayout pip_layout_{nullptr};
  vk::raii::Pipeline pip_{nullptr};
  std::array<std::optional<vk::raii::Buffer>, k_frames_in_flight> vb_{};
  std::array<std::optional<vk::raii::DeviceMemory>, k_frames_in_flight> mem_{};
  std::array<size_t, k_frames_in_flight> vb_size_{};
};
