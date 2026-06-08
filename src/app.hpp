#pragma once

#include "engine_config.hpp"

#include <memory>

namespace app {

class DemoApp {
public:
  explicit DemoApp(engine::EngineConfig config = engine::engine_config_from_environment());
  ~DemoApp();

  DemoApp(const DemoApp &) = delete;
  DemoApp &operator=(const DemoApp &) = delete;

  void run();

private:
  struct Impl;
  std::unique_ptr<Impl> impl_;
};

} // namespace app
