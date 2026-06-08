#pragma once

#include "scene/camera.hpp"

#define GLM_FORCE_RADIANS
#include <glm/gtc/constants.hpp>
#include <glm/gtc/matrix_transform.hpp>
#include <glm/geometric.hpp>
#include <glm/vec3.hpp>

#include <SDL3/SDL.h>

#include <algorithm>
#include <cmath>

namespace app {

class FlyCameraController {
public:
  ~FlyCameraController() {
    release_capture();
  }

  void set_window(SDL_Window *window) {
    window_ = window;
  }

  [[nodiscard]] auto capture_active() const -> bool {
    return captured_;
  }

  void release_capture() {
    if (window_ == nullptr || !captured_)
      return;

    SDL_SetWindowRelativeMouseMode(window_, false);
    captured_ = false;
  }

  void sync_from(const engine::Camera &camera) {
    position_ = camera.position();
    const glm::vec3 forward = camera.look_direction();
    pitch_ = std::asin(std::clamp(forward.y, -1.0F, 1.0F));
    yaw_ = std::atan2(forward.z, forward.x);
  }

  void handle_event(const SDL_Event &event) {
    if (event.type == SDL_EVENT_MOUSE_BUTTON_DOWN && event.button.button == SDL_BUTTON_LEFT) {
      if (!captured_)
        enable_capture();
    } else if (event.type == SDL_EVENT_KEY_DOWN && event.key.key == SDLK_ESCAPE) {
      release_capture();
    } else if (event.type == SDL_EVENT_WINDOW_FOCUS_LOST) {
      release_capture();
    } else if (event.type == SDL_EVENT_MOUSE_MOTION && captured_) {
      yaw_ += event.motion.xrel * mouse_sensitivity_;
      pitch_ -= event.motion.yrel * mouse_sensitivity_;
      pitch_ = std::clamp(pitch_, -glm::half_pi<float>() + 0.01F, glm::half_pi<float>() - 0.01F);
    }
  }

  void update(engine::Camera &camera, float delta_seconds) {
    const bool *keyboard = SDL_GetKeyboardState(nullptr);
    if (keyboard == nullptr)
      return;

    const glm::vec3 forward = orientation_forward();
    const glm::vec3 right = glm::normalize(glm::cross(forward, glm::vec3(0.0F, 1.0F, 0.0F)));
    const glm::vec3 up{0.0F, 1.0F, 0.0F};

    const float speed = (keyboard[SDL_SCANCODE_LSHIFT] || keyboard[SDL_SCANCODE_RSHIFT]) ? fast_speed_ : move_speed_;
    const glm::vec3 velocity =
        (keyboard[SDL_SCANCODE_W] ? forward : glm::vec3(0.0F)) +
        (keyboard[SDL_SCANCODE_S] ? -forward : glm::vec3(0.0F)) +
        (keyboard[SDL_SCANCODE_D] ? right : glm::vec3(0.0F)) +
        (keyboard[SDL_SCANCODE_A] ? -right : glm::vec3(0.0F)) +
        (keyboard[SDL_SCANCODE_E] ? up : glm::vec3(0.0F)) +
        (keyboard[SDL_SCANCODE_Q] ? -up : glm::vec3(0.0F));

    if (glm::length(velocity) > 0.0F)
      position_ += glm::normalize(velocity) * speed * delta_seconds;

    camera.look_at(position_, position_ + forward);
  }

private:
  void enable_capture() {
    if (window_ == nullptr || captured_)
      return;

    if (SDL_SetWindowRelativeMouseMode(window_, true))
      captured_ = true;
  }

  [[nodiscard]] auto orientation_forward() const -> glm::vec3 {
    const float cos_pitch = std::cos(pitch_);
    return glm::normalize(glm::vec3{
        cos_pitch * std::cos(yaw_),
        std::sin(pitch_),
        cos_pitch * std::sin(yaw_),
    });
  }

  SDL_Window *window_{nullptr};
  glm::vec3 position_{2.0F, 1.5F, 4.0F};
  float yaw_{};
  float pitch_{};
  float move_speed_{6.0F};
  float fast_speed_{18.0F};
  float mouse_sensitivity_{0.002F};
  bool captured_{false};
};

} // namespace app
