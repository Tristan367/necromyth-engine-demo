#include "debug_ui.hpp"

#include "renderer/render_host.hpp"
#include "renderer/render_settings.hpp"
#include "renderer/vulkan_context.hpp"
#include "scene/shadow_utils.hpp"

#include <imgui.h>
#include <imgui_impl_sdl3.h>
#include <imgui_impl_vulkan.h>

#include <memory>
#include <stdexcept>

namespace app {

namespace {

void check_vk_result(VkResult result) {
  if (result != VK_SUCCESS)
    throw std::runtime_error("ImGui Vulkan call failed");
}

[[nodiscard]] auto to_c_string(engine::ShadowFilterMode mode) -> const char * {
  switch (mode) {
  case engine::ShadowFilterMode::Hard:
    return "Hard";
  case engine::ShadowFilterMode::Pcf3x3:
    return "PCF 3x3";
  }
  return "Unknown";
}

[[nodiscard]] auto to_c_string(engine::ShadowFocusMode mode) -> const char * {
  switch (mode) {
  case engine::ShadowFocusMode::CameraFootprint:
    return "Camera footprint";
  case engine::ShadowFocusMode::ViewWedge:
    return "View wedge";
  }
  return "Unknown";
}

} // namespace

struct DebugUi::Impl {
  SDL_Window *window{};
  engine::VulkanContext *vulkan{};
  vk::Extent2D last_extent_{};
  float fps_smoothed_{};
  bool initialized{false};

  explicit Impl(SDL_Window *window_in, engine::VulkanContext &vulkan_in)
      : window(window_in), vulkan(&vulkan_in) {
    IMGUI_CHECKVERSION();
    ImGui::CreateContext();
    ImGui::StyleColorsDark();

    ImGuiIO &io = ImGui::GetIO();
    io.ConfigFlags |= ImGuiConfigFlags_NavEnableKeyboard;

    if (!ImGui_ImplSDL3_InitForVulkan(window))
      throw std::runtime_error("ImGui_ImplSDL3_InitForVulkan failed");

    const engine::RenderHostInfo host = vulkan->render_host_info();
    const VkFormat color_format = static_cast<VkFormat>(host.swapchain_color_format);

    ImGui_ImplVulkan_InitInfo init_info{};
    init_info.ApiVersion = VK_API_VERSION_1_3;
    init_info.Instance = host.instance;
    init_info.PhysicalDevice = host.physical_device;
    init_info.Device = host.device;
    init_info.QueueFamily = host.graphics_queue_family_index;
    init_info.Queue = host.graphics_queue;
    init_info.DescriptorPool = VK_NULL_HANDLE;
    init_info.DescriptorPoolSize = 1000;
    init_info.MinImageCount = host.swapchain_image_count;
    init_info.ImageCount = host.swapchain_image_count;
    init_info.MSAASamples = VK_SAMPLE_COUNT_1_BIT;
    init_info.PipelineCache = host.pipeline_cache;
    init_info.CheckVkResultFn = check_vk_result;
    init_info.UseDynamicRendering = true;
    init_info.PipelineRenderingCreateInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_RENDERING_CREATE_INFO_KHR;
    init_info.PipelineRenderingCreateInfo.colorAttachmentCount = 1;
    init_info.PipelineRenderingCreateInfo.pColorAttachmentFormats = &color_format;

    if (!ImGui_ImplVulkan_Init(&init_info))
      throw std::runtime_error("ImGui_ImplVulkan_Init failed");

    if (!ImGui_ImplVulkan_CreateFontsTexture())
      throw std::runtime_error("ImGui_ImplVulkan_CreateFontsTexture failed");

    last_extent_ = host.swapchain_extent;
    initialized = true;
  }

  ~Impl() {
    if (!initialized)
      return;

    ImGui_ImplVulkan_Shutdown();
    ImGui_ImplSDL3_Shutdown();
    ImGui::DestroyContext();
  }

  void sync_swapchain() {
    const engine::RenderHostInfo host = vulkan->render_host_info();
    if (host.swapchain_extent.width == last_extent_.width &&
        host.swapchain_extent.height == last_extent_.height &&
        host.swapchain_image_count != 0)
      return;

    ImGui_ImplVulkan_DestroyFontsTexture();
    ImGui_ImplVulkan_SetMinImageCount(host.swapchain_image_count);
    if (!ImGui_ImplVulkan_CreateFontsTexture())
      throw std::runtime_error("ImGui_ImplVulkan_CreateFontsTexture failed after resize");

    last_extent_ = host.swapchain_extent;
  }
};

DebugUi::DebugUi(SDL_Window *window, engine::VulkanContext &vulkan)
    : impl_(std::make_unique<Impl>(window, vulkan)) {}

DebugUi::~DebugUi() = default;

auto DebugUi::process_event(const SDL_Event &event) -> bool {
  return ImGui_ImplSDL3_ProcessEvent(&event);
}

auto DebugUi::wants_keyboard() const -> bool {
  return ImGui::GetIO().WantCaptureKeyboard;
}

auto DebugUi::wants_mouse() const -> bool {
  return ImGui::GetIO().WantCaptureMouse;
}

auto DebugUi::begin_frame(engine::Scene &scene, float frame_delta_seconds, bool menu_open) -> UiFrameResult {
  UiFrameResult result{};

  impl_->sync_swapchain();

  if (frame_delta_seconds > 0.0F) {
    const float instant_fps = 1.0F / frame_delta_seconds;
    impl_->fps_smoothed_ =
        impl_->fps_smoothed_ <= 0.0F ? instant_fps : impl_->fps_smoothed_ * 0.9F + instant_fps * 0.1F;
  }

  ImGui_ImplSDL3_NewFrame();
  ImGui_ImplVulkan_NewFrame();
  ImGui::NewFrame();

  if (menu_open) {
    const ImGuiViewport *viewport = ImGui::GetMainViewport();
    ImGui::SetNextWindowPos(viewport->GetCenter(), ImGuiCond_Always, ImVec2(0.5F, 0.5F));
    ImGui::SetNextWindowSize(ImVec2(280.0F, 0.0F), ImGuiCond_Always);
    if (ImGui::Begin("Menu", nullptr, ImGuiWindowFlags_NoCollapse | ImGuiWindowFlags_AlwaysAutoResize)) {
      ImGui::TextUnformatted("Paused");
      ImGui::Separator();
      if (ImGui::Button("Resume (Esc)", ImVec2(-1.0F, 0.0F)))
        result.resume_requested = true;
      if (ImGui::Button("Quit", ImVec2(-1.0F, 0.0F)))
        result.quit_requested = true;
      ImGui::TextDisabled("Esc toggles fly mode");
    }
    ImGui::End();
  }

  ImGui::SetNextWindowPos(ImVec2(10.0F, 10.0F), ImGuiCond_FirstUseEver);
  ImGui::SetNextWindowSize(ImVec2(320.0F, 0.0F), ImGuiCond_FirstUseEver);
  if (ImGui::Begin("Debug", nullptr, ImGuiWindowFlags_AlwaysAutoResize)) {
    ImGui::Text("FPS: %.1f", impl_->fps_smoothed_);
    if (frame_delta_seconds > 0.0F)
      ImGui::Text("Frame: %.2f ms", frame_delta_seconds * 1000.0F);
    ImGui::Text("Instances: %zu", scene.instances().size());
    ImGui::Text("GPU: %s", impl_->vulkan->gpu_name().c_str());
    const char *present = engine::present_mode_name(impl_->vulkan->present_mode());
    ImGui::Text("Present: %s", present);
    if (impl_->vulkan->present_mode() == vk::PresentModeKHR::eFifo)
      ImGui::TextDisabled("(vsync — ENGINE_PRESENT=mailbox to uncap)");
    ImGui::Separator();

    engine::DirectionalLightShadowSettings &shadow = scene.shadow_settings();
    ImGui::Text("Shadow filter: %s", to_c_string(shadow.filter_mode));
    ImGui::TextDisabled("Restart to change (ENGINE_SHADOW_FILTER)");

    int focus_index = static_cast<int>(shadow.focus_mode);
    const char *focus_labels[] = {"Camera footprint", "View wedge"};
    if (ImGui::Combo("Shadow focus", &focus_index, focus_labels, IM_ARRAYSIZE(focus_labels)))
      shadow.focus_mode = static_cast<engine::ShadowFocusMode>(focus_index);
    else
      ImGui::Text("Focus: %s", to_c_string(shadow.focus_mode));

    ImGui::Text("Point shadow filter: %s", shadow.point_shadow_filter ? "nearest" : "linear");
    ImGui::Checkbox("Texel snap", &shadow.texel_snapping);
    ImGui::Checkbox("Coverage fade", &shadow.coverage_fade);
    ImGui::SliderFloat("Fade width", &shadow.coverage_fade_uv_width, 0.0F, 0.25F, "%.3f");
  }
  ImGui::End();

  ImGui::Render();
  return result;
}

void DebugUi::record_overlay(const engine::FrameOverlayContext &context) {
  ImGui_ImplVulkan_RenderDrawData(ImGui::GetDrawData(), *context.command_buffer);
}

} // namespace app
