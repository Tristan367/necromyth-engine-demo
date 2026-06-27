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
#include <vector>

class JoltDebugRenderer : public JPH::DebugRenderer {
public:
  JoltDebugRenderer() { Initialize(); }
  void clear() { lines_.clear(); NextFrame(); }

  struct Line { float from[3], to[3]; uint32_t color; };
  [[nodiscard]] auto lines() const -> const std::vector<Line> & { return lines_; }

  // The only method Jolt actually calls for wireframe drawing
  void DrawLine(JPH::RVec3Arg f, JPH::RVec3Arg t, JPH::ColorArg c) override {
    lines_.push_back({{f.GetX(), f.GetY(), f.GetZ()}, {t.GetX(), t.GetY(), t.GetZ()}, c.GetUInt32()});
  }

  // Stubs — not used for wireframe
  void DrawTriangle(JPH::RVec3Arg, JPH::RVec3Arg, JPH::RVec3Arg, JPH::ColorArg, ECastShadow) override {}
  void DrawText3D(JPH::RVec3Arg, const std::string_view &, JPH::ColorArg, float) override {}
  Batch CreateTriangleBatch(const Triangle *, int) override { return nullptr; }
  Batch CreateTriangleBatch(const Vertex *, int, const std::uint32_t *, int) override { return nullptr; }
  void DrawGeometry(JPH::RMat44Arg, const JPH::AABox &, float, JPH::ColorArg, const GeometryRef &,
                    ECullMode, ECastShadow, EDrawMode) override {}

private:
  std::vector<Line> lines_;
};

class DebugLineRenderer {
public:
  struct Vertex { float pos[3]; float color[4]; };

  DebugLineRenderer(vk::raii::Device &dev, vk::raii::PhysicalDevice phys_dev,
                    vk::Format c_fmt, vk::Format d_fmt,
                    std::string_view spirv, vk::DescriptorSetLayout frame_layout,
                    vk::SampleCountFlagBits samples)
      : dev_(dev), phys_dev_(std::move(phys_dev)) {
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
        .cullMode = vk::CullModeFlagBits::eNone, .frontFace = vk::FrontFace::eCounterClockwise, .lineWidth = 2.0f};
    const vk::PipelineMultisampleStateCreateInfo ms{.rasterizationSamples = samples};
    const vk::PipelineDepthStencilStateCreateInfo ds{.depthTestEnable = VK_TRUE,
        .depthWriteEnable = VK_FALSE, .depthCompareOp = vk::CompareOp::eLessOrEqual};
    const vk::PipelineColorBlendAttachmentState bl{.colorWriteMask =
        vk::ColorComponentFlagBits::eR | vk::ColorComponentFlagBits::eG |
        vk::ColorComponentFlagBits::eB | vk::ColorComponentFlagBits::eA};
    const vk::PipelineColorBlendStateCreateInfo cb{
        .attachmentCount = 1, .pAttachments = &bl};
    const vk::PipelineDynamicStateCreateInfo dyn{.dynamicStateCount = 0, .pDynamicStates = nullptr};

    const vk::PipelineShaderStageCreateInfo stages[]{
        {.stage = vk::ShaderStageFlagBits::eVertex, .module = *vs, .pName = "vertMain"},
        {.stage = vk::ShaderStageFlagBits::eFragment, .module = *fs, .pName = "fragMain"}};

    vk::raii::PipelineCache cache(dev, {});
    vk::StructureChain<vk::GraphicsPipelineCreateInfo, vk::PipelineRenderingCreateInfo> chain{
      {.stageCount=2, .pStages=stages, .pVertexInputState=&vi, .pInputAssemblyState=&ia,
       .pRasterizationState=&rs, .pMultisampleState=&ms, .pDepthStencilState=&ds,
       .pColorBlendState=&cb, .layout=*pip_layout_, .renderPass=nullptr},
      {.colorAttachmentCount=1, .pColorAttachmentFormats=&c_fmt, .depthAttachmentFormat=d_fmt}};
    pip_ = vk::raii::Pipeline(dev, cache, chain.get<vk::GraphicsPipelineCreateInfo>());
  }

  void draw(vk::raii::CommandBuffer &cmd, vk::DescriptorSet frame_set,
            const std::vector<JoltDebugRenderer::Line> &lines) {
    if (lines.empty()) return;
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
    vk::raii::Buffer vb(dev_, {.size=sz, .usage=vk::BufferUsageFlagBits::eVertexBuffer});
    auto req = vb.getMemoryRequirements();
    auto mem_props = phys_dev_.getMemoryProperties();
    auto mem_type = engine::detail::find_memory_type(mem_props, req.memoryTypeBits,
        vk::MemoryPropertyFlagBits::eHostVisible|vk::MemoryPropertyFlagBits::eHostCoherent);
    vk::raii::DeviceMemory mem(dev_, {.allocationSize=req.size, .memoryTypeIndex=mem_type});
    vb.bindMemory(*mem, 0);
    std::memcpy(mem.mapMemory(0, sz), verts.data(), sz);
    mem.unmapMemory();
    cmd.bindPipeline(vk::PipelineBindPoint::eGraphics, *pip_);
    cmd.bindDescriptorSets(vk::PipelineBindPoint::eGraphics, *pip_layout_, 0, frame_set, nullptr);
    cmd.bindVertexBuffers(0, *vb, {0});
    cmd.draw(static_cast<uint32_t>(verts.size()), 1, 0, 0);
  }

private:
  vk::raii::Device &dev_;
  vk::raii::PhysicalDevice phys_dev_;
  vk::raii::PipelineLayout pip_layout_{nullptr};
  vk::raii::Pipeline pip_{nullptr};
};
